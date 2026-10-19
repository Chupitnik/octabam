#!/usr/bin/env bash
# Debian/Ubuntu (including WSL2): install the system tools with apt, build the
# m68k-elf cross-compiler with scripts/m68k-toolchain.sh, then run
# scripts/setup.sh. Idempotent. M68K_PREFIX overrides the toolchain prefix
# (default $HOME/.local/m68k-elf); add its bin/ to PATH for later shells.
set -euo pipefail
cd "$(dirname "$0")/.."

command -v apt-get >/dev/null 2>&1 \
  || { echo "setup-linux: apt-get not found; install the packages named in scripts/setup-linux.sh with your package manager, run scripts/m68k-toolchain.sh and scripts/setup.sh" >&2; exit 1; }

SUDO=""
[ "$(id -u)" -eq 0 ] || SUDO="sudo"
$SUDO apt-get update
$SUDO apt-get install -y build-essential cmake git curl unzip xxd binutils python3 python3-numpy \
  binwalk radare2 pkgconf texinfo \
  libgmp-dev libmpfr-dev libmpc-dev libisl-dev libzstd-dev zlib1g-dev
command -v uv >/dev/null 2>&1 \
  || echo "setup-linux: uv not installed; make emu-setup needs it (curl -LsSf https://astral.sh/uv/install.sh | sh)"

PREFIX="${M68K_PREFIX:-$HOME/.local/m68k-elf}"
scripts/m68k-toolchain.sh "$PREFIX"
export PATH="$PREFIX/bin:$PATH"
echo "setup-linux: add $PREFIX/bin to PATH for later shells"

exec scripts/setup.sh
