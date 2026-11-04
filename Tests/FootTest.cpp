// Footswitch and expression test: the real Lathe processor, driven only by plain MIDI
// press (127) and release (0) messages on the shipped defaults, exactly as a footswitch
// sends them. Checks tap, hold, A+B and C+D, the 1 second Keeper hold, presets and the
// glide, tap tempo, expression morphing between presets, the original pedal's MIDI map on
// its own channel, the remote-control parameters, and that nothing else on the MIDI cable
// (sustain pedal, mod wheel, the wrong channel) changes anything.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace juce;

namespace
{
int failures = 0;

void expect (bool ok, const char* what)
{
    std::printf ("  %-64s %s\n", what, ok ? "ok" : "FAILED");
    if (! ok) ++failures;
}

struct Rig
{
    LatheProcessor proc;
    AudioBuffer<float> buffer { 2, 480 };   // 10 ms of audio per tick, so audio time keeps up with the clock
    double now = 100.0;

    Rig()
    {
        // the shipped defaults, whatever is saved on this computer
        auto& s = proc.pedals();
        s.resetFootswitches();
        s.expChannel = 16; s.expCC = 11;
        s.keeperChannel = 0; s.keeperCC = -1;
        s.heel = 0; s.toe = 127; s.curve = 0;
        s.pedalMapChannel = 0;
        proc.prepareToPlay (48000.0, 480);
        proc.apvts.getParameter (ParamIDs::expSmooth)->setValueNotifyingHost (0.0f);
        settle (0.05);
    }

    void send (const MidiMessage& m)
    {
        MidiBuffer midi;
        midi.addEvent (m, 0);
        buffer.clear();
        proc.processBlock (buffer, midi);
        proc.pump (now);
    }
    void cc (int number, int value, int channel = 16) { send (MidiMessage::controllerEvent (channel, number, value)); }

    // let time pass, running audio and the message-thread work every 10 ms as the timer does
    void settle (double seconds)
    {
        const int steps = (int) std::lround (seconds / 0.01);
        for (int k = 0; k < steps; ++k)
        {
            now += 0.01;
            MidiBuffer none;
            buffer.clear();
            proc.processBlock (buffer, none);
            proc.pump (now);
        }
    }

    void press (int sw) { cc (20 + sw, 127); }
    void release (int sw) { cc (20 + sw, 0); }
    void tap (int sw) { press (sw); settle (0.1); release (sw); settle (0.05); }
    void hold (int sw, double seconds) { press (sw); settle (seconds); }

    bool on (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    float value (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); }
    void set (const char* id, float plain)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    std::map<String, float> snapshot()
    {
        std::map<String, float> m;
        for (auto* p : proc.getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (p)) m[rp->getParameterID()] = rp->getValue();
        return m;
    }
};
} // namespace

// --snapshot <folder>: render the panel in a few states as PNGs (needs a display, e.g. xvfb-run)
static int snapshots (const File& dir)
{
    dir.createDirectory();
    Rig r;
    std::unique_ptr<AudioProcessorEditor> ed (r.proc.createEditor());
    auto* editor = dynamic_cast<LatheEditor*> (ed.get());
    ed->setVisible (true);
    double phase = 0.0;
    auto play = [&] (double seconds)
    {
        for (double t = 0.0; t < seconds; t += 480.0 / 48000.0)
        {
            for (int i = 0; i < 480; ++i)
            {
                const float v = (float) (0.3 * std::sin (phase) * std::exp (-std::fmod (phase / 1382.0, 1.0) * 3.0));
                phase += 2.0 * MathConstants<double>::pi * 220.0 / 48000.0;
                r.buffer.setSample (0, i, v);
                r.buffer.setSample (1, i, v);
            }
            MidiBuffer none;
            r.proc.processBlock (r.buffer, none);
            r.now += 480.0 / 48000.0;
            r.proc.pump (r.now);
        }
        MessageManager::getInstance()->runDispatchLoopUntil (400);
    };
    auto shot = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File f = dir.getChildFile (String (name) + ".png");
        f.deleteFile();
        FileOutputStream out (f);
        PNGImageFormat().writeImageToStream (img, out);
        std::printf ("  wrote %s\n", f.getFullPathName().toRawUTF8());
    };
    play (1.0); shot ("1_chorus");
    r.set (ParamIDs::move[lathe::TDepth], 1.0f); r.set (ParamIDs::move[lathe::TMix], 1.0f); r.set (ParamIDs::ramp, 0.5f);
    r.proc.fireRamp(); play (0.8); shot ("2_ramping");
    r.set (ParamIDs::preset, 2.0f); play (1.0); shot ("3_preset_b_warped");
    r.set (ParamIDs::brake, 1.0f); play (1.5); shot ("4_brake");
    r.set (ParamIDs::brake, 0.0f); r.set (ParamIDs::bypass, 1.0f); play (1.0); shot ("5_bypassed");
    r.set (ParamIDs::bypass, 0.0f); r.set (ParamIDs::voice, 1.0f); r.set (ParamIDs::bass, 1.0f); play (0.6); shot ("6_mk2_bass");
    editor->setPage (1); play (0.5); shot ("7_pedal_page");
    return 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;
    if (argc > 2 && String (argv[1]) == "--snapshot") return snapshots (File (argv[2]));

    std::printf ("Footswitch A: bypass\n");
    {
        Rig r;
        r.tap (0);
        expect (r.on (ParamIDs::bypass), "tap A bypasses");
        r.tap (0);
        expect (! r.on (ParamIDs::bypass), "tap A again switches back on");
        r.hold (0, 1.0);
        expect (r.on (ParamIDs::bypass), "hold A: bypassed while held");
        r.release (0); r.settle (0.05);
        expect (! r.on (ParamIDs::bypass), "release A: back on (momentary)");

        r.set (ParamIDs::motoByp, 1.0f); r.set (ParamIDs::bypass, 1.0f); r.settle (0.05);
        r.press (0); r.settle (0.05);
        expect (! r.on (ParamIDs::bypass), "MoToByp: on while A is down");
        r.release (0); r.settle (0.05);
        expect (r.on (ParamIDs::bypass), "MoToByp: off again on release");
    }

    std::printf ("Footswitch B: tap tempo and brake\n");
    {
        Rig r;
        r.tap (1); r.settle (0.35); r.tap (1);   // presses 0.5 s apart
        std::printf ("    (tempo after two taps: %.1f)\n", r.value (ParamIDs::tempo));
        expect (std::abs (r.value (ParamIDs::tempo) - 120.0f) < 1.5f, "two taps 0.5 s apart set 120 bpm");
        expect (r.on (ParamIDs::sync), "tapping syncs the wave to the tempo");
        expect (! r.on (ParamIDs::hostTempo), "a tapped tempo wins over Ableton's");
        r.hold (1, 0.3);
        expect (! r.on (ParamIDs::brake), "no brake before the hold time");
        r.settle (0.3);
        expect (r.on (ParamIDs::brake), "hold B brakes");
        r.settle (1.5);
        expect (r.proc.getEngine().uiBrake.load() > 0.95f, "the record stops");
        r.release (1); r.settle (0.05);
        expect (! r.on (ParamIDs::brake), "release B spins back up");
        expect (std::abs (r.value (ParamIDs::tempo) - 120.0f) < 1.5f, "a hold doesn't count as a tap");
    }

    std::printf ("Footswitch C: ramp\n");
    {
        Rig r;
        r.set (ParamIDs::move[lathe::TDepth], 1.0f); r.set (ParamIDs::ramp, 0.5f); r.settle (6.0);
        expect (! r.proc.getEngine().uiRamping.load(), "resting before the tap");
        r.tap (2);
        r.settle (0.05);
        expect (r.proc.getEngine().uiRamping.load(), "tap C fires the ramp");
        r.settle (6.0);
        r.hold (2, 0.6);
        expect (r.on (ParamIDs::hold), "hold C holds");
        r.settle (2.0);
        expect (r.proc.getEngine().uiEff[lathe::TDepth].load() < 0.05f, "hold C (R): Depth travels to the sweep end");
        r.release (2); r.settle (1.0);
        expect (! r.on (ParamIDs::hold), "release C lets go");
        expect (std::abs (r.proc.getEngine().uiEff[lathe::TDepth].load() - r.value (ParamIDs::depth)) < 0.01f, "Depth glides back to the knob");
        r.set (ParamIDs::holdMode, (float) lathe::HoldPause);
        r.hold (2, 0.8);
        expect (r.proc.getEngine().uiPaused.load(), "hold C (minus) pauses the wave");
        r.release (2); r.settle (0.05);
        r.set (ParamIDs::holdMode, (float) lathe::HoldFlurry);
        r.hold (2, 0.8);
        expect (r.proc.getEngine().uiFlurry.load(), "hold C (plus) is a flurry");
        r.release (2); r.settle (0.05);
    }

    std::printf ("Footswitch D: presets\n");
    {
        Rig r;
        r.tap (3); r.settle (0.5);
        expect (roundToInt (r.value (ParamIDs::preset)) == 0, "tap D from Live goes to A");
        expect (std::abs (r.value (ParamIDs::lag) - r.proc.slotA[ParamIDs::lag]) < 0.001f, "A's settings glide in");
        r.tap (3); r.settle (0.5);
        expect (roundToInt (r.value (ParamIDs::preset)) == 2, "tap D again goes to B");
        expect (std::abs (r.value (ParamIDs::mix) - r.proc.slotB[ParamIDs::mix]) < 0.001f, "B's settings glide in");
        r.set (ParamIDs::depth, 0.9f); r.settle (0.05);
        expect (r.proc.slotEdited(), "moving a knob marks B as edited");
        r.hold (3, 0.8); r.release (3); r.settle (0.05);
        expect (std::abs (r.proc.slotB[ParamIDs::depth] - 0.9f) < 0.001f, "hold D saves into B");
        expect (! r.proc.slotEdited(), "and B is no longer edited");
        // the glide really glides
        r.press (3); r.settle (0.1); r.release (3); r.settle (0.15);
        const float mid = r.value (ParamIDs::mix);
        r.settle (0.5);
        expect (mid < 0.99f && mid > r.proc.slotA[ParamIDs::mix] + 0.001f, "switching presets glides rather than jumps");
    }

    std::printf ("Pairs\n");
    {
        Rig r;
        r.press (0); r.settle (0.03); r.press (1); r.settle (0.1); r.release (0); r.release (1); r.settle (0.1);
        expect (! r.on (ParamIDs::bypass), "A+B leaves bypass as it was");
        expect (r.proc.uiMessage == "RAMP RESET", "A+B resets the ramp");
        const float bpm = r.value (ParamIDs::tempo);
        expect (std::abs (bpm - 92.0f) < 0.01f, "A+B doesn't tap the tempo");

        const int keepsBefore = r.proc.uiKeepPulse.load();
        r.press (3); r.settle (0.03); r.press (2); r.settle (0.1); r.release (2); r.release (3); r.settle (0.1);
        expect (roundToInt (r.value (ParamIDs::preset)) == 1, "C+D tap (D first) leaves the preset alone");
        expect (r.proc.uiKeepPulse.load() == keepsBefore, "C+D tap doesn't keep");

        r.press (2); r.settle (0.03); r.press (3); r.settle (1.2);
        expect (r.proc.uiKeepPulse.load() == keepsBefore + 1, "C+D held 1 second keeps");
        expect (! r.on (ParamIDs::hold), "C+D hold doesn't start C's hold");
        r.release (2); r.release (3); r.settle (0.1);
        expect (roundToInt (r.value (ParamIDs::preset)) == 1, "C+D hold doesn't save or switch presets");
        expect (r.proc.uiKeepPulse.load() == keepsBefore + 1, "only one Keeper per hold");
    }

    std::printf ("Expression: morph between presets A and B\n");
    {
        Rig r;
        r.cc (11, 0); r.settle (0.05);
        expect (std::abs (r.value (ParamIDs::lag) - r.proc.slotA[ParamIDs::lag]) < 0.002f, "heel is preset A");
        expect (std::abs (r.value (ParamIDs::mix) - r.proc.slotA[ParamIDs::mix]) < 0.002f, "heel: A's mix");
        r.cc (11, 127); r.settle (0.05);
        expect (std::abs (r.value (ParamIDs::lag) - r.proc.slotB[ParamIDs::lag]) < 0.002f, "toe is preset B");
        expect (roundToInt (r.value (ParamIDs::wave1)) == roundToInt (r.proc.slotB[ParamIDs::wave1]), "toe: B's wave shape");
        r.cc (11, 64); r.settle (0.05);
        const float half = (r.proc.slotA[ParamIDs::depth] + r.proc.slotB[ParamIDs::depth]) * 0.5f;
        expect (std::abs (r.value (ParamIDs::depth) - half) < 0.01f, "half way is half way between them");
    }

    std::printf ("Expression: one control, ramp targets and whammy\n");
    {
        Rig r;
        r.set (ParamIDs::expTarget, (float) names::ExpDepth);
        r.cc (11, 127); r.settle (0.05);
        expect (r.value (ParamIDs::depth) > 0.99f, "on Depth, toe is full depth");
        expect (std::abs (r.value (ParamIDs::lag) - 0.62f) < 0.001f, "and nothing else moves");
        r.set (ParamIDs::expTarget, (float) names::ExpRampTargets);
        r.set (ParamIDs::move[lathe::TMix], 1.0f); r.set (ParamIDs::sweepTop, 1.0f);
        r.cc (11, 127); r.settle (0.1);
        expect (r.proc.getEngine().uiEff[lathe::TMix].load() > 0.99f, "on ramp targets, toe sweeps Mix to the top");
        r.cc (11, 0); r.settle (0.1);
        expect (std::abs (r.proc.getEngine().uiEff[lathe::TMix].load() - r.value (ParamIDs::mix)) < 0.01f, "heel puts it on the knob");
        r.set (ParamIDs::expTarget, (float) names::ExpWhammy);
        r.cc (11, 0); r.settle (0.1);
        expect (r.proc.getEngine().uiLfo.load() < -0.9f, "whammy: heel bends the wave down");
    }

    std::printf ("Original pedal MIDI map\n");
    {
        Rig r;
        const auto before = r.snapshot();
        r.cc (16, 127, 5); r.cc (102, 0, 5); r.settle (0.1);
        expect (r.snapshot() == before, "off by default: the chart does nothing");
        r.proc.setPedalMapChannel (5);
        r.cc (16, 127, 5); r.settle (0.05);
        expect (r.value (ParamIDs::mix) > 0.99f, "on channel 5: CC 16 sets Mix");
        r.cc (102, 0, 5); r.settle (0.05);
        expect (r.on (ParamIDs::bypass), "CC 102 at 0 bypasses");
        r.cc (102, 127, 5); r.settle (0.05);
        expect (! r.on (ParamIDs::bypass), "CC 102 at 127 engages");
        r.send (MidiMessage::programChange (5, 2)); r.settle (0.5);
        expect (roundToInt (r.value (ParamIDs::preset)) == 0, "program change 2 recalls the left slot (A)");
        r.press (0); r.settle (0.05); r.release (0); r.settle (0.05);
        expect (r.on (ParamIDs::bypass), "footswitches still work with the map on");
        r.proc.setPedalMapChannel (0);
    }

    std::printf ("Remote control through the parameter list\n");
    {
        Rig r;
        r.set (ParamIDs::foot[0], 1.0f); r.settle (0.05); r.set (ParamIDs::foot[0], 0.0f); r.settle (0.05);
        expect (r.on (ParamIDs::bypass), "footA parameter taps A");
        r.set (ParamIDs::foot[1], 1.0f); r.settle (0.8);
        expect (r.on (ParamIDs::brake), "footB parameter held brakes");
        r.set (ParamIDs::foot[1], 0.0f); r.settle (0.05);
        const int keeps = r.proc.uiKeepPulse.load();
        r.set (ParamIDs::keeper, 1.0f); r.settle (0.05);
        expect (r.proc.uiKeepPulse.load() == keeps + 1 && ! r.on (ParamIDs::keeper), "keeper parameter keeps once and resets");
        r.set (ParamIDs::fireRamp, 1.0f); r.settle (0.05);
        expect (! r.on (ParamIDs::fireRamp) && r.proc.uiMessage == "RAMP", "fireRamp parameter fires once and resets");
    }

    std::printf ("No clashes\n");
    {
        Rig r;
        const auto before = r.snapshot();
        r.cc (64, 127); r.settle (0.6); r.cc (64, 0);
        r.cc (1, 100);
        for (int ch : { 1, 2, 15 })
        {
            for (int n : { 20, 21, 22, 23 }) { r.cc (n, 127, ch); r.settle (0.6); r.cc (n, 0, ch); }
            r.cc (11, 0, ch);
            for (int n : { 14, 15, 16, 17, 18, 19, 93, 100, 102 }) r.cc (n, 0, ch);
        }
        r.settle (0.2);
        expect (r.snapshot() == before, "sustain, mod wheel, the old chart and other channels change nothing");
    }

    std::printf ("\n%s: %d failed\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
