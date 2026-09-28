---
related_code:
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_aotir_contract.c
  - tests/parser/test_ssa_aot_callable_abi.c
  - tests/parser/test_ssa_aot_projection_branch_descriptor.c
  - tests/acceptance/ssa-aotir-logical-map-schema.md
  - tests/acceptance/ssa-aotir-explicit-callable-abi.md
  - tests/acceptance/ssa-aotir-branch-descriptor.md
doc_type: module-detail
status: implemented-subset
---

# Shared AOTIR contract

`zr_vm_core/include/zr_vm_core/aot_ir.h` defines the shared ABI projection
consumed by C and LLVM AOT backends.  AOTIR is not a second language IR: its
opcode, effect, CFG, state-map, signature, and frame identities originate in
ExecIR.  Backend-specific code may legalize alignment and calling convention,
but may not infer ownership, calls, or exceptions from quickened bytecode.
The current legacy emitter dependencies and the gap between this descriptor
and the owned ExecIR projection are inventoried in
`docs/instruction-generation/aot-emitter-consumer-matrix.md`. This contract
does not yet drive the archived C/LLVM artifact writers.
The target contract requires a nonzero target-triple hash and ABI hash in
addition to the supported ABI version, pointer size, and endianness; its
required capability mask may contain only known execution capabilities.
The module and function execution contracts likewise reject unknown capability
and effect bits before any backend lowering begins.

Schema version 4 replaces the old `{resumeId, instructionId, stateHash}`
checkpoint summary with a borrowed view of the complete ExecIR logical
state map. A function without checkpoints may leave `logicalStateMap` null.
With a map, the function token, signature hash and generation must match the
function contract; the caller keeps map entries and all side pools alive for
the lifetime of the AOTIR view. The per-function owned projection can supply
that lifetime, but no producer connects it to this module yet. The content
hash includes checkpoint phases, effect/handler/cleanup metadata and live,
root and owner-state pools, not capacities, pointers or a caller-supplied
summary hash. No physical native frame restoration is implied.

The function instruction view also carries typed `typeToken` and
`matchTypeToken` identities plus bounded `memoryIn`/`memoryOut` ranges into a
borrowed memory-token pool. TYPE_TEST must carry a nonzero match type token;
other instructions must carry zero. Memory ranges and every pool token are
validated before hashing, so C/LLVM consumers cannot silently lose memory
ordering or type-test semantics.

Schema version 7 adds `SZrAotIrFunction.callableAbi`. `UNKNOWN` (zero) is the
default for existing descriptor-only producers. The first fixed declaration,
`NOARGS_I64`, says that a trusted callable-signature producer has established
zero parameters and an i64 return; it also names the nonzero AOTIR type token
for that i64 result. The owned `SZrAotIrProjection` and descriptor builder
copy this declaration unchanged, and the backend adapter exposes the same
qualification result. The current source `ZrParser_ExecIr_LowerAot` has no
trusted primitive return-signature producer and explicitly leaves this field
`UNKNOWN`. Neither `typeToken`, opaque `signatureHash`, constant bits, legacy
ExecBC, nor `frameLayout.parameterPrefixBytes` establishes parameter count or
the i64 ABI. ExecIR's `frameLayout.parameterCount` is not carried by this
AOTIR record and does not establish the primitive return kind. Thus no current
source-to-AOTIR path claims an executable ABI.

`ZrCore_AotIr_ValidateModule` rejects unknown ABI enum values, a nonzero
return token on `UNKNOWN`, a zero return token on `NOARGS_I64`, and RETURN
instructions whose arity or type token disagrees with the declaration.
`ZrCore_AotIr_RequireExecutableAbi` additionally requires a validated,
relocation-free module, a known declaration, and a RETURN value with exactly
one matching typed definition across the whole function, earlier in instruction
order than that RETURN. It returns `UNSUPPORTED` for `UNKNOWN` or a missing
RETURN and clears its output on failure. This qualifies ABI/SSA shape; it does
not prove CFG dominance, authenticate a source-signature producer, or show that
an emitter can produce an artifact. The C/LLVM artifact entry points remain
unavailable. The canonical module hash includes ABI kind and return type token,
so descriptor identity changes when this declaration changes.

Frame legalization is represented by borrowed frame-slot records and a
logical-value-to-physical-slot pool. Source spans are borrowed as numeric
source maps and must resolve to the instruction/source identity they describe;
their offsets and one-based line/column endpoints are validated and hashed.
This keeps ABI layout and source diagnostics available to both consumers
without persisting host pointers.

The public records contain pointers only as in-memory views over caller-owned
arrays.  Semantic references are numeric IDs and bounded ranges, so the
canonical `ZrCore_AotIr_HashModule` ignores host addresses and is stable for
identical input.  `ZrCore_AotIr_ValidateModule` checks schema/execution
contract versions, target ABI, IDs, ranges, opcode bounds, CFG block instruction
partition, last-in-block terminator opcode and edge-target membership,
state-map storage shape, function identity, instruction/source membership,
bounded live/root/owner pools and phase-aware resume identity, effect pairing,
typed match-token and memory-token identities/ranges,
frame layout, phi incoming cardinality and ordered
predecessor-edge membership in the containing block's predecessor edge range,
nonzero state-map resume identity, and
module/function hash identity. Effect tokens must be present together or both
be absent. A
non-PHI instruction carrying phi incoming rows is rejected.
Module and function contract identity failures report the specific canonical
field and received value, so a signature, layout, target-token, module-hash, or
version mismatch cannot be misdiagnosed as a different contract field.
Block flags are likewise limited to the canonical entry/cold/cleanup/exception
bits. Storage slots may not outnumber logical slots, the parameter prefix must
fit before the return area, and frame byte size must cover the return area and
be a multiple of the declared frame alignment. Operand and result pools may not
contain the invalid zero value ID. Every function must publish exactly one entry block. Paired
effect tokens must advance strictly (`effectOut > effectIn`). Instruction flags
must include the dynamic effects required by their opcode schema (allocation,
throw, GC, or suspend); unknown flags and missing required effects are rejected
before lowering. Any opcode with a required dynamic effect must also publish a
nonzero paired effect-token range, and successive effectful instructions in a
block must consume the previous instruction's effect output.

Block predecessor and successor ranges are views over the same numeric edge
pool; their bounds use that pool's count, so parallel edges remain representable
without treating the block count as an edge-capacity limit. Each source/target
edge occurrence must have a matching predecessor occurrence in the target
block.

The owned ExecIR AOT projection keeps successors and predecessors in separate
arrays. `ZrParser_AotIrProjection_BuildDescriptor` validates their original
range bounds, then copies successors followed by predecessors into one
descriptor-owned AOTIR edge pool. It leaves instruction and block successor
ranges on the first segment and rebases nonempty block predecessor ranges onto
the second segment. `ZrParser_AotIrProjection_FreeDescriptor` frees that pool
through `function.successorPool`; no public descriptor or persisted schema
field is added. Overflow, malformed source ranges, and invalid edge targets
reject the candidate while an existing descriptor remains intact.

`ZrCore_AotIr_IsRelocationFree` rejects module or function relocation rows.
Unimplemented operation families should be reported by a lowering diagnostic;
they must not silently fall back to semantic decoding of `SZrInstruction`.
Emitter contract hashes include the target, strict-floating mode, and both
runtime-bridge and interpreter-fallback policies, so changing a permitted
degradation path cannot reuse an artifact identity from another policy.
The lowering result separately publishes a target-independent `loweringHash`;
C and LLVM results must agree on it while their target-specific contract hashes
may differ.

The focused fixture is
`tests/parser/test_ssa_aotir_contract.c`.  It exercises deterministic hashing,
contract validation (including multiple phases per resume ID, deep map
contents, malformed ranges and mismatched identities), and
relocation rejection.  CMake registers it as `ssa_aotir_contract`; run it with:

```text
ctest --test-dir build/ssa-gcc-debug -R '^ssa_aotir_contract$' \
  --output-on-failure --no-tests=error
```

`tests/parser/test_ssa_aot_callable_abi.c` exercises the explicit declaration,
unknown and mismatched fail-closed cases, hash sensitivity, owned projection
transfer, and backend adapter qualification. CMake registers it as
`ssa_aot_callable_abi`.

`tests/parser/test_ssa_aot_projection_branch_descriptor.c` builds verified
two-block ExecIR, lowers it with the legacy `UNKNOWN` ABI, and checks the
descriptor's owned edge pool, reversed predecessor range, module validation,
malformed-edge rejection, and transactional replacement. CMake registers it as
`ssa_aot_projection_branch_descriptor`.
