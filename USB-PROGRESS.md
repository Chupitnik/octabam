# USB merge — plan and progress (uncommitted working notes)

Scope per Sam (15 Oct): confirm phase 1 works, then stop. Phases 2–4
(run-time settings, NEXT_BOOT) go to another agent. Phase plan:
docs/proposals/USB.md section 8. The branch implements **phase 1 only**:
settings are `Apply.BUILD` — a LAYOUT/INPUTS change takes effect at the
next build, not on the unit. Sam's reaction to that ("that's crazy") was
the phase confusion, not a design change.

## 16 Oct 2026: the gate failures, resolved

Worktree `/Users/sambanks/git/octabam/.claude/worktrees/usb-module`, branch
`usb-runtime`. Each gate passed on its own, octabam-bus unless named:

- verify_brain: the store writes one SETTINGS record per module (usb's,
  keys 7-12, beside brain's); the check reads brain's record.
- verify_usb_align, verify_usb_post: the ring slot is 88 B (tracks 64,
  MAIN, CUE, MASTER); the harness and the gate parsed 80.
- verify_loadmeter: the publish block is `x:$206 + $100` (stock's banks
  alternate $2600/$4600, A P:$54/$64; the literal $2700 was right every
  other frame: good 43178 / bad 43179). Words paid by two no-op
  `and #>$ffffff` and the OTOP lookup through `x:(r1+n1)`; payload A FREE
  3, the LM_BURN builds fit.
- verify_usb_in (usb-io-tracks-main-cue-ab and the octabam-bus runtime
  rows): `in_slot` had lost `lsll #5` (packet buffers 3 B apart); the
  inject reads `r6 + $320` (the literal $6320 read 0, a literal $2320 is
  one bank); one 24-bit word per channel cannot cross the 16-bit host port
  (hi/lo halfwords, two passes); `brclr` displacements written as numbers
  (TRAPS trap-brclr); in_build reads pair frames at 8 B; the runtime rows
  boot CS1 from `cs1_boot(image)` (a zero CS1 sent input gain 0, TRAPS
  trap-cs1-zero). The merged commit 1915559f, built in a second worktree,
  passed the same gate and split the branch's image from the tools.

### Where it stands (end of the 16 Oct session)

Nothing is committed past 6dc32fbd; 28 paths are modified or staged in this
worktree (`git status --short`). `make check REMIX=octabam-bus` was running
detached (log `/tmp/check_bus2.log`, ends with a `check=<rc>` line): 1,285
checks passed and 0 failed up to the LOAD METER gate, which was running.
Gates not yet seen green in that run: the loadmeter tail, then the final
`check=` line. If the log is gone, rerun it (about 2 h).

Fixes made after the first draft of this section, all in the tree:
- `tools/remix/selftest.py`: `usb-out-tracks-main-cue` is expected to give
  up PLATE REV (its remix harvests it for USB's inject).
- `tools/verify/verify_usb.py`: `cs1_boot(image)` and the `template`
  argument of `cs1_block` (a zero CS1 booted stock's input gain as 0).
- `tools/verify/verify_usb_in.py`: runtime rows use `cs1_boot`; a remix
  with no CS1 entries skips them.
- `modules/usb/usbaudio_in.s`: `in_slot` restored `lsll #5`; two-pass
  `in_build`; per-IN transfer length (192 B pair, 320 B ABCD).
- `modules/usb/rx_inject.asm`: two passes, `brclr` displacements numeric.
- `modules/load-meter/dsp_meter.asm`: publish block `x:$206 + $100`.
- Traps written: trap-brclr, trap-cs1-zero, trap-in-slot (TRAPS.md and
  AGENTS.md).

### RESOLVED: octabam-core and the inject (Sam chose option 1, 16 Oct)

`make reach` failed `octabam-core:image` ("nothing is harvested, so there
is nowhere to place USB"): core keeps all 14 effects and has no donor
region, and no effect may leave (old projects use them). Fix:
`Module.dsp_if(fixed)` (schema.py; applied in registry.bound()) drops USB's
DSP section when IN is fixed OFF in the image (pinned, Apply.BUILD, or a
remix without BRAIN); `octabam-core` pins `IN` OFF; `cfgsel` takes a
pinned setting's built-in value (descriptors.py emits `CS1_USB_<n> = -1`,
usbaudio.s `CFG_READ`); `verify_usb_in` skips the runtime rows when IN has
no CS1 entry. octabam-core builds with no DSP region placed. IN stays
switchable on the unit in octabam-bus and octabam-inserts.

Remaining, in order:
1. `make reach RUN=1 KEEP=1` on HEAD (octabam-core was red before the dsp_if fix; inserts not yet seen).
2. `make docs` and `verify_docs` (remixes/test/README.md may be stale).
3. Commit (end messages with `Co-Authored-By: Claude Code <noreply@anthropic.com>`),
   rebase onto origin/main, `make reach RUN=1 KEEP=1`.
4. `scripts/refhash.sh check` (build_bus.py `_RT_SYMS` is a build change;
   re-save only after the artifacts are shown identical).
5. PR: list each command and result; state the 22-channel bInterval 1
   choice and the LOAD METER payload A margin (3 words free on octabam-bus).
   Body ends with `🤖 Generated with [Claude Code](https://claude.com/claude-code)`.
6. Merge (shoot-first); Sam hardware-tests; CHANGELOG entry only after a
   flashed image.

Rules: this worktree only; verifiers run in sequence (shared
`out/platform`); never pkill by pattern; long runs detached.

## State (16 Oct 2026): phases 2–4 landed on this branch

Sam's operative directive became "power through the phases, hardware test
at the end", so the redesign (docs/proposals/USB.md section 8) is
implemented here, not by another agent:

- Six NEXT_BOOT, early settings (manifest keys 7–12): TRACKS
  OFF/PRE/POST (default PRE), MAIN (default on), CUE (default on),
  MASTER (default off), IN OFF/AB/CD/ABCD (default off), MIDI (default
  on). Pair rule: IN ≠ OFF requires TRACKS ≠ OFF or MAIN or CUE or
  MASTER (brain.c usb_pair_ok; cfgsel forces IN off on a bad block).
- cfgsel reads the CS1 early block at the USB controller init, so the
  settings stand from the first enumeration; the served channel count is
  16 × TRACKS(≠ OFF) + 2 × (MAIN + CUE + MASTER), 22 at most at
  bInterval 1 (125 µs).
- Runtime fixes: the producer's .Lf_nomaster stride (TRACKS OFF walked
  24 of 88 bytes), TRACKS OFF's early return, cfgsel's MAIN/CUE count,
  the four responder stubs' moveml frame (mode-2 trap), MASTER reading
  arena region 7.
- Gates reworked for the six-setting schema: verify_usb (16 runtime rows
  via the CS1 block; the tap check is region-7-aware), verify_usb_in
  (runtime rows), verify_usb_post (boots TRACKS POST via the CS1 block).
- 16 Oct gate fixes: brain.s regenerated (real drift), the align remix
  harvests PLATE REV, verify_usb_in encodes Option indices,
  verify_usb_post's CS1 boot + empty-run report.

## Branch

`usb-module` in worktree `.claude/worktrees/usb-module`.
da2def4d (base) + b2918c99 (merge) + aec9d3a5 (per-unit symbol tables).
da2def4d is stale vs origin/main (missing #721/#722/#723) — rebase before PR.

## Done (phase 1 complete)

- `modules/usb/`: one module (key USB), four old modules merged; LAYOUT
  (TRACKS MAIN CUE / TRACKS / MASTER / MAIN CUE / MAIN / TRACKS POST / OFF,
  default TRACKS MAIN CUE) and INPUTS (AB / CD / ABCD / OFF, default AB),
  both Apply.BUILD; INPUTS on with LAYOUT OFF refused. Claims at the ABCD
  maximum whenever INPUTS is on.
- 33 remixes re-keyed to USB; docs updated; verify_docs clean.
- Gates green: `make check` on usb-io-tracks-main-cue-ab, usb-midi,
  usb-out-tracks; `scripts/refhash.sh check` 24/24 (fresh baseline at
  da2def4d, `REFHASH_LOCAL=1` semantics).
- `make reach RUN=1 KEEP=1`: 7/8 suites ok. cfmeter-post M6a fails
  identically on pure main (pre-existing). octabam-inserts loadmeter
  fails — see below.

## The blocker

`modules/load-meter/manifest.py` declares
`Gate(..., remix_arg=False)`, and `verify_loadmeter.py` defaults `remix`
to `"octabam-bus"` — so the octabam-inserts loadmeter gate builds and
tests an **octabam-bus** image, at **BUILD=0** (`verify_loadmeter.py`
`build()`: `env.setdefault("BUILD", "0")`). That image hangs in
LOAD PROJECT. Pure main's bus passes (cached pass at 85e5bdbe), so the
branch broke it.

Measured:

- Load matrix (all octabam-bus, branch tree):
  BUILD=0 + LAYOUT MAIN CUE — **hang**;
  BUILD=0 + LAYOUT OFF — pass;
  BUILD=1 + MAIN CUE — pass;
  BUILD=79 + MAIN CUE — pass.
- 16 Oct, `make check REMIX=octabam-bus` on the redesign tree: LOAD METER
  still fails (publish: good 43178, bad 43179; otop: fewest 0 samples)
  and the gate's emu run hangs at 99% CPU (killed by PID after 6:56).
- The tag changes the server panel names: `FULLNAME["REVERB SERVER"] =
  b"BusVerb" + BUILD_TAG`, same for BusDelay (build_bus.py:330–334).
  BUILD=0 vs BUILD=1 images differ ONLY in: the loader's payload table in
  the stock region (image offset 0x10fb1c: packed size 0x1241f vs
  0x12420, two CRCs), the packed-stream name bytes (offsets 0x12015c and
  0x1207d7–end), and total length by 1 byte. Boot passes in both, so the
  loader table is inert.
- Names in DRAM (dumped from the built runtime): "BusVerb1" at
  0x40AC3C59, "BusDelay1" at 0x40AC3DED (13-byte NUL-padded fields in the
  six descriptor clones — REVERB SERVER, DELAY SERVER, SEND, SPECTRUM,
  CHARACTER, MODULATION).
- Watch-read on 0x40ac3c59..0x40ac3c65 during the **passing** b1 boot +
  LOAD + open pane: **zero reads**. The ColdFire never reads the name in
  the passing run. Open question: if the name is never read, its value
  cannot drive the hang — the b0 watch run decides.
- Hang signature (b0): engine never returns to the queue receive
  (0x4008484e) after a brain_on_job at instruction ~1.33e9; main spins
  `bras 0x4001fc9c`; ATA re-reads lba 2301/2333/22885; DSPs healthy.

## In flight

- pid 73932: b0 + MAIN CUE load with `--watch-read 0x40ac3c59,13
  --watch-pc 0x4008484e` → /tmp/watch_b0_name.log.
- Next discriminators if b0 also shows zero reads: BUILD=00 build
  ("BusVerb00" — 9 chars, atoi would still be 0); `--watch-mem` on the
  name field (writes, incl. read-modify-write); DMA paths the read watch
  cannot see.

## After the fix

1. Rebuild the gate and re-run the octabam-inserts loadmeter gate to
   green (gate cache: `make reach RUN=1 KEEP=1` on the rebased tree).
2. Rebase `usb-module` onto origin/main.
3. `make reach RUN=1 KEEP=1`, list each command and result in the PR body
   (/tmp/usb_pr_body.md — update the loadmeter row).
4. Open the PR, merge after green (shoot-first).
5. Cleanup: remove scratch `remixes/test/bus-layout-off/`; throwaway
   worktrees `usb-cfmeter-main`, `usb-bisect-b29`.

## Handover to the phases 2–4 agent

docs/proposals/USB.md sections 5 and 8: run-time LAYOUT (all six
producers + boot-read dispatch, 5.1), run-time INPUTS (parametric inject,
5.2), descriptor sets + responder selector (phase 4), then reach +
remix READMEs + hardware round (phase 5). Open items in section 9.

## Codex takeover — 15 Oct 2026

- Continued in the existing worktree; left the main checkout untouched.
- Fetched origin/main and rebased successfully onto 2d1cbcf4. New commits:
  b3dc3d7b (merge), b2a35a51 (per-unit symbol resolution).
- Moved the untracked bus-layout-off scratch remix to
  /tmp/codex-usb-bus-layout-off-scratch. It failed both the docs index and
  the declared stock-effect sacrifice check; not a shipping regression.
- verify_docs passes after that removal. Manual site pages now name the
  merged USB module and explicitly distinguish phase 1 from on-device USB
  configuration (still not implemented).
- Rebased refhash: ALL 24 CASES BIT-IDENTICAL against merge-base 2d1cbcf4.
- In progress: reach /tmp/codex-usb-reach.log; identity
  /tmp/codex-usb-identity.log. The initial reach snapshotted the scratch
  remix before its removal, so its docs/selftest failures need a rerun.
- The b0 name-read watch completed: load timed out, no logged name reads.
  This does not establish the cause of the load failure.
- New hypothesis, not yet proven: packed runtime residue in BRAIN .bss.
  Old bus runtime .bss begins 0x40ad4cf8; stage is 0x40ad5000 and holds
  0x1241f bytes, including UI scratch, job_args and the start of out.
  BRAIN explicitly leaves .bss dirty and marks read-before-write state DATA;
  therefore overlap alone is not a bug: a specific bad read must be shown.
- Controlled diagnostics under /tmp/codex-usb-diag: recreated the old b0
  image from /tmp/rt_b1/runtime.raw (despite its name, contains BusVerb0),
  obtaining the handoff's exact 1,187,683-byte image and 0x1241f packed blob.
  b0-stage-outside-bss.bin changes only table.stage to 0x4905e000, beyond
  the linked .bss. Both variants run the same staged project, with separate
  emulator processes, no card writeback. Logs: original-stage.log and
  relocated-stage.log. Await results before changing source.

### Fresh diagnostic results

- CPU-only original stage and relocated stage both complete LOAD PROJECT
  at exactly instruction 2235061050 (20086.1 ms after the post). Logs:
  /tmp/codex-usb-diag/b0-cf-only.log and
  /tmp/codex-usb-diag/b0-stage-outside-bss-cf-only.log.
  No demonstrated CF load dependence on the stage residue; the DSP-enabled
  comparison is still pending. Do not fix the loader on overlap alone.
- A FRESH main bus BUILD=0 run, same emulator and exact same staged card,
  completes at instruction 2256575080 (20243.8 ms after post), confirming
  the previously cached baseline result. Image copied from the inherited
  usb-cfmeter-main tree; log /tmp/codex-usb-diag/main-stage.log.
- Bounded b0 diagnostic (25000 ms load budget), --pc-ring 2048 and engine
  TCB/stack dumps: /tmp/codex-usb-diag/b0-stack.log. Pending.
- Rebased cfmeter-post now fails the USB vendor-counter no-overrun assertion
  (4 overruns); this is distinct from the handoff's M6a failure. Fresh
  identity says cfmeter-post IMAGE IDENTICAL, report-only changes. Do not
  label this a new firmware regression from the USB merge.
- Rebased octabam-bus per-remix gates pass; the full reach is still running.

### Diagnosis narrowed further

- The full DSP-enabled stage relocation timed out at 90000 ms as well.
  Stage residue is falsified as the proposed explanation for this hang.
- Exact DSP stepping (`--dsp-lazy 0`) also times out at 25000 ms, with
  the same final dispatch history / 13066 forces / 25267 dispatches as
  default lazy stepping. Lazy batching is not the explanation.
- Engine TCB+stack captured by b0-stack.log: saved SP 0x460dd8ac,
  exception-frame PC 0x40014d34. Stack includes stock close 0x40016852,
  stock logger 0x4001ffe4, and static slot-load error logger return
  0x400908ca (format at 0x400b7933: Couldn't load STATIC[%d] ...).
  The foreground CPU is sys in ATA IRQ at 0x4001551e/24/26: read alternate
  status, copy byte to CCR, BPL until DRQ bit 3 is set. This is a tighter
  finding than the handoff's focus on brain_on_job (which handles all jobs).
- Debug port built ONLY under /tmp/codex-usb-port-debug (copy of tracked
  tools plus tapeecho/cpu.c, OT_VENDOR set to shared vendor read-only).
  Adds one diagnostic to Rtos::peripheralRead when currentPc=0x4001551e
  and DRQ is clear: logs ATA pending IRQ, last 16 commands, firmware
  remaining count and pointer. No repo emulator source changes.
  Run /tmp/codex-usb-diag/b0-card-state.log is in progress.

### Concrete ATA finding

- /tmp/codex-usb-diag/b0-card-state.log: at 13053.981 ms, alternate
  status 0x50 (DRQ clear), firmware 0x46c8c592=1 (one sector still to
  send), 0x46c8c58a=2, pointer 0x4eceb200, command 0x30. Last command is
  WRITE LBA 2301 count 1, issued by engine at 0x40014ce4. The model has
  committed it (5705 total sectors written); IRQ due=0 and line clear.
- Corrected interpretation of saved engine PC 0x40014d34: this is the
  FIRST-SECTOR WRITE loop, not a read loop. The final word can already
  be committed while the engine is preempted before it updates pointer
  0x46c8c594 (+512) and remaining-byte 0x46c8c592 (--), at 0x40014d38..52.
  ATA IRQ at 0x4001551e then polls DRQ with that stale count. This is the
  direct observed interruption window, not evidence against USB names.
- Diagnostic --ata-latency 16 with only a 25000 ms budget times out while
  STILL PROGRESSING (19637 forces / 37530 dispatches, main at 0x4000bdfe),
  not the identical 13066/25267 stuck signature. It is not a valid claim
  that doubling card latency fails the same way; budget needs extending.
- An experimental image retains stock's existing SR=0x2700 through the
  first sector and its bookkeeping: 0x40014cf2 `move.w d0,d3` retains the
  saved SR, detour 0x40014d58 restores SR then replays the return epilogue.
  First experiment appended its stub at 0x40122364, then panicked with
  vector 0x0b at that address on the first WRITE. Do not use boot append
  space for persistent code. Retest puts the 10-byte stub at 0x4105e000,
  beyond .bss inside the reserved arena, using --preload. Log:
  /tmp/codex-usb-diag/b0-first-sector-guard-dram.log (pending).
- Identity complete: 34 identical, 28 report-only, 3 changed images
  (octabam-bus/core/inserts), 2 unbuilt both sides (waveload/-port).
- Rerunning the shared half without scratch remix:
  /tmp/codex-usb-shared-clean.log. Original reach is still running.
