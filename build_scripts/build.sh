#!/bin/bash

set -ex

# Configure timezone
TZ=Europe/London
ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install prerequisites
apt-get update
apt-get upgrade -y
apt-get install -y \
    git \
    make \
    libjack-dev \
    libftgl-dev \
    libglu1-mesa-dev \
    lv2-dev \
    xxd \
    g++ \

# Set up repo (assumed to be in container in /repo)
cd /repo
# git checkout .
git clean -dfx
git submodule update --init --recursive

make clean
make ENABLE_ALSA=yes
