; USB AUDIO IN, DSP side, one inject for every INPUTS: while the host's
; stream is open, write its channels over the inputs of core 0's current RX
; block; while it is closed, leave the block alone and the jacks come
; through.
;
; Placed by the build in payload A's donor region and reached from the frame
; head: P:$88's `move r2,x:>$204` becomes `jsr >inject` (schema.DspHook) and
; is replayed here. Every stock reader of the RX block (x:>$202) runs after
; P:$88 in the frame, the core 0 -> core 1 handoff and the read-back the
; recorder takes included, so one inject serves both cores and the recorder.
;
; The ColdFire's frame transfer (usbaudio_in.s in_build) is two passes, each
; a flag word then, when the flag is 1, 16 samples x 2 channels as hi/lo
; halfword pairs (hi = sample[23:8], lo = sample[7:0]):
;   the CD pass: flag, then C,D -- slots 0-1 (INPUTS CD and ABCD)
;   the AB pass: flag, then A,B -- slots 2-3 (INPUTS AB and ABCD)
; A flag of 0 has no data after it, so the AB pass's flag is the next word.
; Closed or silent: both flag words 0 (in_frame clears words 0-1).
; DSP_DEST $6320 (usbaudio_in.s) lands at r6 + $320 after the DSP's mask:
; the frame's two bank setups (A P:$54, P:$64) set r6 to $4000 or $2000 and
; the host-DMA mask to $5fff or $3fff (P:$75, P:$77), so the transfer is at
; $4320 or $2320 by frame; a literal is right every other frame. The host port ships each 16-bit
; halfword as one X word whose high byte is stale, so every word is masked
; or shifted clear; a1 is stored (no limiting: a2 can be stale).
;
; The RX block is 16 samples x 4 slots; slots 2 and 3 are inputs A and B
; (the firmware's GAIN assignment, hardware 26 Sep 2026), 0 and 1 are C/D.
; Live at P:$88: r2 (stored here), r4-r7, b. Free: a, x1, r0, r1.
;
; brclr's operand is the displacement from the brclr itself, written as a
; number: dsp_asm puts a label's ABSOLUTE address in the extension word,
; which the chip reads as a displacement (stock P:$63e `0cc58c 00001d`
; branches +$1d); docs/contributing/TRAPS.md trap-brclr.
;
; Every form has a stock site in payload A: move x:(r0)+,a (P:$b2),
; and #>$ff,a (P:$b3), asl #n,a,a (P:$11a), move a1,x:(rN+$n) (P:$51a),
; lua (r1+$4),r1 (P:$341), do #<n (P:$9f), move r6,a / add #>n,a /
; move a,r0 (the shipped inject),
; move x:(rN)+,x1 (P:$ae), brclr #n,x1 (P:$63e), bsr to a later label
; (Spectrum's, Character's). The pair loop's
; body is the shipped inject's CD loop (build 16 on Bryan T's unit). The do
; form runs its label after the body (the stored operand is the label - 1),
; so `usbi_pend: rts` ends the loop, the CD pass's bsr and the inject.

inject:
        move    r2,x:>$204              ; the displaced instruction
        move    r6,a
        add     #>$320,a
        move    a,r0                    ; the transfer: this frame's bank + $320
        move    x:>$202,r1              ; r1 = the current RX block
        move    x:(r0)+,x1              ; the CD pass's flag
        brclr   #0,x1,$4                ; to usbi_nocd; bit 0: the stale high byte is not read
        bsr     usbi_pair               ; C,D over slots 0-1
usbi_nocd:
        move    x:>$202,r1
        lua     (r1+2),r1               ; slot 2 = input A
        move    x:(r0)+,x1              ; the AB pass's flag
        brclr   #0,x1,$15               ; to usbi_pend: neither, the jacks stay
usbi_pair:
        do      #16,>usbi_pend
        move    x:(r0)+,a               ; first channel, bits 23:8
        asl     #8,a,a
        move    a1,x1
        move    x:(r0)+,a               ; bits 7:0
        and     #>$ff,a
        add     x1,a                    ; x1's low byte is zero: add = or
        move    a1,x:(r1)               ; slot 0 (C) or 2 (A)
        move    x:(r0)+,a               ; second channel
        asl     #8,a,a
        move    a1,x1
        move    x:(r0)+,a
        and     #>$ff,a
        add     x1,a
        move    a1,x:(r1+1)             ; slot 1 (D) or 3 (B)
        lua     (r1+4),r1               ; next sample's four slots
usbi_pend:
        rts
