// Host-agnostic DSP core for Deepblue — an "underwater" effect for guitar.
//
// This file contains ZERO plugin-format dependencies (no lv2.h, no JUCE).
// It is compiled verbatim into every wrapper: the LV2 wrapper (src/plugin.cpp)
// and the JUCE wrapper (juce/PluginProcessor.cpp). Each wrapper only maps host
// controls onto DeepblueParams and calls process().
//
// Signal chain (steps 1–5):
//   in → Absorption → Wobble → Dispersion → [StereoField] → ⊕bubbles → ⊕reverb
//      → Dry/Wet → ×level → out
//
// Everything downstream of the per-channel chain — the StereoField, the Minnaert
// bubble stream and the dark diffuse reverb — is part of the "wet" underwater
// signal, so the single Mix knob balances the dry guitar against the whole
// treatment. StereoField is cross-channel (stereo path only); the reverb is fed
// by the wet signal (bubbles included) so the bubbles reverberate in the space.
//
// The single "Depth" macro is the physical through-line: deeper water means
// stronger high-frequency absorption, more pitch wavering and more dispersion,
// so one knob biases all three at once (with per-control trims on top). It also
// raises the ambient pressure, so the bubble stream's Minnaert pings shift up.
// Bubbles are not an added sound: each one is a resonant band-pass ringing off
// the dry input at its Minnaert frequency, so the water bloops around the playing.

#include "deepblue_dsp.h"

#include <cmath>
#include <algorithm>
#include <new>

#include "biquad.hpp"
#include "delayline.hpp"
#include "lfo.hpp"
#include "dispersion.hpp"
#include "bubbles.hpp"
#include "stereofield.hpp"
#include "reverb.hpp"

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
    float bubbleAmt;       // 0..1, bubble-stream presence
    float bubbleSize;      // 0..1, bubble register
    float depthMeters;     // depth macro mapped to metres of water
    float fieldAmt;        // 0..1, loss-of-localisation amount (stereo only)
    float reverbAmt;       // 0..1, reverb send into the wet
    float reverbG;         // feedback gain (decay / size)
    float reverbDamp;      // in-loop HF damping (darkness), depth/tone-derived
    float reverbSize;      // 0..1, raw size → pre-delay (depth of the space)
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

    m.bubbleAmt   = clampf(pr->bubbles, 0.0f, 1.0f);
    m.bubbleSize  = clampf(pr->bubble_size, 0.0f, 1.0f);
    m.depthMeters = d * 40.0f;    // 0 → 40 m: full depth ≈ ×2.2 on the Minnaert pitch
    // Depth biases the immersion the way it biases wobble/dispersion: deeper
    // water pulls the whole image toward "everywhere / inside the head".
    m.fieldAmt    = clampf(clampf(pr->immersion, 0.0f, 1.0f) * (0.4f + 0.6f * d), 0.0f, 1.0f);

    // Reverb: amount is direct; size sets the decay (feedback gain) and the
    // pre-delay (how far the reflecting walls are); darkness tracks the water —
    // deeper damps more highs, the Tone trim brightens it. The gain reaches
    // further now (0.72 → 0.985) so big settings sustain a long, deep abyss.
    m.reverbAmt  = clampf(pr->reverb, 0.0f, 1.0f);
    m.reverbSize = clampf(pr->reverb_size, 0.0f, 1.0f);
    m.reverbG    = 0.72f + 0.265f * m.reverbSize;                          // 0.72 → 0.985
    m.reverbDamp = clampf(0.25f + 0.50f * d - (t - 0.5f) * 0.4f, 0.10f, 0.92f);
    return m;
}

// Refresh per-block coefficients/rates. Filter state is preserved so the
// modulation stays click-free across blocks.
static inline void prepChannel(Channel& c, const Macro& m, double sr) {
    c.lp1.setup(Biquad::LP, m.cutoff, m.q, (float)sr);
    c.lp2.setup(Biquad::LP, m.cutoff, m.q, (float)sr);
    c.wlfo.setRate(m.wlfoRate);
    c.wrnd.setRate(m.wrndRate);
}

// One sample of the per-channel underwater chain, returning the *wet* signal
// before the dry/wet mix (so a cross-channel stage can sit in between).
static inline float channelWet(Channel& c, const Macro& m,
                               float baseS, float excS, float x) {
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
    return w + m.dispAmount * (dispd - w);
}

static inline float wobbleExc(const Macro& m, double sr) {
    return WOBBLE_MAX_MS * (float)sr / 1000.0f * m.wobbleDepth;
}
static inline float wobbleBase(double sr) {
    return WOBBLE_BASE_MS * (float)sr / 1000.0f;
}

// Extra presence for the bubble filter on top of its per-voice makeup, so the
// resonances clearly stand out in the wet rather than hiding under it. The
// "bubbles" knob still scales the whole layer from silent to this ceiling.
static constexpr float BUBBLE_PRESENCE = 1.7f;

// The bubble stream is one shared filter-bank: each bubble is a resonant
// band-pass ringing off the dry input (not an added sound — silence in,
// silence out), panned in stereo. It must be fed pre-absorption: small bubbles
// live at 4–16 kHz, which the wet path has already low-passed away. Its output
// joins the wet, so the bed low-pass and the dry/wet mix still apply.
// The reverb is one shared stereo FDN. Both feed the wet before the dry/wet mix.
static void processMono(Channel& c, BubbleStream& bub, Reverb& rev,
                        const Macro& m, double sr,
                        const float* in, float* out, uint32_t n)
{
    prepChannel(c, m, sr);
    bub.setParams(m.bubbleAmt, m.bubbleSize, m.depthMeters, m.cutoff);
    rev.setParams(m.reverbG, m.reverbDamp, m.reverbSize);
    const float baseS = wobbleBase(sr);
    const float excS  = wobbleExc(m, sr);
    const bool  doRev = m.reverbAmt > 0.0f;

    for (uint32_t i = 0; i < n; ++i) {
        const float x = in[i];
        float w = channelWet(c, m, baseS, excS, x);

        float bl, br;
        bub.tick(x, bl, br);                        // bubbles ring off the dry input
        w += (bl + br) * (0.5f * BUBBLE_PRESENCE) * m.bubbleAmt;   // folded to mono

        if (doRev) {
            float rL, rR;
            rev.process(w, w, rL, rR);
            w += (rL + rR) * 0.5f * m.reverbAmt;
        }

        out[i] = (x * (1.0f - m.mix) + w * m.mix) * m.level;
    }
}

// Stereo: both channels share one sample loop so the StereoField can act across
// them, then bubbles and reverb are summed into the wet before dry/wet + gain.
static void processStereo(Channel& L, Channel& R, StereoField& field,
                          BubbleStream& bub, Reverb& rev,
                          const Macro& m, double sr,
                          const float* inL, const float* inR,
                          float* outL, float* outR, uint32_t n)
{
    prepChannel(L, m, sr);
    prepChannel(R, m, sr);
    bub.setParams(m.bubbleAmt, m.bubbleSize, m.depthMeters, m.cutoff);
    rev.setParams(m.reverbG, m.reverbDamp, m.reverbSize);
    const float baseS = wobbleBase(sr);
    const float excS  = wobbleExc(m, sr);   // depth-derived, identical L/R
    const bool  doRev = m.reverbAmt > 0.0f;

    for (uint32_t i = 0; i < n; ++i) {
        const float xL = inL[i], xR = inR[i];
        float wL = channelWet(L, m, baseS, excS, xL);
        float wR = channelWet(R, m, baseS, excS, xR);

        field.process(m.fieldAmt, wL, wR);   // loss of localisation, on the wet

        float bl, br;
        bub.tick(0.5f * (xL + xR), bl, br);  // bubbles ring off the dry mid
        wL += bl * BUBBLE_PRESENCE * m.bubbleAmt;
        wR += br * BUBBLE_PRESENCE * m.bubbleAmt;

        if (doRev) {
            float rL, rR;
            rev.process(wL, wR, rL, rR);
            wL += rL * m.reverbAmt;
            wR += rR * m.reverbAmt;
        }

        outL[i] = (xL * (1.0f - m.mix) + wL * m.mix) * m.level;
        outR[i] = (xR * (1.0f - m.mix) + wR * m.mix) * m.level;
    }
}

// ── Instance ─────────────────────────────────────────────────────────────────
struct DeepblueDsp {
    double       sr;
    Channel      L, R;
    BubbleStream bubbles;
    StereoField  field;
    Reverb       reverb;
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
    p->bubbles.init(sample_rate, 0xC0FFEE11u);
    p->field.init(sample_rate);
    p->reverb.init(sample_rate);
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
    p->bubbles.reset();
    p->field.reset();
    p->reverb.reset();
}

void deepblue_dsp_process(DeepblueDsp* p, const DeepblueParams* pr,
                          const float* in, float* out, uint32_t n)
{
    const Macro m = computeMacro(pr);
    processMono(p->L, p->bubbles, p->reverb, m, p->sr, in, out, n);
}

void deepblue_dsp_process_stereo(DeepblueDsp* p, const DeepblueParams* pr,
                                 const float* inL, const float* inR,
                                 float* outL, float* outR, uint32_t n)
{
    const Macro m = computeMacro(pr);
    processStereo(p->L, p->R, p->field, p->bubbles, p->reverb,
                  m, p->sr, inL, inR, outL, outR, n);
}
