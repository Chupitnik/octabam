# `load-meter` — LOAD METER

The frame interrupt's share of the ColdFire, for BRAIN's LOAD view
(`modules/brain`: TOOLS > LOAD, and FUNC + LEVEL push on an MKII).

The frame interrupt (vector `0x41`, handler `0x4000aad0`) runs about
2,756 times a second, every 362.8 µs. LOAD METER stamps its entry and its
epilogue on DMA timer 3's free-running count (132 MHz bus clock), sums each
interrupt's duration and keeps the longest, and every 0.5 s publishes the
window to `lm_stats` (also `LM_STATS`): `{seq, window, sum, count, longest}`
in timer counts. BRAIN's view shows the share, the average and longest run
in µs, the interrupts a second, and the peaks since the view opened.

| site | stock | stub |
|---|---|---|
| `0x4000aad0` | `lea %sp@(-252),%sp / moveml %d0-%fp,%sp@`, the handler's prologue | `lm_in`: the prologue replayed, the entry stamp |
| `0x4000d9a6` | `moveml %sp@,%d0-%fp / lea %sp@(252),%sp / rte`, the epilogue | `lm_out`: the duration summed, the window published, the epilogue replayed |

The sites are BOOT TRACE's entry and CF METER's epilogue, so the ledger
refuses LOAD METER beside either by name. The share counts the frame
interrupt alone: the other interrupts and the tasks are not in it, and the
DSP cores' load is not measured here.

## Measured

Under the port with `--frame` (the frame timer without the DSP cores, 11
Oct 2026, `verify_brain`'s `load` cases): windows published every 0.5 s,
2,758 interrupts a second, 22% share, 82 µs a run. The port's timing is not
the chip's; the numbers on a unit are a hardware-round item.

## Gates

`tools/verify/verify_brain.py` (BRAIN's gate) runs the LOAD view on the
`brain` remix, which carries this module.
