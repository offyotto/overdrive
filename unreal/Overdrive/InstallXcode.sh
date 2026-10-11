#!/usr/bin/env bash
set -euo pipefail

[[ "$(uname -s)" == Darwin ]] || { printf '%s\n' 'This optional installer requires macOS.' >&2; exit 2; }
unset DYLD_INSERT_LIBRARIES
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if bash "$ROOT/Build.sh" check >/dev/null 2>&1; then
    printf '%s\n' 'A supported Xcode toolchain is already available. No extraction or installation is needed.'
    printf '%s\n' 'Run: bash Build.sh play'
    exit 0
fi

MIN_NUM=1502
MAX_NUM=1609

ver_num() { local v="$1" IFS=.; set -- $v; echo $(( $1 * 100 + ${2:-0} )); }

XIP="${1:-}"
if [[ -z "$XIP" || ! -f "$XIP" || "$XIP" != *.xip ]]; then
    printf '%s\n' 'Usage: bash InstallXcode.sh /absolute/path/to/Xcode.xip --yes' >&2
    printf '%s\n' 'Download a toolchain compatible with your macOS and UE 5.6 from https://developer.apple.com/download/all/.' >&2
    exit 2
fi
if [[ "${2:-}" != --yes ]]; then
    printf '%s\n' 'This optional operation expands an archive and installs Xcode outside the repository.' >&2
    printf '%s\n' 'Review the archive and available disk space, then explicitly pass --yes to proceed.' >&2
    exit 2
fi
XIP="$(cd -- "$(dirname -- "$XIP")" && pwd)/$(basename -- "$XIP")"
FREE_KB="$(df -Pk "${TMPDIR:-/tmp}" | awk 'NR==2 {print $4}')"
if (( FREE_KB < 30000000 )); then
    printf '%s\n' 'At least 30 GB free space is required to expand and install Xcode.' >&2
    exit 2
fi
printf 'Selected archive: %s\n' "$XIP"
SIZE_GB=$(( $(stat -f%z "$XIP") / 1024 / 1024 / 1024 ))
if (( SIZE_GB < 2 )); then
    printf 'That file is only %s GB - the download looks incomplete. Re-run this after it finishes.\n' "$SIZE_GB" >&2
    exit 2
fi

DEST_ROOT="/Applications"
[[ -w "$DEST_ROOT" ]] || DEST_ROOT="$HOME/Applications"
mkdir -p "$DEST_ROOT"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/xcode-expand.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
printf 'Expanding %s GB archive (takes ~10 minutes, no progress output is normal)...\n' "$SIZE_GB"
(cd "$WORK" && xip --expand "$XIP")

APP="$(find "$WORK" -maxdepth 1 -name '*.app' -print -quit)"
[[ -n "$APP" ]] || { printf '%s\n' 'Expansion produced no .app. The .xip may be corrupted - delete it and re-download.' >&2; exit 1; }

VERSION="$(DEVELOPER_DIR="$APP/Contents/Developer" xcodebuild -version | sed -n '1s/^Xcode \([0-9.]*\)$/\1/p')"
NUM="$(ver_num "${VERSION:-0.0}")"
if (( NUM < MIN_NUM || NUM > MAX_NUM )); then
    printf 'Xcode %s is NOT in the range Unreal Engine 5.6 accepts (15.2-16.9). Not installing it.\n' "${VERSION:-unknown}" >&2
    exit 2
fi

DEST="$DEST_ROOT/Xcode${VERSION}.app"
[[ -e "$DEST" ]] && DEST="$DEST_ROOT/Xcode${VERSION}_$(date +%s).app"
printf 'Installing to %s (your existing Xcode is untouched)...\n' "$DEST"
mv "$APP" "$DEST"

printf 'Installed Xcode %s. Verifying it builds for Unreal...\n' "$VERSION"
DEVELOPER_DIR="$DEST/Contents/Developer" xcodebuild -version
DEVELOPER_DIR="$DEST/Contents/Developer" xcrun --find metal >/dev/null && printf '%s\n' 'Metal compiler found.'
printf '\nAll set. Now run:  bash Build.sh play\n'
