#!/bin/sh
# Poor-man's sampler for our own rexglue_indexed codegen process (read-only eu-stack).
out=${1:?output dir}; interval=${2:-3}
pid=$(pgrep -x rexglue_indexed | head -1)
[ -n "$pid" ] || { echo "no codegen process"; exit 1; }
i=0
while kill -0 "$pid" 2>/dev/null; do
  i=$((i+1))
  eu-stack -p "$pid" -1 -m 2>/dev/null > "$out/sample-$(printf %04d $i).txt"
  sleep "$interval"
done
echo "samples=$i"
