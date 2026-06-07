# AOT Ownership Direct Core Lowering

Date: 2026-06-06

## Scope

- Generated AOT C lowering for `OWN_UNIQUE`, `OWN_BORROW`, `OWN_LOAN`, `OWN_SHARE`, `OWN_WEAK`, `OWN_DETACH`, `OWN_UPGRADE`, and `OWN_RELEASE`.
- Affected layers: AOT C value lowering, AOT C function-body routing contracts, generated-C compile smoke tests, and language-pipeline test registration.

## Baseline

- Before this slice, generated C emitted `ZrLibrary_AotRuntime_Own*` instruction-helper calls for ownership opcodes.
- The helper bodies were thin wrappers around frame-slot resolution and core ownership APIs.
- Existing repository-level warnings remain outside this slice, including MSVC warnings in core/parser/library builds.

## Test Inventory

- `test_aot_c_source_lowers_ownership_to_direct_core_calls` verifies direct ownership-core generated-C text, all eight opcode routes, and absence of old `ZrLibrary_AotRuntime_Own*` helper strings in the C backend value/function-body path.
- `test_aot_c_generated_shared_library_compiles_direct_ownership_lowering` hand-builds ownership opcodes, verifies generated-C markers/direct core calls/helper absence, and compiles the generated C into a Unix shared library.
- Boundary cases covered: all supported ownership opcode variants, invalid generated-frame slot checks, null slot-value checks, failed transfer reset for copy-like ownership operations, and release resetting the destination slot.
- Negative cases covered: source contracts reject the old ownership helper-wrapper emitter and helper-name strings.

## Tooling Evidence

- WSL GCC build directory: `build-wsl-gcc`
- WSL Clang build directory: `build-wsl-clang`
- Windows MSVC build directory: `build-msvc`
- RED command:
  - `wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc >/tmp/zr_vm_cmake_ownership_red.log && cmake --build build-wsl-gcc --target zr_vm_aot_c_ownership_contracts_test -j 3 && ./build-wsl-gcc/bin/zr_vm_aot_c_ownership_contracts_test'`
- RED result:
  - `zr_vm_aot_c_ownership_contracts_test`: 1 test, 1 failure, missing `backend_aot_write_c_direct_ownership_core_call(`.
- GCC focused validation built and ran AOT C source contracts and generated-C shared-library smokes for source, logical, global, constant, ownership, shift, float, generic numeric, and power subsets.
- Clang ran the same focused source-contract and generated-C smoke target set, split into smaller chunks to avoid command timeouts.
- MSVC imported the Visual Studio command environment, configured `build-msvc`, built the AOT C contract targets and `zr_vm_aot_c_ownership_shared_library_smoke_test`, then ran those executables.
- Source scan checked `backend_aot_c_lowering_values.c`, `backend_aot_c_function_body.c`, `backend_aot_c_emitter.c`, and `backend_aot_c_emitter.h` for ownership helper emissions and new direct-core markers.
- Formatting command:
  - `git diff --check -- tests/parser/test_aot_c_ownership_contracts.c tests/parser/test_aot_c_ownership_shared_library_smoke.c tests/CMakeLists.txt zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`

## Results

- GCC source-contract set: 37 tests, 0 failures.
- GCC generated-C smoke set: 21 tests, 0 failures.
- Clang source-contract set: 37 tests, 0 failures.
- Clang generated-C smoke set: 21 tests, 0 failures.
- MSVC source-contract set: 37 tests, 0 failures.
- MSVC ownership shared-library smoke: 1 test, 0 failures, 1 ignored because generated shared-library compilation is Unix-only.
- Source scan found only the new `backend_aot_write_c_direct_ownership_core_call`, `zr_aot_value_exec_ownership_core`, and `zr_aot_value_exec_ownership_release` markers in the checked C backend path. No old `ZrLibrary_AotRuntime_Own*` emission remains there.
- `git diff --check` reported only LF-to-CRLF normalization warnings for touched tracked files and no whitespace errors.

## Acceptance Decision

Accepted for the focused AOT C ownership lowering slice.

Generated AOT C now resolves ownership opcode operands from the generated frame and calls `ZrCore_Ownership_*Value` / `ZrCore_Ownership_ReleaseValue` directly instead of routing through AOT runtime instruction helpers. Remaining risks are intentional: LLVM parity, real executable ownership source fixtures, and broader non-POD ownership/copy/drop semantics remain future milestone work.
