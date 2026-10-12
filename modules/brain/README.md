# `brain` — BRAIN

The settings store's run-time side on the unit ([`docs/proposals/BRAIN.md`](../../docs/proposals/BRAIN.md)).
A remix lists it to give the card layer to its knob defaults.

It applies card-wide knob defaults and writes them: SAVE FX DEFAULT
(`brain_post_save(track, fx)`) makes the current Part's page of that track's
FX1 or FX2 the effect's default in `card.work`, and SAVE PROJECT copies
`card.work` to `card.strd`.

## On the panel

PROJ (MAIN MENU), then BRAIN, the fifth category. Its pane is one list, in alphabetical order; each row opens a full screen like the stock MIDI SYNC (the BRAIN icon at the left and the title at the right of the first band, a rule, then rows with the inverted cursor bar) except IMAGE CHECK, which is a popup. UP and DOWN move the cursor, YES acts on the row, NO goes back to the menu.

| row | screen |
|---|---|
| DEFAULTS | FX DEFAULTS: SAVE FX DEFAULT (the FX1 or FX2 page in view, of the current audio track in the current Part, becomes that effect's default on this card; a popup names the page) and CLEAR FX DEFAULT (that effect's default leaves `card.work`; the descriptor goes back to the image's). NEW PROJECT: SAVE NEW PROJECT (the current project's settings, the stock PROJECT menu's bytes with TEMPO left out, become the card's settings for new projects: a `stock.project` default in `card.work`) and CLEAR NEW PROJECT (that record leaves `card.work`; new projects take stock's settings). CARD: APPLY DEFAULTS (a checkbox; off, every effect takes the image's defaults) |
| IMAGE CHECK | a popup: what the last load found when it compared the project's BRAIN record with the image: `SAVED WITH THIS IMAGE`, `N EFFECT(S) CHANGED SINCE THE SAVE`, `NO RECORD OF A BRAIN SAVE`, `BRAIN FILES OF THIS PROJECT BAD` |
| MIDI | MIDI LOG (a checkbox: the debug log records MIDI messages; off, the MIDI thread's handler call returns at once after the handler), WRITE DEBUG LOG (the last 256 MIDI messages, clock and active sensing left out, each with the sync flags, the playing pattern and the next pattern after its handler ran, written to `/BRAIN/debug.txt` by the engine task; a popup says so when MIDI LOG is off), and the last three messages decoded and redrawn every half second (`CH01 CC 7= 20`, `CH01 PC 2 P3`: the pattern the program change queued, `CH02 NOTE 36 100`) |
| OTOP | Two pages, YES switches (`modules/load-meter` measures; BRAIN's `OTOP`, the Octatrack's `top`). Page 0, redrawn every half second: a bar for the frame interrupt's share of the ColdFire and one for each DSP core's busy time over its frame, the peak since opening a mark on each; `WORST FRAME`, the longest single frame-interrupt run since opening as a share of the 362.8 µs frame (`OVER` at 100%); `AUDIO DROPS`, the ESAI underrun and overrun frames since opening. `-` before a sample, `?` when the records since the last tick were all bad. Page 1 `OTOP FX`: each effect call's cost, measured by the DSP cores themselves around each FX1 and FX2 call, as `T5 2 BusVerb79  996 22%` (track, FX slot, effect, cycles a sample, share of that core's frame), sorted by cost, with a `STOCK A` and `STOCK B` row for what each core's busy time holds beyond its effects; UP and DOWN scroll. Without LOAD METER the bars stay empty and page 1 says so |
| REMIXES | REMIX SWITCH's rows (`modules/remix-switch`, sanderlegit): each `.RMX` in `/BRAIN/REMIXES/`, sorted; YES asks `BOOT <NAME>?` and YES boots it without writing the flash; after a switch a `FLASHED` separator and the image a power-cycle returns to |
| SETTINGS | a heading per other module that declares a setting and a value row per setting (below). BRAIN's own two are in DEFAULTS (APPLY DEFAULTS) and MIDI (MIDI LOG). `NO MODULE SETTINGS` in a remix with none |
| TEMPLATES | SAVE TEMPLATE (the page in view, asking for a name: the stock name editor opens on `TPL nn`, the lowest free number for the target, eight characters; YES saves it, replacing a template of that name on the target), then a section per target with card templates (the effect's name, AMP, LFO, MIDI TRACK, SRC and its machine type), sections and names alphabetical. YES on a template opens a list: LOAD (onto the page in view when it is that target's, else a popup "OPEN A PAGE FOR <target>"), RENAME (the name editor) and DELETE; each runs as an engine-task job and the rows are rebuilt |

A setting row reads `NAME` at the left and its value at the right (a Binary a checkbox elsewhere, ON or OFF here); YES steps the value (a Binary toggles; an Option steps by one and a Number by its step, each wrapping to its minimum past the maximum) and writes it to `card.work` through an engine-task job.

The stock menu engine is two levels deep (the root and a category's list), so BRAIN's screens are states of its menu-state table: the table (16 entries of `{enter, exit, draw, key, encoder}` at `0x400cbdac`) is copied to DRAM with six states appended (16..21, `brain_stab` in `manifest.py`) and its six references repointed (the same as `modules/busscreen`, in history, did for its 17th state, tags 85-90 on the unit). A row's action calls the engine's state opener (`0x40064e20`); the key dispatcher (`0x4006510a`) passes each key to `brain_scr_key` and redraws after it; NO runs the state's exit member and returns to the menu (`0x400650a0`). The screens draw with the stock routines the MIDI screens use (`0x400128a8` the icon, `0x40012bd8` text, `0x40011910` the rule, `0x40012254` the inverted bar, `0x40011a58` a dotted line) and are rebuilt from `sr[]` on entry and after each action. MAIN MENU's opening resets the pane to its top (REMIX SWITCH's detour at `0x40064c32`). Measured under the port, 12 Oct 2026 (`verify_brain`, `verify_remixswitch`, screens rendered with `ot_emu --lcd`).

Opened over any other page, or on a MIDI track, the popup says to open an
FX page and nothing is written. The category is a fifth root row: the root
list's rows pointer (`0x400cbda4`, a `SymbolRef`) points at a copy of the
four stock rows (read from the stock image at build time) with BRAIN
appended, and the root count (`0x400cbd8c`, a `Poke`) is 5; a second
module growing the root is refused by the ledger. The icon is one long
per column (pixels in the high byte, bit 0 at the top) with the stock
categories' shared second plane (`0x400cbfc4`). At each LOAD PROJECT, and at the bank load the power-up runs, the
core puts every effect's twelve descriptor defaults (`P+0x5e`) back to the
image's values. It then reads `/BRAIN/card.work`, or `card.strd` by the
pair rule (BRAIN.md section 6.2), and writes each valid default record
over its effect's bytes. The FX choosers and the new-part initialiser read
those bytes when they run (`tools/verify/verify_descdefaults.py`).

The file is written on a computer:

```bash
python3 tools/hw/ot_brain.py default /Volumes/CARD/BRAIN/card.work bottleservice DELAY TIME=52 FB=5
python3 tools/hw/ot_brain.py default /Volumes/CARD/BRAIN/card.work bottleservice "DELAY SERVER" --mode GRAIN SCTR=30
python3 tools/hw/ot_brain.py default /Volumes/CARD/BRAIN/card.work bottleservice DELAY --clear
python3 tools/hw/ot_brain.py dump /Volumes/CARD/BRAIN/card.work
```

## Settings

`SETTINGS` lists the settings of the modules in the remix that declare a
`Store` (BRAIN.md section 5). BRAIN's own store is `octabam.brain`:

| key | name | kind | default | does |
|---|---|---|---|---|
| 1 | MIDI LOG | Binary | off | the debug log (MIDI, WRITE DEBUG LOG) records MIDI messages; off, the MIDI thread's handler call returns at once after the handler |
| 2 | APPLY DFLTS | Binary | on | the card's knob defaults (DEFAULTS, APPLY DEFAULTS) apply at each project load; off, every effect takes the image's defaults |

At each load the core reads kind-1 records from `BRAIN/card.work` into
`brain_values[]` (one long per setting): a value outside its bounds or of
another type is skipped, a second record for a store is skipped, and a
setting with no record keeps its manifest default. A row's YES writes one
record per store to `card.work`, keeping the keys it does not know, in an
engine-task job (`brain_post_settings`, `job_args` `0x40000`). A name plus
its widest label must fit the 15-character pane (`PANE_W`); the build
refuses a setting that does not.

A module declares a setting and reads it:

```python
store=Store("octabam.<name>"),
settings=(Setting(1, "SHOW", Binary(), default=1),),
```

```python
from remix import brain
inc = brain.read_macro(remix, mods, "octabam.<name>", "SHOW", "get_show")  # text for the unit's include
```

The source then writes `get_show %d0`. With BRAIN in the remix and the
value neither pinned nor `Apply.BUILD`, the macro is one absolute load of
`brain_v_octabam_<name>_1`; otherwise it is `move.l #<value>,%d0`
(`docs/contributing/MODULES.md`, "Settings on the card"). Not built:
Blob and Trigger rows; `Apply.CALLBACK` has no callback field.

## The page shortcut and templates

Hold a page key (SRC, AMP, LFO, FX1, FX2) and press FUNC: a list opens
over the page with SAVE FX DEFAULT, CLEAR FX DEFAULT, SAVE TEMPLATE, LOAD
TEMPLATE and DELETE TEMPLATE. They act on the track and page in view.
SAVE FX DEFAULT and CLEAR FX DEFAULT take an FX page, as the BRAIN pane's
DEFAULTS rows do. The three template rows also take the AMP and LFO pages
of an audio track and, in MIDI mode, the whole MIDI track (all five pages
and their setup pages); over SRC they say to open an LFO, AMP or FX page.

- **The combination.** A page key's record names a key layer that is
  active while the key is held (`0x400bab6e`, keys `0x400baad2`, the same
  on MKI and MKII); stock's table there has REC, STOP, PLAY, YES and NO.
  BRAIN repoints the layer's keys (`SymbolRef` at `0x400bab72`) at a copy
  of those five records, read from the stock image at build time, with a
  record for FUNC that calls `brain_page_func`. ✅ Under the port on stock
  (10 Oct 2026): holding FX1 shows the knob values under the knobs, and
  FUNC pressed then changes nothing (the FUNC handler sets its held flag
  and redraws); stock's FUNC-then-page gesture (the chooser) is the other
  order and is unaffected. The keymap census is in `docs/proposals/BRAIN.md`
  section 9.
- **The list** is stock's (`0x4006d94c(count, sel, &sel_out, labels,
  handlers)`): YES stores the row, closes it and calls the row's handler.
- **Templates** are records of kind 3 in `card.work` (BRAIN.md section
  7.3): target 0, no mode, the target's layout word, an 8-byte name, its
  slots as Part bytes by key. SAVE TEMPLATE asks for the name (the stock name
  editor, `TPL nn` filled in); DELETE TEMPLATE and LOAD TEMPLATE list the
  target's templates by name. The targets:

  | target | store id | slots and keys | range per slot |
  |---|---|---|---|
  | FX1 or FX2 page | the effect's | its twelve slots by `Param.key` | the effect's descriptor |
  | AMP page | `stock.amp` | 1-6 page 1, 7-12 AMP SETUP | AMP descriptor `0x400d3950` |
  | LFO page | `stock.lfo` | 1-6 SPD/DEP, 7-9 PMTR, 10-12 WAVE, 13-15 MULT, 16-18 TRIG | audio LFO descriptor `0x400d37be` |
  | SRC page | `stock.static`, `stock.flex`, `stock.thru`, `stock.neighbor`, `stock.pickup`: the track's machine type (Part `+0x8eda2 + t`) | 1-6 page 1, 7-12 page 2 | the PLAYBACK descriptor of that machine (`0x400d2fe4 + 0x192 * machine`) |
  | MIDI track | `stock.midi` | 64 + flat for page 1 (0-29; 94, 95 the CTRL1/CTRL2 enable bitmaps), 1 + offset for the setup record (`docs/firmware/MIDI_TRACKS.md` section 1) | NOTE, LFO, ARP, CONTROL 1, CONTROL 2 descriptors; the bitmaps raw |

  A slot whose descriptor enable nibble has bit 0 clear is neither
  stored nor written (`docs/firmware/PARAM_PAGES.md` section 3b): the page-1
  writer refuses it (`0x40054d20`). In the stock image those are AMP XVOL
  (key 6) and TRIG (key 12), and the MIDI setup bytes with no parameter
  (offsets 3, 5, 0x0c, 0x0d, 0x0f, 0x10, 0x12, 0x13). The stock layout
  words are fixed (`tools/remix/brain.py` `STOCK_TARGETS`). Saves and deletes run in the engine task;
  `brain_load` and every template write keep a copy of the template
  records (4 KB) that the lists and LOAD read.
- **LOAD** writes the values as the user's knobs would: page 1 through
  the stock writer `0x40054cd8(track, flat, value)` (a MIDI track as
  `8 + track`), the other bytes as the editors store them
  (`modules/cc-map`'s measured recipe: the Part byte, the CS1 mirror
  `0x1001614e + offset + part * 0x18b2`, the live byte, the four dirty
  flags), each value clamped to its descriptor's `[min, min + count - 1]`.
  The live byte of an audio page-2 byte is the lane `0x80000810 + 72 * t
  + 0x26 + index` (index from `+0x8f072 + 30 * t`; `LFO.md` section 2), of
  a MIDI byte the live copy `0x46c76dc0 + 0x44 * t` (`MIDI_TRACKS.md`
  section 2). A MIDI track takes its setup first, then `0x4009a3e0` when
  CHAN changed and `0x4009ec70(bank, part)` (bank and program out), then
  its page-1 values. A template carries MODE with the other knobs, so MODE
  DEFAULTS' re-default is not called.
- **Rename** opens the stock name editor (`0x4007e664(max, title,
  buffer, 1, done(confirmed))`, the call KITS' SAVE KIT makes); on
  confirm the engine task rewrites the record's name field and payload
  CRC, every other byte kept.
- Not built yet: template files on the card for sharing.

## Project settings for new projects

SAVE NEW PROJ (BRAIN > DEFAULTS) writes a card default of store
`stock.project`: the stock PROJECT menu's bytes (`0x80000024..0x8000006a`
as `docs/firmware/STORAGE.md` section 5 lists them, TEMPOx24 left out;
keys `1 + offset`, raw bytes). CREATE EMPTY PROJECT applies it: a detour
at `0x40085176`, the call after the empty project's RAM (`0x400909d8`) in
the new-project job, writes each byte and its CS1 mirror (`0x100b1460 +
offset`) before the job writes the new `project.work`. Measured under the
port: a new project's own settings differ from a loaded project's in 11 of
56 keys (TEMPO, clock and transport receive, program change, AUTO CHANNEL,
24-bit loading and recording), and with the record the new `project.work`
carries the card's values.

## MIDI knob names

On a MIDI track the page shortcut has a sixth row, NAME KNOB: a list of
the track's ten assignable CCs (`CC1  NAME`, `----` without a name); YES
opens the stock name editor on that CC's name (four characters; an empty
name clears it).

- The names are per bank, Part, MIDI track and CC, in the project's
  `brain.work` as field blocks (kind 5) of store `stock.midi`: one per Part
  with a name, target `bank * 4 + Part`, a Name value keyed `track * 10 +
  cc + 1`. `brain_load` clears them and reads the project's blocks; the
  engine task rewrites the blocks after each edit (`write_names`).
- The CTRL1 and CTRL2 pages draw them: a detour at `0x4004e474`, the
  drawer's label push (`docs/firmware/MIDI_TRACKS.md` section 6), calls
  `brain_klabel(P, slot, stock label)`, which returns the current track's
  name for CTRL1 slots 2-5 (CC1-CC4) and CTRL2 slots 0-5 (CC5-CC10) when
  it has one.
- A MIDI track's template (`stock.midi`) carries its names as Name values
  keyed `129 + cc`; LOAD TEMPLATE puts them on the track and rewrites the
  blocks.

## The live screens

OTOP and MIDI redraw every half second from a soft timer (`0x40031a0c`,
about 0.5 s on the sys task's tick; REMIX SWITCH's boot picker uses the same
call, PR #542): the tick refreshes the screen's figures and calls the menu
redraw (`0x40064d7c`). It stops itself when the state changes (it clears its
slot's function word, which the service re-links only while nonzero); the
state's exit member and its enter member stop it with `UNTIMER`
(`0x40031abc`).

## SAVE FX DEFAULT

`brain_post_save(track, fx)` posts a message of type `0x41` to the engine
task's queue (`0x460d17ce`, through stock's `0x40000c3c`); stock's job
switch ignores types above 45, and the detour at `0x4008485e` runs
`brain_job` for this one in the engine task, where stock writes the
project's files. The job takes the twelve page bytes from the current
Part, replaces that effect's default record (no mode) in `card.work` with
them, keeps every other record's bytes, creates `/BRAIN` on a card that
has none (the file system's directory slot `0x46c8240a`, as stock's new
set does), and writes the values onto the descriptor at once. A damaged
`card.work` with no valid `card.strd` is not overwritten (error 2). One
job is pending at a time.

SAVE PROJECT's project store (`0x4008ee74`, KITS hooks its entry) is
reached through its three call sites (`0x40085642`, `0x400856dc`,
`0x40085780`); each copies a valid `card.work` to `card.strd` before the
store runs, as PLOCKS P2 does at the load call sites.

## What it applies

- Default records (kind 2) for target 0 with no mode, whose store id names
  an effect in the image and whose layout hash is the image's
  (`tools/remix/brain.py` `fx_store_id`, `fx_layout`). The table of
  effects is generated per remix (`manifest.py` `fx_inc`).
- Byte values by key; a value outside its slot's count (read from the
  descriptor) is skipped.
- A record with a mode writes into MODE DEFAULTS' view table
  (`MODEDEF_TABLE`, through `Linked.defsyms`; 0 without that module): the
  values of the slots that mode's view already re-defaults. The table is
  sparse and keeps its size, so a slot the view does not list is skipped
  and counted (`skip_slot`), and a mode record without MODE DEFAULTS in the
  remix is skipped (`skip_mode`).
- A record with a bad payload CRC, an unknown store id, another layout, a
  mode, or a second copy (BRAIN.md section 7.7 rule 5) is skipped and
  counted.
- An effect on both choosers (SPECTRUM, CHARACTER, MODULATION) has one
  descriptor in both id tables; an id-table entry whose descriptor carries
  another id (NONE, an effect on one chooser only) is never written.

## Measured

Under the port (`tools/verify/verify_brain.py`, remix `brain`, 6 Oct 2026),
with T2's FX2 chooser select (SEND -> stock DELAY) and FX1 select
(SPECTRUM -> CHARACTER):

| card | lands | state |
|---|---|---|
| no file | the image's defaults | fresh |
| `card.work` with DELAY and CHARACTER records | their knobs; one value outside its count skipped | card.work |
| `card.work` damaged, `card.strd` valid | `card.strd`'s values | recovered |
| `card.work` damaged, no `card.strd` | the image's defaults | damaged |
| a record with another layout | the image's defaults | card.work, 1 skipped |
| two records for one effect | the image's defaults | card.work, both skipped |

| SAVE FX DEFAULT for T1's FX2 and T2's FX1, then the store copy, on a card holding two other records | the two records hold the live pages; the others keep their bytes; `card.strd` = `card.work`; a second boot puts the saved page on BusDelay's descriptor | card.work |
| the panel: T1, FX2 page, PROJ, DOWN x4, RIGHT, YES on SAVE FX DEFAULT; then OK, DOWN, YES on CLEAR FX DEFAULT | `card.work` holds T1's FX2 page; after CLEAR no BusDelay record | |
| SAVE FX DEFAULT on a card with no `BRAIN` folder | the folder and `card.work` with one record | |
| out-of-count bytes in every Part of every bank, no file | read count - 1 after the load: stock's Part validator `0x40002318`, so the brain has no clamp | |
| `card.work` with MIDI LOG and APPLY DFLTS off and an unknown key 99 | values (0,0); no default record applied; the unknown key kept on write | card.work |
| a settings record with an out-of-bounds Binary and a wrong type | values stay (1,1); 2 skipped | card.work |
| the panel: PROJ, DOWN x4, RIGHT, DOWN x2, YES on SETTINGS, DOWN, YES on MIDI LOG | `card.work` holds one settings record {1:1, 2:1, 99:1}; screens rendered under `ot_emu --lcd` | card.work |
| the shortcut on T1's FX1 page: SAVE TEMPLATE (YES on the name editor), knob A +10, LOAD TEMPLATE `TPL 01` | one template record of CHARACTER with the saved page; the page moves with the knob and comes back to the saved values on LOAD; screens rendered under `ot_emu --lcd` | card.work |
| a GRAIN record for BusDelay (FDBK, SCTR, GLEN listed by the view; DEL not), then MODE CLEAN -> GRAIN through the FX2 page-2 editor on T1 | the view with the card's three values; DEL skipped and untouched | card.work |

Both hooks run during one LOAD PROJECT under the port (2 calls a load). A power-up with no LOAD PROJECT post
(`--no-post`, CS1 from an earlier boot) runs the bank-load hook alone and
the card's DELAY default (TIME 52, FB 5) lands on T2's FX2 select (7 Oct
2026, by hand; not in the gate).

## The project pair

`<set path>/<project folder>/brain.work` and `brain.strd` (file kinds 2 and
3), the set path read from `0x100f8480` and the folder name from
`0x100f8378`. The pair rule is the card pair's (BRAIN.md section 6.2).

- SAVE PROJECT: the engine task copies `card.work` to `card.strd`, then
  writes the project's `brain.work` and copies it to `brain.strd`. The
  file holds the PROJECT-scope settings records (one per store with such a
  setting; no module declares one yet) and one project record (kind 4,
  store id `octabam.brain`): per effect in `brain_fx[]`, the FX id, store
  id and layout hash. Other records of an existing `brain.work` (unknown
  kinds, other stores) are kept byte for byte; a previous project record
  is replaced. A damaged pair with no valid copy is not overwritten
  (write error 2); no project named gives error 5.
- Each load (LOAD PROJECT, power-up bank load): the pair is read, its
  PROJECT-scope settings land in the value table, and the project record
  is compared with the image. Counters, `brain_pcounts` (`.data`): state
  (0 none, 1 `brain.work`, 2 recovered from `brain.strd`, 3 damaged),
  record found, entries, and per entry one of same, moved (its FX id now
  names another store), layout (same store, another layout hash), gone
  (the image has no such store); skipped records (CRC, schema major, a
  second record). `brain_proj_counts` holds the last project save's result.
- IMAGE CHECK shows the last load's result in the stock popup:
  `SAVED WITH THIS IMAGE`, `N EFFECT(S) CHANGED SINCE THE SAVE` (moved +
  layout + gone), `NO RECORD OF A BRAIN SAVE`, or `BRAIN FILES OF THIS
  PROJECT BAD`.
- Parts are not rewritten and not clamped: stock's Part validator clamps
  out-of-count bytes at the load (BRAIN.md section 8).
- `set_ent` (`brain_set[]`) is 44 bytes: a scope word follows the group
  pointer (0 CARD, 1 PROJECT). `card.work` carries the CARD entries.

Measured under the port (`verify_brain`, remix `brain`, project fixture
staged as set `BRAINSET`, project `BRAINPRJ`): SAVE PROJECT on a project
folder holding an unknown-kind record and a stale project record writes
`brain.work` with the unknown record unchanged and one project record
equal to the image's table, and `brain.strd` equal to it with kind 3; a
load with a project record of four entries (one unchanged, one with
another layout, one for an unknown store, one whose FX id names another
store) counts 1 same, 1 layout, 1 gone, 1 moved; a damaged `brain.work`
with a valid `brain.strd` reads the same counts with state 2; a damaged
`brain.work` alone gives state 3 and no record read.

## On the unit

Not yet.

## Open

- A set named `BRAIN` shares the directory: the store's files sit in
  that set's folder beside its `AUDIO` and project folders. Whether the
  project list then shows `templates/` is not checked. The gate uses
  another set name. Stock lists a root directory as a set only when it has
  an `AUDIO` folder (📖 the set list's filler `0x40080d0c` lists `/` and
  keeps an entry when `0x40025650("/<name>")` returns nonzero: the
  directory and `"%s/AUDIO"` both pass the file system's existence probe
  `0x46c823fa`), so `/BRAIN` holding only the store is not listed;
  not measured on a unit.
- Timing of the card read on the unit (one file, at most 16 KB).

- The CS1 block (BRAIN.md section 12). The IMAGE CHECK popup's text
  is not rendered under `--lcd`.
- LOAD TEMPLATE writes the current bank's Part through the stock writers
  (✅ under the port, the Part bytes). The live lane, what the DSP plays,
  follows when that bank is the one playing (the page-1 writer updates the
  lane only then); the port's fixture plays another bank than the UI
  shows, so the sound after a LOAD is a hardware-round item.
- `--step call` runs as main under the port, where file I/O does not
  return (an unimplemented opcode at `0x4003b108`); the gate posts the job
  instead. The menu rows post from the UI task (measured under the port).

## Gates

- `tools/verify/verify_brain.py` (the manifest's gate, image stage), with
  `modules/brain/generate_brain.py --check`.

## Hooks

| site | stock | stub |
|---|---|---|
| `0x40085342` | `moveq #27,%d1 / movel %d1,%fp@(-570)`, LOAD PROJECT before the empty-project init `0x400909d8` | `brain_on_load`: `brain_load`, then the two instructions |
| `0x40084d4a` | `mvzb 0x80000002,%d1`, the bank load of every bank but the current (the power-up's path) | `brain_on_bankload` |
| `0x40005572` | `moveal %a2@(0,%d0:l:4),%a0 / jsr %a0@`, the MIDI thread's handler call (table `0x400d6474`) | `brain_on_midi`: copies the message, calls the handler, logs it with what it left |
| `0x4008485e` | `mvzb %a2@,%d0 / moveq #45,%d1 / cmpl %d0,%d1`, the engine task's job switch | `brain_on_job`: type `0x41` runs `brain_job` and returns to the receive at `0x4008484e`; other types continue at `0x40084864` |
| `0x40085642`, `0x400856dc`, `0x40085780` | `jsr 0x4008ee74`, the project store | `brain_on_store`: `card.work` -> `card.strd`, then the store |

`brain.c` is compiled to `brain.s` by `generate_brain.py` (euclid's flags);
`hooks.s` is appended. 21 KB of `.bss`: a 16 KB file buffer, the 4 KB
I/O buffer, the image's defaults (768 B), MODE DEFAULTS' table (up to 1 KB).
