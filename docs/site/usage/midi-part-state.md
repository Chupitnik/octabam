## What it does

Provides the native Part-state access used by MIDI Loopback. Include MIDI PART
STATE when selecting MIDI LOOPBACK in a custom remix. It adds no screen or
control of its own.

Routing settings belong to the native Part: Save, Reload, Clear and Paste use
the existing Part operations. Playback reads the Part playing on each MIDI
track, independently of the Part selected for editing.

## Verification

Checked with linked-code tests and native Part operations in the emulator.
Hardware acceptance remains outstanding. Later MIDI consumers and their
combinations need their own verification.
