#pragma once
#include <cmath>
#include "dispersion.hpp"   // reuse AllpassFO / DispersionChain for decorrelation

// ── Underwater loss of directional hearing ───────────────────────────────────
//
// In air we localise a sound from the interaural time and level differences
// between our ears. Underwater this breaks down:
//   • sound travels ~4.4× faster (≈1500 m/s vs 343), so the time differences
//     shrink well below the ear's resolution — the main "where" cue vanishes;
//   • head tissue is nearly impedance-matched to water, so the head casts
//     almost no acoustic shadow (no level difference), and bone conduction
//     reaches both cochleas directly.
// A submerged listener therefore largely cannot tell where a sound comes from:
// it seems to arrive from everywhere at once, or from inside the head.
//
// StereoField recreates that perception. As the amount rises it does two things
// at once:
//   1. collapses the *coherent* image — the Side (L−R) component that carries
//      the directional cues — toward mono, removing the sense of "where";
//   2. substitutes an *incoherent*, decorrelated diffuse field: two different
//      all-pass scramblings of the Mid signal, so the sound stays wide and
//      enveloping rather than dryly mono — non-localisable, yet all around you.
//
// Host-agnostic, allocation-free, real-time safe — like the rest of the core.
class StereoField {
public:
    void init(double sr) noexcept {
        // Distinct spreads make the two diffusers decorrelated from each other
        // (the same trick the dispersion stage uses to widen L vs R).
        _diffL.setup((float)sr, 0.93f);
        _diffR.setup((float)sr, 1.11f);
    }
    void reset() noexcept { _diffL.reset(); _diffR.reset(); }

    // amount ∈ [0,1]. Processes one stereo sample in place. amount == 0 is a
    // transparent bypass (the diffusers are still advanced so engaging the knob
    // doesn't click).
    void process(float amount, float& l, float& r) noexcept {
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r);

        // Diffusers always run so their state stays warm across an amount sweep.
        const float dl = _diffL.process(m);
        const float dr = _diffR.process(m);

        if (amount <= 0.0f) return;

        // 1) Collapse the coherent (directional) Side component.
        const float sn = s * (1.0f - amount * 0.92f);
        const float nL = m + sn;
        const float nR = m - sn;

        // 2) Blend in the decorrelated diffuse field.
        const float a = amount * 0.6f;
        l = (1.0f - a) * nL + a * dl;
        r = (1.0f - a) * nR + a * dr;
    }

private:
    // A short cascade is plenty for decorrelation and stays light on the Dwarf
    // (all-pass sections are cheap); fixed rather than build-tuned for that.
    static constexpr int kStages = 6;
    DispersionChain<kStages> _diffL, _diffR;
};
