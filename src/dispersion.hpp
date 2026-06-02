#pragma once
#include <cmath>

// First-order allpass: H(z) = (a + z^-1) / (1 + a·z^-1). Unity magnitude,
// frequency-dependent phase (and group delay). Chained, these approximate the
// dispersive propagation of sound in a shallow-water waveguide, where low and
// high frequencies travel at different group velocities — the source of the
// eerie descending "wooo" of distant underwater sounds.
class AllpassFO {
public:
    void setFreq(float fc, float sr) noexcept {
        float t = std::tan(float(M_PI) * fc / sr);
        _a = (t - 1.0f) / (t + 1.0f);
    }
    float process(float x) noexcept {
        float y = _a * x + _x1 - _a * _y1;
        _x1 = x; _y1 = y;
        return y;
    }
    void reset() noexcept { _x1 = _y1 = 0.0f; }

private:
    float _a = 0.0f, _x1 = 0.0f, _y1 = 0.0f;
};

// Cascade of N first-order allpass sections with break frequencies spread
// logarithmically across the band. N is a compile-time knob so the MOD Dwarf
// (Cortex-A35) can run a short, cheap chain while the Pi 5 / desktop builds
// run a long, richly dispersive one. The wet/dry of the chain is mixed by the
// caller; mixing an allpass output back with its input yields the smeared,
// phaser-like comb that reads as "underwater" rather than merely "muffled".
template <int N>
class DispersionChain {
public:
    // spread shifts every break frequency by a constant factor — used to
    // decorrelate the left and right channels in the stereo path.
    void setup(float sr, float spread = 1.0f) noexcept {
        const float lo = 80.0f, hi = 6000.0f;
        for (int i = 0; i < N; ++i) {
            float t  = (N > 1) ? (float)i / (float)(N - 1) : 0.0f;
            float fc = lo * std::pow(hi / lo, t) * spread;
            if (fc > 0.45f * sr) fc = 0.45f * sr;   // keep below Nyquist
            _ap[i].setFreq(fc, sr);
        }
    }
    void reset() noexcept { for (auto& a : _ap) a.reset(); }
    float process(float x) noexcept {
        for (int i = 0; i < N; ++i) x = _ap[i].process(x);
        return x;
    }

private:
    AllpassFO _ap[N];
};
