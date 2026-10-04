# Direct checkout SSA validation

This independent CMake source directory reads every C translation unit and
header directly from the actual checkout. It creates no source snapshot and
does not change `CMAKE_SOURCE_DIR`. Build products belong outside the checkout.

The seven owned fragments currently assume the full repository source root.
After Root releases their active native validation pins, each fragment must
derive `ZR_SSA_SOURCE_ROOT` with
`get_filename_component(ZR_SSA_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)`
and use that variable for repository inputs. This works from both the repository
parent and this bounded subproject. Until that migration is complete, this
driver stops before `project()` or compiler discovery. No fragment was modified
while preparing this driver.

The driver registers eight finite targets plus the original scalar-text target.
Loop support is an explicit new registration against actual checkout files;
the foreign central registration block is not copied. LICM reuses that lower
support closure through its owned fragment. Scalar text is registered with its
six base sources; the conditional fragment adds both required helpers once.
Assertions remain active through `UNDEBUG`. The actual source graph is written
to `direct-target-sources.tsv` in the build directory after configuration.

CommonMacros currently immediately includes another file through the whole
repository `CMAKE_SOURCE_DIR`. The local minimal test-settings function supplies
only first-party includes, fixture path definitions and CRT diagnostics. It
registers no third-party targets, module graph, downloads or bootstrap actions.

## Native LLVM configuration

Root should create a fresh configuration-only toolchain in the permitted task
temporary directory, using installed `clang-cl.exe`, `lld-link.exe` and Ninja.
Use the verified LLVM resource include first, then VC/SDK paths uniformly as
`/clang:-isystem` pairs, and project include paths from the actual fragments.
Use `MultiThreadedDLL`, no debug-format default, `--no-default-config`, explicit
x64 MSVC target and SDK/VC roots. Keep the child PATH limited to LLVM bin,
resource runtime directory and Windows system directories; installed SDK files
on D stay read only. Temporary and build/report paths remain under the permitted
E task directories.

For LLVM linking use the verified driver rule: compiler flags and sanitizer
options before `/link`, followed by `<LINK_FLAGS> <LINK_LIBRARIES>`. A toolchain
rules override is configuration, not a compiler-success override. Do not force
compiler ID, compiler WORKS, static-only try_compile or the built-in source root.
Keep normal executable ABI detection enabled. For native clang-cl the rule must
explicitly place `/clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all`
before `/link`. Target link options would enter `<LINK_FLAGS>` after `/link`,
where they would incorrectly be parsed by LLD. The driver checks this ordering;
the target supplies the compile instrumentation flags.

Configuration-only toolchain outline (Root writes it under the permitted E
temporary directory, with a companion rules file):

```cmake
set(CMAKE_C_COMPILER "E:/Visual Studio/VC/Tools/Llvm/x64/bin/clang-cl.exe" CACHE FILEPATH "")
set(CMAKE_LINKER "E:/Visual Studio/VC/Tools/Llvm/x64/bin/lld-link.exe" CACHE FILEPATH "")
set(CMAKE_RC_COMPILER "E:/Visual Studio/VC/Tools/Llvm/x64/bin/llvm-rc.exe" CACHE FILEPATH "")
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE STRING "")
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "" CACHE STRING "")
set(CMAKE_C_FLAGS_INIT [=[--no-default-config /clang:--target=x86_64-pc-windows-msvc /nologo /TC /std:c11 /Od /W4 /utf-8 "/winsdkdir:D:/Windows Kits/10" /winsdkversion:10.0.26100.0 "/vctoolsdir:E:/Visual Studio/VC/Tools/MSVC/14.44.35207"]=])
foreach(_system_include IN ITEMS
    "E:/Visual Studio/VC/Tools/Llvm/x64/lib/clang/19/include"
    "E:/Visual Studio/VC/Tools/MSVC/14.44.35207/include"
    "D:/Windows Kits/10/Include/10.0.26100.0/ucrt"
    "D:/Windows Kits/10/Include/10.0.26100.0/shared"
    "D:/Windows Kits/10/Include/10.0.26100.0/um"
    "D:/Windows Kits/10/Include/10.0.26100.0/winrt"
    "D:/Windows Kits/10/Include/10.0.26100.0/cppwinrt")
    string(APPEND CMAKE_C_FLAGS_INIT " /clang:-isystem \"/clang:${_system_include}\"")
endforeach()
set(CMAKE_USER_MAKE_RULES_OVERRIDE_C "${CMAKE_CURRENT_LIST_DIR}/direct-llvm-rules.cmake")
```

Companion rules file:

```cmake
set(CMAKE_C_LINK_EXECUTABLE [=[<CMAKE_C_COMPILER> --no-default-config /clang:--target=x86_64-pc-windows-msvc /clang:-fuse-ld=lld /nologo /MD "/winsdkdir:D:/Windows Kits/10" /winsdkversion:10.0.26100.0 "/vctoolsdir:E:/Visual Studio/VC/Tools/MSVC/14.44.35207" /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all /Fe<TARGET> <OBJECTS> /link <LINK_FLAGS> <LINK_LIBRARIES>]=])
```

Proposed configure source: `E:/Git/zr_vm/tests/cmake/ssa-direct-validation`.
Proposed fresh build: `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/direct-ssa-cmake`.
Use Ninja, an explicit installed Ninja path, `CMAKE_BUILD_TYPE=` and
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`. Build the nine listed targets with
`--parallel 1`; verbose CTest must report exactly nine passing tests. Capture
actual primary inputs from compile commands/Ninja edges and headers/embedded C
from Ninja dependencies. The capacity fixture embeds ranges.c once.

After fragment migration, the concrete configure argv is:

```text
E:/Visual Studio/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe
-S E:/Git/zr_vm/tests/cmake/ssa-direct-validation
-B E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/direct-ssa-cmake
-G Ninja
-DCMAKE_TOOLCHAIN_FILE=E:/cargo-targets/zr_vm/tmp/ssa-20261004-01a0fe2b/msvc-setup/direct-llvm-toolchain.cmake
-DCMAKE_MAKE_PROGRAM=E:/Visual Studio/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe
-DCMAKE_BUILD_TYPE=
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Supply explicit child-only INCLUDE/LIB and LLVM-only PATH using the selected
installed paths recorded in the task's `selected-llvm-capacity.json`; replace
all TEMP/TMP/TMPDIR values with a fresh direct-validation E temporary directory.
No process-global environment import or Visual Studio script is needed. That
setup JSON is tool metadata, not a source snapshot. The new direct toolchain and
rules files shown above have not been created or executed by this preparation.

Use Root's reviewed owned Job wrapper for configuration/build/CTest. Suggested
finite limits are 180 seconds for configuration, 900 for build and 330 for CTest,
within an overall 1500-second deadline. Every accepted step must terminate
naturally with zero status and an empty aggregate Job. No tool has been run for
this preparation. Full repository/all47 acceptance and commit remain separate.
