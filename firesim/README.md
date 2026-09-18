# FireSim bring-up

    cd sims/firesim
    source sourceme-f1-manager.sh
    # 1. build AGFI (takes ~1-2 h)
    firesim buildafi --buildconfigfile deploy/config_build.ini
    #    select the rocket-mining-firesim recipe
    # 2. smoke test with the bare-metal genesis KAT
    firesim launchrunfarm && firesim infrasetup
    firesim runworkload --workload-name miner-smoke
    # pass criterion: exit code 0 (tohost=1)

# Linux + stratum client: build an initramfs image with FireMarshal
# (firesim-software), then run as a workload. Networking: either use the
# FireSim NIC design + firesim-software network config, or the pragmatic
# route -- a host-side proxy speaking stratum over the simulated UART line
# (custom framing or SLIP). For a testnet share test the UART proxy is by
# far the least effort.

# Throughput sanity check on F1: engines should sustain
# 8 engines x 90 MHz sim clock; compare found-rate vs theory (difficulty).
