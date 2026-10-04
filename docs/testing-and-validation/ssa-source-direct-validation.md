---
related_code:
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_compare.inc
  - tests/parser/ssa_source_execbc_vm_diagnostics.inc
  - tests/parser/ssa_source_execbc_vm_loop_break.inc
  - tests/harness/runtime_support.c
  - tests/unity_config.h
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
  - zr_vm_core/CMakeLists.txt
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_library/CMakeLists.txt
  - zr_vm_library/third_party/zr_miniz/miniz/miniz_export.h
implementation_files:
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
plan_sources:
  - user: 2026-10-04 actual-checkout native SSA source route without source snapshots
  - .codex/plans/20261004-source-comparison-native-recipe-1850.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
doc_type: testing-guide
---

# Actual-checkout native SSA source validation

## Purpose and evidence boundary

The standalone project under `tests/cmake/ssa-source-direct-validation` prepares
the real local-source pipeline fixture for a bounded native build. Source files
and headers are read directly from the actual checkout. Sources are not copied,
concatenated into artificial unity files, restored from history or replaced with
stubs. It preserves the existing nine-target direct-validation driver.

This document records the recipe and Root's actual native attempts. Driver and
documentation authoring involved filesystem reads and owned edits only. No CMake configuration,
compiler/linker/runtime invocation, WSL, Git/index, process-management, provider,
security or network operation was performed by the author. Root owns native
validation; V46 established four-case producer RED, and V48 passed the original
13 regressions after freshly relinking the sealed historical inputs with a larger
stack reserve. V48 did not compile producer changes being edited. The earlier nine-target driver's success does not establish this
source-route target. Four-consumer same-source C/LLVM evidence and the full
47-item SSA acceptance remain open.

## Initial source pools and later additions

All paths below are relative to the current repository root, derived from the
driver location. Counts were read on 2026-10-04 and describe the historical
666-TU V42–V46 pool; configure-time metadata records actual current counts and hashes.

| Archive | Source selection | TUs at authoring |
| --- | --- | ---: |
| `ssa_source_zr_vm_parser` | Recursive `zr_vm_parser/src/**/*.c` | 397 |
| `ssa_source_zr_vm_core` | Recursive `zr_vm_core/src/**/*.c` | 218 |
| `ssa_source_zr_vm_library` | Recursive `zr_vm_library/src/**/*.c` | 40 |
| `ssa_source_zr_vm_common` | `zr_vm_common/src/zr_vm_common/ssa_platform_contract.c` | 1 |
| `ssa_source_xxhash` | `zr_vm_core/third_party/zr_xx_hash/xxHash/xxhash.c` | 1 |
| `ssa_source_utf8proc` | `zr_vm_core/third_party/zr_utf8proc/utf8proc/utf8proc.c` | 1 |
| `ssa_source_cjson` | `zr_vm_library/third_party/zr_c_json/cJSON/cJSON.c` | 1 |
| `ssa_source_miniz` | `miniz.c`, `miniz_tdef.c`, `miniz_tinfl.c`, `miniz_zip.c` in `zr_vm_library/third_party/zr_miniz/miniz` | 4 |
| `ssa_source_unity` | `tests/third_party/zr_unity/Unity/src/unity.c` | 1 |
| Executable objects | Actual test and `tests/harness/runtime_support.c` | 2 |
| **Initial compile pool** | | **666** |

Three additional parser TUs will raise the parser pool from 397 to 400 and a newly
configured pool from 666 to 669. The sealed V46 objects, archives and PE remain
evidence for the historical 666-TU pool. They do not validate those three later
TUs or a new 669-TU configuration.

The recursive parser pool includes the newly added SCCP helper. Common is compiled
once in its own archive, rather than embedded in parser/core/library. Sources are
deduplicated within pools and checked for duplicates across all targets. Each
resolved source must remain inside the actual checkout.

Tinydir's implementation is supplied by its actual header. Its placeholder C file
exports no needed implementation and is omitted. The `.inc` files included by the
test remain local textual includes and are not separate TUs. The selected fixture
does not call path/reference/crash-guard harness functions, so those additional
formal Unity-helper objects are not selected.

The formal parser variant also compiles 143 enabled AOT backend TUs. The audited
parser/core/library pools do not reference their external writer exports, and
this fixture has no C/LLVM backend execution. They are omitted initially. Real
unresolved-symbol evidence must identify any mandatory additional actual TU and
its support closure. This recipe is a focused module selection, not a proven
minimum TU set or the exact formal top-level parser variant.

## Compile definitions and include paths

First-party archives and the executable receive `ZR_LIBRARY_TYPE_STATIC=1`,
`ZR_PLATFORM_WIN=1`, `ZR_PLATFORM_WIN_USE_MSVC=1`, `ZR_DEBUG=1`,
`_CRT_SECURE_NO_WARNINGS`, a quoted target-specific `ZR_CURRENT_MODULE` string,
and quoted `ZR_VM_COMPILER_VERSION` from real compiler detection. Static APIs
use ordinary extern declarations. The driver reproduces these definitions and
the module include layout directly; it does not include `CommonMacros.cmake`,
which assumes a full-repository `CMAKE_SOURCE_DIR` and third-party target graph.

The common first-party include roots are the actual repository root plus
`zr_vm_common/include`, `zr_vm_core/include`, `zr_vm_parser/include`, and
`zr_vm_library/include`. Each module adds its actual `src/<module>` private root.
Parser also adds `src/zr_vm_parser/{compiler,parser,type_inference,writer}`.

Vendor include roots are the actual wrapper and nested source directories for
xxHash, utf8proc, cJSON, miniz and tinydir. Test includes add `tests`,
`tests/harness`, and `tests/third_party/zr_unity/Unity/src`. None are copied.
`UTF8PROC_STATIC` is set for utf8proc and its first-party consumers.
`MINIZ_NO_ZLIB_COMPATIBLE_NAMES` is set for miniz and its first-party consumers;
its normal ZIP and stdio APIs remain enabled.

Actual Unity, the test and runtime harness share `UNITY_INCLUDE_CONFIG_H`,
`UNITY_INCLUDE_DOUBLE`, `UNITY_DOUBLE_PRECISION=1e-6`, and
`UNITY_USE_FLUSH_STDOUT`. The actual `tests/unity_config.h` is included.
`ZR_VM_TESTS_SOURCE_DIR` names the actual tests directory and
`ZR_VM_TESTS_BINARY_DIR` names the external build directory. No invented
`UNITY_TEST_PROTECT` or crash-guard override is used.

All selected C TUs use C11, `/UNDEBUG /Od /MD /W4 /utf-8 /Gy /Gw`. UBSan is on by
default, with `/clang:-fsanitize=undefined`,
`/clang:-fno-sanitize-recover=all`, and `/clang:-fno-omit-frame-pointer`. These
settings cover both first-party and selected vendor C inputs. Root's toolchain
supplies the real compiler/resource/MSVC/SDK roots and Windows target flags.

## Native compiler, archive and output contract

The driver requires CMake 3.31+, single-configuration Ninja, Debug, actual Windows
clang-cl and actual `llvm-lib.exe` supplied as `CMAKE_AR`. `project(... LANGUAGES C)`
performs normal compiler identification and an executable ABI/link probe. The
driver does not force compiler IDs, override compiler-WORKS results or replace
executable probes with static-library probes.

Normal CMake static-library creation uses
`<CMAKE_AR> /nologo /out:<TARGET> <OBJECTS>`. CMake quotes the expanded tool path
itself; manually quoting `<CMAKE_AR>` duplicates quotes for installation paths
containing spaces. Compiler and UBSan flags do not
reach the archive tool. No MSVC initialization, lib.exe, vcvars, cl.exe, VCTIP,
ASM, vendor-copy bootstrap, FFI or external provider target is introduced.

The executable names all nine actual static archives and `kernel32`. Windows CRT
and sanitizer runtime selection comes from Root's verified clang-cl link driver
and explicit MSVC/SDK search roots. The driver checks that UBSan options occur
before `/link`. Native link flags request `/INCREMENTAL:NO /OPT:REF /VERBOSE:LIB`
and an external map file. After V46's regression stack overflow, Root added
`/STACK:8388608` for a fresh relink of sealed V46 artifacts. Archive extraction remains normal: no `/WHOLEARCHIVE`,
unresolved-symbol suppression, duplicate-symbol allowance or required-symbol stub.

The build directory must lie under `E:/cargo-targets/zr_vm/build`. Archives and
executables go into its `lib` and `bin` directories. Compiler detection, CMake
products, response files, maps and manifests stay in that external tree. Root's
launcher must also direct compiler temporaries and captured evidence into the
permitted `E:/cargo-targets/zr_vm/{tmp,reports}` roots. A directory check inside
the driver does not configure the parent process's temporary-file environment.

No generated configuration is required for the current checkout. The actual
`zr_vm_library/third_party/zr_miniz/miniz/miniz_export.h` defines empty static
visibility unless `MINIZ_SHARED` is selected; it is the quote-included file.
The separate wrapper-level `miniz_export.h` does not replace it. The driver
requires the local header and writes neither an override nor a copy.

## Registered execution and results

| CTest | Argument | Count | Expected evidence |
| --- | --- | ---: | --- |
| `ssa_source_execbc_vm_comparisons` | `--comparisons-only` | 4 | The actual new LT/GT source checks |
| `ssa_source_execbc_vm_regressions` | `--regressions-only` | 13 | Original source-route regression cases |

Both carry `ssa;source-direct-validation` labels, timeout 120 and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No arguments selects all 17
cases in the same executable. Unknown arguments fail with usage status 2 before
Unity. Registered cases preserve their natural exit status; no expected-failure
wrapper converts RED into success.

V46 established the initial comparison RED: `source comparison producer did not
establish an executable source CFG`. All four cases reached this third CFG assertion after
`parsedAndCompiled` and ordinary `preSemanticIrValidated` checks pass. A compiler,
linker, parser, ordinary validator or sanitizer failure is a separate failure.
The four-case run returned natural status 4. This is bounded producer RED evidence,
not comparison success. A successful
comparison run needs four PASS records and Unity `4 Tests 0 Failures 0 Ignored`;
the regression run needs `13 Tests 0 Failures 0 Ignored` and natural exit 0.

The 13-regression V46 run returned natural status `0xC00000FD`. Root identified a 1,169,768-byte static Core dispatcher
frame exceeding the PE's default 1,048,576-byte stack reserve. Root added the
8,388,608-byte reserve; V48 freshly relinked sealed V46 artifacts and passed all
original 13 cases with natural exit 0. The PE commit is 4,096 bytes. This baseline
rerun did not compile the comparison producer changes being edited. Original failed receipts remain
failed; the four-case producer RED is independently usable evidence.

The statically audited runtime creates local core state, attaches native registry
metadata, obtains builtin/reflection descriptor pointers, runs the local Oracle
place-witness callback and VM trace callback, and uses Unity's ordinary fatal
assertion control flow. These local infrastructure paths are authorized. Selected
sources invoke no external providers, loader/plugin operations, FFI, network,
security/capability provider callbacks, async execution or hotpatch. Linking
generic support routines does not authorize running those other paths.

## Provenance and outstanding validation

Configuration emits these metadata files under the external build tree:

- `source-direct-target-sources.tsv`: actual target/source membership and SHA256.
- `source-direct-target-settings.tsv`: source counts, compiler/archive/link rules,
  include roots, definitions, options and selected libraries.
- `source-direct-header-hashes.tsv`: actual Unity config, test includes and the
  quote-included miniz export header hashes.
- `compile_commands.json`: generated actual compile commands.

The native link also supplies archive/member selection output and
`ssa-source-link.map`. Hashes represent configure-time bytes; Root must recheck
them before execution if concurrent authorized edits occurred.

Read-only authoring hashes on 2026-10-04:

These historical hashes precede Root's later test-main header-order edit. Use the
sealed run's manifests for actual V46 inputs and current manifests for later pools.

| Actual file | SHA256 |
| --- | --- |
| `tests/parser/test_ssa_source_execbc_vm.c` | `481DC593351AA7745EBC6762C861D01AFF13E8AB68F87208C02B3CF4546174DC` |
| `tests/harness/runtime_support.c` | `B7FD630388F490B3F26D2D90415A85561E777737A0D92CADD2EEAB61802ACA5E` |
| `tests/unity_config.h` | `45A7953BAA66DE43B9D2088E320FA136B0FC7DE034B08A82D9CCB205A0E4A484` |
| `zr_vm_library/third_party/zr_miniz/miniz/miniz_export.h` | `F0A8A0AEADAC411476BBB0F4DBF82F88B581FC738ADB275EC878FB90BD0E9BC0` |

Root's actual V42 run completed 664 of the historical 666 objects. Its `build.log` lines
871–879 then show the duplicate-quoted `llvm-lib.exe` command failing before
archive creation, linking or comparison execution. The corrected rule removes
manual placeholder quoting. Root reported terminal native status 1 with EMPTY
process state and no actions; the receipt SHA256 is
`a3021cdd0ae1d2172966993e7a26de91776cf3fed620688cab28c3a343f7f833`
(2,153,532 bytes). V42 did not establish a native link or producer RED.

V45 successfully created all nine archives and compiled `runtime_support.c`,
bringing the completed object count to 665. The main test failed with five
`__declspec(_Noreturn)` errors: Unity's indirect C11 `stdnoreturn` macro appeared
before the CRT/SDK standard-library declarations. Root moved actual stdio,
stdlib and string includes before Unity in the owned test main; no SDK/vendor
header was copied or replaced. V45's failed receipt SHA256 is
`cd38f991dbaaab120fb506da2cd1f186f85dad787456d13947220b5e56659c94`
(2,426,912 bytes).

V46 completed the historical 666-object pool and linked the nine archives into a
41 MB native PE. Its comparison run established the genuine third-CFG-assertion
RED with natural status 4; its regression run returned natural stack-overflow
status `0xC00000FD`. Reports for these attempts are under
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b`. These results do not change
the original failed receipt statuses.

V47's raw native unwind evidence measured 1,169,720 + 48 bytes, corroborating the
1,169,768-byte dispatcher frame. V48 reused the sealed V46 666-object pool and nine
libraries and freshly relinked into
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/source-stack-baseline-v48`.
The actual original 13 cases all PASS with natural exit 0, EMPTY process state,
closed handle and no actions. The whole owned run took 81.454 seconds.
Its TRUE receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/source-stack-baseline-v48/Root-receipt.json`,
599,239 bytes, SHA256
`23c1ac9f1b703453509be030fa6ba1e1f21de41ae14292c466d4232aa1213c40`.
V48 establishes the sealed historical baseline; it neither recompiles nor validates
the producer currently being edited. V46's aggregate failed receipt stays false.

Remaining concrete work is comparison producer repair and validation of later
added TUs, same-source C/LLVM consumers and normal production publication. Any future missing definitions must be resolved
with their actual current defining TUs and support, without stubs or production
configuration changes merely to obtain success.
Root's owned-run deadlines, natural primary exit and process cleanup evidence
remain part of validation. Bounded producer RED and the sealed original 13-case
baseline PASS are established; current producer GREEN, same-source C/LLVM coverage,
normal production publication and full 47-item SSA acceptance are not claimed.
