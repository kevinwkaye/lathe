#pragma once

// Chase Bliss-style motion, shared between Lathe and Murmur (both recreations of that
// maker's pedals). Plain C++17 with no dependencies, so either plugin can copy it unchanged.
//
//  - ModuShape: a wave built from two halves, each sine, triangle or square, with Warp
//    moving the peak so the rise and fall lean either way (Warped Vinyl's LFO).
//  - Ramp: the dip-switch ramp. Each chosen control travels between its knob and one end
//    of its range (Sweep top or bottom), rising or falling. Ramp-and-hold runs once when the
//    effect switches on; Bounce goes back and forth; Hold travels while a footswitch is
//    held and glides back on release. Expression in "ramp targets" mode uses the same path.
//  - Divisions: the 1 2 4 and 3 6 8 tap divisions, as cycles per bar of 4/4.
//
// Murmur's RampEngine (Source/engine/Modulation.h) is the same idea with one direction for
// every target; Ramp here adds per-target rise or fall, hold and expression.

#include <algorithm>
#include <cmath>

namespace chase
{
constexpr float kPi = 3.14159265358979f;

inline float clamp01 (float v) { return std::min (1.0f, std::max (0.0f, v)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep (float t) { t = clamp01 (t); return t * t * (3.0f - 2.0f * t); }

// ---------------------------------------------------------------------------------------
// ModuShape
// ---------------------------------------------------------------------------------------
enum Shape { Sine = 0, Triangle = 1, Square = 2 };

// One half of the wave as a rise from 0 to 1. Square rises over 4% of the half so it
// never clicks, which is also how the analog pedal's square sounds.
inline float halfRise (int shape, float t)
{
    t = clamp01 (t);
    switch (shape)
    {
        case Sine: return 0.5f - 0.5f * std::cos (kPi * t);
        case Square: return smoothstep (t / 0.04f);
        default: return t;
    }
}

// warp 0..1: 0 rises fast and falls slowly, 1 rises slowly and falls fast, 0.5 is even.
inline float peakPoint (float warp) { return lerp (0.06f, 0.94f, clamp01 (warp)); }

// The wave at phase 0..1, from -1 (trough) to +1 (peak). The first half rises with the
// first shape, the second half falls with the second shape.
inline float moduShape (float phase, int firstHalf, int secondHalf, float warp)
{
    phase -= std::floor (phase);
    const float w = peakPoint (warp);
    if (phase < w) return 2.0f * halfRise (firstHalf, phase / w) - 1.0f;
    return 1.0f - 2.0f * halfRise (secondHalf, (phase - w) / (1.0f - w));
}

// ---------------------------------------------------------------------------------------
// Tap divisions: cycles of the wave per bar of 4/4.
// 1 2 4 are whole, half and quarter notes; 3 6 8 (the "3" dip) are read as triplets and
// eighths: a half-note triplet, a quarter-note triplet and an eighth note.
// ---------------------------------------------------------------------------------------
inline float cyclesPerBar (int division /*0,1,2*/, bool alt368)
{
    static const float normal[3] = { 1.0f, 2.0f, 4.0f }, alt[3] = { 3.0f, 6.0f, 8.0f };
    const int i = std::min (2, std::max (0, division));
    return alt368 ? alt[i] : normal[i];
}
inline float divisionHz (float bpm, int division, bool alt368) { return bpm / 240.0f * cyclesPerBar (division, alt368); }
// Ramping runs twice as slow as the wave, as on the pedal.
inline float divisionRampSeconds (float bpm, int division, bool alt368)
{
    return 2.0f * 240.0f / std::max (1.0f, bpm) / cyclesPerBar (division, alt368);
}

// ---------------------------------------------------------------------------------------
// Ramp
// ---------------------------------------------------------------------------------------
// Where a target sits for path position p (0..1). The path runs between the knob and the
// sweep end (top = fully clockwise, bottom = fully anticlockwise): rising goes from the lower
// of the two to the higher, falling the other way. The knob always marks one end.
inline float rampPath (float knob, float p, bool rise, bool sweepTop)
{
    const float far = sweepTop ? 1.0f : 0.0f;
    const float lo = std::min (knob, far), hi = std::max (knob, far);
    return rise ? lerp (lo, hi, p) : lerp (hi, lo, p);
}

class Ramp
{
public:
    enum Mode { Resting, Running, Bouncing };

    struct Settings
    {
        bool bounce = false;
        float seconds = 2.0f;   // one sweep
    };

    // The pedal ramps whenever it switches on, and resets when bypassed.
    void engage() { p = 0.0f; dir = 1.0f; running = true; }
    void fire() { engage(); }
    void reset() { p = 1.0f; running = false; dir = 1.0f; hold = 0.0f; }
    void setHeld (bool h) { held = h; }

    // Advance by dt seconds.
    void tick (float dt, const Settings& s)
    {
        const float step = dt / std::max (0.05f, s.seconds);
        if (s.bounce)
        {
            running = true;
            p += dir * step;
            if (p >= 1.0f) { p = 1.0f; dir = -1.0f; }
            if (p <= 0.0f) { p = 0.0f; dir = 1.0f; }
        }
        else if (running)
        {
            p = std::min (1.0f, p + step);
            if (p >= 1.0f) running = false;
        }
        // hold: travel to the sweep end over one ramp time, glide back in 0.4 s
        hold = held ? std::min (1.0f, hold + step) : std::max (0.0f, hold - dt / 0.4f);
    }

    // The value a target takes. move: this target is in the ramp bank.
    float apply (float knob, bool move, bool rise, bool sweepTop) const
    {
        if (! move) return knob;
        float v = rampPath (knob, smoothstep (p), rise, sweepTop);
        if (hold > 0.0f) v = lerp (v, sweepTop ? 1.0f : 0.0f, smoothstep (hold));
        return v;
    }

    bool isMoving (bool bounce) const { return running || bounce || hold > 0.001f; }
    bool isHeld() const { return held || hold > 0.001f; }
    float position() const { return p; }

private:
    float p = 1.0f, dir = 1.0f, hold = 0.0f;
    bool running = false, held = false;
};
} // namespace chase
