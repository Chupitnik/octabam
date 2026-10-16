# RECBUF PERSIST

On **PROJECT SAVE**, each non-empty recorder buffer (slots 128–135) is
written with the stock synchronous WAV writer (`0x40020f04`) to
`!BUFFERS/RECn.wav` under the project (folder created if missing), PATH’d
to that name, then run through stock slot load + post-load
(`0x40093980` / `0x40099148`) so the control record and UI show the
assignment.

On **PROJECT LOAD** / **RELOAD**, existing `!BUFFERS/RECn.wav` files get
the same PATH + slot-load treatment.

Empty buffers (control-record length ≤ 64) are skipped.

## Status

`Proof.CHECK`. `verify_recbuf` covers symbols and a port save/load
round-trip.

Hardware (author's MKII, personal remix carrying this module, BUILD R2–R4,
16 Oct 2026): PROJECT SAVE wrote all eight `!BUFFERS/REC1.wav`..`REC8.wav`
and assigned the recorder slots. R1 only assigned the last slot (async
SAVE/ASSIGN shared scratch); R2 saved all via sync `wav_write`; R3 bound
assignment via slot load + post-load; R4 paths under `!BUFFERS/`.

## Remix

Listed next to STORE (required). Carrier:
`remixes/test/recbuf-persist`. Conflicts with nothing named; keep
RELOAD_FROM_PROJECT out of a remix that also wants this if both would
own the same save path (they do not today).
