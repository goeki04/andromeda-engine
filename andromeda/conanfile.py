from conan import ConanFile
from conan.tools.cmake import cmake_layout
import requests
class CmakeTest(ConanFile):
    generators = ("CMakeToolchain", "CMakeDeps")
    settings = ("os","build_type", "arch", "compiler")
    # The glslang 1.4.350.0 Conan recipe fails to build from source with the
    # optimizer enabled: it does not propagate the spirv-tools include dirs, so
    # SPIRV/SpvTools.h cannot find 'spirv-tools/libspirv.h'. We don't need the
    # SPIR-V optimizer (shaders are compiled and reflected separately), so we
    # disable it for every glslang instance (host + build context).
    default_options = {
        "glslang/*:enable_optimizer": False,
        "boost/*:without_cobalt": True,
    }
    def requirements(self):
        self.requires("glm/1.0.1")
        self.requires("sdl/3.2.20")
        self.requires("imgui/1.92.5-docking", override=True)
        self.requires("glew/2.2.0")
        self.requires("nlohmann_json/3.12.0")
        self.requires("assimp/6.0.2")
        self.requires("cpp-httplib/0.47.0")
        self.requires("spirv-reflect/1.4.350.0")
        self.requires("glslang/1.4.350.0")
        self.requires("stb/cci.20230920")
        self.requires("protobuf/3.21.12")
        self.requires("openssl/4.0.1")
        self.requires("boost/1.91.0")
        self.requires("imguizmo/cci.20231114")
        self.requires("spdlog/1.17.0")
        # implot is vendored from master (bindings/implot) for imgui 1.92 compatibility.
        # ConanCenter only ships implot/0.17, which is too old for imgui 1.92 and fails
        # to link in Release (missing ImGui::GetForegroundDrawList from ShowMetricsWindow).
        self.requires("gtest/1.17.0")
        # libclang (conan-recipes/libclang) repackages the official LLVM release
        # binaries for modules/reflection. Windows-only for now - the reflection
        # module degrades to a no-op elsewhere, so the Linux build stays green.
        if self.settings.os == "Windows":
            self.requires("libclang/21.1.8")
    def build_requirements(self):
        self.tool_requires("glslang/1.4.350.0")
        self.tool_requires("directx-shader-compiler/1.9.2602")
    def layout(self):
        cmake_layout(self)