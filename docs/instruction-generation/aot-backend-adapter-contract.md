---
related_code:
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_parser/include/zr_vm_parser/aot_ir_lowering.h
  - zr_vm_parser/include/zr_vm_parser/aot_generic_policy.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_llvm.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_llvm.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
  - docs/plans/ssa/07-aot-backends/03-generics-lto-pgo.md
  - "user: 2026-09-14 implement the missing AOT adapter files with explicit unsupported/fallback behavior"
tests:
  - tests/parser/test_ssa_aot_backend_adapters.c
doc_type: module-detail
status: implemented-subset
---

# AOTIR backend adapter contract

## Scope and honesty boundary

The files in this slice live in the dormant `zr_vm_aot` archive.  They consume
the shared `SZrAotIrModule` and parser lowering facade; they do not revive the
retired quickened-bytecode emitter.  The C and LLVM entry points therefore
return scalar lowering facts with `descriptorOnly = true` and
`artifactAvailable = false`.  A caller that requires source text, LLVM IR, or
machine code uses the `_emit_ex(..., requireArtifact = ZR_TRUE, ...)` entry
point and receives `ZR_BACKEND_AOT_IR_ARTIFACT_UNAVAILABLE`.  No successful
result is labelled as an executable artifact.

`backend_aot_ir_adapter_validate` applies the core AOTIR schema, execution
contract, CFG/range, and relocation checks.  `backend_aot_ir_adapter_collect`
can fill a caller-owned lowering buffer; all records contain numeric IDs and
are pointer-free in content.  The C and LLVM wrappers call the existing
`ZrParser_AotIr_EmitC`/`ZrParser_AotIr_EmitLlvm` facade and normalize bridge
counts against the complete semantic-site denominator, avoiding compatibility
double counting in the older facade result.

## Coverage and fallback

`backend_aot_ir_coverage_from_facts` preserves one semantic site per AOTIR
instruction.  Native lowering, runtime-helper bridges, interpreter fallback,
and unsupported sites are separate counters; fusion or inlining cannot reduce
the denominator.  A zero denominator is reported as
`ZR_BACKEND_AOT_IR_COVERAGE_UNAVAILABLE`, never as 100 percent.  Per-mille
ratios use overflow-safe integer arithmetic and require counters to sum to the
denominator.

## Link profiles and roots

`backend_aot_link_profile_resolve` delegates profile/toolchain checks to the
existing parser generic-release policy.  Stale PGO fingerprints and unavailable
LTO/ThinLTO/PGO support are rejected with an explicit status.  The optional
fallback path changes only the effective mode to DEV and records
`fallbackToDev`; it never reports an unavailable optimization as enabled.
`backend_aot_link_profile_mark_roots` validates root kinds before delegating to
the shared root marker, preserving entry, export, reflection, native callback,
serialization, capability, and future-patch roots.

## Focused validation

The focused fixture is also registered as the `aot_backend_adapters` CTest
target.  It remains independent of the dormant native emitters, so a normal
build can validate the descriptor contract without enabling an AOT archive.
From the repository root, strict Windows GCC can be run directly as:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include \
  -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot \
  tests/parser/test_ssa_aot_backend_adapters.c \
  zr_vm_core/src/zr_vm_core/aot_ir.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_generic_policy.c \
  zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c \
  zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_c.c \
  zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_llvm.c \
  zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.c \
  zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.c \
  -o ssa_aot_backend_adapters_test
```

The same source list is used for WSL GCC/Clang and ASan/UBSan runs.  The
project CMake target links the parser/core archives and runs the same fixture;
the adapter itself remains pointer-free and does not require LLVM at build
time.
