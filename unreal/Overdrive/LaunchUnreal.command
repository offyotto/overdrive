#!/bin/bash
set -euo pipefail

if [[ -n "${DYLD_INSERT_LIBRARIES:-}" ]]; then
    printf '%s\n' 'Launching Unreal without injected desktop libraries. Other apps are unchanged.'
fi
unset DYLD_INSERT_LIBRARIES

ENGINE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.6}"
EDITOR="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
if [[ ! -x "$EDITOR" ]]; then
    printf 'Unreal Editor was not found at:\n%s\nSet UE_ROOT to your Unreal installation.\n' "$EDITOR" >&2
    exit 2
fi

if [[ "$#" -eq 0 ]]; then
    PROJECT="$(cd -- "$(dirname -- "$0")" && pwd)/Overdrive.uproject"
    exec "$EDITOR" "$PROJECT" -NoEpicPortal
fi
exec "$EDITOR" -NoEpicPortal "$@"
