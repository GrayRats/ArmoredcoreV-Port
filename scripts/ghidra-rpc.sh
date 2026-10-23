#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/decomp-logging/entrypoint.sh"
xbox_logged_entrypoint "$ROOT" "ghidra_rpc" "$ROOT/scripts/ghidra-rpc.sh" "$@"
export GHIDRA_INSTALL_DIR=/opt/ghidra
export GHIDRA_RPC_PROJECT="$ROOT/tmp/ghidra/isolated-projects/ACV_RETRY.gpr"
export GHIDRA_RPC_ENDPOINT_DIR="$ROOT/tmp/ghidra/rpc"
export GHIDRA_RPC_STATE_DIR="$ROOT/tmp/ghidra/rpc-state"
export GHIDRA_RPC_OPEN_PROGRAM=/default.xex
export UV_CACHE_DIR="$ROOT/tmp/uv-cache"
export UV_PROJECT_ENVIRONMENT="$ROOT/tmp/ghidra-rpc-venv"
export UV_PYTHON_DOWNLOADS=never
export PYTHONPYCACHEPREFIX="$ROOT/tmp/python-cache"
export PYTHONPATH="$ROOT/tools/ghidra-rpc-local"
export TMPDIR="$ROOT/tmp/ghidra/temp"
export XDG_CONFIG_HOME="$ROOT/tmp/ghidra/home/.config"
export XDG_CACHE_HOME="$ROOT/tmp/ghidra/home/.cache"
export XDG_DATA_HOME="$ROOT/tmp/ghidra/home/.local/share"
export JAVA_TOOL_OPTIONS="-Duser.home=$ROOT/tmp/ghidra/home -Dapplication.cachedir=$ROOT/tmp/ghidra/cache -Dapplication.tempdir=$TMPDIR -Djava.io.tmpdir=$TMPDIR"
mkdir -p "$GHIDRA_RPC_ENDPOINT_DIR" "$GHIDRA_RPC_STATE_DIR" "$TMPDIR" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$ROOT/tmp/ghidra/cache"
cd /home/art/.agents/skills/ghidra-rpc
exec uv run --project "$ROOT/tools/ghidra-rpc-local" ghidra-rpc "$@"
