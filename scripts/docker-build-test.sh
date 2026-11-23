#!/bin/sh
# Build and run all tests inside an ubuntu:24.04 container.
# Run from the repository root. Set DOCKER="sudo docker" if needed.
# Usage: scripts/docker-build-test.sh [package]
#   package: after the tests pass, also build the .deb into build-docker/.
# Exit code is that of the build/ctest. The image has Qt 6, so the build
# fails when the vocabulary manager is not built.
set -e
${DOCKER:-docker} build -t mcbpmf-u2404 -f docker/Dockerfile.ubuntu2404 docker
if [ "$1" = "package" ]; then
  ${DOCKER:-docker} run --rm -v "$PWD":/src -w /src mcbpmf-u2404 sh -c \
    'rm -rf build-docker && cmake -B build-docker -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DENABLE_TEST=On && cmake --build build-docker && test -x build-docker/src/VocabularyManager/mcbopomofo-vocabulary && (cd build-docker && ctest --output-on-failure && cpack -G DEB) && dpkg -I build-docker/*.deb && dpkg -c build-docker/*.deb'
else
  ${DOCKER:-docker} run --rm -v "$PWD":/src -w /src mcbpmf-u2404 sh -c \
    'cmake -B build-docker -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_TEST=On && cmake --build build-docker && test -x build-docker/src/VocabularyManager/mcbopomofo-vocabulary && cd build-docker && ctest --output-on-failure'
fi
