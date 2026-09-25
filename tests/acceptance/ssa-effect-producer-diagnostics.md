---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_internal.h
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
tests:
  - tests/parser/test_ssa_cfg_effects_builder.c
doc_type: testing-guide
status: focused-passed
---

# SSA effect-producer diagnostic positions

## Scope

The shared parser effect diagnostic identifies the owning block and source
for a known instruction ID.  It checks pointer and capacity bounds before
reading any instruction or block.  For damaged storage, a missing source
remains zero rather than causing an out-of-bounds access.  The producer
publishes no partial token facts after a malformed opcode.

The CFG fixture has one entry block and one successor with an unknown opcode
at instruction 2, source 951.  Before the fix, the diagnostic reported
block 1 and source 0; it now must report block 2 and source 951.  Adjacent
range and missing-predecessor tests still exercise fail-closed storage paths.

The existing CFG fixture remains a single-purpose producer test under 1000
lines after this small assertion; splitting it would duplicate its setup.

## Reproduce

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_cfg_effects_builder_test zr_vm_ssa_builder_cfg_test zr_vm_ssa_source_cleanup_cfg_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_(cfg_effects_builder|builder_cfg|source_cleanup_cfg)$' --output-on-failure --no-tests=error
```

## Toolchain results (2026-09-26)

- WSL GCC 11.4 and Clang 14: each rebuilt `ssa_cfg_effects_builder`,
  `ssa_builder_cfg`, and `ssa_source_cleanup_cfg`; focused CTest 3/3
  passed on each compiler.
- Windows MSVC 19.44: rebuilt those three targets; focused CTest 3/3 passed.
  The existing `/W3` versus `/W4` override warning was emitted.
- WSL GCC ASan/UBSan: rebuilt `ssa_cfg_effects_builder`; focused CTest 1/1
  passed with no sanitizer failure.
- GCC SSA-labelled CTest passed 80/80.  The three directly affected targets
  were rebuilt in this phase; other SSA binaries were not refreshed.

Accepted for guarded parser effect-producer diagnostic locations only.
Malformed input still fails without publishing token facts; neither
irreducible effect synthesis nor the full M1 gate is claimed.
