"""midi-scenes -- MIDI SCENES alone.

One ColdFire module, no DSP, no menu row: the reference minimal build of
the DRAM platform. This remix has not been flashed.
MIDISC 2.0 ran on bkkbrls-del's unit in the `ok-ms` image (14 Sep 2026).
"""

from remix.schema import Proof, Remix

REMIX = Remix(
    name="midi-scenes",
    family="mods", proof=Proof.PORT, proof_note="`verify_scenes` against MIDISC2.1 under the port; not flashed",
    doc="Reference minimal build: the MIDI SCENES ColdFire patch, alone.",
    modules=("STORE", "MIDI SCENES",),
    fallback="NONE",
)
