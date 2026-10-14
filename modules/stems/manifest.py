"""STEM REC -- tracks to the card while the sequencer plays, streamed.

MAIN MENU > PROJECT > STEMS > REC arms a recording, or starts one if the
sequencer is running; selecting it again stops it, and so does the
sequencer stopping or 60 minutes. T1 to T8, MAIN, CUE, AB and CD turn
sources on and off (the eight tracks at boot; locked during a take);
AB STEREO and CD STEREO (on at boot) record an input pair as one stereo
file or two mono ones; 24 BIT (off at boot) records 24-bit files. Each
source is a file, <set>/AUDIO/YYMMDD-HHMM/<name>.wav: the tracks after
the fader, MAIN and CUE as mixed, the inputs raw. stems_peak keeps the
take's largest ring fill, for the menu's PEAK row. Design:
git show 4d2d6456:docs/superpowers/specs/2026-09-22-stem-rec-streaming-design.md; the menu,
git show 4d2d6456:docs/superpowers/specs/2026-09-28-stem-rec-menu-design.md.
Every stock fact the unit uses: docs/firmware/STEM_REC.md.

HOW, in one breath: a detour at the per-frame routine's only call site
(0x40004b12) mirrors core 0's MAIN gains every frame and, while a take
runs, packs each source into its file's own ring in the 8 MiB ring region,
through the uncached alias, as the file's bytes: each track's read-back
block times its gain (its share of MAIN), MAIN and CUE as mixed, the
inputs raw; an RTOS task of the module's own streams each ring's whole
sectors to the card while the take runs, straight from the ring (zero
copy), through the file layer's raw sector routines (never the buffered
API or its shared staging buffer), then rewrites each file's header and
sets its length; the ring and the task's stack are DramRegions at the
free top of the platform reserve, so the module costs no sample memory
beyond what any DRAM remix already gives up. A third region, `stems_buf`,
holds fourteen 512-byte sector-0 copies (the header's fix).

Placement: STEMS is a row under PROJECT (not a fifth MAIN MENU root
category), so BRAIN can own the root's fifth slot. The ledger refuses two
modules growing MAIN MENU's root.

On hardware: STEMS1 to STEMS3 on Yves's MKII (docs/firmware/STEM_REC.md
section 17) used the earlier root placement; PROJECT placement is port-gated.
"""

import pathlib

from remix.schema import Category, Detour, DramRegion, Gate, Kind, Linked, Module, Poke, Proof, TableGrow

# The per-frame routine's only call site, inside the audio interrupt at
# IPL 5: `jsr %pc@(0x400031a0)` then `move.w #0x2700,%sr` (cfprobe's site).
# The hook performs both, so a jsr + nop replaces them.
FRAME_SITE = 0x40004B12
FRAME_STOCK = bytes.fromhex("4ebae68c" "46fc2700")

# The stock PIO write routine (0x40014c48), right after its wait for DRQ:
# `movea.l 0x46c8c594,%a0`, the first sector's data. The stub streams the
# sector after advancing the handler's pointer and count, not before
# (docs/firmware/STEM_REC.md 11.7), and returns through stock's epilogue.
ATA_FIRST_SITE = 0x40014CFE
ATA_FIRST_STOCK = bytes.fromhex("207946c8c594")

# MAIN MENU > PROJECT (docs/firmware/MAINMENU.md §2): list descriptor
# 0x400cbcac (count at +0x00, row array at +0x18), stock 15 rows at
# 0x400cc308. TableGrow copies those rows and appends STEMS: label +
# action stems_enter (YES from a non-root list never follows +0x10;
# stems_enter repoints PROJECT onto stems_rows so the pane redraws).
# Window/getter/child/id zero. The STEMS rows are in stems.s.
PROJECT_DESC = 0x400CBCAC
PROJECT_ROWS = 0x400CC308
PROJECT_N, ROW_WORDS = 15, 6

# Core 0's gain table (X:0x6c00, 258 words, 3 bytes each, little-endian)
# from the USER'S stock slice at build time: the OS reuses the RAM the
# image holds it in after the DSP upload (docs/firmware/STEM_REC.md 18.4),
# and the repository carries no Elektron byte.
STOCK_SLICE = pathlib.Path(__file__).resolve().parents[2] / "out/raw/section_3_MAIN_OS.bin"
GTAB_OFF, GTAB_LEN = 0x400EA18A - 0x40000400, 258 * 3


def gtab_inc(modules):
    """remix.inc for stems.s: the label stems_gtab and the table's bytes."""
    return ("| remix.inc -- STEM REC's copy of core 0's gain table\n"
            "        .global stems_gtab\n"
            "stems_gtab:\n"
            f"        .incbin \"{STOCK_SLICE}\", {GTAB_OFF:#x}, {GTAB_LEN}\n")

MODULE = Module(
    name="stems",
    key="STEM REC",
    kind=Kind.CF_PATCH,
    category=Category.MACHINES, author="yvesrosius", author_url="https://github.com/yvesrosius",
    proof=Proof.HARDWARE, proof_note="Yves's MKII: STEMS1 (30 Sep 2026), T1-T8 for about two minutes; "
        "STEMS3 (6 Oct 2026), T1-T8 after the fader, MAIN and AB at 16 and 24 bits",
    doc="Multitrack recording to the card: each track (after its fader), MAIN, CUE and the "
        "inputs as separate WAV files, 16 or 24 bits, up to 60 minutes. MAIN MENU > PROJECT > STEMS.",
    linked=(Linked("stems", "modules/stems/stems.s", dram=True, include=gtab_inc),),
    detours=(Detour(FRAME_SITE, FRAME_STOCK, "stems", "stems_frame_hook",
                    "per-frame tap: the enabled tracks into the ring, then the stock routine",
                    kind="jsr", pad_to=8),
             Detour(ATA_FIRST_SITE, ATA_FIRST_STOCK, "stems", "stems_ata_first",
                    "the PIO write's first sector, pointer and count advanced first",
                    kind="jmp"),),
    tables=(TableGrow("PROJECT + STEMS", old=PROJECT_ROWS,
                      count=PROJECT_N * ROW_WORDS,
                      symbols=(("stems", "stems_cat_label"), ("stems", "stems_zero"),
                               ("stems", "stems_enter"), ("stems", "stems_zero"),
                               ("stems", "stems_zero"), ("stems", "stems_zero")),
                      refs=((PROJECT_DESC + 0x18, PROJECT_ROWS),)),),
    pokes=(Poke(PROJECT_DESC, expect=(PROJECT_N).to_bytes(4, "big"),
                write=(PROJECT_N + 1).to_bytes(4, "big"), note="PROJECT rows 15 -> 16 (STEMS)"),),
    requires=("MEMORY",),
    dram_regions=(DramRegion("stems_ring", 0x800000),
                  DramRegion("stems_stack", 0x2000),
                  DramRegion("stems_buf", 14 * 512, align=512),),
    # verify_stems builds its own stems image and its fixture cards (from the
    # project template STEMS_TEMPLATE names, default out/projects/Ultimate FX
    # 1.5.3; it SKIPs the port runs by name without one) and runs the takes
    # under the port. The card reader's round trip is remix-independent:
    # every take is read back through it. verify_stems_menu boots the image
    # twice (MKII, MKI) under `ot_emu --interactive` and presses the menu's
    # keys; it SKIPs by name without the port or the template.
    gates=(Gate("tools/verify/verify_card_reader.py", remix_arg=False, venv=True),
           Gate("tools/verify/verify_stems.py", venv=True),
           Gate("tools/verify/verify_stems_menu.py", venv=True)),
)
