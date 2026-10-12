
| ---- hooks.s: the two load-path detours and the per-remix table --------------
|
| Both sites are on the engine task's load path, before the banks are read:
|   0x40085342  LOAD PROJECT, before the empty-project init 0x400909d8:
|               `moveq #27,%d1 / movel %d1,%fp@(-570)`
|   0x40084d4a  the bank load every bank but the current (the power-up's
|               path too): `mvzb 0x80000002,%d1`
| Each stub keeps every register, calls brain_load and replays what the
| detour displaced.

        .include "remix.inc"           | brain_fx, brain_fx_n (manifest.py fx_inc)

| MODE DEFAULTS' view table, or 0 when that module is not in the remix
| (Linked.defsyms resolves MODEDEF_TABLE).
        .section .rodata
        .balign 4
        .globl  brain_modedef
brain_modedef: .long MODEDEF_TABLE
| LOAD METER's lm_stats (modules/load-meter), 0 without that module (LM_STATS defsym)
        .globl  brain_lm
brain_lm: .long LM_STATS
| its DSP figures (lm_dsp), 0 without it (LM_DSP defsym)
        .globl  brain_lmd
brain_lmd: .long LM_DSP

        .text
        .globl  brain_on_load, brain_on_bankload
brain_on_load:
        lea     %sp@(-16),%sp
        movem.l %d0-%d1/%a0-%a1,%sp@
        jsr     brain_load
        movem.l %sp@,%d0-%d1/%a0-%a1
        lea     %sp@(16),%sp
        moveq   #27,%d1
        move.l  %d1,%fp@(-570)
        rts
brain_on_bankload:
        lea     %sp@(-16),%sp
        movem.l %d0-%d1/%a0-%a1,%sp@
        jsr     brain_load
        movem.l %sp@,%d0-%d1/%a0-%a1
        lea     %sp@(16),%sp
        mvz.b   0x80000002,%d1
        rts

| ---- SAVE PROJECT: card.work -> card.strd --------------------------------------
| The project store 0x4008ee74 is called from 0x40085642, 0x400856dc and
| 0x40085780; KITS hooks its entry. Each call site jumps here: copy, then
| tail-call the store with the caller's arguments (PLOCKS P2's form).
        .globl  brain_on_store
brain_on_store:
        lea     %sp@(-16),%sp
        movem.l %d0-%d1/%a0-%a1,%sp@
        jsr     brain_card_store
        movem.l %sp@,%d0-%d1/%a0-%a1
        lea     %sp@(16),%sp
        jmp     0x4008ee74

| ---- the engine task's job switch --------------------------------------------
|   0x4008485e  `mvzb %a2@,%d0 / moveq #45,%d1 / cmpl %d0,%d1`, then bcss
|               0x4008484e (a type above 45 goes back to the receive) and the
|               jump table. Type 0x41 runs brain_job and goes back to the
|               receive; every other type takes the stock path.
        .globl  brain_on_job
brain_on_job:
        mvz.b   %a2@,%d0
        cmp.l   #0x41,%d0
        bne.s   1f
        lea     %sp@(-16),%sp
        movem.l %d0-%d1/%a0-%a1,%sp@
        jsr     brain_job
        movem.l %sp@,%d0-%d1/%a0-%a1
        lea     %sp@(16),%sp
        jmp     0x4008484e
1:      moveq   #45,%d1
        cmp.l   %d0,%d1
        jmp     0x40084864

| ---- the debug log: the MIDI thread's handler call -----------------------------
|   0x40005572  `moveal %a2@(0,%d0:l:4),%a0 / jsr %a0@`: the handler for the
|               message (status>>4 in d0), with the message pointer and 0
|               pushed. The stub keeps its own return in a slot (the MIDI
|               thread is the only caller), calls the handler as stock does,
|               then logs the message and what the handler left.
        .globl  brain_on_midi
brain_on_midi:
        move.l  %sp@+,brain_midi_ret
        lea     brain_midi_msg,%a1      | the message as it came in: a handler
        move.b  %a0@,%a1@               | may overwrite it (program change does)
        move.b  %a0@(1),%a1@(1)
        move.b  %a0@(2),%a1@(2)
        moveal  %a2@(0,%d0:l:4),%a0
        jsr     %a0@
        lea     %sp@(-16),%sp
        movem.l %d0-%d1/%a0-%a1,%sp@
        pea     brain_midi_msg
        jsr     brain_midi_log
        addq.l  #4,%sp
        movem.l %sp@,%d0-%d1/%a0-%a1
        lea     %sp@(16),%sp
        move.l  brain_midi_ret,%sp@-
        rts
        .data
        .balign 4
brain_midi_ret: .long 0
brain_midi_msg: .long 0
        .text

| ---- the CTRL pages' knob labels (MIDI_TRACKS.md section 6) ---------------
| Detour at 0x4004e474 (`movel %fp@(-56),%sp@-` / `bras 0x4004e480`), the
| CTRL page drawer's label push for slot %a4 of descriptor %fp@(-80):
| brain_klabel(P, slot, stock label) chooses the label, which is pushed in
| its place before the draw call's other arguments at 0x4004e480. The
| drawer keeps %d0, %d1 and %a0 live across the site.
        .globl  brain_klabel_hook
brain_klabel_hook:
        lea     %sp@(-12),%sp
        movem.l %d0-%d1/%a0,%sp@
        move.l  %fp@(-56),%sp@-
        move.l  %a4,%sp@-
        move.l  %fp@(-80),%sp@-
        jsr     brain_klabel
        lea     %sp@(12),%sp
        move.l  %d0,%a1
        movem.l %sp@,%d0-%d1/%a0
        lea     %sp@(12),%sp
        move.l  %a1,%sp@-
        jmp     0x4004e480

| ---- CREATE EMPTY PROJECT ---------------------------------------------------
| Detour at 0x40085176 (`jsr 0x40096f24`), right after the empty project's
| RAM is set up (0x400909d8) in the new-project job: brain_new_project puts
| the card's project settings on the settings bytes, then the displaced call
| runs with the return address the site gave.
        .globl  brain_on_newproj
brain_on_newproj:
        lea     %sp@(-16),%sp
        movem.l %d0-%d1/%a0-%a1,%sp@
        jsr     brain_new_project
        movem.l %sp@,%d0-%d1/%a0-%a1
        lea     %sp@(16),%sp
        jmp     0x40096f24

| ---- LEFT inside a BRAIN sub-list goes back to BRAIN's top list ------------
|   0x40064e98  `move.l #0x400cbd8c,%d2`: the LEFT key's handler puts the
|               focus on the root list (0x400cbda8 = the root list's
|               descriptor) and ends at the redraw 0x40065060. With the focus
|               in the list, the root selection on BRAIN (index 4, 0x400cbd98)
|               and a sub-list showing (brain_list's rows pointer, +24, is not
|               brain_top_rows), the pane goes back to the top list and the
|               focus stays; otherwise stock's move runs.
        .text
        .globl  brain_on_left
brain_on_left:
        move.l  0x400cbda8,%d0
        cmp.l   #0x400cbd8c,%d0         | focus already on the root
        beq.s   1f
        moveq   #4,%d1
        move.l  0x400cbd98,%d0
        cmp.l   %d1,%d0                 | the root selection is not BRAIN
        bne.s   1f
        lea     brain_list,%a0
        move.l  %a0@(24),%d0
        cmp.l   #brain_top_rows,%d0     | the top list is already showing
        beq.s   1f
        clr.l   %sp@-
        jsr     brain_menu_back
        addq.l  #4,%sp
        jmp     0x40065060
1:      move.l  #0x400cbd8c,%d2         | stock's instruction, then on
        jmp     0x40064e9e
