---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
  - docs/parser-and-semantics/ssa-source-module-contract.md
tests:
  - tests/parser/test_ssa_source_aot_scalar_text.c
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/parser/test_ssa_aot_scalar_text.c
  - tests/parser/test_ssa_aot_scalar_arithmetic.c
  - tests/parser/test_ssa_aot_scalar_conditional.c
doc_type: testing-guide
status: finite-source-scalar-text-green
---

# Actual Source Scalar Text Test Guide

## Test inventory

`test_ssa_source_aot_scalar_text.c` consumes the shared real literal SCRIPT
fixture. It uses actual metadata and owners through binding, compaction,
primitive frame/layout, canonical AOT lowering, host target and descriptor.
The emitter never constructs a replacement graph.

| Invocation | Inventory |
| --- | --- |
| `--prerequisites-only` | Two real 9/8 descriptor prerequisites. |
| `--features-only` | Four emitter feature cases: C/LLVM × 9/8. |
| No arguments | 22 source cases on the admitted current host. |
| Other arguments | Exit 2; not a passing test invocation. |
| Unsupported actual host | One explicit host refusal case; not the 22-case admitted profile. |

The 22 cases are two prerequisites, four actual source text features, two null
diagnostic successes, two exact-capacity boundaries, two required-output cases,
two extra-layout refusals, two different-type NOP refusals, two Core-invalid NOP cases,
two frame-type contradictions and two primitive-width contradictions.

Input observations cover the live compiler, source metadata and heaps, original
and compacted graphs, module pools, target, projection/state storage, descriptor
and its pools. These digests detect mutation; they are not ABI hash algorithms.
Refusal cases save, perturb, observe and restore the actual record under test.
Buffer cases verify writable first-byte/length reset and adjacent sentinels.
The whole buffer is parsed to confirm the actual function identity and 9/8
constant. No generated text is compiled or executed. The real source NOP retains
the actual return ABI type; the different-type refusal must use another nonzero
token accepted by Core, prove it differs, and restore the actual record.

## Direct build and finite execution

Use the existing Root driver with its expected source count explicitly set:

```powershell
pwsh -NoProfile -File E:/cargo-targets/zr_vm/tmp/source-scalar-text-direct-driver-v1.ps1 -Mode green -RunTag r3 -ExpectedSourceCases 22
```

Root's actual r3 run passed the finite gate. The driver default
count is 18; the explicit 22 is required for the current inventory.
It configures `tests/cmake/ssa-source-direct-validation` against actual checkout
sources, then builds these five targets:

- `zr_vm_ssa_source_aot_scalar_text_test`
- `zr_vm_ssa_source_aot_descriptor_test`
- `zr_vm_ssa_aot_scalar_text_test`
- `zr_vm_ssa_aot_scalar_arithmetic_test`
- `zr_vm_ssa_aot_scalar_conditional_test`

The selected CTest regex is:

```text
^(ssa_source_aot_scalar_text|ssa_source_aot_descriptor|ssa_aot_scalar_text|ssa_aot_scalar_arithmetic|ssa_aot_scalar_conditional)$
```

Build output is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`;
reports are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`;
drivers and temporary storage are under `E:/cargo-targets/zr_vm/tmp`.
No output or source copy to D: is part of this procedure. The existing driver
may read installed Windows SDK libraries on D:; those read-only toolchain inputs
are not output directories or copied source trees.

The build's four direct metadata files (`source-direct-target-sources.tsv`,
`source-direct-target-settings.tsv`, `source-direct-header-hashes.tsv`,
`compile_commands.json`) record actual checkout source membership and hashes.
The five shared backend sources must occur in the actual support target with
assertions enabled (`/UNDEBUG`) and UBSan (`fsanitize=undefined`). The source
suite contains the actual test plus shared literal fixture. Retain source,
binary, log and metadata hashes at the successful run; fail on drift.

## Interpreting results

The finite gate requires five CTest suites, source22 + descriptor20 = 42 Unity
cases, zero failures/ignored, and three prior standalone regressions run without
arguments. The standalone tests report their own checks rather than Unity
case totals. Do not count their checks as additional Unity cases.

Committed actual RED passed prerequisites and failed four feature cases.
The retained r1 and r2 implementation runs configured/built successfully, but
CTest exited 8 with 12/22 source failures in each. The cursor change alone did
not repair actual NOP admission. [Acceptance](../../docs/acceptance/ssa-source-aot-scalar-text.md)
pins those historical logs and the final successful r3 proof. A valid
empty NOP range has zero count and may have a nonzero Core-validated cursor;
do not patch producer ranges to make an overstrict consumer pass.

MSVC compile-only reproduction uses the existing driver:

```powershell
pwsh -NoProfile -File E:/cargo-targets/zr_vm/tmp/source-scalar-text-msvc-smoke-r1.ps1
```

It compiles only the shared scalar text C translation unit using `/TC /std:c11
/experimental:c11atomics /W4 /c`. The current driver labels outputs r3 and may overwrite
those names on reuse; preserve historical receipts and identify actual source
pins for each revision. Root's r3 compile passed with zero warnings. Compile-only
success is not generated-code or full Windows test evidence.

## Boundaries

No source execution, oracle, native loader/registration, generated executable,
network/FFI/provider/security probe is authorized in this finite procedure.
The projection remains non-runnable. Linux/full MSVC/general47/native coverage
and owner retention after release remain OPEN. The independent owner lifetime test
is authored; its Core implementation, registration and actual validation remain OPEN.
The full C/LLVM milestone's unchecked tasks remain unchanged.
