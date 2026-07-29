# CMake Portability And Feature Switches Design

## Goal

Remove checkout-specific paths from tracked CMake files and expose functional
CMake options for optional ZR VM components without changing the default build.

## Scope

- Stop tracking CMake-generated `CTestTestfile.cmake` and `cmake_install.cmake`
  artifacts, which embed absolute source, toolchain, and build paths.
- Ignore those generated artifacts and other standard CMake build outputs.
- Gate optional network, debug, thread, language-server, Rust-binding, and CLI
  subdirectories from the top-level `CMakeLists.txt`.
- Preserve the required core and standard-library chain: common, core, parser,
  library, math, system, iteration, container, and FFI.

## Non-Goals

- Do not hand-edit generated CMake files; CMake overwrites them at configure
  time.
- Do not make required modules independently switchable. Their existing target
  dependencies would require source-level feature work beyond this CMake task.
- Do not modify the existing `zr_vm_lib_iteration` staged/unstaged conflict.

## Design

The top-level configuration will replace the unused
`BUILD_EXPERIMENTAL_NETWORK_LIB` cache option with independently documented,
default-ON options for the optional components:

- `BUILD_NETWORK_LIB`
- `BUILD_DEBUG_LIB`
- `BUILD_THREAD_LIB`
- `BUILD_LANGUAGE_SERVER`
- `BUILD_RUST_BINDING`
- `BUILD_CLI`

Each option conditionally calls `add_subdirectory`. Debug and Rust binding
require the network library, so they are skipped with an explicit configure
status message when networking is disabled. The language-server extension
requires `BUILD_LANGUAGE_SERVER=ON`; an incompatible request fails during
configure with a specific diagnostic. CLI and language server continue to
detect their already-optional runtime dependencies by CMake target presence.

The repository will ignore `CMakeCache.txt`, `CMakeFiles/`,
`CTestTestfile.cmake`, `cmake_install.cmake`, `build.ninja`, and `Makefile` at
any generated build-directory depth. The 48 currently tracked generated test
and install files will be removed from Git's index only; their local copies
remain available until a normal CMake configure refreshes them.

## Validation

- Configure a minimal core/CLI build with all optional components disabled.
- Configure the default component set.
- Configure with a disabled network library and verify dependent optional
  modules are skipped, not configured.
- Run the required WSL GCC and Clang builds, then a Windows MSVC CLI smoke.

## Commit Isolation

`CMakeLists.txt` currently has an unrelated staged removal and unstaged
restoration of `zr_vm_lib_iteration`. Commits for this work use a temporary Git
index populated from `HEAD`, stage only this task's files, and leave the shared
index and working tree intact.
