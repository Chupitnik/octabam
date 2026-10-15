# MIDI Scales

Extends the existing **KEY** control at the bottom right of MIDI ARP SETUP
(FUNC + AMP), without moving controls or replacing the graph.

## Controls

F selects OFF or any of the 12 keys in Major, Dorian, Phrygian, Lydian,
Mixolydian, natural Minor or Locrian. Encoder order groups modes by key.
The stock values 1–24 still mean exactly the original Major/Minor pairs;
25–84 append the other five modes. OFF remains zero. The existing Part
byte, working mirror, dirty flags, save/reload and Part copy store the choice.
It is therefore a **Part setting**, not a new per-pattern project field.

## Composition

Select `MIDI SCALES` in the remix. This module works independently. It extends stock sequencer/arp output
quantization; it does not generate chords. Existing Major/Minor playback
keeps stock quantization. Added modes use nearest scale notes, ties downward.
Follow and Harmony integration is reserved for separate contributions. This
PR does not include or qualify those combinations.

No audio tracks or Scale Quantizer settings are involved. Older firmware does
not understand values 25–84: set KEY to OFF or an original Major/Minor value
before using an edited Part on stock firmware.

## Verification

Run `make check REMIX=midi-scales`; the linked-code gate executes
all 84 combinations over all 128 MIDI notes and checks the formatter bounds.
The installed encoder and output detours are checked, including all eight
tracks, stock Major/Minor continuation, surrounding bytes and registers.
Native Part storage is reused; project save/reload and physical UI behavior
are not established by this linked-code test.
Hardware testing remains pending.

The extended KEY selector uses the stock movement accumulator at four raw
counts per choice, capped to one choice per report. This keeps nearby scale
choices selectable without changing their saved IDs. Physical feel is untested.

## First playing workflow

Set a track's output channel and program a few notes. Open MIDI ARP SETUP
with FUNC + AMP, then use F (KEY) to choose C Dorian. The sequencer's scale
correction uses C, D, Eb, F, G, A and Bb. KEY OFF removes scale correction.


## Pinned author source

The adapter builds [author commit 9d31ce2f](https://github.com/radiohagen/octatrack-modules/commit/9d31ce2ff977205bdd3c17a58e55f86bc11376ac)
from `upstream/`. That review branch contains this module alone. Assembly
SHA-256 matches the completed native implementation; the source manifest now
resolves its path relative to itself. Octabam owns the gate and machine harness.

The adapter's `Linked.reference` checks the 870-byte standalone author runtime
at `0x40a955e0` on every build (SHA-256
`e265a08d11b72cc6806c6ac5212969b828c28e2c34681ae088c686e51db8c806`).
This reference was measured from the native source in integration commit
`8f8e00d061d17a890a3b01d5f340492f912712e2`. Native and pinned-adapter
standalone images are byte-identical at BUILD=79. Future Follow/Harmony
variants need their own references and combination checks.
