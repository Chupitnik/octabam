# `persistent-buffers`

STORE and [PERSISTENT BUFFERS](../../../modules/persistent-buffers/README.md) on
the stock effects. `verify_persistent_buffers` covers symbols and a port save/load
round-trip.

**Not flashed** in this form. Hardware was on the author's personal `minimal`
remix, BUILD **R2–R6** (MKII, 16–17 Oct 2026); **current head is R6**.
PROJECT SAVE writes `BUFFERS/REC1.wav`..`REC8.wav` (no `!` prefix — `!BUFFERS/`
on R4–R5 left PATH in `project.work` but UI unassigned). R6 assigns the
recorder slots with the same slot-load bind as R3.
