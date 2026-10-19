A TB-303 voice as a track machine, shown as BLKPYRAMD: Robin Schmidt's
Open303 algorithm on the DSP, one voice per DSP core (one on T1–T4, one on
T5–T8), ahead of the track's stock AMP, FX1 and FX2.

## Access

BLKPYRAMD is the sixth row of a track's machine list: double-tap the track,
or [FUNC] + [SRC] > MACHINE.

![SRC SETUP with BLKPYRAMD: WAVE, TUNE, SLID, GATE, VOL.](../../img/blackpyramids-303/setup.png)

## Controls

![BLKPYRAMD source controls: PTCH, CUT, RES, ENV, DEC, ACC.](../../img/blackpyramids-303/source.png)

| Page | Knob | What it does |
|---|---|---|
| SRC | PTCH | the note, C0–C8 by name; [FUNC] + turn steps by octaves |
| SRC | CUT | filter cutoff, 200 Hz–10 kHz |
| SRC | RES | filter resonance, 0–100 % |
| SRC | ENV | filter envelope amount (env mod) |
| SRC | DEC | filter envelope decay, 50 ms to 2 s |
| SRC | ACC | the accent scale for the steps you accent: 127 = each lock's amount, 0 = no accents |
| SETUP | WAVE | saw (0) to square (127), a continuous blend at equal loudness |
| SETUP | TUNE | A4 = 400–480 Hz; 64 is 440 Hz |
| SETUP | SLID | slide time, 0–198 ms |
| SETUP | GATE | note length, (GATE + 1)/128 of a step |
| SETUP | VOL | 0 off; 1–64 clean; 65–127 the same level driven into a soft clip |

## Playing it

- A trig plays a note with the knobs at that step. P-lock PTCH for a
  melody and ACC for an accent.
- A slide trig glides to its note without a retrigger. Trigless locks and
  trigless trigs right after a note tie it.
- Live: the track key, the chromatic keys (four octaves of absolute notes,
  [PAGE] the 17th) and MIDI notes play while held. TRIG + YES previews a
  step.
- SLOTS and SLICES have nothing to play here: their LEDs stay dark.

## Notes

- The track is a FLEX track underneath: a stock OS opens the project with
  it as FLEX, and FLEX's own values come back when the track leaves the
  machine.
- The voice takes SPRING REV's place on the DSP, so SPRING REV is not in a
  remix with it. A Part that still names SPRING REV shows its page and
  passes audio through.
