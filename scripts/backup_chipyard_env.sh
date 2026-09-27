#!/usr/bin/env bash
# backup_chipyard_env.sh -- persist the built chipyard environment so a
# sandbox reset costs minutes, not 40. Run after sbt compile completes.
set -euo pipefail
OUT=${1:-/mnt/agents/output}
HOME=${HOME:-~}
echo "== taring chipyard (this is big, ~5-8GB -> ~1.5-2GB gz) =="
tar -C "$HOME" -czf "$OUT/chipyard_env.tar.gz" chipyard .local .sbt .cache/coursier 2>/dev/null || \
tar -C "$HOME" -czf "$OUT/chipyard_env.tar.gz" chipyard .local .sbt 2>/dev/null || \
tar -C "$HOME" -czf "$OUT/chipyard_env.tar.gz" chipyard .local
ls -la "$OUT"/chipyard_env.tar.gz
echo "restore next session: tar -C ~ -xzf $OUT/chipyard_env.tar.gz"
