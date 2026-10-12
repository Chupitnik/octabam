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
interrupt alone: the other interrupts and the tasks are not in it.

## The DSP cores

Each core measures its own busy time per frame and core 0 publishes both
cores' figures to the ColdFire in bytes it already sends. It is always on,
on both cores, whatever the tracks hold: no FX slot and no effect id.

**The meter** (`dsp_meter.asm`, both payloads; `DspHook`s with the same
stock words on each, and core 0's publish on payload A alone):

| hook | A | B | stock | does |
|---|---|---|---|---|
| frame start | `P:$8c` | `P:$74` | `move r5,x:>$206`, just after the frame wait | timer 0 (CLK/2) stamped; period = the advance since the last start; timer 0 armed the first time (TRM, TE; no interrupt enable) |
| frame end | `P:$54b` | `P:$340` | `move #>$421,r6`, before the jump back to the wait | busy = the advance since this frame's start; core 0 also counts ESAI TUE/ROE frames (SAISR read once a frame) |
| publish | `P:$2ec` | none | `move x:>$205,r0`, just after the MAIN pack | core 0 writes the 17 bytes below |

The figures live in the shared window at `X:$37F10-$37F2F` (`Claims.dsp_ranges`;
inside payload A's bank-track-4 FX2 slot past where any stock effect
writes, and in the range payload B's boot loop zeroes): core 0's block at
`$37F10`, core 1's at `$37F18`, so core 0 reads core 1's figures. The core
switch is the per-payload base literal (`YBase.ALWAYS`); no modifier
register is written (the address registers used are ones the stock code
right after each hook relies on being linear).

**The path to the ColdFire.** Measured under the port with `--block-dump`
(11 Oct 2026): what the DSPs send each frame is core 0's 512-word pull
(channel 1, the 256-word read-back of the eight tracks; channel 6, 128
words of MAIN and CUE at `0x80005e60`; channel 7, 128 words of the inputs)
and core 1's 256-word read-back. Every word is audio, so there are no spare
words. The packer (`P:$55a`) stores each sample's low word as `(sample &
$ffff) << 8`, so the low byte of every low word is always zero. Core 0 adds
17 bytes there, in MAIN's first 17 longs, right after the packing: the
ColdFire pulls the block before core 0's frame ends (measured: bytes
written at the frame end never arrived), so the busy and period sent are
the last finished frame's:

| byte | holds |
|---|---|
| 0 | `$a5` |
| 1 | core 0's frame count, low byte |
| 2-4, 5-7 | core 0 busy, period (timer-0 counts, high byte first) |
| 8-10, 11-13 | core 1 busy, period |
| 14, 15 | core 0 ESAI TUE frames, ROE frames (mod 256) |
| 16 | the sum of bytes 0-15, mod 256 |

`meter.s`'s `lm_scrub` (a detour at `0x4000d310`, the `tstl` before the
stock recorder reads MAIN) copies the bytes out and zeroes them, before the
recorder (`0x400068e4`), USB AUDIO OUT and STEMS read the buffer: every
reader sees stock's bytes; it runs once per frame interrupt (its pass
count equals the interrupt count, `verify_loadmeter` every), ahead of the
recorder's call at `0x4000d340` and USB AUDIO OUT's at `0x4000d9a0`. STEM
REC reads MAIN from the transfer routine (`0x40004b12`), outside that
order, so the two are refused together (`conflicts`) until measured. A record whose magic, sum, period (20,000 to
60,000 counts; nominal 36,256) or busy (at most its period) is wrong is
counted as bad and changes nothing. CF METER's page-lane route (an FX2
insert printing values as audio) was the alternative; it takes a track and
a slot, which this meter must not.

**The view** (BRAIN's TOOLS > LOAD): `CF` the frame interrupt's share now /
peak since open and its longest run; `DSP0` and `DSP1` each core's busy /
period as a % now / peak since open, and `M` on DSP0 the ESAI underrun and
overrun frames since open. `-` = no sample yet, `?` = only bad records
since the last refresh. The title row is `LOAD` and the refresh count. Lines
are cut to the box's width in pixels (`0x40012f30`). While the view is
closed nothing draws and the DSPs and the host port carry the same traffic
as with it open (`verify_loadmeter` closed); the scrub runs every frame
either way.

Credit: the view and its rules (bounded reads, `-` and `?`, the refresh
counter, the zero-cost close) follow Hamish Renton's ([SamplesAndSnacks](https://github.com/SamplesAndSnacks)) LOAD
view design for the Machinedrum. The code here is octabam's.

**Payload room.** 156 words on each payload. The only remix carrying LOAD
METER is the test remix `brain`: payload A 61 words free, payload B 623
(11 Oct 2026). A remix it does not fit is refused by the build's region
fit, by name.

**Calibration.** `LM_BURN=N` builds put a DO loop of N one-cycle `nop`s
at each core's frame end, before the busy stamp: N core cycles plus the
DO's setup, a constant that cancels between two burns. Timer 0 counts at
CLK/2, so N cycles add N/2 counts to busy. Under the port one frame's
busy cannot show that: the vendored emulator updates its timers every
2,048 instructions (`dsp56kEmu/timers.cpp`, `m_timerupdateInterval`), so
timer 0 reads in 1,024-count steps. A first gate run compared single
frames and saw a 1,000-cycle and a 2,000-cycle burn both add about 1,024
counts: the step, not the model. `lm_scrub` sums each core's busy over the
open window (`lm_dsum`); the frame length is not a multiple of 2,048
instructions, so the mean resolves below the step, and the gate compares
means. It reports each rise's error against N/2. The port counts
instructions, the chip cycles; the chip's ratio is a hardware-round item.

## Hardware round

Every % and µs here is from the port, whose cycle table and timer are not
the chip's:
- timer 0's rate (CLK/2) and the nominal period 36,256 counts;
- busy and period per core against a known load (a burn, as `LM_BURN`);
- the ESAI TUE/ROE counts at a forced underrun;
- MAIN, CUE and USB audio bit-identical with the meter on (a recording of
  MAIN, the USB stream);
- the view's text widths on the unit's LCD.

## Measured

Both DSP cores under the port (`verify_loadmeter`, 12 Oct 2026, project
BRAINPRJ, the view open for 4 s of emulated time): 89,783 good records and 2 bad
(the first frames, before timer 0 is armed) out of 89,785 frame
interrupts; core 0 busy 20,480 of a 36,864-count period, core 1 17,421 of
36,028; the view read `DSP0 57%/ 60% M0`, `DSP1 51%/ 54%`. The mean busy
rose by 508 and 1,504 counts on core 0, 500 and 1,503 on core 1, for
1,000- and 3,000-cycle burns (expected 500 and 1,500: errors +1.6%, +0.3%,
-0.1%, +0.2%). USB AUDIO OUT's ring was byte-identical with the publish on
and off; 9.0 host commands per frame interrupt with the view open and
closed.

Under the port with `--frame` (the frame timer without the DSP cores, 11
Oct 2026, `verify_brain`'s `load` cases): windows published every 0.5 s,
2,758 interrupts a second, 22% share, 82 µs a run. The port's timing is not
the chip's; the numbers on a unit are a hardware-round item.

## Gates

- `tools/verify/verify_loadmeter.py`: with both DSP cores under the port,
  the records arrive and pass, the view's DSP rows carry numbers, the
  scrub zeroes MAIN's byte every frame and runs once per frame interrupt,
  USB AUDIO OUT's ring is byte-identical with the publish on and off
  (`LM_NOPUB`), the closed view draws nothing and moves the same host
  traffic, nothing stock writes the claimed words but payload B's boot
  zero loop, and an injected burn (`LM_BURN` builds) raises each core's
  busy by N/2 counts.
- `tools/verify/verify_brain.py` (BRAIN's gate) runs the LOAD view on the
  `brain` remix, which carries this module.
- `tools/verify/verify_dspvectors.py`, `verify_twocore` and `make
  verify-bus` (audio bit-identity) for the DSP section.
