#!/usr/bin/env bash
# integrate_into_chipyard.sh <chipyard-dir>
# Copies mining sources into the chipyard subproject and applies the
# one-line Subsystem.scala patch that mixes in CanHavePeripheryMiningAccel.
set -euo pipefail
CY=${1:?usage: integrate_into_chipyard.sh <chipyard-dir>}
DEST="$CY/generators/chipyard/src/main/scala/mining"
mkdir -p "$DEST"
cp "$(dirname "$0")"/../hw/*.scala "$DEST/"
SUBSYS="$CY/generators/chipyard/src/main/scala/DigitalTop.scala"
if ! grep -q CanHavePeripheryMiningAccel "$SUBSYS"; then
  # mix the trait into the subsystem class (same pattern as CanHavePeripheryGCD)
  sed -i 's/with chipyard.example.CanHavePeripheryGCD/with chipyard.example.CanHavePeripheryGCD\\n  with mining.CanHavePeripheryMiningAccel/' "$SUBSYS"
fi
grep -q "mining_accel" "$SUBSYS" && echo "patched Subsystem.scala" || echo "WARNING: pattern not found -- inspect $SUBSYS"
echo "next: cd $CY && JAVA_OPTS=-Xmx2g sbt compile && sbt \"testOnly mining.Sha256PipeTest\""
