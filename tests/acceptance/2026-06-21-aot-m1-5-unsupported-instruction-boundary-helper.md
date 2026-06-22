# AOT M1.5 07-S5 Unsupported Instruction Boundary Helper

Timestamp: 2026-06-21 16:08:08 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling and dynamic/deopt edges to stay behind fixed
boundary templates instead of expanding interpreter state in generated C. This slice
routes the current unsupported-instruction fallback through the shared runtime
reporter and the generated function cleanup exit.

## Scope

- AOT C unsupported instruction lowering for explicit unsupported opcodes and dispatch
  fallthrough.
- Source-contract and generated shared-library smoke coverage under `tests/parser`.
- Compatibility with existing `ZrLibrary_AotRuntime_ReportUnsupportedInstruction()`;
  this slice does not add a new runtime symbol.

## Baseline

- `backend_aot_write_c_unsupported_instruction_expr()` expanded instruction/opcode
  locals and a hand-written `ZrCore_Debug_RunError(state, "unsupported AOT instruction")`
  plus `ZR_AOT_C_FAIL()` directly into generated C.
- The generated block did not call the existing runtime reporter and did not use
  `ZR_AOT_C_RETURN(...)`, so it duplicated boundary reporting logic in generated code.
- The repository checkout is heavily dirty with unrelated AOT, parser, runtime, and docs
  work; this slice only accepts the focused WSL evidence below.

## RED

- `zr_vm_aot_c_source_contracts_test` first failed 1/19 after the cleanup-exit contract
  required `ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state,`
  and rejected the old unsupported-instruction inline failure template.

## GREEN

- `backend_aot_write_c_unsupported_instruction_expr()` now emits only
  `zr_aot_unsupported_instruction` plus one
  `ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_ReportUnsupportedInstruction(...))` call.
- The generated unsupported-instruction path no longer declares
  `zr_aot_instruction_index` or `zr_aot_opcode`, and no longer embeds the old
  hand-written unsupported-instruction `RunError` template.
- The live generated-C helper in `zr_vm_aot/tests/parser/test_known_call_pipeline.c`
  now recognizes the cleanup-exit helper shape.

## Verification

- `zr_vm_aot_c_source_contracts_test` passes 19/0.
- In `zr_vm_aot_c_shared_library_smoke_test`, the unsupported instruction boundary
  subtest passes after requiring the cleanup-exit helper call and rejecting the old
  generated locals and hand-written failure template.
- The full `zr_vm_aot_c_shared_library_smoke_test` binary still reports two unrelated
  execution failures in numeric arithmetic and generic primitive conversion cases in the
  current dirty checkout; those failures are outside this unsupported-instruction
  boundary slice.
- Broader WSL GCC focused group passes: source contracts 19/0, return contracts 1/0,
  frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts
  4/0, call shared-library smoke 3/0, global contracts 6/0, and global shared-library
  smoke 7/0.
- `ctest -R 'aot_c_(typed_scalar|call_shared_library|global_(contracts|shared_library_smoke))'`
  still matches only the registered `aot_c_typed_scalar` test in this build and passes
  1/1.
- Generated unsupported-instruction boundary C contains three
  `ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, ...)` cleanup-exit calls and
  no generated `const TZrUInt32 zr_aot_instruction_index`,
  `const TZrUInt32 zr_aot_opcode`, or
  `ZrCore_Debug_RunError(state, "unsupported AOT instruction")` template.

## Acceptance Decision

Accepted for this 07-S5 sub-slice. The unsupported instruction VM boundary now reuses
the shared AOT runtime reporter and exits through the generated function cleanup label
instead of expanding reporting and failure code inside generated C.

Remaining work: this is not final deopt/dynamic bridge handling. Typed-to-typed native
signature routing, in/out writeback, deopt/dynamic bridges, and other remaining boundary
templates remain later 07-S5 work; stages 08-12 are not started.
