---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
tests:
  - tests/parser/test_ssa_gvn_range.c
  - tests/parser/test_ssa_oracle_projections.c
doc_type: acceptance-record
status: partial
---

# SSA 02.02: implicit conversion target in GVN keys

## Scope and baseline

The direct ExecIR oracle uses the result Value's canonical type when a
`CONVERT` instruction leaves `typeToken` unset; the ExecBC projection also
resolves the implicit target type. Previously, same-block GVN compared only
the instruction fields and input operands, then replaced a distinct-target
conversion with `COPY` of the earlier result. This changes integer-to-float
conversion semantics and violates the canonical result's typed value contract.

## Focused test inventory

`test_gvn_preserves_conversion_result_type_when_instruction_type_is_implicit`
constructs verifier-valid ExecIR with one external integer input, an implicit
integer conversion, an implicit double conversion, and another implicit double
conversion. It checks that the different-target operation remains `CONVERT`,
the identical-target repeat becomes `COPY` of the double result, and the
function verifies before and after GVN. The test initially failed at the
different-target assertion after the pre-change GVN rewrite. The existing
pure-add and explicit TYPE_TEST regressions remain in the same executable.
`test_gvn_rejects_missing_value_storage` checks malformed Value metadata is
rejected with `ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT` before reading it,
both for a missing pointer and for a non-null buffer with count exceeding
capacity. A read-only independent review found no actionable issue after
the capacity case was added.

## Tooling evidence and decision (2026-09-27)

- WSL GCC Debug in `/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu`:
  `ninja zr_vm_ssa_gvn_range_test -j 4` built the previously absent standalone
  objects. Before the fix, `./bin/zr_vm_ssa_gvn_range_test` exited 1 at the
  different-target conversion assertion after the source fixture had passed
  full verification. After the fix, `ctest -R
  '^(ssa_gvn_range|ssa_pass_manager_scalar)$' --output-on-failure
  --no-tests=error` passed 2/2.
- WSL Clang Debug in `/home/hejiahui/zrvm-ssa-nested-clang.kBIWlA`:
  `ninja zr_vm_ssa_gvn_range_test -j 4` and the same CTest selector passed
  2/2.
- MSVC Debug in `build/codex-ssa-conversion-msvc`: built the focused test
  target with the Visual Studio developer environment. The first CTest run
  passed `ssa_gvn_range` but reported `ssa_pass_manager_scalar` Not Run
  because its executable had not been built in this cache. After building
  `zr_vm_ssa_pass_manager_scalar_test`, the same CTest selector passed 2/2.
- `python3 scripts/validate_wiki.py` passed (116 Markdown files, 115
  manifest pages, 646 local links). The test uses canonical Value types; no
  source-language or production backend differential is claimed by this
  focused pass test.

This checkpoint accepts only the same-block GVN conversion-key repair. Do not
promote it to the 02.02 exit gate: cross-block dominance, edge-sensitive
ranges, memory versions, and guarded-check deletion remain outside this
change. No ownership transfer, lease, cancellation, or persistent state is
introduced by the pure-value key comparison.
