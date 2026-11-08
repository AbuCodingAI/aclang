#!/bin/bash
# npm pack/publish silently drops symlinks from the tarball (verified: a plain symlink in an
# otherwise-empty package is dropped with no warning, on npm 12.2.1 — not specific to this
# repo's ignore rules). This repo tracks a handful of real symlinks in git (library/ilib/*/
# *.hpp aliasing a sibling _c.h file) that would otherwise silently vanish from the published
# package. Run via package.json's prepack/postpack hooks: materialize copies the real file
# over each symlink right before packing, restore puts the exact tracked symlink back
# afterward via `git checkout`, so the working tree is never left modified.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

mapfile -t LINKS < <(git ls-files -s | awk '$1 == 120000 { print $4 }')

case "${1:-}" in
    materialize)
        for f in "${LINKS[@]}"; do
            target="$(readlink -f "$f")"
            cp --remove-destination "$target" "$f"
        done
        ;;
    restore)
        if [ "${#LINKS[@]}" -gt 0 ]; then
            git checkout -- "${LINKS[@]}"
        fi
        ;;
    *)
        echo "usage: $0 {materialize|restore}" >&2
        exit 1
        ;;
esac
