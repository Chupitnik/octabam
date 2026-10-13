"""CLOUDS VERB -- the reverb of Mutable Instruments Clouds, as an FX2 insert.

Clouds' reverb stage (clouds/dsp/fx/reverb.h, MIT, Emilie Gillet 2014): a
Griesinger/Dattorro plate of four input allpasses and a loop of two halves,
each two allpasses and a delay, with the first allpass smeared by an LFO and the
second delay modulated. The delay lengths are scaled by 0.984 so the ring is
16,120 words: the allocator gives an FX2 instance 16,384, and the top 256 words
of the slot carry the stock mailbox and LOAD METER, so the module declares its
extent (Claims.buffer_words) and never writes them.

The five knobs are Clouds' own setters: AMNT (amount), GAIN (input gain), TIME
(reverb time), DIFF (the allpass coefficient), TONE (the damping lowpass).
Defaults are the Clouds module's values.
"""
from remix.schema import (BusRole, Category, Claims, DspSection, Formatter, Gate, Harness,
                          Kind, MenuEntry, Module, Param, Proof, YBase)

MODULE = Module(
    name="clouds-verb", key="CLOUDS VERB", kind=Kind.DSP_EFFECT,
    category=Category.TRACK, author="Emilie Gillet (Mutable Instruments), ported here",
    author_url="https://github.com/pichenettes/eurorack",
    proof=Proof.RENDER, proof_note="`verify_cloudsverb` (dsp_host, against a float model of Clouds' reverb); not on a unit",
    doc="Clouds' Griesinger plate reverb: amount, input gain, time, diffusion, tone. One FX2 slot per instance.",
    menu=MenuEntry(fx2_id=0x14, replaces="PLATE REV", donor_desc=0x400d5594,
                   abbr=b"CLVB", fullname=b"Clouds Verb", build_tag=False),
    params=(
        Param(b"AMNT", 40, active=True, formatter=Formatter.PLAIN, doc="dry/wet; 0 is exactly dry"),
        Param(b"GAIN", 51, active=True, formatter=Formatter.PLAIN, doc="input gain; Clouds' 0.2 at 51"),
        Param(b"TIME", 64, active=True, formatter=Formatter.PLAIN, doc="loop gain 0.35..0.98"),
        Param(b"DIFF", 100, active=True, formatter=Formatter.PLAIN, doc="allpass coefficient 0..0.8; Clouds' 0.625 at 100"),
        Param(b"TONE", 90, active=True, formatter=Formatter.PLAIN, doc="damping lowpass coefficient 0.05..1; Clouds' 0.7 at 90"),
        *(Param() for _ in range(7)),
    ),
    dsp=DspSection(asm="modules/clouds-verb/clouds_verb.asm", priority=11,
                   bus_role=BusRole.NONE, ybase=YBase.NEVER,
                   r7_latch_slot=None, gate_label=None),
    claims=Claims(stock_instance_buffer=True, buffer_words=16120),
    harness=Harness(layout_char="H", is_server=False),
    gates=(Gate('tools/verify/verify_cloudsverb.py', remix_arg=False, venv=True),),
)
