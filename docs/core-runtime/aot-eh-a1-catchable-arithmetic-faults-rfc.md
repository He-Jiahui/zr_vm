---
related_code:
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_exception_control.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
  - zr_vm_core/include/zr_vm_core/integer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
doc_type: rfc
status: proposed
---

# RFC AOT-EH-A1: catchable arithmetic faults in AOT (plan only)

This is follow-up F2 of C-U1 (decision R2). It is a plan, not an
implementation. Nothing here changes behaviour on its own.

## Problem

An arithmetic fault in AOT code ends the whole AOT entry. This covers integer
overflow and a zero divisor or modulus. The error long-jumps to
`ZrLibrary_AotRuntime_ExecuteEntry`'s `ZrCore_Exception_TryRun`, which returns
`RUNTIME_ERROR`. No zr `catch` or `finally` runs, either in the faulting frame
or in its callers. This holds for both AOT backends and for every arithmetic
opcode:

- runtime helpers such as `Add/Sub/Mul/Div/ModSignedConst` raise through the real
  `ZrCore_Debug_RunError`;
- the AOT C inline writers do the same.

The interpreter is not uniform either.

## Measured interpreter behaviour (2026-11-25)

Measured with `zr_vm_cli -e` from the shared Release build of main `b3782a2c`.
The interpreter sources are unchanged on the branches that were built.

| Source body inside `try { … } catch (e) { return 777; }` | Argument | Result |
| --- | --- | --- |
| `return v + 4;` (fused `ADD_SIGNED_LOAD_STACK_CONST`) | `INT64_MAX` | **uncaught** `RuntimeError: signed integer addition overflow` |
| `return v + w;` (`ADD_SIGNED`) | `INT64_MAX, 4` | **uncaught**, same message |
| `return v - 4;` | `-INT64_MAX` | **uncaught** `signed integer subtraction overflow` |
| `return v * 4;` | `2^62` | caught, returns `777` |
| `return v / 0;` | `7` | caught, returns `777` |
| `return v % 0;` | `7` | caught, returns `777` |
| `try { r = v + 4; } finally { r = 5; }` | `INT64_MAX` | **uncaught**; `finally` does not run |

Why add and sub differ from mul, div and mod:

- `execution_dispatch.c` defines a dispatch-local `ZrCore_Debug_RunError`
  (`:6103`, `#undef` at `:10380`). It routes to `execution_raise_vm_runtime_error`
  (`:2241`), which unwinds to zr handlers. The mul, div and mod bodies use it.
- The add and sub bodies call the `static inline` `ZrCore_Integer_Add` /
  `ZrCore_Integer_Subtract` from `integer.h`. Those were compiled before the
  macro existed, so they call the real long-jumping `ZrCore_Debug_RunError`.

**HANDOFF (zr_vm_core, not AOT-owned):** make interpreter signed add and sub
overflow catchable. One way is to use `ZrCore_Integer_TryAdd` /
`ZrCore_Integer_TrySubtract` and raise dispatch-locally. This is a
prerequisite. AOT should not become "more catchable" than the interpreter.

## Goal (end state chosen under t727u, decision R2)

Every arithmetic fault in both AOT backends behaves like the interpreter, after
the HANDOFF above lands:

- a `catch` in the same frame or in any caller receives the error object, with
  the same message and the faulting PC;
- `finally` blocks run.

## Design

The design reuses the resumable-raise pattern that `REQUIRE_NON_NULL` already
uses (`ZrLibrary_AotRuntime_RequireNonNull`, `aot_runtime.c:9421`):

1. **Runtime.** Add `ZrLibrary_AotRuntime_RaiseArithmeticFault(state, frame,
   faultKind, &resumeInstruction)`. It works the same way `RequireNonNull` does:
   1. Normalize a `RuntimeError` with the interpreter's message, without a
      long jump.
   2. Call `execution_clear_pending_control` and
      `execution_unwind_exception_to_handler`.
   3. Handle the outcome:
      - **Handler in this frame:** refresh the frame and return the resume
        index.
      - **Handler in a caller:** return false quietly, so that
        `FailGeneratedFunctionAt` and the caller's
        `CompletePreparedDirectCallWithResume` resume there. This path already
        exists for `THROW`.
      - **No handler:** call `ZrCore_Exception_Throw`, which keeps today's
        terminal behaviour.
2. **Helpers.** The checked arithmetic helpers stop raising internally. They
   call `ZrCore_Integer_Try*` and `execution_checked_i64_*`, and report
   failure through a new out-parameter (`faultKind`). Signatures and the prelude
   change, so the prelude/ABI version must be bumped. Existing callers that pass
   no out-parameter keep today's raising behaviour behind a compatibility
   wrapper until both emitters have moved over.
3. **LLVM.** Each arithmetic lowering that can fault (step flag `MAY_THROW`,
   including the C-U1 fused forms) does two things:
   - passes `ptr %resume_instruction`;
   - on helper failure, branches to a cold block that calls
     `RaiseArithmeticFault`, then dispatches through the existing resume switch
     used by `THROW` and `REQUIRE_NON_NULL`.
4. **AOT C.** The inline writers already test each fault condition before they
   compute. Replace their `ZrCore_Debug_RunError(...); ZR_AOT_C_FAIL();` with
   `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_RaiseArithmeticFault(state, &frame,
   kind, &zr_aot_next_instruction))` followed by the existing resume dispatch.
   The checked-arithmetic calls (`ZrCore_Integer_*`) become their `Try*` forms
   with an explicit branch.
5. **PC.** Step flags already mark these opcodes `MAY_THROW` (D3), so the
   faulting PC is published before the raise.

## Phases

| Phase | Owner | Content |
| --- | --- | --- |
| P0 | zr_vm_core (HANDOFF) | Interpreter add/sub overflow catchable, with tests mirroring the table above |
| P1 | AOT | Runtime raise helper and fault-kind helpers; LLVM lowering for helper-based ops; prelude version bump |
| P2 | AOT | AOT C inline writers (typed, fused load/const, scalar locals) |
| P3 | AOT | Cross-frame tests: callee faults, caller catches; `finally` ordering; nested `try` |

## Tests (per phase)

- Run each row of the table above through the interpreter, AOT C and LLVM.
  - **Before the change:** AOT is terminal. This is the current contract and is
    pinned by `test_aot_llvm_signed_load_const_lowering`'s fault rows.
  - **After the change:** AOT returns `777`, `finally` runs, and the message is
    equal in all three.
- Faulting-PC and line in the caught error object match the interpreter.
- Run the full AOT sweep and require zero failure delta. `ExecuteEntry` must
  still be terminal when there is no handler.

## Cost and risk

- **Fast path.** Every check already exists; only the cold path changes. A
  measurement is still needed, with at least 5 alternating runs of
  `numeric_loops` and `dispatch_loops`, comparing C and LLVM generated-code size
  and time.
- **ABI and prelude.** Helper signatures change, so generated artifacts from
  older builds must be rejected by version. The existing artifact-schema
  checks apply.
- **Not in scope:**
  - catchability of non-arithmetic runtime errors raised inside helpers, such
    as member access or calls;
  - unsigned and float faults, which follow the same pattern later.

## Rejected alternatives

- **Making only the eleven C-U1 fused opcodes catchable.** Catch behaviour
  would then depend on whether quickening fused an expression.
- **Declaring terminal faults the permanent AOT contract.** This contradicts
  the interpreter for mul/div/mod. AOT is an execution mode of the same
  language.
- **`setjmp` inside every generated frame.** The per-call cost is too high, and
  it duplicates the existing resume-index machinery.
