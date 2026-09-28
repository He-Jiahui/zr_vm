---
related_code:
  - tests/cmake/ssa-tests.cmake
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_llvm.c
implementation_files:
  - tests/cmake/ssa-tests.cmake
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
tests:
  - tests/parser/test_ssa_aot_scalar_text.c
  - tests/acceptance/ssa-aotir-scalar-text-emitter.md
doc_type: module-detail
status: implemented-subset
---

# AOTIR constant i64 and direct-branch text emitter

## Contract and trust boundary

The two entry points in `backend_aot_ir_scalar_text.h` emit one standalone
function each: C11 source or textual LLVM IR. Both consume the same validated
`SZrAotIrModule` and share one shape check. This is a narrow 07.02
emission slice, not an artifact writer or a VM entry registration path. The
existing `backend_aot_ir_emit_c_ex` and `backend_aot_ir_emit_llvm_ex` still
return `ARTIFACT_UNAVAILABLE` when `requireArtifact` is true.

The caller must supply an explicit, trusted
`ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64` declaration. The shared
`ZrCore_AotIr_RequireExecutableAbi` gate checks the AOTIR schema, relocation
freedom, ABI declaration, RETURN type/value shape, and one earlier definition
of that SSA value across the function. The emitter then accepts only its
smaller single-block form. Neither check authenticates the producer of the
declaration. In the test fixture, `17u` is an arbitrary opaque metadata type
token: equality to the declared return token alone does not prove that token
means a language i64. The explicit trusted callable ABI declaration is the
authority for treating the 64 constant bits as an i64 value. The current
`ZrParser_ExecIr_LowerAot` always declares `UNKNOWN`, so a normal source
projection cannot enter this emitter slice. The executable ABI gate establishes
one definition before `RETURN`, but does not prove general CFG dominance. The
exact accepted branch shape below places the definition in the only successor
of the entry block, immediately before its `RETURN`.

## Accepted shape

The module contains exactly one function, one constant and no layouts,
relocations, required capabilities, declared effects, or module flags. The
function has exactly one operand and result pool entry, with one of these
instruction and CFG shapes:

- One entry block contains `CONSTANT → RETURN`, with no CFG edge.
- An entry block contains only an unconditional `BRANCH` to a second block.
  That block contains `CONSTANT → RETURN`. The branch and entry successor name
  the second block, whose sole predecessor names the entry block. No other
  edges or instructions are accepted.

`CONSTANT` defines the typed value; `RETURN` consumes that same value and ends
its block. The constant-pool index is zero, its flags are zero, and its type
token matches the trusted ABI return token. There are no additional block
flags, CFG edges, phi values, memory tokens, effect
tokens, frame slots, GC/deoptimization state, or side-state maps. Source map
metadata may remain because this emitter does not use it to decide semantics.
All other shapes fail before output is published.

The source defines `int64_t zr_aot_scalar_fn_<functionId>(void)`. LLVM text
defines the same symbol with `define i64 ...()`. A 64-bit constant bit pattern
is rendered as a signed i64: nonnegative values use an ordinary positive
literal, negative values use a negative magnitude, and C11 uses `INT64_MIN`
for the minimum value to avoid an out-of-range positive literal. This slice
renders the two-block edge as C `goto` and a label, or LLVM `br label` and a
basic-block label. It does not perform arithmetic, conversions, calls, memory
operations, exception handling, or state restoration. The generated function
takes no `SZrState*` and is not the VM's `FZrAotEntryThunk` ABI.

The AOTIR target hashes are schema identities, not a host toolchain match.
The standalone text has no target-specific instruction or data layout; the
external compiler chooses the actual target when compiling it. A future
artifact path must check target compatibility, bind the VM entry ABI, and
register/load the result before claiming AOT execution coverage.

## Failure and ownership

The caller owns `output`, `outLength`, and the AOTIR input lifetime. A
successful call writes a NUL-terminated source string and its byte count
excluding the terminator. When `output` has nonzero capacity, failure clears
its first byte; when `outLength` is present, failure sets it to zero. Null
arguments, invalid schema, missing function, `UNKNOWN` ABI, unsupported IR
shape, and too-small buffers return their specific `EZrAotIrStatus` and fill
`SZrAotIrDiagnostic` when supplied. `snprintf` performs bounded writes and a
short buffer is reported as `ZR_AOT_IR_INVALID_RANGE`; no truncated source is
accepted. The emitter allocates no memory and does not retain input pointers.

## Validation scope

`test_ssa_aot_scalar_text.c` checks the C/LLVM text for one-block `42`, `-1`
and `INT64_MIN`, and two-block `42` and `INT64_MIN`, then writes fixtures for
external compilation and invocation. It also checks `UNKNOWN`, bad input,
malformed or extra CFG edges, extra pools/operations/effects, constant flags,
missing terminator markers and short buffers, including output clearing.
The registered `zr_vm_ssa_aot_scalar_text_test` target runs as CTest
`ssa_aot_scalar_text` without writing generated files. The acceptance record
contains direct GCC, Clang, and MSVC commands, actual generated-function
invocations, and the formal MSVC CTest result. These are standalone native
function checks; they do not establish 07.02's full scalar/control/call,
runtime loader, or four backend differential milestones.
