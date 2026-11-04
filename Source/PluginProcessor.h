#pragma once

#include "Params.h"
#include "PedalControl.h"
#include "Keeper.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

class LatheProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    LatheProcessor();
    ~LatheProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lathe"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ParamIDs::bypass); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    lathe::LatheEngine& getEngine() { return engine; }

    // ---- the four footswitches, from the panel's buttons (message thread) ----
    void footDown (int sw);
    void footUp (int sw);
    void focusHere();   // the panel was clicked: footswitches drive this copy

    // ---- the pedal's own moves ----
    void tapTempo (double now);
    void tapTempo() { tapTempo (juce::Time::getMillisecondCounterHiRes() * 0.001); }
    void fireRamp() { pendingFire.store (true); say ("RAMP"); }     // run on the audio thread
    void resetRamp() { pendingReset.store (true); say ("RAMP RESET"); }
    void selectSlot (int slot);          // 0 A, 1 Live, 2 B: glides to that preset
    void saveSlot (int slot);            // store the panel in A (0) or B (2)
    bool slotEdited() const;             // the panel no longer matches the recalled slot
    float bpmNow() const { return uiBpm.load(); }

    // footswitch and expression setup
    PedalSettings& pedals() { return settings; }
    void learnFootswitch (int sw);
    void learnExpression();
    void calibrateExpression();
    void resetFootswitches();
    void setCurve (int c);
    void setPedalMapChannel (int ch);

    // Keeper: log this moment, the last 8 seconds of audio included
    void keepNow();
    void addKeeperNote (int number, const juce::String& note) { keeper.addNote (number, note); }
    juce::File exportKeepers();
    void openKeepersFolder();
    void learnKeeper();
    bool keeperLive() const { return settings.keeperLive; }
    void setKeeperLive (bool live);
    int lastKeeper() const { return keeper.lastNumber(); }

    // footswitch MIDI read straight from the device, so every copy works without a MIDI track
    void handleDirectMidi (const juce::MidiMessage&);   // MIDI thread
    bool pedalsDirect() const { return direct->connected(); }
    juce::String footInputStatus() const { return direct->status(); }
    juce::String footDevice() const { return settings.footDevice; }
    void setFootDevice (const juce::String& device);
    std::atomic<bool> hostFeedsPedals { false };

    bool isLearning() const { return learnTarget != LearnNone; }
    int learningWhat() const { return learnTarget; }
    bool isCalibrating() const { return calibrating; }
    enum { LearnNone = -1, LearnExpression = 4, LearnKeeper = 5 };

    // ---- what the panel and Pebble show ----
    float expressionLevel() const { return uiExp.load(); }
    double lastExpressionMove() const { return expMovedAt; }
    std::atomic<int> uiFootPulse { 0 };       // counts footswitch presses: Pebble taps its foot
    std::atomic<int> uiKeepPulse { 0 };       // counts Keepers: Pebble nods
    std::atomic<int> uiFootDown { 0 };        // bitmask of footswitches held now
    std::atomic<int> uiTapCount { 0 };
    juce::String uiMessage;                   // a short line for Pebble's status
    double uiMessageAt = 0.0;

    // Runs everything the timer does. Public so the footswitch test can drive it without a message loop.
    void pump (double now);

    // the two preset slots, A and B (plain parameter values)
    std::map<juce::String, float> slotA, slotB;

private:
    void timerCallback() override;
    lathe::EngineParams readParams();
    void handleMidi (const juce::MidiMessage&, bool fromDevice);
    bool handlePedalMap (const juce::MidiMessage&);
    void onFootEvent (int sw, FootswitchLogic::Event);
    double lastNow = 0.0;
    void setNorm (const juce::String& id, float v01);
    void setPlain (const juce::String& id, float plain);
    bool flag (const char* id) const { return raw.at (id)->load() > 0.5f; }
    void toggle (const char* id) { setPlain (id, flag (id) ? 0.0f : 1.0f); }
    void say (const juce::String& s);
    void applyExpression (float v);
    std::map<juce::String, float> capture() const;
    void startGlide (const std::map<juce::String, float>& to);
    static std::map<juce::String, float> defaultSlot (bool b);

    lathe::LatheEngine engine;
    Keeper keeper;

    std::map<juce::String, std::atomic<float>*> raw;
    float f (const char* id) const { return raw.at (id)->load (std::memory_order_relaxed); }
    int idx (const char* id) const { return (int) std::lround (f (id)); }

    // MIDI handed to the message thread
    struct Pending { int kind = 0; int a = 0; };
    enum PendingKind { FootPress, FootRelease, Expression, Learned, KeepCC, MapCC, MapProgram };
    juce::AbstractFifo fifo { 512 };
    std::array<Pending, 512> pending;
    juce::AbstractFifo directFifo { 512 };    // the same, written from the MIDI thread
    std::array<Pending, 512> directPending;
    juce::SpinLock directWriteLock;
    juce::SharedResourcePointer<DirectMidiInput> direct;
    juce::Time settingsStamp;
    bool wasDirect = false;
    double nextSettingsCheck = 0.0;
    void queue (int kind, int a);

    PedalSettings settings;
    FootswitchLogic footswitches;
    std::atomic<int> learnTarget { LearnNone };
    bool calibrating = false;
    double calibrateUntil = 0.0;
    int calLo = 127, calHi = 0, calFirst = -1;
    double expMovedAt = 0.0;
    float expTargetValue = -1.0f, expSmoothed = -1.0f;
    std::array<bool, 4> remoteFoot {};        // last seen state of the footA..footD parameters

    // what a footswitch's press did, so a combo can undo it
    bool aDownToggled = false;
    double lastTapDown = -10.0, bDownAt = -10.0;
    double tapInterval = 0.0;                 // seconds between the last two taps, 0 until tapped

    // preset glide
    std::map<juce::String, float> glideFrom, glideTo;
    double glideStart = -1.0;
    int lastSlotParam = 1;
    bool suppressSlotGlide = false;

    // expression straight into the engine
    std::atomic<float> engineRampExp { -1.0f }, engineWhammy { -1.0f };
    std::atomic<float> hostBpm { 0.0f }, uiBpm { 92.0f };
    std::atomic<bool> pendingFire { false }, pendingReset { false };

    std::atomic<float> uiExp { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatheProcessor)
};
