# Source from project scripts after resolving ROOT. Nested commands share the parent capture.
xbox_logged_entrypoint() {
  local task_root="$1" task_stage="$2" task_script="$3"
  shift 3
  [[ -n "${XBOXDECOMP_LOGGING_ACTIVE:-}" ]] && return 0
  command -v node >/dev/null || { printf 'ERROR: Node is required for persistent project logging.\n' >&2; return 1; }
  exec node "$task_root/tools/decomp-logging/cli.mjs" run --stage "$task_stage" --cwd "$PWD" -- bash "$task_script" "$@"
}
