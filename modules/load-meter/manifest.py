"""LOAD METER -- the frame interrupt's share of the ColdFire, published for
BRAIN's LOAD view (modules/brain: TOOLS > LOAD, FUNC + LEVEL push on an
MKII).

Two detours on the frame interrupt (vector 0x41, ~2,756 a second): its
entry 0x4000aad0, where the stub replays the prologue and stamps DMA timer
3's count, and its epilogue 0x4000d9a6, where the duration is summed and
the longest kept; every 0.5 s the window is published to lm_stats
{seq, window, sum, count, longest} in DMA timer 3 counts (132 MHz). The
sites are CF METER's epilogue and BOOT TRACE's entry, so the ledger refuses
LOAD METER beside either by name. meter.s has the arithmetic.
"""

from remix.schema import Category, Detour, Kind, Linked, Module, Proof

H = bytes.fromhex

MODULE = Module(
    name="load-meter", key="LOAD METER", kind=Kind.CF_PATCH,
    category=Category.REFERENCE, author="sambanks", author_url="https://github.com/sambanks",
    proof=Proof.PORT, proof_note="the window under the port with BRAIN's LOAD view (verify_brain); not on a unit",
    doc="The frame interrupt's share of the ColdFire and its longest run, every 0.5 s, for BRAIN's LOAD view.",
    linked=(Linked("loadmeter", "modules/load-meter/meter.s", cpu="5475", dram=True),),
    detours=(
        Detour(0x4000AAD0, H("4fefff0448d77fff"), "loadmeter", "lm_in",
               "the frame interrupt's entry: its prologue replayed, then the entry stamp", pad_to=8),
        Detour(0x4000D9A6, H("4cd77fff4fef00fc4e73"), "loadmeter", "lm_out",
               "the frame interrupt's epilogue, every exit path: the duration summed, the window published", pad_to=10),
    ),
)
