# Build setup for Windows using the released Visual Studio 2022 toolset.
# Uses the profiles/vs2022 profile for BOTH host (-pr) and build context (-pr:b),
# so the shader tools (glslang/spirv-tools) also resolve to prebuilt binaries.

conan export ./conan-recipes/directx-shader-compiler --version 1.9.2602
conan export ./conan-recipes/libclang --version 21.1.8

conan install . -pr profiles/vs2022 -pr:b profiles/vs2022 -s build_type=Debug   -of conan/ --build=missing
conan install . -pr profiles/vs2022 -pr:b profiles/vs2022 -s build_type=Release -of conan/ --build=missing
Read-Host "Press Enter to close"
