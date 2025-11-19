# Coverage README

This document explains how to reproduce the local code-coverage run for the project and where to find the generated report.

Location of the report (local):

- `build/coverage/coverage.html`

Prerequisites

- `conan` installed and configured for your toolchain.
- The project's Conan profile for coverage (or an equivalent profile that targets MinGW/GCC): `coverage` (you can substitute your profile name).
- `gcovr` installed (pip: `pip install gcovr`) and in PATH.
- A working native build tool matching the chosen generator (e.g., MinGW `make` for `MinGW Makefiles`, or `ninja` for `Ninja`).
- GCC toolchain that supports `--coverage` (gcov/gcovr compatible).

Quick reproduction (PowerShell)

```powershell
# 1) Install dependencies via Conan (creates toolchain in build/coverage)
conan install . --output-folder=build/coverage --build=missing --profile=coverage

# 2) Configure an instrumented build (example uses MinGW Makefiles)
cmake -B build/coverage -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE="Debug" -D "CMAKE_TOOLCHAIN_FILE=build/coverage/conan_toolchain.cmake" -DENABLE_COVERAGE=ON

# 3) Build
cmake --build build/coverage -j6

# 4) Run tests
ctest --test-dir build/coverage --output-on-failure

# 5) Generate HTML coverage (gcovr)
cd build/coverage
gcovr -r ../.. --html --html-details -o coverage.html

# 6) Open the report (PowerShell)
Start-Process coverage.html
```

Notes and tips

- If your environment uses `Ninja` as the generator, replace the `-G "MinGW Makefiles"` argument with `-G Ninja` and ensure `ninja` is on PATH.
- If `conan install` writes the toolchain to a different path, adjust the `CMAKE_TOOLCHAIN_FILE` argument accordingly.
- If `gcovr` is not available, you can use `lcov` + `genhtml` as an alternative on systems that support it.
- On CI, ensure the coverage profile or equivalent toolchain is available and that `gcovr` is installed in the runner environment.

If you want, I can:

- Add a small `specs/run_coverage.ps1` script that wraps the commands above and adds basic error checking.
- Commit the `specs/COVERAGE_README.md` or move it to a different path if you prefer.

-- BilibiliPlayer dev
