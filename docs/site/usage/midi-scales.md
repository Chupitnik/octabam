## What it does

MIDI Scales extends the native MIDI ARP SETUP **KEY** control to seven
diatonic modes in all twelve keys. OFF bypasses scale correction. Major
and natural Minor keep their original saved values and playback behavior;
Dorian, Phrygian, Lydian, Mixolydian and Locrian add five more choices per key.

## How to use it

Select a MIDI track, set its output channel and program some notes. Open
ARP SETUP with **FUNC + AMP**, then press encoder **F** to enable KEY if it is OFF and turn F to select
the key and mode. Press F again to bypass it. Choices are
grouped by key: Major, Dorian, Phrygian, Lydian, Mixolydian, Minor, Locrian.
The screen abbreviates these as MAJ, DOR, PHR, LYD, MIX, MIN and LOC.

![MIDI ARP SETUP with KEY OFF](../../img/midi-scales/key-off.png)

![MIDI ARP SETUP with C Major selected](../../img/midi-scales/c-major.png)

For example, **C DOR** corrects outgoing sequencer/arp notes to C, D, Eb,
F, G, A and Bb. Added modes choose the nearest scale note, with ties going
down. It changes note pitches; it does not generate chords.

![MIDI ARP SETUP with C Dorian selected](../../img/midi-scales/c-dorian.png)

**C MIX** instead uses C, D, E, F, G, A and Bb.

![MIDI ARP SETUP with C Mixolydian selected](../../img/midi-scales/c-mixolydian.png)

## Storage and compatibility

KEY is stored in the existing Part setting. Before using an edited Part on
stock firmware, select OFF or an original Major/Minor choice: stock firmware
does not understand the additional mode values. Audio-track Scale Quantizer
is separate.

This module works independently; it does not require the PLOCKS or TEMPO SYNC
contributions. Follow and Harmony combinations belong to later contributions.

## Verification limits

These are LCD captures from the emulator using the standalone MIDI Scales
remix (BUILD=91), not photographs of hardware. Linked-code checks cover all
84 key/mode combinations, all 128 note values, the installed hooks and
formatter bounds. Physical encoder feel, hardware MIDI timing and full
project save/reload acceptance remain unverified.
