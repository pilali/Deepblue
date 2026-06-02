// Host-agnostic DSP core for Deepblue — an "underwater" effect for guitar.
//
// This file contains ZERO plugin-format dependencies (no lv2.h, no JUCE).
// It is compiled verbatim into every wrapper: the LV2 wrapper (src/plugin.cpp)
// and the JUCE wrapper (juce/PluginProcessor.cpp). Each wrapper only maps host
// controls onto DeepblueParams and calls process().
//
// Signal chain (steps 1 & 2):
//   in → Absorption (4-pole low-pass) → Wobble (modulated fractional delay)
//      → Dispersion (allpass chain) → Dry/Wet → ×level → out
//
// The single "Depth" macro is the physical through-line: deeper water means
// stronger high-frequency absorption, more pitch wavering and more dispersion,
// so one knob biases all three at once (with per-control trims on top).

#include "deepblue_dsp.h"

#include <cmath>
#include <algorithm>
#include <new>

#include "biquad.hpp"
#include "delayline.hpp"
#include "lfo.hpp"
#include "dispersion.hpp"

// Length of the dispersion allpass chain. Short on the MOD Dwarf (Cortex-A35),
// long on the Pi 5 / desktop. Overridden by the build (-DDEEPBLUE_DISP_STAGES).
#ifndef DEEPBLUE_DISP_STAGES
#define DEEPBLUE_DISP_STAGES 12
#endif

// Internal constants (not exposed as parameters).
static constexpr float WOBBLE_BASE_MS = 12.0f;   // centre read offset
static constexpr float WOBBLE_MAX_MS  = 7.0f;    // ± excursion at full depth
static constexpr int   DELAY_MAX_MS   = 60;

static inline float clampf(float x, float lo, float hi) noexcept {
    return x < lo ? lo : (x > hi ? hi : x);
}

// ── Per-channel chain ────────────────────────────────────────────────────────
struct Channel {
    Biquad    lp1, lp2;                          // 4-pole absorption low-pass
    DelayLine wob;
    SineLFO   wlfo;
    RandomLFO wrnd;
    DispersionChain<DEEPBLUE_DISP_STAGES> disp;

    void init(double sr, uint32_t seed, double lfoPhase, float dispSpread) {
        wob.init((int)(DELAY_MAX_MS * sr / 1000.0) + 4);
        wlfo.init(sr);
        wlfo.setPhase(lfoPhase);
        wrnd.init(sr, seed);
        disp.setup((float)sr, dispSpread);
    }
    void reset() {
        lp1.reset(); lp2.reset();
        wob.reset();
        wlfo.reset();
        wrnd.reset();
        disp.reset();
    }
};

// Per-block parameters, derived once from DeepblueParams + the Depth macro.
struct Macro {
    float cutoff, q;
    float wobbleDepth;     // 0..1, after depth bias
    float dispAmount;      // 0..1, after depth bias
    float mix, level;
    float wlfoRate, wrndRate;
};

static Macro computeMacro(const DeepblueParams* pr) {
    const float d = clampf(pr->depth, 0.0f, 1.0f);
    const float t = clampf(pr->tone,  0.0f, 1.0f);

    // Absorption: high frequencies vanish with depth. 9 kHz at the surface
    // down to ~450 Hz at full depth, then a bright/dark trim (±2 octaves).
    const float depthCut   = 9000.0f * std::pow(450.0f / 9000.0f, d);
    const float toneFactor = std::pow(4.0f, t - 0.5f);

    Macro m;
    m.cutoff      = clampf(depthCut * toneFactor, 200.0f, 12000.0f);
    m.q           = 0.707f;
    m.wobbleDepth = clampf(clampf(pr->wobble, 0.0f, 1.0f) * (0.4f + 0.6f * d), 0.0f, 1.0f);
    m.dispAmount  = clampf(clampf(pr->dispersion, 0.0f, 1.0f) * (0.5f + 0.5f * d), 0.0f, 1.0f);
    m.mix         = clampf(pr->mix, 0.0f, 1.0f);
    m.level       = clampf(pr->level, 0.0f, 4.0f);

    const float rate = clampf(pr->wobble_rate, 0.05f, 2.0f);
    m.wlfoRate = rate;
    m.wrndRate = rate * 0.6f;     // the random drift breathes slower than the sine
    return m;
}

static void processChannel(Channel& c, const Macro& m, double sr,
                           const float* in, float* out, uint32_t n)
{
    // Coefficients/rates refresh once per block; filter state is preserved so
    // the modulation stays click-free.
    c.lp1.setup(Biquad::LP, m.cutoff, m.q, (float)sr);
    c.lp2.setup(Biquad::LP, m.cutoff, m.q, (float)sr);
    c.wlfo.setRate(m.wlfoRate);
    c.wrnd.setRate(m.wrndRate);

    const float baseS = WOBBLE_BASE_MS * (float)sr / 1000.0f;
    const float excS  = WOBBLE_MAX_MS  * (float)sr / 1000.0f * m.wobbleDepth;

    for (uint32_t i = 0; i < n; ++i) {
        const float x = in[i];

        // 1) Absorption — 4-pole low-pass.
        const float lp = c.lp2.process(c.lp1.process(x));

        // 2) Wobble — read the low-passed signal at a slowly wavering offset.
        c.wob.write(lp);
        const float mod = 0.7f * c.wlfo.tick() + 0.3f * c.wrnd.tick();
        float dly = baseS + mod * excS;
        if (dly < 1.0f) dly = 1.0f;
        const float w = c.wob.read(dly);

        // 3) Dispersion — allpass chain, mixed back in by amount.
        const float dispd = c.disp.process(w);
        const float wet   = w + m.dispAmount * (dispd - w);

        // 4) Dry/wet + output gain.
        out[i] = (x * (1.0f - m.mix) + wet * m.mix) * m.level;
    }
}

// ── Instance ─────────────────────────────────────────────────────────────────
struct DeepblueDsp {
    double  sr;
    Channel L, R;
};

DeepblueDsp* deepblue_dsp_new(double sample_rate)
{
    DeepblueDsp* p = new (std::nothrow) DeepblueDsp();
    if (!p) return nullptr;
    p->sr = sample_rate;
    // L is the reference; R is decorrelated (anti-phase-ish wobble, different
    // random stream, slightly spread dispersion) so a mono source widens
    // naturally on the stereo path.
    p->L.init(sample_rate, 0x00003039u, 0.00, 1.00f);
    p->R.init(sample_rate, 0x9E3779B9u, 0.25, 1.06f);
    return p;
}

void deepblue_dsp_free(DeepblueDsp* p)
{
    delete p;
}

void deepblue_dsp_reset(DeepblueDsp* p)
{
    p->L.reset();
    p->R.reset();
}

void deepblue_dsp_process(DeepblueDsp* p, const DeepblueParams* pr,
                          const float* in, float* out, uint32_t n)
{
    const Macro m = computeMacro(pr);
    processChannel(p->L, m, p->sr, in, out, n);
}

void deepblue_dsp_process_stereo(DeepblueDsp* p, const DeepblueParams* pr,
                                 const float* inL, const float* inR,
                                 float* outL, float* outR, uint32_t n)
{
    const Macro m = computeMacro(pr);
    processChannel(p->L, m, p->sr, inL, outL, n);
    processChannel(p->R, m, p->sr, inR, outR, n);
}
