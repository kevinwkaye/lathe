# Lathe foot control plan

Written 4 November 2026. Built in the first version, following the house rules and Prism Patch's `PedalControl` (copied through OilSlick, unchanged).

## Footswitch defaults

Momentary CC 20, 21, 22, 23 on channel 16 (any channel when read straight from the footswitch's USB cable). Learn remaps any of them.

| Footswitch | Tap | Hold |
| --- | --- | --- |
| A | Bypass, acting on the press | Momentary: flips while held (MoToByp on: plays only while held) |
| B | Tap tempo, from the press times, last two presses | Brake: the record slows to a stop, spins back up on release |
| C | Run the ramp once | Hold mode: ramp while held (R), pause (minus) or flurry (plus) |
| D | Preset A or B, gliding 0.3 s (Live goes to A) | Save the panel into the current slot (A if on Live) |
| A+B | Reset the ramp | |
| C+D | Hand the footswitches to the next Lathe | Held 1 second: Keeper |

Why: A and B are the pedal's own two stomps, Bypass and Tap, in the family order (bypass on A). The pedal's Tap stomp also does the Hold mode, but sharing one switch would make every tap wait to see if it's a hold, so Hold moved to C as Kevin suggested, and B's hold became Brake, the pause the maker describes made into the warped-vinyl move: the record slowing down. D is the pedal's preset toggle on a foot, with save on hold as in Murmur. A+B is the pedal's CC 102 ramp reset.

A acts on the press, so it's instant. If the press turns out to be half of A+B, the press is undone, so the pair never leaves a stray bypass behind. B's tap tempo uses the press times, so a hold never counts as a tap.

Keeper stays on hold C+D, and doesn't clash: a second switch within the 0.12 s combo window turns the press into C+D, so C's hold never starts.

## Expression default

CC 11 on channel 16. Morph by default: heel is preset A, toe is preset B, every knob glides between them, switches flip half way. On-screen chips switch to Ramp (the original's dip-selected sweep) or Whammy (the wave bent directly). The menu reaches every knob. Heel and toe calibration, curve and the shared settings come from `PedalControl`. Range and smoothing are per set.

## Original pedal MIDI map

The mkII chart (CC 14 to 21, 93, 100, 102, plus Lag on CC 22) answers only on the channel chosen under MIDI MAP on the Pedal page, off by default. Off, its CC 20 and 21 can't collide with footswitches A and B. On, the footswitch's own messages straight from its cable still win.

## Tests

`Tests/FootTest.cpp` covers every tap and hold, MoToByp, tap tempo at 120 bpm, brake, the three hold modes, presets (glide, edited, save), A+B and C+D in both orders, the 1 second Keeper hold, expression on Morph, one control, ramp targets and whammy, the MIDI map off and on, the remote parameters, and a clash test: sustain (CC 64), mod wheel (CC 1), CC 20 to 23, CC 11 and the old chart's numbers on channels 1, 2 and 15 change nothing.
