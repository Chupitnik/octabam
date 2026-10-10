# `vocoder` -- VOCODER

A ten-band channel vocoder after the Roland VP-330: the track's audio (a voice) shapes a carrier.

> **It runs on FX2 of tracks 2, 3, 6 and 7 only**, and passes audio through untouched on tracks 1, 4, 5 and 8 and on any FX1 slot. On a MKII two VOCODERs on a DSP core (tracks 1-4 share one, 5-8 the other) play, and a third overran the core (a loud glitch, then no audio and no sequencer until a reboot; OCTABAM7-9, 4 Oct 2026). The real cost is about 850-940 cycles a sample, not the 760 `make cycles` counts (one-word displaced moves measured at about 4 cycles, CHIP.md section 2), and the 3,120 usable already includes the four FX1 FILTERs the pricer does not charge. So the limit is built in rather than documented: a third VOCODER on a core stays dry instead of stalling the unit. It avoids each core's first track (T1, T5). On T1, VOCODER put left-only click bursts into the main out at one phase of the pattern, worst at trigs (measured: Ignorato's MKII, 5 Oct 2026, captured at the main outs; absent with FX2 NONE and on T2 and T3; FAILURE_MODES.md, "Left-only click bursts from VOCODER on T1"). The cause is inferred: an effect at a core's position 0 finishing before the ColdFire's pull, as in "Junk on main R ... T1 with BusDelay", though that entry is main R and 16-24 samples of junk. T5, core 0's first track, is excluded by analogy, not measured.

On the unit it is an FX2 effect. Put it on track 2, 3, 6 or 7, playing a voice, sample or live through a THRU machine: with MODE INT the carrier is built in, two sawtooths an octave apart at NOTE, so turning NOTE (or parameter-locking it on the steps) plays the vocoded voice as a melody. Version 0.2 puts the performance controls on page 1, where scenes, the crossfader, LFOs and p-locks reach them: BEND (the fader as a pitch wheel), GEND (from a male to a female voice) and WHSP (singing to whispering). With MODE EXT the carrier is the track's right channel: a THRU track with the voice on input A and a synth on input B. [`DESIGN.md`](DESIGN.md) has the law and the reasons; [`vocoder_ref.py`](vocoder_ref.py) is the float reference the DSP is proved against.

## Knobs

| page | slot | name | range | what it does |
|---|---|---|---|---|
| 1 | 0 | NOTE | C1..C6 | the built-in carrier's pitch, shown as a note; parameter-lock it to play a melody; default C3 |
| 1 | 1 | BEND | -64..+63 | pitch bend on NOTE, -12 to +12 semitones (3/16 of a semitone a step, smoothed); a scene on each side of the crossfader makes the fader a pitch wheel; default 0 |
| 1 | 2 | GEND | -64..+63 | formants: the voice is analysed off the synthesis bands, so every formant comes out scaled, x0.84 (-3 semitones: a bigger, male voice, at -64) to x1.41 (+6: a female one, at +63); each step above the centre is twice one below it; with BEND an octave up and a little WHSP, one fader goes from a male to a female voice; smoothed over 8 ms; default 0 |
| 1 | 3 | WHSP | 0-127 | whisper: the built-in carrier from the sawtooths (0) to white noise at the same level (127); smoothed over 8 ms; default 0 |
| 1 | 4 | DRY | 0-127 | the dry voice mixed in; smoothed over 8 ms; default 0 |
| 1 | 5 | CONS | 0-127 | consonants: the voice above 4 kHz passed through, as the VP-330's high-consonant circuit does; smoothed over 8 ms; default 48 |
| 2 | 6 | MODE | INT, EXT | the carrier: INT the built-in sawtooths at NOTE; EXT the right channel |
| 2 | 8 | GLDE | OFF, 30, 80, 200, 500 | glide on NOTE and BEND, in ms (a log-pitch time constant); OFF is 8 ms, which smooths BEND's steps |

There is no level knob (0.1 had LEVL): the track's AMP page sets the level. NOTE, BEND, GLDE and WHSP shape the built-in carrier, so with MODE EXT (the carrier is input B) they do nothing; GEND acts on the voice and works in both modes (checked in `dsp_host`: in EXT, BEND and WHSP leave the output bit-identical, GEND changes it).

**A project saved under 0.1 needs its defaults re-stamped.** 0.2 moved CONS, DRY and LEVL and put BEND, GEND, WHSP and GLDE in their slots, so a 0.1 part feeds 0.2 its old bytes: LEVL's 100 lands on WHSP and the voice turns to noise. Run `tools/hw/ot_project.py stamp-defaults <project> vocoder` on the card before playing it (AGENTS.md, "A part saved under an older slot layout").

The modulator is the left and right channels averaged (INT), or the left channel (EXT). The output is mono, on both channels.

## Measured

All by `tools/verify/verify_vocoder.py` through `dsp_host` on the `vocoder` remix's image (payload A), 4-5 Oct 2026 (0.2's de-zipper 10 Oct 2026), unless stated. The test modulator is synthetic: a 120 Hz glottal pulse train through three formants, the vowel changing every 200 ms, with noise bursts for consonants.

- ✅ The output matches the float reference within -62.4 dB (INT at C3) and -64.2 dB (EXT with a chord carrier), level within 0.001 dB. The difference is the band sum's 24-bit rounding (it was -85.7 dB with a 48-bit sum, which cost two moves a band); it exists only while a band is active.
- ✅ Silence in gives silence out, bit-exact, with CONS and DRY up.
- ✅ 0.3 s into a pause the output is -136.3 dBFS: the envelopes decay in 48 bits (in 24 they stuck at about -90 dBFS and let the carrier through between words).
- ✅ A sine at a band's centre keeps that band at least 18.6 dB above its neighbours.
- ✅ NOTE: C1, C3 and C6 within 0.05 % (measured 32.703, 130.813 and 1046.503 Hz); a saved NOTE past C6 plays C6.
- ✅ 0.2's controls match the reference (`vocoder_ref.py`, whose pitch, GEND, noise and de-zipper arithmetic is bit-exact): GEND 0 and 127 within -61.9 and -62.6 dB; WHSP 64 and 127 within -57.5 and -57.4 dB; a p-locked melody with two BEND moves under GLDE 80 ms within -62.1 dB; BEND, GEND and WHSP swept by the fader one value a call within -63.7, -62.4 and -57.8 dB; a scene jump of GEND, WHSP, DRY and CONS at once within -65.6 dB, and its first 20 ms within -63.3 dB (10 Oct 2026). On a LibriVox voice (renders, 10 Oct 2026) the same controls match within -66.5 to -70.6 dB, the error's peak under -77 dBFS; in the 20 ms after a scene jump the DSP is -54.3 dB from the smoothed reference and -5.5 dB from a stepped one, so the gate sees a missing de-zipper. BEND -64 and +63 on C3 measure 65.407 and 258.805 Hz (want 65.406 and 258.807). GLDE 80 ms reaches 63 % of a step in 79.6 ms (the reference's self-check).
- ✅ `dsp_host -guard`: nothing written over a loaded module, with GEND's Y slot in use.
- 0.2 costs 779 counted cycles a sample (`make cycles`; 0.1: 760); the pricer does not see per-call work. Measured (instructions, not cycles: neither instrument sees a multi-cycle instruction, CHIP.md section 2), 10 Oct 2026. Under the ColdFire port (the firmware's own dispatch, `ot_emu --dsp-stopwatch` over a core's four-track window, `benchmark_analog_bd`'s fixture: every track a THRU machine on live tones, FX1 FILTER on all eight, FX2 VOCODER on all eight, so each core runs two and passes two dry; 850 frames): 0.1 2,885.8 a sample mean, 2,913.8 peak; 0.2 2,927.3 and 2,956.6, so +42.8 a core at the peak (+1.5 %), the same on both cores; FILTER on both slots instead, 2,398.9 and 2,442.7. One instance in `dsp_host -meter`, 16-sample blocks: 0.1 736.6 a sample; 0.2 753.8 with the knobs still and 764.6 with BEND, GEND, WHSP, DRY and CONS moving every call (the division and the 20 coefficients each call); the code heard on OCTABAM20 (0.2 before the de-zipper) 751.8 and 762.7, so the de-zipper costs 2. Inferred: with every control moving, two 0.2s on a core cost about 65 instructions a sample more than two 0.1s (about 100 cycles once the extra displaced moves are priced at about 4 cycles), against the roughly 700-900 a third 0.1 added when it overran the unit; the real margin on the unit is not measured (a CF METER run would).
- ✅ Every knob at both ends renders.
- ✅ Two per core, built in: it runs at r7 0x6500 and 0x6800 (the FX2 state blocks of a core's second and third tracks) and is an exact dry pass at the other FX2 blocks (0x6200, 0x6b00) and every FX1 block (0x6100, 0x6400, 0x6700, 0x6a00); `dsp_host -guard` is clean at both running positions (position 2's Y slot is in the shared window, 256 words in).
- 🟡 760 counted cycles a sample (`make cycles REMIX=vocoder`); `make accept` passes, pricing four on a core at 3,040. On the unit this is not true: see the status note above. The first version counted 1,085 (three on a core stalled the unit on OCTABAM7); the parallel-move rewrite, a peak-detector envelope, a 24-bit band sum and the tables in X memory (DJ EQ out of the remix) brought the count down, but three on a core with FX1 FILTERs still overrun (OCTABAM8 and OCTABAM9).
- Listened to (4 Oct 2026): a LibriVox reading (public domain) through this DSP code in `dsp_host`, at C3, C2, without consonants, a melody parameter-locked per 1/8 and an external chord carrier; judged intelligible and "really good" by the tester. The design was chosen by ear and measurement: the VP-330's steep bands against a first law with shallow ones (DESIGN.md).

## On the unit

- OCTABAM7-9 (Ignorato's MKII, 4 Oct 2026, by ear): VOCODER plays and is intelligible; one or two per core play; a third on a core (FX1 = FILTER) overran it, a loud glitch, then no audio or sequencer until a reboot.
- ✅ OCTABAM12 (`make image REMIX=vocoder BUILD=12`, from the card), same unit and day, by ear: on the Octatrack Advisor's project with VOCODER on FX2 of T1, T2 and T3 (the set that stalled OCTABAM9), T1 and T2 vocode and T3 passes its audio dry; with T4 added, T4 is dry too; then VOCODER on FX2 of all eight tracks: T1, T2, T5 and T6 vocode, T3, T4, T7 and T8 are dry, stable on both cores. Those were the earlier positions.
- 🟡 The current positions (T2, T3, T6, T7) are not flashed on this build. They were heard clean on the 0.2 code (OCTABAM20, branch `vocoder-0.2` at `f83463c8`; Ignorato's MKII, 5 Oct 2026, by ear): alone and with two per core, through trigs and knob sweeps, T1 dry. That code is this module's without the de-zipper (the instructions are otherwise the same).
- 0.2 with the de-zipper: not flashed (10 Oct 2026).

## Open

- The VP-330's ensemble (a bucket-brigade chorus on its output) is not modelled; a chorus on the Octatrack's FX would come after FX2's output only on another track.
- More than two per core: getting four to fit beside four FILTERs needs under about 590 real cycles each; until then the limit is built in.
- The pricing gap is worth reporting upstream: displaced moves and table reads counted as one cycle, and FX1 FILTERs not charged against a budget that contains them.

## Gates

- `tools/verify/verify_vocoder.py` (the manifest's `Gate`). It refuses to run if the id resolves to SEND's entry points.
- `make check REMIX=vocoder`; with `OT_PROJECT=<a project>` it adds `verify_set` under the ColdFire port.

## Design

[`DESIGN.md`](DESIGN.md): the VP-330 as documented, why the bands are steep, what the 24-bit version changed and why, the r7 slots and the tables.
