#!/usr/bin/env bash
set -euo pipefail

if [[ "${OSTYPE:-}" == darwin* && -n "${DYLD_INSERT_LIBRARIES:-}" ]]; then
    printf '%s\n' 'Ignoring injected desktop libraries for Unreal and its build tools (this process only).'
    unset DYLD_INSERT_LIBRARIES
fi

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="$ROOT/Overdrive.uproject"
ACTION="${1:-help}"

usage() {
    printf '%s\n' \
        'OVERDRIVE native project — Unreal Engine 5.6' \
        '' \
        'Usage: UE_ROOT="/path/to/UE_5.6" bash Build.sh <action>' \
        '' \
        '  check    Check the engine and native toolchain.' \
        '  build    Compile the Unreal editor modules.' \
        '  setup    Compile and generate missing game assets.' \
        '  karts    Build and refresh only the upgraded karts (with backup).' \
        '  editor   Compile, generate assets, and open Cloudtop Coast.' \
        '  play     Compile, generate assets, and run the native game.' \
        '  package  Build a Development game into Dist/.' \
        '  test     Run the standalone native tests; no Unreal required.' \
        '' \
        'This script never downloads Unreal or modifies system configuration.'
}

case "$ACTION" in
    help|-h|--help) usage; exit 0 ;;
    test)
        command -v cmake >/dev/null || { printf '%s\n' 'CMake 3.20+ and a C++20 compiler are required.' >&2; exit 2; }
        cmake -S "$ROOT/Tests" -B "$ROOT/Saved/NativeTests/CMake" -DCMAKE_BUILD_TYPE=Debug
        cmake --build "$ROOT/Saved/NativeTests/CMake" --parallel
        ctest --test-dir "$ROOT/Saved/NativeTests/CMake" --output-on-failure
        exit 0
        ;;
    check|build|setup|karts|editor|play|package) ;;
    *) usage >&2; exit 2 ;;
esac

case "$(uname -s)" in
    Darwin)
        PLATFORM=Mac
        UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.6}"
        ;;
    Linux)
        PLATFORM=Linux
        UE_ROOT="${UE_ROOT:-}"
        ;;
    *) printf '%s\n' 'Use UnrealBuildTool or Unreal Editor on Windows; see README.md.' >&2; exit 2 ;;
esac

if [[ -z "$UE_ROOT" || ! -f "$UE_ROOT/Engine/Build/Build.version" ]]; then
    printf '%s\n' 'Unreal Engine was not found. Install UE 5.6 and set UE_ROOT to its installation directory.' >&2
    printf 'Checked: %s\n' "${UE_ROOT:-<UE_ROOT is unset>}" >&2
    exit 2
fi
if ! grep -Eq '"MajorVersion"[[:space:]]*:[[:space:]]*5[[:space:]]*,' "$UE_ROOT/Engine/Build/Build.version" ||
   ! grep -Eq '"MinorVersion"[[:space:]]*:[[:space:]]*6[[:space:]]*,' "$UE_ROOT/Engine/Build/Build.version"; then
    printf '%s\n' 'This project targets UE 5.6. Retarget and validate the C++ project before using another engine release.' >&2
    exit 2
fi

BUILD="$UE_ROOT/Engine/Build/BatchFiles/$PLATFORM/Build.sh"
UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"

xcode_ver_num() {
    local Version="$1" IFS=.
    set -- $Version
    echo $(( $1 * 100 + ${2:-0} ))
}
to_dev_dir() {
    case "$1" in
        */Contents/Developer) echo "$1" ;;
        *.app) echo "${1%/}/Contents/Developer" ;;
        *) echo "$1" ;;
    esac
}
dev_dir_version() { DEVELOPER_DIR="$1" xcodebuild -version 2>/dev/null | sed -n '1s/^Xcode \([0-9][0-9]*\.[0-9][0-9]*\).*/\1/p'; }

pick_supported_xcode() {
    local Best="" BestNum=0 Candidate Dir Version Num Seen=" "
    local Candidates=("${DEVELOPER_DIR:-}" "$(xcode-select -p 2>/dev/null || true)")
    local Globbed
    for Globbed in /Applications/Xcode*.app "$HOME/Applications/Xcode"*.app "$HOME/Downloads/Xcode"*.app; do
        [[ -d "$Globbed" ]] && Candidates+=("$Globbed")
    done
    for Candidate in "${Candidates[@]}"; do
        [[ -n "$Candidate" ]] || continue
        Dir="$(to_dev_dir "$Candidate")"
        [[ -d "$Dir" ]] || continue
        case " $Seen " in *" $Dir "*) continue ;; esac
        Seen="$Seen $Dir "
        case "$Dir" in *CommandLineTools*) continue ;; esac
        Version="$(dev_dir_version "$Dir")"
        [[ -n "$Version" ]] || continue
        Num="$(xcode_ver_num "$Version")"
        if (( Num >= 1502 && Num <= 1609 )); then
            if (( Num > BestNum )); then BestNum=$Num; Best="$Dir"; fi
        else
            printf 'Ignoring Xcode %s at %s (Unreal Engine 5.6 accepts 15.2-16.9 only).\n' "$Version" "$Dir" >&2
        fi
    done
    [[ -n "$Best" ]] && echo "$Best"
}

if [[ "$PLATFORM" == Mac ]]; then
    if SELECTED_DEV="$(pick_supported_xcode)"; then
        export DEVELOPER_DIR="$SELECTED_DEV"
        printf 'Using Xcode at: %s\n' "$DEVELOPER_DIR"
    else
        printf '%s\n' 'No Xcode compatible with Unreal Engine 5.6 was found on this Mac.' >&2
        printf '%s\n' 'This engine accepts Xcode versions 15.2 through 16.9; that version gate alone does not guarantee macOS compatibility.' >&2
        printf '%s\n' '' 'If Xcode is already expanded, set DEVELOPER_DIR to its Contents/Developer directory.' >&2
        printf '%s\n' 'This script searches /Applications, ~/Applications and ~/Downloads without opening the Xcode GUI.' >&2
        printf '%s\n' 'Before downloading another Xcode, verify compatibility with BOTH macOS and Unreal.' >&2
        printf '%s\n' 'No engine version checks or system compatibility checks are bypassed.' >&2
        printf '%s\n' '' 'Command Line Tools alone cannot build Unreal projects.' >&2
        exit 2
    fi
    xcrun --find metal >/dev/null 2>&1 || { printf '%s\n' 'The selected Xcode is missing the Metal compiler.' >&2; exit 2; }
    EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
else
    EDITOR="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
fi

[[ -f "$BUILD" && -x "$EDITOR" ]] || { printf '%s\n' 'The Unreal installation is missing its build tools or editor executable.' >&2; exit 2; }
printf 'Engine: %s\nProject: %s\nPlatform: %s\n' "$UE_ROOT" "$PROJECT" "$PLATFORM"
[[ "$ACTION" != check ]] || exit 0

bash "$BUILD" OverdriveEditor "$PLATFORM" Development "-Project=$PROJECT" -WaitMutex -NoHotReloadFromIDE
[[ "$ACTION" != build ]] || exit 0
if [[ "$ACTION" == karts ]]; then
    "$EDITOR" "$PROJECT" -run=OverdriveSetup -KartsOnly -RebuildKarts -unattended -nop4 -nosplash -NullRHI
    exit 0
fi

"$EDITOR" "$PROJECT" -run=OverdriveSetup -unattended -nop4 -nosplash -NullRHI
[[ -f "$ROOT/Content/Overdrive/Maps/CloudtopCoast.umap" ]] || { printf '%s\n' 'Asset generation did not produce CloudtopCoast.umap. Inspect Saved/Logs.' >&2; exit 1; }

case "$ACTION" in
    editor) exec "$EDITOR" "$PROJECT" /Game/Overdrive/Maps/CloudtopCoast ;;
    play) exec "$EDITOR" "$PROJECT" /Game/Overdrive/Maps/CloudtopCoast -game -windowed -ResX=1600 -ResY=900 ;;
    package)
        [[ -f "$UAT" ]] || { printf '%s\n' 'Unreal Automation Tool was not found.' >&2; exit 2; }
        bash "$UAT" BuildCookRun "-project=$PROJECT" -noP4 "-platform=$PLATFORM" \
            -clientconfig=Development -build -cook -stage -pak -archive \
            "-archivedirectory=$ROOT/Dist" -map=/Game/Overdrive/Maps/CloudtopCoast -utf8output
        ;;
esac
