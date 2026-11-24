#!/bin/bash
# Build setup for Linux using Clang (profiles/linux for BOTH host -pr and build
# context -pr:b, so the shader tools glslang/spirv-tools also use Clang).
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt install -y clang lld cmake ninja-build git pkg-config python3 python3-pip \
#       libgl1-mesa-dev libglu1-mesa-dev mesa-common-dev xorg-dev libxrandr-dev \
#       libxinerama-dev libxcursor-dev libxi-dev libwayland-dev wayland-protocols \
#       libxkbcommon-dev libdecor-0-dev
#   pip install --user --upgrade conan   # needs CMake >= 3.23
set -e

if ! command -v clang >/dev/null 2>&1; then
    echo "error: clang not found in PATH. Install it (see prerequisites above)." >&2
    exit 1
fi

# profiles/linux pins compiler.version. Hardcoding it there means the profile
# silently disagrees with whatever clang is actually installed (Ubuntu 24.04
# ships 18, the profile default is 21). Detect it and override per run.
CLANG_MAJOR="$(clang --version | sed -n 's/.*version \([0-9]*\).*/\1/p' | head -1)"
echo "Using clang ${CLANG_MAJOR}"

# Local recipes that are not (or not correctly) on ConanCenter.
conan export ./conan-recipes/directx-shader-compiler --version 1.9.2602
# kuba-zip (via assimp): ConanCenter's pinned source hash no longer matches GitHub,
# see conan-recipes/kuba-zip/conandata.yml.
conan export ./conan-recipes/kuba-zip --version 0.3.0

for build_type in Debug Release; do
    conan install . \
        -pr profiles/linux -pr:b profiles/linux \
        -s   compiler.version="${CLANG_MAJOR}" \
        -s:b compiler.version="${CLANG_MAJOR}" \
        -s build_type="${build_type}" \
        -of conan/ --build=missing
done

echo
echo "Conan done. Configure & build with:"
echo "  cmake --preset conan-release && cmake --build --preset conan-release"
