#!/bin/bash

# Run build script in Ubuntu container and copy output back to local machine

set -ex

cd $(dirname $(dirname $0))
rm -rf build
podman rm -f builder
podman run -dt --name builder docker.io/library/ubuntu:20.04
podman cp . builder:repo
podman exec builder /repo/build_scripts/build.sh
podman cp builder:/repo/build/ .
