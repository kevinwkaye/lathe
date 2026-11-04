# Lathe: research and feature map

Written 4 November 2026. Lathe recreates the behaviour of a digitally controlled analog chorus and vibrato pedal (the Chase Bliss Audio Warped Vinyl) in the Flip Fella dark family look. All DSP will be original code written from public descriptions of the pedal. The plugin UI names no brands.

Status: research done, questions answered (see Decisions), mockup next. Updated the same day from the mkII manual Kevin supplied, which corrected several details below.

## Versions

| Version | Released | What changed |
| --- | --- | --- |
| Original (MKI) | about 2014 | Six knobs: Tone, Volume, Mix, RPM, Depth, Warp. Two wave-shape toggles (first and second half of the wave) and a tap division toggle. Tap and Bypass footswitches. Ramping and expression assigned by dip switches on the underside. Bypass could be made momentary by a dip switch |
| mkII | 2015 | Better signal to noise, a wider vibrato range, Tone now shapes the vibrato (clockwise shimmery, anticlockwise murky) and doubles as the **Ramp** time knob whenever a ramp dip switch is on. A **preset toggle** (left slot, Live, right slot). 16 dip switches on the side, including **Lo-Fi** (longer vibrato delay, noisier and gooier). JFET boost with amp-like grit when pushed. MIDI over the tap jack (channel 5): a CC per knob, 122 presets by Program Change, MIDI clock. Remembered as the warm, murky one |
| HiFi | late 2017 | Combines the mkII's features with a redesigned tone circuit (noon is transparent, clockwise thins the lows, anticlockwise darkens), a new **Lag** control for lush chorus, brighter and clearer circuitry, lower noise and more headroom. Lag takes the Volume knob's place on the panel and in the dip banks. New **Hold** mode, with the toggle repurposed as R, minus and plus. MIDI on channel 6, with presets, MIDI clock and a CC per control. Named HiFi rather than mk3 because the changes were large enough |
| After HiFi | | Discontinued, alongside Thermae, Dark World, Preamp MKII and Condor HiFi (Chase Bliss cited new EU regulations). No successor has been announced |

**Recommendation: recreate the HiFi, keep Volume, and add a mkII voice switch.**

- You asked for lush and hi-fi. Lag is the control that makes the HiFi's chorus lush, it only exists on the HiFi, and the HiFi is the cleaner, quieter, higher-headroom circuit.
- The HiFi has the deepest hidden layer: Hold mode (ramp, pause or flurry while held), Tap Control, Division routing and Sweep, on top of the mkII's ramp, bounce and rise or fall switches.
- Volume comes back as a seventh knob. A plugin needs an output level anyway, and on bass you'll want to match levels.
- A **HiFi / mkII** voice switch (as OilSlick has V2 and First edition) gives the warmer, murkier, slightly noisier mkII at any time. Nobody publishes measured differences, so it's tuned by ear.

## Controls

"Sources" is what the manuals, retailer copies of the manuals and reviews say. Where they disagree or stop short, the plugin column says what Lathe will assume, and the question list asks you.

| Control | What the pedal does | How Lathe does it |
| --- | --- | --- |
| **Tone** | HiFi: noon is transparent, clockwise shaves off lows, anticlockwise shaves off highs. mkII: wet side only (never the dry), clockwise transparent and shimmery, anticlockwise dark and murky. On the mkII it becomes the Ramp time knob when any ramp dip is on | A tilt filter on the wet path: noon flat, clockwise a gentle low cut (up to about 250 Hz) plus air, anticlockwise a gentle high cut (down to about 1.5 kHz). In mkII voice it follows the mkII's darker law |
| **Volume** | Original and mkII: level of the effect, with a JFET boost that gives some amp-like grit when pushed. Can be ramped or put on expression, so it doubles as tremolo or a volume pedal | Output level, −inf to +12 dB, unity at about 2 o'clock, with a gentle soft clip above unity for the JFET grit. Lives on the panel and can be ramped (tremolo) |
| **Lag** (HiFi) | Delay between dry and wet. More lag means more interference across the spectrum and a lusher chorus. Adds a little noise as it rises | The base delay of the modulated line, about 1 ms to 25 ms on an exponential taper. Clean in Lathe; the noise is a separate Wear control so lush doesn't have to mean hissy |
| **Mix** | Dry against wet. Fully wet is pure vibrato, around noon is chorus | Equal-power blend. Fully clockwise is all wet (vibrato), noon is a classic chorus |
| **RPM** (Rate) | Vibrato rate. Overridden by tap tempo | 0.1 Hz to 12 Hz, exponential, with a fine readout in Hz and, when synced, the note value. Tap or host sync overrides it until you touch the knob, as on the pedal |
| **Depth** | Vibrato width. Fully up is "insane, pitch-bending" modulation | Delay sweep in milliseconds, scaled by rate so the pitch swing stays musical at any speed, reaching about ±1 semitone at full clockwise (more than enough for warped records) |
| **Warp** | Moves the centre point of the wave. Noon is a perfectly symmetric wave; either side skews the rise and fall toward the start or end of the cycle | Phase skew: the wave's peak moves from 5% to 95% of the cycle, noon is 50%. Gives the lopsided seasick lurch people love |
| **Left wave toggle** | Shape of the first half of the wave: sine, triangle or square | Same three shapes for the first half. Square is softened by about 3 ms so it never clicks |
| **Right wave toggle** | Shape of the second half: square, triangle or sine (in that order, mirrored) | Same, mirrored order kept so the panel reads like the pedal |
| **Tap division toggle** | 1, 2, 4 (whole, half, quarter note per wave cycle). The Division dip switch gives 3, 6, 8 instead (mkII: dotted eighth, eighth, sixteenth; HiFi reading: triplets). Divisions are twice as slow for ramping. On the HiFi, in Hold mode the toggle becomes R, minus and plus | The same toggle. 1, 2, 4 normally, 3, 6, 8 with the 3 6 8 option, read as triplets (Kevin's call, see Decisions). In Hold mode it picks what footswitch C's hold does (see Hold below) |
| **Tap footswitch** | Tap tempo, always honours the last two stomps. HiFi Hold mode: hold it to ramp, pause or flurry | Split between B (tap) and C (hold) so neither job waits on the other. See footswitches |
| **Bypass footswitch** | Relay true bypass. The MoToByp (momentary-to-bypass) dip makes it momentary. Ramping always resets on bypass, and starts again on engage | Footswitch A, tap to latch, hold for momentary. Relay-style click-free crossfade |
| **EXP / CV jack** | Plugging in switches the left dip bank from ramping to expression. The pedal sweeps the selected knobs from the knob to max or min (Sweep); the knob sets the range per control. With no knobs selected, expression moves the waveform directly: an analog whammy, its range set by Depth | **Morph** between preset A and B (default), **Sweep** one control, the original's **Ramp targets**, or **Whammy** (direct waveform bend). See Expression |
| **Preset toggle** | Left slot, Live (the panel as it is), right slot. Save by holding one stomp 3 s, then both for 3 s. The LED dims if you move a knob after recalling. 122 more slots over MIDI | An A / LIVE / B toggle as in Murmur. Footswitch D flips between A and B, hold D saves. The slot dims when edited. Program change 0 Live, 1 A, 2 B. Saved with Ableton's set and morphable by expression |
| **MIDI** | mkII (channel 5): Tone 14, Volume 15, Mix 16, RPM 17, Depth 18, Warp 19, Ramp 20, Divisions 21, Tap 93, Expression 100, Bypass 102 (127 while on resets the ramp). HiFi (channel 6) adds Lag and MIDI clock on CC 51 | Every control is a host parameter. The mkII chart (with Lag added on CC 22) is available behind an **Original pedal MIDI map** channel setting, off by default, so it can't collide with the footswitch and expression defaults (CC 20 to 23, CC 11). Host tempo replaces MIDI clock |

## The dip switches

The pedal has two banks of eight. The left bank picks which knobs move (by ramp, or by expression when one is plugged in) plus three behaviours; the right bank picks which way each knob goes plus three more. Ramping always resets on bypass, and the knob position is where each control starts or stops. From the mkII manual, with the HiFi's changes:

| Left bank (mkII) | Right bank (mkII) | HiFi changes |
| --- | --- | --- |
| Volume: ramp or expression moves it | Volume: rise (clockwise) or fall | Lag replaces Volume in both banks |
| Mix | Mix: rise or fall | |
| RPM | RPM: rise or fall | |
| Depth | Depth: rise or fall | |
| Warp | Warp: rise or fall | |
| Bounce: back and forth (modulate) instead of ramp and hold | Tap Control: R, tapping sets the ramp rate; P, it sets RPM | |
| Lo-Fi: longer vibrato delay, noisier and gooier | Tap Division: 3 gives the 3, 6, 8 divisions | Hold replaces Lo-Fi: holding tap ramps, the toggle picks R, minus, plus |
| MoToByp: momentary-to-bypass, on only while the stomp is held | Sweep: T, between the knob and max; B, between the knob and min | |

How Lathe does it: a **Ramp** page laid out like the two banks, using the same flip switches as Murmur's Motion page.

- **Which knobs move.** One switch each for Volume, Lag, Mix, Rate, Depth and Warp (both pedals' sets), plus **Tone** (neither pedal can ramp it, the plugin can at no cost). All off by default, as on the pedal.
- **Rise or fall** for each knob.
- **Ramp or bounce.** Ramp runs once when the effect switches on (or when footswitch C or the RAMP button fires it), then holds. Bounce keeps going back and forth with Murmur's five waveforms (triangle, square, sine, random, smooth random).
- **Ramp speed** gets its own small RAMP knob, 0.1 s to 30 s per sweep, or follows the tap with **Tap control: R**. The mkII borrows the Tone knob for this; a plugin can give Tone and Ramp a knob each.
- **Lo-Fi** (mkII): longer vibrato delay, more hiss and goo, as its own switch in either voice.
- **Division to ramp**: tap divisions set the ramp length instead of the rate, twice as slow, as on the pedal.
- **Sweep top or bottom**, used by both ramp and the single-control expression mode.
- **Hold mode**: footswitch C's hold job follows the toggle. R ramps the chosen knobs while held and they glide back on release. Minus pauses the wobble where it is (the record stops). Plus is a flurry, rate jumps up about four times while held. Default R.
- **MoToByp**: footswitch A's hold is always momentary. The switch is kept for the full momentary behaviour (on only while held, taps do nothing).
- **Ramp resets on bypass** and runs again on engage, as on the pedal.
- **3 6 8 divisions** is a switch on the same page.
- **Ramp reset** (the pedal's CC 102) is A+B.

## The character to keep

From reviews, demos and the maker's text: analog BBD warmth with a digital brain, a vibrato that goes from a gentle seasick drift to a dying turntable, chorus that blooms with Lag, and hiss that people learn to live with (Chase Bliss's own FAQ: keep Lag and Tone at noon or below to reduce it). Settings people return to:

- **Warped record**: Mix full, Depth around 10 o'clock, RPM slow (about 0.5 Hz), Warp off-centre, left sine, right triangle. The lopsided pitch sag of an off-centre record.
- **Lush chorus**: Mix noon, Lag 2 to 3 o'clock, Depth 9 to 10 o'clock, RPM about 1 Hz, both triangle.
- **Lo-fi tape**: Depth low, Lag low, Tone anticlockwise, mkII voice, Wear up. The cassette warble the guides describe.
- **Leslie-ish**: Mix noon, RPM fast, Depth low, square halves softened.
- **Ramp into vibrato**: Depth and RPM ramping up from minimum over a few seconds as you switch on, so a chord slowly melts.

How Lathe gets there while staying hi-fi:

- **Delay line**: 4x oversampled modulated delay with 4-point Hermite interpolation (or polyphase in the lush preset), so deep vibrato and slow sweeps stay free of zipper noise and aliasing. The "hi-fi" part.
- **True to the pedal's noise** (Kevin's call): hiss in the wet path that rises with Lag and with Tone past noon, as the maker's FAQ describes, lower in HiFi voice, higher in mkII voice, highest with Lo-Fi. A gentle anti-alias roll-off that follows the delay time like a BBD clock, light compander breathing, and soft saturation. A **Noise** trim on the Deep page goes from 0 to twice the pedal's, at 1x by default.
- **Wow and flutter**: a slow random drift on the rate (Warp's skew plus a small irregularity) so it never sounds like a metronome, because real platters don't.
- **Stereo, MISO by default** (mono in, stereo out, Kevin's call): the left and right wet signals take opposite halves of the modulation, so chorus spreads wide and vibrato moves across the field. A Width control on the Deep page goes from mono (the pedal's own sound) to fully opposed. Stereo in is a switch for stereo sources.

## Bass

- **Keep lows dry**, a **BASS** button on the panel, off by default (Kevin's call): a Linkwitz-Riley crossover splits the input, the lows skip the modulation and rejoin the wet highs. Its frequency is a small knob beside the button, 60 Hz to 400 Hz, 150 Hz by default. The low band is delayed by the line's centre delay so the crossover sums flat with no comb or phase hole, even fully wet.
- **Headroom**: the engine works in floating point with no clipping before the optional Wear saturation, so a low B string at full level passes clean.
- **Tone's low cut** stays gentle (12 dB per octave at most, and only clockwise of noon), so the knob at noon is truly flat for bass.
- Test cases will include a 41 Hz E and a 31 Hz B at full scale, with and without the split, checking level, phase and no clipping.

## Footswitches (proposed defaults)

Momentary CC 20, 21, 22, 23 on channel 16 (any channel when read straight from the footswitch's USB cable). Expression CC 11 on channel 16. Learn remaps anything. Timings shared with every Flip Fella plugin: hold 0.45 s, combo window 0.12 s, combo hold 1 s.

| Footswitch | Tap | Hold |
| --- | --- | --- |
| A | Bypass on or off, on the press | Momentary: flips while held (the pedal's momentary bypass dip) |
| B | Tap tempo (two or more taps) | **Brake**: the platter slows to a stop while held and spins back up on release |
| C | Fire the ramp once from the start | **Hold mode**: ramp while held (R), or pause (minus) or flurry (plus), set by the toggle |
| D | Switch between preset A and B (from Live it goes to A), gliding over about 0.3 s | Save the panel into the current slot (A if on Live) |
| A+B | Ramp reset: knobs back to where they're set, wave restarts on the beat | |
| C+D | Hand the footswitches to the next Lathe | Held 1 second: **Keeper** |

Why:

- **A and B are the pedal's two switches**, in your pedal-board order (bypass on the left). The pedal has tap on the left, but every Flip Fella plugin puts bypass on A, and muscle memory across plugins matters more than across pedals.
- **B's hold** on the pedal is the Hold mode. I've moved that to C as you suggested, so B's tap tempo never has to wait to see if you're holding. B's hold becomes Brake, the "pause" the maker describes ("interrupt the motion for pauses or flurries"), made into the one move every warped-vinyl player wants: the record slowing down.
- **C hold to ramp** is your idea and the pedal's Hold mode exactly. C tap fires the one-shot ramp, the same thing switching the pedal on does, so you can re-melt a chord without bypassing.
- **D switches presets**, your idea, with save on hold as in Murmur. A short glide stops a jump between very different settings clicking or lurching.
- **A+B ramp reset** is the pedal's CC 102 on a footswitch, for getting back to the knobs when a bounce has wandered.
- **Keeper stays on hold C+D, and it doesn't clash.** C's hold waits 0.45 s, but a second switch within the 0.12 s combo window turns the press into C+D, so the ramp never starts. C+D tap keeps its family meaning (hand over).

## Expression

Default: **Morph A to B**. Heel is preset A, toe is preset B, and every knob glides between the two. This is how the original's expression works, generalised: the pedal sweeps the dip-selected knobs from their position to a set end, and Lathe sweeps every knob from one full setting to another. Switches (wave toggles, division, voice) flip at half travel. Tap tempo and host sync aren't morphed.

Other targets, chosen on the expression bar:

- **Whammy**: the original's behaviour with no dip switches set, the waveform moved directly by the pedal, so you bend the pitch by foot. Range set by Depth.

- **Sweep one control**: Rate, Depth, Mix, Lag, Warp, Tone or Volume, from the knob to the Sweep end (top or bottom), or between two set points.
- **Ramp targets**: the original pedal's behaviour exactly, the knobs switched on in the ramp bank, each going its rise or fall way.

Heel and toe calibration, the curve, smoothing and learn come from `PedalControl` as in every Flip Fella plugin. Moving the expression pedal pauses ramping for the controls it owns, as the pedal does when an expression pedal is plugged in.

With your Chocolate Plus and EX-P, nothing needs mapping: CC 20 to 23 and CC 11 are the family defaults, it's read straight from USB on any channel, and Calibrate fixes the EX-P reading backwards.

## Sharing with Murmur

Both are Chase Bliss recreations, so the motion system is the same idea. Lathe will copy, and keep compatible:

- **`RampEngine`** (Murmur `Source/engine/Modulation.h`): ramp-and-hold, bounce, five morphing waveforms, Top and Bottom sweep. Lathe adds per-target rise or fall, ramp length from tap divisions, one-shot retrigger, hold-to-ramp with glide back, and pause. Written as a standalone `ChaseMotion.h` with no plugin dependencies so Murmur can adopt it unchanged later.
- **Preset slots**: Murmur's A, Live, B with save on hold. Lathe uses A and B plus the morph between them; the slot code and state saving are the same shape.
- **Original pedal MIDI map behind a channel setting**: the same pattern and Pedal page row.
- **`PedalControl` and `Keeper`**: copied from Prism Patch through Murmur (with the B+C combo events), unchanged.
- **Tap tempo with divisions**: Murmur's tap logic, with the pedal's 1 2 4 and 3 6 8 tables.

I can't push to Murmur from this session, so Lathe carries its own copy. The Murmur change would be a straight swap of `RampEngine` for `ChaseMotion.h`, its tests already cover it.

## Family conventions

- Engine in plain C++17 with no JUCE, with an offline render test, so it builds for the Raspberry Pi rig (portability).
- Builds: Windows VST3 and Linux ARM64 through GitHub Actions, published to the `builds` branch, with CPU figures at 48 kHz, 64 and 128 samples.
- Pages: Main, Ramp, Deep, Pedal. Every footswitch job has an on-screen button.
- Pebble sways with the modulation and wobbles during ramps, taps its foot on a press, leans with the expression pedal, nods after Keeper.
- No brand names in the UI.

## Decisions (Kevin, 4 November 2026)

1. **Hold mode minus and plus**: not in the mkII manual (Hold is HiFi only), so Lathe uses R ramp, minus pause, plus flurry, from the maker's "pauses or flurries of warping".
2. **Tap divisions**: 1, 2, 4 are whole, half and quarter-note cycles. 3, 6, 8 are read as triplets: a half-note triplet cycle (3 per bar), a quarter-note triplet (6 per bar) and an eighth note (8 per bar). Twice as slow for ramping.
3. **MoToByp**: momentary bypass, confirmed by the manual. On A's hold, plus the full switch.
4. **Keep lows dry**: off by default, BASS button on the panel.
5. **Stereo**: MISO, mono in and stereo out, by default.
6. **Noise**: as true to the pedal as possible.
7. **Footswitch B hold**: Brake.

## Sources

- Warped Vinyl mkII instructions (PDF supplied by Kevin): controls, dip switches, presets, expression, MIDI chart
- [Warped Vinyl HiFi instructions overview](https://www.klangfarbe.com/files/Shop/Artikel/44359/A-G44359_1.pdf) and [HiFi manual](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/622cbfccd1caf210cffce630/1647099857873/Warped+Vinyl+HiFi_Manual_Pedal_Chase+Bliss.pdf), as quoted in search results
- [Warped Vinyl HiFi MIDI manual](https://cb2k22.squarespace.com/s/Warped-Vinyl-HiFi_MIDI-Manual_Pedal_Chase-Bliss.pdf), as quoted in search results
- [Warped Vinyl mkII manual](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/622cbffe4ee40a32b496eed0/1647099909056/Warped+Vinyl+MKII_Manual_Pedal_Chase+Bliss.pdf) and [MKI manual](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/622cbfee20810b13395207f6/1647099889055/Warped+Vinyl+MKI_Manual_Pedal_Chase+Bliss.pdf), as quoted
- [Chase Bliss Warped Vinyl HiFi product page and FAQ](https://www.chasebliss.com/warped-vinyl-hifi)
- [MusicRadar: HiFi launch](https://www.musicradar.com/news/chase-bliss-audio-unveils-warped-vinyl-hifi-vibratochorus-pedal), [Mixdown: HiFi launch](https://mixdownmag.com.au/news/product/chase-bliss-audio-introduces-warped-vinyl-hifi-pedal/), [Premier Guitar: mkII launch](https://www.premierguitar.com/chase-bliss-audio-unveils-the-warped-vinyl-mkii-wombtone-mkii-and-gravitas)
- [Equipboard mkII](https://equipboard.com/items/chase-bliss-audio-warped-vinyl-mkii-analog-vibrato-chorus) and [HiFi](https://equipboard.com/items/chase-bliss-audio-warped-vinyl-hifi), [Pedal of the Day HiFi](https://www.pedal-of-the-day.com/2018/04/20/chase-bliss-audio-warped-vinyl-hifi-analog-vibrato-chorus/), [Harmony Central review](https://www.harmonycentral.com/expert-reviews/effects/chase-bliss-warped-vinyl-analog-vibratochorus-r235/)
- [Guitar Chalk HiFi settings](https://www.guitarchalk.com/chase-bliss-warped-vinyl-hifi-settings/), [Mod Wiggler thread](https://www.modwiggler.com/forum/viewtopic.php?t=115182), [Gearspace: VST rig of the Warped Vinyl](https://gearspace.com/board/so-many-guitars-so-little-time/1327047-diy-vst-rig-chase-bliss-warped-vinyl.html)
- [Max for Live HiFi controller](https://maxforlive.com/library/device/6744/chase-bliss-warped-vinyl-hifi-control) (CC 102 ramp reset)
- [Gear Gods: five pedals discontinued](https://geargods.net/news/chase-bliss-discontinues-five-pedals-due-to-safety-regulations)

The mkII manual was read in full. The HiFi manual couldn't be opened from this session (the network policy blocks the hosts), so HiFi-only details come from excerpts quoted in search results.
