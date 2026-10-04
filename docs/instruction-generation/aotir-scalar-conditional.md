---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_conditional.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_conditional.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - tests/parser/test_ssa_aot_scalar_conditional.c
  - tests/cmake/ssa-aot-scalar-conditional-tests.cmake
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_conditional.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_conditional.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
plan_sources:
  - .codex/plans/20261004-aot-scalar-conditional.md
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
tests:
  - tests/parser/test_ssa_aot_scalar_conditional.c
  - tests/acceptance/ssa-aot-scalar-conditional.md
doc_type: module-detail
status: implemented-awaiting-root-validation
---

# Finite signed i64 comparison and conditional returns

## API and representation boundary

Both existing scalar public emitters select the dedicated conditional helper
when `constantCount == 4`, after establishing empty output and zero length.
The constant-only and two-constant arithmetic paths retain their separate
preparation. The helper calls RequireExecutableAbi before indexing descriptors,
requires exactly one explicitly declared NOARGS_I64 function, and preserves
shared validation and relocation errors.

COMPARE `typeToken` is a predicate selector: 1 means signed less-than, 3 means
signed greater-than. This convention comes from the shared ExecIR interpreter
and ExecBC comparison model. Each operand must refer to one of the entry's two
CONSTANT definitions. Their instruction and pool tokens must match the explicit
i64 ABI token; that trusted declaration determines signed representation.
COMPARE defines the unique internal boolean used by CONDITIONAL_BRANCH. C uses
`_Bool`, LLVM uses `i1`; the callable function returns i64 and has no bool ABI.
The condition cannot be returned or substituted for an i64 operand.

The helper consumes manually constructed, schema-valid finite descriptors.
AOTIR currently allows nonzero `matchTypeToken` only on TYPE_TEST, whereas typed
ExecBC comparisons carry their i64 match token. The current projection producer
copies that field unchanged and cannot supply this finite descriptor directly.
The helper does not weaken that shared rule or implement a typed producer
adapter. Parser-to-native conditional integration remains a separate gap.

## Exact accepted graph and mapping

The graph has three blocks and eight instructions. Entry contains CONSTANT,
CONSTANT, COMPARE, CONDITIONAL_BRANCH. Each target contains its own CONSTANT and
RETURN. Result and operand pools each have five entries; the shared CFG pool has
four entries: true/false successor IDs followed by one entry predecessor ID for
each return block. Blocks have exact instruction ranges and terminators; only
the first block has ENTRY flags. Both target IDs must be distinct and equal to
the two actual return-block IDs. Target storage order does not determine truth:
successor ordinal 0 is true, ordinal 1 is false.

Five result ValueIds must be distinct and nonzero. The first two are comparison
input definitions, the third is the condition, and the last two are local return
definitions. Operands are resolved by actual IDs in their declared order, so
reversed comparison operands retain their meaning. Each return must consume the
definition in its own block, even if another earlier i64 definition would pass
the broader ABI gate.

The four CONSTANT layoutIds are actual pool indices. They must cover the four
entries without duplication. Pool flags are zero, and all constants and return
instructions match the ABI token. The private plan copies operand and return
bit patterns, predicate, condition identity, block IDs and ordered targets. It
keeps no borrowed pointer and allocates nothing.

## Metadata and output safety

The finite graph cannot carry executable effects or state. It rejects module
flags, capabilities/effects, layout descriptors, runtime slots and nonzero frame
logical/storage/parameter/return-area fields; phi/memory pools and instruction
ranges; GC maps/roots/hashes, exception/debug hashes, deopt metadata and logical
state maps; instruction flags, effect tokens, binding rows, deopt IDs and match
tokens. Shared module validation still handles descriptive source annotations.
The helper emits no allocation, throw, suspend, runtime guard or memory effect.

C source contains signed input locals, an actual relational comparison and
conditional gotos to two labelled returns. LLVM emits `icmp slt i64` or
`icmp sgt i64`, a `br i1` with the copied successor order, and both returns.
Neither implementation subtracts the operands to compare, folds the selected
return into one literal, or emits PHI. MIN uses `INT64_MIN` in C; other signed
literals and LLVM negative decimals use unsigned magnitude, avoiding undefined
host negation or unsigned-to-signed conversion.

Length excludes NUL. Exact length plus one succeeds. Exact length fails with
INVALID_RANGE, clears the output and leaves length zero. All other failures
also preserve the public entry's empty-output baseline. Diagnostics are optional;
finite rejections identify the responsible instruction, while shared validation
errors retain their exact status and site.

## Verification scope

The fixture retains twelve independently module/ABI-validated positives covering
LT/GT, true/false, equality, negative/mixed values, MIN/MAX, reversed operands,
reversed pool indices and reversed branch edges. Expected result bits are
independent fixed witnesses 101 and -202. Sixteen named rejection rows check
exact statuses/sites and clear stale output. Ten emitter parameter checks cover
null module/output/length, zero capacity and optional diagnostic. Every positive
checks exact capacity, failure clearing and successful buffer reuse after that
failure. Optional successful generation writes twelve C, twelve LLVM and twelve
runner files; root must compile/run 24 corresponding native products.

Source preparation is not a GREEN execution result. Actual RED and subsequent
root validation evidence belong in the acceptance document. Full SSA 07.02,
PHI/joins/loops, general bool ABI, parser projection and artifact registration
remain outside this finite implementation.
