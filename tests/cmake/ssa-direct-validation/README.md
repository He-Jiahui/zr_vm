# Direct checkout SSA validation

This independent CMake source directory reads every C translation unit and
header directly from the actual checkout. It creates no source snapshot and
does not change `CMAKE_SOURCE_DIR`. Build products belong outside the checkout.

Each of the seven test fragments derives `ZR_SSA_SOURCE_ROOT` with
`get_filename_component(ZR_SSA_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)`
and uses that variable for repository inputs. The same fragments work when
included from the repository parent or this bounded subproject. The driver uses
normal `project()` compiler discovery and executable ABI detection.

The driver registers eight finite targets plus the original scalar-text target.
Loop support is an explicit new registration against actual checkout files;
the foreign central registration block is not copied. LICM reuses that lower
support closure through its owned fragment. Scalar text is registered with its
six base sources; the conditional fragment adds both required helpers once.
Assertions remain active through `UNDEBUG`. The actual source graph is written
to `direct-target-sources.tsv` in the build directory after configuration. The
driver rejects missing sources, sources outside the checkout and duplicate
translation units within a target. This graph contains source paths, not source
contents or copies.

CommonMacros currently immediately includes another file through the whole
repository `CMAKE_SOURCE_DIR`. The local minimal test-settings function supplies
only first-party includes, fixture path definitions and CRT diagnostics. It
registers no third-party targets, module graph, downloads or bootstrap actions.

## Native LLVM configuration

For a new run, Root creates a fresh configuration-only toolchain in the permitted task
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
set(CMAKE_USER_MAKE_RULES_OVERRIDE_C "${CMAKE_CURRENT_LIST_DIR}/direct-llvm-rules-v31.cmake")
```

Companion rules file:

```cmake
set(CMAKE_C_LINK_EXECUTABLE [=[<CMAKE_C_COMPILER> --no-default-config /clang:--target=x86_64-pc-windows-msvc /clang:-fuse-ld=lld /nologo /MD "/winsdkdir:D:/Windows Kits/10" /winsdkversion:10.0.26100.0 "/vctoolsdir:E:/Visual Studio/VC/Tools/MSVC/14.44.35207" /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all /Fe<TARGET> <OBJECTS> /link <LINK_FLAGS> <LINK_LIBRARIES>]=])
```

Configure source: `E:/Git/zr_vm/tests/cmake/ssa-direct-validation`.
Fresh build: `E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31`.
Use Ninja, an explicit installed Ninja path, `CMAKE_BUILD_TYPE=` and
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`. Build the nine listed targets with
`--parallel 1`; verbose CTest must report exactly nine passing tests. Capture
actual primary inputs from compile commands/Ninja edges and headers/embedded C
from Ninja dependencies. The capacity fixture embeds ranges.c once.

The concrete configure argv is:

```text
E:/Visual Studio/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe
-S E:/Git/zr_vm/tests/cmake/ssa-direct-validation
-B E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31
-G Ninja
-DCMAKE_TOOLCHAIN_FILE=E:/cargo-targets/zr_vm/tmp/ssa-20261004-01a0fe2b/direct-llvm-toolchain-v31.cmake
-DCMAKE_MAKE_PROGRAM=E:/Visual Studio/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe
-DCMAKE_BUILD_TYPE=
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Supply explicit child-only INCLUDE/LIB and LLVM-only PATH using the selected
installed paths recorded in the task's `selected-llvm-capacity.json`; replace
all TEMP/TMP/TMPDIR values with a fresh direct-validation E temporary directory.
No process-global environment import or Visual Studio script is needed. That
setup JSON is tool metadata, not a source snapshot. Create the direct toolchain
and rules files in the permitted temporary directory before configuration.

Use Root's reviewed owned Job wrapper for configuration/build/CTest. Suggested
finite limits are 180 seconds for configuration, 900 for build and 330 for CTest,
within an overall 1500-second deadline. Every accepted step must terminate
naturally with zero status and an empty aggregate Job. A successful nine-test
run accepts this finite integration only. The full SSA goal, full repository
acceptance and the all47 gate remain open until their own required evidence is
recorded. The completed V31 run is recorded below.

## Completed V31 gate

Root accepted the direct checkout configuration, 138 actual compile edges,
build and all nine CTests with native Clang + LLD UBSan. The receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/direct-ssa-cmake-v31/Root-receipt.json`:
258995 bytes, SHA256
`17c71e728b1d84f679bfa7d2d60f2f46793a3c76ab6430782509d244b39e88e2`.
The overall gate took 525.4708234 seconds. Its five steps (configure, build,
CTest, Ninja dependencies and Ninja commands) each exited naturally with code
zero, no active Job processes, no termination actions and no close errors.

The configured source graph has 138 rows: loops 21, scalar text 8, capacity 1,
three branch targets 22 each, scalar arithmetic 8, scalar conditional 8 and
LICM legality 26. Source, header and embedded C dependencies refer to the real
checkout. Read the [module documentation](../../../docs/testing-and-validation/ssa-direct-validation.md)
for the integration contract and evidence limits.

Independent V31 adoption passed all 40 checks, including current input/product
pins, all 138 compile edges, UBSan instrumentation, active assertions and the
nine CTest results. Its report is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-direct-cmake-v31-current-v1.json`,
12198 bytes, SHA256
`506ad7a314b4b37af3a75af6b746e23298742f73b9079d8d9da9b1a4bb78802e`.
The actual dependency union contains 257 real input files: 109 checkout files
(67 Ninja header/embedded inputs plus 42 primary sources) and 148 read-only
system dependencies. No source snapshots are used.

Full47, full SSA milestones,
Linux, MSVC compiler, ASan and native32 acceptance remain unestablished by this
gate.
