#!/usr/bin/env bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
else
    CACHED_TYPE=$(grep 'CMAKE_BUILD_TYPE:' "${BUILD_DIR}/CMakeCache.txt" | cut -d= -f2)
    if [ "${CACHED_TYPE}" != "${BUILD_TYPE}" ]; then
        cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    fi
fi

cmake --build "${BUILD_DIR}" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

echo "Build complete (${BUILD_TYPE}). Binary: ${BUILD_DIR}/berkide-tui"
