#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/decomp-logging/entrypoint.sh"
xbox_logged_entrypoint "$ROOT" "unity" "$ROOT/scripts/unity.sh" "$@"
PROJECT="$ROOT/unity/ArmoredCoreReconstruction"
LOG_DIR="$ROOT/logs"
mkdir -p "$LOG_DIR"
action="${1:-help}"
stamp="$(date -u +%Y%m%dT%H%M%SZ)"
exec > >(tee -a "$LOG_DIR/unity-${action}-${stamp}.log") 2>&1
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
case "$action" in
  validate)
    [[ -f "$PROJECT/Packages/manifest.json" ]] || die 'Unity project manifest is not initialized.'
    [[ -f "$PROJECT/ProjectSettings/ProjectVersion.txt" ]] || die 'ProjectVersion.txt is missing.'
    command -v unity >/dev/null || die 'Unity CLI is unavailable.'
    python3 -m json.tool "$PROJECT/Packages/manifest.json" >/dev/null || die 'Packages/manifest.json is invalid JSON.'
    editor_version="$(sed -n 's/^m_EditorVersion: //p' "$PROJECT/ProjectSettings/ProjectVersion.txt")"
    [[ -n "$editor_version" ]] || die 'Could not read the required editor version.'
    editor_inventory="$(unity editors list --no-banner --format json)"
    printf '%s\n' "$editor_inventory" | python3 -c 'import json,sys; wanted=sys.argv[1]; data=json.load(sys.stdin); sys.exit(0 if any(e.get("version")==wanted and e.get("location") for e in data.get("data", [])) else 1)' "$editor_version" || die "Required Editor $editor_version is not installed; package resolution and Editor validation cannot run."
    printf 'Project metadata is valid; required Editor %s is installed.\n' "$editor_version"
    ;;
  build)
    [[ -f "$PROJECT/Packages/manifest.json" && -f "$PROJECT/ProjectSettings/ProjectVersion.txt" ]] || die 'Project is not initialized; refusing to start Unity batch mode.'
    command -v unity >/dev/null || die 'Unity CLI is unavailable.'
    editor_version="$(sed -n 's/^m_EditorVersion: //p' "$PROJECT/ProjectSettings/ProjectVersion.txt")"
    [[ -n "$editor_version" ]] || die 'Could not read the required editor version.'
    editor_inventory="$(unity editors list --no-banner --format json)"
    printf '%s\n' "$editor_inventory" | python3 -c 'import json,sys; wanted=sys.argv[1]; data=json.load(sys.stdin); sys.exit(0 if any(e.get("version")==wanted and e.get("location") for e in data.get("data", [])) else 1)' "$editor_version" || die "Required Editor $editor_version is not installed."
    unity build --list-targets --no-banner | rg -q '^StandaloneLinux64[[:space:]]' || die 'Unity CLI does not list StandaloneLinux64.'
    unity build "$PROJECT" --target StandaloneLinux64 --output-path "$ROOT/tmp/unity-player/ArmoredCoreReconstruction"
    ;;
  help|-h|--help) printf 'Usage: %s {validate|build}\n' "$0" ;;
  *) die "Unknown command: $action" ;;
esac
