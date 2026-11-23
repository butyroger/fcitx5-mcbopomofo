#!/bin/sh
# Build and run all tests inside an ubuntu:24.04 container.
# Run from the repository root. Set DOCKER="sudo docker" if needed.
# Exit code is that of the build/ctest.
set -e
${DOCKER:-docker} build -t mcbpmf-u2404 -f docker/Dockerfile.ubuntu2404 docker
${DOCKER:-docker} run --rm -v "$PWD":/src -w /src mcbpmf-u2404 sh -c \
  'cmake -B build-docker -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_TEST=On && cmake --build build-docker && cd build-docker && ctest --output-on-failure'
