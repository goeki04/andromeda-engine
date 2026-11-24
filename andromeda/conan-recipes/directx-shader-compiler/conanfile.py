from conan import ConanFile
from conan.tools.files import get, copy
from conan.errors import ConanInvalidConfiguration
import os
import shutil

class DirectXShaderCompiler(ConanFile):
    name = "directx-shader-compiler"
    version = "1.9.2602"

    settings = "os", "arch"
    package_type = "application"

    description = "DirectX Shader Compiler (DXC) is a compiler for HLSL."
    homepage = "https://github.com/microsoft/DirectXShaderCompiler"
    license = "LLVM-Exception"

    # SHA256 of the prebuilt Windows release asset for this version, fetched
    # from the GitHub release metadata. Ensures the download can't silently
    # be swapped/corrupted.
    _windows_sha256 = "a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e"

    # Microsoft publishes no macOS binaries, so on macOS DXC is built from the
    # release tag v{version}.
    _git_url = "https://github.com/microsoft/DirectXShaderCompiler.git"

    def validate(self):
        if self.settings.os not in ("Windows", "Macos"):
            raise ConanInvalidConfiguration(
                "directx-shader-compiler currently only packages Windows (prebuilt) and macOS (from source).")

    def build(self):
        if self.settings.os == "Windows":
            url = f"https://github.com/microsoft/DirectXShaderCompiler/releases/download/v{self.version}/dxc_2026_02_20.zip"
            # get() downloads the zip and extracts it directly into the build folder
            get(self, url, sha256=self._windows_sha256)
        else:
            self._build_from_source()

    def _build_from_source(self):
        src = os.path.join(self.build_folder, "src")
        out = os.path.join(self.build_folder, "out")
        if not os.path.isdir(src):
            # DXC vendors SPIRV-Headers/SPIRV-Tools/DirectX-Headers as git submodules,
            # which GitHub's source tarballs do not include - hence a git clone.
            self.run(f"git clone --depth 1 --branch v{self.version} --recurse-submodules "
                     f"--shallow-submodules {self._git_url} \"{src}\"")

        osx_arch = "arm64" if self.settings.arch == "armv8" else "x86_64"
        # -Wno-invalid-specialization: Apple Clang 21 rejects DXC's (LLVM 3.7-era)
        # std::is_nothrow_constructible specializations in StringRef.h as an error.
        # Test targets are switched off - only the dxc executable is needed.
        self.run(
            f"cmake -S \"{src}\" -B \"{out}\" -G Ninja "
            f"-C \"{src}/cmake/caches/PredefinedParams.cmake\" "
            f"-DCMAKE_BUILD_TYPE=Release "
            f"-DCMAKE_OSX_ARCHITECTURES={osx_arch} "
            f"-DCMAKE_POLICY_VERSION_MINIMUM=3.5 "
            f"-DCMAKE_CXX_FLAGS=-Wno-invalid-specialization "
            f"-DHLSL_INCLUDE_TESTS=OFF -DSPIRV_BUILD_TESTS=OFF "
            f"-DLLVM_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_TESTS=OFF "
            f"-DLLVM_ENABLE_ASSERTIONS=OFF -DLLVM_APPEND_VC_REV=OFF")
        self.run(f"cmake --build \"{out}\" --target dxc")

    def package(self):
        if self.settings.os == "Windows":
            # The release zip ships per-architecture subfolders (bin/x64, bin/arm64, ...).
            # Flatten the one matching our arch into package_folder/bin so that
            # find_program()/CMAKE_PROGRAM_PATH can locate dxc.exe directly.
            arch_folder = "arm64" if self.settings.arch == "armv8" else "x64"
            copy(self, "*", src=os.path.join(self.build_folder, "bin", arch_folder),
                 dst=os.path.join(self.package_folder, "bin"))
            copy(self, "*", src=os.path.join(self.build_folder, "lib", arch_folder),
                 dst=os.path.join(self.package_folder, "lib"))
            copy(self, "*", src=os.path.join(self.build_folder, "inc"),
                 dst=os.path.join(self.package_folder, "include"))
            copy(self, "LICENSE*", src=self.build_folder,
                 dst=os.path.join(self.package_folder, "licenses"))
            return

        # From-source layout: bin/dxc (a symlink to dxc-3.7) loads
        # @rpath/libdxcompiler.dylib with rpath @executable_path/../lib, so keeping
        # the bin/ + lib/ pair intact is all it needs to run from the package.
        src = os.path.join(self.build_folder, "src")
        out = os.path.join(self.build_folder, "out")
        bin_dir = os.path.join(self.package_folder, "bin")
        lib_dir = os.path.join(self.package_folder, "lib")
        os.makedirs(bin_dir, exist_ok=True)
        os.makedirs(lib_dir, exist_ok=True)
        # shutil.copy follows the symlink, so the package gets a real dxc binary.
        shutil.copy(os.path.join(out, "bin", "dxc"), os.path.join(bin_dir, "dxc"))
        shutil.copy(os.path.join(out, "lib", "libdxcompiler.dylib"), lib_dir)
        copy(self, "*", src=os.path.join(src, "include", "dxc"),
             dst=os.path.join(self.package_folder, "include", "dxc"))
        copy(self, "LICENSE*", src=src, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        bin_path = os.path.join(self.package_folder, "bin")
        self.buildenv_info.prepend_path("PATH", bin_path)
