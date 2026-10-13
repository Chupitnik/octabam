#!/usr/bin/env python3
"""LOAD METER's DSP half under the ColdFire port, with both DSP cores running.

    python3 tools/verify/verify_loadmeter.py [remix] [--project DIR]

The remix (default `octabam-bus`) carries BRAIN and LOAD METER. Each run loads the
project, opens TOOLS > LOAD (or leaves it closed) and dumps LOAD METER's
lm_dsp (modules/load-meter/meter.s) and the dialog's lines and title.

  publish     with the view open: records arrive every frame (seq close to
              the frames run), periods in range, the dialog's DSP rows
              carry numbers, the title carries the refresh count
  scrub       a write watch on MAIN's long 0 low byte (0x80005e63): the
              scrub (lm_scrub) zeroes it every frame the DSP wrote it
  every       lm_scrub runs once per frame interrupt (its pass count equals
              lm_out's), so it precedes every reader of MAIN in the interrupt
              (the stock recorder at 0x4000d340, USB AUDIO OUT at 0x4000d9a0)
  usb         USB AUDIO OUT's ring (aud_ring, the producer forced on) is
              byte-identical between this build and an LM_NOPUB build
              (core 0 publishes nothing): no published byte reaches a reader
  closed      the view closed: no dialog draws (0x4006d128 never runs from
              the LOAD tick) and the host port carries the same blocks and
              words as with the view open over the same frames
  range       a DSP write watch on X:$37F28 (in the claimed $37F10-$37F2F,
              a word the meter never writes) on each core: only payload B's
              boot zero loop writes it
  calibrate   builds with LM_BURN = 0, 1000 and 3000 (a 1-cycle nop DO
              loop at each core's frame end): each core's mean busy over
              the run rises by the injected cycles in timer-0 counts (CLK/2,
              so N/2), within TOL; the error is printed

What it cannot see: the timer's rate and the burn's cost on the chip (the
port's cycle table), DMA timing, hardware jitter. Every % and count is a
hardware-round item.
"""
import argparse, os, pathlib, re, shutil, struct, subprocess, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1])); import toolpath  # noqa: E402,F401
from remix import registry  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
EMU = ROOT / "out/emu/ot_emu"
PY = ROOT / ".venv/bin/python3"
ELF = ROOT / "out/platform/runtime/runtime.elf"
OUT = ROOT / "out/loadmeter"
DLG_LINES, DLG_TITLE, DLG_DRAW = 0x460e5d28, 0x460e5ce4, 0x4006d128
MAIN_LO = 0x80005e63
TOL = 0.05                       # measured errors 12 Oct 2026: +1.6%, +0.3%, -0.1%, +0.2%
BURNS = (0, 1000, 3000)
FAILS = []


def check(what, ok):
    print(f"  [{'ok' if ok else 'FAIL'}] {what}")
    if not ok:
        FAILS.append(what)


def symbol(name):
    nm = shutil.which("m68k-elf-nm") or shutil.which("m68k-linux-gnu-nm")
    for line in subprocess.run([nm, str(ELF)], capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    sys.exit(f"verify_loadmeter: {name} not in {ELF}")


def build(remix, burn, tag, nopub=False):
    env = dict(os.environ, REMIX=remix, XBUS="1", SPEC="1")
    env.setdefault("BUILD", "0")
    env.pop("LM_BURN", None)
    env.pop("LM_NOPUB", None)
    if burn:
        env["LM_BURN"] = str(burn)
    if nopub:
        env["LM_NOPUB"] = "1"
    r = subprocess.run([sys.executable, str(ROOT / "tools/build/build_bus.py")], env=env,
                       capture_output=True, text=True, cwd=ROOT)
    if r.returncode:
        sys.exit(f"verify_loadmeter: building {remix} (LM_BURN={burn}) failed:\n{(r.stdout + r.stderr)[-1500:]}")
    image = OUT / f"mainos_{tag}.bin"
    shutil.copy2(ROOT / "out/mainos_bus.bin", image)
    return image, symbol("LM_DSP")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("remix", nargs="?", default="octabam-bus")
    ap.add_argument("--project", default=os.environ.get("OT_PROJECT", ""))
    ap.add_argument("--set-name", default="BRAINSET")
    ap.add_argument("--name", default="BRAINPRJ")
    ap.add_argument("--frames-ms", type=int, default=4000, help="emulated ms the view stays open")
    a = ap.parse_args()
    project = a.project
    if not project and pathlib.Path("~/.octabam_project").expanduser().is_file():
        project = pathlib.Path("~/.octabam_project").expanduser().read_text().strip()
    if not project:
        print("  [SKIP] verify_loadmeter: no project (OT_PROJECT=<dir>, --project or ~/.octabam_project)"); return 0
    if not EMU.is_file():
        print("  [SKIP] verify_loadmeter: no port binary (make emu-cf)"); return 0
    remix = registry.remix(a.remix)
    if "LOAD METER" not in remix.modules or "BRAIN" not in remix.modules:
        print(f"  [ -- ] verify_loadmeter: {a.remix} carries no LOAD METER and BRAIN"); return 0
    OUT.mkdir(parents=True, exist_ok=True)
    copy = OUT / "project"
    shutil.rmtree(copy, ignore_errors=True)
    copy.mkdir(parents=True)
    for f in pathlib.Path(project).expanduser().iterdir():
        if f.is_file() and f.suffix.lower() == ".work":
            shutil.copy2(f, copy / f.name)

    tools = [(0x1c, 500)] + [(0x20, 500)] * 4 + [(0x21, 500)] + [(0x20, 500)] * 4 + [(0x31, 500)]
    open_load = [(0x32, 500)] + tools + [(0x20, 500), (0x20, 500), (0x31, 500)]   # BRAIN > TOOLS > LOAD

    def run(tag, image, lmd, keys, wait, extra=()):
        card = OUT / f"{tag}.img"
        r = subprocess.run([str(PY), str(ROOT / "tools/emu/ot_emu/stage_card.py"), str(copy), a.set_name, a.name,
                            "--tree", str(OUT / f"tree_{tag}"), "--out", str(card)], cwd=ROOT,
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"verify_loadmeter: stage_card failed:\n{r.stdout[-800:]}{r.stderr[-800:]}")
        t, lines = 1500, []
        for code, gap in keys:
            lines += [f"{t} key {code:#x} down", f"{t + 30} key {code:#x} up"]
            t += gap
        lines.append(f"{t + wait} quit")
        (OUT / f"{tag}.script").write_text("\n".join(lines) + "\n")
        d_lmd, d_lines, d_title = OUT / f"{tag}_lmd.bin", OUT / f"{tag}_lines.bin", OUT / f"{tag}_title.bin"
        cmd = [EMU, "--image", image, "--card", card, "--set", a.set_name, "--project", a.name,
               "--load-ms", "90000", "--mkii", "--dsp", "--frame", "--live-script", OUT / f"{tag}.script",
               "--mem-dump", f"{lmd:#x},72={d_lmd};{DLG_LINES:#x},93={d_lines};{DLG_TITLE:#x},16={d_title}",
               *extra]
        log = OUT / f"{tag}.txt"
        with open(log, "w") as fh:
            r = subprocess.run(list(map(str, cmd)), cwd=ROOT, stdout=fh, stderr=subprocess.STDOUT)
        text = log.read_text()
        if r.returncode or "ended on quit" not in text:
            sys.exit(f"verify_loadmeter: the {tag} run did not finish -- {log}")
        v = struct.unpack(">18I", d_lmd.read_bytes())
        raw = d_lines.read_bytes()
        rows = [raw[i * 31:i * 31 + 30].split(b"\0")[0].decode("ascii", "replace") for i in range(3)]
        title = d_title.read_bytes().split(b"\0")[0].decode("ascii", "replace")
        return v, rows, title, text

    def host_totals(text, irqs):
        """Host commands per frame interrupt: writes to the HI08 command
        register (a --watch-mem on 0x20000004; the port prints no host-port
        summary under --live-script)."""
        n = sum(1 for ln in text.splitlines() if "[0x20000004]" in ln)
        return (round(n / irqs, 4),) if irqs and n else ()

    # ---- publish, scrub ------------------------------------------------------
    image, lmd = build(a.remix, 0, "b0")
    v, rows, title, text = run("open", image, lmd, open_load, a.frames_ms,
                               ("--watch-mem", f"{MAIN_LO:#x},1;0x20000004,2"))
    seq, bad = v[0], v[1]
    b0, p0, b1, p1 = v[2], v[3], v[4], v[5]
    check(f"publish: good records {seq}, bad {bad}; core 0 busy/period {b0}/{p0}, core 1 {b1}/{p1}",
          seq > 1000 and bad <= seq // 100 + 2 and 20000 <= p0 <= 60000 and 20000 <= p1 <= 60000
          and 0 < b0 <= p0 and 0 < b1 <= p1)
    check(f"view: the DSP rows carry numbers and the title the refresh count  {title!r} {rows}",
          rows[1].startswith("DSP0") and "-" not in rows[1].split(" M")[0] and "?" not in rows[1]
          and rows[2].startswith("DSP1") and "-" not in rows[2] and title.startswith("LOAD ")
          and title[5:].isdigit() and int(title[5:]) >= 2)
    clears = sum(1 for ln in text.splitlines() if f"[{MAIN_LO:#x}]" in ln and "<- 0 " in ln)
    sets = sum(1 for ln in text.splitlines() if f"[{MAIN_LO:#x}]" in ln and "<- 0 " not in ln)
    check(f"scrub: MAIN's long 0 low byte zeroed after landing ({clears} clears, {sets} other writes)",
          clears > 1000)
    check(f"every: lm_scrub's passes {v[13]} against frame interrupts {v[14]}", v[14] > 1000 and abs(v[13] - v[14]) <= 1)
    open_host = host_totals(text, v[14])

    # ---- closed: no draws, the same host traffic --------------------------------
    v, rows, title, text = run("closed", image, lmd, [(0x32, 500)] + tools + [(0x20, 500), (0x20, 500)],
                               a.frames_ms + 500, ("--watch-pc", f"{DLG_DRAW:#x}", "--dsp-watch", "0:X:37f28",
                                                   "--watch-mem", "0x20000004,2"))
    range_logs = [text]
    draws = next((int(ln.split(":")[1].split()[0]) for ln in text.splitlines()
                  if ln.startswith("watch-pc") and "hit(s)" in ln), -1)
    check(f"closed: the dialog draw never runs ({draws} hits) and the DSPs still publish (seq {v[0]})",
          draws == 0 and v[0] > 1000)
    closed_host = host_totals(text, v[14])
    check(f"closed: host commands per frame interrupt match the open run  open {open_host}, closed {closed_host}",
          bool(open_host) and len(open_host) == len(closed_host)
          and all(abs(x - y) <= 0.01 * max(x, 1) for x, y in zip(open_host, closed_host)))

    # ---- calibration: an injected burn raises busy by N/2 counts ----------------
    # A DO loop of N one-cycle nops costs N core cycles plus the DO's own
    # setup (a constant, gone in the difference between burns); timer 0 counts
    # CLK/2, so N cycles are N/2 counts. The port's timer moves in 1,024-count
    # steps (dsp56kEmu timers.cpp: m_timerupdateInterval = 2,048 instructions),
    # so the figure compared is each core's mean busy over the run (lm_dsum),
    # which the frame length (not a multiple of 2,048) dithers.
    busy = {}
    for n in BURNS:
        img_n, lmd_n = (image, lmd) if n == 0 else build(a.remix, n, f"b{n}")
        extra = ("--dsp-watch", "1:X:37f28") if n == BURNS[1] else ()
        v, _r, _t, text_n = run(f"burn{n}", img_n, lmd_n, open_load, a.frames_ms, extra)
        if extra:
            range_logs.append(text_n)
        busy[n] = (v[15] / max(v[17], 1), v[16] / max(v[17], 1))
    for core in (0, 1):
        for n in BURNS[1:]:
            rise = busy[n][core] - busy[0][core]
            want = n / 2
            check(f"calibrate: core {core} mean busy rises by {rise:.0f} counts for a {n}-cycle burn (expected {want:.0f}; "
                  f"error {100 * (rise - want) / want:+.1f}%, within {TOL:.0%})", abs(rise - want) <= TOL * want)

    # ---- range: nothing stock writes the claimed words but B's boot zero --------
    for core, log in enumerate(range_logs):
        writers = []
        on = False
        for ln in log.splitlines():
            if ln.startswith("dsp watch"):
                on = True
                continue
            if on:
                if not ln.startswith(" "):
                    break
                writers.append(ln.strip())
        stray = [w for w in writers if "pc 0x0004" not in w.lower() and "pc=0x4" not in w.lower()
                 and not any(f"{x:x}" in w.lower() for x in (0x49, 0x4a))]
        check(f"range: core {core}'s writers of X:$37F28: {len(writers)}, none but payload B's boot zero loop "
              f"(P:$49/$4a)  {stray[:3]}", not stray)

    # ---- usb: no published byte reaches USB AUDIO OUT's ring --------------------
    sym = {}
    nm = shutil.which("m68k-elf-nm") or shutil.which("m68k-linux-gnu-nm")
    rings = []
    for tag, nopub in (("usb_on", False), ("usb_off", True)):
        img_u, _l = (image, lmd) if not nopub else build(a.remix, 0, "nopub", nopub=True)
        for line in subprocess.run([nm, str(ELF)], capture_output=True, text=True).stdout.splitlines():
            parts = line.split()
            if len(parts) == 3:
                sym[parts[2]] = int(parts[0], 16)
        if "aud_ring" not in sym:
            check("usb: the remix carries USB AUDIO OUT (aud_ring)", False)
            break
        card = OUT / f"{tag}.img"
        subprocess.run([str(PY), str(ROOT / "tools/emu/ot_emu/stage_card.py"), str(copy), a.set_name, a.name,
                        "--tree", str(OUT / f"tree_{tag}"), "--out", str(card)], cwd=ROOT, check=True,
                       capture_output=True)
        ring = OUT / f"{tag}_ring.bin"
        log = OUT / f"{tag}.txt"
        with open(log, "w") as fh:
            r = subprocess.run(list(map(str, [EMU, "--image", img_u, "--card", card, "--set", a.set_name,
                                              "--project", a.name, "--sequencer", "--internal-clock",
                                              "--frames", "3000", "--load-ms", "90000", "--dsp",
                                              "--poke", f"{sym['aud_force']:#x}=1",
                                              "--mem-dump", f"{sym['aud_ring']:#x},8192={ring}"])),
                               cwd=ROOT, stdout=fh, stderr=subprocess.STDOUT)
        if r.returncode:
            sys.exit(f"verify_loadmeter: the {tag} run failed -- {log}")
        rings.append(ring.read_bytes())
    if len(rings) == 2:
        diff = sum(1 for x, y in zip(*rings) if x != y)
        lows = sum(1 for i in range(3, len(rings[0]), 4) if rings[0][i])
        check(f"usb: USB AUDIO OUT's ring byte-identical with the publish on and off ({diff} bytes differ; "
              f"{lows} nonzero low bytes with it on)", diff == 0 and lows == 0)
    if FAILS:
        print(f"verify_loadmeter: FAILED ({len(FAILS)} failure(s)) -- {OUT}")
        return 1
    print(f"verify_loadmeter: ok (0 failure(s)) -- {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
