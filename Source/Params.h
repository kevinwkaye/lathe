#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/LatheEngine.h"

// Every parameter lives in this one list, the pedal's controls, its dip switches, the
// footswitch jobs and Keeper included, so a host, an automation lane or a remote controller
// can read and set everything the panel and the footswitches can.
namespace ParamIDs
{
// the knobs
inline constexpr const char* tone   = "tone";
inline constexpr const char* lag    = "lag";
inline constexpr const char* mix    = "mix";
inline constexpr const char* volume = "volume";
inline constexpr const char* rate   = "rate";
inline constexpr const char* depth  = "depth";
inline constexpr const char* warp   = "warp";
inline constexpr const char* ramp   = "ramp";
// toggles
inline constexpr const char* wave1    = "wave1";      // Sine, Triangle, Square
inline constexpr const char* wave2    = "wave2";      // Square, Triangle, Sine (the pedal's order)
inline constexpr const char* division = "division";   // 1 2 4 (or 3 6 8)
inline constexpr const char* div368   = "div368";
inline constexpr const char* preset   = "preset";     // A, Live, B
inline constexpr const char* bypass   = "bypass";
inline constexpr const char* voice    = "voice";      // HiFi, mkII
// the ramp bank (dip switches), in the panel's order
inline constexpr const char* move[lathe::kTargets] = { "moveVolume", "moveLag", "moveMix", "moveRate", "moveDepth", "moveWarp", "moveTone" };
inline constexpr const char* rise[lathe::kTargets] = { "riseVolume", "riseLag", "riseMix", "riseRate", "riseDepth", "riseWarp", "riseTone" };
inline constexpr const char* bounce     = "bounce";
inline constexpr const char* sweepTop   = "sweepTop";
inline constexpr const char* lofi       = "lofi";
inline constexpr const char* motoByp    = "motoByp";
inline constexpr const char* tapControl = "tapControl";   // Rate, Ramp
inline constexpr const char* holdMode   = "holdMode";     // R, -, +
// tempo
inline constexpr const char* tempo     = "tempo";
inline constexpr const char* sync      = "sync";        // the wave follows the tempo and division
inline constexpr const char* hostTempo = "hostTempo";   // use Ableton's tempo instead of the tapped one
// rig
inline constexpr const char* bass   = "bass";
inline constexpr const char* bassHz = "bassHz";
inline constexpr const char* output = "output";   // Mono, MISO, Stereo
inline constexpr const char* width  = "width";
inline constexpr const char* noise  = "noise";
// expression
inline constexpr const char* expTarget = "expTarget";
inline constexpr const char* expMin    = "expMin";
inline constexpr const char* expMax    = "expMax";
inline constexpr const char* expSmooth = "expSmooth";
// footswitch moves, and actions for a remote controller (1 to press, 0 to release)
inline constexpr const char* brake    = "brake";
inline constexpr const char* hold     = "hold";
inline constexpr const char* fireRamp = "fireRamp";
inline constexpr const char* foot[4]  = { "footA", "footB", "footC", "footD" };
inline constexpr const char* keeper   = "keeper";
} // namespace ParamIDs

namespace names
{
struct ExpTarget { const char* id; const char* label; };
// Morph, Ramp targets and Whammy are whole-pedal moves; the rest sweep one knob.
inline const std::vector<ExpTarget>& expTargets()
{
    static const std::vector<ExpTarget> t {
        { "", "Morph A to B" }, { "", "Ramp targets" }, { "", "Whammy" },
        { ParamIDs::depth, "Depth" }, { ParamIDs::rate, "Rate" }, { ParamIDs::mix, "Mix" }, { ParamIDs::lag, "Lag" },
        { ParamIDs::warp, "Warp" }, { ParamIDs::tone, "Tone" }, { ParamIDs::volume, "Volume" } };
    return t;
}
enum { ExpMorph = 0, ExpRampTargets = 1, ExpWhammy = 2, ExpDepth = 3 };

// wave2's choices are listed as on the pedal (square, triangle, sine); the engine wants shapes
inline int wave2Shape (int choice) { return 2 - juce::jlimit (0, 2, choice); }

inline juce::String rateText (float hz) { return juce::String (hz, hz < 1.0f ? 2 : 1) + " Hz"; }
inline juce::String volumeText (float v)
{
    const float db = lathe::volumeDb (v);
    if (db < -100.0f) return "Off";
    if (std::abs (db) < 0.25f) return "Unity";
    return (db > 0 ? "+" : "") + juce::String (db, 1) + " dB";
}
inline juce::String toneText (float t, int voice)
{
    if (voice == lathe::MkII) return t > 0.8f ? "Shimmery" : (t < 0.3f ? "Murky" : "Warm");
    if (std::abs (t - 0.5f) < 0.03f) return "Flat";
    return t < 0.5f ? "Dark " + juce::String (juce::roundToInt ((0.5f - t) * 20.0f)) : "Thin " + juce::String (juce::roundToInt ((t - 0.5f) * 20.0f));
}
inline juce::String warpText (float w)
{
    if (std::abs (w - 0.5f) < 0.03f) return "Even";
    return w < 0.5f ? "Fast rise" : "Slow rise";
}
inline juce::String mixText (float m)
{
    if (m > 0.98f) return "Vibrato";
    if (m < 0.02f) return "Dry";
    return juce::String (juce::roundToInt (m * 100.0f)) + "% wet";
}
inline juce::String rampText (float k)
{
    const float s = lathe::rampSecondsFromKnob (k);
    return juce::String (s, s < 10.0f ? 1 : 0) + " s";
}
} // namespace names

// The settings a preset slot holds: the whole pedal, not the rig (Bass, output, noise stay put).
inline const std::vector<juce::String>& presetParamIds()
{
    static const std::vector<juce::String> ids = [] {
        std::vector<juce::String> v { ParamIDs::tone, ParamIDs::lag, ParamIDs::mix, ParamIDs::volume, ParamIDs::rate, ParamIDs::depth,
                                      ParamIDs::warp, ParamIDs::ramp, ParamIDs::wave1, ParamIDs::wave2, ParamIDs::division, ParamIDs::div368,
                                      ParamIDs::voice, ParamIDs::bounce, ParamIDs::sweepTop, ParamIDs::lofi, ParamIDs::motoByp,
                                      ParamIDs::tapControl, ParamIDs::holdMode, ParamIDs::sync, ParamIDs::tempo };
        for (int t = 0; t < lathe::kTargets; ++t) { v.push_back (ParamIDs::move[t]); v.push_back (ParamIDs::rise[t]); }
        return v;
    }();
    return ids;
}
// which of those glide (and morph smoothly); the rest switch half way
inline bool isContinuous (const juce::String& id)
{
    for (auto* c : { ParamIDs::tone, ParamIDs::lag, ParamIDs::mix, ParamIDs::volume, ParamIDs::rate, ParamIDs::depth, ParamIDs::warp, ParamIDs::ramp, ParamIDs::tempo })
        if (id == c) return true;
    return false;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&layout] (auto p) { layout.add (std::move (p)); };
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto knob = [&] (const char* id, const char* name, float def, std::function<String (float, int)> text)
    {
        add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                                    AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text))));
    };
    auto toggle = [&] (const char* id, const String& name, bool def) { add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto choice = [&] (const char* id, const char* name, StringArray items, int def) { add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    // the knobs (defaults: a lush chorus)
    knob (ParamIDs::tone, "Tone", 0.5f, [] (float v, int) { return names::toneText (v, lathe::HiFi); });
    knob (ParamIDs::lag, "Lag", 0.62f, [] (float v, int) { return String (lathe::lagMs (v, false), 1) + " ms"; });
    knob (ParamIDs::mix, "Mix", 0.5f, [] (float v, int) { return names::mixText (v); });
    knob (ParamIDs::volume, "Volume", 0.7f, [] (float v, int) { return names::volumeText (v); });
    knob (ParamIDs::rate, "Rate", 0.45f, [] (float v, int) { return names::rateText (lathe::rateHzFromKnob (v)); });
    knob (ParamIDs::depth, "Depth", 0.35f, pct);
    knob (ParamIDs::warp, "Warp", 0.5f, [] (float v, int) { return names::warpText (v); });
    knob (ParamIDs::ramp, "Ramp", 0.4f, [] (float v, int) { return names::rampText (v); });

    choice (ParamIDs::wave1, "First Half", { "Sine", "Triangle", "Square" }, 1);
    choice (ParamIDs::wave2, "Second Half", { "Square", "Triangle", "Sine" }, 1);
    choice (ParamIDs::division, "Division", { "1", "2", "4" }, 2);
    toggle (ParamIDs::div368, "3 6 8 Divisions", false);
    choice (ParamIDs::preset, "Preset", { "A", "Live", "B" }, 1);
    toggle (ParamIDs::bypass, "Bypass", false);
    choice (ParamIDs::voice, "Voice", { "HiFi", "mkII" }, 0);

    const char* targetNames[lathe::kTargets] = { "Volume", "Lag", "Mix", "Rate", "Depth", "Warp", "Tone" };
    for (int t = 0; t < lathe::kTargets; ++t)
    {
        toggle (ParamIDs::move[t], String ("Ramp Moves ") + targetNames[t], false);
        toggle (ParamIDs::rise[t], String ("Ramp Rises ") + targetNames[t], t != lathe::TWarp);
    }
    toggle (ParamIDs::bounce, "Bounce", false);
    toggle (ParamIDs::sweepTop, "Sweep Top", false);
    toggle (ParamIDs::lofi, "Lo-Fi", false);
    toggle (ParamIDs::motoByp, "Momentary Bypass", false);
    choice (ParamIDs::tapControl, "Tap Sets", { "Rate", "Ramp" }, 0);
    choice (ParamIDs::holdMode, "Hold C", { "Ramp", "Pause", "Flurry" }, 0);

    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tempo, 1 }, "Tempo", NormalisableRange<float> (30.0f, 300.0f, 0.1f), 92.0f,
         AudioParameterFloatAttributes().withLabel ("bpm").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " bpm"; })));
    toggle (ParamIDs::sync, "Sync To Tempo", false);
    toggle (ParamIDs::hostTempo, "Use Host Tempo", true);

    toggle (ParamIDs::bass, "Bass: Keep Lows Dry", false);
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::bassHz, 1 }, "Bass Below", NormalisableRange<float> (60.0f, 400.0f, 1.0f, 0.5f), 150.0f,
         AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));
    choice (ParamIDs::output, "Output", { "Mono", "MISO", "Stereo" }, 1);
    knob (ParamIDs::width, "Width", 0.7f, pct);
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::noise, 1 }, "Noise", NormalisableRange<float> (0.0f, 2.0f), 1.0f,
         AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
         { return v < 0.01f ? String ("Silent") : (std::abs (v - 1.0f) < 0.02f ? String ("As the pedal") : String (v, 2) + "x"); })));

    StringArray targets;
    for (const auto& t : names::expTargets()) targets.add (t.label);
    choice (ParamIDs::expTarget, "Expression Target", targets, names::ExpMorph);
    knob (ParamIDs::expMin, "Expression Min", 0.0f, pct);
    knob (ParamIDs::expMax, "Expression Max", 1.0f, pct);
    knob (ParamIDs::expSmooth, "Expression Smoothing", 0.25f, pct);

    toggle (ParamIDs::brake, "Brake", false);
    toggle (ParamIDs::hold, "Hold", false);
    toggle (ParamIDs::fireRamp, "Fire Ramp", false);
    const char* footNames[4] = { "Footswitch A", "Footswitch B", "Footswitch C", "Footswitch D" };
    for (int i = 0; i < 4; ++i) toggle (ParamIDs::foot[i], footNames[i], false);
    toggle (ParamIDs::keeper, "Keeper", false);
    return layout;
}
