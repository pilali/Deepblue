#pragma once
#include <cmath>
#include "delayline.hpp"

// ── Dark diffuse underwater reverb ────────────────────────────────────────────
//
// Underwater there is no dry "room": sound scatters and bounces between the
// pressure-release surface and the absorbing bottom into a dense, diffuse,
// dark reverberant field. This is a 4-line Feedback Delay Network — the lean,
// dense way to get that wash:
//   • four mutually-detuned delay lines (Freeverb-derived lengths, scaled to
//     the sample rate) keep the tail smooth and metallic-free;
//   • a unitary Householder feedback matrix scatters energy between them every
//     pass, building density fast;
//   • a one-pole low-pass inside the loop darkens each reflection, so the tail
//     loses its highs the way water swallows them — tied to the absorption, the
//     deeper the water the darker the tail;
//   • the feedback gain sets the decay time ("size" of the body of water).
//
// Stereo in / stereo out (cross-tapped for width). Host-agnostic, allocation
// happens only in init(); process() is real-time safe.
class Reverb {
public:
    void init(double sr) noexcept {
        // Freeverb comb tunings (tuned at 44.1 kHz), scaled to the host rate.
        static const int base[N] = { 1116, 1277, 1491, 1617 };
        const double k = sr / 44100.0;
        for (int i = 0; i < N; ++i) {
            _len[i] = (int)std::lround(base[i] * k);
            if (_len[i] < 2) _len[i] = 2;
            _d[i].init(_len[i]);
            _lp[i] = 0.0f;
        }
        _g = 0.7f; _damp = 0.5f;
    }

    void reset() noexcept {
        for (int i = 0; i < N; ++i) { _d[i].reset(); _lp[i] = 0.0f; }
    }

    // Refreshed per block.
    //   g     [0–1] feedback gain → decay time / "size".
    //   damp  [0–1] in-loop HF damping → darkness of the tail.
    void setParams(float g, float damp) noexcept {
        _g    = g    < 0.0f ? 0.0f : (g    > 0.97f ? 0.97f : g);
        _damp = damp < 0.0f ? 0.0f : (damp > 0.95f ? 0.95f : damp);
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept {
        float s0 = _d[0].read((float)_len[0]);
        float s1 = _d[1].read((float)_len[1]);
        float s2 = _d[2].read((float)_len[2]);
        float s3 = _d[3].read((float)_len[3]);

        // One-pole low-pass in each feedback path (the "dark").
        const float a = _damp;
        _lp[0] = s0 * (1.0f - a) + _lp[0] * a;
        _lp[1] = s1 * (1.0f - a) + _lp[1] * a;
        _lp[2] = s2 * (1.0f - a) + _lp[2] * a;
        _lp[3] = s3 * (1.0f - a) + _lp[3] * a;

        // Unitary Householder reflection: m = lp − (2/N)·Σlp  (2/N = 0.5).
        const float h = 0.5f * (_lp[0] + _lp[1] + _lp[2] + _lp[3]);
        const float m0 = _lp[0] - h, m1 = _lp[1] - h,
                    m2 = _lp[2] - h, m3 = _lp[3] - h;

        // Inject the stereo input and feed the scattered, decayed signal back.
        const float inA = inL * 0.5f, inB = inR * 0.5f;
        _d[0].write(inA + _g * m0);
        _d[1].write(inB + _g * m1);
        _d[2].write(inA + _g * m2);
        _d[3].write(inB + _g * m3);

        // Cross-tapped output for a wide, enveloping field.
        outL = (s0 + s3) * 0.5f;
        outR = (s1 + s2) * 0.5f;
    }

private:
    static constexpr int N = 4;
    DelayLine _d[N];
    int       _len[N] = { 0, 0, 0, 0 };
    float     _lp[N]  = { 0, 0, 0, 0 };
    float     _g = 0.7f, _damp = 0.5f;
};
