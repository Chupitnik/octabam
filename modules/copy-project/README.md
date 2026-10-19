# COPY PROJECT

Duplicate a project folder on the CompactFlash card without loading it
first.

**MAIN MENU → PROJECT → COPY PROJECT** (directly under SAVE TO NEW) lists
every project in the mounted set, asks for a name, confirms, and copies
the source folder byte-for-byte into a new folder — stock project files
and module extras alike. Stock SAVE TO NEW writes a fresh project and
drops those extras. After a successful copy it asks whether to open the
copy (stock LOAD PROJECT).

**Sidecars** = non-stock files modules leave in the project folder
(`kits.work` / `kits.strd`, `scenes.work`, `brain.work`, `p2lkNN.work`,
`BUFFERS/`, …). A folder copy keeps them; SAVE TO NEW does not. On a
live card set they show up next to `project.work` / `bankNN.work` (e.g.
`261018 IDLEYARD`, `!DEF_TMPL`).

## Use

1. Mount the set that holds the source project (CHANGE set if needed).
2. PROJECT → COPY PROJECT.
3. Pick the source from the list (folders with `project.work` /
   `project.strd`; `AUDIO` is skipped). LEVEL scrolls (STORE + this
   module: gated on this list's callbacks only). List rows show the
   first 16 characters of the name (LISTPOP sizes to the longest label;
   a full 32-char name empties the panel); the copy still uses the full
   folder name.
4. Edit the suggested name (`PROJECT YYMMDD`, or FUNC+CUE with RND NAME)
   and YES. The name editor itself caps at 32 characters.
5. Confirm `COPY SRC -> DST?` (same 16-char display cap on the line).
6. On success: `PROJECT COPIED` / `OPEN IT?` — YES loads the copy, NO
   leaves the live project as it was. `COPY FAILED` on error; a partial
   destination folder is removed so a retry does not hit
   `NAME ALREADY IN USE!`.

This is a card-to-card copy of whatever is on disk (including unsaved
`.work` cache). SAVE PROJECT on the source first if you want the last
saved state only.

Limits that refuse with `COPY FAILED` (no silent truncate / partial
copy): more than 64 projects in the set, a directory with more than 256
entries, an entry name longer than 32 characters inside the tree, or
nesting deeper than 6 folders.

## Measured

🟡 Menu grow and row placement match PROJECT's stock table
(`0x400cbcac` / `0x400cc308`); same recipe as PROJ SETUP on SYSTEM.

🟡 Dir-scan borrow/restore follows REMIX SWITCH
(`docs/contributing/FAILURE_MODES.md`). Copy uses stock `DIRSCAN`
(`0x4007f598`), mkdir (`0x46c8240a`), and file copy (`0x40016388`) from
an engine job (type 21), posted like REMIX SWITCH's deferred load.
Failed-copy cleanup uses stock's recursive walker (`0x40090a14`, mode 0). Gated under the port: a source directory with more than 256 entries forces `COPY FAILED`, removes the partial destination, leaves SRC intact, and a retry with the same name does not read `NAME ALREADY IN USE!`.

## On the unit

Flashed with CF card update and tested on the author's MKII (17 Oct 2026).
BUILD of that flash was not recorded. Tried: project list + LEVEL scroll,
stock default name / FUNC+CUE with RND NAME, copy, and OPEN IT? → load.

U1 (`LATEST U1`, 19 Oct 2026): a long project name stretched the pick
list until nothing drew — fixed by the 16-char display cap (full name
still copied). Name editor >32 is unreachable (form cap). Sidecars
present on the author's DEFAULT set (`kits.work`, `scenes.work`,
`brain.work`, `p2lk*`, `BUFFERS/`). Not tried: a directory near/over 256
entries, or copying while the sequencer plays.

## Open

- Cross-set copy (v1 is the mounted set only).

## Gates

`make check REMIX=copy-project` (test remix). Port gate:
`tools/verify/verify_copy_project.py` — stages SRC, copies to the stock
default name under a pinned RTC, checks `project.work` and the rest of
the folder.
