---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_fusion.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_binding_facts.h
  - zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_fusion_patterns.def
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_internal.h
  - scripts/codegen/generate_execbc_patterns.py
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_fusion.h
  - zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_fusion_patterns.def
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_internal.h
  - scripts/codegen/generate_execbc_patterns.py
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
  - docs/plans/ssa/guides/E-projections-fusion-aot.md
  - "user: 2026-09-13 implement 03.04 generated fusion contract"
tests:
  - tests/parser/test_ssa_generated_fusion.c
  - tests/acceptance/ssa-generated-fusion.md
doc_type: testing-guide
status: implemented
---

# Generated fusion acceptance evidence

## Scope

This slice adds the parser-owned six-pattern ExecBC fusion contract.  It covers
fixed-width instruction words, bounded side tables, source/resume projection,
conservative matcher boundaries, deterministic generation, budget fallback, and
generation/contract invalidation.  It does not wire the legacy quickening or
core dispatch implementation.

## Baseline

Before this slice there was no generated fusion header, matcher contract, or
focused fixture.  Existing dispatch and quickening behavior is unchanged by
these files.  Repository-wide test failures are therefore outside this focused
acceptance target.

## Test inventory

- Six positive windows: load+arithmetic, compare+branch, index+load,
  binding+call, increment+loop branch, and call+return.
- Negative boundaries: debug poll, type mismatch, layout absence, unresolved
  binding, effect discontinuity, and over-budget side-table output.  A
  zero-based first static-binding row is accepted only with a validated facts
  witness; an un-witnessed zero remains unresolved.
- Encoding boundaries: fixed `sizeof(SZrInstruction)` assertion, a wide
  operand retained losslessly in the side entry, and a branch target PC above
  `UINT16_MAX` retained in the u32 side table.
- Observability: two source/resume records per fused pair and branch target PC
  remapping (both explicit conditional-branch successors are retained).
- Reproducibility and lifecycle: same-input hash/bytes, stale generation
  invalidation, contract mismatch, transactional output, and repeated cleanup.
- Empty-function projection and partial-lifecycle cleanup remain valid without
  dereferencing a missing sentinel map.
- Generator freshness: `generate_execbc_patterns.py --check`.

## Tooling evidence

Focused GCC C11 compile and run (WSL, GCC 11.4):

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes
    -Wmissing-prototypes -Wcast-qual -Werror ...
```

The executable (linked with `exec_ir_fusion.c`, `exec_ir_fusion_contract.c`,
`exec_ir_fusion_match.c`, `exec_ir_fusion_lifecycle.c`, and the existing
`exec_ir_binding_facts.c` validator for the zero-based-row witness) exited
with status 0.  The same fixture passed a strict Clang C11 compile/run.  Both GCC and
Clang AddressSanitizer/UndefinedBehaviorSanitizer builds, with leak detection
and halt-on-error, also exited with status 0.  GCC `-fanalyzer` and Clang
static analysis reported no diagnostics.  The generator freshness command
was:

```text
python scripts/codegen/generate_execbc_patterns.py \
  --input zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def \
  --output zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h \
  --check
```

## Results

All focused assertions passed.  Malformed/unsupported windows retain an
unfused fixed-width operation and an explicit fallback reason; no test observed
operand truncation or stale plan reuse.  The focused test links only the core
ExecIR model and fusion source, so it does not depend on shared CMake
registration.  MSVC `/std:c11 /W4 /WX` compilation of all four projection
units also passed with `/utf-8`.

## Acceptance decision

Accepted for the bounded contract stage.  Remaining integration work is to
register the target in `tests/cmake/ssa-tests.cmake`, connect the projection to
the existing ExecBC/quickening pipeline, and measure code-size/dispatch impact
under the 00.01 benchmark gate.  Those shared build/index changes are
intentionally left to the parent agent.
