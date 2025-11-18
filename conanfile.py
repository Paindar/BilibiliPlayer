from conan.tools.files import copy
from conan import ConanFile

class BilibiliPlayerConan(ConanFile):
    name = "BilibiliPlayer-deps"
    version = "0.1"
    settings = "os", "arch", "compiler", "build_type"

    # Mirror dependencies from conanfile.txt
    requires = (
        "ffmpeg/7.1.1",
        "ninja/1.13.1",
        "cpp-httplib/0.15.3",
        "jsoncpp/1.9.6",
        "openssl/3.6.0",
        "spdlog/1.16.0",
        "fmt/12.0.0",
        "magic_enum/0.9.7",
        "catch2/3.11.0",
        "taglib/2.0"
    )

    # Default options reflecting conanfile.txt
    default_options = {
        "ffmpeg/*:with_opus": False,
        "ffmpeg/*:shared": True,
        "cpp-httplib/*:with_openssl": True,
        "spdlog/*:shared": True,
        "fmt/*:shared": True,
    }

    generators = "CMakeDeps", "CMakeToolchain"

    # Lightweight recipe - we only need this to be usable by `conan install`.
    def layout(self):
        pass
    
    def generate(self):
        for dep in self.dependencies.values():
            if self.settings.os == "Windows":
                self.copy_dlls(dep)
            else:
                # unsupported OS for DLL copying
                pass
    
    def copy_dlls(self, dep):
        if dep.cpp_info.bindirs:
            bindir = dep.cpp_info.bindirs[0]
            copy(self, "*.dll", 
                 src=bindir, 
                 dst=f"{self.build_folder}/bin")
