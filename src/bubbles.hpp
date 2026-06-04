#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// ── Minnaert bubble stream ───────────────────────────────────────────────────
//
// A generative stream of bubble "pings". A gas bubble in water rings at its
// Minnaert resonance (Minnaert, 1933):
//
//     f0 = (1 / 2πa) · √(3γP / ρ)        a = radius, γ = 1.4, ρ = 1000 kg/m³
//
// which at the surface is ≈ 3.28 / a Hz (a in metres): a 1 mm bubble pings at
// ~3.3 kHz, a 5 mm one at ~660 Hz. Each ping is a short, exponentially-damped
// sinusoid with a small *upward* frequency chirp during the decay — the
// characteristic watery "bloop". Bubbles are emitted as a Poisson process.
//
// Two physical hooks make the layer feel like one body of water with the rest
// of the effect:
//   • Depth raises the ambient pressure P, so f0 ∝ √P — deeper bubbles ping
//     higher. (~1 atm per 10.33 m of water.)
//   • The whole bed is rolled off by a one-pole low-pass tracking the water's
//     absorption cutoff, so deep water also makes the bubbles darker.
//
// The generator is stereo: each bubble is panned at random (equal-power) so the
// stream spreads naturally across the image. It is host-agnostic, allocation-
// free and real-time safe — exactly like the rest of the DSP core.

#ifndef DEEPBLUE_BUBBLE_VOICES
#define DEEPBLUE_BUBBLE_VOICES 12   // overridden per build target (6 on the Dwarf)
#endif

class BubbleStream {
public:
    void init(double sr, uint32_t seed) noexcept {
        _sr  = sr;
        _rng = seed ? seed : 1u;
        reset();
        _bedG = 1.0f;
    }

    void reset() noexcept {
        for (auto& v : _v) v.active = false;
        _lpL = _lpR = 0.0f;
        // _rng deliberately keeps running across resets so the stream never
        // repeats the same pattern from one activation to the next.
    }

    // Refreshed once per block.
    //   density  [0–1]  Poisson rate of new bubbles (the "bubbles" knob).
    //   size     [0–1]  central bubble radius → register (small/fizzy → big/gloopy).
    //   depthM   metres of water → pressure → upward shift of every f0.
    //   bedCut   Hz     low-pass cutoff that darkens the bed (tracks absorption).
    void setParams(float density, float size, float depthM, float bedCut) noexcept {
        density = clamp01(density);
        _size   = clamp01(size);
        // Taper the low end so small knob values stay sparse and intimate.
        _rate   = density * density * MAX_RATE;
        _pSpawn = (float)(_rate / _sr);

        // √P pressure factor: ~×2.2 at 40 m.
        _pressF = std::sqrt(1.0f + std::max(0.0f, depthM) / 10.33f);

        // Keep the bed brighter than the guitar path — bubbles live up high, so
        // a one-pole tracking the raw absorption cutoff would erase them; this
        // still darkens with depth, just more gently.
        float fc = bedCut * 1.8f + 1200.0f;
        fc = clampf(fc, 600.0f, 0.45f * (float)_sr);
        _bedG = 1.0f - std::exp(-2.0f * (float)M_PI * fc / (float)_sr);
    }

    // True while any voice is still ringing — lets the caller skip the per-sample
    // work entirely when the layer is silent.
    bool active() const noexcept {
        for (const auto& v : _v) if (v.active) return true;
        return false;
    }

    // Advance one sample, returning the stereo bubble bed.
    void tick(float& outL, float& outR) noexcept {
        // Poisson emission.
        if (_pSpawn > 0.0f && uniform() < _pSpawn) spawn();

        float l = 0.0f, r = 0.0f;
        for (auto& v : _v) {
            if (!v.active) continue;

            const float s = v.env * std::sin(2.0f * (float)M_PI * v.phase);
            l += s * v.ampL;
            r += s * v.ampR;

            // Upward chirp: frequency glides toward its target as the ping fades.
            v.freq += (v.target - v.freq) * v.glide;
            v.phase += v.freq / (float)_sr;
            if (v.phase >= 1.0f) v.phase -= 1.0f;

            v.env *= v.decay;
            if (v.env < 1.0e-4f) v.active = false;
        }

        // Shared one-pole low-pass darkens the whole bed with depth.
        _lpL += _bedG * (l - _lpL);
        _lpR += _bedG * (r - _lpR);
        outL = _lpL;
        outR = _lpR;
    }

private:
    static constexpr float MAX_RATE = 32.0f;   // bubbles/second at full density

    struct Voice {
        bool  active = false;
        float phase  = 0.0f;
        float freq   = 0.0f;   // current (chirping) frequency, Hz
        float target = 0.0f;   // chirp destination, Hz
        float glide  = 0.0f;   // per-sample glide coefficient
        float env    = 0.0f;   // amplitude envelope
        float decay  = 0.0f;   // per-sample env multiplier
        float ampL   = 0.0f;
        float ampR   = 0.0f;
    };

    void spawn() noexcept {
        // Central radius from the size knob: 0.4 mm (fizzy ~8 kHz) → 4 mm
        // (gloopy ~0.8 kHz), each bubble jittered ±~1 octave around it.
        const float aCenter = 0.0004f * std::pow(10.0f, _size);          // metres
        const float jitter  = std::pow(2.0f, (uniform() * 2.0f - 1.0f)); // ±1 oct
        const float a       = aCenter * jitter;

        float f0 = (3.283f / a) * _pressF;                  // Minnaert × pressure
        f0 = clampf(f0, 100.0f, 0.45f * (float)_sr);

        const float chirp  = 0.30f + 0.35f * uniform();     // upward glide amount
        const float delta  = clampf(0.012f + 0.0009f * std::sqrt(f0), 0.012f, 0.09f);
        const float decay  = std::exp(-2.0f * (float)M_PI * delta * f0 / (float)_sr);

        // Equal-power random pan across the full image.
        const float pan = uniform() * 0.5f * (float)M_PI;   // 0 → π/2
        const float amp = 0.6f * (0.55f + 0.45f * uniform());

        Voice& v = pick();
        v.active = true;
        v.phase  = uniform();
        v.freq   = f0;
        v.target = f0 * (1.0f + chirp);
        v.glide  = 1.0f - decay;                            // chirp tracks the decay
        v.env    = 1.0f;
        v.decay  = decay;
        v.ampL   = amp * std::cos(pan);
        v.ampR   = amp * std::sin(pan);
    }

    // Free voice if any, else steal the quietest one.
    Voice& pick() noexcept {
        int quietest = 0;
        for (int i = 0; i < DEEPBLUE_BUBBLE_VOICES; ++i) {
            if (!_v[i].active) return _v[i];
            if (_v[i].env < _v[quietest].env) quietest = i;
        }
        return _v[quietest];
    }

    float uniform() noexcept {            // [0, 1)
        _rng = _rng * 1664525u + 1013904223u;
        return (float)(_rng >> 9) * (1.0f / 8388608.0f);
    }

    static float clamp01(float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
    static float clampf(float x, float lo, float hi) noexcept {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    double   _sr     = 48000.0;
    uint32_t _rng    = 1u;
    float    _size   = 0.4f;
    double   _rate   = 0.0;
    float    _pSpawn = 0.0f;
    float    _pressF = 1.0f;
    float    _bedG   = 1.0f;
    float    _lpL = 0.0f, _lpR = 0.0f;
    Voice    _v[DEEPBLUE_BUBBLE_VOICES];
};
