#!/usr/bin/env python3
"""COPY PROJECT under the ColdFire port: card-to-card folder duplicate.

    python3 tools/verify/verify_copy_project.py [REMIX]
    make check REMIX=copy-project   # runs this as the module gate

Stages a project as SRC, opens PROJECT > COPY PROJECT, picks it, accepts
the stock default name (`PROJECT YYMMDD` under a pinned RTC), confirms,
answers NO on OPEN IT?, flushes the card, and checks the destination
folder holds the same project.work bytes as the source.

SKIPs without the port or a project. Cannot see: a directory past EMAX
entries, a name past NLEN, copy while the sequencer plays (hardware).
"""
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import toolpath  # noqa: E402,F401
from remix import registry  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
EMU = ROOT / "out/emu/ot_emu"
PY = ROOT / ".venv/bin/python3"
OUT = ROOT / "out/copy_project_verify"
# 2026-10-14 12:00:00 UTC — same epoch as verify_rnd_name
RTC_EPOCH = 1791979200
DST = "PROJECT 261014"


def press(p, name, n=1, settle=80):
    for _ in range(n):
        p.press(name, settle=settle)


def menu_copy_project(p):
    from panel_server import popup_geometry  # noqa: PLC0415

    if popup_geometry(p.rt.uc) != (5, 0, 0xf6, 0x40):
        p.open_menu()
    press(p, "left")
    press(p, "up", 5)
    press(p, "down", 0)  # PROJECT
    press(p, "right")
    press(p, "up", 20)
    press(p, "down", 5)  # COPY PROJECT (after SAVE TO NEW)


def main() -> int:
    remix = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("REMIX")
    if not remix:
        sys.exit("verify_copy_project: remix name required")
    if "COPY PROJECT" not in registry.remix(remix).modules:
        print(f"  [ -- ] verify_copy_project: {remix} carries no COPY PROJECT")
        return 0
    if not EMU.is_file():
        print("  [SKIP] verify_copy_project: no port binary (make emu-cf)")
        return 0
    pdir = pathlib.Path(os.environ.get(
        "OT_PROJECT", str(ROOT / "template_project/!DEF_N1"))).expanduser()
    if not (pdir / "project.work").is_file():
        print(f"  [SKIP] verify_copy_project: no project at {pdir}")
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, REMIX=remix, XBUS="1", SPEC="1")
    env.setdefault("BUILD", "0")
    r = subprocess.run([sys.executable, str(ROOT / "tools/build/build_bus.py")],
                       env=env, capture_output=True, text=True, cwd=ROOT)
    if r.returncode:
        sys.exit(f"verify_copy_project: build failed:\n{(r.stdout + r.stderr)[-1500:]}")
    image = OUT / "mainos.bin"
    image.write_bytes((ROOT / "out/mainos_bus.bin").read_bytes())

    fixture = OUT / "src"
    if fixture.exists():
        shutil.rmtree(fixture)
    shutil.copytree(pdir, fixture)
    src_work = (fixture / "project.work").read_bytes()

    card, tree = OUT / "card.img", OUT / "tree"
    r = subprocess.run(
        [str(PY), str(ROOT / "tools/emu/ot_emu/stage_card.py"),
         str(fixture), "OCTABAM", "SRC", "--tree", str(tree), "--out", str(card)],
        cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"verify_copy_project: stage_card failed:\n{r.stdout[-800:]}{r.stderr[-800:]}")

    sys.path.insert(0, str(ROOT / "tools/verify"))
    from stems_panel import MenuPort  # noqa: E402

    log = OUT / "port.log"
    with MenuPort(image.resolve(), card.resolve(), "OCTABAM", "SRC", True,
                  log_path=log, extra=("--rtc", str(RTC_EPOCH))) as p:
        p.settle()
        menu_copy_project(p)
        press(p, "yes", settle=400)          # open list
        press(p, "yes", settle=600)          # pick SRC -> name editor
        press(p, "yes", settle=400)          # accept PROJECT YYMMDD
        press(p, "yes", settle=400)          # confirm COPY SRC -> DST?
        p.run(20000)                         # engine job 21
        press(p, "no", settle=400)           # OPEN IT? -> leave live project
        p.card_flush()

    import emu_card  # noqa: E402

    files = emu_card.extract_image(card.read_bytes())
    dst_key = next((k for k in files if k.endswith(f"/{DST}/project.work")), None)
    if not dst_key:
        sample = sorted(k for k in files if "project.work" in k)[:12]
        sys.exit(f"verify_copy_project: no {DST}/project.work on card; "
                 f"project.work paths={sample} -- {log}")
    if files[dst_key] != src_work:
        sys.exit(f"verify_copy_project: {dst_key} differs from source "
                 f"({len(files[dst_key])} vs {len(src_work)} bytes) -- {log}")
    src_key = next((k for k in files if k.endswith("/SRC/project.work")), None)
    if not src_key:
        sys.exit(f"verify_copy_project: SRC/project.work missing after copy -- {log}")
    src_prefix = src_key[: -len("project.work")]
    dst_prefix = dst_key[: -len("project.work")]
    missing = []
    for k in files:
        if not k.startswith(src_prefix):
            continue
        peer = dst_prefix + k[len(src_prefix):]
        if peer not in files:
            missing.append(peer)
    if missing:
        sys.exit(f"verify_copy_project: destination missing {missing[:8]} -- {log}")
    print(f"  [PASS] verify_copy_project: {DST!r} matches SRC "
          f"({len(src_work)}-byte project.work, {len(files) - len(missing)} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
