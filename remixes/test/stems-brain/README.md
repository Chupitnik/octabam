# `stems-brain` — STEM REC under PROJECT with BRAIN

Coexistence proof: STEM REC grows `MAIN MENU > PROJECT` (STEMS row);
BRAIN grows the root's fifth category. Previously both grew the root and
the ledger refused the pair.

## What is in it

- **STEM REC** (yvesrosius) — `MAIN MENU > PROJECT > STEMS`.
  [The module page](../../../modules/stems/README.md).
- **BRAIN** — root category; pulls **REMIX SWITCH** with it.
- **MEMORY** — required by STEM REC's DramRegions.
- **The 14 stock effects**, listed so the FX2 chooser is stock's.

## Status

Port: builds without a root-grow collision; `verify_stems_menu.py
stems-brain` walks PROJECT > STEMS. Not hardware-flashed.

## Build

```bash
make image REMIX=stems-brain BUILD=1
python3 tools/verify/verify_stems_menu.py stems-brain
```
