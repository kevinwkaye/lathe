// Offline engine test: no plugin host, no JUCE. Plays a phrase through every scenario,
// checks for NaNs, levels above full scale and dead output, checks the ramps, brake,
// pause, bass handling and stereo, then measures CPU per instance at 48 kHz with 64 and
// 128 sample buffers (a house rule for every release).
//
// Pass a folder to also write a WAV of each scenario.

#include "LatheEngine.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace lathe;

namespace
{
struct Result
{
    std::vector<float> l, r, inL;
    std::vector<float> depthEff;     // the engine's Depth as it moved, per block
    std::vector<bool> ramping;
};

struct Scene
{
    std::string name;
    EngineParams p;
    double seconds = 6.0;
    std::function<float (double)> input;                       // default: a strummed chord
    std::function<void (double t, EngineParams&, LatheEngine&)> script;
    std::function<bool (const Result&, double sr, std::string& why)> check;
};

// a strummed chord with a pick-like attack, retriggered every 1.25 s
float phrase (double t)
{
    static const double freqs[4] = { 110.0, 164.81, 220.0, 277.18 };
    const double local = std::fmod (t, 1.25);
    const double env = std::exp (-local * 2.0) * std::min (1.0, local * 400.0);
    double s = 0.0;
    for (int k = 0; k < 4; ++k)
    {
        const double ph = 2.0 * 3.14159265358979 * freqs[k] * t;
        s += std::sin (ph) + 0.35 * std::sin (2.0 * ph) + 0.15 * std::sin (3.0 * ph);
    }
    return (float) (0.12 * env * s);
}

double rms (const std::vector<float>& x, double sr, double from, double to)
{
    const size_t a = (size_t) (from * sr), b = std::min (x.size(), (size_t) (to * sr));
    double s = 0.0;
    for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return b > a ? std::sqrt (s / (double) (b - a)) : 0.0;
}

double diffRms (const std::vector<float>& a, const std::vector<float>& b, double sr, double from, double to)
{
    const size_t s0 = (size_t) (from * sr), s1 = std::min (a.size(), (size_t) (to * sr));
    double s = 0.0;
    for (size_t i = s0; i < s1; ++i) { const double d = (double) a[i] - b[i]; s += d * d; }
    return s1 > s0 ? std::sqrt (s / (double) (s1 - s0)) : 0.0;
}

void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t frames = (uint32_t) l.size(), dataBytes = frames * 4;
    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
    {
        const int16_t a = (int16_t) std::lround (clampf (l[i], -1.0f, 1.0f) * 32767.0f);
        const int16_t b = (int16_t) std::lround (clampf (r[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

bool run (const Scene& s, double sr, const std::string& wavDir, std::string& why)
{
    auto eng = std::make_unique<LatheEngine>();
    eng->prepare (sr);
    const int block = 128;
    const size_t total = (size_t) (s.seconds * sr);
    Result res;
    res.l.resize (total); res.r.resize (total); res.inL.resize (total);
    std::vector<float> bl (block), br (block);
    EngineParams p = s.p;
    for (size_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min ((size_t) block, total - pos);
        const double t = (double) pos / sr;
        if (s.script) s.script (t, p, *eng);
        for (int i = 0; i < n; ++i)
        {
            const double ti = (double) (pos + (size_t) i) / sr;
            const float x = s.input ? s.input (ti) : phrase (ti);
            bl[(size_t) i] = br[(size_t) i] = x;
            res.inL[pos + (size_t) i] = x;
        }
        eng->process (bl.data(), br.data(), bl.data(), br.data(), n, p);
        for (int i = 0; i < n; ++i) { res.l[pos + (size_t) i] = bl[(size_t) i]; res.r[pos + (size_t) i] = br[(size_t) i]; }
        res.depthEff.push_back (eng->uiEff[TDepth].load());
        res.ramping.push_back (eng->uiRamping.load());
    }
    for (size_t i = 0; i < total; ++i)
    {
        if (! std::isfinite (res.l[i]) || ! std::isfinite (res.r[i])) { why = "NaN or infinity at sample " + std::to_string (i); return false; }
        if (std::abs (res.l[i]) > 1.0001f || std::abs (res.r[i]) > 1.0001f) { why = "level above full scale"; return false; }
    }
    if (! wavDir.empty() && sr == 48000.0) writeWav (wavDir + "/" + s.name + ".wav", res.l, res.r, (int) sr);
    return s.check ? s.check (res, sr, why) : true;
}

double cpuPercent (int block, bool mk2)
{
    const double sr = 48000.0;
    LatheEngine eng;
    eng.prepare (sr);
    EngineParams p;
    p.depth = 0.8f; p.mix = 0.5f; p.lag = 0.8f; p.bass = true; p.output = Miso; p.voice = mk2 ? MkII : HiFi;
    p.move[TDepth] = p.move[TRate] = true; p.bounce = true;
    const int seconds = 20;
    std::vector<float> l ((size_t) block), r ((size_t) block);
    const auto start = std::chrono::steady_clock::now();
    for (long pos = 0; pos < (long) (seconds * sr); pos += block)
    {
        for (int i = 0; i < block; ++i) l[(size_t) i] = r[(size_t) i] = phrase ((double) (pos + i) / sr);
        eng.process (l.data(), r.data(), l.data(), r.data(), block, p);
    }
    const double used = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    return 100.0 * used / (double) seconds;
}

// Goertzel level of one frequency, for the bass checks
double toneLevel (const std::vector<float>& x, double sr, double hz, double from, double to)
{
    const size_t a = (size_t) (from * sr), b = std::min (x.size(), (size_t) (to * sr));
    const double w = 2.0 * 3.14159265358979 * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    const double power = s1 * s1 + s2 * s2 - c * s1 * s2;
    return std::sqrt (std::max (0.0, power)) * 2.0 / (double) (b - a);
}
} // namespace

int main (int argc, char** argv)
{
    const std::string wavDir = argc > 1 ? argv[1] : "";
    std::vector<Scene> scenes;

    auto changes = [] (double minDiff)
    {
        return [minDiff] (const Result& r, double sr, std::string& why)
        {
            const double d = diffRms (r.l, r.inL, sr, 0.5, 4.5);
            if (d < minDiff) { why = "the effect barely changes the sound: " + std::to_string (d); return false; }
            if (rms (r.l, sr, 0.5, 4.5) < 0.01) { why = "output too quiet"; return false; }
            return true;
        };
    };

    { Scene s; s.name = "01_lush_chorus"; s.p.lag = 0.75f; s.check = changes (0.005); scenes.push_back (s); }
    { Scene s; s.name = "02_warped_record"; s.p.mix = 1.0f; s.p.depth = 0.55f; s.p.rate = 0.18f; s.p.warp = 0.8f; s.p.wave1 = chase::Sine; s.check = changes (0.01); scenes.push_back (s); }
    { Scene s; s.name = "03_deep_fast_vibrato"; s.p.mix = 1.0f; s.p.depth = 1.0f; s.p.rate = 0.85f; s.check = changes (0.01); scenes.push_back (s); }
    { Scene s; s.name = "04_slow_deep"; s.p.mix = 1.0f; s.p.depth = 1.0f; s.p.rate = 0.0f; s.check = changes (0.005); scenes.push_back (s); }
    { Scene s; s.name = "05_square_halves"; s.p.mix = 1.0f; s.p.depth = 0.6f; s.p.wave1 = chase::Square; s.p.wave2 = chase::Square; s.check = changes (0.01); scenes.push_back (s); }
    { Scene s; s.name = "06_warp_hard_left"; s.p.mix = 1.0f; s.p.warp = 0.0f; s.p.depth = 0.6f; s.check = changes (0.01); scenes.push_back (s); }
    { Scene s; s.name = "07_tone_dark"; s.p.tone = 0.0f; s.check = changes (0.005); scenes.push_back (s); }
    { Scene s; s.name = "08_tone_thin"; s.p.tone = 1.0f; s.check = changes (0.005); scenes.push_back (s); }
    { Scene s; s.name = "09_mk2_murky"; s.p.voice = MkII; s.p.tone = 0.2f; s.p.mix = 1.0f; s.check = changes (0.01); scenes.push_back (s); }
    { Scene s; s.name = "10_lofi_goo"; s.p.voice = MkII; s.p.lofi = true; s.p.mix = 0.7f; s.check = changes (0.01); scenes.push_back (s); }
    { Scene s; s.name = "11_volume_pushed"; s.p.volume = 1.0f; s.check = changes (0.01); scenes.push_back (s); }

    {
        Scene s; s.name = "12_dry_is_dry"; s.p.mix = 0.0f;
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            const double d = diffRms (r.l, r.inL, sr, 0.5, 4.5);
            if (d > 1.0e-3) { why = "Mix at zero changed the sound: " + std::to_string (d); return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "13_bypass";
        s.script = [] (double t, EngineParams& p, LatheEngine&) { p.active = t < 1.0; };
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            const double d = diffRms (r.l, r.inL, sr, 1.5, 5.5);
            if (d > 1.0e-5) { why = "bypassed output isn't the input: " + std::to_string (d); return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        // Depth ramping up from the bottom when the effect comes on, then holding at the knob
        Scene s; s.name = "14_ramp_on_engage"; s.p.mix = 1.0f; s.p.depth = 0.7f; s.p.ramp = 0.45f;   // about 1.3 s
        s.p.move[TDepth] = true; s.p.rise[TDepth] = true; s.p.sweepTop = false;
        s.check = [] (const Result& r, double, std::string& why)
        {
            const float startD = r.depthEff[2], endD = r.depthEff.back();
            if (startD > 0.1f) { why = "the ramp didn't start at the bottom: " + std::to_string (startD); return false; }
            if (std::abs (endD - 0.7f) > 0.01f) { why = "the ramp didn't end at the knob: " + std::to_string (endD); return false; }
            if (r.ramping.back()) { why = "still ramping after it finished"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        // falling with sweep top: starts at max, falls to the knob
        Scene s; s.name = "15_ramp_fall_top"; s.p.depth = 0.3f; s.p.ramp = 0.3f;
        s.p.move[TDepth] = true; s.p.rise[TDepth] = false; s.p.sweepTop = true;
        s.check = [] (const Result& r, double, std::string& why)
        {
            if (r.depthEff[2] < 0.9f || std::abs (r.depthEff.back() - 0.3f) > 0.01f) { why = "fall from the top didn't land on the knob"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "16_bounce"; s.p.depth = 0.8f; s.p.ramp = 0.2f; s.p.bounce = true;
        s.p.move[TDepth] = true; s.p.move[TMix] = true;
        s.check = [] (const Result& r, double, std::string& why)
        {
            float lo = 1, hi = 0;
            for (size_t i = r.depthEff.size() / 2; i < r.depthEff.size(); ++i) { lo = std::min (lo, r.depthEff[i]); hi = std::max (hi, r.depthEff[i]); }
            if (hi - lo < 0.6f) { why = "bounce isn't going back and forth"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        // hold C to ramp: Depth travels to the bottom while held, glides back on release
        Scene s; s.name = "17_hold_to_ramp"; s.p.depth = 0.8f; s.p.ramp = 0.3f; s.p.move[TDepth] = true;
        s.script = [] (double t, EngineParams& p, LatheEngine&) { p.holdDown = t > 2.0 && t < 3.5; };
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            const size_t at = (size_t) (3.4 * sr / 128), after = (size_t) (5.0 * sr / 128);
            if (r.depthEff[at] > 0.1f) { why = "hold didn't take Depth to the sweep end: " + std::to_string (r.depthEff[at]); return false; }
            if (std::abs (r.depthEff[after] - 0.8f) > 0.01f) { why = "Depth didn't glide back to the knob"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "18_fire_and_reset"; s.p.depth = 0.6f; s.p.ramp = 0.6f; s.p.move[TDepth] = true;
        s.script = [] (double t, EngineParams&, LatheEngine& e)
        {
            if (t >= 2.0 && t < 2.0 + 128 / 48000.0 * 1.01) e.fireRamp();
            if (t >= 2.5 && t < 2.5 + 128 / 48000.0 * 1.01) e.resetRamp();
        };
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            if (std::abs (r.depthEff[(size_t) (3.0 * sr / 128)] - 0.6f) > 0.01f) { why = "A+B reset didn't put Depth back on the knob"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "19_pause_and_flurry"; s.p.mix = 1.0f; s.p.depth = 0.6f;
        s.script = [] (double t, EngineParams& p, LatheEngine&)
        {
            p.holdDown = (t > 1.0 && t < 2.0) || (t > 3.0 && t < 4.0);
            p.holdMode = t < 2.5 ? HoldPause : HoldFlurry;
        };
        s.check = changes (0.005);
        scenes.push_back (s);
    }
    {
        // brake: the record slows to a stop and goes quiet, then spins back up
        Scene s; s.name = "20_brake"; s.p.mix = 0.5f;
        s.script = [] (double t, EngineParams& p, LatheEngine&) { p.brake = t > 1.0 && t < 3.0; };
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            const double stopped = rms (r.l, sr, 2.6, 3.0), playing = rms (r.l, sr, 0.3, 0.9), back = rms (r.l, sr, 5.2, 5.9);
            if (stopped > playing * 0.1) { why = "the record didn't stop: " + std::to_string (stopped); return false; }
            if (back < playing * 0.3) { why = "the record didn't spin back up"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "21_whammy"; s.p.mix = 1.0f; s.p.depth = 1.0f;
        s.script = [] (double t, EngineParams& p, LatheEngine&) { p.whammy = (float) (0.5 + 0.5 * std::sin (t * 2.0)); };
        s.check = changes (0.01);
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "22_expression_on_ramp_targets"; s.p.depth = 0.5f; s.p.move[TDepth] = true; s.p.sweepTop = true;
        s.script = [] (double t, EngineParams& p, LatheEngine&) { p.rampExpression = t < 3.0 ? 0.0f : 1.0f; };
        s.check = [] (const Result& r, double, std::string& why)
        {
            if (std::abs (r.depthEff[200] - 0.5f) > 0.01f || r.depthEff.back() < 0.99f) { why = "expression didn't sweep from the knob to the top"; return false; }
            return true;
        };
        scenes.push_back (s);
    }

    // ---- bass ----
    auto bassScene = [] (const char* name, double hz, bool bass)
    {
        Scene s; s.name = name; s.p.mix = 1.0f; s.p.depth = 0.6f; s.p.bass = bass; s.p.bassHz = 150.0f;
        s.input = [hz] (double t) { return (float) (0.8 * std::sin (2.0 * 3.14159265358979 * hz * t)); };   // -2 dBFS
        s.check = [hz, bass] (const Result& r, double sr, std::string& why)
        {
            // with BASS on, the fundamental itself must come through untouched; full range,
            // vibrato spreads it into sidebands, so compare the overall level instead
            const double in = bass ? toneLevel (r.inL, sr, hz, 1.0, 5.0) : rms (r.inL, sr, 1.0, 5.0);
            const double out = bass ? toneLevel (r.l, sr, hz, 1.0, 5.0) : rms (r.l, sr, 1.0, 5.0);
            const double db = 20.0 * std::log10 (out / in);
            if (bass && std::abs (db) > 0.5) { why = "lows moved by " + std::to_string (db) + " dB with BASS on"; return false; }
            if (! bass && std::abs (db) > 1.0) { why = "low end lost " + std::to_string (db) + " dB"; return false; }
            if (bass)
            {
                // the lows should be steady: no vibrato on the fundamental
                const double a = toneLevel (r.l, sr, hz, 1.0, 2.0), b = toneLevel (r.l, sr, hz, 3.0, 4.0);
                if (std::abs (20.0 * std::log10 (a / b)) > 0.2) { why = "the lows are wobbling"; return false; }
            }
            return true;
        };
        return s;
    };
    scenes.push_back (bassScene ("23_bass_41hz_lows_dry", 41.2, true));
    scenes.push_back (bassScene ("24_bass_31hz_lows_dry", 30.9, true));
    scenes.push_back (bassScene ("25_bass_41hz_full_range", 41.2, false));

    // ---- stereo ----
    {
        Scene s; s.name = "26_miso_wide"; s.p.output = Miso; s.p.width = 1.0f; s.p.mix = 0.5f; s.p.depth = 0.6f;
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            if (diffRms (r.l, r.r, sr, 0.5, 4.5) < 0.005) { why = "MISO isn't wide"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "27_mono"; s.p.output = Mono;
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            if (diffRms (r.l, r.r, sr, 0.5, 4.5) > 1.0e-6) { why = "Mono output differs left to right"; return false; }
            return true;
        };
        scenes.push_back (s);
    }
    {
        Scene s; s.name = "28_hiss_true_to_pedal"; s.p.voice = MkII; s.p.lag = 1.0f; s.p.tone = 1.0f; s.p.mix = 1.0f; s.p.lofi = true;
        s.input = [] (double) { return 0.0f; };
        s.check = [] (const Result& r, double sr, std::string& why)
        {
            const double db = 20.0 * std::log10 (rms (r.l, sr, 1.0, 5.0) + 1e-12);
            if (db < -95.0 || db > -50.0) { why = "hiss level out of range: " + std::to_string (db) + " dBFS"; return false; }
            std::printf ("    (hiss, worst case: %.1f dBFS)\n", db);
            return true;
        };
        scenes.push_back (s);
    }

    int failures = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("At %.0f Hz\n", sr);
        for (const auto& s : scenes)
        {
            std::string why;
            const bool ok = run (s, sr, wavDir, why);
            std::printf ("  %-34s %s%s\n", s.name.c_str(), ok ? "ok" : "FAILED: ", why.c_str());
            if (! ok) ++failures;
        }
    }

    std::printf ("\nCPU per instance, one core, 48 kHz (bass split, MISO, bounce on):\n");
    for (bool mk2 : { false, true })
        for (int block : { 64, 128 })
            std::printf ("  %-5s %3d samples: %.2f%%\n", mk2 ? "mkII" : "HiFi", block, cpuPercent (block, mk2));

    std::printf ("\n%s: %d failed\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
