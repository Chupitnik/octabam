# COPY PROJECT

Duplicate a project folder on the CompactFlash card without loading it
first.

**MAIN MENU → PROJECT → COPY PROJECT** (directly under SAVE TO NEW) lists
every project in the mounted set, asks for a name, confirms, and copies
the source folder byte-for-byte into a new folder. Module sidecars in the
folder (`kits.work`, `scenes.work`, `p2lk*`, …) come along; stock SAVE TO
NEW does not. After a successful copy it asks whether to open the copy
(stock LOAD PROJECT).

## Use

1. Mount the set that holds the source project (CHANGE set if needed).
2. PROJECT → COPY PROJECT.
3. Pick the source from the list (folders with `project.work` /
   `project.strd`; `AUDIO` is skipped). LEVEL scrolls (STORE + this
   module: gated on this list's callbacks only).
4. Edit the suggested name (`PROJECT YYMMDD`, or FUNC+CUE with RND NAME)
   and YES. Names longer than 32 characters refuse with `COPY FAILED`.
5. Confirm `COPY SRC -> DST?`.
6. On success: `PROJECT COPIED` / `OPEN IT?` — YES loads the copy, NO
   leaves the live project as it was. `COPY FAILED` on error; a partial
   destination folder is removed so a retry does not hit
   `NAME ALREADY IN USE!`.

This is a card-to-card copy of whatever is on disk (including unsaved
`.work` cache). SAVE PROJECT on the source first if you want the last
saved state only.

Limits that refuse with `COPY FAILED` (no silent truncate / partial
copy): more than 64 projects in the set, a destination name longer than
32 characters, a directory with more than 256 entries, or an entry name
longer than 32 characters. Nesting deeper than 6 folders also fails.

## Measured

🟡 Menu grow and row placement match PROJECT's stock table
(`0x400cbcac` / `0x400cc308`); same recipe as PROJ SETUP on SYSTEM.

🟡 Dir-scan borrow/restore follows REMIX SWITCH
(`docs/contributing/FAILURE_MODES.md`). Copy uses stock `DIRSCAN`
(`0x4007f598`), mkdir (`0x46c8240a`), and file copy (`0x40016388`) from
an engine job (type 21), posted like REMIX SWITCH's deferred load.
Failed-copy cleanup uses stock's recursive walker (`0x40090a14`, mode 0).

## On the unit

Flashed with CF card update and tested on the author's MKII (17 Oct 2026).
BUILD of that flash was not recorded. Tried: project list + LEVEL scroll,
stock default name / FUNC+CUE with RND NAME, copy, and OPEN IT? → load.
Not tried on the unit: a project with many files (near/over 256 entries
per directory), a name of 32 characters or more, or copying while the
sequencer plays.

## Open

- Cross-set copy (v1 is the mounted set only).

## Gates

`make check REMIX=copy-project` (test remix). Port gate:
`tools/verify/verify_copy_project.py` — stages SRC, copies to the stock
default name under a pinned RTC, checks `project.work` and the rest of
the folder.
