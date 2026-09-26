# SSA 07.01: complete logical map in shared AOTIR

## Scope

The shared AOTIR schema now borrows the entire ExecIR logical checkpoint map,
including multi-phase entries and the live-value/root/owner side pools. Module
validation checks storage, identity and indexed records; module hashing uses
semantic contents, never host pointers or allocation capacity. This replaces
the old resume-ID/state-hash summaries and advances the owned projection to
shared descriptor handoff. It does not emit a C/LLVM artifact or restore a
physical frame.

## Baseline

The test was RED at GCC compilation: `SZrAotIrFunction` had no
`logicalStateMap`; the old record rejected repeated resume IDs across phases
and could not represent recovery pools. The previous full GCC SSA label was
79/80, with two pre-execution semantic registration failures in
`ssa_source_cleanup_cfg` outside this schema change.

## Test Inventory

`test_ssa_aotir_contract.c` covers two phases for one resume ID, hash changes
from owner/cleanup/deopt metadata and live/root pools, no-map validity, bad
generation, missing root, invalid owner enum, bad instruction, zero resume ID,
duplicate phase and out-of-range owner pool.
Existing `ssa_c_llvm_lowering` and `aot_backend_adapters` tests cover the
downstream descriptor-only consumers. The schema has no new allocations,
concurrency, execution, or interpreter fallback.

## Tooling Evidence

Environment: Windows host x64 with WSL Ubuntu GCC 11.4.0 and Clang 14.0.0;
Windows MSVC via `E:\Visual Studio\Common7\Tools\VsDevCmd.bat`. The direct
fixture sources are `tests/parser/test_ssa_aotir_contract.c`,
`zr_vm_core/src/zr_vm_core/aot_ir.c`, and
`zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c`, with
`zr_vm_common/include` and `zr_vm_core/include` include roots.

## Results

- RED: direct GCC compilation failed because `SZrAotIrFunction` lacked
  `logicalStateMap` before implementation.
- Direct WSL GCC 11.4 and Clang 14: each compiled the three fixture sources
  with `-std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes
  -Wmissing-prototypes -Werror` and ran its executable, all exit 0.
- WSL GCC `-fsanitize=address,undefined -fno-omit-frame-pointer` with the
  same strict flags: compilation and execution exit 0, no sanitizer report.
- Windows MSVC x64: `cl /nologo /utf-8 /W4 /WX /std:c11` for the same sources;
  executable exits 0. `/utf-8` avoids unrelated CP936 encoding warnings.
- GCC direct `ssa_c_llvm_lowering` and strict full adapter fixture compiled
  and executed with the new storage source, exit 0. The full adapter fixture
  was recompiled and run after the final contract change. Clang CMake targets
  `zr_vm_ssa_aotir_contract_test` and `zr_vm_ssa_c_llvm_lowering_test` built;
  `ctest --test-dir build/ssa-clang-debug -R
  '^(ssa_aotir_contract|ssa_c_llvm_lowering)$' --output-on-failure
  --no-tests=error` passed 2/2.
- GCC CMake targets `zr_vm_ssa_c_llvm_lowering_test`,
  `zr_vm_ssa_aot_backend_adapters_test`, `zr_vm_core_shared`, and
  `zr_vm_parser_shared` built in `build/ssa-gcc-debug` (contract fixture was
  built in the same directory earlier). Focused CTest regex
  `^(ssa_aotir_contract|ssa_c_llvm_lowering|aot_backend_adapters)$` passed
  3/3. `ctest --test-dir build/ssa-gcc-debug -L ssa
  --output-on-failure --no-tests=error -j 4` passed 79/80. The sole failing
  CTest is `ssa_source_cleanup_cfg`, with two pre-execution Semantic IR
  variable-registration errors in
  `conditional_invoke_and_throw_try_finally_cleanup.zr:4:5` and
  `repeated_conditional_invokes_and_object_throws_cleanup.zr:4:5`. The same
  two failures were present in the baseline before this schema change.

## Acceptance Decision

This schema slice is accepted with focused GCC 3/3, Clang 2/2, direct MSVC,
GCC/Clang and GCC ASan/UBSan checks passing; the existing 79/80 full-label
gap remains open outside this slice. Descriptor-only adapter and legacy
artifact writer remain separate paths; no C/LLVM execution parity is claimed.
