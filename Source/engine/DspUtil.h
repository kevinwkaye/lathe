#pragma once

// Small DSP building blocks for Lathe. Plain C++17, no JUCE, so the engine builds anywhere
// (Windows x64, macOS, Linux ARM64 for the Pi rig) and can move to a headless host later.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lathe
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// tanh-like soft clipper: unity gain for small signals, never above 1
inline float softClip (float x)
{
    x = clampf (x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// Leaves everything under 0.9 alone (so a bass at -1 dBFS passes untouched), then bends
// smoothly to a ceiling of 1.0. Only there so nothing ever leaves the plugin above full scale.
inline float safetyLimit (float x)
{
    const float a = std::abs (x);
    if (a <= 0.9f) return x;
    const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
    return x < 0.0f ? -y : y;
}

// One-pole smoother with a time constant in seconds.
struct Smoother
{
    float y = 0.0f, a = 0.0f;
    void setTime (float seconds, double sr) { a = seconds <= 0.0f ? 0.0f : std::exp (-1.0f / (float) (seconds * sr)); }
    void reset (float v) { y = v; }
    float next (float target) { y = target + a * (y - target); return y; }
};

// xorshift noise, -1..1
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 0x9e3779b9u) : s (seed ? seed : 1u) {}
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (s & 0xffffffu) / 8388608.0f - 1.0f;
    }
};

// Slow random wander, -1..1: a new random target every 1/rate seconds, smoothed.
// Real platters and BBD clocks are never perfectly steady.
struct Wander
{
    Rng rng;
    Smoother sm;
    float target = 0.0f;
    int countdown = 0, period = 1;
    void prepare (double sr, float rateHz, uint32_t seed)
    {
        rng = Rng (seed);
        period = std::max (1, (int) (sr / std::max (0.01f, rateHz)));
        sm.setTime (0.6f / std::max (0.01f, rateHz), sr);
        sm.reset (0.0f);
        countdown = 0;
    }
    float next()
    {
        if (--countdown <= 0) { target = rng.next(); countdown = period; }
        return sm.next (target);
    }
};

// RBJ biquad: low-pass, high-pass and high shelf
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setLowPass (float hz, float q, double sr)
    {
        hz = clampf (hz, 20.0f, 0.45f * (float) sr);
        const float w = kTwoPi * hz / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        const float a0 = 1.0f + al;
        b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0;
        a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    void setHighPass (float hz, float q, double sr)
    {
        hz = clampf (hz, 5.0f, 0.45f * (float) sr);
        const float w = kTwoPi * hz / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        const float a0 = 1.0f + al;
        b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0;
        a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    void setHighShelf (float hz, float db, double sr)
    {
        hz = clampf (hz, 20.0f, 0.45f * (float) sr);
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * hz / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / 2.0f * std::sqrt (2.0f);
        const float sa = 2.0f * std::sqrt (A) * al;
        const float a0 = (A + 1) - (A - 1) * c + sa;
        b0 = A * ((A + 1) + (A - 1) * c + sa) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        b2 = A * ((A + 1) + (A - 1) * c - sa) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        a2 = ((A + 1) - (A - 1) * c - sa) / a0;
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }
    // take another filter's coefficients but keep this one's memory
    void coeffsFrom (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
};

// Linkwitz-Riley 4th-order crossover: low + high sums to an all-pass, so splitting the
// bass off and putting it back leaves the level flat.
struct Crossover
{
    Biquad lp1, lp2, hp1, hp2;
    float hz = -1.0f;
    void set (float f, double sr)
    {
        if (std::abs (f - hz) < 0.5f) return;
        hz = f;
        const float q = 0.70710678f;
        lp1.setLowPass (f, q, sr); lp2.coeffsFrom (lp1);
        hp1.setHighPass (f, q, sr); hp2.coeffsFrom (hp1);
    }
    void process (float x, float& lo, float& hi)
    {
        lo = lp2.process (lp1.process (x));
        hi = hp2.process (hp1.process (x));
    }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
};

struct DcBlock
{
    float x1 = 0.0f, y1 = 0.0f;
    float process (float x) { const float y = x - x1 + 0.9995f * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

// Power-of-two circular delay line with 4-point Hermite interpolation: smooth enough that a
// slow, deep vibrato stays clean, with no zipper noise or dulling at fractional delays.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int minSize)
    {
        int n = 1;
        while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // delay in samples, at least 2 (the sample just written is delay 1)
    float read (float delay) const
    {
        delay = clampf (delay, 2.0f, (float) (mask - 4));
        const float pos = (float) w - delay;
        const float fl = std::floor (pos);
        const int i1 = (int) fl & mask;
        const float f = pos - fl;
        const float xm1 = buf[(size_t) ((i1 - 1) & mask)], x0 = buf[(size_t) i1];
        const float x1 = buf[(size_t) ((i1 + 1) & mask)], x2 = buf[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};
} // namespace lathe
