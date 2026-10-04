# Actual-checkout SSA source validation

This independent CMake project compiles current files directly from the checkout
into ordinary static archives. It selects the source → SemIR → ExecIR → ExecBC
VM fixture. It does not stage source or headers, include the repository's full
CMake graph, or modify the existing nine-target `ssa-direct-validation` project.

## Native toolchain contract

Use CMake 3.31 or newer, Ninja, Debug, and Root's verified Windows clang-cl/LLD
toolchain. That toolchain supplies actual compiler/resource/MSVC/SDK locations,
the actual `llvm-lib.exe` as `CMAKE_AR`, and an executable link rule with UBSan
driver options **before `/link`**. Keep real compiler identification and the
executable ABI probe enabled. Do not use forced compiler IDs, compiler-WORKS
overrides, static-library probes, MSVC initialization scripts or vendor bootstrap.

The source root is derived from this directory. The build directory must be under
`E:/cargo-targets/zr_vm/build`; binaries, archives, CMake products, compile command
metadata and manifests are written there. The launcher must also put temporary
files and captured logs under the permitted `E:/cargo-targets/zr_vm/{tmp,reports}`
roots. Toolchain preparation and execution are owned by Root.

The archive rule is `<CMAKE_AR> /nologo /out:<TARGET> <OBJECTS>`. CMake quotes the
expanded tool path itself; manual quotes around `<CMAKE_AR>` produce duplicate
quotes for the LLVM installation path containing spaces.

Use the following command shape with Root's actual toolchain file and an owned
output tree. Root's recorded native attempts use their own sealed output trees:

```powershell
cmake -S E:/Git/zr_vm/tests/cmake/ssa-source-direct-validation -B E:/cargo-targets/zr_vm/build/ssa-source-direct -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=<actual-toolchain-file>
cmake --build E:/cargo-targets/zr_vm/build/ssa-source-direct --target zr_vm_ssa_source_execbc_vm_test
ctest --test-dir E:/cargo-targets/zr_vm/build/ssa-source-direct --output-on-failure -R '^ssa_source_execbc_vm_(comparisons|regressions)$'
```

## Selected pool and test results

At authoring, parser/core/library/common contain 397/218/40/1 C files. The common
platform contract is compiled once. Actual xxHash/utf8proc/cJSON/miniz contribute
1/1/1/4 C files; actual Unity contributes 1; the test and `runtime_support.c`
contribute 2. The initial compile pool is 666 TUs. Tinydir supplies its actual
header implementation and has no selected placeholder C object. This is a module
closure, not a proven minimum TU closure. All selected C inputs have assertions
enabled and, by default, UBSan instrumentation.

The 666-TU pool is the historical V42–V46 pool. Three additional parser TUs will
raise the parser pool to 400 and the newly configured total to 669. Configure-time
manifests identify that new pool; sealed V46 artifacts remain evidence for the
historical 666-TU pool.

Normal archive member extraction determines the linked support. No whole-archive
or missing-symbol stubs are used. The 143 enabled AOT backend TUs are initially
omitted because the selected fixture exercises no C/LLVM backend and the audited
parser/core/library pools do not reference their external writer exports. A real
linker diagnostic must identify any additional actual TU needed. External FFI,
network and provider modules are outside this project.

| CTest | Fixture argument | Cases | Result contract |
| --- | --- | ---: | --- |
| `ssa_source_execbc_vm_comparisons` | `--comparisons-only` | 4 | Genuine comparison success or failure |
| `ssa_source_execbc_vm_regressions` | `--regressions-only` | 13 | Preserve original regression checks |

Both have labels `ssa;source-direct-validation` and timeout 120 seconds. Invoking
the executable without arguments selects all 17 cases. No argument runs are not
registered as a third duplicate CTest. Unknown arguments return 2 before Unity.

V46 established the initial comparison RED: `source comparison producer did not
establish an executable source CFG`, reached **after** ordinary parse/compile and
pre-SemanticIR validation. All four cases failed at this third CFG assertion with
natural exit 4. Earlier failures do not establish that producer RED.
No `WILL_FAIL`, failure inversion, retry wrapper or forced zero exit is present.
The 13-regression V46 run exited naturally with `0xC00000FD` (stack overflow).
Root's V48 fresh relink of the sealed V46 artifacts with `/STACK:8388608` then
passed all original 13 cases with natural exit 0. This validates the sealed
historical baseline; it did not compile the producer changes being edited.

## Provenance and current limitations

Configuration writes `source-direct-target-sources.tsv` (target, actual absolute
source path, SHA256), `source-direct-target-settings.tsv` (counts, includes,
definitions, flags, link libraries and tool rules), and
`source-direct-header-hashes.tsv` (selected actual configuration/include files).
The build also emits `compile_commands.json`, a real LLD map and archive selection
messages. These are metadata, not copies of implementation files. CMake globbing
discovers the current pool, including the new SCCP helper, and global duplicate
source checks prevent compiling common more than once.

The checked-out `zr_miniz/miniz/miniz_export.h` is required and read in place. Its
quote-include directory wins; this project neither generates an override nor
copies/modifies either local miniz export header. A missing header stops
configuration for provenance review.

No configure, compiler, linker, runtime, WSL or Git action was performed by the
driver author. Root's actual native history is:

| Attempt | Actual result |
| --- | --- |
| V42 | 664/666 objects completed; duplicate archive-tool quotes stopped archive creation. |
| V45 | Nine archives created and `runtime_support.c` compiled (665 objects); main failed with five SDK `__declspec(_Noreturn)` errors caused by Unity's C11 `noreturn` macro preceding CRT headers. |
| V46 | After Root moved actual stdio/stdlib/string includes before Unity, 666 objects and nine archives linked into a 41 MB PE; four comparisons established genuine producer RED, while regressions ended in stack overflow. |
| V47 | Native unwind evidence identified 1,169,720 + 48 bytes for the static dispatcher frame. |
| V48 | Sealed V46 inputs freshly relinked with 8,388,608-byte stack reserve and 4,096-byte commit; original 13 cases all PASS, natural exit 0, EMPTY process state, closed handle and no actions. |

Root's static Core dispatcher frame is 1,169,768 bytes, exceeding the PE's default
1,048,576-byte stack reserve. V48's larger reserve resolved the observed native
test launch constraint for the original 13-case baseline.
Reports are under `E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b`. Original
failed receipts remain failed; the bounded four-case RED can be accepted
independently. V48's `source-stack-baseline-v48/Root-receipt.json` is TRUE,
599,239 bytes, SHA256
`23c1ac9f1b703453509be030fa6ba1e1f21de41ae14292c466d4232aa1213c40`;
the whole owned run took 81.454 seconds. Its fresh output tree is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/source-stack-baseline-v48`.
Same-source four-consumer C/LLVM coverage, normal production publication and the full 47-item SSA
objective remain unestablished. See [the detailed guide](../../../docs/testing-and-validation/ssa-source-direct-validation.md).
