from conan import ConanFile
from conan.tools.files import download, copy
from conan.errors import ConanInvalidConfiguration, ConanException
import os
import shutil
import tarfile


class LibClangConan(ConanFile):
    name = "libclang"
    version = "21.1.8"

    settings = "os", "arch"
    package_type = "shared-library"

    description = "Clang's stable C API (libclang), repackaged from the official LLVM release binaries."
    homepage = "https://clang.llvm.org/doxygen/group__CINDEX.html"
    license = "Apache-2.0 WITH LLVM-exception"



    @property
    def _archive(self):
        return f"clang+llvm-{self.version}-x86_64-pc-windows-msvc.tar.xz"


    _sha256 = None


    _wanted = ("bin/libclang.dll", "lib/libclang.lib", "include/clang-c/", "LICENSE.TXT")

    def validate(self):
        if self.settings.os != "Windows" or self.settings.arch != "x86_64":
            raise ConanInvalidConfiguration(
                "libclang currently only repackages the official x86_64 Windows LLVM binaries. "
                "To add Linux, either extract LLVM-<version>-Linux-X64.tar.xz here the same way "
                "(1.9 GB download) or install the distribution's libclang-dev via "
                "conan.tools.system.package_manager.Apt - profiles/linux already enables "
                "tools.system.package_manager:mode=install."
            )

    def build(self):
        url = (f"https://github.com/llvm/llvm-project/releases/download/"
               f"llvmorg-{self.version}/{self._archive}")

        if not self._sha256:
            self.output.warning(
                "Downloading libclang without checksum verification. "
                "Set _sha256 in conan-recipes/libclang/conanfile.py to pin the artifact."
            )

        self.output.info(f"Downloading {self._archive} (899 MB) to {self.build_folder}")
        download(self, url, self._archive, sha256=self._sha256)

        top_level_dirs = set()
        with tarfile.open(os.path.join(self.build_folder, self._archive)) as archive:
            for member in archive:
                if not member.isfile():
                    continue
                _, _, relative = member.name.partition("/")
                if not relative.startswith(self._wanted):
                    continue
                destination = os.path.join(self.build_folder, *relative.split("/"))
                os.makedirs(os.path.dirname(destination), exist_ok=True)
                with archive.extractfile(member) as source, open(destination, "wb") as target:
                    shutil.copyfileobj(source, target)
                top_level_dirs.add(relative.split("/")[0])

        missing = {"bin", "lib", "include"} - top_level_dirs
        if missing:
            raise ConanException(
                f"{self._archive} did not contain the expected libclang files ({sorted(missing)}). "
                f"The layout of the LLVM release asset may have changed - check _wanted in this recipe."
            )

    def package(self):
        copy(self, "libclang.dll",
             src=os.path.join(self.build_folder, "bin"),
             dst=os.path.join(self.package_folder, "bin"))
        copy(self, "libclang.lib",
             src=os.path.join(self.build_folder, "lib"),
             dst=os.path.join(self.package_folder, "lib"))
        copy(self, "*.h",
             src=os.path.join(self.build_folder, "include", "clang-c"),
             dst=os.path.join(self.package_folder, "include", "clang-c"))
        copy(self, "LICENSE.TXT",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "libclang")
        self.cpp_info.set_property("cmake_target_name", "libclang::libclang")
        self.cpp_info.libs = ["libclang"]
        self.cpp_info.bindirs = ["bin"]
