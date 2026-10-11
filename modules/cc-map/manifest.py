"""CC MAP -- MIDI CC numbers stock ignores, mapped to parameters stock CC
cannot reach. Today: CC 62-67 -> the FX2 effect's page-2 slots 6-11, CC 68-73
-> the FX1 effect's. README.md lists the free CC numbers.

Stock incoming CC reaches page 1 only (CC 16-45; the handler admits
cc-16 < 30, docs/firmware/MIDI.md appendix A). The MIDI dispatch table entry
0x400d6474[0xB] (the CC vector) is repointed from the stock handler
0x4000e79c to the cave. The cave reads the CC number; anything outside its
blocks tail-calls CC_NEXT (the stock handler) with the argument intact. Inside a block it rebuilds the
channel->track map, gates on AUDIO CC IN, and on every audio track whose
trig channel matches makes the stores of that page's editor: FX2 page 2
as 0x4003aab2 (Part +0x8f084, shadow 0x100a51d2, lane +0x38), FX1 page 2
as 0x4003abe4 (Part +0x8f07e, shadow 0x100a51cc, lane +0x32), plus the four
dirty flags. FX2 writes only on BusDelay/BusVerb, clamped by the cave's
count tables; FX1 writes on any id but NONE (0), clamped by the FX1
descriptor's min/count. Selects are clamped to their count: an over-count
stored value is used as an index and stalls the sequencer.

Source is the truth: the build links cc_map.s into the DRAM runtime;
`legacy_bytes()` is the hand-assembled oracle the linked source is
compared against (Linked.reference, and
tools/verify/verify_ccmap.py, which also proves the write for all eight
tracks in the emulator against the firmware editor)."""

import hashlib
import os
import pathlib

from remix.schema import (Apply, Category, Gate, Kind, Linked, Module, Number, Proof, Scope, Setting,
                          Store, SymbolRef)

STORE = "octabam.cc-map"


def cc_inc(modules):
    """remix.inc for cc_map.s: the macros CC_FX2 and CC_FX1 load the two
    blocks' first CC (tools/remix/brain.py read_macro): from BRAIN's value
    table when the remix carries BRAIN (the settings are per project), else
    the resolved value as a constant."""
    from remix import brain, registry
    remix = registry.remix(os.environ["REMIX"])
    return (brain.read_macro(remix, modules, STORE, "FX2 CC", "CC_FX2")
            + brain.read_macro(remix, modules, STORE, "FX1 CC", "CC_FX1"))


def reference(modules):
    """The oracle applies to the constant form: none when BRAIN holds the
    block CCs at run time."""
    from remix import brain, registry
    remix = registry.remix(os.environ["REMIX"])
    if any(r.store_id == STORE and r.runtime for r, _s in brain.resolve(remix, modules)):
        return None
    return (ORACLE_AT, hashlib.sha256(legacy_bytes(ORACLE_AT)).hexdigest())

# Page-2 clamp counts, slots 6..11: selects carry their count, knobs 128.
# Must match modules/busverb and modules/busdelay.
VERB_COUNTS = bytes((3, 128, 128, 128, 128, 128))  # MODE, TONE, DIFF, GATE, DLY, TIME (slot 9 was SHFT, count 4, until 15 Sep 2026)
DLY_COUNTS = bytes((3, 128, 128, 4, 128, 128))  # MODE, SCTR, DENS, SIZE(select), PTCH, TIME

# The CC dispatch vector (status>>4 == 0xB) and its stock target.
DISPATCH_CC = 0x400d64a0
STOCK_CC = 0x4000e79c
STOCK_RTS = 0x40027e1a           # a bare `rts` in stock (the tail of 0x40027e00)

# The hand-assembled form of cc_map.s, with 0x40bad000/4 placeholders
# for its two count tables; legacy_bytes(addr) patches them in. The oracle
# the linked source is compared against every build.
CODE = bytes.fromhex(
    "206f0004700010280001723e90817205b280641a700010280001724490817205b2806504"
    "5c8060064ef94000e79c4fefffe448d704fc28002448263946104cf44eb9400018547a00"
    "1a2a000202850000007f4a398000004967287000101202800000000f41f946c7febe2e30"
    "0c007c007001eda8c0876702611252867008b0866eee4cd704fc4fef001c4e757206b284"
    "6f000114203946c824567200123980000003263c000018b24c031000d0812040d1fc0008"
    "ed88d1c6700010107206b28067107207b28067024e7543f940bad000600643f940bad004"
    "72001231480053812405b4816f022401203946c824567200123980000003263c000018b2"
    "4c031000d0817200d0812040d1fc0008f0842206761e4c031000d1c1d1c410822040d1fc"
    "0008f084d1c1d1c41082220676484c03100041f980000810d1c1d1fc00000038d1c41082"
    "2606721e4c013000220092b946c82456d2830681100a51d22041d1c41082720012398000"
    "00037601e3ab207946c824562248d3fc000950481211828312811239100b145e828313c1"
    "100b145ed1fc0009b3327201208123c1100f8598610000ea4e75203946c8245672001239"
    "80000003263c000018b24c031000d0812040d1fc0008ed80d1c672001210670000ba43f9"
    "400d5f5822711c00260441f13c002228009a2628006ad28353812405b4836c022403b481"
    "6f0224012206761e4c03100026045d832040d1c1d1c3d1fc0008f07e1082224093f946c8"
    "2456d3c1d3c3d3fc100a51cc128272001239800000037601e3ab207946c824562248d3fc"
    "000950481211828312811239100b145e828313c1100b145ed1fc0009b3327201208123c1"
    "100f8598220676484c03100026045d8341f980000810d1c1d1fc00000032d1c310826100"
    "00284e754feffff048d70470244428067a001a39800000034eb940027e1a4cd704704fef"
    "00104e754feffff048d704702a045d85244528067a001a39800000034eb940027e1a4cd7"
    "04704fef00104e75"
)

VCOUNT_MARK = bytes.fromhex("40bad000")
DCOUNT_MARK = bytes.fromhex("40bad004")


def legacy_bytes(addr):
    """CODE at `addr` with its two placeholders patched to the appended
    tables: the oracle for the linked source. The build never writes these
    bytes itself."""
    code = bytearray(CODE)
    vcount_at = addr + len(code)
    dcount_at = vcount_at + len(VERB_COUNTS)
    for mark, target in ((VCOUNT_MARK, vcount_at), (DCOUNT_MARK, dcount_at)):
        i = code.find(mark)
        assert i >= 0, "placeholder %s missing" % mark.hex()
        assert code.find(mark, i + 4) < 0, "placeholder %s not unique" % mark.hex()
        code[i:i + 4] = target.to_bytes(4, "big")
    return bytes(code) + VERB_COUNTS + DLY_COUNTS


# Where the oracle links the hand-assembled form: the address the ROM cave
# last landed at (bottleservice, 2f89c186).
ORACLE_AT = 0x400d26bc


MODULE = Module(
    name="cc-map",
    key="CC MAP",
    kind=Kind.CF_PATCH,
    category=Category.MIDI_USB, author="sambanks", author_url="https://github.com/sambanks",
    proof=Proof.HARDWARE, proof_note="Sam's MKII (image 96, 13 Sep 2026)",
    doc="MIDI CC 62-67 drive the FX2 engine's page-2 slots 6-11; CC 68-73 the FX1 station's.",
    # A DRAM unit (8 Oct 2026; a floating ROM cave until then): the CC
    # dispatch entry points at its entry symbol.
    # The two blocks' first CC, per project (BRAIN's PROJECT layer; the
    # defaults without BRAIN). A block is six CCs; the FX2 block wins where
    # the two overlap.
    store=Store(STORE),
    settings=(Setting(1, "FX2 CC", Number(0, 122), default=62, scope=Scope.PROJECT, apply=Apply.LIVE,
                      doc="the first of the six CCs that drive the FX2 effect's page-2 slots 6-11"),
              Setting(2, "FX1 CC", Number(0, 122), default=68, scope=Scope.PROJECT, apply=Apply.LIVE,
                      doc="the first of the six CCs that drive the FX1 effect's page-2 slots 6-11")),
    linked=(Linked("ccmap", "modules/cc-map/cc_map.s", dram=True, include=cc_inc,
                   reference=reference,
                   # Where other CCs go: stock's handler.
                   # CC_MODEDEF2 / CC_MODEDEF1: a stock `rts` unless MODE DEFAULTS is in
                   # the image, whose unit exports them.
                   defsyms=(("CC_NEXT", STOCK_CC), ("CC_MODEDEF2", STOCK_RTS),
                            ("CC_MODEDEF1", STOCK_RTS))),),
    symbol_refs=(SymbolRef(DISPATCH_CC, STOCK_CC, "ccmap", "ccm_entry",
                           "MIDI dispatch: the CC vector (CC 62-67 reach FX2 page 2, CC 68-73 FX1 page 2)"),),
    gates=(Gate('tools/verify/verify_ccmap.py', remix_arg=False, venv=True),),
)
