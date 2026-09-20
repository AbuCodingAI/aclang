#!/usr/bin/env bash
# AC installer — pulls the packages the AC compiler needs, builds it from source, and installs
# it globally (`ac` on your PATH). Linux only (AC's native backends emit ELF binaries); on
# Windows use WSL, or the prebuilt ac-compiler/ac.exe for the text-emitting backends.
#
#   ./install.sh                  build + install to /usr/local, pull the default dependencies
#   ./install.sh --user           install to ~/.local (no root needed for the install itself)
#   ./install.sh --prefix DIR     install somewhere else
#   ./install.sh --minimal        only what is needed to build the compiler
#   ./install.sh --all            also the toolchains for every backend + optional ilib deps
#   ./install.sh --dev            also the cross toolchains used to rebuild ac.exe / ac.arm
#   ./install.sh --no-deps        do not touch system packages at all
#   ./install.sh --prebuilt       use the shipped ac-compiler/ac instead of compiling (x86-64)
#   ./install.sh --dry-run        print what would happen, change nothing
#   ./install.sh --uninstall      remove a previous install
set -euo pipefail

PREFIX="/usr/local"
DEPS="default"          # minimal | default | all
DEV=0
INSTALL_DEPS=1
PREBUILT=0
DRY_RUN=0
ASSUME_YES=0
ACTION="install"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -t 1 ]; then B=$'\033[1m'; G=$'\033[32m'; Y=$'\033[33m'; R=$'\033[31m'; Z=$'\033[0m'; else B=; G=; Y=; R=; Z=; fi
info() { printf '%s==>%s %s\n' "$B$G" "$Z" "$*"; }
warn() { printf '%swarning:%s %s\n' "$B$Y" "$Z" "$*" >&2; }
die()  { printf '%serror:%s %s\n' "$B$R" "$Z" "$*" >&2; exit 1; }

usage() { sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
  case "$1" in
    --prefix)    [ $# -ge 2 ] || die "--prefix needs a directory"; PREFIX="$2"; shift ;;
    --prefix=*)  PREFIX="${1#--prefix=}" ;;
    --user)      PREFIX="$HOME/.local" ;;
    --minimal)   DEPS="minimal" ;;
    --all)       DEPS="all" ;;
    --dev)       DEV=1 ;;
    --no-deps)   INSTALL_DEPS=0 ;;
    --prebuilt)  PREBUILT=1 ;;
    --dry-run)   DRY_RUN=1 ;;
    -y|--yes)    ASSUME_YES=1 ;;
    --uninstall) ACTION="uninstall" ;;
    -h|--help)   usage; exit 0 ;;
    *)           die "unknown option: $1 (see --help)" ;;
  esac
  shift
done

[ "$(uname -s)" = "Linux" ] || die "AC's installer supports Linux only (detected $(uname -s)). On Windows use WSL."

LIBDIR="$PREFIX/lib/ac"
SHAREDIR="$PREFIX/share/ac"
BINLINK="$PREFIX/bin/ac"

run() {   # echo, and execute unless --dry-run
  if [ "$DRY_RUN" = 1 ]; then printf '  [dry-run] %s\n' "$*"; else "$@"; fi
}

# Use sudo only when the target needs it and we are not already root.
need_root_for() {  # $1 = path that will be written
  local p="$1"
  while [ ! -e "$p" ] && [ "$p" != "/" ]; do p="$(dirname "$p")"; done
  [ -w "$p" ] && return 1 || return 0
}
SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1; then SUDO="sudo"; fi

root_run() {   # run a command as root (system packages)
  if [ "$(id -u)" -eq 0 ]; then run "$@"
  elif [ -n "$SUDO" ]; then run "$SUDO" "$@"
  else die "need root to install system packages, but sudo is not available (re-run as root, or use --no-deps)"; fi
}
prefix_run() {  # run a command that writes under $PREFIX, escalating only if needed
  if need_root_for "$PREFIX" && [ "$(id -u)" -ne 0 ]; then
    [ -n "$SUDO" ] || die "$PREFIX is not writable and sudo is unavailable — try --user"
    run "$SUDO" "$@"
  else run "$@"; fi
}

# ── uninstall ──────────────────────────────────────────────────────────────────────────────
if [ "$ACTION" = "uninstall" ]; then
  info "Removing AC from $PREFIX"
  prefix_run rm -rf "$LIBDIR" "$SHAREDIR"
  prefix_run rm -f "$BINLINK"
  info "Done. (System packages installed for AC's toolchains were left in place.)"
  exit 0
fi

# ── package manager + package names ──────────────────────────────────────────────────────────
PM=""
for c in pacman apt-get dnf zypper; do command -v "$c" >/dev/null 2>&1 && { PM="$c"; break; }; done

PKGS=()
add() { PKGS+=("$@"); }

pick_packages() {
  case "$PM" in
    pacman)
      add base-devel pkgconf
      [ "$DEPS" != minimal ] && add python nodejs nasm zlib gtk3
      [ "$DEPS" = all ]      && add jdk-openjdk rust go alsa-lib mpg123 opencv
      [ "$DEV" = 1 ]         && add mingw-w64-gcc aarch64-linux-gnu-gcc qemu-user ;;
    apt-get)
      add build-essential pkg-config
      [ "$DEPS" != minimal ] && add python3 nodejs nasm zlib1g-dev libgtk-3-dev
      [ "$DEPS" = all ]      && add default-jdk rustc golang-go libasound2-dev libmpg123-dev libopencv-dev
      [ "$DEV" = 1 ]         && add gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 g++-aarch64-linux-gnu qemu-user ;;
    dnf)
      add gcc-c++ make pkgconf-pkg-config
      [ "$DEPS" != minimal ] && add python3 nodejs nasm zlib-devel gtk3-devel
      [ "$DEPS" = all ]      && add java-latest-openjdk-devel rust golang alsa-lib-devel mpg123-devel opencv-devel
      [ "$DEV" = 1 ]         && add mingw64-gcc-c++ gcc-c++-aarch64-linux-gnu qemu-user ;;
    zypper)
      add gcc-c++ make pkg-config
      [ "$DEPS" != minimal ] && add python3 nodejs nasm zlib-devel gtk3-devel
      [ "$DEPS" = all ]      && add java-devel rust go alsa-devel mpg123-devel opencv-devel
      [ "$DEV" = 1 ] && warn "--dev cross toolchains are not mapped for zypper; install mingw64 / aarch64 cross gcc manually" ;;
  esac
  return 0   # the last `[ .. ] && add ..` above may legitimately be false; don't leak that under set -e
}

install_deps() {
  [ "$INSTALL_DEPS" = 1 ] || { info "Skipping system packages (--no-deps)"; return; }
  [ -n "$PM" ] || { warn "no supported package manager found (pacman, apt-get, dnf, zypper) — install a C++17 compiler and make yourself"; return; }
  pick_packages
  info "Installing packages with $PM: ${PKGS[*]}"
  local yes_flag=()
  case "$PM" in
    pacman)  [ "$ASSUME_YES" = 1 ] && yes_flag=(--noconfirm); root_run pacman -S --needed ${yes_flag[@]+"${yes_flag[@]}"} "${PKGS[@]}" ;;
    apt-get) root_run apt-get update
             [ "$ASSUME_YES" = 1 ] && yes_flag=(-y); root_run apt-get install ${yes_flag[@]+"${yes_flag[@]}"} "${PKGS[@]}" ;;
    dnf)     [ "$ASSUME_YES" = 1 ] && yes_flag=(-y); root_run dnf install ${yes_flag[@]+"${yes_flag[@]}"} "${PKGS[@]}" ;;
    zypper)  [ "$ASSUME_YES" = 1 ] && yes_flag=(-y); root_run zypper install ${yes_flag[@]+"${yes_flag[@]}"} "${PKGS[@]}" ;;
  esac
  if [ "$DEPS" = all ]; then
    command -v v >/dev/null 2>&1 || warn "the V backend (AC->V) needs the V compiler, which distros rarely package: see https://vlang.io"
  fi
}

# ── build ──────────────────────────────────────────────────────────────────────────────────────
build_compiler() {
  local jobs; jobs="$(nproc 2>/dev/null || echo 2)"
  if [ "$PREBUILT" = 1 ]; then
    [ "$(uname -m)" = "x86_64" ] || die "--prebuilt ships an x86-64 binary; this machine is $(uname -m) — build from source instead"
    [ -x "$HERE/ac-compiler/ac" ] || die "ac-compiler/ac not found or not executable"
    info "Using the shipped ac-compiler/ac"
    return
  fi
  command -v g++ >/dev/null 2>&1 || [ "$DRY_RUN" = 1 ] || die "g++ not found (C++17 required) — re-run without --no-deps, or install a C++ toolchain"
  info "Building the compiler (make -j$jobs)"
  run make -C "$HERE/ac-compiler" -j"$jobs"
}

build_ilibs() {
  # Best effort: an ilib whose system dependency is missing (e.g. GTK for widgets) is skipped, not fatal.
  info "Building the ilib shared libraries"
  local d name
  for d in "$HERE"/library/ilib/*/; do
    name="$(basename "$d")"
    [ -f "$d/Makefile" ] || continue
    if [ "$DRY_RUN" = 1 ]; then printf '  [dry-run] make -C %s\n' "$d"; continue; fi
    if make -C "$d" >/dev/null 2>&1; then printf '  built   %s\n' "$name"
    else warn "skipped ilib '$name' (its build dependencies are not installed)"; fi
  done
}

# ── install ────────────────────────────────────────────────────────────────────────────────────
# `ac` finds its library as <dir of the real binary>/../library, so the binary and library/
# live side by side under $PREFIX/lib/ac and only a symlink goes on the PATH.
install_files() {
  info "Installing to $PREFIX"
  prefix_run mkdir -p "$LIBDIR/ac-compiler" "$SHAREDIR" "$PREFIX/bin"
  prefix_run install -m 0755 "$HERE/ac-compiler/ac" "$LIBDIR/ac-compiler/ac"
  prefix_run rm -rf "$LIBDIR/library"
  prefix_run cp -a "$HERE/library" "$LIBDIR/library"
  prefix_run rm -rf "$SHAREDIR/examples"
  prefix_run cp -a "$HERE/examples" "$SHAREDIR/examples"
  prefix_run ln -sf "$LIBDIR/ac-compiler/ac" "$BINLINK"
}

smoke_test() {
  [ "$DRY_RUN" = 1 ] && return
  info "Smoke test"
  local t; t="$(mktemp -d)"
  printf 'AC->BNY\n\n<mainloop>\n    Term.display $AC works$\n<mainloop>\n' > "$t/hello.ac"
  if ( cd "$t" && "$BINLINK" hello.ac --no-cache 2>&1 | grep -q "AC works" ); then
    printf '  ac %s\n' "$("$BINLINK" --version 2>&1 | head -1)"
    printf '  hello world: ok (native BNY backend)\n'
  else
    warn "the smoke test did not print the expected output — run: $BINLINK --version"
  fi
  rm -rf "$t"
}

install_deps
build_compiler
[ "$DEPS" = minimal ] || build_ilibs
install_files
smoke_test

case ":$PATH:" in
  *":$PREFIX/bin:"*) ;;
  *) warn "$PREFIX/bin is not on your PATH — add:  export PATH=\"$PREFIX/bin:\$PATH\"" ;;
esac
info "AC is installed. Try:  ac $SHAREDIR/examples/hello_all_targets.ac"
