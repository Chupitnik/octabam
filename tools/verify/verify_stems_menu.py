#!/usr/bin/env python3
"""STEMS under PROJECT in MAIN MENU, key by key under the port, on both panels.

    python3 tools/verify/verify_stems_menu.py [remix]      (default: stems)

Builds the remix and the fixtures (STEMS_TEMPLATE), then boots the image
twice under `ot_emu --interactive` on a copy of the one-track fixture card:
as an MKII (MAIN MENU = PROJ) and as an MKI (FUNC+MIXER). Each boot:
- the cursor walks the stock root categories (four, or five with BRAIN);
  STEMS is not among them;
- SYSTEM opens and the cursor reaches OS UPGRADE, the recovery path; ENTER
  is never pressed there;
- PROJECT opens, DOWN reaches STEMS (last row), YES opens the STEMS list
  on REC; DOWN lands on T1 and UP on REC, the status row skipped both ways;
  DOWN reaches 24 BIT and stays there: PEAK, the last row, is never reached
  (a row with action 0 is skipped one at a time and never moved onto at the
  end, STEM_REC.md 16.1);
- every track row turned off in turn leaves T8 on (the last source
  stays); with MAIN on, T8 can go off and MAIN can't; the three switches
  flip and say so; T3 off, then every other track on again;
- REC arms (CANCEL, ARMED) and cancels (REC, READY), then arms;
- with the menu closed, PLAY; the menu reopened on STEMS (PROJECT >
  STEMS again after the frame hook restored PROJECT) shows STOP and
  REC mm:ss, and a key later the seconds have moved on (the menu redraws
  on keys only, STEM_REC.md 16.1); T3 is locked;
- with the menu closed, STOP; the take ends DONE, and its folder holds
  seven tracks and MAIN, T3's missing;
- every text the module can show, drawn in the status row, ends left of
  the pane's clip edge, and STEMS left of the PROJECT column's (checked
  before the take — post-take --dsp runs wedge the interactive port);
The keys, as measured (STEM_REC.md 16.1): YES enters a list, LEFT goes
back a level, NO closes the whole menu. STEMS is opened by stems_enter
(not a child pointer); while it is shown, DOWN then UP redraws and keeps
REC. A PNG of the screen at each step goes to
out/stems_runs/menu-<model>-*.png. Pass or fail comes from memory, never
from a picture.

What it cannot see: the unit's key timing, its card speed while the menu
is open, and the MKII's hardware (flash A)."""
import json
import os
import pathlib
import shutil
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1])); import toolpath  # noqa: E402,F401
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import verify_stems as vs  # noqa: E402
from stems_panel import MENU_FOCUS, MenuPort, diff_box  # noqa: E402

ROOT_DESC, PROJECT_DESC, SYSTEM_DESC, SEL = 0x400cbd8c, 0x400cbcac, 0x400cbd1c, 0x0c
OS_UPGRADE = 1                  # SYSTEM's row 1 (MAINMENU.md 2)
STEMS_PROJ_ROW = 15             # PROJECT's appended STEMS row (stock 15 → 16)
ENTER, BACK, CLOSE = "yes", "left", "no"
WORK = pathlib.Path("out/stems_runs")
ERRS = ["RING FULL", "PATH FAILED", "OPEN FAILED", "SAME MINUTE", "WRITE FAILED",
        "SEEK FAILED", "CLOSE FAILED", "TASK FAILED"]
TEXTS = (["REC", "CANCEL", "STOP", "SAVING", "READY", "ARMED", "NO CARD", "PEAK 0%", "PEAK 100%",
          "REC 60:00", "DONE 60:00"] + ERRS + [f"T{k} [{m}]" for k in range(1, 9) for m in "X "] + ["T8 MASTER"]
         + [f"{n} [{m}]" for n in ("MAIN", "CUE", "AB", "CD", "AB STEREO", "CD STEREO", "24 BIT") for m in "X "])


def texts(p, s):
    return [p.cstr(p.long(s["stems_rows"] + vs.ROW_LEN * k)) for k in range(vs.MENU_ROWS)]


def redraw(p):
    """Force a menu redraw while STEMS is showing: DOWN then UP keeps REC
    selected. Do not LEFT — that returns to the root, restores PROJECT, and
    YES would hit REC (stems_action) instead of re-entering STEMS."""
    p.press("down", settle=80)
    p.press("up", settle=80)


def box_of(p, label_at, scratch, text, redraw_fn=redraw):
    p.poke(scratch, text.encode("latin1") + b"\0")
    p.poke(label_at, scratch.to_bytes(4, "big"))
    # Short emulated settle + hard wall cap: widths must not wedge on DSP.
    p.run(40, wall=1.5)
    redraw_fn(p)
    return p.screen()


def widths(p, s, model):
    """Every text in the status row, and STEMS in the PROJECT column, against
    24 Ws: each must end left of the clip edge. The label pointers are put
    back after. Call while STEMS is open (idle — before a take): post-take
    --dsp runs starve the interactive port. Cursor on REC."""
    scratch = s["stems_fpath"]
    # --- list pane (STEMS rows, still swapped onto PROJECT) ---
    label_at = s["stems_rows"] + vs.ROW_LEN
    keep = p.long(label_at)
    empty = box_of(p, label_at, scratch, "")
    clip = diff_box(empty, box_of(p, label_at, scratch, "W" * 24))
    over = []
    for t in TEXTS:
        b = diff_box(empty, box_of(p, label_at, scratch, t))
        if clip is None or b is None or b[1] >= clip[1]:
            over.append((t, b[1] if b else None))
    p.poke(label_at, keep.to_bytes(4, "big"))
    redraw(p)
    vs.check(f"{model}: every text ends inside the list pane", clip is not None and not over,
             f"clip {clip[1] if clip else None}, over {over}")

    # --- PROJECT column: STEMS label only visible after PROJECT is restored ---
    grown_rows = p.long(s["stems_proj_save"] + 0x18)
    label_at = grown_rows + vs.ROW_LEN * STEMS_PROJ_ROW
    keep = p.long(label_at)
    p.press(BACK)                                   # root; frame hook restores PROJECT
    p.run(80, wall=2.0)
    p.press("right")                                # PROJECT, cursor on STEMS (saved sel)

    def redraw_proj(p):
        p.press("up", settle=80)
        p.press("down", settle=80)                  # stay on STEMS, force redraw

    empty = box_of(p, label_at, scratch, "", redraw_proj)
    clip = diff_box(empty, box_of(p, label_at, scratch, "W" * 24, redraw_proj))
    over = []
    for t in ("STEMS",):
        b = diff_box(empty, box_of(p, label_at, scratch, t, redraw_proj))
        if clip is None or b is None or b[1] >= clip[1]:
            over.append((t, b[1] if b else None))
    p.poke(label_at, keep.to_bytes(4, "big"))
    redraw_proj(p)
    vs.check(f"{model}: every text ends inside the PROJECT column", clip is not None and not over,
             f"clip {clip[1] if clip else None}, over {over}")
    p.press(ENTER)                                  # back into STEMS (REC) for the take


def enter_stems(p, s, model):
    """From the root with PROJECT selected: YES into PROJECT, DOWN to STEMS,
    YES to swap PROJECT's rows onto stems_rows (REC selected)."""
    p.press(ENTER)                                  # PROJECT
    vs.check(f"{model}: PROJECT opens", p.long(MENU_FOCUS) == PROJECT_DESC,
             f"focus {p.long(MENU_FOCUS):#x}")
    # Stock PROJECT: 11 selectable rows (separators skipped), then STEMS.
    # Cursor opens on CHANGE (row 1); 11 DOWNs land on STEMS (row 15).
    for _ in range(11):
        p.press("down")
    shot_sel = p.long(PROJECT_DESC + SEL)
    vs.check(f"{model}: PROJECT cursor reaches STEMS", shot_sel == STEMS_PROJ_ROW,
             f"selection {shot_sel}")
    p.press(ENTER)
    # stems_enter repoints PROJECT onto stems_rows; focus stays PROJECT.
    vs.check(f"{model}: STEMS opens on REC",
             p.long(MENU_FOCUS) == PROJECT_DESC and sel(p, PROJECT_DESC) == 0
             and p.long(PROJECT_DESC + 0x18) == s["stems_rows"],
             f"focus {p.long(MENU_FOCUS):#x}, selection {sel(p, PROJECT_DESC)}, "
             f"rows {p.long(PROJECT_DESC + 0x18):#x}")


def reopen_stems(p, s, model):
    """After NO closed the menu (frame hook restored PROJECT): from the root,
    YES into PROJECT (saved selection is still STEMS), YES into STEMS."""
    if (p.long(MENU_FOCUS) == PROJECT_DESC
            and p.long(PROJECT_DESC + 0x18) == s["stems_rows"]):
        return
    if p.long(MENU_FOCUS) != ROOT_DESC:
        p.press(BACK)
        p.run(400)
    while sel(p, ROOT_DESC) != 0:
        p.press("up")
    p.press(ENTER)                                  # PROJECT; sel restored to STEMS
    vs.check(f"{model}: PROJECT reopens on STEMS",
             p.long(MENU_FOCUS) == PROJECT_DESC and sel(p, PROJECT_DESC) == STEMS_PROJ_ROW,
             f"focus {p.long(MENU_FOCUS):#x}, selection {sel(p, PROJECT_DESC)}")
    p.press(ENTER)                                  # stems_enter
    vs.check(f"{model}: STEMS reopens on REC",
             p.long(MENU_FOCUS) == PROJECT_DESC and sel(p, PROJECT_DESC) == 0
             and p.long(PROJECT_DESC + 0x18) == s["stems_rows"],
             f"focus {p.long(MENU_FOCUS):#x}, selection {sel(p, PROJECT_DESC)}, "
             f"rows {p.long(PROJECT_DESC + 0x18):#x}")


def sel(p, desc):
    return p.long(desc + SEL)


def walk(model, s, fx):
    card = WORK / f"menu-{model}.img"
    shutil.copyfile(fx["card"], card)
    with MenuPort(vs.IMAGE, card, fx["set"], fx["project"], model == "mkii",
                  log_path=WORK / f"menu-{model}.log") as p:
        def shot(name):
            p.png(WORK / f"menu-{model}-{name}.png")

        p.settle()
        p.open_menu()
        shot("root")
        root_n = p.long(ROOT_DESC)
        seen = [sel(p, ROOT_DESC)]
        for _ in range(root_n - 1):
            p.press("down")
            seen.append(sel(p, ROOT_DESC))
        shot("root-last")
        vs.check(f"{model}: MAIN MENU opens on the root and the cursor walks {root_n} categories",
                 p.long(MENU_FOCUS) == ROOT_DESC and seen == list(range(root_n)), f"{seen}")
        # SYSTEM is always root selection 1 (PROJECT=0, SYSTEM=1, ...).
        while sel(p, ROOT_DESC) != 1:
            p.press("up")
        p.press(ENTER)
        p.press("down")
        shot("os-upgrade")
        vs.check(f"{model}: SYSTEM opens and the cursor reaches OS UPGRADE",
                 p.long(MENU_FOCUS) == SYSTEM_DESC and sel(p, SYSTEM_DESC) == OS_UPGRADE,
                 f"focus {p.long(MENU_FOCUS):#x}, selection {sel(p, SYSTEM_DESC)}")
        p.press(BACK)                                   # the root, SYSTEM selected
        while sel(p, ROOT_DESC) != 0:
            p.press("up")                               # PROJECT
        enter_stems(p, s, model)
        shot("stems")
        p.press("down")
        down = sel(p, PROJECT_DESC)
        p.press("up")
        up = sel(p, PROJECT_DESC)
        vs.check(f"{model}: DOWN from REC lands on T1 and UP from T1 on REC", (down, up) == (vs.ROW_TRK0, 0),
                 f"{(down, up)}")
        walked = []
        for _ in range(16):
            p.press("down")
            walked.append(sel(p, PROJECT_DESC))
        shot("last")
        last = 16
        vs.check(f"{model}: DOWN reaches 24 BIT and stays there, never on the PEAK row",
                 walked == list(range(vs.ROW_TRK0, last + 1)) + [last], f"{walked}")
        for _ in range(last - vs.ROW_TRK0):
            p.press("up")                               # back to T1
        for k in range(8):                              # every track off in turn, T1 to T8
            p.press(ENTER)
            if k < 7:
                p.press("down")                         # no DOWN past T8: it might wrap
        left = p.long(s["stems_tracks"]) & 0xFF
        vs.check(f"{model}: turning every track off leaves the last one on", left == 0x80, f"mask {left:#04x}")
        p.press("down")                                 # T8 -> MAIN (row 10)
        p.press(ENTER)                                  # MAIN on
        p.press("up")                                   # T8
        p.press(ENTER)                                  # T8 off: MAIN keeps a source
        p.press("down")                                 # MAIN
        p.press(ENTER)                                  # MAIN off: refused, the last source
        left = p.long(s["stems_tracks"]) & 0xFFF
        vs.check(f"{model}: with MAIN on, T8 can go off, and the last source stays on",
                 left == 0x100, f"sources {left:#05x}")
        p.press("up")                                   # T8
        p.press(ENTER)                                  # T8 on again; MAIN stays on for the take
        for _ in range(7):
            p.press("down")                             # T8 (row 9) -> 24 BIT (row 16)
        p.press(ENTER)
        p.press(ENTER)                                  # 24 BIT on, then off
        p.press("up")                                   # CD STEREO (row 15)
        p.press(ENTER)                                  # off
        fmt = p.long(s["stems_fmt"]) & 7
        t = texts(p, s)
        vs.check(f"{model}: the switches flip and say so",
                 fmt == 0b010 and t[14:17] == ["AB STEREO [X]", "CD STEREO [ ]", "24 BIT [ ]"],
                 f"fmt {fmt:03b}, {t[14:17]}")
        p.press(ENTER)                                  # CD STEREO on again
        for _ in range(6):
            p.press("up")                               # CD STEREO (row 15) -> T8 (row 9)
        shot("tracks-off")
        for _ in range(7):
            p.press("up")                               # T8 -> T1; then T1 .. T7 on again, T3 stays off
        for k in range(7):
            if k != 2:
                p.press(ENTER)
            p.press("down")
        t = texts(p, s)
        mask = p.long(s["stems_tracks"]) & 0xFF
        trows = t[vs.ROW_TRK0:vs.ROW_TRK0 + 8]
        vs.check(f"{model}: T3 off, every other track on, the rows say so",
                 mask == 0xFB and trows == [f"T{k} [{' ' if k == 3 else 'X'}]" for k in range(1, 9)],
                 f"mask {mask:#04x}, {trows}")
        for _ in range(8):
            p.press("up")                               # T8 -> T1, then REC
        widths(p, s, model)                              # before the take: idle DSP, STEMS on REC
        p.press(ENTER)
        a = texts(p, s)[:2] + [p.long(s["stems_state"])]
        p.press(ENTER)
        c = texts(p, s)[:2] + [p.long(s["stems_state"])]
        p.press(ENTER)
        shot("armed")
        vs.check(f"{model}: REC arms and cancels", a == ["CANCEL", "ARMED", vs.ST_ARMED]
                 and c == ["REC", "READY", vs.ST_IDLE] and p.long(s["stems_state"]) == vs.ST_ARMED,
                 f"{a}, {c}")
        p.press(CLOSE)                                  # NO closes the menu; frame hook restores PROJECT
        p.press("play", settle=1500)
        p.open_menu()
        reopen_stems(p, s, model)
        t0 = texts(p, s)
        before = p.screen()
        shot("recording")
        p.run(2500, wall=30)
        redraw(p)
        t1 = texts(p, s)
        after = p.screen()
        shot("recording-later")
        vs.check(f"{model}: reopened on STEMS: STOP and REC mm:ss, and a key later the seconds moved on",
                 p.long(MENU_FOCUS) == PROJECT_DESC and p.long(PROJECT_DESC + 0x18) == s["stems_rows"]
                 and t0[0] == "STOP" and t0[1].startswith("REC ")
                 and t1[1].startswith("REC ") and t1[1] > t0[1] and diff_box(before, after) is not None,
                 f"{t0[:2]} -> {t1[:2]}")
        for _ in range(3):
            p.press("down")                             # REC -> T1 -> T2 -> T3
        p.press(ENTER)
        vs.check(f"{model}: T3 is locked while it records",
                 p.long(s["stems_tracks"]) & 0xFF == 0xFB and texts(p, s)[vs.ROW_TRK0 + 2] == "T3 [ ]")
        p.press(CLOSE)
        p.press("stop", settle=500)
        for _ in range(60):
            if p.long(s["stems_state"]) == vs.ST_IDLE:
                break
            p.run(500, wall=5)
        p.open_menu()
        reopen_stems(p, s, model)
        t = texts(p, s)
        shot("done")
        vs.check(f"{model}: after STOP: IDLE, REC and DONE mm:ss",
                 p.long(s["stems_state"]) == vs.ST_IDLE and t[0] == "REC" and t[1].startswith("DONE "),
                 f"state {p.long(s['stems_state'])}, {t[:3]}")
        p.card_flush()
    names = [n.upper() for n, _ in vs.take_files(str(card))]
    vs.check(f"{model}: the take holds seven tracks and MAIN, T3's missing",
             names == ["MAIN.WAV"] + [f"T{k}.WAV" for k in (1, 2, 4, 5, 6, 7, 8)], f"{names}")


def main():
    from remix import registry
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    name = args[0] if args else "stems"
    if "STEM REC" not in registry.remix(name).modules:
        print(f"  [ -- ] {name} does not carry STEM REC -- nothing to check")
        return 0
    env = {**os.environ, "REMIX": name, "XBUS": "1", "SPEC": "1"}
    r = subprocess.run([sys.executable, "tools/build/build_bus.py"], capture_output=True, text=True, env=env)
    if r.returncode:
        tail = (r.stdout + r.stderr).strip().splitlines()
        sys.exit(f"{name}: build failed: {tail[-1] if tail else '?'}")
    if not vs.EMU.exists():
        print("  [SKIP] the menu under the port: build the port (make emu-cf)")
        return 0
    if not vs.TEMPLATE.is_dir():
        print(f"  [SKIP] the menu under the port: no project template at {vs.TEMPLATE} (STEMS_TEMPLATE=<dir>)")
        return 0
    if not vs.fixtures():
        return 1
    s = vs.syms()
    fx = json.loads(vs.FIXTURE.read_text())
    WORK.mkdir(parents=True, exist_ok=True)
    for model in ("mkii",) if "--mkii" in sys.argv else ("mkii", "mki"):    # --mkii: a development switch
        walk(model, s, fx)
    return 1 if vs.fails else 0


if __name__ == "__main__":
    sys.exit(main())
