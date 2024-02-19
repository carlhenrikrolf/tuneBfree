#!/bin/bash

# Run build script in Ubuntu container and copy output back to local machine

set -ex

cd $(dirname $(dirname $0))
rm -rf build
mkdir -p build/b_synth/
podman rm -f builder
podman run -dt --name builder docker.io/library/ubuntu:18.04
podman cp . builder:repo
podman exec builder /repo/build_scripts/build.sh
for ITEM in b_synth.so b_synth.ttl b_synthUI.so manifest.ttl modgui.ttl modgui
do
    podman cp builder:/repo/b_synth/$ITEM ./build/b_synth
done
