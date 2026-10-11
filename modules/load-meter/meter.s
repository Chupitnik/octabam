| LOAD METER -- the frame interrupt's share of the ColdFire, for BRAIN's
| LOAD view.
|
| The frame interrupt (vector 0x41, handler 0x4000aad0, ~2,756 a second at
| 362.8 us) is stamped at its entry and at its epilogue on DTCN3, DMA timer
| 3's free-running count (132 MHz bus clock; CF METER and the OS read it
| too). Each exit adds the interrupt's duration to a sum and keeps the
| longest; every WIN counts (0.5 s) the window is published to lm_stats and
| a new one starts. BRAIN's LOAD view reads lm_stats (its LM_STATS defsym)
| and shows the share and the longest interrupt.
|
| Interrupts are masked at level 5 inside the handler, so the stamps and
| the sums are not shared with anything that can preempt them.

        .equ    DTCN3,  0xfc07c00c
        .equ    WIN,    66000000                | 0.5 s at 132 MHz
        .equ    BODY,   0x4000aad8              | the handler after its displaced prologue

        .text
| ---- 0x4000aad0: the handler's first instructions, then the entry stamp --
        .globl  lm_in
lm_in:
        lea     %sp@(-252),%sp                  | displaced: lea %sp@(-252),%sp
        movem.l %d0-%d7/%a0-%a6,%sp@            | displaced: moveml %d0-%fp,%sp@
        move.l  DTCN3,%d0                       | every register is saved: d0 is free
        move.l  %d0,lm_t0
        jmp     BODY

| ---- 0x4000d9a6: the epilogue, every exit path --------------------------
| d0-a6 are saved at (sp) and restored below, so every register is free.
        .globl  lm_out
lm_out:
        move.l  DTCN3,%d2                       | now
        move.l  %d2,%d0
        sub.l   lm_t0,%d0                       | this interrupt's duration
        add.l   %d0,lm_sum
        addq.l  #1,lm_cnt
        cmp.l   lm_max,%d0
        bls.s   1f
        move.l  %d0,lm_max
1:      move.l  %d2,%d1
        sub.l   lm_w0,%d1                       | the window so far
        cmp.l   #WIN,%d1
        bcs.s   2f
        lea     lm_stats,%a0                    | publish: seq, window, sum, count, longest
        move.l  %d1,%a0@(4)
        move.l  lm_sum,%a0@(8)
        move.l  lm_cnt,%a0@(12)
        move.l  lm_max,%a0@(16)
        addq.l  #1,%a0@                         | the seq last: a reader sees a whole window
        move.l  %d2,lm_w0
        clr.l   lm_sum
        clr.l   lm_cnt
        clr.l   lm_max
2:      movem.l %sp@,%d0-%d7/%a0-%a6            | displaced
        lea     %sp@(252),%sp                   | displaced
        rte                                     | displaced

        .data
        .balign 4
        .globl  lm_stats
lm_stats: .long 0, 0, 0, 0, 0                   | seq, window counts, sum counts, interrupts, longest counts
lm_t0:  .long   0
lm_w0:  .long   0
lm_sum: .long   0
lm_cnt: .long   0
lm_max: .long   0
