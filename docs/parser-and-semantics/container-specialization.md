---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_container_specialize.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c
  - zr_vm_core/include/zr_vm_core/container_storage_contract.h
  - zr_vm_core/src/zr_vm_core/object/container_storage_contract.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_container_specialize.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c
plan_sources:
  - docs/plans/ssa/05-data-layout/03-maps-strings.md
tests:
  - tests/parser/test_ssa_container_specialize.c
doc_type: module-detail
status: implemented-subset
---

# ExecIR map and string container specialization

## Scope

The parser-side contract is an admission boundary for the scalar map and
string storage witnesses in `zr_vm_core`.  It does not allocate a container,
retain a managed object, or install a callback.  The current ExecIR schema has
no container-specific opcode or payload slot, so the pass builds a value-only
plan and leaves instruction storage untouched.  A later lowering pass can
consume a validated plan when such an opcode is introduced.

## Facts and plans

`SZrContainerSpecializationFacts` carries function/source scope, generation
and optional IR hash together with `SZrCompactMapCandidate` and
`SZrStringStorageFacts`.  The explicit `mapKnown` and `stringKnown` bits are
authoritative: an absent layout is uncertainty, not permission to guess a
physical representation.  `SZrContainerSpecializationPlan` records the
selected map (`GENERIC` or `COMPACT`) and string (`GENERIC`, SSO, intern,
builder, or rope) strategies, copies only scalar candidates, and seals the
snapshot with a deterministic hash.

`ZrParser_ExecIr_BuildContainerSpecialization` validates the function scope
and facts before consulting the core contract.  Stable map candidates may
use cached hashes only when the core hash/equality/domain witness permits it;
hash collisions still require the language equality operation.  String
selection delegates to the existing intern/SSO/builder/rope gates, including
escape, identity, effect-equivalence, and flatten-budget checks.  Unknown or
unsafe candidates become a generic fallback with a precise reason.  Callers
may set `REQUIRE_MAP` or `REQUIRE_STRING` to turn a missing proof into a hard
failure, while the default keeps the baseline implementation available.

`ZrParser_ExecIr_SpecializeContainers` is the draft plan entry point from
05.03.  It returns true only when at least one non-generic candidate is
admitted; false for an unknown/unsafe-only input is an optimization miss, not
a language error.  Its `SZrExecIrDiagnostic` still carries the source and
IR scope and the generic path remains valid.

## Safety and lifecycle rules

All persisted fields are fixed-width integers, booleans, enums, or the scalar
core witnesses.  No pointer, host address, object identity address, or
callback is copied into a plan.  Function storage and optional generation/IR
hash bindings are checked before planning; a sealed or stale function is
rejected.  Plan validation rebuilds the expected scalar plan and compares its
hash, preventing a nested candidate from being forged independently of its
facts.  Since this slice has no ownership transfer or allocation entry point,
failure has no partial object to roll back and leaves the generic runtime
path untouched.

## Focused evidence

`tests/parser/test_ssa_container_specialize.c` covers stable map plus builder
admission, unknown-layout generic fallback, custom equality and identity
escape rejection, sealed functions, stale plan hashes, and the draft entry
point's no-rewrite behavior.  The fixture is standalone so it can be compiled
against the two contract sources without changing shared CMake registration.
The integration owner should register it in the parser/SSA test manifest
after the shared build graph is ready.

## Follow-up

When a container opcode and lowering storage are added, that pass should use
this contract as a proof gate and preserve `preservesGenericEquality` and
`irUnchanged` semantics.  Collision/equality ordering, deletion tombstones,
Unicode content, allocation measurements, and cross-domain hash rekeying
remain runtime responsibilities covered by the core storage contract.
