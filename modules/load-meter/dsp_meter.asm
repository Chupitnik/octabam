; LOAD METER, DSP side: each core's busy time per frame, published to the
; ColdFire in bytes it already receives.
;
; Three hooks (schema.DspHook): two with the same stock words on both
; payloads, and core 0's publish:
;   frame start  `move r5,x:>$206` (A P:$8c, B P:$74), just after the core
;                leaves its frame wait: stamp timer 0, period = the advance
;                since the last start.
;   frame end    `move #>$421,r6` (A P:$54b, B P:$340), the last work before
;                the jump back to the wait: busy = the advance since this
;                frame's start.
;   publish      `move x:>$205,r0` (A P:$2ec only), just after the MAIN pack
;                and before the ColdFire pulls this frame's read-back.
; Timer 0 (CLK/2) is armed here the first time a core passes the start hook
; (TCSR0 = TRM | TE, no interrupt enable: stock never writes the timers,
; tools/verify/verify_dspvectors.py's peripheral set).
;
; The figures live in the shared window at X:$37F10-$37F2F (Claims
; dsp_ranges), so core 1's reach core 0: core 0's at $37F10, core 1's at
; $37F18. Core 0 publishes 17 bytes into the low byte of the low word of
; MAIN's first 17 longs in the read-back it is about to send (x:$206 +
; $100, the ch 6 block; docs/firmware/DSP.md section 6c). Measured under
; the port: the ColdFire pulls the block before the frame ends, so the
; publish follows the pack (P:$2d5-$2eb), not the frame end.
; The packer (P:$55a) stores a sample's low word as (sample & $ffff) << 8,
; so that byte is zero in stock; the ColdFire takes the bytes and zeroes
; them again before any reader of the buffer runs (meter.s lm_scrub), so
; every consumer sees stock's bytes.
;
;   byte 0 $a5 (the magic)    1 core 0's frame count (low byte)
;   2-4 core 0 busy, 5-7 core 0 period, 8-10 core 1 busy, 11-13 core 1
;   period (timer 0 counts, high byte first)
;   14 core 0 ESAI TUE frames, 15 ROE frames (counts mod 256: SAISR read
;   once a frame; core 1 has no ESAI)    16 the sum of bytes 0-15, mod 256
;
; The core: the two `move #>...,a` before `cmp #>$38000,a` load the build's
; per-payload base (YBase.ALWAYS): payload A's half on core 0, B's ($38000)
; on core 1.
;
; Registers. Start hooks: a, x0, x1 and r1 are dead (each written before it
; is read in the stock code that follows: r1 at A P:$a8, B P:$7c); b is live
; at A's (the spin count stored at P:$92) and is not touched. r1 is the
; core's block, and m1 is linear there: the stock code right after moves
; through r1 (A P:$ae `move x:(r1)+,x0`, B P:$353 in func_34f). End hooks: a, x0, x1
; are dead; r6 is the block and r7 the SAISR pointer, both linear there
; (the stock copy right after runs `move x:(r6)+,x0 / move x0,x:(r7)+`),
; r7 is set by stock's next instruction and r6 takes the displaced
; `#>$421` last. No modifier register is written.
;
; OTOP, the cost of each effect call (a second record, below): four more
; hooks on the dispatcher, a start and an end around each FX1 and FX2 slot's
; calls (the pre-call, an init on a changed id, and the process call), each
; core timing its own slots with timer 0. A core's cost block at X:$37F40
; (core 0) / $37F50 (core 1): +0..+7 the last cost of each call of the
; frame in dispatch order (position 0's FX1, FX2, position 1's FX1, ...),
; +8 the call's start stamp, +9 the address of the next cost cell (set by
; the frame start hook, advanced by each end hook). Core 0 publishes one
; slot a frame, round robin over both cores' 16, in a second record.
;
; A core's block: +0 the start stamp, +1 busy, +2 period, +3 TUE frames,
; +4 ROE frames, +5 armed ($5a5a5a), +6 frames; core 0's at $37F10, core
; 1's at $37F18; core 0's publish keeps its sum at +$10 and a value at
; +$11.
;
; Every form has a stock site: move x:>abs,acc (P:$d0), move a1,x:>abs
; (P:$86), sub x0,a (P:$178), and #>imm,a (P:$7f), cmp #>imm,a (P:$4c),
; beq (P:$4e), move x1,x:>abs (P:$86a), add x0,a (P:$83), lsr #n,a (P:$3eb),
; move a,r0 (USB AUDIO IN's inject, run on a unit), move x:(rN+n),a and
; move a1,x:(rN+n) (the one-word displaced form, 533 sites in A),
; lua (r0+n),r0 (P:$a7), move r6,a (as USB AUDIO IN's move r6,a), bsr to a
; later label (Spectrum's, Character's),
; move x:(rN)+,x1 and move a/x0,x:(rN)+ (P:$ae, P:$359, P:$dd) through a
; pointer for the peripherals, as #581's meter_out.asm.

lm_start:
        move    r5,x:>$206              ; the displaced instruction
        bsr     otop_reset
        move    #>$ffff8c,r1            ; peripherals through a pointer: dsp_asm's
        move    x:(r1)+,x1              ;   round trip names them (TCR0 now)
        move    #>$37f10,r1             ; core 0's block
        move    #>$30000,a
        cmp     #>$38000,a
        bne     lms_0
        move    #>$37f18,r1             ; core 1's
lms_0:
        move    x:(r1+5),a              ; armed?
        cmp     #>$5a5a5a,a
        beq     lms_t
        clr     a                       ; timer 0 counts from 0 now: the start stamp
        move    a,x:(r1+0)
        move    #>$5a5a5a,x0
        move    x0,x:(r1+5)
        move    #>$ffff8e,r1            ; TLR0 = 0
        move    a,x:(r1)+
        move    #>$ffffff,x0            ; TCPR0 all ones
        move    #>$ffff8d,r1
        move    x0,x:(r1)+
        move    #>$000201,x0            ; TCSR0: TRM | TE, no interrupt enable
        move    #>$ffff8f,r1
        move    x0,x:(r1)+
        rts
lms_t:
        move    x:(r1+0),x0             ; the last start
        move    x1,a
        sub     x0,a
        and     #>$ffffff,a             ; the advance mod 2^24 (a2 stale: stored from a1)
        move    a1,x:(r1+2)             ; period
        move    x1,x:(r1+0)
        rts

lm_end:
; LM_BURN
        move    #>$ffff8c,r6
        move    x:(r6)+,x1              ; TCR0 now
        move    #>$37f10,r6
        move    #>$30000,a
        cmp     #>$38000,a
        bne     lme_0
        move    #>$37f18,r6
lme_0:
        move    x:(r6+0),x0             ; busy = now - this frame's start
        move    x1,a
        sub     x0,a
        and     #>$ffffff,a
        move    a1,x:(r6+1)
        move    r6,a
        cmp     #>$37f10,a
        bne     lme_x                   ; core 1: done
        move    #>$ffffb3,r7
        move    x:(r7)+,x1              ; SAISR: TUE bit 14, ROE bit 7
        move    #>$1,x0
        move    x1,a
        and     #>$004000,a
        beq     lme_nt
        move    x:(r6+3),a
        add     x0,a
        move    a1,x:(r6+3)
lme_nt:
        move    x1,a
        and     #>$000080,a
        beq     lme_nr
        move    x:(r6+4),a
        add     x0,a
        move    a1,x:(r6+4)
lme_nr:
        move    x:(r6+6),a              ; frames
        add     x0,a
        move    a1,x:(r6+6)
lme_x:
        move    #>$421,r6               ; the displaced instruction
        rts

; OTOP: the frame start points this core's next cost cell at its first. a
; and x0 are dead (lm_start's own), r1 is set again by what follows.
otop_reset:
        move    #>$30000,a
        cmp     #>$38000,a
        move    #>$37f40,x0
        bne     otr_0
        move    #>$37f50,x0
otr_0:
        move    x0,r1
        move    x0,x:(r1+9)
        rts

; OTOP start, the displaced `move a,x:>$20a` (A P:$4b2 FX1, $4e8 FX2; B P:$2a7,
; $2dd): the call's start stamp. The displaced store runs first, which keeps
; a in x:$20a; a is reloaded last. b and r2 are dead (reloaded below the
; site, A P:$4b8 and $4ba, B's the same) and nothing else is touched.
otop_s1:
otop_s2:
        move    a,x:>$20a               ; the displaced instruction
; OT_S
        move    #>$ffff8c,r2
        move    x:(r2),b                ; TCR0 now (no address update: m2 may be modulo)
        move    #>$30000,a
        cmp     #>$38000,a
        move    #>$37f48,r2             ; core 0's start stamp cell
        bne     ots_0
        move    #>$37f58,r2             ; core 1's
ots_0:
        move    b,x:(r2)
        move    x:>$20a,a
        rts

; OTOP end, after the displaced `move x:>$213,b` (A P:$4d8, B $2cd) for FX1 and
; `move x:>$206,r0` (A P:$50e, B $303) for FX2: now minus the start stamp into
; the next cost cell. a, b, r2 and r3 are dead after both sites (each is
; written before it is read by the stock code that follows), x0 is left alone
; (the call at A P:$514 may read it), no modifier register is used.
otop_e1:
; OT_E1
        bsr     otop_end
        move    x:>$213,b               ; the displaced instruction
        rts
otop_e2:
; OT_E2
        bsr     otop_end
        move    x:>$206,r0              ; the displaced instruction
        rts
otop_end:
        move    #>$ffff8c,r2
        move    x:(r2),a                ; now
        move    #>$30000,b
        cmp     #>$38000,b
        move    #>$37f48,r2
        bne     ote_0
        move    #>$37f58,r2
ote_0:
        move    x:(r2),b                ; the start stamp
        sub     b,a                     ; a1: the advance mod 2^24
        move    x:(r2+1),b              ; the next cost cell
        move    b,r3
        move    a1,x:(r3)
        add     #<$1,b
        tfr     b,a
        move    a1,x:(r2+1)
        rts

; core 0, after the MAIN pack (A P:$2ec `move x:>$205,r0`, before the
; ColdFire's pull of this frame's read-back): the 17 bytes into MAIN's low
; words (x:$206 + $100: MAIN's long i at words 2i, 2i + 1). Busy and period
; are the last finished frame's. r1 is core 0's block, r0 the pointer; both
; are written by the stock code that follows, which relies on m0 and m1
; being linear (P:$2f4 `move x:(r0+$32),a`, P:$303 `a,x:(r1)+`); a, b, x0
; and x1 are written there before they are read.
lm_pub:
; LM_PUB
        move    #>$37f10,r1
        move    x:>$206,a
        move    #>$100,x0
        add     x0,a
        move    a,r0
        move    #>$a5,x0                ; the magic, and the sum's start
        move    x0,x:(r1+$10)
        bsr     lm_byte
        move    x:(r1+6),a
        bsr     lm_low
        move    x:(r1+1),a
        bsr     lm_three
        move    x:(r1+2),a
        bsr     lm_three
        move    x:(r1+9),a              ; core 1's busy
        bsr     lm_three
        move    x:(r1+$a),a             ; core 1's period
        bsr     lm_three
        move    x:(r1+3),a
        bsr     lm_low
        move    x:(r1+4),a
        bsr     lm_low
        move    x:(r1+$10),a            ; the sum, published as it is
        and     #>$ff,a
        move    a1,x0
        bsr     lm_byte
; OTOP's record: $b4, the slot (0..15: this frame's count mod 16), its cost's
; three bytes, the sum of those five mod 256. The cost cell is core 0's $37F40
; + slot for slots 0..7, core 1's $37F50 + slot - 8 for 8..15.
        move    #>$b4,x0
        move    x0,x:(r1+$10)
        bsr     lm_byte
        move    x:(r1+6),a
        and     #>$f,a
        move    a1,x:(r1+$12)           ; the slot
        bsr     lm_low
        move    x:(r1+$12),a
        tfr     a,b
        and     #>$8,b                  ; slots 8..15 are 8 cells further on
        add     b,a
        add     #>$37f40,a
        move    a,r2
        move    x:(r2),a                ; the cost
        bsr     lm_three
        move    x:(r1+$10),a
        and     #>$ff,a
        move    a1,x0
        bsr     lm_byte
lmp_x:
        move    x:>$205,r0              ; the displaced instruction
        rts

; a1's three bytes, high first
lm_three:
        move    a1,x:(r1+$11)
        lsr     #$10,a
        bsr     lm_low
        move    x:(r1+$11),a
        lsr     #$8,a
        bsr     lm_low
        move    x:(r1+$11),a
; a1's low byte: added to the running sum and published
lm_low:
        and     #>$ff,a
        move    a1,x0
        move    x:(r1+$10),a
        add     x0,a
        move    a1,x:(r1+$10)           ; the sum, mod 256 when published
; x0 (0..255) into the low byte of the long at r0's low word; r0 to the next long
lm_byte:
        move    x:(r0+1),a
        and     #>$ffff00,a             ; the byte is zero in stock; cleared anyway, so no carry
        add     x0,a                    ;   can reach the sample's bits (x0 is 0..255)
        move    a1,x:(r0+1)             ; a1: no limiting
        lua     (r0+2),r0
        rts
