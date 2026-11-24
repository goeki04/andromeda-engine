#!/bin/bash
# Build setup for macOS (Apple Silicon) using Apple Clang (profiles/macos for BOTH
# host -pr and build context -pr:b, so the shader tools glslang/dxc also use it).
#
# Prerequisites:
#   xcode-select --install            # Apple Clang + macOS SDK (Xcode for Metal tools)
#   brew install cmake ninja conan    # needs CMake >= 3.23
#
# The first run builds several dependencies from source (DXC alone takes a while),
# later runs reuse the Conan cache in ~/.conan2.
set -e

cd "$(dirname "$0")"

if ! command -v clang >/dev/null 2>&1; then
    echo "error: clang not found. Run 'xcode-select --install' first." >&2
    exit 1
fi

# profiles/macos pins compiler.version as a fallback. Detect the installed Apple
# Clang major version and override it per run so Conan settings match reality.
CLANG_MAJOR="$(clang --version | sed -n 's/.*clang version \([0-9]*\).*/\1/p' | head -1)"
echo "Using apple-clang ${CLANG_MAJOR}"

# Local recipes that are not on ConanCenter.
conan export ./conan-recipes/directx-shader-compiler --version 1.9.2602
# kuba-zip (via assimp): ConanCenter's pinned source hash no longer matches GitHub,
# see conan-recipes/kuba-zip/conandata.yml.
conan export ./conan-recipes/kuba-zip --version 0.3.0

for build_type in Debug Release; do
    conan install . \
        -pr profiles/macos -pr:b profiles/macos \
        -s   compiler.version="${CLANG_MAJOR}" \
        -s:b compiler.version="${CLANG_MAJOR}" \
        -s build_type="${build_type}" \
        -of conan/ --build=missing
done

echo
echo "Conan done. Configure & build with:"
echo "  cmake --preset conan-release && cmake --build --preset conan-release"
