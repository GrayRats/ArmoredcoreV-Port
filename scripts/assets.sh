#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/decomp-logging/entrypoint.sh"
xbox_logged_entrypoint "$ROOT" "assets" "$ROOT/scripts/assets.sh" "$@"
LOG_DIR="$ROOT/logs"
mkdir -p "$LOG_DIR"
action="${1:-help}"
stamp="$(date -u +%Y%m%dT%H%M%SZ)"
exec > >(tee -a "$LOG_DIR/assets-${action}-${stamp}.log") 2>&1
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
case "$action" in
  scan)
    source_dir="${2:-$ROOT/tmp/iso_root}"
    [[ -d "$source_dir" ]] || die "Missing extracted data directory: $source_dir"
    find "$source_dir" -type f -printf '%P\t%s bytes\n' | sort | tee "$ROOT/assets/manifests/acv-files-${stamp}.tsv"
    ;;
  convert)
    printf 'Container indexes and raw TPF payload extraction are validated; Unity-ready texture conversion is not.\n'
    die 'Conversion remains disabled until Xbox 360 format-byte mapping and Xenos untile/byte order are verified.'
    ;;
  extract-bnd3)
    bhd="${2:-$ROOT/tmp/iso_root/bind/dvdbnd5.bhd}"
    bdt="${3:-$ROOT/tmp/iso_root/bind/dvdbnd.bdt}"
    [[ -f "$bhd" ]] || die "Missing BHD5 index: $bhd"
    [[ -f "$bdt" ]] || die "Missing BDT data: $bdt"
    python3 "$ROOT/tools/bnd3_batch.py" "$bhd" "$bdt" --out "$ROOT/assets/original/acv-bnd3-extracted"
    ;;
  extract-tpf)
    input_dir="${2:-$ROOT/assets/original/acv-bnd3-extracted}"
    [[ -d "$input_dir" ]] || die "Missing BND3 extraction directory: $input_dir"
    python3 "$ROOT/tools/tpf_batch.py" "$input_dir" --out "$ROOT/assets/original/acv-bnd3-textures"
    ;;
  extract-dvdbnd-tpf)
    bhd="${2:-$ROOT/tmp/iso_root/bind/dvdbnd5.bhd}"
    bdt="${3:-$ROOT/tmp/iso_root/bind/dvdbnd.bdt}"
    [[ -f "$bhd" ]] || die "Missing BHD5 index: $bhd"
    [[ -f "$bdt" ]] || die "Missing BDT data: $bdt"
    python3 "$ROOT/tools/tpf_dvdbnd_batch.py" "$bhd" "$bdt" --out "$ROOT/assets/original/acv-dvdbnd-textures"
    ;;
  inventory-dcx)
    bhd="${2:-$ROOT/tmp/iso_root/bind/dvdbnd5.bhd}"
    bdt="${3:-$ROOT/tmp/iso_root/bind/dvdbnd.bdt}"
    [[ -f "$bhd" ]] || die "Missing BHD5 index: $bhd"
    [[ -f "$bdt" ]] || die "Missing BDT data: $bdt"
    python3 "$ROOT/tools/dcx_inventory.py" "$bhd" "$bdt" --manifest "$ROOT/assets/manifests/acv-dvdbnd-dcx-contents.tsv"
    ;;
  help|-h|--help) printf 'Usage: %s {scan [directory]|inventory-dcx [bhd bdt]|extract-bnd3 [bhd bdt]|extract-tpf [bnd3-directory]|extract-dvdbnd-tpf [bhd bdt]|convert}\n' "$0" ;;
  *) die "Unknown command: $action" ;;
esac
