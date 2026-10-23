#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/decomp-logging/entrypoint.sh"
xbox_logged_entrypoint "$ROOT" "decompilation" "$ROOT/scripts/decomp.sh" "$@"
ORIGINAL="${AC_ORIGINAL_DIR:-/home/art/DEV/DECOMP/Original/Armoredcore}"
LOG_DIR="$ROOT/logs"
mkdir -p "$LOG_DIR" "$ROOT/tmp"
command_name="${1:-help}"
shift || true
stamp="$(date -u +%Y%m%dT%H%M%SZ)"
log="$LOG_DIR/decomp-${command_name}-${stamp}.log"
exec > >(tee -a "$log") 2>&1
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
need_file() { [[ -f "$1" ]] || die "Missing file: $1"; }
setup_ghidra_env() {
  GHIDRA_HEADLESS="${GHIDRA_HEADLESS:-/opt/ghidra/support/analyzeHeadless}"
  [[ -x "$GHIDRA_HEADLESS" ]] || die "Ghidra headless analyzer unavailable: $GHIDRA_HEADLESS"
  mkdir -p "$ROOT/tmp/ghidra/home/.cache" "$ROOT/tmp/ghidra/home/.local/share" \
    "$ROOT/tmp/ghidra/cache" "$ROOT/tmp/ghidra/temp" "$ROOT/tmp/java-temp"
  export XDG_CONFIG_HOME="$ROOT/tmp/ghidra/home/.config"
  export XDG_CACHE_HOME="$ROOT/tmp/ghidra/home/.cache"
  export XDG_DATA_HOME="$ROOT/tmp/ghidra/home/.local/share"
  export GHIDRA_HEADLESS_JAVA_OPTIONS="-Duser.home=$ROOT/tmp/ghidra/home -Dapplication.cachedir=$ROOT/tmp/ghidra/cache -Dapplication.tempdir=$ROOT/tmp/ghidra/temp -Djava.io.tmpdir=$ROOT/tmp/java-temp"
}

case "$command_name" in
  scan)
    [[ -d "$ORIGINAL" ]] || die "Missing original directory: $ORIGINAL"
    find "$ORIGINAL" -type f -printf '%P\t%s bytes\n' | sort | tee "$LOG_DIR/original-files-${stamp}.tsv"
    while IFS= read -r -d '' image; do
      printf '\n--- XISO inventory: %s ---\n' "$image"
      if [[ -x "$ROOT/bin/extract-xiso" ]]; then
        "$ROOT/bin/extract-xiso" -l "$image"
      else
        printf 'extract-xiso unavailable; optical ISO view may omit XDVDFS data.\n'
      fi
    done < <(find "$ORIGINAL" -type f \( -iname '*.iso' -o -iname '*.xiso' \) -print0)
    ;;
  extract)
    image="${1:-$ORIGINAL/iso.iso}"
    need_file "$image"
    [[ -x "$ROOT/bin/extract-xiso" ]] || die 'Build/install bin/extract-xiso first.'
    dest="$ROOT/tmp/iso_root"
    mkdir -p "$dest"
    if [[ ! -f "$dest/default.xex" ]]; then
      "$ROOT/bin/extract-xiso" -x -d "$dest" "$image"
    else
      printf 'Using existing staged extraction at %s\n' "$dest"
    fi
    mkdir -p "$ROOT/games/acv/extracted"
    mapfile -d '' executables < <(find "$dest" -type f -iname 'default.xex' -print0)
    ((${#executables[@]} > 0)) || die "No default.xex found under $dest"
    for exe in "${executables[@]}"; do
      cp -- "$exe" "$ROOT/games/acv/extracted/default.xex"
      printf 'Staged executable: %s\n' "$ROOT/games/acv/extracted/default.xex"
    done
    ;;
  analyze)
    xex="${1:-$ROOT/games/acv/extracted/default.xex}"
    need_file "$xex"
    setup_ghidra_env
    project_dir="$ROOT/tmp/ghidra/isolated-projects"
    project_name=ACV_RETRY
    program_name="$(basename -- "$xex")"
    if [[ -f "$project_dir/$project_name.gpr" ]]; then
      "$GHIDRA_HEADLESS" "$project_dir" "$project_name" -process "$program_name" \
        -analysisTimeoutPerFile 1800 -log "$LOG_DIR/ghidra-analysis-${stamp}.log"
    else
      "$GHIDRA_HEADLESS" "$project_dir" "$project_name" -import "$xex" \
        -analysisTimeoutPerFile 1800 -log "$LOG_DIR/ghidra-analysis-${stamp}.log"
    fi
    ;;
  export)
    setup_ghidra_env
    project_dir="$ROOT/tmp/ghidra/isolated-projects"
    project_name=ACV_RETRY
    [[ -f "$project_dir/$project_name.gpr" ]] || die 'No isolated ACV Ghidra project exists yet.'
    out="$ROOT/decompilation/functions/acv-ghidra-functions.tsv"
    mkdir -p "$ROOT/decompilation/functions"
    "$GHIDRA_HEADLESS" "$project_dir" "$project_name" -process default.xex -noanalysis \
      -scriptPath "$ROOT/tools/ghidra" -postScript ExportFunctionMap.java "$out" \
      -log "$LOG_DIR/ghidra-export-${stamp}.log"
    ;;
  decompile)
    address="${1:-0x82B91F88}"
    [[ "$address" =~ ^0x[0-9a-fA-F]{1,8}$ ]] || die 'Expected a 32-bit guest address such as 0x82B91F88.'
    setup_ghidra_env
    project_dir="$ROOT/tmp/ghidra/isolated-projects"
    project_name=ACV_RETRY
    [[ -f "$project_dir/$project_name.gpr" ]] || die 'No isolated ACV Ghidra project exists yet.'
    mkdir -p "$ROOT/decompilation/pseudocode"
    safe_address="${address#0x}"
    out="$ROOT/decompilation/pseudocode/acv-${safe_address}.c"
    "$GHIDRA_HEADLESS" "$project_dir" "$project_name" -process default.xex -noanalysis \
      -scriptPath "$ROOT/tools/ghidra" -postScript DecompileAt.java "$address" "$out" \
      -log "$LOG_DIR/ghidra-decompile-${stamp}.log"
    printf 'Decompiler output: %s\n' "$out"
    ;;
  capture)
    (($# == 2)) || die 'Usage: decomp.sh capture 0xADDRESS BYTE_LENGTH'
    python3 -B "$ROOT/tools/ghidra_capture.py" "$1" "$2"
    ;;
  help|-h|--help)
    printf 'Usage: %s {scan|extract [iso]|analyze [default.xex]|export|decompile [address]|capture ADDRESS LENGTH}\n' "$0"
    ;;
  *) die "Unknown command: $command_name" ;;
esac
