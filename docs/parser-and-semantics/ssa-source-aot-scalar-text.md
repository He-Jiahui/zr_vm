---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
  - docs/parser-and-semantics/ssa-source-module-contract.md
tests:
  - tests/parser/test_ssa_source_aot_scalar_text.c
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/parser/test_ssa_aot_scalar_text.c
  - tests/parser/test_ssa_aot_scalar_arithmetic.c
  - tests/parser/test_ssa_aot_scalar_conditional.c
doc_type: module-detail
status: finite-source-scalar-text-green
---

# Actual Source Descriptor Scalar Text Buffers

## Scope and current decision

The existing `backend_aot_ir_c_emit_const_i64` and
`backend_aot_ir_llvm_emit_const_i64` buffer APIs gain a finite profile for the
actual source descriptor produced from the literal SCRIPT fixtures `9;` and
`8;`. Root's actual r3 finite validation passed five suites: source22 and
descriptor20 Unity cases plus three prior standalone regressions. The
[acceptance record](../acceptance/ssa-source-aot-scalar-text.md)
retains the r1/r2 failures; the [test guide](../../tests/acceptance/ssa-source-aot-scalar-text.md)
defines the finite gate.

This slice emits C11 or LLVM text. It does not establish a generated executable,
registered entry, native loader, source execution, or whole-language C/LLVM
implementation. Projection `runnable` remains false. Plan 07.02 and the complete
47-shape typed matrix remain OPEN.

## API and output ownership

Both APIs have the same arguments and return `EZrAotIrStatus`:

```c
(module, functionId, output, capacity, outLength, diagnostic)
```

| Argument | Caller contract |
| --- | --- |
| `module` | Borrow a valid AOTIR module and every reachable array/owner for the whole call. |
| `functionId` | Use the actual descriptor function identity. |
| `output` | Supply writable text storage; it must not overlap input owners or other outputs. |
| `capacity` | Include the final NUL byte; zero is invalid. |
| `outLength` | Required writable length; successful length excludes NUL. |
| `diagnostic` | Optional separate writable `SZrAotIrDiagnostic`. |

The buffer checker sets writable length to zero and writable `output[0]` to NUL
before validation. A null buffer, zero capacity or null length returns
`ZR_AOT_IR_INVALID_ARGUMENT`. Insufficient capacity returns
`ZR_AOT_IR_INVALID_RANGE`, clears the first byte and leaves length zero.
Only the first byte is guaranteed cleared: callers must not use the remainder
of a failed buffer. An exact text-length-plus-one buffer succeeds; a buffer
whose capacity equals text length cannot hold the NUL.

The emitters allocate no persistent storage, retain no input views, transfer no
owners and do not extend descriptor lifetime. The original compiler, module,
compacted function, projection and descriptor owners must remain live and
unchanged through emission. Do not reset, compact, resize, free or concurrently
modify them. After emission, independent caller-owned text remains valid even
after those input owners are released. Normal use of a returned descriptor
after owner release remains an OPEN follow-up; no retention guarantee exists.

## Actual producer pipeline

The fixture uses the shared literal source preparation, finalized SCRIPT
metadata and real `BuildModule` result. The pipeline is:

1. Bind the original module using `ZrParser_ExecIr_BindSourceModuleContract`
   before compaction. Preserve actual function token, canonical signature hash,
   module identity and generation.
2. Verify the original function, then call
   `ZrParser_ExecIr_EliminateDeadSourcePlaces`. The compacted source remains
   CONSTANT / NOP / RETURN with the real IDs, source maps and surviving SSA value.
3. Derive the actual canonical i64 return type and create its host primitive
   row using `ZrParser_ExecIr_MakeHostPrimitiveLayout`. Append that row to the
   actual module and attach `ZrParser_ExecIr_AttachPrimitiveSourceFrame`.
4. Verify the compacted function and call
   `ZrParser_ExecIr_LowerAotWithCanonicalCallable`, preserving frame slots,
   value slots and the complete empty logical state map.
5. Derive the current-host no-argument i64 target through
   `ZrParser_ExecIr_MakeHostNoArgsI64AotTarget`, then build the borrowed descriptor
   with `ZrParser_AotIrProjection_BuildDescriptor`.
6. Call Core `ValidateModule` and `RequireExecutableAbi`, then emit both buffers.

The emitter is a consumer of these records. Clearing frame/state/source
records, rebasing identity, replacing the original graph with a synthetic
fixture or repairing metadata inside the consumer would invalidate this gate.

## Three constant profiles

The constant preparer preserves its two prior profiles: one-block
CONSTANT / RETURN, and entry BRANCH to a CONSTANT / RETURN block. They carry
no primitive rows, frame/value slots or logical state map.

The actual source profile is distinct:

| Record | Finite requirement |
| --- | --- |
| Module/function | One function; current explicit `NOARGS_I64` ABI; no capabilities/effects or relocations. |
| Control flow | One ENTRY block with three instructions in order: CONSTANT, plain NOP, RETURN; no edges. |
| SSA | One result and one return operand; their actual value IDs agree. |
| Constant | One flags-zero row with the ABI return type; resolve by the CONSTANT pool index. |
| Primitive | One primitive row with matching return type, 8-byte width and 8-byte alignment. |
| Frame | One VALUE slot matching the returned SSA ID/type; offset zero, width/alignment 8; one value-slot mapping to physical slot zero. |
| Frame layout | One logical/storage slot, no parameter prefix, 8-byte frame, return area offset 8; nonzero frame hash equals function contract layout hash. |
| State map | A present, Core-valid map with zero entries, values, roots and owner states; preserve its identity. |
| Contract | Module/function generation 1; module layout hash absent (zero); function layout hash witnesses its frame. |
| Runtime maps | No GC map/roots, EH map, memory chains, phi, deopt or recovery payload. |
| Debug/source | Preserve actual source/debug identities and maps accepted by Core. |

Instruction order constrains semantics; numeric function/block/instruction/value
IDs are not fixed fixture constants. A CONSTANT's `layoutId` denotes its
zero-based constant pool index in this opcode. That index is not the constant
row's stable `id`.

A plain source NOP retains the actual ABI return type token. Its layout/match
tokens, flags, effect links and binding/deopt references are zero, as are counts
for results, operands, successors,
phi inputs and memory inputs/outputs. Empty ranges may retain valid pool
cursors: `count == 0` does not require `offset == 0`. Core validates their
ranges before the finite profile inspects counts. The r1 implementation
incorrectly required zero cursors. After the cursor change, r2 still rejected
the actual NOP and failed 12/22; that change alone has not established admission.
Root's r4 diagnostic confirmed a second overstrict assumption: the real NOP
retains the return ABI type. The current consumer requires exactly that token,
does not admit arbitrary types and does not clear the producer field. Both
constraints passed the finite r3 functional gate without clearing producer fields.

Existing arithmetic and conditional dispatch remains selected by the prior
two-constant/four-constant profiles. Those three standalone regressions are
separate finite evidence and cannot substitute for the real source suite.

## Validation and diagnostics

The shared preparer calls `ZrCore_AotIr_RequireExecutableAbi` before dereferencing
the finite records. Core failures propagate their status and diagnostic unchanged.
A Core-valid but unsupported shape returns `ZR_AOT_IR_UNSUPPORTED`. Cross-record
primitive/frame contradictions return `ZR_AOT_IR_INVALID_LAYOUT` with the actual
function/instruction and expected/actual fields. The test changes actual frame
type or primitive width to exercise this boundary; an extra actual layout is
unsupported. A NOP type different from the actual return ABI is unsupported;
an invalid NOP flag propagates
Core `INVALID_OPCODE`.

Successful C uses `int64_t zr_aot_scalar_fn_<actual-id>(void)` with an explicit
integer literal. Negative magnitude is calculated in the unsigned domain;
`INT64_MIN` has a special literal. LLVM uses the same actual function identity
and signed decimal i64 return. The source NOP has no emitted runtime operation.
Tests parse the whole returned buffer and verify the actual 9/8 value, NUL,
length and unchanged input observations; they do not execute generated text.

## Remaining gates

Linux validation is OPEN after permission denial; MSVC coverage is limited to
changed-TU compile evidence and is not full Windows runtime acceptance.
Native/general lowering, registration, GC/EH/ownership, conversion/arithmetic
source families, full numeric boundaries and the full 47-shape matrix remain
OPEN. Independent Core module ownership is the next task. Its lifetime test
`tests/parser/test_ssa_source_aot_owned_module.c` is authored; the Core owner
implementation, test registration and actual validation remain OPEN.
