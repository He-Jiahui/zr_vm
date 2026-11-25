---
related_code:
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_arithmetic.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c
  - zr_vm_core/include/zr_vm_core/integer.h
related_tests:
  - tests/parser/test_aot_llvm_signed_load_const_lowering.c
  - tests/parser/test_aot_llvm_specialized_cast_fusion.c
doc_type: runtime-contract
status: implemented
---

# Fused signed load/const arithmetic in AOT (C-U1)

Quickening fuses a `GET_CONSTANT` or `GET_STACK` that materializes a live slot
with the signed arithmetic that consumes it. Eleven opcodes result:

| Shape | Opcodes | Materialization (interpreter order) | Left operand |
| --- | --- | --- | --- |
| LOAD_CONST | `ADD/SUB/MUL/DIV/MOD_SIGNED_LOAD_CONST` | `B := const[k]` | `A` |
| LOAD_STACK_CONST | `ADD/SUB/MUL/DIV/MOD_SIGNED_LOAD_STACK_CONST` | `B := copy(A)` | `B` |
| LOAD_STACK_LOAD_CONST | `ADD_SIGNED_LOAD_STACK_LOAD_CONST` | `B := copy(A)`, then `C := const[k]` | `B` |

`D = operandExtra`, `A/B/C = operand0[0..2]`, `k = operand1[1]`. For the
three-slot form, `operand0[2]` shares a byte with `k`;
`ZR_INSTRUCTION_LOAD_STACK_LOAD_CONST_SLOT_FITS(C, k)` must hold.

## The contract both AOT backends follow

- **Order.** Materialize first, then read the left operand. A LOAD_CONST form
  with `A == B` therefore reads `k`, and `D == B` or `D == C` ends with the
  result.
- **Arithmetic.** It is the same checked arithmetic as the interpreter, with the
  same messages: `signed integer addition/subtraction/multiplication overflow`,
  `divide by zero`, `signed integer division overflow` and `modulo by zero`.
- **Result tag.** Add and sub keep the left operand's signed tag
  (`INT8/16/32/64`). Mul, div and mod store `INT64`. The materialized constant
  slot keeps the constant's own tag.
- **Failure.** On failure, `D` is not written. The materialized slots stay
  written.

The AOT C backend writes these operations as inline C
(`backend_aot_c_lowering_typed_arithmetic_load_const.c`). The LLVM backend
composes the existing `GET_CONSTANT`/`GET_STACK` sub-lowerings with the guarded
`ZrLibrary_AotRuntime_{Add,Sub,Mul,Div,Mod}SignedConst` helpers. For add and
sub it then stores the saved left tag into `D`. An emit-time check (`k` in range
and signed, plus `SLOT_FITS` for the three-slot form) keeps the
unsupported-instruction report for malformed input.

## Decisions (t727u, recorded)

### R1: `DivSignedConst` and `INT64_MIN / -1`

The helper's integer branch now uses `ZrCore_Integer_Divide`. `INT64_MIN / -1`
raises `signed integer division overflow` (`RUNTIME_ERROR`) instead of
executing host-undefined behaviour, which traps with SIGFPE on x86-64. The
interpreter is the oracle. This also fixes the already-lowered non-fused LLVM
`DIV_SIGNED_CONST(_PLAIN_DEST)`.

Rejected alternatives:

- an emitter `-1` guard, because its failure kind differs from the interpreter's;
- lowering the division as `Neg`, because the message would be wrong;
- wrapping to `INT64_MIN`, because that changes the language's overflow policy.

### R2: arithmetic faults stay terminal in AOT for now

Every AOT arithmetic fault, fused or not, in C or LLVM, raises through
`ZrCore_Debug_RunError`. It long-jumps to `ZrLibrary_AotRuntime_ExecuteEntry`,
which returns `RUNTIME_ERROR`. No zr `catch` or `finally` runs. This differs
from the interpreter, which catches mul/div/mod faults. Interpreter parity is
the chosen end state and is tracked as follow-up AOT-EH-A1.

Making only these eleven opcodes catchable would make catch behaviour depend on
quickening, so that option was rejected.

### D3: faulting PC

The shared classifier `backend_aot_c_step_flags_for_instruction` marks these
opcodes `MAY_THROW`, so `BeginInstruction` publishes the faulting instruction:

- the add, sub and mul forms always;
- the mod forms unless `k` is a known non-zero constant;
- the signed div forms, including non-fused `DIV_SIGNED_CONST(_PLAIN_DEST)`,
  unless `k` is a known constant that is neither `0` nor `-1`.

The other consumers of the classifier only test
`CONTROL_FLOW|CALL|RETURN`, so liveness and call handling do not change.

## Test coverage

`tests/parser/test_aot_llvm_signed_load_const_lowering.c` (CTest
`aot_llvm_signed_load_const_lowering`) covers:

- the classifier flags;
- the LLVM IR for hand-built rows of all eleven opcodes, including the
  three-slot form;
- `DivSignedConst` at `INT64_MIN / -1`, run in a child process because the old
  code traps;
- source programs that the quickener fuses into ten of the eleven opcodes.
  Each executes through the generated LLVM shared library and is compared with
  the interpreter's value and tag;
- the terminal fault contract: LLVM fails with the interpreter's overflow
  messages.

`ADD_SIGNED_LOAD_STACK_LOAD_CONST` survives quickening only when its constant
slot is still live afterwards, so ordinary sources dematerialize it. It is
covered by the hand-built rows only.

## Known follow-ups (not part of C-U1)

- **F1.** Other raw signed divisions remain: `DivSigned`, the generic `Div`,
  and the AOT C fused/typed div writers.
- **F2 / AOT-EH-A1.** Make AOT arithmetic faults catchable. First measure
  whether the interpreter's fused add/sub overflow is catchable.
- **F3.** Non-fused `ADD/SUB_SIGNED(_CONST)` widen the result tag to `INT64` in
  both AOT backends, while the interpreter keeps the left operand's tag.
- **F4.** `ModSignedConst` and the interpreter both negate a negative divisor,
  which is undefined when the divisor is `INT64_MIN`.
