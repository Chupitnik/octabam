#!/usr/bin/env python3
"""The ColdFire port built once per source state, shared by every worktree.

    python3 tools/emu/port_store.py install [DEST]     # out/emu by default
    python3 tools/emu/port_store.py key                # the key, in this tree

A fresh worktree's first `make reach RUN=1` built the port in the
worktree (69 s) and again in each new shard (125 s for three, 11 Oct
2026). The store is <git common dir>/octabam-port/<key>/: the executables
one `cmake --build` of tools/emu/ot_emu produces, built once under an
flock into a private tree and moved into place, then copied (never
linked: AGENTS.md, out/emu's CMake cache names its own tree) into DEST
with the source stamp reach.port_is_stale reads.

The key is a sha256 over the port's sources (reach.port_digest), every
file tools/emu/ot_emu/CMakeLists.txt names outside its own directory, each
vendored core's commit and working-tree diff (dsp56300 carries
tools/patches/dsp56300.patch as uncommitted changes), the host
architecture and the compiler's version. `make emu-cf` still builds in the
tree, incrementally, for work on the port itself.
"""
import fcntl
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/verify"))
SRC = ROOT / "tools/emu/ot_emu"
VENDORED = ("vendor/mc68k", "vendor/dsp56300")


def _run(*args, cwd=ROOT):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True).stdout


def outside_sources(src=SRC):
    """The files CMakeLists.txt names by a path leaving its directory."""
    text = (src / "CMakeLists.txt").read_text()
    out = set()
    for rel in re.findall(r"(\.\./[\w./-]+\.(?:cpp|cc|c|hpp|h))\b", text):
        p = (src / rel).resolve()
        if p.is_file():
            out.add(p)
    return sorted(out)


def host_arch():
    import check_shards
    return check_shards.host_arch()


def key(root=ROOT):
    import reach
    h = hashlib.sha256()
    h.update(reach.port_digest(root / "tools/emu/ot_emu").encode())
    for p in outside_sources(root / "tools/emu/ot_emu"):
        h.update(str(p.relative_to(root.resolve())).encode()); h.update(b"\0"); h.update(p.read_bytes())
    for d in VENDORED:
        h.update(d.encode())
        h.update(_run("git", "rev-parse", "HEAD", cwd=root / d).encode())
        h.update(_run("git", "diff", "--binary", cwd=root / d).encode())
    h.update(host_arch().encode())
    h.update(_run("c++", "--version").splitlines()[0].encode() if _run("c++", "--version") else b"")
    return h.hexdigest()


def store_dir(root=ROOT):
    common = _run("git", "rev-parse", "--path-format=absolute", "--git-common-dir", cwd=root).strip()
    return pathlib.Path(common) / "octabam-port"


def built(k, root=ROOT):
    """The store's directory for key `k`, building it first if absent."""
    d = store_dir(root) / k
    if (d / "ot_emu").is_file():
        return d
    d.parent.mkdir(parents=True, exist_ok=True)
    with open(d.parent / f"{k}.lock", "a+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if (d / "ot_emu").is_file():
            return d             # another process built it while this one waited
        work = pathlib.Path(tempfile.mkdtemp(prefix=f"{k[:12]}.", dir=d.parent))
        build = work / "build"
        arch = host_arch()
        for cmd in (["cmake", "--fresh", "-B", str(build), "-S", str(root / "tools/emu/ot_emu"),
                     f"-DCMAKE_OSX_ARCHITECTURES={arch}"],
                    ["cmake", "--build", str(build), "-j8"]):
            r = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
            if r.returncode:
                shutil.rmtree(work, ignore_errors=True)
                raise SystemExit(f"port_store: {' '.join(cmd)} failed ({r.returncode})\n{r.stdout[-1500:]}{r.stderr[-1500:]}")
        staged = work / "bin"
        staged.mkdir()
        for f in build.iterdir():
            if f.is_file() and os.access(f, os.X_OK) and (f.name == "ot_emu" or f.name.startswith("ot_")):
                shutil.copy2(f, staged / f.name)
        os.replace(staged, d)
        shutil.rmtree(work, ignore_errors=True)
    return d


def install(dest=None, root=ROOT):
    """The port for this tree's sources into `dest` (out/emu), from the store."""
    import reach
    dest = pathlib.Path(dest or root / "out/emu")
    k = key(root)
    d = built(k, root)
    dest.mkdir(parents=True, exist_ok=True)
    for f in d.iterdir():
        tmp = dest / f".{f.name}.tmp"
        shutil.copy2(f, tmp)
        os.replace(tmp, dest / f.name)
    reach.stamp_port(root / "tools/emu/ot_emu", dest / pathlib.Path(reach.PORT_STAMP).name)
    return dest, k


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    cmd = argv.pop(0) if argv else "install"
    if cmd == "key":
        print(key())
        return 0
    if cmd == "install":
        dest, k = install(argv[0] if argv else None)
        print(f"port_store: {dest}/ot_emu from {store_dir() / k}")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
