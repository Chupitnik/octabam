"""VARISPEED -- STATIC/FLEX TSTR VARI: tempo-locked varispeed, QUAN on PTCH, nothing emulated.

Source: `upstream/` is Zac Kyoti's repository (Zac-Kyoti/octatrack-kyoti-fw,
submodule, pinned to `8fe11df`). The declaration is
`upstream/octabam-modules/varispeed/manifest.py`: the linked unit `vari_cf`
(`varispeed.s`, VARISPEED_EMUS's source with its switch at 0, read from
`../varispeed-emus/` in the same checkout), re-linked and compared with the author's own
bytes (`reference`) every build, and `vari_store` (`varispeed_store.s`, its STORE handlers),
both placed in DRAM or a ROM cave by the `PLACE` setting (default DRAM); 16 detours and four
pokes; no DSP section. It requires STORE: the PART menu's CLEAR (0x4004a9d0) is STORE's
site, and VARISPEED holds and forgets there as a client (partclear, partsaved). Its source paths are
derived from its own directory, so it is executed here from the source on disk,
as the registry does for every manifest, and this file only re-exports its
MODULE. Nothing inside `upstream/` is edited here.

On hardware: the author's MKI, 15 Oct 2026 (VARI1 build).
"""

import dataclasses
import pathlib
import runpy

from remix.schema import Category, Proof

_UPSTREAM = pathlib.Path(__file__).resolve().parent / "upstream" / "octabam-modules" / "varispeed" / "manifest.py"

MODULE = runpy.run_path(str(_UPSTREAM), run_name="remix_manifest_varispeed")["MODULE"]
# The module table's fields are octabam's (README.md, `make docs`), so they
# are added here rather than in the author's manifest.
MODULE = dataclasses.replace(
    MODULE, category=Category.MACHINES, author="Zac-Kyoti/octatrack-kyoti-fw", author_url="https://github.com/Zac-Kyoti/octatrack-kyoti-fw",
    proof=Proof.HARDWARE, proof_note="the author's MKI, 15 Oct 2026 (the VARI1 build, standalone image)")
