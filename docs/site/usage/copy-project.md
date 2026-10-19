Duplicate a project folder on the CompactFlash card without loading it
first — stock files and module extras (`kits.work`, `scenes.work`,
`brain.work`, `p2lk*`, `BUFFERS/`, …). Stock SAVE TO NEW does not keep
those extras.

## Access

MAIN MENU → PROJECT → COPY PROJECT (directly under SAVE TO NEW). Mount
the set that holds the source first (CHANGE set if needed).

## Operation

1. Pick the source from the list (folders with `project.work` /
   `project.strd`; `AUDIO` is skipped). LEVEL scrolls a row per detent.
   Rows show the first 16 characters of the name; the copy uses the
   full folder name.
2. Edit the suggested name (`PROJECT` + date). With RND NAME in the
   remix, FUNC+CUE fills `YYMMDD` + two pool words. The name editor
   caps at 32 characters.
3. Confirm `COPY SRC -> DST?`.
4. On success: `PROJECT COPIED` / `OPEN IT?` — YES loads the copy (stock
   LOAD PROJECT), NO leaves the live project as it was. `COPY FAILED`
   on error; a partial destination folder is removed.

This copies whatever is on disk (including unsaved `.work` cache). SAVE
PROJECT on the source first if you want the last saved state only. v1
copies within the mounted set only. Also refuses (no silent truncate)
when the set has more than 64 projects, a directory has more than 256
entries, or an entry name inside the tree is longer than 32 characters.
