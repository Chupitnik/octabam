"""core -- bottleservice plus BRAIN: the settings store on the rig.

bottleservice's selection (remixes/bottleservice) with BRAIN: card-wide knob
defaults from BRAIN/card.work, SAVE FX DEFAULT and CLEAR FX DEFAULT in the
BRAIN MAIN MENU category. BRAIN's gate (tools/verify/verify_brain.py) runs
on it, restoring the FX2 chooser's YES entry in RAM where it selects.

Without MODULATION (bottleservice has it): LOAD METER's OTOP hooks need
payload A's room (about 110 words a payload; MODULATION is 1,411).
"""

from remix.schema import Pin, Proof, Remix

REMIX = Remix(
    name="brain",
    family="mods", proof=Proof.PORT, proof_note="`verify_brain` under the port, 6 Oct 2026; not on hardware",
    doc="bottleservice + BRAIN: card-wide knob defaults, SAVE FX DEFAULT in the BRAIN menu.",
    modules=("REVERB SERVER", "DELAY SERVER", "SEND",
             "SPECTRUM", "CHARACTER",
             "TEMPO SYNC", "CC MAP", "CC FEEDBACK", "MODE DEFAULTS", "RIG HOSTS", "TEMPO BUS",
             "USB MIDI", "USB AUDIO OUT",
             "STORE", "KITS",
             "SCENES P2", "PLOCKS P2",
             "DELAY",
             "FX2 LOCK",
             "BRAIN", "LOAD METER"),
    fallback="SEND",
    hidden=("REVERB SERVER", "DELAY SERVER"),
    host_slots=(("DELAY SERVER", 2), ("REVERB SERVER", 2)),
    locked=("REVERB SERVER", "DELAY SERVER"),
    fx1=("SPECTRUM", "CHARACTER"),
    settings={("octabam.usb-audio-out", "LAYOUT"): Pin("MASTER")},
)
