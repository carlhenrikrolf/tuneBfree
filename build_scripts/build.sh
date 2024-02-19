#!/bin/bash

set -ex

# Install prerequisites
apt-get update
apt-get upgrade -y
apt-get install -y \
    git \
    make \
    libjack-dev \
    libftgl-dev \
    libglu1-mesa-dev \
    ttf-bitstream-vera \
    lv2-dev \
    libasound2-dev

# Set up repo (assumed to be in container in /repo)
cd /repo
# git checkout .
git clean -dfx
git submodule update --init --recursive

make clean
make ENABLE_ALSA=yes
