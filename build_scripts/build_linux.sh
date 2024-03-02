#!/bin/bash

set -eux

# Install prerequisites
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install -y \
    git \
    make \
    libjack-dev \
    libftgl-dev \
    libglu1-mesa-dev \
    lv2-dev \
    xxd \
    g++ \

make

ls build
ls build/tuneBfree.lv2
