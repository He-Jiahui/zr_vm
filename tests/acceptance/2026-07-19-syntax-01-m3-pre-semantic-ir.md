# Syntax Plan 01 M3 Pre-Execution Semantic IR And Facts Acceptance

## Status

- State: completed; implementation, staged-snapshot matrix, and final Critical/Important review passed.
- Plan: `docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md`, M3 pre-execution Semantic IR and facts.
- Scope: canonical semantic instructions, owned CFG/Place/Value/region/cleanup/source-map data, compiler bridge, structural validation, and separated flow joins.

## Acceptance Inventory

- Semantic function ownership: symbol/callable IDs, locals, Place graph, CFG, values, instructions, operands, regions, cleanup scopes, source map, and loan facts.
- Stable IDs: instructions refer to `TypeId`, `PlaceId`, `ValueId`, `LoanId`, region, cleanup scope, and block IDs rather than VM stack slots.
- Opcode families: constant/conversion; Place operations; load/store/init/move/copy/drop; borrow/reborrow/deref; typed/virtual/dynamic/meta call; control flow; scope/cleanup; distinct construction families; properties; evaluate-once destructuring and leaf operations.
- Compiler lowering: regular local initialization, identifier reads, local writes, compound reads/writes, and ownership builtins emit front-end Semantic IR first; load/store and exact ownership ExecBC opcodes are then selected from the emitted semantic opcode and explicit ownership operation.
- Validation gate: the pre-execution function is structurally validated before final function assembly and execution SemIR compatibility projection.
- Sidecar boundary: `compiler_semir.c` is explicitly an after-assembly execution compatibility projection and is absent during the source-level Semantic IR assertion.
- Flow dimensions: initialization, availability, shared/mutable borrowing, escape, and reachability remain separate.
- Join rules: unreachable predecessor filtering, conservative initialization/availability joins, loan union, and widest escape bound.
- Diagnostics: uninitialized/maybe-uninitialized, moved/maybe-moved/dropped, loan conflict, and escape violation carry causal IDs and source ranges.

## TDD Evidence

- The initial target failed because `zr_vm_parser/semantic_ir.h` did not exist.
- The first implementation established the opcode golden and CFG-flow negatives.
- The compiler bridge test then fixed the exact source sequence `place.base, initialize, load, place.base, initialize, load, store`.
- The same test proves pre-execution validation succeeds while `SZrFunction.semIrInstructions` is still null and an end-to-end compile remains successful.
- Review-driven regressions prove store restores a moved Place, mutable loan blocks ordinary reads until its exact end, and ending one arm of a multiple-mutable-loan join remains conservative.
- Final review-driven red/green tests prove `%unique` records an explicit unique ownership operation, both `%borrow(owner)` and `owner.borrow()` record shared loans, and unsupported ownership kinds cannot fall through to `OwnConstruct`.
- Ownership mapping is protected by the existing runtime lifecycle and AOT ownership contract suites.

## MSVC Evidence

- `zr_vm_pre_semantic_ir_test`: 6 tests, 0 failures.
- Place/CFG, union exhaustiveness, reachability, finally, try/catch, typed catch, loop flow, dataflow, parser, type inference, and semantic facts targets: all passed.
- `zr_vm_compiler_integration_test`: 127 tests, 0 failures.
- `zr_vm_aot_c_value_semir_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_ownership_contracts_test`: 1 test, 0 failures.
- Toolchain: MSVC 19.44.35228 x64 Debug, `build-syntax-01-m1-msvc`.

## Staged-Snapshot Evidence

- GCC 11.4 Debug: all 15 targets passed in `/home/hejiahui/zr_vm-syntax-m3-staged-gcc-20260719-r5`.
- Clang 14.0 Debug: all 15 targets passed in `/home/hejiahui/zr_vm-syntax-m3-staged-clang-20260719-r5`.
- Both snapshots passed 6 pre-SemIR tests, 127 compiler integration tests, 4 value-SemIR AOT contracts, 1 ownership AOT contract, and all Place/CFG/dataflow/parser/type-inference/semantic-facts targets.
- `GCC_R5_INDEX_MATCH` proves every staged M3 path in the source snapshot is byte-identical to the Git index.
- Snapshot assembly explicitly overlays the existing dirty baseline `zr_vm_core/include/zr_vm_core/profile.h` and `zr_vm_core/src/zr_vm_core/profile.c`, because committed `value.h` already references `ZR_PROFILE_HELPER_VALUE_CONSTRUCT`; these files are not part of the M3 staged diff.

## Review And Boundaries

- New implementation files remain below the 1000-line module threshold; flow implementation is isolated from compiler bridge and formatting.
- Existing large compiler files receive orchestration hooks only; semantic state and lowering decisions live in `compiler_semantic_ir.c`.
- M3 does not serialize local Place/CFG/loan facts, change artifact schema, or migrate VM/AOT/LSP consumers; those remain M4 and M5.
- Existing execution SemIR remains available for AOT/deopt compatibility but is no longer the front-end semantic source.
- Final self-review result: GO, 0 Critical and 0 Important findings remaining. Review found and fixed mutable-loan semantics, store restoration, conservative multiple-loan end, SemIR-driven ExecBC selection, ownership fallback, and point-form ownership lowering before this gate was closed.

## Final Gate

- Focused and MSVC compatibility tests: passed.
- GCC/Clang r5 staged-snapshot matrix: passed.
- Final Critical/Important review: GO (0 Critical, 0 Important).
- `git diff --cached --check`: passed.
