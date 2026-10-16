# PLOCKS-P2 contribution notes

The page-2 editor marks a held trig as edited and clears native pending tap
masks before committing a lock. This lets release distinguish an edit from
a bare tap, preserving the lock just entered. `verify_plocksp2.py`
checks the held-step edit followed by release and subsequent bare-tap clearing.

### Stock Address Provenance

These are stock **1.40C** addresses, checked in the local MAIN_OS disassembly.
The native knob-commit routine starts at `0x4004ecfc`: at `0x4004f0aa`
it writes 1 to the 32-bit word `0x460d173a`, then clears the six 16-bit
words below. `pe_commit` mirrors that sequence.

| RAM word | Native knob-commit clear | Native trig-down read | Native release read / clear |
|---|---|---|---|
| `0x460d1a9e` | `0x4004f0b2` | `0x40050fee` | `0x4005fbe2` / `0x40060a5a` |
| `0x460d1aa0` | `0x4004f0b8` | `0x40050ff4` | `0x4006001a` / `0x40060a68` |
| `0x460d1aa2` | `0x4004f0be` | `0x40050ffa` | `0x4005feaa` / `0x40060a76` |
| `0x460d1aa4` | `0x4004f0c4` | `0x400516a6` | `0x40060674` / `0x40060a84` |
| `0x460d1aa6` | `0x4004f0ca` | `0x400516b8` | `0x40060a14` / `0x40060a92` |
| `0x460d10dc` | `0x4004f0d0` | `0x4003004a` | `0x4003013a` / `0x40030144` |

The main trig-down routine starts at `0x40050f20`; the release routine at
`0x4005fb44`. Release also reads the edited word at `0x40060b04` and clears
it at `0x40060b14` when the held-trig mask at `0x460d174a` is zero.
The sixth mask belongs to an alternate trig path starting at `0x4002fff8`,
called by release at `0x40060aa8`. Its consumed bits are cleared separately.
The common reset helper at `0x400339b0` clears all six masks as well.

The addresses, widths and matching stores are statically observed. The names
“edited” and “pending tap masks” describe their inferred role from those
read/write paths, corroborated by the release regression. They are not
firmware symbols. This is not an exhaustive claim about every reader.

Under the emulator, a recorded page-two lock survives release; a subsequent
bare tap clears it. Project/card persistence and protected-file refusal are
also covered by the passing regression. These are emulator measurements;
physical button timing is not verified on the unit for this build.

## SETUP display lifecycle

- The native trig-down path calls `0x400418e0` at `0x40051ec4`. Its
  page redraw at `0x40051e96` is conditional and can be skipped while a
  SETUP window is open. The PLOCKS hook keeps the bookkeeping call and
  then refreshes the active FX SETUP window.
- The release call to `0x4004d948(-1)` at `0x40060b38` is wrapped,
  keeping its argument and return registers. It redraws the native page
  then refreshes active FX SETUP. Committing a page-two edit also refreshes
  SETUP immediately. Native dial animation and value timeout are retained.
- FX1/FX2 descriptors at `0x400bc25a` / `0x400bc484` carry the window
  pointer at +8 and native draw callback at +44. The popup object is
  shared with other dialogs. Refresh requires the active pointer, its
  open flag, SETUP geometry `(7, 0, 0xf4, 0x40)`, an audio track and the
  corresponding FX page. A nonzero window pointer alone is insufficient.
- Label calls at `0x40037c78` / `0x400378dc` use the sixth argument of
  `0x40013904` for inversion. `plk_dial` supplies the same first-held-step
  lookup as the value display. An absent lock leaves the native label
  unchanged; a present lock inverts it.

`verify_plocksp2.py` now drives native panel inputs and decodes the panel
UART on a disposable card. FX2 PLATE REV and FX1 COMPRESSOR slot A are
compared with independently rendered normal-value references: unlocked
hold, lock 50 over Part 20, release, unheld +1, rehold, three slow +1
turns and FUNC removal. It checks pixels, label inversion, lock and Part
bytes, untouched page-one locks, and native MIXER/TEMPO/MIDI windows.
Screens and the port log remain in the gate's output directory. These
measure emulator behavior, not physical panel latency.

The pre-PR control restores upstream PLOCKS source at base
`c3f37e531f6b072b8197d93f173e063b55249363`. Its six initial FX2 screen
captures matched P1 (`7cb22414a58dbbd03e468ba1c69f03a862fb46ef`) exactly:
the lock bytes changed but the displayed value stayed stale. This is an
existing upstream display defect, separate from the release-erasure defect.
The initial memory/persistence regression did not decode the LCD and
therefore could not detect it.

### Broader Scenes gate limitation

The unmodified `verify_scenesp2.py` expects DELAY SERVER's page-two MODE
and TIME layout. With the local Core fixture, both the candidate and the
upstream PLOCKS control fail the same seven assertions (six frame values
and the seeded editor value). Core contains stock DELAY, not DELAY SERVER.
Changing that fixture from PLATE REV to stock DELAY does not make this
particular oracle applicable. This is not a passing aggregate Scenes
check, and it is not evidence of a new regression. The PLOCKS panel,
playback and persistence checks are reported separately. No Scenes code
or verification thresholds are changed by this contribution.
