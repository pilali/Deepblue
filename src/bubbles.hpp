#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// ── Minnaert bubble filter-bank ──────────────────────────────────────────────
//
// A generative stream of bubble *resonances*. A gas bubble in water rings at
// its Minnaert frequency (Minnaert, 1933):
//
//     f0 = (1 / 2πa) · √(3γP / ρ)        a = radius, γ = 1.4, ρ = 1000 kg/m³
//
// which at the surface is ≈ 3.28 / a Hz (a in metres): a 0.4 mm bubble sits at
// ~8 kHz, a 40 mm one — a very big "gloop" — down at ~80 Hz.
//
// A bubble here is NOT a sound of its own: it is a filter applied to the
// signal. Each voice is a resonant band-pass (TPT / Zavalishin state-variable
// filter) whose centre frequency sits at the bubble's Minnaert resonance and
// glides *upward* as it fades (the characteristic watery chirp). The filter's
// damping is the bubble's own physical damping (k = 2δ → Q ≈ 5–40), so the
// voice *rings*: a transient excites it and it releases a decaying, chirping
// tone at f0 — the bloop — yet every drop of that energy came from the player.
// Silence in, silence out. Bubbles are emitted as a Poisson process.
//
// Feed the bank a full-bandwidth tap (the dry input, NOT the absorbed wet
// signal): small bubbles ring at 4–16 kHz, a band the absorption low-pass has
// already erased from the wet path — filtering the wet leaves them inaudible.
//
// Two physical hooks make the layer feel like one body of water with the rest
// of the effect:
//   • Depth raises the ambient pressure P, so f0 ∝ √P — deeper bubbles ring
//     higher. (~1 atm per 10.33 m of water.)
//   • The summed resonances are rolled off by a one-pole low-pass tracking the
//     water's absorption cutoff, so deep water also makes the bubbles darker.
//
// The bank is stereo: each bubble is panned at random (equal-power) so the
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
        for (auto& v : _v) { v.active = false; v.ic1 = v.ic2 = 0.0f; }
        _lpL = _lpR = 0.0f;
        // _rng deliberately keeps running across resets so the stream never
        // repeats the same pattern from one activation to the next.
    }

    // Refreshed once per block.
    //   density  [0–1]  Poisson rate of new bubbles (the "bubbles" knob).
    //   size     [0–1]  central bubble radius → register (small/fizzy → huge/gloopy).
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

    // Advance one sample. `in` is the (mono-folded) full-bandwidth source the
    // bubbles filter; outL/outR receive the stereo resonances to sum into the wet.
    void tick(float in, float& outL, float& outR) noexcept {
        // Poisson emission.
        if (_pSpawn > 0.0f && uniform() < _pSpawn) spawn();

        float l = 0.0f, r = 0.0f;
        for (auto& v : _v) {
            if (!v.active) continue;

            // TPT state-variable band-pass at the chirping Minnaert frequency.
            // Unconditionally stable, so the coefficient can glide per sample.
            const float g  = std::tan((float)M_PI * v.freq / (float)_sr);
            const float a1 = 1.0f / (1.0f + g * (g + v.res));
            const float bp = a1 * (v.ic1 + g * (in - v.ic2));
            const float lo = v.ic2 + g * bp;
            v.ic1 = 2.0f * bp - v.ic1;
            v.ic2 = 2.0f * lo - v.ic2;

            // ×res normalises the band peak to unity regardless of Q; MAKEUP
            // then lifts the narrow slice so the bloop sits on top of the wet.
            const float s = bp * v.res * MAKEUP * v.env;
            l += s * v.ampL;
            r += s * v.ampR;

            // Upward chirp: the resonance glides up as the bubble fades.
            v.freq += (v.target - v.freq) * v.glide;

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
    static constexpr float MAKEUP   = 5.0f;    // band-peak gain of one voice (≈ +14 dB)

    struct Voice {
        bool  active = false;
        float freq   = 0.0f;   // current (chirping) centre frequency, Hz
        float target = 0.0f;   // chirp destination, Hz
        float glide  = 0.0f;   // per-sample glide coefficient
        float res    = 0.1f;   // SVF damping k = 2δ (1/Q) — the physical ring
        float env    = 0.0f;   // amplitude envelope (slow gate over the ring)
        float decay  = 0.0f;   // per-sample env multiplier
        float ic1    = 0.0f;   // SVF integrator states
        float ic2    = 0.0f;
        float ampL   = 0.0f;
        float ampR   = 0.0f;
    };

    void spawn() noexcept {
        // Central radius from the size knob: 0.4 mm (fizzy ~8 kHz) → 40 mm
        // (a very big ~80 Hz gloop), each bubble jittered ±~1 octave around it.
        const float aCenter = 0.0004f * std::pow(100.0f, _size);          // metres
        const float jitter  = std::pow(2.0f, (uniform() * 2.0f - 1.0f)); // ±1 oct
        const float a       = aCenter * jitter;

        float f0 = (3.283f / a) * _pressF;                  // Minnaert × pressure
        f0 = clampf(f0, 30.0f, 0.35f * (float)_sr);

        const float chirp  = 0.30f + 0.35f * uniform();     // upward glide amount
        const float delta  = clampf(0.012f + 0.0009f * std::sqrt(f0), 0.012f, 0.09f);
        const float decay  = std::exp(-2.0f * (float)M_PI * delta * f0 / (float)_sr);

        // Guitar-like sources roll off steeply above the fundamental register,
        // which would starve the small (high) bubbles while the big (low) ones
        // sit right on the sustained fundamental and bloom: mirror that tilt so
        // every register rings at a comparable level. Referenced at 500 Hz,
        // −10 dB floor below it, +18 dB cap above.
        const float tilt = clampf(f0 * (1.0f / 500.0f), 0.3f, 8.0f);

        // Equal-power random pan across the full image.
        const float pan = uniform() * 0.5f * (float)M_PI;   // 0 → π/2
        const float amp = (0.55f + 0.45f * uniform()) * tilt;

        Voice& v = pick();
        v.active = true;
        v.freq   = f0;
        v.target = clampf(f0 * (1.0f + chirp), f0, 0.45f * (float)_sr);
        v.glide  = 1.0f - decay;                            // chirp tracks the ring
        v.res    = clampf(2.0f * delta, 0.024f, 0.18f);     // physical damping → Q
        v.env    = 1.0f;
        // The filter's own ring carries the physical decay; the envelope is a
        // half-rate gate on top so it ends the voice without shortening it.
        v.decay  = std::exp(-(float)M_PI * delta * f0 / (float)_sr);
        v.ic1    = 0.0f;                                    // fresh filter state
        v.ic2    = 0.0f;
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
