#!/bin/sh

# Install prerequisites
brew install lv2 jack freeglut ftgl bzip2 pkg-config

make

ls build
ls build/tuneBfree.lv2
