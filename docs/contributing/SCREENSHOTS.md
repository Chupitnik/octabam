# Screenshots for the manual site

The manual site (`docs/site/`) shows LCD screens captured from the port, the
ColdFire machine in `tools/emu/ot_emu`. A capture is a scripted panel session:
`tools/emu/live_shots.py` builds a remix, boots it on a card staged from a
project, replays a script of key presses, and writes one PNG per `shot` line.

Confidence markers: **measured** = run on the port; **read** = from the
source of the tool, not run for this document.

## When to re-capture

Re-capture in the same PR when the diff changes anything a shot shows:

- a module's menu rows, names, value formatters, pages or `FxPage` layout;
- BRAIN screens;
- a remix's module set, where it changes what a screen lists;
- the first-boot sequence the scripts rely on (the single `[NO]` after the date prompt).

After a rebase onto `origin/main`, check whether main changed any of those
since the merge-base:

```bash
git diff --stat $(git merge-base HEAD origin/main) origin/main -- \
  modules/*/manifest.py modules/brain tools/site/shots docs/site/usage docs/site/img
```

A non-empty result touching a module your branch also touches, or a script
for a remix your branch builds, means re-capture that remix's scripts.

`mkdocs build --strict` checks that image links resolve. It does not compare
a PNG with the current UI; a stale shot passes CI.

## Prerequisites (read)

- a worktree with `vendor/` and `.venv/` linked and `out/raw/section_3_MAIN_OS.bin` present (`AGENTS.md`, "Worktrees and the shared checkout");
- the port built in the worktree: `make emu-cf` (`out/emu/ot_emu`; never symlink `out/emu`);
- an Octatrack project directory: `OT_PROJECT=<dir>`, `--project <dir>`, or the path in `~/.octabam_project`. The scripts name the gate projects `OCTABAM89_setgate` (bus) and `OCTABAM89_shots` (inserts); neither is in the repo, because a project holds sample data. A script's header comment says which effect sits on which track. Without those projects, build the same layout with `tools/hw/ot_project.py set-fx` (below).

## Run

```bash
OT_PROJECT=<project dir> .venv/bin/python3 tools/emu/live_shots.py <remix> \
  --script tools/site/shots/<remix>.script
```

Options: `--bank N` (default: the project's current bank + 1, or `OT_BANK`),
`--scale N` (PNG scale, default 4), `--keep-card`, `--dsp` (runs the DSP pair,
about 4x slower than real time), `--audio-in`, `--main-level`.

The tool builds the remix with `BUILD=91 XBUS=1 SPEC=1`, copies
`out/mainos_bus.bin` to `out/live/image.bin`, stages the card at
`out/live/card.img`, and logs the port to `out/live/port.txt`. Output PNGs go to
`docs/site/img/<module>/<screen>.png`.

## Script format

The directives are in the `live_shots.py` docstring: `key`, `chord`, `hold`,
`release`, `enc`, `wait`, `shot <module>/<screen>`, `quit`. One line per
directive; `#` starts a comment. Key codes are hex panel codes. Used in the committed scripts: `0x25` FX1,
`0x26` FX2, `0x10` `0x11` `0x12` tracks 1-3 (tracks 4-8 are read from the
scripts, not probed here), `0x32` NO, `0x28` PROJ. Each script's header
records the probe walk that fixed its sequence.

## Project layout the scripts assume

One effect per track, assigned in the project file, so a script needs only
track keys and the FX keys, with no chooser navigation:

```bash
python3 tools/hw/ot_project.py set-fx <PROJECT_DIR> fx1|fx2 <TRACK> <MODULE>
```

Effect ids sit at `PART_BASE + part*PART_STRIDE + 0x09` (FX1) and `+ 0x11`
(FX2) of the bank file (`tools/hw/ot_project.py`). The writes cover all eight
part records (parts 5-8 are the saved copies; see the comment above `PART_BASE`).
Use a copy of the project; `ot_project.py` refuses writes without a same-day
backup directory.

## FX page behaviour (measured on the port, 16 Oct 2026)

- Boot restores the FX1 setup on **page 2** (the routing grid).
- Track keys switch tracks on both pages.
- FX2 from FX1's page 2 opens FX2's page 2 (stateful).
- NO on page 2 goes to page 1; YES on page 2 opens the routing-cell editor.
- FX from page 1 stays on page 1; page 2 is reached only from boot or from the other FX slot's page 2.
- The bus FX2 servers have one page and no routing page.
- The tuner shows `--` under the live path: the port's DSP track chain produced no output (tone on the DSP input, gain table filled, TX0 ring all zero, 16 Oct 2026).

## What each script covers

| script | remix | covers |
|---|---|---|
| `octabam-core.script` | `octabam-core` | main menu, BRAIN screens, KITS, tuner |
| `octabam-inserts.script` | `octabam-inserts` | FX1/FX2 pages and the per-track effects |
| `octabam-inserts-p2.script` | `octabam-inserts` | page 2 walk, per-effect choosers |
| `octabam-bus.script` | `octabam-bus` | bus servers' FX2 pages, locked FX2 chooser, TEMPO window |

A new remix gets its own `tools/site/shots/<remix>.script`, named after the
remix directory.

## Naming and what is committed

`shot <module>/<screen>` writes `docs/site/img/<module>/<screen>.png`. The
usage fragment `docs/site/usage/<module>.md` references it as
`../../img/<module>/<screen>.png`.

The committed scripts write most of their 70 shots under `raw/` and `raw2/`
(probe walks); `docs/site/img/` in the tree has no `raw/` or `raw2/`
directory, and the committed images (`brain/`, `busdelay/`, `character/`, ...)
carry the final names. The step that turned the `raw/` captures into the
final names is not in the repo (measured: no script or doc references
`img/raw`). Until a script does it, a re-capture of a module's final image
needs a `shot <module>/<screen>` line with the final name in the script.

## Before the PR

```bash
git status --short docs/site/img          # only the PNGs you meant to change
mkdocs build --strict --config-file docs/site/mkdocs.yml
```

Commit the PNGs with the script change. Name the remix, project and script
you ran in the PR body.
