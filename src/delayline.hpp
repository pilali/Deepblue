#pragma once
#include <vector>
#include <cmath>

// Power-of-two ring buffer with 4-point Hermite (Catmull-Rom) fractional read.
// Used by the wobble: a slowly modulated read position turns a fixed-pitch
// signal into a gently wavering one — the watery "light through water" pitch
// drift.
class DelayLine {
public:
    void init(int maxSamples) {
        _size = 1;
        while (_size < maxSamples + 4) _size <<= 1;
        _mask = _size - 1;
        _buf.assign((size_t)_size, 0.0f);
        _w = 0;
    }

    void reset() noexcept {
        std::fill(_buf.begin(), _buf.end(), 0.0f);
        _w = 0;
    }

    void write(float x) noexcept {
        _buf[(size_t)_w] = x;
        _w = (_w + 1) & _mask;
    }

    // Read `delay` samples in the past (delay >= 1), Hermite-interpolated.
    float read(float delay) const noexcept {
        float rp = (float)_w - 1.0f - delay;
        int   i  = (int)std::floor(rp);
        float f  = rp - (float)i;

        float ym1 = at(i - 1), y0 = at(i), y1 = at(i + 1), y2 = at(i + 2);
        float c0 = y0;
        float c1 = 0.5f * (y1 - ym1);
        float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

private:
    // delay ∈ [1, max] and _w ∈ [0, _size) ⇒ rp > -(_size), so adding 2·_size
    // before masking keeps every index non-negative.
    float at(int idx) const noexcept { return _buf[(size_t)((idx + (_size << 1)) & _mask)]; }

    std::vector<float> _buf;
    int _size = 0, _mask = 0, _w = 0;
};
