"""octabam-inserts -- the core ColdFire modules plus effects that run on any track.

CORE (BRAIN, STORE, KITS, MIDI SCENES, SCENES P2, PLOCKS P2, CC MAP, CC FEEDBACK,
USB MIDI, USB AUDIO OUT MAIN CUE, the recorder and bug fixes, MUTE_MODES from
DRAM, SCALE QUANTIZER, TUNER, DIRECT_JUMP_KYOTI, RELOAD_FROM_PROJECT, RLEN PLEN,
LOAD METER) with the effects that are inserts rather than bus servers:
SPECTRUM, CHARACTER and MODULATION (SLOTS = FX1+FX2: live on both menus),
SIDECHAIN_COMPRESSOR, TRANSIENT and EUCLID, the stock DELAY, CLOUDS VERB (FX2)
and REPITCH_REPEAT98_KYOTI. No bus and no SEND (fallback NONE). The other stock
DSP effects give up their words; SIDECHAIN_COMPRESSOR keeps COMPRESSOR's.
"""

from remix.schema import Pin, Proof, Remix

CORE = ("BRAIN", "STORE", "KITS", "MIDI SCENES", "SCENES P2", "PLOCKS P2",
        "CC MAP", "CC FEEDBACK", "USB MIDI", "USB AUDIO OUT",
        "RECORDER LOOP FIX", "BATCH_BUGFIXES", "ERASE_EMPTY_TRIGLESS_LOCKS",
        "QUANTIZE_LIVE_REC_TOGGLE", "REC_TRIG_MUTE", "MUTE_MODES",
        "SCALE QUANTIZER", "TUNER", "DIRECT_JUMP_KYOTI", "RELOAD_FROM_PROJECT", "RLEN PLEN",
        "LOAD METER")
INSERTS = ("SIDECHAIN_COMPRESSOR", "TRANSIENT", "EUCLID")
STATIONS = ("SPECTRUM", "CHARACTER", "MODULATION")

REMIX = Remix(
    name="octabam-inserts",
    family="mods", proof=Proof.PORT, proof_note="",
    doc="CORE + SPECTRUM, CHARACTER, MODULATION, SIDECHAIN_COMPRESSOR, TRANSIENT and EUCLID on FX1 and FX2, the stock DELAY, CLOUDS VERB on FX2, and REPITCH.",
    modules=STATIONS + ("MODE DEFAULTS", "DELAY", "CLOUDS VERB", "REPITCH_REPEAT98_KYOTI") + INSERTS + CORE,
    fallback="NONE",
    fx1=STATIONS + INSERTS,
    settings={("octabam.usb-audio-out", "LAYOUT"): Pin("MAIN CUE"),
              ("kyoti.mute-modes", "PLACE"): Pin("DRAM"),
              **{(f"octabam.{k.lower()}", "SLOTS"): Pin("FX1+FX2") for k in STATIONS}},
)
