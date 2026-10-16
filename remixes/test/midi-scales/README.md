# MIDI Scales Test Remix

Selects MIDI SCALES and all fourteen stock effects, with fallback NONE.
It adds seven-mode KEY selection to native MIDI ARP SETUP. It does not
select Follow, Harmony, Loopback or any replacement effect.

```sh
make check REMIX=midi-scales
make reach RUN=1 KEEP=1 TESTS=1
.venv/bin/python tools/verify/verify_midi_scales_stock.py --project /path/to/project
```

The linked-code gate checks 84 key/mode combinations over 128 notes,
installed encoder/output hooks and formatter bounds. The project-backed
test checks cold load and native save across candidate, stock 1.40C and
candidate again on a disposable virtual card. Stock clamps extended KEY
values to B Minor when loaded and loses them when the bank is saved.

See [MIDI Scales](../../../modules/midi-scales/README.md) for controls,
source provenance, compatibility and evidence boundaries. Emulator screenshots
show BUILD=91. The current rebased build still needs physical encoder,
MIDI output and save/reload checks on hardware.
