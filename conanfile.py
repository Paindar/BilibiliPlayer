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
        "catch2/3.11.0"
    )

    # Default options reflecting conanfile.txt
    default_options = {
        "ffmpeg/*:with_opus": False,
        "ffmpeg/*:shared": True,
        "cpp-httplib/*:with_openssl": True,
    }

    generators = "CMakeDeps", "CMakeToolchain"

    # Lightweight recipe - we only need this to be usable by `conan install`.
    def layout(self):
        pass
