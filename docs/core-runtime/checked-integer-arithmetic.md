---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_checked_integer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_signed_multiply.inc
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
  - tests/core/test_execution_checked_multiply.c
  - tests/cmake/checked-multiply-tests.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_checked_integer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_signed_multiply.inc
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - user: 2026-10-02 SSA plan execution and checked i64 multiplication
  - docs/plans/ssa/index.md
  - docs/plans/ssa/architecture-design.md
  - docs/instruction-generation/numeric-vector-contract.md
tests:
  - tests/core/test_execution_checked_multiply.c
  - tests/acceptance/2026-10-02-ssa-checked-multiply.md
doc_type: module-detail
---

# Checked signed integer multiplication

The legacy VM's seven typed signed multiplication instructions now reject an
i64 product outside `INT64_MIN..INT64_MAX` before evaluating a C signed
multiplication. This supplies the runtime foundation required by strict SSA
multiplication materialization. Previously Oracle and the pure ExecBC interpreter
rejected overflow while the legacy instructions executed an unchecked C product.

## Scope and instruction forms

The same check applies to `MUL_SIGNED`, `MUL_SIGNED_PLAIN_DEST`,
`MUL_SIGNED_CONST`, `MUL_SIGNED_CONST_PLAIN_DEST`, `MUL_SIGNED_LOAD_CONST`,
`MUL_SIGNED_LOAD_STACK_CONST`, and `MUL_SIGNED_LOAD_STACK`. Operand decoding,
constant/stack materialization, destination storage, and floating fallback retain
their established instruction contracts. Unsigned multiplication, generic mixed
numeric arithmetic, signed addition/subtraction/division, AOT emission, and numeric
policy execution are outside this module's coverage. A declared checked policy
alone does not implement those operations.

`execution_signed_multiply.inc` contains the existing seven instruction bodies.
The dispatch file includes this cohesive block instead of growing new arithmetic
logic inside its already large execution loop. `execution_checked_integer.h` is
a private header with a pure reusable predicate/result helper; it adds no public
ABI or runtime state.

## The arithmetic contract

`execution_checked_i64_multiply(left, right, output)` returns false for a null
output or an overflowing product, without changing the output. Inputs are passed
by value, so the output may alias a caller's input variable. A successful call
writes the exact i64 product and returns true.

Zero is handled without division. The two `INT64_MIN * -1` permutations are
rejected explicitly. The remaining cases compare one operand against a safe
division of the matching signed limit, separately for each sign combination.
Only after those checks does the helper evaluate `left * right`. No signed
negation of `INT64_MIN`, division by zero, `INT64_MIN / -1`, wrapping compiler
flag, sanitizer suppression, or implementation-defined out-of-range conversion
is needed. The algorithm follows the existing checked arithmetic in the Oracle
and pure ExecBC interpreter; those independently owned files are unchanged.

## Stores, aliases, and ownership

Each integer instruction computes a temporary checked result before writing its
destination. This also keeps aliasing destinations safe when they are either
input slot. Successful ordinary stores use `EXECUTION_STORE_PLAIN_REUSE`, which
releases an owned destination through the existing overwrite support. Plain
destination forms use their existing direct store, whose frame contract admits
plain metadata. Their operands are read before any destination overwrite.

On overflow, the multiplication body performs no result store. Existing VM
unwind support closes registered values and ownership proxies. Load variants may
already have performed the load specified by their opcode before multiplication;
the check does not change that ordering. Floating operands continue through the
existing numeric fallback and do not get an integer overflow check.

## Errors, program counters, and cleanup

The instruction macros expand inside the dispatch loop and call its local
`ZrCore_Debug_RunError` macro on overflow, with the message
`signed integer multiplication overflow`. That macro saves the current faulting
instruction PC before invoking `execution_raise_vm_runtime_error`. The latter
normalizes a runtime Error payload with `ZR_THREAD_STATUS_RUNTIME_ERROR` and
enters `execution_unwind_exception_to_handler`.

Consequently a matching language catch can consume the error, a finally block
runs before propagation, and an uncaught error closes scopes, drops inline frame
values, closes closures, and leaves the affected call frames before throwing to
the host recovery point. The arithmetic helper does not allocate, store VM
values, throw, or call the external `ZrCore_Debug_RunError` function. Calling that
external function from the helper would bypass the dispatch loop's local catch
and finally transfer.

The runtime error is the legacy VM's status representation of the arithmetic
failure that Oracle/ExecBC expose as `ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR`.
This module does not add an ExecIR diagnostic field to legacy instructions.

## Reference language evidence

The local reference implementations all make their overflow representation
explicit. Lua `lua/src/lvm.h` uses unsigned arithmetic through `intop`, and
`lua/testes/math.lua` checks `minint * minint == 0`. QuickJS
`lua/QuickJS-master/quickjs.c` widens its int32 multiplication to int64, then
selects a float representation when the result no longer fits int32;
`tests/test_language.js` exercises large products. CPython
`lua/cpython/Objects/longobject.c` uses a wider digit or arbitrary precision
product, with large product identities in `Lib/test/test_long.py`.

ZR intentionally uses a checked i64 error instead of those languages' wrapping,
floating promotion, or arbitrary precision rules. This follows the SSA strict
integer contract and the checked default in the numeric vector contract.

## Validation and limits

The independent core target exercises a 26-row boundary catalog across all seven
instruction forms, with ordinary and trace-observer execution. It also covers
both destination aliases, unchanged output on helper failure, null output,
faulting PC preservation, error status normalization, catch/finally, finally
rethrow, registered ownership cleanup, floating fallback, and reuse of a state
after failure. `LOAD_STACK_CONST` uses source slot 0 and a distinct materialized
slot 3 throughout the matrix, including alias and overflow cases, so a missing
load cannot be masked by reading the original source slot.

Fixture resources have static lifetime across Unity assertion longjmp: the
active state/function and actual owner/weak values are retained by the fixture.
Both successful completion and `tearDown` clear the trace observer first, close
registered values, release any tracked owned destination through a saved VM
stack offset, release owner/weak values, reset the thread, free the function, and
destroy the state. Successful cleanup clears the tracked pointers. Cleanup does
not depend on addresses of automatic values or a departed observer capture.
Owned slots are exercised for the five ordinary store forms;
putting owned metadata into an unqualified plain destination would violate that
instruction's existing precondition.

The dated acceptance document records toolchain results and outstanding gates.
In particular MSVC ordinary/observer execution alone cannot certify GCC's
computed-goto dispatch. Full language/materializer tests remain the responsibility
of the parent SSA integration slice after the lower runtime support is accepted.
