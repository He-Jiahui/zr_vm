# 2026-07-30 AOT 12-S1B / 12-S2F / 12-S6A Function Reachability Manifest

## Scope

This sub-milestone completes the function-node reason-schema boundary and deterministic retained-function report for
`docs/plans/aot/12-code-stripping.md`:

- S1B validates the function graph's root and dependency-edge reason classes before marking.
- S2F preserves a finite predecessor chain from every retained function to an explicit entry, export, manifest,
  reflection-annotation, or property-accessor root.
- S6A publishes stable versioned function-manifest rows in ascending flat-index order before table filtering.

The owning layers are the AOT parser reachability engine, the AOT C emitter, parser subsystem tests, and plan/module
documentation. This does not close the full S1, S2, or S6 stages. Type/layout, generic dictionary, native callback,
module initializer, metadata/debug sidecar, resource Drop, byte-size comparison, and CLI dump/diff convergence remain
open.

## Baseline

The deterministic function manifest implementation and its initial tests were already present in commit `05a96a8`.
However, `backend_aot_reachability_compute()` accepted malformed graph inputs: a dependency reason could be supplied as
a root, a root reason could be supplied on an edge, and unknown enum values could enter the mark array. The manifest
writer rejected those marks later, but the graph computation boundary did not fail closed.

RED was captured on Windows MSVC by rebuilding `zr_vm_aot_reachability_test` after adding the reason-schema negative
matrix. The executable reported 17 tests with 1 failure at
`test_reachability_rejects_invalid_reason_schema`: `DIRECT_CALL` used as a root returned true.

Validation started from `fe684d1`; while it ran, shared `main` advanced to `5af9a0c`. The final matrix therefore used
an effective `5af9a0c` snapshot: the original archive, the exact 25-file `fe684d1..5af9a0c` committed delta, and only
the two owned code/test overlays. Blob checks for changed core/parser files and SHA-256 checks for both overlays matched.

## Test Inventory

- `tests/parser/test_aot_reachability.c`
  - valid root and direct/field dependency propagation
  - disconnected nodes and first-root reason preservation
  - out-of-range roots/edges and undersized queues
  - edge reason used as root, root reason used as edge, and unknown reason values in both positions
  - stable manifest order and reason names
  - pending state, dirty unmarked state, out-of-range predecessor, edge-as-root, and predecessor cycle rejection
  - entry, export, manifest, reflection annotation, property accessor, and static callable graph coverage
- `tests/parser/test_aot_c_code_stripping.c`
  - direct-call predecessor publication
  - explicit export and manifest roots
  - omission of trimmed function nodes
  - existing metadata and MethodDef pruning integration cases

The focused reachability executable contains 17 tests. The focused C code-stripping executable contains 10 tests.

## Tooling Evidence

Tool versions:

- WSL GCC 11.4.0, CMake 3.22.1, Ninja 1.10.1
- WSL Clang 14.0.0, CMake 3.22.1, Ninja 1.10.1
- Windows MSVC 19.44 (`14.44.35207` toolset), Visual Studio 17 2022 generator

Effective source roots:

- WSL: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`

Final focused commands:

```text
cmake --build <wsl-source>/build-gcc --target zr_vm_aot_reachability_test zr_vm_aot_c_code_stripping_test -j4
ctest --test-dir <wsl-source>/build-gcc -R '^(aot_reachability|aot_c_code_stripping)$' --output-on-failure
<wsl-source>/build-gcc/bin/zr_vm_aot_reachability_test
<wsl-source>/build-gcc/bin/zr_vm_aot_c_code_stripping_test

cmake --build <wsl-source>/build-clang --target zr_vm_aot_reachability_test zr_vm_aot_c_code_stripping_test -j4
ctest --test-dir <wsl-source>/build-clang -R '^(aot_reachability|aot_c_code_stripping)$' --output-on-failure
<wsl-source>/build-clang/bin/zr_vm_aot_reachability_test
<wsl-source>/build-clang/bin/zr_vm_aot_c_code_stripping_test

cmake -S <windows-source> -B <windows-source>/build-msvc-latest -G "Visual Studio 17 2022" -A x64 ...
cmake --build <windows-source>/build-msvc-latest --config Debug --target zr_vm_aot_reachability_test zr_vm_aot_c_code_stripping_test --parallel 8
ctest --test-dir <windows-source>/build-msvc-latest -C Debug -R '^(aot_reachability|aot_c_code_stripping)$' --output-on-failure
```

The GCC and Clang latest-HEAD rebuild logs each recorded 543 compile/link actions and explicitly rebuilt
`backend_aot_reachability.c` after the delta timestamps were refreshed.

## Results

- WSL GCC: focused CTest 2/2 passed; direct runs were 17/0 and 10/0.
- WSL Clang: focused CTest 2/2 passed; direct runs were 17/0 and 10/0.
- Windows MSVC clean build: focused CTest 2/2 passed; direct runs were 17/0 and 10/0.
- The new reason-schema negative matrix passes on all three compilers.
- Existing direct-call, export-root, manifest-root, metadata, and MethodDef stripping cases remain green.

An MSVC build directory first created against `fe684d1` failed after the `5af9a0c` ABI delta with 8 reachability and
10 code-stripping failures. The new reason-schema test itself passed; failures clustered around function export and
reflection metadata, consistent with stale objects after fields were removed from shared function/compiler structs.
A clean `build-msvc-latest` rebuilt the same effective source and passed the complete focused matrix. The stale
incremental directory is excluded from acceptance evidence but retained as the recorded failure signal.

## Acceptance Decision

Accepted as AOT 12-S1B / 12-S2F / 12-S6A. Function reachability now rejects malformed reason classes before marking,
and every emitted retained-function row has a deterministic reason and finite predecessor chain to an explicit root.
Full AOT 12 remains open for the non-function node families, closed-world dynamic policy, behavior/size comparison,
artifact publication parity, and stable CLI dump/diff reporting.
