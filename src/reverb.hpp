#pragma once
#include <cmath>
#include "delayline.hpp"

// ── Deep, dark, diffuse underwater reverb ─────────────────────────────────────
//
// Underwater there is no dry "room": sound scatters and bounces between the
// pressure-release surface and the absorbing bottom into a dense, diffuse,
// dark reverberant field. This is a Feedback Delay Network — the lean, dense
// way to get that wash — built for *depth*:
//   • eight (configurable) mutually-detuned delay lines, longer than the
//     Freeverb combs so the body of water is bigger and the tail sits deeper;
//   • each read position is slowly modulated by its own quadrature LFO, so the
//     modes drift and the long tail stays liquid and chorused instead of
//     ringing on a fixed comb — the "moving water" smear;
//   • a stereo pre-delay opens a gap before the wash builds, the cue that the
//     reflecting walls are far away (a large, deep volume of water);
//   • a unitary Householder feedback matrix scatters energy between every line
//     each pass, building density fast and metallic-free;
//   • a one-pole low-pass inside each loop darkens every reflection, so the tail
//     loses its highs the way water swallows them — tied to the absorption, the
//     deeper the water the darker the tail;
//   • the feedback gain sets the decay time ("size" of the body of water), and
//     reaches further now (up to 0.985) so big settings sustain a long abyss.
//
// Stereo in / stereo out (cross-tapped for width). Host-agnostic, allocation
// happens only in init(); process() is real-time safe (the per-sample LFO is a
// cheap magic-circle oscillator, no std::sin in the loop).

#ifndef DEEPBLUE_REVERB_LINES
#define DEEPBLUE_REVERB_LINES 8   // overridden per build target (4 on the Dwarf)
#endif

class Reverb {
public:
    void init(double sr) noexcept {
        _sr = sr;
        const double k = sr / 44100.0;
        // Eight mutually-prime lengths, larger than Freeverb's combs so the
        // space is a bigger, deeper volume of water (~26–66 ms @44.1k).
        static const int base[8] =
            { 1153, 1399, 1601, 1867, 2113, 2371, 2647, 2909 };

        _mod = (float)std::lround(9.0 * k);          // ± LFO excursion, samples
        if (_mod < 1.0f) _mod = 1.0f;

        for (int i = 0; i < N; ++i) {
            _len[i] = (int)std::lround(base[i] * k);
            if (_len[i] < 4) _len[i] = 4;
            _d[i].init(_len[i] + (int)_mod + 8);
            _lp[i] = 0.0f;

            // Decorrelated slow modulation (0.18–0.55 Hz), one per line. The
            // magic-circle oscillator keeps a unit-amplitude sine for a couple
            // of multiplies per sample — no transcendental in the audio loop.
            const float hz  = 0.18f + 0.053f * (float)i;
            _eps[i] = 2.0f * std::sin((float)M_PI * hz / (float)_sr);
            _ms[i]  = std::sin((float)i * 0.9f);     // staggered phases
            _mc[i]  = std::cos((float)i * 0.9f);
        }

        // Stereo pre-delay: up to ~70 ms, read offset set per block from size.
        const int preMax = (int)std::lround(0.070 * sr) + 8;
        _preL.init(preMax);
        _preR.init(preMax);
        _preS = (float)std::lround(0.012 * sr);

        _g = 0.85f; _damp = 0.5f;
    }

    void reset() noexcept {
        for (int i = 0; i < N; ++i) {
            _d[i].reset(); _lp[i] = 0.0f;
            _ms[i] = std::sin((float)i * 0.9f);
            _mc[i] = std::cos((float)i * 0.9f);
        }
        _preL.reset(); _preR.reset();
    }

    // Refreshed per block.
    //   g     [0–1] feedback gain → decay time / "size".
    //   damp  [0–1] in-loop HF damping → darkness of the tail.
    //   size  [0–1] also stretches the pre-delay → a larger, deeper volume.
    void setParams(float g, float damp, float size) noexcept {
        _g    = g    < 0.0f ? 0.0f : (g    > 0.985f ? 0.985f : g);
        _damp = damp < 0.0f ? 0.0f : (damp > 0.95f  ? 0.95f  : damp);
        size  = size < 0.0f ? 0.0f : (size > 1.0f   ? 1.0f   : size);
        // 10 ms at the smallest, ~55 ms at the largest body of water.
        _preS = (float)(0.010 + 0.045 * size) * (float)_sr;
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept {
        // Pre-delay each input so the wash arrives after a gap — the cue of a
        // large, far-walled volume rather than a tight room.
        _preL.write(inL);
        _preR.write(inR);
        const float pL = _preL.read(_preS);
        const float pR = _preR.read(_preS);

        // Read every line at its modulated position.
        float s[8];
        for (int i = 0; i < N; ++i) {
            // magic-circle update: unit-amplitude quadrature sine.
            _ms[i] += _eps[i] * _mc[i];
            _mc[i] -= _eps[i] * _ms[i];
            float rd = (float)_len[i] + _mod * _ms[i];
            if (rd < 1.0f) rd = 1.0f;
            s[i] = _d[i].read(rd);
        }

        // One-pole low-pass in each feedback path (the "dark").
        const float a = _damp;
        float sum = 0.0f;
        for (int i = 0; i < N; ++i) {
            _lp[i] = s[i] * (1.0f - a) + _lp[i] * a;
            sum += _lp[i];
        }

        // Unitary Householder reflection: m_i = lp_i − (2/N)·Σlp.
        const float h = (2.0f / (float)N) * sum;

        // Inject the pre-delayed stereo input (alternating L/R bias for width)
        // and feed the scattered, decayed signal back.
        const float inA = pL * 0.5f, inB = pR * 0.5f;
        for (int i = 0; i < N; ++i) {
            const float in = (i & 1) ? inB : inA;
            _d[i].write(in + _g * (_lp[i] - h));
        }

        // Cross-tapped output: even lines to the left, odd to the right, for a
        // wide, enveloping field. Averaged so the level tracks the old 4-line.
        float oL = 0.0f, oR = 0.0f;
        for (int i = 0; i < N; i += 2) { oL += s[i]; oR += s[i + 1]; }
        const float norm = 2.0f / (float)N;          // = average of the N/2 taps
        outL = oL * norm;
        outR = oR * norm;
    }

private:
    static constexpr int N = DEEPBLUE_REVERB_LINES;
    static_assert(N >= 2 && N <= 8 && (N % 2 == 0),
                  "DEEPBLUE_REVERB_LINES must be an even count in [2,8]");

    double    _sr = 48000.0;
    DelayLine _d[N];
    int       _len[N] = { 0 };
    float     _lp[N]  = { 0 };
    float     _eps[N] = { 0 };          // LFO coefficients
    float     _ms[N]  = { 0 };          // LFO sine state
    float     _mc[N]  = { 0 };          // LFO cosine state
    float     _mod    = 1.0f;           // LFO excursion, samples
    DelayLine _preL, _preR;             // stereo pre-delay
    float     _preS   = 0.0f;           // pre-delay read offset, samples
    float     _g = 0.85f, _damp = 0.5f;
};
