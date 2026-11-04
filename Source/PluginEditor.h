#pragma once

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Flip Fella palette, shared across all of Kevin's builds.
namespace ff
{
const juce::Colour bg        { 0xff050505 };
const juce::Colour face      { 0xff0d0d0d };
const juce::Colour housing   { 0xff060606 };
const juce::Colour card      { 0xff1b1b1b };
const juce::Colour cardHi    { 0xff252525 };
const juce::Colour cardLow   { 0xff141414 };
const juce::Colour ink       { 0xfff3f0e8 };
const juce::Colour dim       { 0xffa29e95 };
const juce::Colour faint     { 0xff6c6962 };
const juce::Colour line      { 0xff3a3a3a };
const juce::Colour ring      { 0xff1a1a1a };
const juce::Colour pin       { 0xff3c3c3c };
const juce::Colour red       { 0xffe8392f };
const juce::Colour teal      { 0xff3fd6c8 };
const juce::Colour coral     { 0xffff7a59 };
const juce::Colour amber     { 0xfff2a33a };   // Lathe's record label
const juce::Colour amberDeep { 0xffc9752b };   // the mkII label
const juce::Colour green     { 0xff4fcf6a };
const juce::Colour blob      { 0xff4d5bff };
const juce::Colour blobShade { 0xff2329a8 };
const juce::Colour eye       { 0xffffffff };
const juce::Colour faceInk   { 0xff0b0b12 };

juce::Font font (float height, float kerning = 0.04f);
void label (juce::Graphics&, const juce::String& text, juce::Rectangle<int> r,
            juce::Justification j = juce::Justification::centredLeft, juce::Colour c = dim, float size = 13.0f);
} // namespace ff

namespace ui
{
void pill (juce::Graphics&, juce::Rectangle<float> b, const juce::String& text, bool on, float size, juce::Colour onColour = ff::ink);

// A split-flap card that flips like the Karlsson clock when its value changes.
class FlipCard : public juce::Component
{
public:
    void setValue (const juce::String& v);
    void setTextHeight (float h) { textHeight = h; }
    void advance (double dt);
    void paint (juce::Graphics&) override;

private:
    void drawHalf (juce::Graphics&, juce::Rectangle<float> card, const juce::String& text, bool top, float scale, bool shade);
    juce::String current, previous;
    float progress = 1.0f;
    float textHeight = 34.0f;
};

// Rounded pill buttons bound to a choice (or on/off) parameter.
class Pills : public juce::Component
{
public:
    Pills (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels, float textSize = 13.0f);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setLabels (const juce::StringArray& l) { if (l != labels) { labels = l; repaint(); } }
    void setDimmed (int which) { if (which != dimmed) { dimmed = which; repaint(); } }   // a recalled slot that's been edited
    std::function<void()> onPick;

private:
    juce::Rectangle<float> cell (int i) const;
    juce::StringArray labels;
    int selected = 0, dimmed = -1;
    float textSize;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A single pill that turns ink-white (or another colour) when its switch is on.
class ToggleChip : public juce::Component
{
public:
    ToggleChip (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text,
                const juce::String& onText = {}, float textSize = 13.0f, juce::Colour onColour = ff::ink);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
    float cornerRadius = -1.0f;   // < 0: a full pill

private:
    juce::String text, onText;
    float textSize;
    juce::Colour onColour;
    bool on = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A plain pill button that runs a function.
class ActionChip : public juce::Component
{
public:
    ActionChip (const juce::String& t, std::function<void()> fn, float textSize = 12.0f)
        : text (t), action (std::move (fn)), size (textSize) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics& g) override { pill (g, getLocalBounds().toFloat().reduced (1.0f), text, lit, size, colour); }
    void mouseDown (const juce::MouseEvent&) override { if (action) action(); }
    void setLit (bool l) { if (l != lit) { lit = l; repaint(); } }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void setColour (juce::Colour c) { colour = c; repaint(); }

private:
    juce::String text;
    std::function<void()> action;
    float size;
    bool lit = false;
    juce::Colour colour = ff::ink;
};

class FlipLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override {}
};

// A knob drawn as a small clock face, its name above and its value below. The hand shows
// what you hear: when a ramp or the expression pedal moves the control, the hand moves on
// its own, the name turns amber and a faint ghost hand marks where the knob is set.
class Knob : public juce::Component
{
public:
    Knob (LatheProcessor&, const juce::String& paramId, const juce::String& caption, int target, bool redHand = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    std::function<juce::String (float)> valueText;   // text for the value you hear
    juce::Slider slider;

private:
    LatheProcessor& proc;
    juce::RangedAudioParameter* param;
    juce::String caption;
    int target;   // lathe::Target, or -1 for the Ramp knob
    bool red;
    float shown = -1.0f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class LabelledSlider : public juce::Component
{
public:
    LabelledSlider (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::RangedAudioParameter* param;
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// The record: the platter turns at the wave's rate, the tonearm rides the wave, the label
// shows the voice. It slows and stops under Brake, and the disc's edge breathes with Depth.
class RecordView : public juce::Component
{
public:
    explicit RecordView (LatheProcessor& p) : proc (p) {}
    void tick (double dt);
    void paint (juce::Graphics&) override;

private:
    LatheProcessor& proc;
    float angle = 0.0f, lfo = 0.0f, depth = 0.0f;
};

// One cycle of the wave as you hear it: the two half shapes, Warp's lean and Depth's height,
// with the playhead running across it.
class WaveView : public juce::Component
{
public:
    explicit WaveView (LatheProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    LatheProcessor& proc;
};

// One footswitch in the bottom strip. Works exactly like the real one: tap, hold, pairs.
class FootButton : public juce::Component
{
public:
    FootButton (LatheProcessor& p, int s) : proc (p), sw (s) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { proc.footDown (sw); }
    void mouseUp (const juce::MouseEvent&) override { proc.footUp (sw); }
    void tick();

private:
    LatheProcessor& proc;
    int sw;
    juce::String tapText, holdText;
    juce::Colour led = ff::pin;
    bool lit = false, down = false;
};

// The big ON button: tap to bypass, like footswitch A
class OnButton : public juce::Component
{
public:
    explicit OnButton (LatheProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { proc.footDown (0); }
    void mouseUp (const juce::MouseEvent&) override { proc.footUp (0); }
    void tick();

private:
    LatheProcessor& proc;
    bool on = true;
};

// The expression pedal: live position, Morph, Ramp and Whammy chips, and a menu for the rest.
class ExpressionBar : public juce::Component
{
public:
    explicit ExpressionBar (LatheProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick();
    std::function<void()> showPedalPage;

private:
    LatheProcessor& proc;
    ActionChip morphChip, rampChip, whammyChip;
    juce::RangedAudioParameter* target;
    float level = 0.0f;
    int lastTarget = -1;
};

// "FOOTSWITCHES": a dot that lights when the footswitch is heard directly, and the input menu.
class FootInputLabel : public juce::Component
{
public:
    explicit FootInputLabel (LatheProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    LatheProcessor& proc;
};

// One footswitch on the Pedal page: its jobs, what it listens for, Learn and Defaults.
class FootswitchCard : public juce::Component
{
public:
    FootswitchCard (LatheProcessor&, int sw);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (double dt);

private:
    LatheProcessor& proc;
    int sw;
    FlipCard letter;
    ActionChip learn, reset;
    juce::String sends;
};

// Pebble sways with the modulation, leaning further as Depth rises. Wobbles like jelly while
// a ramp runs, goes still and dizzy under Brake, jitters in a flurry, holds its breath on a
// pause, dozes when bypassed, taps its foot on each press and nods after Keeper.
class Pebble : public juce::Component
{
public:
    explicit Pebble (LatheProcessor& p) : proc (p) {}
    void update (double t, double dt);
    void paint (juce::Graphics&) override;
    juce::String mood() const;
    juce::String hint() const;

private:
    LatheProcessor& proc;
    double now = 0.0;
    float sway = 0.0f, depth = 0.0f, wobble = 0.0f, brake = 0.0f, lean = 0.0f, level = 0.0f;
    float hop = 0.0f, nod = 0.0f;
    bool asleep = false, paused = false, flurry = false, ramping = false, blinking = false;
    double nextBlink = 2.0, blinkUntil = 0.0;
    int lastFoot = 0, lastKeep = 0;
};

class LatheEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LatheEditor (LatheProcessor&);
    ~LatheEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { proc.focusHere(); }
    void setPage (int p);

private:
    void timerCallback() override;
    void keeperMenu();
    void closeNote (bool save);
    void section (juce::Graphics&, juce::Rectangle<int>) const;

    LatheProcessor& proc;
    FlipLookAndFeel lnf;
    int page = 0;   // 0 Main, 1 Pedal

    // header
    Pills voice, preset;
    FlipCard tempoCard;
    ActionChip tapButton;
    ToggleChip syncChip, hostChip;
    ActionChip mainTab, pedalTab, keepButton, keepMenu;
    FootInputLabel footInput;

    // record
    RecordView record;
    FlipCard rateCard, swingCard;
    WaveView wave;
    Pills wave1, wave2, division;
    ToggleChip div368;

    // sound and motion
    Knob tone, lag, mix, volume, rate, depth, warp, ramp;
    ToggleChip bass;
    LabelledSlider bassHz;
    Pills output, tapControl, holdMode;

    // ramp bank
    std::array<std::unique_ptr<ToggleChip>, lathe::kTargets> moves, rises;
    Pills bounce, sweep, lofi, motoByp;
    ActionChip fireButton;

    // strip
    Pebble pebble;
    std::array<std::unique_ptr<FootButton>, 4> feet;
    OnButton onButton;
    ExpressionBar expression;

    // pedal page
    std::array<std::unique_ptr<FootswitchCard>, 4> cards;
    std::array<std::unique_ptr<ActionChip>, 10> targetChips;
    LabelledSlider expMin, expMax, expSmooth, widthSlider, noiseSlider;
    ActionChip calibrate, learnExp, curve, liveChip, learnKeep, exportKeep, folderKeep, mapChip;

    juce::TextEditor noteEditor;
    int noteNumber = 0;
    double noteUntil = 0.0;

    double lastTick = 0.0, start = 0.0;
};
} // namespace ui

using LatheEditor = ui::LatheEditor;
