---
related_code:
  - zr_vm_parser/src/zr_vm_parser/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_internal.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot_c_emitter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_c_emitter.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot_llvm_emitter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_llvm_emitter.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - tests/parser/test_execbc_aot_pipeline.c
  - tests/cmake/run_projects_suite.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_internal.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot_c_emitter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_c_emitter.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot_llvm_emitter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot_llvm_emitter.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
plan_sources:
  - user: 2026-04-05 true AOT ExecIR / 调用帧协议 / aot_c 重整与 LLVM 真后端方案
  - .codex/plans/真 AOT ExecIR 调用帧协议  aot_c 重整与 LLVM 真后端方案.md
  - .codex/plans/真 AOT C 执行管线替代当前 Shim AOT.md
tests:
  - tests/parser/test_execbc_aot_pipeline.c
  - tests/fixtures/projects/aot_eh_tail_gc_stress/src/main.zr
  - tests/fixtures/projects/aot_eh_tail_gc_stress/bin/main.zri
  - tests/cmake/run_projects_suite.cmake
doc_type: module-detail
---

# True AOT ExecIR / Call-Frame Protocol / LLVM Backend Design

## Scope

This document records the current true-AOT backend boundary after the April 5, 2026 lowering work:

- `aot_c` executes generated C control flow instead of a VM shim thunk.
- `aot_llvm` emits real `zr_aot_fn_N` bodies and no longer stops at shim wrappers.
- both backends share the same generated-call frame protocol and the same first extracted `ExecIR` layer.

This round does not add a new public AOT ABI version. `ZrAotCompiledModule` and `ZrVm_GetAotCompiledModule` remain the formal descriptor surface.

## Current Architecture

### Shared ExecIR

`backend_aot_exec_ir.[ch]` is the current backend-neutral extraction. It already owns:

- flattened executable SemIR listing for the whole function tree
- runtime-contract aggregation
- backend-shared SemIR opcode naming
- backend-shared runtime-contract naming

This is intentionally smaller than the final milestone target. The remaining planned expansion is still:

- explicit per-function CFG objects
- stable frame-layout records
- callsite-kind records
- EH resume edges
- debug line maps attached to blocks instead of reconstructed ad hoc in emitters

Even in its current form, the extraction matters because `aot_c` and `aot_llvm` now consume one shared execution artifact instead of rebuilding separate overlays.

`backend_aot.c` is now a shared-helper layer only. It owns function-table construction, executable-subset gating, shared callsite provenance tracking, and backend-neutral writer option helpers. Public writer entrypoints plus backend-specific body/file emission now live in `backend_aot_c_emitter.[ch]` and `backend_aot_llvm_emitter.[ch]`, so the old monolithic file no longer depends on backend headers or backend-specific lowering templates.

### Call-Frame Protocol

`ZrAotGeneratedDirectCall` is the formal generated-call record. It carries:

- `nativeFunction`
- `callerCallInfo`
- `calleeCallInfo`
- `callerFunctionIndex`
- `calleeFunctionIndex`
- `callInstructionIndex`
- `resumeInstructionIndex`
- `observationMaskSnapshot`
- `publishAllInstructionsSnapshot`
- `prepared`

The runtime helpers that define the protocol are:

- `ZrLibrary_AotRuntime_PrepareDirectCall`
- `ZrLibrary_AotRuntime_PrepareMetaCall`
- `ZrLibrary_AotRuntime_PrepareStaticDirectCall`
- `ZrLibrary_AotRuntime_CallPreparedOrGeneric`
- `ZrLibrary_AotRuntime_FinishDirectCall`
- `ZrLibrary_AotRuntime_FailGeneratedFunction`

The protocol requirement is unchanged from the plan: native direct entry is allowed, but the VM-visible caller/callee chain must remain readable for exception propagation, debug stepping, observation policy, and instrumentation.

## AOT C Status

`aot_c` now follows the intended true-AOT shape:

- static known callees emit `PrepareStaticDirectCall -> zr_aot_fn_N -> FinishDirectCall`
- dynamic and meta calls emit `Prepare*Call -> CallPreparedOrGeneric`
- generated failures converge to `ZrLibrary_AotRuntime_FailGeneratedFunction`
- checked-in fixture output no longer contains:
  - `if (zr_aot_direct_call.prepared)`
  - `goto zr_aot_fail`
  - bare `return 0;`
  - `InvokeActiveShim`

`backend_aot_c_emitter.[ch]` now owns the C writer surface end-to-end:

- public `ZrParser_Writer_WriteAotCFileWithOptions` / `ZrParser_Writer_WriteAotCFile`
- file-level descriptor/blob/runtime-contract emission
- generated C function-body lowering

That keeps future `aot_c` shape work out of `backend_aot.c` and aligned with the parallel LLVM split.

## LLVM Backend Mapping

The textual LLVM backend is now a real execution backend, not a shim shell.

### Function Shape

Each ZR function lowers to:

```llvm
define internal i64 @zr_aot_fn_N(ptr %state)
```

with:

- `%frame = alloca %ZrAotGeneratedFrame`
- `%direct_call = alloca %ZrAotGeneratedDirectCall`
- `%resume_instruction = alloca i32`
- `%truthy_value = alloca i8`
- entry through `ZrLibrary_AotRuntime_BeginGeneratedFunction`
- per-instruction `BeginInstruction`
- a single fail block through `ZrLibrary_AotRuntime_FailGeneratedFunction`

### Direct Calls

The LLVM mapping currently supports:

- static direct call:
  - `PrepareStaticDirectCall`
  - `call @zr_aot_fn_M`
  - `FinishDirectCall`
- dynamic direct call:
  - `PrepareDirectCall`
  - `CallPreparedOrGeneric`
- meta call:
  - `PrepareMetaCall`
  - `CallPreparedOrGeneric`

This includes:

- `SUPER_META_CALL_NO_ARGS`
- `SUPER_META_CALL_CACHED`
- `META_CALL`
- `SUPER_META_TAIL_CALL_NO_ARGS`
- `SUPER_META_TAIL_CALL_CACHED`
- `META_TAIL_CALL`

Tail meta calls return through `ZrLibrary_AotRuntime_Return` after the shared callsite protocol instead of falling back to unsupported sinks.

### EH / Pending Control

The LLVM backend now lowers the generated control-transfer helper family:

- `TRY`
- `END_TRY`
- `THROW`
- `CATCH`
- `END_FINALLY`
- `SET_PENDING_RETURN`
- `SET_PENDING_BREAK`
- `SET_PENDING_CONTINUE`

`TRY`, `END_TRY`, and `CATCH` are simple guarded helper calls.

`THROW`, `END_FINALLY`, and the pending-control instructions use a backend-generated resume-dispatch protocol:

1. Emit the runtime helper with `ptr %resume_instruction`.
2. Load the produced instruction index.
3. Compare it against the fallthrough sentinel.
4. If not fallthrough, dispatch through an LLVM `switch` to the resumed instruction label.
5. If the runtime returns an invalid target, report it through `ZrLibrary_AotRuntime_ReportUnsupportedInstruction`.

This is the LLVM analogue of the existing C emitter dispatch loop. It keeps the resume target VM-driven instead of re-encoding exception/finally semantics in ad hoc frontend logic.

## Optimization Guardrails

The current implementation still treats VM visibility as the hard boundary.

Optimizations that remain allowed:

- block-local peephole shaping
- slot pointer reuse/caching
- typed arithmetic fast paths that preserve helper fallback

Optimizations still disabled by default:

- cross-function LLVM inlining
- eliding generated call records
- collapsing frame-visible call edges while observation, EH, or debug is active

The rule is still: generated native execution is fine, invisible VM stack edges are not.

## Validation Evidence

### Parser / Emitter Tests

`tests/parser/test_execbc_aot_pipeline.c` now locks:

- true-AOT LLVM meta-call lowering
- true-AOT LLVM meta tail-call lowering
- true-AOT LLVM EH/control-transfer lowering
- absence of unsupported sinks for the covered opcode families
- absence of shim entry wrappers

New LLVM-specific assertions cover:

- `PrepareMetaCall`
- `CallPreparedOrGeneric`
- `Try`
- `EndTry`
- `Throw`
- `Catch`
- `EndFinally`
- `SetPendingReturn`

### Project-Level Regression

The stress project `aot_eh_tail_gc_stress` is the key acceptance case for this round because it forces:

- ownership transitions
- cached meta call lowering
- nested `try/finally/catch`
- pending return inside `finally`
- resumed control flow

It now passes in both:

- `--execution-mode aot_c`
- `--execution-mode aot_llvm`

under the stress `projects` suite on WSL gcc and WSL clang.

## Remaining Plan Debt

The plan is not structurally complete yet.

### Still Missing

- full per-function ExecIR CFG and frame-layout objects
- an extracted LLVM emitter module to match the existing C emitter extraction
- a smaller orchestration-only `backend_aot.c`
- broader explicit LLVM lowering coverage for any opcode families still not exercised by the true-AOT suites

### Next Recommended Cut

The next split boundary should be:

1. move LLVM emitter helpers and `backend_aot_write_llvm_function_body` out of `backend_aot.c`
2. keep `backend_aot.c` as orchestration plus shared function-table construction
3. let both emitter modules consume only shared function-table and ExecIR inputs

That is the cleanest path to stop re-growing the current 3k+ line backend file while preserving the already-working true-AOT behavior.
