#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// =====================================================================================
// Palette helpers
// =====================================================================================
namespace ff
{
Font font (float height, float kerning)
{
    static Typeface::Ptr typeface = Typeface::createSystemTypefaceFor (BinaryData::RobotoCondensedBold_ttf,
                                                                       BinaryData::RobotoCondensedBold_ttfSize);
    return Font (FontOptions (typeface).withHeight (height).withKerningFactor (kerning));
}

void label (Graphics& g, const String& text, Rectangle<int> r, Justification j, Colour c, float size)
{
    g.setColour (c);
    g.setFont (font (size, 0.08f));
    g.drawText (text, r, j, false);
}
} // namespace ff

namespace ui
{
static double nowSeconds() { return Time::getMillisecondCounterHiRes() * 0.001; }
static float raw (LatheProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
static bool isOn (LatheProcessor& p, const char* id) { return raw (p, id) > 0.5f; }
static int choiceOf (LatheProcessor& p, const char* id) { return roundToInt (raw (p, id)); }
static const String dot = String::fromUTF8 (" \xc2\xb7 ");

static void setParam (RangedAudioParameter* p, float plain)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
    p->endChangeGesture();
}

void pill (Graphics& g, Rectangle<float> b, const String& text, bool on, float size, Colour onColour)
{
    const float r = b.getHeight() * 0.5f;
    if (on) { g.setColour (onColour); g.fillRoundedRectangle (b, r); }
    else { g.setColour (ff::line); g.drawRoundedRectangle (b.reduced (1.0f), r, 2.0f); }
    g.setColour (on ? ff::bg : ff::ink);
    g.setFont (ff::font (size, 0.06f));
    g.drawText (text, b, Justification::centred, false);
}

// =====================================================================================
// Flip card
// =====================================================================================
void FlipCard::setValue (const String& v)
{
    if (v == current) return;
    previous = current;
    current = v;
    progress = previous.isEmpty() ? 1.0f : 0.0f;
    repaint();
}

void FlipCard::advance (double dt)
{
    if (progress < 1.0f)
    {
        progress = jmin (1.0f, progress + (float) (dt / 0.26));
        repaint();
    }
}

void FlipCard::drawHalf (Graphics& g, Rectangle<float> card, const String& text, bool top, float scale, bool shade)
{
    const float hinge = card.getCentreY();
    const float halfH = card.getHeight() * 0.5f;
    if (scale <= 0.001f) return;

    Graphics::ScopedSaveState save (g);
    auto clip = top ? Rectangle<float> (card.getX(), hinge - halfH * scale, card.getWidth(), halfH * scale)
                    : Rectangle<float> (card.getX(), hinge, card.getWidth(), halfH * scale);
    g.reduceClipRegion (clip.getSmallestIntegerContainer());
    g.addTransform (AffineTransform::scale (1.0f, scale, 0.0f, hinge));

    if (top) g.setGradientFill (ColourGradient (ff::cardHi, 0, card.getY(), ff::card, 0, hinge, false));
    else     g.setGradientFill (ColourGradient (ff::card, 0, hinge, ff::cardLow, 0, card.getBottom(), false));
    g.fillRoundedRectangle (card, 7.0f);
    g.setColour (ff::ink);
    g.setFont (ff::font (textHeight, 0.02f));
    g.drawFittedText (text, card.reduced (6.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);

    if (shade)
    {
        g.setColour (Colours::black.withAlpha (0.45f * (1.0f - scale)));
        g.fillRoundedRectangle (card, 7.0f);
    }
}

void FlipCard::paint (Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    g.setColour (ff::housing);
    g.fillRoundedRectangle (outer, 10.0f);
    auto card = outer.reduced (6.0f);

    if (progress >= 1.0f)
    {
        drawHalf (g, card, current, true, 1.0f, false);
        drawHalf (g, card, current, false, 1.0f, false);
    }
    else
    {
        drawHalf (g, card, current, true, 1.0f, false);
        drawHalf (g, card, previous, false, 1.0f, false);
        const float eased = 1.0f - std::pow (1.0f - progress, 2.0f);
        if (eased < 0.5f) drawHalf (g, card, previous, true, 1.0f - eased * 2.0f, true);
        else              drawHalf (g, card, current, false, (eased - 0.5f) * 2.0f, true);
    }

    g.setColour (Colours::black);
    g.fillRect (card.getX(), card.getCentreY() - 1.5f, card.getWidth(), 3.0f);
    g.setColour (ff::pin);
    g.fillRoundedRectangle (outer.getX() + 1.0f, card.getCentreY() - 6.0f, 3.0f, 12.0f, 1.0f);
    g.fillRoundedRectangle (outer.getRight() - 4.0f, card.getCentreY() - 6.0f, 3.0f, 12.0f, 1.0f);
}

// =====================================================================================
// Pills and chips
// =====================================================================================
Pills::Pills (AudioProcessorValueTreeState& s, const String& paramId, StringArray l, float size)
    : labels (std::move (l)), textSize (size)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment = std::make_unique<ParameterAttachment> (*s.getParameter (paramId), [this] (float v) { selected = (int) std::lround (v); repaint(); });
    attachment->sendInitialUpdate();
}

Rectangle<float> Pills::cell (int i) const
{
    const int n = labels.size();
    const float gap = 6.0f, w = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
    return { (float) i * (w + gap), 0.0f, w, (float) getHeight() };
}

void Pills::paint (Graphics& g)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        Graphics::ScopedSaveState s (g);
        if (i == dimmed) g.setOpacity (0.55f);
        pill (g, cell (i).reduced (1.0f), labels[i], i == selected, textSize);
    }
}

void Pills::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < labels.size(); ++i)
        if (cell (i).contains (e.position))
        {
            attachment->setValueAsCompleteGesture ((float) i);
            if (onPick) onPick();
            return;
        }
}

ToggleChip::ToggleChip (AudioProcessorValueTreeState& s, const String& paramId, const String& t, const String& tOn, float size, Colour c)
    : text (t), onText (tOn.isEmpty() ? t : tOn), textSize (size), onColour (c)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment = std::make_unique<ParameterAttachment> (*s.getParameter (paramId), [this] (float v) { on = v > 0.5f; repaint(); });
    attachment->sendInitialUpdate();
}

void ToggleChip::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (cornerRadius < 0.0f) { pill (g, b, on ? onText : text, on, textSize, onColour); return; }
    if (on) { g.setColour (onColour); g.fillRoundedRectangle (b, cornerRadius); }
    else { g.setColour (ff::line); g.drawRoundedRectangle (b.reduced (1.0f), cornerRadius, 2.0f); }
    g.setColour (on ? ff::bg : ff::ink);
    g.setFont (ff::font (textSize, 0.08f));
    g.drawText (on ? onText : text, b, Justification::centred, false);
}

void ToggleChip::mouseDown (const MouseEvent&) { attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

// =====================================================================================
// Look and feel
// =====================================================================================
void FlipLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const float left = (float) x + 7.0f, right = (float) (x + w) - 7.0f;
    g.setColour (ff::ring);
    g.fillRoundedRectangle (left, cy - 3.0f, right - left, 6.0f, 3.0f);
    g.setColour (ff::ink.withAlpha (s.isMouseOverOrDragging() ? 1.0f : 0.85f));
    g.fillRoundedRectangle (left, cy - 3.0f, jmax (0.0f, pos - left), 6.0f, 3.0f);
    g.setColour (ff::ink);
    g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
}

static void setupRotary (Slider& s)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setMouseCursor (MouseCursor::PointingHandCursor);
}

// =====================================================================================
// Knobs
// =====================================================================================
Knob::Knob (LatheProcessor& p, const String& paramId, const String& c, int t, bool redHand)
    : proc (p), caption (c), target (t), red (redHand)
{
    param = p.apvts.getParameter (paramId);
    setupRotary (slider);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [this, paramId]
    {
        proc.focusHere();
        // the pedal: turning RPM takes the rate back from tap tempo
        if (paramId == ParamIDs::rate) setParam (proc.apvts.getParameter (ParamIDs::sync), 0.0f);
    };
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::resized() { slider.setBounds (Rectangle<int> (80, 80).withCentre ({ getWidth() / 2, 20 + 40 })); }

void Knob::tick()
{
    const float v = target >= 0 ? proc.getEngine().uiEff[(size_t) target].load() : (float) slider.getValue();
    if (std::abs (v - shown) > 0.002f) { shown = v; repaint(); }
}

void Knob::paint (Graphics& g)
{
    const float knob = (float) slider.getValue();
    const float heard = shown < 0.0f ? knob : shown;
    const bool moving = std::abs (heard - knob) > 0.006f;
    ff::label (g, caption.toUpperCase(), { 0, 0, getWidth(), 18 }, Justification::centred, moving ? ff::amber : ff::dim);

    const Point<float> c ((float) getWidth() * 0.5f, 60.0f);
    const float r = 39.0f, start = -degreesToRadians (135.0f), span = degreesToRadians (270.0f);
    g.setColour (ff::ring);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (ff::bg);
    g.fillEllipse (c.x - r * 0.9f, c.y - r * 0.9f, r * 1.8f, r * 1.8f);
    g.setColour (ff::ink);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + span * (float) i / 10.0f;
        g.drawLine ({ { c.x + std::sin (a) * r * 0.80f, c.y - std::cos (a) * r * 0.80f },
                      { c.x + std::sin (a) * r * 0.92f, c.y - std::cos (a) * r * 0.92f } }, 2.0f);
    }
    if (moving)
    {
        Path ghost;
        ghost.addRoundedRectangle (-1.5f, -r * 0.68f, 3.0f, r * 0.6f, 1.5f);
        g.setColour (ff::faint);
        g.fillPath (ghost, AffineTransform::rotation (start + knob * span).translated (c));
    }
    Path hand;
    hand.addRoundedRectangle (-3.0f, -r * 0.68f, 6.0f, r * 0.68f + r * 0.15f, 2.0f);
    g.setColour (red ? ff::red : (moving ? ff::amber : ff::ink));
    g.fillPath (hand, AffineTransform::rotation (start + heard * span).translated (c));
    g.setColour (ff::ink);
    g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    g.setColour (ff::bg);
    g.fillEllipse (c.x - 1.8f, c.y - 1.8f, 3.6f, 3.6f);

    const String value = valueText ? valueText (heard) : param->getCurrentValueAsText();
    ff::label (g, value.toUpperCase(), { 0, 104, getWidth(), 22 }, Justification::centred, ff::ink, 15.0f);
}

LabelledSlider::LabelledSlider (AudioProcessorValueTreeState& s, const String& paramId, const String& c) : caption (c)
{
    param = s.getParameter (paramId);
    slider.setSliderStyle (Slider::LinearHorizontal);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setMouseCursor (MouseCursor::PointingHandCursor);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void LabelledSlider::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (16)); }

void LabelledSlider::paint (Graphics& g)
{
    auto top = getLocalBounds().removeFromTop (16);
    ff::label (g, caption.toUpperCase(), top, Justification::centredLeft, ff::dim, 11.0f);
    ff::label (g, param->getCurrentValueAsText().toUpperCase(), top, Justification::centredRight, ff::ink, 11.0f);
}

// =====================================================================================
// Record and wave
// =====================================================================================
void RecordView::tick (double dt)
{
    auto& e = proc.getEngine();
    const bool on = ! isOn (proc, ParamIDs::bypass);
    const float hz = e.uiHz.load(), brake = e.uiBrake.load();
    angle += (float) dt * (on ? (1.1f + 0.5f * jmin (6.0f, hz)) * (1.0f - brake) : 0.08f);
    angle = std::fmod (angle, MathConstants<float>::twoPi);
    lfo = on ? e.uiLfo.load() : 0.0f;
    depth = e.uiEff[lathe::TDepth].load();
    repaint();
}

void RecordView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f - 4.0f;
    const Point<float> c (b.getX() + R + 4.0f, b.getCentreY());
    const bool mk2 = choiceOf (proc, ParamIDs::voice) == lathe::MkII;

    g.setColour (ff::ring);
    g.fillEllipse (c.x - R - 2, c.y - R - 2, (R + 2) * 2, (R + 2) * 2);
    // the disc, its edge breathing with Depth like a warped record
    Path disc;
    for (int i = 0; i <= 64; ++i)
    {
        const float a = (float) i / 64.0f * MathConstants<float>::twoPi;
        const float r = R - 4.0f + std::sin (a * 2.0f + angle) * 2.6f * depth;
        const Point<float> p (c.x + std::cos (a) * r, c.y + std::sin (a) * r);
        if (i == 0) disc.startNewSubPath (p); else disc.lineTo (p);
    }
    disc.closeSubPath();
    g.setColour (Colour (0xff0a0a0a));
    g.fillPath (disc);
    g.setColour (Colour (0xff232323));
    for (float r = R * 0.38f; r < R * 0.9f; r += 3.2f) g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    // two glints that show it turning
    g.setColour (ff::line);
    for (float off : { 0.0f, MathConstants<float>::pi })
    {
        Path glint;
        glint.addCentredArc (c.x, c.y, R * 0.7f, R * 0.7f, 0.0f, angle + off, angle + off + 0.8f, true);
        g.strokePath (glint, PathStrokeType (1.6f));
    }
    // the label
    const float lr = R * 0.32f;
    g.setColour (mk2 ? ff::amberDeep : ff::amber);
    g.fillEllipse (c.x - lr, c.y - lr, lr * 2, lr * 2);
    {
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (angle, c.x, c.y));
        g.setColour (ff::bg);
        g.setFont (ff::font (11.0f, 0.1f));
        g.drawText ("LATHE", Rectangle<float> (lr * 2, 14).withCentre ({ c.x, c.y - lr * 0.42f }), Justification::centred, false);
        g.setFont (ff::font (9.0f, 0.1f));
        g.drawText (mk2 ? "MKII" : "HIFI", Rectangle<float> (lr * 2, 12).withCentre ({ c.x, c.y + lr * 0.5f }), Justification::centred, false);
    }
    g.setColour (ff::bg);
    g.fillEllipse (c.x - 3, c.y - 3, 6, 6);

    // the tonearm, riding the wave
    const Point<float> pivot (b.getRight() - 12.0f, b.getY() + 14.0f);
    const float arm = -0.42f + lfo * 0.06f * (0.3f + depth);
    Path p;
    p.startNewSubPath (0, 0);
    p.lineTo (0, R * 1.02f);
    p.lineTo (-R * 0.16f, R * 1.2f);
    g.setColour (ff::dim);
    g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::rotation (arm).translated (pivot));
    Path head;
    head.addRectangle (-R * 0.24f, R * 1.17f, R * 0.13f, 8.0f);
    g.setColour (ff::ink);
    g.fillPath (head, AffineTransform::rotation (arm).translated (pivot));
    g.setColour (ff::line);
    g.fillEllipse (pivot.x - 10, pivot.y - 10, 20, 20);
}

void WaveView::paint (Graphics& g)
{
    auto& e = proc.getEngine();
    const float w = (float) getWidth(), h = (float) getHeight(), mid = h * 0.5f;
    const int s1 = choiceOf (proc, ParamIDs::wave1), s2 = names::wave2Shape (choiceOf (proc, ParamIDs::wave2));
    const float warp = e.uiEff[lathe::TWarp].load(), depth = e.uiEff[lathe::TDepth].load();
    const float amp = (mid - 6.0f) * (0.25f + 0.75f * depth);
    const bool on = ! isOn (proc, ParamIDs::bypass);

    g.setColour (ff::ring);
    g.fillRect (0.0f, mid, w, 1.0f);
    Path p;
    for (int x = 0; x <= (int) w; ++x)
    {
        const float y = mid - chase::moduShape ((float) x / w, s1, s2, warp) * amp;
        if (x == 0) p.startNewSubPath (0.0f, y); else p.lineTo ((float) x, y);
    }
    g.setColour (on ? ff::ink : ff::faint);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const float ph = e.uiPhase.load(), x = ph * w;
    const float y = mid - chase::moduShape (ph, s1, s2, warp) * amp;
    g.setColour (ff::red);
    g.fillRect (x - 1.0f, 2.0f, 2.0f, h - 4.0f);
    g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
}

// =====================================================================================
// The footswitch strip
// =====================================================================================
void FootButton::tick()
{
    auto& e = proc.getEngine();
    String t, h;
    Colour l = ff::pin;
    bool lt = false;
    switch (sw)
    {
        case 0:
        {
            const bool on = ! isOn (proc, ParamIDs::bypass);
            t = isOn (proc, ParamIDs::motoByp) ? "ON WHILE HELD" : "TAP BYPASS";
            h = isOn (proc, ParamIDs::motoByp) ? "MOMENTARY ONLY" : "HOLD MOMENTARY";
            lt = on; if (on) l = ff::red;
            break;
        }
        case 1:
        {
            const bool braking = e.uiBrake.load() > 0.02f;
            t = "TAP TEMPO"; h = "HOLD BRAKE";
            const bool beat = std::fmod (nowSeconds() * proc.bpmNow() / 60.0, 1.0) < 0.15;
            lt = braking || (isOn (proc, ParamIDs::sync) && beat);
            l = braking ? ff::coral : ff::teal;
            break;
        }
        case 2:
        {
            const int m = choiceOf (proc, ParamIDs::holdMode);
            t = "TAP RAMP ONCE";
            h = m == lathe::HoldPause ? "HOLD PAUSE" : (m == lathe::HoldFlurry ? "HOLD FLURRY" : "HOLD RAMP");
            lt = e.uiRamping.load() || e.uiHeld.load() || isOn (proc, ParamIDs::hold);
            l = ff::amber;
            break;
        }
        default:
        {
            const int slot = choiceOf (proc, ParamIDs::preset);
            t = slot == 0 ? "TAP TO B" : (slot == 2 ? "TAP TO A" : "TAP A OR B");
            h = slot == 1 ? "HOLD SAVE TO A" : String ("HOLD SAVE TO ") + (slot == 0 ? "A" : "B");
            lt = slot != 1; l = ff::ink;
            break;
        }
    }
    const bool d = (proc.uiFootDown.load() >> sw) & 1;
    if (t != tapText || h != holdText || l != led || lt != lit || d != down)
    {
        tapText = t; holdText = h; led = l; lit = lt; down = d;
        repaint();
    }
}

void FootButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? Colour (0xff1a1206) : ff::housing);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (down ? ff::amber : ff::line);
    g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 2.0f);
    const Point<float> p (r.getX() + 14.0f, r.getY() + 14.0f);
    if (lit) { g.setColour (led.withAlpha (0.22f)); g.fillEllipse (p.x - 9, p.y - 9, 18, 18); }
    g.setColour (lit ? led : ff::pin);
    g.fillEllipse (p.x - 5, p.y - 5, 10, 10);
    ff::label (g, String::charToString ((juce_wchar) ('A' + sw)), { 0, 6, getWidth(), 30 }, Justification::centred, ff::ink, 26.0f);
    ff::label (g, tapText, { 2, 42, getWidth() - 4, 14 }, Justification::centred, ff::dim, 10.0f);
    ff::label (g, holdText, { 2, 58, getWidth() - 4, 14 }, Justification::centred, ff::dim, 10.0f);
}

void OnButton::tick()
{
    const bool o = ! isOn (proc, ParamIDs::bypass);
    if (o != on) { on = o; repaint(); }
}

void OnButton::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (ff::bg);
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (on ? ff::ink : ff::line);
    g.drawRoundedRectangle (b.reduced (1.0f), b.getHeight() * 0.5f, 2.0f);
    ff::label (g, on ? "ON" : "BYPASSED", getLocalBounds().withTrimmedBottom (20).withTrimmedTop (8), Justification::centred, ff::ink, 18.0f);
    ff::label (g, isOn (proc, ParamIDs::motoByp) ? "HOLD TO PLAY" : "TAP TO BYPASS", getLocalBounds().withTrimmedTop (34).withTrimmedBottom (10),
               Justification::centred, ff::dim, 11.0f);
}

ExpressionBar::ExpressionBar (LatheProcessor& p)
    : proc (p),
      morphChip ("MORPH", [this] { setParam (target, (float) names::ExpMorph); }, 10.5f),
      rampChip ("RAMP", [this] { setParam (target, (float) names::ExpRampTargets); }, 10.5f),
      whammyChip ("WHAMMY", [this] { setParam (target, (float) names::ExpWhammy); }, 10.5f)
{
    target = p.apvts.getParameter (ParamIDs::expTarget);
    morphChip.setColour (ff::teal);
    rampChip.setColour (ff::amber);
    whammyChip.setColour (ff::coral);
    addAndMakeVisible (morphChip);
    addAndMakeVisible (rampChip);
    addAndMakeVisible (whammyChip);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void ExpressionBar::resized()
{
    whammyChip.setBounds (getWidth() - 62, 0, 62, 22);
    rampChip.setBounds (getWidth() - 112, 0, 48, 22);
    morphChip.setBounds (getWidth() - 166, 0, 52, 22);
}

void ExpressionBar::tick()
{
    const int t = roundToInt (raw (proc, ParamIDs::expTarget));
    morphChip.setLit (t == names::ExpMorph);
    rampChip.setLit (t == names::ExpRampTargets);
    whammyChip.setLit (t == names::ExpWhammy);
    const float v = proc.expressionLevel();
    if (t != lastTarget || std::abs (v - level) > 0.002f) { lastTarget = t; level = v; repaint(); }
    else if (proc.isCalibrating() || proc.isLearning()) repaint();
}

void ExpressionBar::paint (Graphics& g)
{
    const auto& t = names::expTargets();
    const int which = jlimit (0, (int) t.size() - 1, roundToInt (raw (proc, ParamIDs::expTarget)));
    String head = "EXPRESSION";
    if (which >= names::ExpDepth) head += dot + String (t[(size_t) which].label).toUpperCase();
    head += "  v";
    if (proc.isCalibrating()) head = "ROCK HEEL TO TOE";
    else if (proc.learningWhat() == LatheProcessor::LearnExpression) head = "MOVE THE PEDAL";
    ff::label (g, head, { 0, 0, getWidth() - 168, 22 }, Justification::centredLeft, proc.isCalibrating() ? ff::ink : ff::dim, 11.0f);

    String heel = "HEEL", toe = "TOE";
    if (which == names::ExpMorph) { heel = "HEEL" + dot + "A"; toe = "TOE" + dot + "B"; }
    else if (which == names::ExpRampTargets) { heel = "HEEL" + dot + "START"; toe = "TOE" + dot + "END"; }
    else if (which == names::ExpWhammy) { heel = "HEEL" + dot + "DOWN"; toe = "TOE" + dot + "UP"; }
    ff::label (g, heel, { 0, 30, 120, 14 }, Justification::centredLeft, ff::dim, 10.5f);
    ff::label (g, toe, { getWidth() - 120, 30, 120, 14 }, Justification::centredRight, ff::dim, 10.5f);

    const float x0 = 14.0f, w = (float) getWidth() - 28.0f, y = 62.0f;
    g.setColour (ff::ring);
    g.fillRoundedRectangle (x0 - 14.0f, y - 11.0f, w + 28.0f, 22.0f, 11.0f);
    g.setGradientFill (ColourGradient (ff::teal.withAlpha (0.75f), x0, y, ff::amber.withAlpha (0.75f), x0 + w, y, false));
    g.fillRoundedRectangle (x0 - 14.0f, y - 11.0f, jmax (22.0f, w * level + 28.0f), 22.0f, 11.0f);
    const bool fresh = nowSeconds() - proc.lastExpressionMove() < 1.5;
    g.setColour (fresh ? ff::ink : ff::dim);
    g.fillEllipse (x0 + w * level - 14.0f, y - 14.0f, 28.0f, 28.0f);
    g.setColour (ff::face);
    g.drawEllipse (x0 + w * level - 14.0f, y - 14.0f, 28.0f, 28.0f, 3.0f);
    const String note = which == names::ExpMorph ? "EVERY KNOB GLIDES FROM PRESET A TO B"
                      : which == names::ExpRampTargets ? "MOVES THE KNOBS IN THE RAMP BANK"
                      : which == names::ExpWhammy ? "BENDS THE WAVE BY FOOT" : "CLICK FOR CALIBRATE AND LEARN";
    ff::label (g, note, { 0, getHeight() - 12, getWidth(), 12 }, Justification::centred, ff::faint, 9.5f);
}

void ExpressionBar::mouseDown (const MouseEvent&)
{
    proc.focusHere();
    PopupMenu targets;
    const auto& t = names::expTargets();
    const int cur = roundToInt (raw (proc, ParamIDs::expTarget));
    for (int i = 0; i < (int) t.size(); ++i) targets.addItem (100 + i, t[(size_t) i].label, true, i == cur);
    PopupMenu curves;
    const char* const curveNames[3] = { "Linear", "Slow start", "Fast start" };
    for (int i = 0; i < 3; ++i) curves.addItem (200 + i, curveNames[i], true, proc.pedals().curve == i);

    PopupMenu m;
    m.addSubMenu ("Expression moves", targets);
    m.addItem (1, "Calibrate heel and toe");
    m.addItem (2, "Learn expression pedal");
    m.addSubMenu ("Curve", curves);
    m.addItem (3, "Range and smoothing on the Pedal page");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r == 1) proc.calibrateExpression();
        else if (r == 2) proc.learnExpression();
        else if (r == 3) { if (showPedalPage) showPedalPage(); }
        else if (r >= 100 && r < 200) setParam (target, (float) (r - 100));
        else if (r >= 200 && r < 300) proc.setCurve (r - 200);
        repaint();
    });
}

void FootInputLabel::paint (Graphics& g)
{
    g.setColour (proc.pedalsDirect() ? ff::green : ff::pin);
    g.fillEllipse (0.0f, (float) getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
    ff::label (g, proc.pedalsDirect() ? "FOOTSWITCHES READY  v" : "FOOTSWITCHES  v", getLocalBounds().withTrimmedLeft (16), Justification::centredLeft, ff::dim, 12.0f);
}

void FootInputLabel::mouseDown (const MouseEvent&)
{
    PopupMenu m;
    m.addSectionHeader ("Footswitch input");
    m.addItem (-1, proc.footInputStatus(), false, false);
    const auto chosen = proc.footDevice();
    m.addItem (10, "Find my footswitch automatically", true, chosen.isEmpty());
    const auto devices = DirectMidiInput::deviceNames();
    for (int i = 0; i < devices.size() && i < 40; ++i)
        m.addItem (100 + i, "Use " + devices[i], true, chosen == devices[i]);
    m.addItem (11, "Only from a MIDI track in Ableton", true, chosen == "none");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, devices] (int r)
    {
        if (r == 10) proc.setFootDevice ({});
        else if (r == 11) proc.setFootDevice ("none");
        else if (r >= 100 && r - 100 < devices.size()) proc.setFootDevice (devices[r - 100]);
        repaint();
    });
}

// =====================================================================================
// Pedal page
// =====================================================================================
FootswitchCard::FootswitchCard (LatheProcessor& p, int s)
    : proc (p), sw (s),
      learn ("LEARN", [this] { proc.learnFootswitch (sw); }),
      reset ("DEFAULTS", [this] { proc.resetFootswitches(); })
{
    letter.setTextHeight (54.0f);
    letter.setValue (String::charToString ((juce_wchar) ('A' + s)));
    addAndMakeVisible (letter);
    addAndMakeVisible (learn);
    addAndMakeVisible (reset);
}

void FootswitchCard::resized()
{
    letter.setBounds (16, 40, getWidth() - 32, 90);
    learn.setBounds (16, getHeight() - 50, (getWidth() - 40) / 2, 32);
    reset.setBounds (24 + (getWidth() - 40) / 2, getHeight() - 50, (getWidth() - 40) / 2, 32);
}

void FootswitchCard::paint (Graphics& g)
{
    g.setColour (ff::face);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
    ff::label (g, "FOOTSWITCH", { 16, 12, 200, 18 });

    static const char* const taps[4] = { "BYPASS", "TAP TEMPO", "RAMP ONCE", "PRESET A OR B" };
    static const char* const holds[4] = { "MOMENTARY", "BRAKE", "HOLD MODE", "SAVE PRESET" };
    static const char* const notes[4] = { "ACTS ON THE PRESS", "THE RECORD SLOWS AND STOPS", "RAMP, PAUSE OR FLURRY", "GLIDES BETWEEN SETTINGS" };
    static const char* const combos[4] = { "A+B  RESET THE RAMP", "A+B  RESET THE RAMP", "C+D  NEXT COPY", "C+D  NEXT COPY" };
    struct Row { const char* k; String v; };
    const Row rows[3] = { { "TAP", taps[sw] }, { "HOLD", holds[sw] }, { "LISTENS FOR", sends } };
    const int w = getWidth() - 32;
    int y = 146;
    for (const auto& r : rows)
    {
        g.setColour (ff::ring);
        g.fillRect (16, y, w, 2);
        ff::label (g, r.k, { 16, y + 4, 100, 30 }, Justification::centredLeft, ff::dim, 12.0f);
        ff::label (g, r.v, { 16, y + 4, w, 30 }, Justification::centredRight, ff::ink, 13.0f);
        y += 38;
    }
    g.setColour (ff::ring);
    g.fillRect (16, y, w, 2);
    ff::label (g, notes[sw], { 16, y + 8, w, 16 }, Justification::centredLeft, ff::dim, 11.0f);
    ff::label (g, combos[sw], { 16, y + 30, w, 16 }, Justification::centredLeft, ff::dim, 11.0f);
    if (sw >= 2) ff::label (g, "C+D HELD 1 S  KEEPER", { 16, y + 50, w, 16 }, Justification::centredLeft, ff::dim, 11.0f);
}

void FootswitchCard::tick (double dt)
{
    const auto& s = proc.pedals();
    const String t = "CH " + String (s.fsChannel[(size_t) sw]) + dot + "CC " + String (s.fsCC[(size_t) sw]);
    if (t != sends) { sends = t; repaint(); }
    learn.setLit (proc.learningWhat() == sw);
    learn.setText (proc.learningWhat() == sw ? "PRESS IT" : "LEARN");
    const bool down = (proc.uiFootDown.load() >> sw) & 1;
    letter.setAlpha (down ? 0.6f : 1.0f);
    letter.advance (dt);
}

// =====================================================================================
// Pebble
// =====================================================================================
void Pebble::update (double t, double dt)
{
    now = t;
    auto& e = proc.getEngine();
    asleep = isOn (proc, ParamIDs::bypass);
    depth = e.uiEff[lathe::TDepth].load();
    const float target = asleep ? 0.0f : e.uiLfo.load();
    sway += (target - sway) * (float) jmin (1.0, dt * 12.0);
    brake = asleep ? 0.0f : e.uiBrake.load();
    paused = ! asleep && e.uiPaused.load();
    flurry = ! asleep && e.uiFlurry.load();
    ramping = ! asleep && (e.uiRamping.load() || e.uiHeld.load());
    level = e.uiOut.load();
    wobble += ((ramping ? 1.0f : 0.0f) - wobble) * (float) jmin (1.0, dt * (ramping ? 6.0 : 1.5));

    const float leanTarget = t - proc.lastExpressionMove() < 1.5 ? (proc.expressionLevel() - 0.5f) * 0.3f : 0.0f;
    lean += (leanTarget - lean) * (float) jmin (1.0, dt * 6.0);

    if (proc.uiFootPulse.load() != lastFoot) { lastFoot = proc.uiFootPulse.load(); hop = 1.0f; }
    if (proc.uiKeepPulse.load() != lastKeep) { lastKeep = proc.uiKeepPulse.load(); nod = 1.0f; }
    hop = jmax (0.0f, hop - (float) dt * 4.0f);
    nod = jmax (0.0f, nod - (float) dt * 1.6f);

    if (t > nextBlink) { blinking = true; blinkUntil = t + 0.14; nextBlink = t + 2.5 + Random::getSystemRandom().nextDouble() * 4.0; }
    if (blinking && t > blinkUntil) blinking = false;
    repaint();
}

String Pebble::mood() const
{
    const double ago = nowSeconds() - proc.uiMessageAt;
    if (ago < 2.5 && proc.uiMessage.isNotEmpty()) return proc.uiMessage;
    if (asleep) return "DOZING";
    if (brake > 0.95f) return "STOPPED";
    if (brake > 0.02f) return "BRAKING";
    if (flurry) return "FLURRY";
    if (paused) return "PAUSED";
    if (wobble > 0.3f) return "WOBBLING";
    if (nowSeconds() - proc.lastExpressionMove() < 1.5 && choiceOf (proc, ParamIDs::expTarget) == names::ExpMorph) return "MORPHING";
    return "SWAYING";
}

String Pebble::hint() const
{
    if (asleep) return "TAP A TO WAKE PEBBLE";
    if (brake > 0.02f) return "LET GO OF B TO SPIN UP";
    if (flurry || paused) return "LET GO OF C";
    if (wobble > 0.3f) return "THE KNOBS ARE RAMPING";
    if (isOn (proc, ParamIDs::bass)) return "LOWS DRY" + dot + "TOP END WARBLES";
    return {};
}

void Pebble::paint (Graphics& g)
{
    auto a = getLocalBounds().toFloat();
    const float s = a.getWidth() / 240.0f;
    const auto tf = AffineTransform::translation (-100.0f, -106.0f).scaled (s).translated (a.getCentreX(), a.getCentreY());
    const float t = (float) now;

    // the ring: teal swaying, amber while ramping, coral braking
    if (! asleep)
    {
        const Colour ringCol = brake > 0.02f ? ff::coral : (wobble > 0.3f ? ff::amber : ff::teal);
        Path rp; rp.addEllipse (100 - 80, 106 - 70, 160, 140);
        g.setColour (ringCol.withAlpha (0.65f));
        g.strokePath (rp, PathStrokeType (6.0f), AffineTransform::translation (-100, -106).rotated (sway * 0.1f).translated (100, 106).followedBy (tf));
    }

    // the body sways with the wave, further as Depth rises
    const float swayAmt = paused ? 0.0f : sway * (1.0f - brake);
    const float dx = swayAmt * (6.0f + 18.0f * depth) + (flurry ? 2.5f * std::sin (t * 60.0f) : 0.0f);
    const float tilt = swayAmt * (0.08f + 0.22f * depth) + lean;
    const float jelly = wobble * std::sin (t * 17.0f);
    const float breathe = asleep ? 1.0f + 0.025f * std::sin (t * 1.6f) : 1.0f;
    const float droop = brake * 0.12f;
    const float sx = (1.0f + 0.12f * jelly + 0.05f * nod + droop * 0.6f) * breathe;
    const float sy = (1.0f - 0.12f * jelly - 0.08f * nod * std::sin (nod * 9.0f) - droop) / breathe;
    const float bob = (asleep || paused) ? 0.0f : 1.5f * std::sin (t * 2.4f);
    const float dy = bob - hop * 10.0f + droop * 30.0f;
    const auto body = AffineTransform::translation (-100.0f, -106.0f).scaled (sx, sy).rotated (tilt)
                          .translated (100.0f + dx, 106.0f + dy).followedBy (tf);
    auto ell = [&] (float cx, float cy, float rx, float ry, Colour col, const AffineTransform& at)
    {
        Path p; p.addEllipse (cx - rx, cy - ry, rx * 2, ry * 2);
        g.setColour (col); g.fillPath (p, at);
    };
    auto stroke = [&] (Path& p, float w)
    {
        g.setColour (ff::faceInk);
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded), body);
    };

    // feet, one tapping on each press
    ell (78 + dx * 0.4f, 160 - hop * 8.0f, 16, 7, ff::blobShade, tf);
    ell (122 + dx * 0.4f, 160, 16, 7, ff::blobShade, tf);

    ell (107, 109, 58, 48, ff::blobShade, body);
    ell (100, 100, 58, 48, ff::blob, body);

    if (asleep || blinking)
    {
        for (float ex : { 80.0f, 120.0f })
        {
            Path lid; lid.startNewSubPath (ex - 12, 94); lid.quadraticTo (ex, 102, ex + 12, 94);
            stroke (lid, 6.0f);
        }
    }
    else
    {
        const float look = swayAmt * 4.0f;
        for (int k = 0; k < 2; ++k)
        {
            const float ex = k == 0 ? 80.0f : 120.0f;
            ell (ex, 92, 14, 14, ff::eye, body);
            if (brake > 0.1f)
            {
                // dizzy: spiral pupils as the record stops
                Path sp;
                for (int i = 0; i < 30; ++i)
                {
                    const float ang = (float) i * 0.45f + t * 4.0f * (k == 0 ? 1.0f : -1.0f), rr = (float) i * 0.32f;
                    const Point<float> q (ex + std::cos (ang) * rr, 93 + std::sin (ang) * rr);
                    if (i == 0) sp.startNewSubPath (q); else sp.lineTo (q);
                }
                stroke (sp, 2.5f);
            }
            else ell (ex + 2.0f + look, 94, 7, 7, ff::faceInk, body);
        }
    }

    if (asleep) { Path m; m.startNewSubPath (94, 124); m.lineTo (106, 124); stroke (m, 6.0f); }
    else if (brake > 0.1f || paused) ell (100, 123, 6.0f, 6.0f, ff::faceInk, body);   // holding its breath
    else if (wobble > 0.3f)
    {
        Path m; m.startNewSubPath (86, 120); m.quadraticTo (93, 130, 100, 120); m.quadraticTo (107, 130, 114, 120);
        stroke (m, 5.0f);   // a wobbly grin
    }
    else if (level > 0.3f) ell (100, 124, 7.0f + 3.0f * level, 4.0f + 7.0f * jmin (1.0f, level), ff::faceInk, body);
    else { Path m; m.startNewSubPath (90, 120); m.quadraticTo (100, 130, 110, 120); stroke (m, 6.0f); }

    if (asleep)
    {
        g.setFont (ff::font (16.0f, 0.0f));
        for (int k = 0; k < 3; ++k)
        {
            const float ph = (float) std::fmod (now * 0.5 + k / 3.0, 1.0);
            g.setColour (ff::ink.withAlpha (1.0f - ph));
            g.drawText ("z", Rectangle<float> (a.getRight() - 30.0f + ph * 8.0f, a.getY() + 24.0f - ph * 16.0f, 14.0f, 14.0f),
                        Justification::centred, false);
        }
    }
}

// =====================================================================================
// Editor
// =====================================================================================
LatheEditor::LatheEditor (LatheProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      voice (p.apvts, ParamIDs::voice, { "HIFI", "MKII" }, 12.0f),
      preset (p.apvts, ParamIDs::preset, { "A", "LIVE", "B" }, 12.0f),
      tapButton ("TAP", [this] { proc.focusHere(); proc.tapTempo(); }),
      syncChip (p.apvts, ParamIDs::sync, "SYNC", "SYNC", 12.0f),
      hostChip (p.apvts, ParamIDs::hostTempo, "TEMPO: TAPPED", "TEMPO: ABLETON", 10.5f),
      mainTab ("MAIN", [this] { setPage (0); }),
      pedalTab ("PEDAL", [this] { setPage (1); }),
      keepButton ("KEEP", [this] { proc.keepNow(); }),
      keepMenu ("v", [this] { keeperMenu(); }),
      footInput (p),
      record (p),
      wave (p),
      wave1 (p.apvts, ParamIDs::wave1, { "SINE", "TRI", "SQR" }, 12.0f),
      wave2 (p.apvts, ParamIDs::wave2, { "SQR", "TRI", "SINE" }, 12.0f),
      division (p.apvts, ParamIDs::division, { "1", "2", "4" }, 12.0f),
      div368 (p.apvts, ParamIDs::div368, String::fromUTF8 ("3 \xc2\xb7 6 \xc2\xb7 8"), {}, 12.0f),
      tone (p, ParamIDs::tone, "Tone", lathe::TTone),
      lag (p, ParamIDs::lag, "Lag", lathe::TLag),
      mix (p, ParamIDs::mix, "Mix", lathe::TMix, true),
      volume (p, ParamIDs::volume, "Volume", lathe::TVolume),
      rate (p, ParamIDs::rate, "Rate", lathe::TRate),
      depth (p, ParamIDs::depth, "Depth", lathe::TDepth),
      warp (p, ParamIDs::warp, "Warp", lathe::TWarp),
      ramp (p, ParamIDs::ramp, "Ramp", -1),
      bass (p.apvts, ParamIDs::bass, "BASS", "BASS", 13.0f, ff::amber),
      bassHz (p.apvts, ParamIDs::bassHz, "Below"),
      output (p.apvts, ParamIDs::output, { "MONO", "MISO", "STEREO" }, 12.0f),
      tapControl (p.apvts, ParamIDs::tapControl, { "RATE", "RAMP" }, 12.0f),
      holdMode (p.apvts, ParamIDs::holdMode, { "R RAMP", String::fromUTF8 ("\xe2\x88\x92 PAUSE"), "+ FLURRY" }, 11.5f),
      bounce (p.apvts, ParamIDs::bounce, { "RAMP", "BOUNCE" }, 11.0f),
      sweep (p.apvts, ParamIDs::sweepTop, { "BOTTOM", "TOP" }, 11.0f),
      lofi (p.apvts, ParamIDs::lofi, { "OFF", "ON" }, 11.0f),
      motoByp (p.apvts, ParamIDs::motoByp, { "OFF", "ON" }, 11.0f),
      fireButton ("RAMP NOW", [this] { proc.focusHere(); proc.fireRamp(); }, 11.5f),
      pebble (p),
      onButton (p),
      expression (p),
      expMin (p.apvts, ParamIDs::expMin, "Heel sets"),
      expMax (p.apvts, ParamIDs::expMax, "Toe sets"),
      expSmooth (p.apvts, ParamIDs::expSmooth, "Smoothing"),
      widthSlider (p.apvts, ParamIDs::width, "Width"),
      noiseSlider (p.apvts, ParamIDs::noise, "Hiss"),
      calibrate ("CALIBRATE", [this] { proc.calibrateExpression(); }),
      learnExp ("LEARN", [this] { proc.learnExpression(); }),
      curve ("CURVE", [this] { proc.setCurve ((proc.pedals().curve + 1) % 3); }),
      liveChip ("STUDIO", [this] { proc.setKeeperLive (! proc.keeperLive()); }),
      learnKeep ("LEARN SWITCH", [this] { proc.learnKeeper(); }),
      exportKeep ("EXPORT", [this] { proc.exportKeepers(); }),
      folderKeep ("FOLDER", [this] { proc.openKeepersFolder(); }),
      mapChip ("MIDI MAP OFF", [this]
      {
          PopupMenu m;
          m.addSectionHeader ("Original pedal MIDI map");
          m.addItem (100, "Off", true, proc.pedals().pedalMapChannel == 0);
          for (int c = 1; c <= 16; ++c) m.addItem (100 + c, "Channel " + String (c), true, proc.pedals().pedalMapChannel == c);
          m.showMenuAsync (PopupMenu::Options().withTargetComponent (&mapChip), [this] (int r) { if (r >= 100) proc.setPedalMapChannel (r - 100); });
      }, 10.5f)
{
    setLookAndFeel (&lnf);

    auto bpmOf = [this] { return proc.getEngine().uiHz.load(); };
    tone.valueText = [this] (float v) { return names::toneText (v, choiceOf (proc, ParamIDs::voice)); };
    lag.valueText = [this] (float v) { return String (lathe::lagMs (v, isOn (proc, ParamIDs::lofi)), 1) + " MS"; };
    mix.valueText = [] (float v) { return names::mixText (v); };
    volume.valueText = [] (float v) { return names::volumeText (v); };
    rate.valueText = [this, bpmOf] (float v)
    {
        if (isOn (proc, ParamIDs::sync) && choiceOf (proc, ParamIDs::tapControl) == 0) return String ("SYNCED");
        return names::rateText (lathe::rateHzFromKnob (v));
    };
    depth.valueText = [] (float v) { return String (roundToInt (v * 100.0f)) + "%"; };
    warp.valueText = [] (float v) { return names::warpText (v); };
    ramp.valueText = [this] (float v)
    {
        if (choiceOf (proc, ParamIDs::tapControl) == 1) return String ("FROM TAP");
        return names::rampText (v);
    };

    for (auto* c : std::initializer_list<Component*> { &voice, &preset, &tempoCard, &tapButton, &syncChip, &hostChip, &mainTab, &pedalTab,
                                                       &keepButton, &keepMenu, &footInput, &record, &rateCard, &swingCard, &wave, &wave1, &wave2,
                                                       &division, &div368, &tone, &lag, &mix, &volume, &rate, &depth, &warp, &ramp, &bass, &bassHz,
                                                       &output, &tapControl, &holdMode, &bounce, &sweep, &lofi, &motoByp, &fireButton,
                                                       &pebble, &onButton, &expression, &expMin, &expMax, &expSmooth, &widthSlider, &noiseSlider,
                                                       &calibrate, &learnExp, &curve, &liveChip, &learnKeep, &exportKeep, &folderKeep, &mapChip })
        addAndMakeVisible (c);

    tempoCard.setTextHeight (20.0f);
    rateCard.setTextHeight (24.0f);
    swingCard.setTextHeight (24.0f);
    preset.onPick = [this] { proc.focusHere(); };
    fireButton.setColour (ff::amber);
    expression.showPedalPage = [this] { setPage (1); };

    for (int t = 0; t < lathe::kTargets; ++t)
    {
        moves[(size_t) t] = std::make_unique<ToggleChip> (p.apvts, ParamIDs::move[t], "OFF", "ON", 11.0f, ff::amber);
        rises[(size_t) t] = std::make_unique<ToggleChip> (p.apvts, ParamIDs::rise[t], "FALL", "RISE", 11.0f);
        moves[(size_t) t]->cornerRadius = rises[(size_t) t]->cornerRadius = 7.0f;
        addAndMakeVisible (*moves[(size_t) t]);
        addAndMakeVisible (*rises[(size_t) t]);
    }
    for (int i = 0; i < 4; ++i)
    {
        feet[(size_t) i] = std::make_unique<FootButton> (p, i);
        addAndMakeVisible (*feet[(size_t) i]);
        cards[(size_t) i] = std::make_unique<FootswitchCard> (p, i);
        addChildComponent (*cards[(size_t) i]);
    }
    const auto& t = names::expTargets();
    static const char* const shortNames[10] = { "MORPH", "RAMP", "WHAMMY", "DEPTH", "RATE", "MIX", "LAG", "WARP", "TONE", "VOLUME" };
    for (int i = 0; i < (int) t.size(); ++i)
    {
        targetChips[(size_t) i] = std::make_unique<ActionChip> (shortNames[i],
            [this, i] { setParam (proc.apvts.getParameter (ParamIDs::expTarget), (float) i); }, 10.5f);
        addChildComponent (*targetChips[(size_t) i]);
    }

    noteEditor.setFont (ff::font (14.0f));
    noteEditor.setColour (TextEditor::backgroundColourId, ff::card);
    noteEditor.setColour (TextEditor::textColourId, ff::ink);
    noteEditor.setColour (TextEditor::outlineColourId, ff::line);
    noteEditor.setTextToShowWhenEmpty ("A NOTE FOR THIS KEEPER, THEN ENTER", ff::dim);
    noteEditor.onReturnKey = [this] { closeNote (true); };
    noteEditor.onEscapeKey = [this] { closeNote (false); };
    addChildComponent (noteEditor);

    setPage (0);
    setSize (1260, 660);
    start = lastTick = nowSeconds();
    startTimerHz (30);
}

LatheEditor::~LatheEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LatheEditor::setPage (int p)
{
    page = p;
    const bool main = p == 0;
    for (auto* c : std::initializer_list<Component*> { &record, &rateCard, &swingCard, &wave, &wave1, &wave2, &division, &div368,
                                                       &tone, &lag, &mix, &volume, &rate, &depth, &warp, &ramp, &bass, &bassHz, &output,
                                                       &tapControl, &holdMode, &bounce, &sweep, &lofi, &motoByp, &fireButton })
        c->setVisible (main);
    for (auto& c : moves) c->setVisible (main);
    for (auto& c : rises) c->setVisible (main);
    for (auto& c : cards) c->setVisible (! main);
    for (auto& c : targetChips) c->setVisible (! main);
    for (auto* c : std::initializer_list<Component*> { &expMin, &expMax, &expSmooth, &widthSlider, &noiseSlider, &hostChip, &mapChip,
                                                       &calibrate, &learnExp, &curve, &liveChip, &learnKeep, &exportKeep, &folderKeep })
        c->setVisible (! main);
    mainTab.setLit (main);
    pedalTab.setLit (! main);
    repaint();
}

void LatheEditor::keeperMenu()
{
    PopupMenu m;
    m.addItem (1, "Add a note to Keeper No. " + String (proc.lastKeeper()), proc.lastKeeper() > 0);
    m.addItem (2, "Export for Claude");
    m.addItem (3, "Open the Keepers folder");
    m.addSeparator();
    m.addItem (5, "Live", true, proc.keeperLive());
    m.addItem (6, "Studio", true, ! proc.keeperLive());
    m.addSeparator();
    m.addItem (4, "Learn a Keeper footswitch");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&keepMenu), [this] (int r)
    {
        if (r == 1)
        {
            noteNumber = proc.lastKeeper();
            noteEditor.clear();
            noteEditor.setVisible (true);
            noteEditor.grabKeyboardFocus();
            noteUntil = nowSeconds() + 20.0;
        }
        else if (r == 2) proc.exportKeepers();
        else if (r == 3) proc.openKeepersFolder();
        else if (r == 4) proc.learnKeeper();
        else if (r == 5 || r == 6) proc.setKeeperLive (r == 5);
    });
}

void LatheEditor::closeNote (bool save)
{
    if (save) proc.addKeeperNote (noteNumber, noteEditor.getText());
    if (save && noteEditor.getText().trim().isNotEmpty())
    {
        proc.uiMessage = "NOTED";
        proc.uiMessageAt = nowSeconds();
    }
    noteEditor.setVisible (false);
    noteEditor.clear();
    noteUntil = 0.0;
}

void LatheEditor::section (Graphics& g, Rectangle<int> r) const
{
    g.setColour (ff::face);
    g.fillRoundedRectangle (r.toFloat(), 12.0f);
}

void LatheEditor::paint (Graphics& g)
{
    g.fillAll (ff::bg);
    g.setColour (ff::ink);
    g.setFont (ff::font (26.0f, 0.06f));
    g.drawText ("LATHE", 20, 20, 90, 40, Justification::centredLeft, false);
    ff::label (g, "VOICE", { 112, 20, 50, 40 });
    ff::label (g, "PRESET", { 300, 20, 60, 40 });
    ff::label (g, "TEMPO", { 552, 20, 56, 40 });

    // the strip
    section (g, { 20, 532, 1220, 112 });
    ff::label (g, "PEBBLE", { 140, 544, 160, 16 }, Justification::centredLeft, ff::dim, 12.0f);
    ff::label (g, pebble.mood(), { 140, 560, 168, 30 }, Justification::centredLeft, ff::ink, 20.0f);
    ff::label (g, pebble.hint(), { 140, 592, 168, 30 }, Justification::topLeft, ff::amber, 10.5f);
    ff::label (g, "A+B RESET RAMP" + dot + "C+D HAND OVER" + dot + "HOLD C+D KEEPER", { 584, 632, 408, 12 }, Justification::centred, ff::faint, 9.5f);

    if (page == 0)
    {
        section (g, { 20, 76, 400, 440 });
        section (g, { 440, 76, 250, 440 });
        section (g, { 710, 76, 250, 440 });
        section (g, { 980, 76, 260, 440 });
        ff::label (g, "RECORD", { 36, 88, 200, 18 });
        ff::label (g, "SOUND", { 456, 88, 200, 18 });
        ff::label (g, "MOTION", { 726, 88, 200, 18 });
        ff::label (g, "RAMP", { 996, 88, 100, 18 });
        auto& e = proc.getEngine();
        String state = "RESTING";
        const bool bouncing = isOn (proc, ParamIDs::bounce);
        if (isOn (proc, ParamIDs::bypass)) state = "RESETS ON BYPASS";
        else if (e.uiHeld.load()) state = "HELD";
        else if (e.uiRamping.load()) state = bouncing ? "BOUNCING" : "RAMPING";
        else if (choiceOf (proc, ParamIDs::expTarget) == names::ExpRampTargets && nowSeconds() - proc.lastExpressionMove() < 3.0) state = "EXPRESSION";
        ff::label (g, state, { 1096, 88, 128, 18 }, Justification::centredRight, e.uiRamping.load() || e.uiHeld.load() ? ff::amber : ff::ink);

        ff::label (g, "RATE", { 262, 116, 142, 16 }, Justification::centredLeft, ff::dim, 12.0f);
        ff::label (g, "PITCH SWING", { 262, 208, 142, 16 }, Justification::centredLeft, ff::dim, 12.0f);
        ff::label (g, "FIRST HALF", { 36, 414, 180, 14 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "SECOND HALF", { 224, 414, 180, 14 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "DIVISION", { 36, 474, 70, 30 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "OUTPUT", { 456, 440, 218, 14 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "TAP SETS", { 726, 384, 218, 14 }, Justification::centredLeft, ff::dim, 11.0f);
        ff::label (g, "HOLD C TO", { 726, 440, 218, 14 }, Justification::centredLeft, ff::dim, 11.0f);

        ff::label (g, "MOVES", { 1040, 108, 92, 14 }, Justification::centred, ff::faint, 10.5f);
        ff::label (g, "WAY", { 1138, 108, 86, 14 }, Justification::centred, ff::faint, 10.5f);
        static const char* const rows[lathe::kTargets] = { "VOLUME", "LAG", "MIX", "RATE", "DEPTH", "WARP", "TONE" };
        for (int t = 0; t < lathe::kTargets; ++t) ff::label (g, rows[t], { 996, 124 + t * 30, 60, 26 }, Justification::centredLeft, ff::dim, 11.5f);
        static const char* const opts[4] = { "BOUNCE", "SWEEP", "LO-FI", "MOTOBYP" };
        for (int i = 0; i < 4; ++i) ff::label (g, opts[i], { 996, 344 + i * 32, 60, 28 }, Justification::centredLeft, ff::dim, 11.5f);
    }
    else
    {
        section (g, { 880, 76, 360, 280 });
        section (g, { 880, 366, 176, 150 });
        section (g, { 1064, 366, 176, 150 });
        ff::label (g, "EXPRESSION MOVES", { 896, 88, 260, 18 });
        ff::label (g, "KEEPER", { 896, 378, 80, 18 });
        ff::label (g, "No. " + String (proc.lastKeeper()), { 976, 378, 64, 18 }, Justification::centredRight, ff::ink, 12.0f);
        ff::label (g, "HOLD C+D OR PRESS KEEP", { 896, 496, 150, 14 }, Justification::centredLeft, ff::dim, 9.5f);
        ff::label (g, "DEEP", { 1080, 378, 140, 18 });
        ff::label (g, "SET ONCE, SHARED BY EVERY FLIP FELLA PLUGIN", { 896, 340, 330, 12 }, Justification::centredLeft, ff::faint, 9.5f);
    }
}

void LatheEditor::resized()
{
    // header
    voice.setBounds (156, 24, 128, 32);
    preset.setBounds (360, 24, 176, 32);
    tempoCard.setBounds (608, 20, 80, 40);
    tapButton.setBounds (696, 24, 56, 32);
    syncChip.setBounds (758, 24, 62, 32);
    mainTab.setBounds (836, 24, 60, 32);
    pedalTab.setBounds (900, 24, 66, 32);
    keepButton.setBounds (976, 24, 60, 32);
    keepMenu.setBounds (1040, 24, 30, 32);
    footInput.setBounds (1080, 20, 160, 40);

    // record
    record.setBounds (32, 110, 228, 228);
    rateCard.setBounds (262, 136, 142, 56);
    swingCard.setBounds (262, 228, 142, 56);
    wave.setBounds (36, 340, 368, 66);
    wave1.setBounds (36, 432, 180, 30);
    wave2.setBounds (224, 432, 180, 30);
    division.setBounds (110, 474, 200, 30);
    div368.setBounds (318, 474, 86, 30);

    // sound
    tone.setBounds (447, 106, 115, 130);
    lag.setBounds (562, 106, 115, 130);
    mix.setBounds (447, 242, 115, 130);
    volume.setBounds (562, 242, 115, 130);
    bass.setBounds (456, 390, 96, 36);
    bassHz.setBounds (562, 386, 112, 40);
    output.setBounds (456, 458, 218, 30);

    // motion
    rate.setBounds (717, 106, 115, 130);
    depth.setBounds (832, 106, 115, 130);
    warp.setBounds (717, 242, 115, 130);
    ramp.setBounds (832, 242, 115, 130);
    tapControl.setBounds (726, 402, 218, 30);
    holdMode.setBounds (726, 458, 218, 30);

    // ramp bank
    for (int t = 0; t < lathe::kTargets; ++t)
    {
        moves[(size_t) t]->setBounds (1046, 124 + t * 30, 86, 26);
        rises[(size_t) t]->setBounds (1138, 124 + t * 30, 86, 26);
    }
    bounce.setBounds (1046, 344, 178, 28);
    sweep.setBounds (1046, 376, 178, 28);
    lofi.setBounds (1046, 408, 178, 28);
    motoByp.setBounds (1046, 440, 178, 28);
    fireButton.setBounds (996, 478, 228, 28);

    // strip
    pebble.setBounds (22, 532, 116, 112);
    expression.setBounds (316, 542, 252, 96);
    for (int i = 0; i < 4; ++i) feet[(size_t) i]->setBounds (584 + i * 104, 544, 96, 84);
    onButton.setBounds (1008, 556, 216, 64);
    noteEditor.setBounds (316, 500, 252, 28);

    // pedal page
    for (int i = 0; i < 4; ++i) cards[(size_t) i]->setBounds (20 + i * 215, 76, 205, 440);
    for (int i = 0; i < 10; ++i) targetChips[(size_t) i]->setBounds (896 + (i % 4) * 82, 112 + (i / 4) * 32, 78, 26);
    expMin.setBounds (896, 212, 328, 32);
    expMax.setBounds (896, 244, 328, 32);
    expSmooth.setBounds (896, 276, 328, 32);
    calibrate.setBounds (896, 312, 104, 26);
    learnExp.setBounds (1008, 312, 104, 26);
    curve.setBounds (1120, 312, 104, 26);
    liveChip.setBounds (896, 400, 144, 26);
    learnKeep.setBounds (896, 430, 144, 26);
    exportKeep.setBounds (896, 460, 70, 26);
    folderKeep.setBounds (970, 460, 70, 26);
    widthSlider.setBounds (1080, 396, 144, 30);
    noiseSlider.setBounds (1080, 428, 144, 30);
    hostChip.setBounds (1080, 462, 144, 22);
    mapChip.setBounds (1080, 488, 144, 22);
}

void LatheEditor::timerCallback()
{
    const double now = nowSeconds();
    const double dt = jlimit (0.0, 0.2, now - lastTick);
    lastTick = now;

    for (auto& f : feet) f->tick();
    onButton.tick();
    expression.tick();
    pebble.update (now - start, dt);
    footInput.repaint();

    tempoCard.setValue (String (roundToInt (proc.bpmNow())));
    tempoCard.advance (dt);
    preset.setDimmed (proc.slotEdited() ? choiceOf (proc, ParamIDs::preset) : -1);

    // Keeper: KEPT on the button for a moment, and the learn state
    const bool kept = proc.uiMessage == "KEPT" && now - proc.uiMessageAt < 1.5;
    keepButton.setLit (kept);
    keepButton.setText (kept ? "KEPT" : (proc.learningWhat() == LatheProcessor::LearnKeeper ? "PRESS" : "KEEP"));
    if (noteUntil > 0.0 && now > noteUntil) closeNote (true);
    repaint (136, 540, 176, 92);

    if (page == 0)
    {
        auto& e = proc.getEngine();
        for (auto* k : { &tone, &lag, &mix, &volume, &rate, &depth, &warp, &ramp }) k->tick();
        record.tick (dt);
        wave.repaint();
        const float hz = e.uiHz.load();
        rateCard.setValue (String (hz, hz < 10.0f ? 2 : 1) + " HZ");
        rateCard.advance (dt);
        swingCard.setValue (String::fromUTF8 ("\xc2\xb1") + String (roundToInt (e.uiSwing.load())) + " CT");
        swingCard.advance (dt);
        const bool alt = isOn (proc, ParamIDs::div368);
        division.setLabels (alt ? StringArray { "3", "6", "8" } : StringArray { "1", "2", "4" });
        fireButton.setLit (e.uiRamping.load() || e.uiHeld.load());
        repaint (990, 84, 240, 26);
    }
    else
    {
        for (auto& c : cards) c->tick (dt);
        const int t = roundToInt (raw (proc, ParamIDs::expTarget));
        for (int i = 0; i < 10; ++i) targetChips[(size_t) i]->setLit (i == t);
        const char* const curveNames[3] = { "LINEAR", "SLOW START", "FAST START" };
        curve.setText (curveNames[jlimit (0, 2, proc.pedals().curve)]);
        calibrate.setLit (proc.isCalibrating());
        calibrate.setText (proc.isCalibrating() ? "ROCK IT" : "CALIBRATE");
        learnExp.setLit (proc.learningWhat() == LatheProcessor::LearnExpression);
        liveChip.setText (proc.keeperLive() ? "LIVE" : "STUDIO");
        learnKeep.setLit (proc.learningWhat() == LatheProcessor::LearnKeeper);
        const int ch = proc.pedals().pedalMapChannel;
        mapChip.setText (ch == 0 ? "MIDI MAP OFF" : "MIDI MAP CH " + String (ch));
        mapChip.setLit (ch != 0);
        repaint (880, 370, 180, 30);
    }
}
} // namespace ui
