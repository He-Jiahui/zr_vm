---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_alias.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
tests:
  - tests/parser/test_ssa_gvn_range.c
  - tests/acceptance/ssa-alias-zero-generation.md
doc_type: acceptance-record
status: partial
---

# SSA 02.02: zero-generation alias evidence

## Scope and baseline

`ZrParser_ExecIr_AliasQuery` classifies two parser-side alias locations. Before
this change, it rejected a generation mismatch only when both generations
were nonzero. A missing generation (`0`) could therefore pass through stable
base and layout checks and produce `must-alias` or `disjoint`. The neighboring
range, shape, and nullability fact APIs reject generation `0`, and the 02.02
plan requires uncertain alias evidence to retain loads and guards.

The change makes a missing generation return `unknown`, as an unequal nonzero
pair already did. Matching nonzero generations retain the existing positive
classifications. It changes only the direct alias query and its focused test;
no optimizer instruction is deleted by this patch.

## Test inventory and RED evidence

`test_zero_generation_cannot_prove_alias_relation` covers three otherwise
positive classifications: the same stable allocation/projection, distinct
stable allocations, and distinct field projections with an explicit common
layout proof. For each pair it clears only the left generation, only the
right generation, or both, and requires `unknown` in all nine cases. Existing
tests in the same executable retain positive controls for matching nonzero
generations and conservative external/escaped locations.

After adding the tracked test but before changing `exec_ir_alias.c`, GCC 11.4
compiled the then-current alias source with a temporary D-drive driver using
the same left/right zero-mask inputs. The driver exited 1 and printed:

```text
must case mask 1: must-alias
must case mask 2: must-alias
must case mask 3: must-alias
disjoint case mask 1: disjoint
disjoint case mask 2: disjoint
disjoint case mask 3: disjoint
```

Mask 1 clears the left generation, mask 2 the right, and mask 3 both. The
driver used a distinct allocation for its disjoint case. The tracked test
adds the field-projection case. The full GCC CMake target could not provide
an additional pre-fix RED: Ninja's `VerifyGlobs.cmake` spent about 15 minutes
in uninterruptible filesystem I/O before compilation. That build was stopped;
the direct source-level RED above is the observed pre-fix failure.

## GREEN and toolchain evidence

The same temporary GCC driver recompiled against the fixed alias source,
exited 0, and printed `unknown` for all six cases above. The driver and its
binary were temporary files under `D:\tmp\zr_vm`.

All three Debug caches were configured before this slice under
`D:\tmp\zr_vm`: WSL GCC 11.4 (`ssa-gcc-debug`), WSL Clang 14
(`ssa-clang-debug`), and MSVC 19.44 (`ssa-msvc-debug`). For each cache,
`ninja -t commands zr_vm_ssa_gvn_range_test` supplied the exact generated
commands. Commands 1 and 14 recompiled `test_ssa_gvn_range.c` and
`exec_ir_alias.c`; command 17 linked `zr_vm_ssa_gvn_range_test`. MSVC ran
those commands after importing Visual Studio's x64 environment with
`Import-VsDevCmdEnvironment.ps1`. This bypassed the stalled automatic
regeneration step only; it did not reuse the old test or alias objects.

The Unix epoch modification times below confirm source, rebuilt object, and
linked executable order for every cache:

| Cache | Test source → object | Alias source → object | Test executable |
| --- | --- | --- | --- |
| GCC | `1790535129 → 1790536356` | `1790535826 → 1790536394` | `1790536727` |
| Clang | `1790535129 → 1790537061` | `1790535826 → 1790537094` | `1790537257` |
| MSVC | `1790535129 → 1790538020` | `1790535826 → 1790538023` | `1790538074` |

Focused registered tests were run without invoking Ninja:

```text
WSL: ctest --test-dir /mnt/d/tmp/zr_vm/ssa-gcc-debug -R ssa_gvn_range --output-on-failure --no-tests=error
WSL: ctest --test-dir /mnt/d/tmp/zr_vm/ssa-gcc-debug -R ssa_pass_manager_scalar --output-on-failure --no-tests=error
WSL: ctest --test-dir /mnt/d/tmp/zr_vm/ssa-clang-debug -R ssa_gvn_range --output-on-failure --no-tests=error
WSL: ctest --test-dir /mnt/d/tmp/zr_vm/ssa-clang-debug -R ssa_pass_manager_scalar --output-on-failure --no-tests=error
Windows: ctest --test-dir D:\tmp\zr_vm\ssa-msvc-debug -R ssa_gvn_range --output-on-failure --no-tests=error
Windows: ctest --test-dir D:\tmp\zr_vm\ssa-msvc-debug -R ssa_pass_manager_scalar --output-on-failure --no-tests=error
```

Each command found one registered test and passed; this is 2/2 for each of
GCC, Clang, and MSVC. The test process itself returned success in every case.
Windows Python ran `scripts/validate_wiki.py` and reported `wiki validation:
116 Markdown files, 115 manifest pages, 646 local links` followed by
`wiki validation passed`. An earlier WSL invocation of the same script was
terminated after its DrvFS read stalled; no result was claimed from that run.

## Acceptance decision and remaining boundary

The zero-generation direct-query correction is accepted by the RED→GREEN
source reproduction, nine-case tracked matrix, and three-toolchain focused
CTest. It is not the full 02.02 exit gate: the query receives no active
analysis generation, so two equal but stale nonzero generations still require
caller-side freshness checks. `exec_ir_sroa.c` and `exec_ir_data_layout.c`
contain independent observed-alias preconditions with the prior zero-tolerant
generation comparison; this four-file slice does not change or validate those
passes. Edge-sensitive range propagation, actual guard elimination, and
backend differential execution remain outside this acceptance decision.

The three Debug caches are retained for the adjacent aggregate SSA subtask
to reuse; that task owns their path-checked cleanup after its validation.
