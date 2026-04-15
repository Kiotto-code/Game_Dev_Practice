#!/usr/bin/env bash
set -euo pipefail

# Configure out-of-source build and reuse existing build dir safely.
# Use vcpkg only when a valid toolchain file is available.


VCPKG_TOOLCHAIN="${VCPKG_TOOLCHAIN:-}"
BUILD_DIR="build"

# if [[ -n "${VCPKG_ROOT:-}" ]] && [[ -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
# 	VCPKG_TOOLCHAIN="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
# elif [[ -f "./vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
# 	VCPKG_TOOLCHAIN="./vcpkg/scripts/buildsystems/vcpkg.cmake"
# fi

if [[ -n "${VCPKG_TOOLCHAIN}" ]]; then
	cmake -S . -B "${BUILD_DIR}" -DCMAKE_TOOLCHAIN_FILE="${VCPKG_TOOLCHAIN}"
else
	echo "vcpkg toolchain not found; configuring without vcpkg (using external/ dependencies)."
	cmake -S . -B "${BUILD_DIR}"
fi

# Build selected configuration.
cmake --build "${BUILD_DIR}" -j

# Run app.
./"${BUILD_DIR}"/RaylibEnttGame