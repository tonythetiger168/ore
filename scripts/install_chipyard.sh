#!/usr/bin/env bash
# install_chipyard.sh -- full official-ish install. Run on a fresh
# Ubuntu 22.04+ box (16GB RAM, 60GB disk, 8 cores ideal).
set -euo pipefail
CHIPYARD_DIR=${1:-~/chipyard}
sudo apt-get update
sudo apt-get install -y git make autoconf automake gcc g++ device-tree-compiler \
    libgoogle-perftools-dev numactl perl libfl2 libfl-dev zlib1g-dev \
    default-jdk curl verilator help2man
git clone https://github.com/ucb-bar/chipyard.git "$CHIPYARD_DIR"
cd "$CHIPYARD_DIR"
./scripts/init-submodules-no-riscv-tools.sh
./scripts/build-toolchains.sh riscv-tools
source ./env.sh
echo 'verify: cd sims/verilator && make CONFIG=RocketConfig'
