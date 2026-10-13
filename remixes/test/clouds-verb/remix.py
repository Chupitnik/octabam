"""Minimal image for CLOUDS VERB's gate: the module alone, no stock effect listed."""
from remix.schema import Proof, Remix
REMIX = Remix(family="effects", proof=Proof.RENDER, proof_note="`verify_cloudsverb`; not flashed", name="clouds-verb",
              doc="CLOUDS VERB alone: the fixture of its render gate.",
              modules=("CLOUDS VERB",), fallback="NONE")
