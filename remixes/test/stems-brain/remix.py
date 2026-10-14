"""STEM REC under PROJECT with BRAIN on the root: coexistence proof.

STEM REC grows PROJECT (MAIN MENU > PROJECT > STEMS); BRAIN grows the
root's fifth category. Stock effects keep the FX2 chooser filled. MEMORY
is required by STEM REC's DramRegions.
"""

from remix.schema import Proof, Remix

REMIX = Remix(
    name="stems-brain",
    family="probes", proof=Proof.PORT, proof_note="verify_stems_menu under ot_emu",
    doc="STEM REC under PROJECT with BRAIN on the root: coexistence of the two menu growers.",
    modules=("STEM REC", "BRAIN", "MEMORY",
             "FILTER", "EQUALIZER", "DJ EQ", "PHASER", "FLANGER", "CHORUS",
             "SPATIALIZER", "COMB FILTER", "COMPRESSOR", "LO-FI", "DELAY",
             "PLATE REV", "SPRING REV", "DARK REV"),
    fallback="NONE",
)
