#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// Several Lathe windows can be open at once. Footswitches drive one of them at a time:
// the one whose panel was last touched, or the next one along when C+D are tapped.
namespace
{
SpinLock focusLock;
Array<LatheProcessor*> instances;
std::atomic<LatheProcessor*> focused { nullptr };

// set while a message from the footswitch device is being handled on the MIDI thread
thread_local bool fromDeviceNow = false;

// footswitch device messages go to the copy that is learning, else the copy in charge
void sendToFocused (const MidiMessage& m)
{
    const SpinLock::ScopedLockType l (focusLock);
    LatheProcessor* target = nullptr;
    for (auto* p : instances) if (p->isLearning()) target = p;
    if (target == nullptr) target = focused.load();
    if (target == nullptr && ! instances.isEmpty()) target = instances.getLast();
    if (target != nullptr) target->handleDirectMidi (m);
}

double nowSeconds() { return Time::getMillisecondCounterHiRes() * 0.001; }
} // namespace

LatheProcessor::LatheProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LatheState", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            raw[rp->getParameterID()] = apvts.getRawParameterValue (rp->getParameterID());
    slotA = defaultSlot (false);
    slotB = defaultSlot (true);
    settings.load();
    settingsStamp = PedalSettings::file().getLastModificationTime();
    direct->setSink (&sendToFocused);
    direct->setWanted (settings.footDevice);
    footswitches.onEvent = [this] (int sw, FootswitchLogic::Event e) { onFootEvent (sw, e); };
    {
        const SpinLock::ScopedLockType l (focusLock);
        instances.add (this);
    }
    focused.store (this);
    startTimerHz (100);
}

LatheProcessor::~LatheProcessor()
{
    stopTimer();
    const SpinLock::ScopedLockType l (focusLock);
    instances.removeAllInstancesOf (this);
    if (focused.load() == this) focused.store (instances.isEmpty() ? nullptr : instances.getLast());
}

void LatheProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    keeper.prepare (sampleRate);
}

bool LatheProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
}

// A: a lush chorus. B: a warped record. Both are starting points to save over.
std::map<String, float> LatheProcessor::defaultSlot (bool b)
{
    using namespace ParamIDs;
    std::map<String, float> m {
        { tone, 0.5f }, { lag, 0.72f }, { mix, 0.5f }, { volume, 0.7f }, { rate, 0.3f }, { depth, 0.3f }, { warp, 0.5f }, { ramp, 0.4f },
        { wave1, 1.0f }, { wave2, 1.0f }, { division, 2.0f }, { div368, 0.0f }, { voice, 0.0f }, { bounce, 0.0f }, { sweepTop, 0.0f },
        { lofi, 0.0f }, { motoByp, 0.0f }, { tapControl, 0.0f }, { holdMode, 0.0f }, { ParamIDs::sync, 0.0f }, { tempo, 92.0f } };
    for (int t = 0; t < lathe::kTargets; ++t) { m[move[t]] = 0.0f; m[rise[t]] = t == lathe::TWarp ? 0.0f : 1.0f; }
    if (b)
    {
        m[tone] = 0.34f; m[lag] = 0.25f; m[mix] = 1.0f; m[rate] = 0.12f; m[depth] = 0.55f; m[warp] = 0.78f;
        m[wave1] = 0.0f;   // sine rise, triangle fall: the lopsided sag of an off-centre record
        m[ramp] = 0.55f; m[move[lathe::TDepth]] = 1.0f;   // and it melts in when switched on
    }
    return m;
}

lathe::EngineParams LatheProcessor::readParams()
{
    using namespace ParamIDs;
    lathe::EngineParams p;
    p.tone = f (tone); p.lag = f (lag); p.mix = f (mix); p.volume = f (volume);
    p.rate = f (rate); p.depth = f (depth); p.warp = f (warp); p.ramp = f (ramp);
    p.wave1 = idx (wave1);
    p.wave2 = names::wave2Shape (idx (wave2));
    p.active = ! flag (bypass);
    p.voice = idx (voice);
    p.lofi = flag (lofi);
    for (int t = 0; t < lathe::kTargets; ++t) { p.move[(size_t) t] = flag (move[t]); p.rise[(size_t) t] = flag (rise[t]); }
    p.bounce = flag (bounce);
    p.sweepTop = flag (sweepTop);

    const float host = hostBpm.load();
    const float bpm = (flag (hostTempo) && host > 0.0f) ? host : f (tempo);
    uiBpm.store (bpm);
    const bool tapRamp = idx (tapControl) == 1;
    if (flag (ParamIDs::sync) && ! tapRamp) p.rateHz = chase::divisionHz (bpm, idx (division), flag (div368));
    if (tapRamp) p.rampSeconds = chase::divisionRampSeconds (bpm, idx (division), flag (div368));

    p.holdMode = idx (holdMode);
    p.holdDown = flag (hold);
    p.brake = flag (brake);
    p.rampExpression = engineRampExp.load();
    p.whammy = engineWhammy.load();
    p.bass = flag (bass);
    p.bassHz = f (bassHz);
    p.output = idx (output);
    p.width = f (width);
    p.noise = f (noise);
    return p;
}

void LatheProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    for (const auto meta : midi) handleMidi (meta.getMessage(), false);
    midi.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) hostBpm.store ((float) *b);

    if (pendingFire.exchange (false)) engine.fireRamp();
    if (pendingReset.exchange (false)) engine.resetRamp();

    const auto p = readParams();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    keeper.pushInput (L, R, n);
    engine.process (L, R, L, R, n, p);
    keeper.pushOutput (L, R, n);
}

// =====================================================================================
// MIDI: footswitches, expression, Keeper, and the original pedal's CC chart on its own
// channel (off by default, so its CC 11 and CC 20 to 23 can't collide with the defaults).
// =====================================================================================
void LatheProcessor::queue (int kind, int a)
{
    if (fromDeviceNow)
    {
        const auto scope = directFifo.write (1);
        if (scope.blockSize1 > 0) directPending[(size_t) scope.startIndex1] = { kind, a };
        return;
    }
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0) pending[(size_t) scope.startIndex1] = { kind, a };
}

void LatheProcessor::handleDirectMidi (const MidiMessage& m)
{
    const SpinLock::ScopedLockType l (directWriteLock);
    fromDeviceNow = true;
    handleMidi (m, true);
    fromDeviceNow = false;
}

void LatheProcessor::setFootDevice (const String& device)
{
    settings.footDevice = device;
    settings.save();
    settingsStamp = PedalSettings::file().getLastModificationTime();
    direct->setWanted (device);
}

bool LatheProcessor::handlePedalMap (const MidiMessage& m)
{
    const int mapCh = settings.pedalMapChannel;
    if (mapCh <= 0 || m.getChannel() != mapCh) return false;
    if (m.isProgramChange()) { queue (MapProgram, m.getProgramChangeNumber()); return true; }
    if (m.isController()) { queue (MapCC, m.getControllerNumber() * 128 + m.getControllerValue()); return true; }
    return false;
}

void LatheProcessor::handleMidi (const MidiMessage& m, bool fromDevice)
{
    if (m.isProgramChange() && ! fromDevice)
    {
        if (! handlePedalMap (m)) queue (MapProgram, m.getProgramChangeNumber());
        return;
    }
    if (! m.isController()) return;
    const int ch = m.getChannel(), cc = m.getControllerNumber(), val = m.getControllerValue();

    if (learnTarget.load() != LearnNone)
    {
        if (val > 0) queue (Learned, ch * 128 + cc);
        return;
    }

    // Straight from the footswitch device, any channel will do: it's the footswitch's own
    // cable, and some footswitches ignore their channel setting. Through Ableton, the
    // channel must match. While the device is read directly, the same messages arriving
    // through Ableton are ignored, so nothing happens twice.
    if (! fromDevice && pedalsDirect() && settings.pedalMapChannel <= 0) return;

    int foot = -1;
    for (int k = 0; k < 4 && foot < 0; ++k)
        if ((fromDevice || ch == settings.fsChannel[(size_t) k]) && cc == settings.fsCC[(size_t) k]) foot = k;
    const bool keep = foot < 0 && settings.keeperCC >= 0 && (fromDevice || ch == settings.keeperChannel) && cc == settings.keeperCC;
    const bool expression = foot < 0 && ! keep && (fromDevice || ch == settings.expChannel) && cc == settings.expCC;

    // the original chart answers only on its own channel, and never steals the footswitch's own messages
    if (! (fromDevice && (foot >= 0 || keep || expression)) && handlePedalMap (m)) return;
    if (! fromDevice && pedalsDirect()) return;
    if (! (foot >= 0 || keep || expression)) return;   // anything else, sustain pedal included, is ignored

    if (! fromDevice)
    {
        // The copy Ableton sends footswitch MIDI to takes charge, unless the copy in
        // charge is fed too. A newly loaded copy with no MIDI track can't hold the pedals.
        hostFeedsPedals.store (true);
        const SpinLock::ScopedLockType l (focusLock);
        auto* fc = focused.load();
        if (fc != this && (fc == nullptr || ! fc->hostFeedsPedals.load())) focused.store (this);
        if (focused.load() != this) return;
    }
    if (foot >= 0) queue (val >= 64 ? FootPress : FootRelease, foot);
    else if (keep) { if (val >= 64) queue (KeepCC, 0); }
    else queue (Expression, val);
}

void LatheProcessor::setNorm (const String& id, float v01)
{
    if (auto* p = apvts.getParameter (id))
    {
        v01 = jlimit (0.0f, 1.0f, v01);
        if (std::abs (p->getValue() - v01) < 1.0e-5f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v01);
        p->endChangeGesture();
    }
}

void LatheProcessor::setPlain (const String& id, float plain)
{
    if (auto* p = apvts.getParameter (id)) setNorm (id, p->convertTo0to1 (plain));
}

void LatheProcessor::say (const String& s)
{
    uiMessage = s;
    uiMessageAt = nowSeconds();
}

void LatheProcessor::timerCallback() { pump (nowSeconds()); }

void LatheProcessor::pump (double now)
{
    lastNow = now;
    // settings changed by another copy, or another Flip Fella plugin: pick them up
    if (now >= nextSettingsCheck)
    {
        nextSettingsCheck = now + 1.0;
        const auto stamp = PedalSettings::file().getLastModificationTime();
        if (stamp != settingsStamp && learnTarget.load() == LearnNone && ! calibrating)
        {
            settingsStamp = stamp;
            settings.load();
            direct->setWanted (settings.footDevice);
        }
        const bool d = pedalsDirect();
        if (d != wasDirect)
        {
            wasDirect = d;
            if (d && focused.load() == this) say ("FOOTSWITCHES READY");
        }
    }

    auto apply = [&] (const std::array<Pending, 512>& from, int start, int size)
    {
        for (int i = start; i < start + size; ++i)
        {
            const auto& q = from[(size_t) i];
            switch (q.kind)
            {
                case FootPress: footswitches.press (q.a, now); break;
                case FootRelease: footswitches.release (q.a, now); break;
                case KeepCC: keepNow(); break;
                case Expression:
                {
                    expMovedAt = now;
                    if (calibrating)
                    {
                        if (calFirst < 0) calFirst = q.a;
                        calLo = jmin (calLo, q.a);
                        calHi = jmax (calHi, q.a);
                        uiExp.store ((float) q.a / 127.0f);
                        break;
                    }
                    expTargetValue = settings.expressionValue (q.a);
                    if (expSmoothed < 0.0f) expSmoothed = expTargetValue;
                    break;
                }
                case MapProgram:
                    // as on the pedal: 0 is Live, 1 the right slot (B), 2 the left slot (A)
                    if (q.a == 0) selectSlot (1);
                    else if (q.a == 1) selectSlot (2);
                    else if (q.a == 2) selectSlot (0);
                    break;
                case MapCC:
                {
                    const int cc = q.a / 128, v = q.a % 128;
                    const float x = (float) v / 127.0f;
                    switch (cc)
                    {
                        case 14: setNorm (ParamIDs::tone, x); break;
                        case 15: setNorm (ParamIDs::volume, x); break;
                        case 16: setNorm (ParamIDs::mix, x); break;
                        case 17: setNorm (ParamIDs::rate, x); setPlain (ParamIDs::sync, 0.0f); break;
                        case 18: setNorm (ParamIDs::depth, x); break;
                        case 19: setNorm (ParamIDs::warp, x); break;
                        case 20: setNorm (ParamIDs::ramp, x); break;
                        case 22: setNorm (ParamIDs::lag, x); break;   // the HiFi's Lag, not on the mkII chart
                        case 21:
                        {
                            // 0 whole, 1 half, 2 dotted eighth (Lathe's 3), 3 quarter, 4 eighth (6), 5 sixteenth (8)
                            static const int div[6] = { 0, 1, 0, 2, 1, 2 };
                            static const bool alt[6] = { false, false, true, false, true, true };
                            const int k = jlimit (0, 5, v);
                            setPlain (ParamIDs::division, (float) div[k]);
                            setPlain (ParamIDs::div368, alt[k] ? 1.0f : 0.0f);
                            break;
                        }
                        case 93: tapTempo (now); break;
                        case 100: expMovedAt = now; expTargetValue = x; if (expSmoothed < 0.0f) expSmoothed = x; break;
                        case 102:
                            if (v >= 64) { if (flag (ParamIDs::bypass)) setPlain (ParamIDs::bypass, 0.0f); else fireRamp(); }
                            else setPlain (ParamIDs::bypass, 1.0f);
                            break;
                        default: break;
                    }
                    break;
                }
                case Learned:
                {
                    const int ch = q.a / 128, cc = q.a % 128;
                    const int what = learnTarget.exchange (LearnNone);
                    if (what >= 0 && what < 4)
                    {
                        settings.fsChannel[(size_t) what] = ch;
                        settings.fsCC[(size_t) what] = cc;
                        say ("FOOTSWITCH " + String::charToString ((juce_wchar) ('A' + what)) + " IS CC " + String (cc));
                    }
                    else if (what == LearnKeeper)
                    {
                        settings.keeperChannel = ch;
                        settings.keeperCC = cc;
                        say ("KEEPER IS CC " + String (cc));
                    }
                    else if (what == LearnExpression)
                    {
                        settings.expChannel = ch;
                        settings.expCC = cc;
                        say ("EXPRESSION IS CC " + String (cc));
                    }
                    settings.save();
                    settingsStamp = PedalSettings::file().getLastModificationTime();
                    break;
                }
                default: break;
            }
        }
    };
    {
        const auto scope = fifo.read (fifo.getNumReady());
        apply (pending, scope.startIndex1, scope.blockSize1);
        apply (pending, scope.startIndex2, scope.blockSize2);
    }
    {
        const auto scope = directFifo.read (directFifo.getNumReady());
        apply (directPending, scope.startIndex1, scope.blockSize1);
        apply (directPending, scope.startIndex2, scope.blockSize2);
    }

    // a remote controller pressing the footswitches through their parameters
    for (int k = 0; k < 4; ++k)
    {
        const bool down = flag (ParamIDs::foot[k]);
        if (down != remoteFoot[(size_t) k])
        {
            remoteFoot[(size_t) k] = down;
            if (down) footswitches.press (k, now);
            else footswitches.release (k, now);
        }
    }
    if (flag (ParamIDs::keeper)) { setPlain (ParamIDs::keeper, 0.0f); keepNow(); }
    if (flag (ParamIDs::fireRamp)) { setPlain (ParamIDs::fireRamp, 0.0f); fireRamp(); }

    // the preset toggle, moved from the panel, automation or a footswitch: glide to it
    const int slot = idx (ParamIDs::preset);
    if (slot != lastSlotParam)
    {
        lastSlotParam = slot;
        if (! suppressSlotGlide && slot != 1) startGlide (slot == 0 ? slotA : slotB);
        suppressSlotGlide = false;
    }
    if (glideStart >= 0.0)
    {
        const float t = (float) jlimit (0.0, 1.0, (now - glideStart) / 0.3);
        const float e = t * t * (3.0f - 2.0f * t);
        for (const auto& [id, to] : glideTo)
            if (isContinuous (id)) setPlain (id, glideFrom[id] + (to - glideFrom[id]) * e);
        if (t >= 1.0f) glideStart = -1.0;
    }

    // expression, smoothed
    if (expTargetValue >= 0.0f)
    {
        const float alpha = jmap (f (ParamIDs::expSmooth), 1.0f, 0.04f);
        expSmoothed += (expTargetValue - expSmoothed) * alpha;
        const bool settled = std::abs (expTargetValue - expSmoothed) < 0.0005f;
        if (settled) expSmoothed = expTargetValue;
        applyExpression (expSmoothed);
        if (settled) expTargetValue = -1.0f;   // settled: let the knob be moved by hand
    }
    // leaving Ramp targets or Whammy hands the knobs back
    const int target = idx (ParamIDs::expTarget);
    if (target != names::ExpRampTargets) engineRampExp.store (-1.0f);
    if (target != names::ExpWhammy) engineWhammy.store (-1.0f);

    footswitches.tick (now);
    int mask = 0;
    for (int k = 0; k < 4; ++k) if (footswitches.isDown (k)) mask |= 1 << k;
    uiFootDown.store (mask);

    if (calibrating && now > calibrateUntil)
    {
        calibrating = false;
        if (calHi - calLo > 20)
        {
            // whichever end the pedal started at is the heel
            const bool startedLow = std::abs (calFirst - calLo) < std::abs (calFirst - calHi);
            settings.heel = startedLow ? calLo : calHi;
            settings.toe = startedLow ? calHi : calLo;
            settings.save();
            settingsStamp = PedalSettings::file().getLastModificationTime();
            say ("EXPRESSION CALIBRATED");
        }
        else
            say ("NO MOVEMENT, TRY AGAIN");
    }
}

// Heel to toe. Morph glides every knob from preset A (heel) to preset B (toe) and flips the
// switches half way, which is the original's expression mode generalised to the whole pedal.
void LatheProcessor::applyExpression (float v)
{
    uiExp.store (v);
    const float x = jmap (v, f (ParamIDs::expMin), f (ParamIDs::expMax));
    const auto& targets = names::expTargets();
    const int t = jlimit (0, (int) targets.size() - 1, idx (ParamIDs::expTarget));
    if (t == names::ExpMorph)
    {
        if (glideStart >= 0.0) return;
        for (const auto& id : presetParamIds())
        {
            const float a = slotA[id], b = slotB[id];
            setPlain (id, isContinuous (id) ? a + (b - a) * x : (x < 0.5f ? a : b));
        }
    }
    else if (t == names::ExpRampTargets) engineRampExp.store (x);
    else if (t == names::ExpWhammy) engineWhammy.store (x);
    else
    {
        if (String (targets[(size_t) t].id) == ParamIDs::rate) setPlain (ParamIDs::sync, 0.0f);
        setNorm (targets[(size_t) t].id, x);
    }
}

// =====================================================================================
// Presets: A, Live and B, as the pedal's preset toggle
// =====================================================================================
std::map<String, float> LatheProcessor::capture() const
{
    std::map<String, float> m;
    for (const auto& id : presetParamIds()) m[id] = raw.at (id)->load();
    return m;
}

void LatheProcessor::startGlide (const std::map<String, float>& to)
{
    glideFrom = capture();
    glideTo = to;
    glideStart = lastNow;
    for (const auto& [id, v] : to)
        if (! isContinuous (id)) setPlain (id, v);   // switches change at once
}

void LatheProcessor::selectSlot (int slot)
{
    slot = jlimit (0, 2, slot);
    if (slot == idx (ParamIDs::preset) && slot != 1) { lastSlotParam = -1; }   // recall again: glide back to it
    setPlain (ParamIDs::preset, (float) slot);
    say (slot == 0 ? "PRESET A" : (slot == 2 ? "PRESET B" : "LIVE"));
}

void LatheProcessor::saveSlot (int slot)
{
    (slot == 2 ? slotB : slotA) = capture();
    suppressSlotGlide = idx (ParamIDs::preset) != slot;
    setPlain (ParamIDs::preset, (float) slot);
    say (slot == 2 ? "SAVED TO B" : "SAVED TO A");
}

bool LatheProcessor::slotEdited() const
{
    const int slot = (int) std::lround (raw.at (ParamIDs::preset)->load());
    if (slot == 1 || glideStart >= 0.0) return false;
    const auto& s = slot == 0 ? slotA : slotB;
    for (const auto& id : presetParamIds())
    {
        auto* p = apvts.getParameter (id);
        const auto it = s.find (id);
        if (p == nullptr || it == s.end()) continue;
        if (std::abs (p->convertTo0to1 (raw.at (id)->load()) - p->convertTo0to1 (it->second)) > 0.01f) return true;
    }
    return false;
}

void LatheProcessor::tapTempo (double now)
{
    const double interval = now - lastTapDown;
    lastTapDown = now;
    uiTapCount.fetch_add (1);
    if (interval < 0.15 || interval > 3.0) { say ("TAP AGAIN"); return; }
    tapInterval = interval;
    const float bpm = jlimit (30.0f, 300.0f, (float) (60.0 / interval));
    setPlain (ParamIDs::tempo, bpm);
    setPlain (ParamIDs::hostTempo, 0.0f);   // a tapped tempo wins over Ableton's until Sync to host is chosen again
    if (idx (ParamIDs::tapControl) == 0) setPlain (ParamIDs::sync, 1.0f);
    say ("TEMPO " + String (roundToInt (bpm)));
}

// =====================================================================================
// Footswitches, based on the pedal's two (Bypass and Tap/Hold).
// A bypass (hold for momentary), B tap tempo (hold to brake), C fires the ramp (hold for the
// Hold mode: ramp, pause or flurry), D flips between presets A and B (hold to save).
// A+B resets the ramp, C+D passes the footswitches on, holding C+D for a second keeps.
// =====================================================================================
void LatheProcessor::onFootEvent (int sw, FootswitchLogic::Event e)
{
    using E = FootswitchLogic;
    const double now = lastNow;
    if (e == E::Down || e == E::ComboAB || e == E::ComboCD) uiFootPulse.fetch_add (1);
    const bool momentaryOnly = flag (ParamIDs::motoByp);

    switch (e)
    {
        case E::ComboHoldCD:
            focused.store (this);   // Keeper on the plugin you were playing
            keepNow();
            return;
        case E::ComboCD:
        {
            const SpinLock::ScopedLockType l (focusLock);
            const int i = instances.indexOf (this);
            auto* next = instances.isEmpty() ? nullptr : instances[(i + 1) % instances.size()];
            focused.store (next);
            if (next != nullptr) next->say ("FOOTSWITCHES HERE");
            return;
        }
        case E::ComboAB:
            if (aDownToggled)   // A was half of the pair: undo its press
            {
                if (momentaryOnly) setPlain (ParamIDs::bypass, 1.0f); else toggle (ParamIDs::bypass);
                aDownToggled = false;
            }
            resetRamp();
            return;
        case E::ComboHoldAB:
            return;
        default: break;
    }

    if (sw == 0)   // A: bypass. Acts on the press, so it's instant; held, it's momentary
    {
        if (e == E::Down)
        {
            if (momentaryOnly) setPlain (ParamIDs::bypass, 0.0f); else toggle (ParamIDs::bypass);
            aDownToggled = true;
            say (flag (ParamIDs::bypass) ? "BYPASSED" : "ON");
        }
        else if (e == E::Tap) { if (momentaryOnly) setPlain (ParamIDs::bypass, 1.0f); aDownToggled = false; }
        else if (e == E::HoldEnd)
        {
            if (momentaryOnly) setPlain (ParamIDs::bypass, 1.0f); else toggle (ParamIDs::bypass);
            aDownToggled = false;
        }
    }
    else if (sw == 1)   // B: tap tempo (from the press times), hold to brake
    {
        if (e == E::Down) bDownAt = now;
        else if (e == E::Tap) tapTempo (bDownAt);
        else if (e == E::HoldStart) { setPlain (ParamIDs::brake, 1.0f); say ("BRAKE"); }
        else if (e == E::HoldEnd) { setPlain (ParamIDs::brake, 0.0f); say ("SPINNING UP"); }
    }
    else if (sw == 2)   // C: fire the ramp; hold for ramp, pause or flurry
    {
        if (e == E::Tap) fireRamp();
        else if (e == E::HoldStart)
        {
            setPlain (ParamIDs::hold, 1.0f);
            const int m = idx (ParamIDs::holdMode);
            say (m == lathe::HoldPause ? "PAUSE" : (m == lathe::HoldFlurry ? "FLURRY" : "RAMP WHILE HELD"));
        }
        else if (e == E::HoldEnd) setPlain (ParamIDs::hold, 0.0f);
    }
    else if (sw == 3)   // D: presets
    {
        if (e == E::Tap) selectSlot (idx (ParamIDs::preset) == 0 ? 2 : 0);
        else if (e == E::HoldStart)
        {
            const int cur = idx (ParamIDs::preset);
            saveSlot (cur == 1 ? 0 : cur);
        }
    }
}

void LatheProcessor::footDown (int sw)
{
    focusHere();
    const double now = nowSeconds();
    lastNow = now;
    footswitches.press (sw, now);
}

void LatheProcessor::footUp (int sw)
{
    const double now = nowSeconds();
    lastNow = now;
    footswitches.release (sw, now);
}

void LatheProcessor::focusHere() { focused.store (this); }

// =====================================================================================
// Footswitch learn and expression calibration
// =====================================================================================
void LatheProcessor::learnFootswitch (int sw)
{
    learnTarget.store (sw);
    say ("PRESS FOOTSWITCH " + String::charToString ((juce_wchar) ('A' + sw)));
}

void LatheProcessor::learnExpression()
{
    learnTarget.store (LearnExpression);
    say ("MOVE THE EXPRESSION PEDAL");
}

void LatheProcessor::calibrateExpression()
{
    calibrating = true;
    calibrateUntil = nowSeconds() + 6.0;
    calLo = 127; calHi = 0; calFirst = -1;
    say ("HEEL DOWN, THEN ROCK TO TOE");
}

void LatheProcessor::resetFootswitches()
{
    settings.resetFootswitches();
    settings.save();
    settingsStamp = PedalSettings::file().getLastModificationTime();
    say ("FOOTSWITCH DEFAULTS RESTORED");
}

void LatheProcessor::setCurve (int c)
{
    settings.curve = jlimit (0, 2, c);
    settings.save();
    settingsStamp = PedalSettings::file().getLastModificationTime();
}

void LatheProcessor::setPedalMapChannel (int ch)
{
    settings.pedalMapChannel = jlimit (0, 16, ch);
    settings.save();
    settingsStamp = PedalSettings::file().getLastModificationTime();
    say (ch == 0 ? "ORIGINAL MIDI MAP OFF" : "ORIGINAL MIDI MAP ON CH " + String (ch));
}

// =====================================================================================
// Keeper
// =====================================================================================
void LatheProcessor::keepNow()
{
    DynamicObject::Ptr e = new DynamicObject();
    e->setProperty ("plugin", "Lathe");
   #ifdef JucePlugin_VersionString
    e->setProperty ("version", JucePlugin_VersionString);
   #endif
    e->setProperty ("recreates", "Chase Bliss Audio Warped Vinyl HiFi (with the mkII as a voice)");
    e->setProperty ("pedalVersion", idx (ParamIDs::voice) == lathe::MkII ? "mkII" : "HiFi");
    e->setProperty ("time", Time::getCurrentTime().toISO8601 (true));
    e->setProperty ("context", settings.keeperLive ? "Live" : "Studio");
    const int slot = idx (ParamIDs::preset);
    e->setProperty ("preset", slot == 0 ? "A" : (slot == 2 ? "B" : "Live"));
    e->setProperty ("presetEdited", slotEdited());
    e->setProperty ("tempo", uiBpm.load());
    e->setProperty ("tempoFromHost", flag (ParamIDs::hostTempo) && hostBpm.load() > 0.0f);

    // the moment, as you'd hear it
    DynamicObject::Ptr heard = new DynamicObject();
    const char* targetNames[lathe::kTargets] = { "volume", "lag", "mix", "rate", "depth", "warp", "tone" };
    for (int t = 0; t < lathe::kTargets; ++t) heard->setProperty (targetNames[t], engine.uiEff[(size_t) t].load());
    heard->setProperty ("rateHz", engine.uiHz.load());
    heard->setProperty ("pitchSwingCents", engine.uiSwing.load());
    heard->setProperty ("ramping", engine.uiRamping.load());
    heard->setProperty ("braking", engine.uiBrake.load() > 0.05f);
    e->setProperty ("heard", var (heard.get()));

    DynamicObject::Ptr exp = new DynamicObject();
    const auto& targets = names::expTargets();
    exp->setProperty ("target", targets[(size_t) jlimit (0, (int) targets.size() - 1, idx (ParamIDs::expTarget))].label);
    exp->setProperty ("position", uiExp.load());
    exp->setProperty ("movedSecondsAgo", nowSeconds() - expMovedAt);
    e->setProperty ("expression", var (exp.get()));

    // every parameter, hidden ones included, as value and as shown on the panel
    DynamicObject::Ptr params = new DynamicObject();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
        {
            if (rp->getParameterID() == ParamIDs::keeper) continue;
            DynamicObject::Ptr pv = new DynamicObject();
            pv->setProperty ("value", rp->convertFrom0to1 (rp->getValue()));
            pv->setProperty ("shown", rp->getCurrentValueAsText());
            params->setProperty (rp->getParameterID(), var (pv.get()));
        }
    e->setProperty ("parameters", var (params.get()));

    keeper.keep (e);
    uiKeepPulse.fetch_add (1);
    say ("KEPT");
}

File LatheProcessor::exportKeepers()
{
    const auto file = keeper.exportZip();
    if (file.existsAsFile()) { say ("EXPORTED TO DOCUMENTS"); file.revealToUser(); }
    else say ("NOTHING KEPT YET");
    return file;
}

void LatheProcessor::openKeepersFolder()
{
    Keeper::folder().createDirectory();
    Keeper::folder().startAsProcess();
}

void LatheProcessor::learnKeeper()
{
    learnTarget.store (LearnKeeper);
    say ("PRESS YOUR KEEPER SWITCH");
}

void LatheProcessor::setKeeperLive (bool live)
{
    settings.keeperLive = live;
    settings.save();
    settingsStamp = PedalSettings::file().getLastModificationTime();
}

// =====================================================================================
// State: the parameters plus the two preset slots
// =====================================================================================
void LatheProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    ValueTree slots ("Slots");
    for (int s = 0; s < 2; ++s)
    {
        ValueTree t (s == 0 ? "A" : "B");
        for (const auto& [id, v] : (s == 0 ? slotA : slotB)) t.setProperty (Identifier (id), v, nullptr);
        slots.appendChild (t, nullptr);
    }
    state.removeChild (state.getChildWithName ("Slots"), nullptr);
    state.appendChild (slots, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void LatheProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            const auto slots = state.getChildWithName ("Slots");
            for (int s = 0; s < 2; ++s)
            {
                const auto t = slots.getChildWithName (s == 0 ? "A" : "B");
                auto& m = s == 0 ? slotA : slotB;
                for (const auto& id : presetParamIds())
                    if (t.hasProperty (Identifier (id))) m[id] = (float) t.getProperty (Identifier (id));
            }
            apvts.replaceState (state);
        }
    lastSlotParam = idx (ParamIDs::preset);   // loading a set doesn't glide
    // nothing held down survives a reload
    for (auto* id : { ParamIDs::foot[0], ParamIDs::foot[1], ParamIDs::foot[2], ParamIDs::foot[3],
                      ParamIDs::keeper, ParamIDs::fireRamp, ParamIDs::brake, ParamIDs::hold })
        setPlain (id, 0.0f);
}

AudioProcessorEditor* LatheProcessor::createEditor() { return new LatheEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LatheProcessor(); }
