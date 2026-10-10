"""VOCODER -- a ten-band channel vocoder after the Roland VP-330.

A per-track insert. The track's audio is the modulator (a voice); the carrier
is the built-in 8' and 4' sawtooths at NOTE (p-lock NOTE on the steps to play a
melody), or the track's right channel with MODE EXT (a THRU track with the
voice on input A and a synth on input B). Ten bands at the VP-330's centres,
each a 4th-order pair on both sides, so the formants stay apart and the words
come through. modules/vocoder/DESIGN.md has the law and the reasons;
vocoder_ref.py is the float reference.
"""
import sys as _sys
from pathlib import Path as _Path

from remix.schema import (BusRole, Category, Claims, DspSection, Formatter, Gate, Harness,
                          Kind, MenuEntry, Module, Param, Proof, YBase)

_sys.path.insert(0, str(_Path(__file__).resolve().parent))
import vocoder_law as _ref  # noqa: E402  (numpy-free: the build runs on plain python3)


def _q23(v):
    """Q23, two's complement in 24 bits, held at the largest positive value."""
    return min(round(v * (1 << 23)), 0x7FFFFF) & 0xFFFFFF


# P table, 43 words, read through the ptable literal:
#   +0   BAND[10] x {W/2, f1, f2}  the synthesis side, in the order the band loop reads it
#        (the analysis f1 A, f2 A are computed per call into the instance's Y slot: GEND)
#   +30  OCT[8]   the increment of C(o), x 2: inc = OCT[o] 2^r / 2 (a cycle 2^24)
#   +38  GLD[5]   GLIDE's K per sample, 2^23 / (FS tau): 8 (OFF), 30, 80, 200, 500 ms
BAND = tuple(_q23(v) for f1, f2, w in _ref.band_coefs() for v in (w, f1, f2))
OCT = _ref.OCT
GLD = _ref.GLD
assert len(BAND) == _ref.TABLE_OCT and len(BAND) + len(OCT) == _ref.TABLE_GLD

NOTE_LABELS = tuple(_ref.note_name(k) for k in range(61))
_P = Formatter.PLAIN
_BLANK = Param(b"", 0)

MODULE = Module(
    name="vocoder",
    key="VOCODER",
    kind=Kind.DSP_EFFECT,
    category=Category.TRACK, author="Ignorato", author_url="https://github.com/Ignorato",
    proof=Proof.RENDER, proof_note="0.2 in dsp_host and under the ColdFire port (verify_vocoder, make check), 10 Oct 2026; not flashed. The 0.2 code before the de-zipper was heard on Ignorato's MKII (OCTABAM20, 5 Oct 2026, T2 T3, by ear); 0.1 ran on OCTABAM12",
    doc="Ten-band vocoder after the Roland VP-330: the track's voice, a built-in carrier at NOTE or input B. FX2 of tracks 2, 3, 6 and 7 only.",
    menu=MenuEntry(
        fx2_id=0x1a,
        donor_desc=0x400d58b8,        # DARK REV
        abbr=b"VOCO",
        fullname=b"VOCODER",
        build_tag=False,
    ),
    params=(
        Param(b"NOTE", 24, 61, active=True, formatter=Formatter.WIDE_STEPPED, labels=NOTE_LABELS,
              doc="the built-in carrier's pitch, C1 to C6; p-lock it to play a melody"),
        Param(b"BEND", 64, 128, active=True, formatter=Formatter.BIPOLAR,
              doc="pitch bend, -12..+12 semitones on NOTE: the crossfader as a pitch wheel"),
        Param(b"GEND", 64, 128, active=True, formatter=Formatter.BIPOLAR,
              doc="formants: -64 a bigger, male voice (x0.84, -3 st) .. +63 a female one (x1.41, +6 st)"),
        Param(b"WHSP", 0, 128, active=True, formatter=_P,
              doc="whisper: the built-in carrier from the sawtooths (0) to noise (127)"),
        Param(b"DRY", 0, 128, active=True, formatter=_P,
              doc="the dry voice mixed in"),
        Param(b"CONS", 48, 128, active=True, formatter=_P,
              doc="consonants: the voice above 4 kHz passed through, as the VP-330 does"),
        Param(b"MODE", 0, 2, active=True, formatter=Formatter.STEPPED, labels=("INT", "EXT"),
              doc="carrier: INT the built-in sawtooths at NOTE; EXT the right channel (input B)"),
        _BLANK,
        Param(b"GLDE", 0, 5, active=True, formatter=Formatter.STEPPED, labels=_ref.GLIDE_LABELS,
              doc="glide on NOTE and BEND, ms (OFF is 8 ms: it smooths BEND's steps)"),
        _BLANK, _BLANK, _BLANK,
    ),
    mode_slot=6,
    dsp=DspSection(
        asm="modules/vocoder/vocoder.asm",
        ptable=BAND + OCT + GLD,
        priority=19,
        bus_role=BusRole.NONE,        # an insert
        ybase=YBase.NEVER,            # its allocator slot only (claims), nothing in the shared window
        r7_latch_slot=None,
        gate_label=None,
    ),
    harness=Harness(layout_char="X", is_server=False, render_r7=5),   # runs at 0x6500/0x6800 only
    # One allocator slot, as stock DELAY: GEND writes 20 words of analysis coefficients
    # there, 256 words in from the base init reads. It runs only at FX2 positions 1 and 2:
    # Y:0x8000 on either core, and position 2's buffer in the shared window (0x30000 on
    # core 0, 0x38000 on core 1), which this claim reserves.
    claims=Claims(stock_instance_buffer=True, buffer_words=20),
    gates=(Gate("tools/verify/verify_vocoder.py", remix_arg=False),),
    dear={"NOTE": 60, "BEND": 127, "GEND": 127, "WHSP": 127, "CONS": 127, "DRY": 127, "MODE": 1, "GLDE": 4},
)
