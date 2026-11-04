#pragma once

// Lathe's DSP engine: a hi-fi recreation of a digitally controlled analog (BBD) chorus and
// vibrato, written from public descriptions of the pedal's behaviour. Plain C++17 with no
// JUCE, so it builds anywhere and runs in the offline render test.
//
// Signal path, per channel:
//   input -> [BASS: crossover, lows bypass everything] -> modulated delay (Lag + Depth,
//   shaped by the ModuShape wave) -> BBD clock roll-off -> hiss and compander breathing ->
//   gentle BBD saturation -> Tone -> Mix with the dry -> Volume (JFET grit above unity)
//   -> bypass crossfade -> safety limit
//
// The knobs the dip switches pick are moved by chase::Ramp (shared with Murmur): ramp and
// hold when the effect switches on, bounce, hold while footswitch C is down, or expression.

#include "ChaseMotion.h"
#include "DspUtil.h"
#include <array>
#include <atomic>

namespace lathe
{
enum Voice { HiFi = 0, MkII = 1 };
enum Output { Mono = 0, Miso = 1, Stereo = 2 };
enum HoldMode { HoldRamp = 0, HoldPause = 1, HoldFlurry = 2 };
// The controls the ramp bank can move, in the order of the panel's ramp table.
enum Target { TVolume = 0, TLag, TMix, TRate, TDepth, TWarp, TTone, kTargets };

// ---- knob laws, shared by the engine, the parameter text and the panel ----
inline float rateHzFromKnob (float k) { return 0.1f * std::pow (120.0f, clampf (k, 0, 1)); }   // 0.1 to 12 Hz
inline float knobFromRateHz (float hz) { return clampf (std::log (std::max (0.1f, hz) / 0.1f) / std::log (120.0f), 0, 1); }
inline float lagMs (float k, bool lofi) { return std::pow (25.0f, clampf (k, 0, 1)) + (lofi ? 12.0f : 0.0f); }   // 1 to 25 ms
inline float rampSecondsFromKnob (float k) { return 0.1f * std::pow (300.0f, clampf (k, 0, 1)); }   // 0.1 to 30 s
// Depth sets the pitch swing, so the vibrato stays musical at any rate: about +-1.3
// semitones at full depth, more with square halves and a hard Warp.
inline float depthSeconds (float depth, float hz)
{
    const float d = std::pow (clampf (depth, 0, 1), 1.6f);
    return d * clampf (0.075f / (kTwoPi * std::max (0.05f, hz)), 0.0005f, 0.09f);
}
inline float swingCents (float depth, float hz)
{
    const float slope = kTwoPi * hz * depthSeconds (depth, hz) * 0.5f * 1.27f;   // peak pitch deviation of a sine
    return 1200.0f * std::log2 (1.0f + slope);
}
// Volume: unity at 0.7 (about 2 o'clock), up to +12 dB, off at the bottom
inline float volumeDb (float v)
{
    if (v >= 0.7f) return (v - 0.7f) / 0.3f * 12.0f;
    if (v < 0.005f) return -120.0f;
    return 40.0f * std::log10 (v / 0.7f);
}

struct EngineParams
{
    // the knobs, 0..1
    float tone = 0.5f, lag = 0.55f, mix = 0.5f, volume = 0.7f;
    float rate = 0.45f, depth = 0.35f, warp = 0.5f, ramp = 0.4f;
    int wave1 = chase::Triangle, wave2 = chase::Triangle;
    bool active = true;
    int voice = HiFi;
    bool lofi = false;

    // the ramp bank
    std::array<bool, kTargets> move {};
    std::array<bool, kTargets> rise { true, true, true, true, true, false, true };
    bool bounce = false, sweepTop = false;
    float rateHz = -1.0f;        // tempo sync: overrides the Rate knob when > 0
    float rampSeconds = -1.0f;   // tap control on ramp: overrides the Ramp knob when > 0

    // footswitch moves
    int holdMode = HoldRamp;
    bool holdDown = false;       // footswitch C held
    bool brake = false;          // footswitch B held

    // expression straight into the engine (the processor handles Morph and single knobs)
    float rampExpression = -1.0f;   // 0..1: the original's expression on the ramp targets
    float whammy = -1.0f;           // 0..1: the waveform moved directly (nothing in the ramp bank)

    // rig
    bool bass = false;
    float bassHz = 150.0f;
    int output = Miso;
    float width = 0.7f;
    float noise = 1.0f;             // 1 = as the pedal, 0 silent, 2 twice
};

class LatheEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxDelay = (int) (2.6 * sr) + 16;
        for (auto& c : ch)
        {
            c.line.prepare (maxDelay);
            c.low.prepare ((int) (0.2 * sr) + 16);
            c.xover.reset();
            c.xover.hz = -1.0f;
            c.bbd1.reset(); c.bbd2.reset(); c.toneLp.reset(); c.toneHp.reset(); c.shelf.reset();
            c.dc.reset();
            c.lfoSm.setTime (0.0015f, sr);
            c.lfoSm.reset (0.0f);
            c.env = 0.0f;
        }
        ch[1].noise = Rng (0x51ed2701u);
        rateDrift.prepare (sr, 0.7f, 0x1234567u);
        delayDrift.prepare (sr, 4.3f, 0x7654321u);
        lagSm.setTime (0.08f, sr);
        ampSm.setTime (0.05f, sr);
        mixSm.setTime (0.02f, sr);
        volSm.setTime (0.02f, sr);
        activeSm.setTime (0.008f, sr);
        widthSm.setTime (0.05f, sr);
        lowDelaySm.setTime (0.12f, sr);
        brakeMixSm.setTime (0.03f, sr);
        first = true;
        brakeK = 1.0f;
        brakeDelay = 0.0f;
        rateMul = 1.0f;
        phase = 0.0;
    }

    // footswitch C tap / the RAMP button: run the ramp once from the start
    void fireRamp() { ramp.fire(); }
    // A+B: knobs back to where they're set, the wave restarts
    void resetRamp() { ramp.reset(); phase = 0.0; }

    // in and out may be the same buffers
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p)
    {
        const float dt = (float) n / (float) sr;

        if (first)
        {
            first = false;
            wasActive = p.active;
            const float e[kTargets] = { p.volume, p.lag, p.mix, p.rate, p.depth, p.warp, p.tone };
            for (int t = 0; t < kTargets; ++t) eff[(size_t) t] = e[t];
            lagSm.reset (lagMs (p.lag, p.lofi) * 0.001f);
            ampSm.reset (depthSeconds (p.depth, rateHzFromKnob (p.rate)));
            mixSm.reset (p.mix);
            volSm.reset (dbToGain (volumeDb (p.volume)));
            activeSm.reset (p.active ? 1.0f : 0.0f);
            widthSm.reset (p.output == Mono ? 0.0f : p.width);
            if (p.active) ramp.engage();
        }

        // ramping always resets on bypass, and runs again when the effect comes on
        if (p.active && ! wasActive) ramp.engage();
        if (! p.active && wasActive) ramp.reset();
        wasActive = p.active;

        // ---- the ramp bank ----
        chase::Ramp::Settings rs;
        rs.bounce = p.bounce;
        rs.seconds = p.rampSeconds > 0.0f ? p.rampSeconds : rampSecondsFromKnob (p.ramp);
        ramp.setHeld (p.holdDown && p.holdMode == HoldRamp);
        bool anyMove = false;
        for (bool m : p.move) anyMove = anyMove || m;
        if (anyMove) ramp.tick (dt, rs);

        const float knobs[kTargets] = { p.volume, p.lag, p.mix, p.rate, p.depth, p.warp, p.tone };
        for (int t = 0; t < kTargets; ++t)
        {
            const bool mv = p.move[(size_t) t];
            if (p.rampExpression >= 0.0f && mv)
                eff[(size_t) t] = chase::rampPath (knobs[t], p.rampExpression, p.rise[(size_t) t], p.sweepTop);
            else
                eff[(size_t) t] = ramp.apply (knobs[t], mv, p.rise[(size_t) t], p.sweepTop);
        }

        // ---- rate, brake, pause and flurry ----
        const float baseHz = (p.rateHz > 0.0f && ! p.move[TRate]) ? p.rateHz : rateHzFromKnob (eff[TRate]);
        const bool pause = p.holdDown && p.holdMode == HoldPause;
        const bool flurry = p.holdDown && p.holdMode == HoldFlurry;
        brakeK = p.brake ? std::max (0.0f, brakeK - dt / 1.4f) : std::min (1.0f, brakeK + dt / 0.6f);
        const float mulTarget = pause ? 0.0f : (flurry ? 4.0f : 1.0f);
        rateMul += (mulTarget - rateMul) * std::min (1.0f, dt / 0.08f);
        const float hz = baseHz * rateMul * brakeK;

        // ---- per-block filter settings ----
        const bool mk2 = p.voice == MkII;
        const float lagS = lagMs (eff[TLag], p.lofi) * 0.001f;
        const float ampS = depthSeconds (eff[TDepth], std::max (0.05f, baseHz));
        const float meanDelay = lagSm.y + ampSm.y;
        // a BBD's clock slows as its delay grows, and its filter closes with it
        const float stages = mk2 ? 1024.0f : 2048.0f;
        const float bbdHz = clampf (0.42f * stages / (2.0f * std::max (0.0005f, meanDelay)), 1500.0f, 20000.0f) * (p.lofi ? 0.55f : 1.0f);
        const float tone = eff[TTone];
        float lpHz, hpHz = 10.0f, shelfDb = 0.0f;
        if (mk2) lpHz = 700.0f * std::pow (20000.0f / 700.0f, tone);   // dark and murky to shimmery, never touching the lows
        else if (tone < 0.5f) lpHz = 1200.0f * std::pow (20000.0f / 1200.0f, tone * 2.0f);
        else
        {
            lpHz = 20000.0f;
            const float t = (tone - 0.5f) * 2.0f;
            hpHz = 20.0f * std::pow (350.0f / 20.0f, t);   // gentle: 12 dB per octave at most
            shelfDb = 3.0f * t;
        }
        for (auto& c : ch)
        {
            c.bbd1.setLowPass (bbdHz, 0.54f, sr); c.bbd2.setLowPass (bbdHz, 1.31f, sr);
            c.toneLp.setLowPass (std::min (lpHz, 0.45f * (float) sr), 0.707f, sr);
            c.toneHp.setHighPass (hpHz, 0.707f, sr);
            c.shelf.setHighShelf (6000.0f, shelfDb, sr);
            c.xover.set (clampf (p.bassHz, 40.0f, 500.0f), sr);
        }
        // hiss, as on the pedal: more with Lag and with Tone past noon, more in mkII, most with Lo-Fi
        float hissDb = mk2 ? -79.0f : -88.0f;
        if (p.lofi) hissDb += 9.0f;
        const float hiss = dbToGain (hissDb) * (0.45f + 1.1f * eff[TLag]) * (1.0f + 2.0f * std::max (0.0f, tone - 0.5f)) * clampf (p.noise, 0, 2);
        const float colour = p.lofi ? 0.6f : (mk2 ? 0.3f : 0.1f);   // how much BBD saturation is blended in
        const float volGain = dbToGain (volumeDb (eff[TVolume]));
        const float widthTarget = p.output == Mono ? 0.0f : clampf (p.width, 0, 1);
        const float driftAmt = mk2 ? 1.0f : 0.5f;
        const float brakeAmt = 1.0f - brakeK;
        const float brakeGainTarget = clampf (brakeK * 4.0f, 0.0f, 1.0f);
        const bool stereoIn = p.output == Stereo;
        const int shape1 = p.wave1, shape2 = p.wave2;
        const float warp = eff[TWarp];
        const float maxBrake = (float) (1.6 * sr);
        float peak = 0.0f, lfoShown = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float xl = inL[i], xr = inR[i];
            const float mono = 0.5f * (xl + xr);

            // the wave
            const float drift = rateDrift.next() * 0.006f * driftAmt;
            phase += (double) (hz * (1.0f + drift)) / sr;
            phase -= std::floor (phase);
            const float lag = lagSm.next (lagS), amp = ampSm.next (ampS);
            const float wdt = widthSm.next (widthTarget);
            const float jitter = delayDrift.next() * 0.00004f * driftAmt;

            // brake: the platter slows and stops (pitch falls), then spins back up
            if (p.brake) brakeDelay = std::min (maxBrake, brakeDelay + brakeAmt);
            else brakeDelay = std::max (0.0f, brakeDelay - 0.55f);
            const float bmix = brakeMixSm.next (p.brake ? 1.0f : 0.0f);
            const float bgain = brakeGainTarget;

            const float mix = mixSm.next (eff[TMix]);
            const float m = mix + (1.0f - mix) * bmix;   // braking takes the whole record with it
            const float vol = volSm.next (volGain);
            const float act = activeSm.next (p.active ? 1.0f : 0.0f);
            const float lowDelay = lowDelaySm.next (m * (lag + amp) * (float) sr);

            float out[2];
            for (int k = 0; k < 2; ++k)
            {
                auto& c = ch[(size_t) k];
                const float x = stereoIn ? (k == 0 ? xl : xr) : mono;
                float lo = 0.0f, hi = x;
                if (p.bass) c.xover.process (x, lo, hi);

                float lfo;
                if (p.whammy >= 0.0f) lfo = 2.0f * clampf (p.whammy, 0, 1) - 1.0f;
                else lfo = chase::moduShape ((float) phase + (k == 1 ? 0.5f * wdt : 0.0f), shape1, shape2, warp);
                lfo = c.lfoSm.next (lfo);
                if (k == 0) lfoShown = lfo;

                c.line.write (hi);
                const float d = (lag + amp * (1.0f + lfo) + jitter) * (float) sr + brakeDelay;
                float w = c.line.read (d);
                w = c.bbd2.process (c.bbd1.process (w));
                c.env += (std::abs (w) - c.env) * (std::abs (w) > c.env ? 0.004f : 0.0003f);
                w += c.noise.next() * hiss * (0.3f + 0.7f * std::min (1.0f, c.env * 8.0f));
                w += (softClip (w) - w) * colour;
                w = c.shelf.process (c.toneHp.process (c.toneLp.process (w)));
                w = c.dc.process (w) * bgain;

                float y;
                if (p.bass)
                {
                    c.low.write (lo);
                    y = c.low.read (std::max (2.0f, lowDelay)) + (1.0f - m) * hi + m * w;
                }
                else y = (1.0f - m) * x + m * w;

                y *= vol;
                if (vol > 1.0f) y += (softClip (y) - y) * std::min (1.0f, (vol - 1.0f) / 3.0f);   // JFET grit when pushed
                const float dry = k == 0 ? xl : xr;
                out[k] = safetyLimit (dry + (y - dry) * act);
            }
            if (p.output == Mono) out[0] = out[1] = 0.5f * (out[0] + out[1]);
            outL[i] = out[0];
            outR[i] = out[1];
            peak = std::max (peak, std::max (std::abs (out[0]), std::abs (out[1])));
        }

        // ---- what the panel and Pebble show ----
        for (int t = 0; t < kTargets; ++t) uiEff[(size_t) t].store (eff[(size_t) t], std::memory_order_relaxed);
        uiLfo.store (lfoShown, std::memory_order_relaxed);
        uiPhase.store ((float) phase, std::memory_order_relaxed);
        uiHz.store (hz, std::memory_order_relaxed);
        uiSwing.store (p.whammy >= 0.0f ? swingCents (eff[TDepth], 0.5f) : swingCents (eff[TDepth], std::max (0.05f, baseHz)), std::memory_order_relaxed);
        uiRamping.store (anyMove && p.rampExpression < 0.0f && ramp.isMoving (p.bounce), std::memory_order_relaxed);
        uiHeld.store (anyMove && ramp.isHeld(), std::memory_order_relaxed);
        uiBrake.store (brakeAmt, std::memory_order_relaxed);
        uiPaused.store (pause, std::memory_order_relaxed);
        uiFlurry.store (flurry, std::memory_order_relaxed);
        uiOut.store (uiOut.load (std::memory_order_relaxed) * 0.8f + 0.2f * peak, std::memory_order_relaxed);
    }

    std::array<std::atomic<float>, kTargets> uiEff {};
    std::atomic<float> uiLfo { 0.0f }, uiPhase { 0.0f }, uiHz { 1.0f }, uiSwing { 0.0f }, uiBrake { 0.0f }, uiOut { 0.0f };
    std::atomic<bool> uiRamping { false }, uiHeld { false }, uiPaused { false }, uiFlurry { false };

private:
    struct Channel
    {
        DelayLine line, low;
        Crossover xover;
        Biquad bbd1, bbd2, toneLp, toneHp, shelf;
        DcBlock dc;
        Smoother lfoSm;
        Rng noise { 0x2545f491u };
        float env = 0.0f;
    };

    double sr = 48000.0;
    std::array<Channel, 2> ch;
    chase::Ramp ramp;
    std::array<float, kTargets> eff {};
    Wander rateDrift, delayDrift;
    Smoother lagSm, ampSm, mixSm, volSm, activeSm, widthSm, lowDelaySm, brakeMixSm;
    double phase = 0.0;
    float brakeK = 1.0f, brakeDelay = 0.0f, rateMul = 1.0f;
    bool first = true, wasActive = true;
};
} // namespace lathe
