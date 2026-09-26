#!/usr/bin/env bash
# AC installer — installs the AC compiler globally (`ac` on your PATH; `ac.exe` on Windows).
#   Linux    builds the compiler from source (or uses a prebuilt one), pulls the packages it needs, and
#            builds the ilib shared libraries.
#   macOS    builds from source with the system compiler (Xcode Command Line Tools), or uses the prebuilt
#            universal binary. Installed as `ac`.
#   Windows  run from Git Bash / MSYS2 / Cygwin: installs the prebuilt `ac.exe` (no build step).
# A universal binary (ac-compiler/ac.com, Cosmopolitan: one file for Linux/macOS/Windows on x86-64 and
# ARM64) is preferred when it ships in the repo; otherwise the per-platform binary is used:
# ac-compiler/ac (Linux x86-64), ac.arm (Linux AArch64), ac.exe (Windows).
#
#   ./install.sh                  build + install to /usr/local (Windows: ~/AC), pull the default dependencies
#   ./install.sh --user           install to ~/.local (no root needed for the install itself)
#   ./install.sh --prefix DIR     install somewhere else
#   ./install.sh --minimal        only what is needed to build the compiler
#   ./install.sh --all            also the toolchains for every backend + optional ilib deps
#   ./install.sh --dev            also the cross toolchains used to rebuild ac.exe / ac.arm
#   ./install.sh --no-deps        do not touch system packages at all
#   ./install.sh --prebuilt       use the shipped binary instead of compiling
#   ./install.sh --dry-run        print what would happen, change nothing
#   ./install.sh --uninstall      remove a previous install
set -euo pipefail

PREFIX=""
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

usage() { sed -n 2,23p "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

# ── which OS / CPU / executable name ──────────────────────────────────────────────────────────
case "$(uname -s)" in
  Linux)                OS="linux"   ;;
  Darwin)               OS="macos"   ;;
  MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
  *) die "unsupported OS: $(uname -s) (Linux, macOS, or Windows via Git Bash / MSYS2 / Cygwin)" ;;
esac
ARCH="$(uname -m)"
[ "$ARCH" = "arm64" ] && ARCH="aarch64"
if [ "$OS" = "windows" ]; then EXE_NAME="ac.exe"; else EXE_NAME="ac"; fi

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

if [ -z "$PREFIX" ]; then
  if [ "$OS" = "windows" ]; then PREFIX="$HOME/AC"; else PREFIX="/usr/local"; fi
fi

# Layout. `ac` finds its library as <dir of the real binary>/../library:
#   Linux/macOS: $PREFIX/lib/ac/{ac-compiler/ac, library/}  + a symlink $PREFIX/bin/ac
#   Windows:     $PREFIX/bin/ac.exe + $PREFIX/library/       (no symlinks — Git Bash's are unreliable)
if [ "$OS" = "windows" ]; then
  LIBDIR="$PREFIX"
  BINDEST="$PREFIX/bin/$EXE_NAME"
  LIBRARYDEST="$PREFIX/library"
else
  LIBDIR="$PREFIX/lib/ac"
  BINDEST="$LIBDIR/ac-compiler/$EXE_NAME"
  LIBRARYDEST="$LIBDIR/library"
fi
SHAREDIR="$PREFIX/share/ac"
BINLINK="$PREFIX/bin/$EXE_NAME"

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
  if [ "$OS" = "windows" ]; then
    prefix_run rm -rf "$LIBRARYDEST" "$SHAREDIR"
    prefix_run rm -f "$BINDEST"
  else
    prefix_run rm -rf "$LIBDIR" "$SHAREDIR"
    prefix_run rm -f "$BINLINK"
  fi
  info "Done. (System packages installed for AC's toolchains were left in place.)"
  exit 0
fi

# ── the shipped binary for this machine (empty if there is none) ─────────────────────────────────
pick_prebuilt() {
  local d="$HERE/ac-compiler"
  if [ -f "$d/ac.com" ]; then echo "$d/ac.com"; return 0; fi   # universal Cosmopolitan binary: every OS, x86-64 + ARM64
  case "$OS/$ARCH" in
    windows/*)               [ -f "$d/ac.exe" ] && echo "$d/ac.exe" ;;
    linux/x86_64)            [ -f "$d/ac" ]     && echo "$d/ac" ;;
    linux/aarch64|linux/arm) [ -f "$d/ac.arm" ] && echo "$d/ac.arm" ;;
  esac
  return 0
}

# Windows has no build step here — it always uses a shipped binary.
[ "$OS" = "windows" ] && PREBUILT=1

# ── package manager + package names ──────────────────────────────────────────────────────────
PM=""
if [ "$OS" = "macos" ]; then
  command -v brew >/dev/null 2>&1 && PM="brew"
elif [ "$OS" = "linux" ]; then
  for c in pacman apt-get dnf zypper; do command -v "$c" >/dev/null 2>&1 && { PM="$c"; break; }; done
fi

PKGS=()
add() { PKGS+=("$@"); }

pick_packages() {
  case "$PM" in
    brew)
      [ "$DEPS" != minimal ] && add python node
      [ "$DEPS" = all ]      && add openjdk rust go pkgconf ;;
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
  if [ "$OS" = "windows" ]; then info "Windows: no system packages are installed (the compiler is a prebuilt exe)"; return; fi
  if [ "$OS" = "macos" ]; then
    xcode-select -p >/dev/null 2>&1 || warn "Xcode Command Line Tools are missing — run: xcode-select --install"
  fi
  [ -n "$PM" ] || { warn "no supported package manager found — install a C++17 compiler and make yourself$([ "$OS" = macos ] && echo " (macOS: Homebrew, https://brew.sh)")"; return; }
  pick_packages
  [ ${#PKGS[@]} -gt 0 ] || { info "No extra packages needed"; return; }
  info "Installing packages with $PM: ${PKGS[*]}"
  local yes_flag=()
  case "$PM" in
    brew)    run brew install "${PKGS[@]}" ;;   # Homebrew refuses to run as root — never sudo it
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

# ── build / pick the binary ────────────────────────────────────────────────────────────────────
SRC_BIN=""     # the binary that gets installed
build_compiler() {
  local jobs; jobs="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"
  if [ "$PREBUILT" = 1 ]; then
    SRC_BIN="$(pick_prebuilt)"
    [ -n "$SRC_BIN" ] || die "no prebuilt binary ships for $OS/$ARCH (looked for ac-compiler/ac.com, ac, ac.arm, ac.exe) — build from source instead"
    info "Using the shipped $(basename "$SRC_BIN")"
    return
  fi
  command -v g++ >/dev/null 2>&1 || command -v c++ >/dev/null 2>&1 || [ "$DRY_RUN" = 1 ] \
    || die "no C++ compiler found (C++17 required) — re-run without --no-deps, or install one"
  info "Building the compiler (make -j$jobs)"
  run make -C "$HERE/ac-compiler" -j"$jobs"
  SRC_BIN="$HERE/ac-compiler/ac"
}

build_ilibs() {
  # The ilib shared libraries are Linux builds; elsewhere the compiler still works for every text backend.
  if [ "$OS" != "linux" ]; then info "Skipping the ilib shared libraries (Linux only for now)"; return; fi
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
install_files() {
  info "Installing to $PREFIX"
  if [ "$OS" = "windows" ]; then
    prefix_run mkdir -p "$PREFIX/bin" "$SHAREDIR"
  else
    prefix_run mkdir -p "$LIBDIR/ac-compiler" "$SHAREDIR" "$PREFIX/bin"
  fi
  prefix_run install -m 0755 "$SRC_BIN" "$BINDEST"       # installed under the platform's name: `ac`, or `ac.exe` on Windows
  prefix_run rm -rf "$LIBRARYDEST"
  prefix_run cp -a "$HERE/library" "$LIBRARYDEST"
  prefix_run rm -rf "$SHAREDIR/examples"
  prefix_run cp -a "$HERE/examples" "$SHAREDIR/examples"
  [ "$OS" = "windows" ] || prefix_run ln -sf "$BINDEST" "$BINLINK"
  # A file that came through a browser/zip carries macOS's quarantine flag and would be blocked on first run.
  if [ "$OS" = "macos" ] && [ "$DRY_RUN" != 1 ]; then
    xattr -d com.apple.quarantine "$BINDEST" 2>/dev/null || true
  fi
}

smoke_test() {
  [ "$DRY_RUN" = 1 ] && return
  info "Smoke test"
  local bin="$BINLINK"
  if ! "$bin" --version >/dev/null 2>&1; then
    warn "$bin did not start — run: $bin --version"
    return
  fi
  printf '  ac %s\n' "$("$bin" --version 2>&1 | head -1)"
  local t; t="$(mktemp -d)"
  if [ "$OS" = "linux" ] && [ "$ARCH" = "x86_64" ]; then
    # native BNY backend: hand-written x86-64 ELF, no external toolchain
    printf 'AC->BNY\n\n<mainloop>\n    Term.display $AC works$\n<mainloop>\n' > "$t/hello.ac"
    if ( cd "$t" && "$bin" hello.ac --no-cache 2>&1 | grep -q "AC works" ); then
      printf '  hello world: ok (native BNY backend)\n'
    else
      warn "the smoke test did not print the expected output — run: $bin --version"
    fi
  else
    local py=""; command -v python3 >/dev/null 2>&1 && py=python3
    if [ -n "$py" ]; then
      printf 'AC->PY\n\n<mainloop>\n    Term.display $AC works$\n<mainloop>\n' > "$t/hello.ac"
      if ( cd "$t" && "$bin" hello.ac --no-cache 2>&1 | grep -q "AC works" ); then
        printf '  hello world: ok (PY backend)\n'
      else
        warn "the smoke test did not print the expected output — run: $bin --version"
      fi
    else
      printf '  (no python3 found — skipped the hello-world run)\n'
    fi
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
  *) if [ "$OS" = "windows" ]; then
       wp="$PREFIX/bin"; command -v cygpath >/dev/null 2>&1 && wp="$(cygpath -w "$PREFIX/bin")"
       warn "$wp is not on your PATH — add it in Windows' Environment Variables (or, in Git Bash: export PATH=\"$PREFIX/bin:\$PATH\")"
     else
       warn "$PREFIX/bin is not on your PATH — add:  export PATH=\"$PREFIX/bin:\$PATH\""
     fi ;;
esac
info "AC is installed. Try:  $EXE_NAME $SHAREDIR/examples/hello_all_targets.ac"
