#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// Plain sine LFO in [-1, 1].
class SineLFO {
public:
    void init(double sr) noexcept { _sr = sr; _phase = 0.0; _inc = 0.0; }
    void reset(double phase0 = 0.0) noexcept { _phase = phase0; }
    void setPhase(double p) noexcept { _phase = p; }
    void setRate(float hz) noexcept { _inc = (double)hz / _sr; }

    float tick() noexcept {
        float v = std::sin(2.0 * M_PI * _phase);
        _phase += _inc;
        if (_phase >= 1.0) _phase -= 1.0;
        return v;
    }

private:
    double _sr = 48000.0, _phase = 0.0, _inc = 0.0;
};

// Smoothed value-noise drift in [-1, 1]. New random targets are picked at the
// requested rate and smoothstep-interpolated between them — an organic,
// non-periodic companion to the sine so the wobble never sounds like a plain
// vibrato. Cheap LCG, deterministic per seed (so L/R can be decorrelated).
class RandomLFO {
public:
    void init(double sr, uint32_t seed = 22222u) noexcept {
        _sr = sr; _state = seed; _stepSamples = 4800; _ctr = 0;
        _prev = 0.0f; _target = next();
    }
    void reset() noexcept { _prev = _target = 0.0f; _ctr = 0; }
    void setRate(float hz) noexcept {
        _stepSamples = (int)(_sr / std::max(0.01f, hz));
        if (_stepSamples < 1) _stepSamples = 1;
    }

    float tick() noexcept {
        if (_ctr <= 0) { _prev = _target; _target = next(); _ctr = _stepSamples; }
        float a = 1.0f - (float)_ctr / (float)_stepSamples;   // 0 → 1 across the step
        a = a * a * (3.0f - 2.0f * a);                        // smoothstep
        --_ctr;
        return _prev + (_target - _prev) * a;
    }

private:
    float next() noexcept {
        _state = _state * 1664525u + 1013904223u;
        return ((float)(_state >> 9) * (1.0f / 8388608.0f)) - 1.0f;  // [-1, 1)
    }

    double   _sr = 48000.0;
    uint32_t _state = 22222u;
    float    _prev = 0.0f, _target = 0.0f;
    int      _stepSamples = 4800, _ctr = 0;
};
