# Templates from the image

Each `<file>.json` here is one template that a remix can carry
(`Remix.templates`, `docs/proposals/BRAIN.md` section 4.2). BRAIN lists the
image's templates before the card's, both in the page shortcut's LOAD
TEMPLATE list and in BRAIN > TEMPLATES, where COPY TO CARD makes one an
ordinary card template (`modules/brain/README.md`).

```json
{
  "name": "SHORT",
  "target": "stock.amp",
  "knobs": {"ATK": 0, "HOLD": 12, "REL": 24, "VOL": 96}
}
```

- `name`: 1 to 8 printable ASCII characters, the list row.
- `target`: an effect's module key (`"CHARACTER"`), or a stock target:
  `stock.amp`, `stock.lfo`, `stock.midi`, or the SRC page of one machine
  type, `stock.static`, `stock.flex`, `stock.thru`, `stock.neighbor`,
  `stock.pickup` (the short forms `amp`, `flex`, ... too).
- `knobs`: knob name to value. An effect's knobs by `Param.name`. Stock
  targets by the names `tools/remix/brain.py` `stock_slots` reads from the
  stock image's page descriptors:
  - AMP: `ATK HOLD REL VOL BAL AMP SYNC ATCK FX1 FX2`;
  - SRC: STATIC and FLEX `PTCH STRT LEN RATE RTRG RTIM LOOP SLIC LEN2
    RATE2 TSTR TSNS`; THRU `INAB VOL INCD VOL2`; PICKUP `PTCH DIR LEN
    GAIN OP TSTR TSNS` (a name a page repeats takes a `2`);
  - LFO: `SPD1..SPD3 DEP1..DEP3 PMTR1..PMTR3 WAVE1..WAVE3 MULT1..MULT3
    TRIG1..TRIG3`;
  - MIDI track: `PAGE.NAME` for page 1 (`NOTE.VEL`, `ARP.MODE`,
    `CTL1.CC1`), the setup slots by their names (`NOTE.CHAN`, `LFO.PMTR1`,
    `ARP.KEY`), a CC number as `CTL1.CC1#`, the enable bitmaps
    `CTL1.ON` and `CTL2.ON`.
- A knob the file leaves out keeps the track's current value on LOAD.

The build refuses a missing file, an unknown target or knob, a value
outside the knob's range, a bad name, two templates of one target under one
name, and more than 4096 bytes of records (`brain.check_templates`).
