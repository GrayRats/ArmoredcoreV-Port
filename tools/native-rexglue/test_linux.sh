#!/usr/bin/env bash
# Run the current Linux test build with the canonical config and persistent profile.
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd -- "$project_root"
exec node "$project_root/tools/decomp-logging/cli.mjs" run \
  --stage acv_linux_test --timeout-ms 360000 -- \
  python3 "$project_root/tools/native-rexglue/probe_linux.py" \
  --config "$project_root/tools/native-rexglue/acv.config" \
  --user-root "$project_root/tmp/acv-mnk-profile" --seconds 300 "$@"
