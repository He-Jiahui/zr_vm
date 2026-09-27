---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c
plan_sources:
  - docs/plans/ssa/05-data-layout/04-aggregate-soa.md
  - docs/instruction-generation/aggregate-layout-transform.md
tests:
  - tests/parser/test_ssa_aggregate_soa.c
doc_type: acceptance-record
status: accepted-focused
---

# SSA 05.04: observed alias generation in aggregate layouts

## Scope

The parser's aggregate witness is a value-only plan for SROA and AoS/SoA.
This checkpoint requires an alias-observed field's location generation to
equal the nonzero aggregate facts generation. It also rejects zero or unequal
generations before an observed alias pair can justify splitting storage.
Incomplete but structurally valid evidence must retain its diagnostic and use
ordinary AoS or GENERIC fallback. No runtime storage, allocation, ownership,
lease, or release path is changed.

## Baseline and root cause

`AggregateFactsValidateStructural` correctly allows incomplete semantic
evidence to be represented. `AggregateFactsValidate`, the SROA gate, and the
SoA gates previously used `location.generation != 0 &&
location.generation != facts.generation`. The pair gates similarly required
both generations to be nonzero before rejecting a mismatch. Thus a zero
location generation could be accepted as a positive alias proof although the
facts generation is required to be nonzero. Known nonzero stale generations
were already rejected. Alias-unobserved fields intentionally carry no alias
location proof and remain eligible under their other safety gates.

## Test inventory

`test_observed_alias_requires_current_nonzero_generation` in
`tests/parser/test_ssa_aggregate_soa.c` uses a valid two-field, disjoint
projection witness as a control. It then checks a zero generation on the first
field, the second field, and both fields, plus a nonzero stale generation.
Each case checks structural validity, semantic rejection with
`ALIAS_UNPROVEN`, the offending field index, source/IR coordinates, SROA and
SoA refusal, and valid AoS/GENERIC fallback with the same reason. The existing
private-pair and valid alias-pair cases check that absent alias locations on
unobserved fields and current stamped locations on observed fields still pass.
`test_observed_alias_generation_invalidates_optimized_plans` builds valid
SROA, SoA, and high-level SoA plans first, then clears the second field's
generation. All three consumer boundaries must reject those plans with
`ALIAS_UNPROVEN` at field index 1.

Adjacent regressions for the broader SSA matrix are `ssa_deopt_aggregates`,
`ssa_objects_layout_maps`, and `ssa_arrays_slices`. They cover neighboring
materialization, layout map, and data layout contracts. Their executables were
not built in the dedicated light caches, so they were not counted as passing
checks for this focused repair. The focused fixture does not exercise source
lowering or a runtime physical rewrite; those remain outside this value-only
planning API.

## Tooling evidence

WSL GCC is `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`. The direct fixture
compile used exactly these four source files and wrote its executable only to
`/mnt/d/tmp/zr_vm/ssa-aggregate-generation-gcc-red`:

```bash
cd /mnt/e/Git/zr_vm
gcc -std=c11 -g -O0 -I zr_vm_parser/include -I zr_vm_core/include -I zr_vm_common/include tests/parser/test_ssa_aggregate_soa.c zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_materialization_map.c -o /mnt/d/tmp/zr_vm/ssa-aggregate-generation-gcc-red/test_ssa_aggregate_soa
cd /mnt/d/tmp/zr_vm/ssa-aggregate-generation-gcc-red
./test_ssa_aggregate_soa
```

Before the implementation edit, the executable exited 1 at
`test_ssa_aggregate_soa.c:315`: assertion
`!ZrParser_ExecIr_AggregateFactsValidate(facts, &diagnostic)` failed for a
first-field zero generation. Repeating the same compile and execution after
the five generation checks were tightened exited 0 with no output. This is a
direct RED/GREEN result from the real four-source fixture, not a CTest result.

The dedicated CMake caches are on `D:\tmp\zr_vm`: `ssa-gcc-debug`,
`ssa-clang-debug`, and `ssa-msvc-debug`. For each cache,
`ninja -t commands zr_vm_ssa_aggregate_soa_test` supplied the exact four
compile commands and one link command. The object files for the changed test,
SROA, and data-layout sources were checked newer than their source files;
the linked executable was checked newer than the fixture object. The MSVC
check used `D:\tmp\zr_vm\ssa-aggregate-generation-msvc.ps1` and ran all five
generated commands with an explicit `$LASTEXITCODE` check. `cl` emitted D9025
because `/W4` overrides `/W3`; the commands returned zero. The initial
PowerShell script attempt stopped on that native warning before its exit-code
check, so its error handling was corrected and the commands rerun.

```text
WSL GCC:   ctest --test-dir /mnt/d/tmp/zr_vm/ssa-gcc-debug -R ^ssa_aggregate_soa$ --output-on-failure
WSL Clang: ctest --test-dir /mnt/d/tmp/zr_vm/ssa-clang-debug -R ^ssa_aggregate_soa$ --output-on-failure
MSVC:      ctest --test-dir D:\tmp\zr_vm\ssa-msvc-debug -R ^ssa_aggregate_soa$ --output-on-failure
Wiki:      python scripts/validate_wiki.py --root .
```

WSL GCC 11.4.0 and Clang 14.0.0 each reported 1/1 `ssa_aggregate_soa`
passed with CTest 3.22.1. MSVC 19.44.35228.0 reported 1/1 passed with
CTest 3.23.0-rc2. The Wiki source validator reported 116 Markdown files,
115 manifest pages, 646 local links, and `wiki validation passed`.

## Results and acceptance decision

The direct GCC RED/GREEN fixture and the focused CMake CTest target passed on
GCC, Clang, and MSVC. Zero, stale, and valid current generations are covered
at validation and consumer boundaries, including diagnostic and safe-fallback
behavior. The alias-generation repair is accepted for this value-only planning
API.

The adjacent three CTests were attempted in the GCC and Clang light caches and
reported `Not Run` because their executables were absent. They are not evidence
for this acceptance decision and remain for the broader SSA matrix. No source
lowering, runtime physical rewrite, aggregate layout milestone exit gate, or
measured SoA performance claim is made by this repair.
