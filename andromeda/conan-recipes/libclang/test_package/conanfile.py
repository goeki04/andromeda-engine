import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class LibClangTestConan(ConanFile):
    # Runs automatically on every `conan create`. It proves the three things a
    # repackaging recipe can get wrong and that nothing else notices until much
    # later: that find_package() finds the package, that the import library
    # links, and that the DLL is actually loadable and functional at runtime.
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            # env="conanrun" puts the package's bindir on PATH so libclang.dll is found.
            self.run(os.path.join(self.cpp.build.bindir, "test_package"), env="conanrun")
