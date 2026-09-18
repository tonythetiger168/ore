# syntax=docker/dockerfile:1
# Reproducible build env for the mining SoC (matches docs/CHIPYARD_INTEGRATION.md)
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    git make autoconf automake gcc g++ device-tree-compiler \
    libgoogle-perftools-dev numactl perl libfl2 libfl-dev zlib1g-dev \
    default-jdk curl verilator help2man sudo ca-certificates \
    && rm -rf /var/lib/apt/lists/*
# chipyard wants a non-root user with sudo
ARG USER=dev
RUN useradd -m -s /bin/bash $USER && echo "$USER ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USER
USER $USER
WORKDIR /home/$USER
# clone + init submodules (toolchain excluded); toolchain build is a
# deliberate manual step (docs/CHIPYARD_INTEGRATION.md) -- it is 1-3 h.
RUN git clone https://github.com/ucb-bar/chipyard.git chipyard \
 && cd chipyard && bash scripts/init-submodules-no-riscv-tools.sh
# copy this repo in + integrate (adjust COPY source to your checkout layout)
# COPY . /home/$USER/mining_soc
# RUN bash /home/$USER/mining_soc/scripts/integrate_into_chipyard.sh /home/$USER/chipyard
# compile (needs >= 8GB container memory):
#   cd /home/$USER/chipyard && sbt compile && sbt "testOnly mining.Sha256PipeTest"
CMD ["bash"]
