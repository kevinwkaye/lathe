# Lathe

A VST3 for Ableton Live, recreating the Chase Bliss Audio Warped Vinyl chorus and vibrato (the HiFi, with the mkII as a voice) in the Flip Fella look. All DSP here is original code written from public descriptions of the pedal's behaviour.

A warm, lush chorus at noon, a seasick warped record fully wet, and the pedal's hidden layer on show: the dip-switch ramps that make knobs rise, fall and bounce on their own, tap tempo with divisions, two presets and an expression pedal that morphs between them.

## Install

1. Open the **builds** branch of this repo and download `Lathe-Windows.zip`.
2. Unzip it and copy the `Lathe.vst3` folder to `C:\Program Files\Common Files\VST3`.
3. In Ableton, rescan plugins (Preferences, Plug-Ins, Rescan). Lathe appears under Audio Effects.

The same branch has `Lathe-Linux-ARM64.zip`, built for the future Raspberry Pi rig.

## The panel

**Record.** The platter turns at the wave's rate and the tonearm rides the wave. Below it is one cycle of the modulation as you hear it, with the playhead running across it. The two half-shape toggles set the rise (sine, triangle, square) and the fall (square, triangle, sine, in the pedal's order). Division sets the tap division: 1, 2, 4 wave cycles per bar, or 3, 6, 8 with the 3 · 6 · 8 switch.

| Control | What it does |
| --- | --- |
| **Tone** | HiFi: noon is flat, anticlockwise darkens, clockwise thins the lows and adds air. mkII: dark and murky to shimmery. Wet signal only, never the dry |
| **Lag** | The delay between dry and wet, 1 to 25 ms. Up for a lusher chorus. A little more hiss as it rises, as on the pedal |
| **Mix** | Dry to wet. Noon is chorus, fully clockwise is pure vibrato |
| **Volume** | Unity at about 2 o'clock, up to +12 dB with a little JFET-style grit when pushed |
| **Rate** | 0.1 to 12 Hz. Tap tempo or Sync takes over; turning the knob takes it back |
| **Depth** | How far the pitch swings, about ±1.3 semitones at full, kept musical at any rate |
| **Warp** | Moves the wave's peak: anticlockwise rises fast and falls slowly, clockwise the reverse, noon is even |
| **Ramp** | How long a ramp takes, 0.1 to 30 s (on the mkII this hides behind the Tone knob) |
| **Bass** | Keeps everything below its frequency (60 to 400 Hz, 150 by default) dry, centred and steady while the top end warbles |
| **Output** | Mono, MISO (mono in, stereo out, the default) or Stereo. Width is on the Pedal page |
| **Tap sets** | Whether tapping sets the wave's rate or the ramp's (twice as slow, as on the pedal) |
| **Hold C to** | What holding footswitch C does: ramp, pause the wave, or a flurry (four times faster) |
| **Voice** | HiFi (bright, clean, quiet) or mkII (warmer, murkier, a little hissier) |

A knob that a ramp or the expression pedal is moving turns amber and its hand moves on its own; a faint ghost hand marks where the knob is set.

**Ramp** is the pedal's two dip-switch banks as one table:

- **Moves** picks which knobs ramp: Volume, Lag, Mix, Rate, Depth, Warp and Tone.
- **Way** picks rise or fall for each.
- **Bounce** goes back and forth instead of ramping once and holding.
- **Sweep** sets which end each knob travels to: Top (fully clockwise) or Bottom. The knob always marks the other end.
- **Lo-Fi** is the mkII's longer, noisier, gooier vibrato.
- **MoToByp** makes the effect play only while footswitch A is held.

The ramp runs whenever the effect switches on, and resets when bypassed, as on the pedal. RAMP NOW (or tapping C) runs it again.

**Preset** is the pedal's preset toggle: A, Live (the panel as it is) and B. Switching glides between settings over 0.3 s. A recalled slot dims when you move a knob, and holding D saves. A starts as a lush chorus, B as a warped record that melts in when switched on.

**Pebble** sways with the modulation, leaning further as Depth rises. It wobbles like jelly while a ramp runs, goes dizzy and still under Brake, jitters in a flurry, holds its breath on a pause, dozes when bypassed, taps its foot on each press and nods after Keeper.

## Footswitches and expression (optional)

Everything works from the screen, so you don't need any pedals. The four squares along the bottom act exactly like footswitches, taps, holds and pairs included. If you have a MIDI footswitch:

1. Set its first four switches to send **momentary CC 20, 21, 22 and 23 on MIDI channel 16** (127 when pressed, 0 when released). Or use **Learn** on the Pedal page to use whatever yours sends.
2. Set an expression pedal input to **CC 11 on channel 16**, or click the expression bar and choose Learn.
3. Click the expression bar, choose **Calibrate heel and toe**, then rock the pedal from heel to toe once.

These settings are shared by every Flip Fella plugin on your computer, so you set them up once.

| Footswitch | Tap | Hold |
| --- | --- | --- |
| A | Bypass on or off (acts on the press) | Momentary: flips while held |
| B | **Tap tempo** (two taps set it; the wave syncs to it) | **Brake**: the record slows to a stop, then spins back up when you let go |
| C | Run the ramp once | **Hold mode**: ramp while held (glides back on release), pause, or flurry |
| D | Switch between preset A and B (from Live it goes to A) | Save the panel into the current slot |
| A+B | Reset the ramp: knobs back to where they're set, the wave restarts | |
| C+D | Hand the footswitches to the next Lathe | Held 1 second: **Keeper** |

Nothing needs a double-tap and nothing needs a hold of more than a second. The timings match the other Flip Fella plugins: hold 0.45 s, combo window 0.12 s, combo hold 1 s.

**Expression** morphs between the two presets by default: heel is preset A, toe is preset B, every knob glides between them and the switches flip half way. That's the pedal's expression mode (it sweeps the dip-selected knobs between the knob and the sweep end) made into a whole-pedal move. The chips on the expression bar switch to:

- **Ramp**: the original's exact behaviour, the knobs in the ramp bank sweeping between their knob and the sweep end, each its own way.
- **Whammy**: the original with nothing in the ramp bank, the wave bent directly by foot, its range set by Depth.

The menu reaches single controls too (Depth, Rate, Mix, Lag, Warp, Tone, Volume), and the Pedal page sets what the heel and toe reach, the smoothing and the curve.

### Getting footswitch MIDI to the plugin in Ableton

Lathe reads your footswitch straight from its USB cable, so every copy you load works with no MIDI track. The dot next to **FOOTSWITCHES** turns green when it's connected, and Pebble says FOOTSWITCHES READY.

1. In Ableton's Preferences, Link Tempo MIDI, turn **Track**, **Sync** and **Remote** off for your footswitch's input. Windows lets only one program use a MIDI input at a time, so Ableton has to let go of it.
2. Load Lathe. That's it.

If the dot stays grey, click **FOOTSWITCHES**. The menu says what's wrong, and you can pick your footswitch from the list if it wasn't found automatically.

With several copies loaded, the footswitches drive the one you opened or clicked last. Tap C+D to hand them to the next copy.

Prefer routing through Ableton? Choose **Only from a MIDI track in Ableton** in that menu, then turn **Track** back on for the footswitch, make a MIDI track with **MIDI From** set to the footswitch and **MIDI To** set to Lathe, and arm it.

One limit, as in the other Flip Fella plugins: Windows lets only one plugin file hold the footswitch's input, so if another Flip Fella plugin already has it, Lathe needs the Ableton MIDI track route above.

### Kevin's setup (M-Vave Chocolate Plus and M-Audio EX-P)

In CubeSuite: Custom mode, footswitches A to D as momentary CC 20 to 23, the pedal jack as Expression Pedal on CC 11. It always sends on channel 1 whatever CubeSuite says; read directly from its cable, Lathe accepts any channel. Put the Chocolate Plus switch on **U** and connect with the USB cable, not Bluetooth. The EX-P needs a 3.5 mm to 6.35 mm TRS adapter and reads backwards on the Chocolate Plus, which Calibrate fixes.

## Original pedal MIDI map

The pedal's own CC chart works on a channel you choose under **MIDI MAP** on the Pedal page. It's off by default, because its CC 20 (Ramp) and CC 21 (divisions) would otherwise clash with the footswitch defaults.

| CC | Control |
| --- | --- |
| 14 | Tone |
| 15 | Volume |
| 16 | Mix |
| 17 | RPM (Rate) |
| 18 | Depth |
| 19 | Warp |
| 20 | Ramp |
| 21 | Divisions: 0 whole, 1 half, 2 dotted eighth (Lathe's 3), 3 quarter, 4 eighth (6), 5 sixteenth (8) |
| 22 | Lag (Lathe's own, the HiFi's extra knob) |
| 93 | Tap |
| 100 | Expression |
| 102 | 0 bypasses, 127 engages, 127 while on reruns the ramp |

Program change 0 is Live, 1 the right slot (B) and 2 the left slot (A), as on the pedal. Program changes from Ableton work on any channel. Host tempo replaces MIDI clock: with **Tempo: Ableton** on (the Pedal page), Sync follows Ableton's tempo until you tap one in.

## Keeper

Press **KEEP** (or hold C+D for a second) to log the moment: every setting, what you're actually hearing (ramped values, rate, pitch swing), the preset, the expression pedal, the tempo, and the last 8 seconds of input and output as WAV files, into `Documents\Flip Fella Keepers`. The small arrow next to KEEP adds a note, exports a zip for Claude, opens the folder, sets Live or Studio, and learns a dedicated Keeper switch.

## Remote control

Every control, every dip switch, every footswitch job and Keeper are in one parameter list that a host or a remote controller can read and set. `footA` to `footD` act as the footswitches (1 to press, 0 to release, so taps, holds and pairs all work), `keeper` and `fireRamp` fire once and reset, and `brake` and `hold` can be set directly.

## CPU

Per instance, one core, the heaviest case (bass split, MISO, two knobs bouncing):

| Machine | 48 kHz, 64 samples | 48 kHz, 128 samples |
| --- | --- | --- |
| Windows x64, GitHub runner (HiFi) | 1.51% | 1.56% |
| Windows x64, GitHub runner (mkII) | 1.50% | 1.61% |
| Linux ARM64, GitHub runner (HiFi) | 1.34% | 1.33% |
| Linux ARM64, GitHub runner (mkII) | 1.34% | 1.33% |

Measured for v0.1 on 4 November 2026. A Raspberry Pi 5 core is slower than these runners, so expect a few times these figures there, still small.

Every build writes fresh figures for Windows x64 and Linux ARM64 to `cpu-windows-x64.txt` and `cpu-linux-arm64.txt` on the builds branch.

## Building

GitHub Actions builds the Windows VST3 and the Linux ARM64 build on every push and publishes them to the `builds` branch. To build locally: `cmake -S . -B build && cmake --build build --config Release`. JUCE downloads automatically on the first configure.

## Tests

- `lathe_render_test` runs the engine with no host: 28 scenarios at three sample rates (every control and voice, ramp on engage, rise and fall with both sweeps, bounce, hold to ramp and glide back, fire and reset, pause, flurry, brake really stopping and spinning up, whammy, expression on the ramp targets, 41 Hz and 31 Hz bass at -2 dBFS with the lows kept dry and steady, MISO width, mono, hiss level), checks for NaNs, levels above full scale and dead output, then measures CPU. Pass a folder to write a WAV of each scenario.
- `lathe_foot_test` drives the real plugin with plain MIDI press and release messages: every tap and hold, MoToByp, tap tempo, brake, the three hold modes, presets with glide, edit and save, A+B and C+D, the 1 second Keeper hold, expression morphing between presets, single controls, ramp targets and whammy, the original MIDI map on its own channel, the remote parameters, and a clash test. Run it with `--snapshot <folder>` under a display to render the panel in seven states.

## Project layout

| Path | What it holds |
| --- | --- |
| `Source/engine/` | The DSP engine, plain C++17 with no JUCE |
| `Source/engine/ChaseMotion.h` | The wave, ramp and tap divisions, written to be shared with Murmur |
| `Source/Params.h` | Every parameter, its range and default, and what a preset holds |
| `Source/PluginProcessor.*` | Host wrapper, footswitch jobs, presets, expression, MIDI map, Keeper, state |
| `Source/PluginEditor.*` | The Main and Pedal pages, and Pebble |
| `Source/PedalControl.*`, `Source/Keeper.*` | Shared with every Flip Fella plugin, copied from OilSlick (Prism Patch's module) |
| `docs/` | The research, feature map and footswitch plan |
