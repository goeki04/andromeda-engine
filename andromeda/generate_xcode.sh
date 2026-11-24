#!/bin/bash
# Generates an Xcode project (conan-xcode/build/Andromeda.xcodeproj) for working on
# Andromeda in Xcode. Xcode cannot open CMakeLists.txt directly - opening the folder
# only shows files, without targets, build settings or code indexing.
#
# Lives next to the Ninja build from build_macos.sh (conan/), it does not touch it.
# Xcode is a multi-config generator, so Debug and Release share one generators
# folder and are switched via the scheme in Xcode.
#
# Prerequisites: the same as build_macos.sh. Dependencies already built by it are
# reused from the Conan cache, so this only takes a moment after the first build.
set -e

cd "$(dirname "$0")"

CLANG_MAJOR="$(clang --version | sed -n 's/.*clang version \([0-9]*\).*/\1/p' | head -1)"
echo "Using apple-clang ${CLANG_MAJOR}"

conan export ./conan-recipes/directx-shader-compiler --version 1.9.2602
conan export ./conan-recipes/kuba-zip --version 0.3.0

for build_type in Debug Release; do
    # user_presets="" keeps Conan from adding this folder to CMakeUserPresets.json:
    # its preset names (conan-debug/conan-release) would collide with the Ninja ones.
    conan install . \
        -pr profiles/macos -pr:b profiles/macos \
        -s   compiler.version="${CLANG_MAJOR}" \
        -s:b compiler.version="${CLANG_MAJOR}" \
        -s build_type="${build_type}" \
        -c tools.cmake.cmaketoolchain:generator=Xcode \
        -c tools.cmake.cmaketoolchain:user_presets="" \
        -of conan-xcode/ --build=missing
done

# conanbuild.sh puts the build tools (glslangValidator, dxc, protoc) on PATH for the
# configure step. Xcode builds find them through the absolute paths CMake stored.
source conan-xcode/build/generators/conanbuild.sh
# CODE_SIGN_IDENTITY=-: ad-hoc signing. Xcode tells the linker not to sign
# (-no_adhoc_codesign) and signs in its own CodeSign phase instead; without an
# identity the executables stay unsigned, and Apple Silicon kills unsigned arm64
# binaries on launch.
#
# GTEST discovery PRE_TEST: Xcode runs CMake's post-build rules *before* its
# CodeSign phase, so the default POST_BUILD discovery (gtest_discover_tests runs
# the test binary right after linking) hits the still-unsigned binary and fails
# the build. PRE_TEST lists the tests when ctest runs instead, after signing.
cmake -S . -B conan-xcode/build -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE="$(pwd)/conan-xcode/build/generators/conan_toolchain.cmake" \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="-" \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST

echo
echo "Open in Xcode with:"
echo "  open conan-xcode/build/Andromeda.xcodeproj"
