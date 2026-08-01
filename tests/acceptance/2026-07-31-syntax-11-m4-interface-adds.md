# Syntax 11 M4 typed Patch interface additions acceptance

Date: 2026-07-31

Scope: the published non-empty `DeclarationPatch.interfaceAdds: TypeId[]`
operation only. This record does not promote Gate 11 M4 or M5 as a whole.

## Contract

- Every entry must be a canonical reflection `TypeId` whose identity resolves
  to an interface prototype.
- A Patch may add at most 10,000 interfaces. Zero identities, duplicate TypeIds,
  repeated Patch entries, and relations already present through direct names,
  type-value aliases, or module-qualified aliases are rejected before mutation.
- Selected interface declarations are bound during Signature, before Expansion,
  so source order does not make a later interface unavailable to a transform.
  The same AST node is skipped during the later top-level pass rather than
  compiled twice.
- Patch diagnostics are processed before interface mutation. An error
  diagnostic therefore leaves both generated members and interface relations
  unapplied.
- Successful additions enter both canonical `inherits` and `implements`
  metadata, then use ordinary class or struct interface checks. Required member
  signatures, receiver effects, const fields, recursive parent interfaces, and
  interface contract slots are validated before type publication/layout.
- Native temporary arrays are released on every executor exit, including the
  existing-member snapshot allocation failure path.

## TDD and review evidence

1. Initial interface-add RED: the positive class case was the only new failure
   because the executor rejected all non-empty `interfaceAdds`.
2. Initial GREEN: canonical TypeId decoding, budgets, duplicate checks, Patch
   validation, relation application, required-member validation, and contract
   slot binding passed in the focused executable.
3. Phase-order RED: a transform naming an interface declared later in the same
   module failed with `TypeId must resolve to an interface`.
4. Phase-order GREEN: selected interface signatures are now prebound before
   Expansion; the later-declaration case passes without double compilation.
5. Independent review found three slice defects: struct targets bypassed the
   ordinary contract, an allocation failure returned past cleanup, and existing
   interface aliases were compared by spelling instead of canonical identity.
6. Review RED reproduced the semantic defects as exactly two failures in the
   48-case executable: a missing struct member was accepted and an alias of an
   existing interface was accepted as a second relation.
7. Review GREEN adds isolated value-type interface validation, canonical
   alias-aware comparison, and cleanup-only exits. Positive and negative struct
   transform paths, class required members/const fields, later declarations,
   invalid TypeIds, explicit and alias duplicates all pass.
8. Final review found that recursive parent validation retained a prototype
   array element pointer across a lookup that may materialize imported/generic
   prototypes and reallocate the array. The implementation now snapshots only
   the count, reacquires the prototype by stable name each iteration, copies the
   GC-stable parent name before recursion, and rejects unresolved/non-interface
   parents. The final read-only review reports no remaining P1/P2 findings.
9. Added coverage proves local recursive parent success/failure, struct
   receiver and const failures, illegal parent type rejection, persisted struct
   contract slots, and imported parent materialization after prototype growth.

## Fresh WSL GCC and Clang evidence

Both build trees are temporary Debug/Ninja builds under `/tmp`, configured with
`BUILD_DEBUG_LIB=OFF` and tests enabled. They compile the mounted repository
source; unrelated worktree debug-library files are excluded by target and build
configuration.

```text
cmake --build /tmp/zr-vm-gate11-interface-build \
  --target zr_vm_compile_time_test zr_vm_comptime_contract_test \
  zr_vm_attribute_contract_test zr_vm_declaration_transform_contract_test \
  zr_vm_comptime_runtime_contract_test zr_vm_compiler_integration_test -j4

cmake --build /tmp/zr-vm-gate11-interface-clang-build \
  --target zr_vm_compile_time_test zr_vm_comptime_contract_test \
  zr_vm_attribute_contract_test zr_vm_declaration_transform_contract_test \
  zr_vm_comptime_runtime_contract_test zr_vm_compiler_integration_test -j4
```

GCC 11.4 and Clang 14 both pass:

- compile-time execution: 51/51;
- comptime contract: 2/2;
- attribute/descriptor contract: 3/3;
- declaration-transform contract: 5/5;
- comptime runtime contract: 10/10;
- focused total: 71/71;
- compiler integration: 127/127.

## Remaining Gate 11 blockers

- non-empty `attributeAdds`;
- `GeneratedType`, `GeneratedMethod`, and `GeneratedProperty` execution paths;
- generated source maps and transactional rollback across multi-add failure;
- artifact, reflection, LSP, formatter, build dependency and migration
  consumers;
- removal of the legacy runtime decorator path.
