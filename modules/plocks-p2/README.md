# PLOCKS P2

Parameter locks on page 2 of FX1 and FX2. Requires **SCENES P2**.

## Use

- Open FX1 or FX2 SETUP, hold one or more trigs and turn a knob to lock its
  page-two value on those steps. Releasing an edited trig keeps the lock.
- Hold FUNC while turning to remove the lock.
- Playback applies the locked value; the next trig restores the Part value.
- Trig, track and pattern copy/clear operations carry the page-two locks.

Page-two locks do not slide. Adding a lock to a step without a trig does not
create a playable trig; place the trig separately.

## Storage

Keep `p2lkNN.work` and `p2lkNN.strd` with the matching bank files in the project.
They hold the page-two locks for each bank. The current bank's retained copy
holds up to **4,768 locks**. Above that capacity, a power cut can discard
unsaved page-two changes, so save the project before powering off.

[Implementation, address provenance and verification](DEVELOPMENT.md).
