# Syntax 11 M4 declaration Patch transaction acceptance

Date: 2026-08-01

Scope: atomic publication and rollback for the currently implemented
`GeneratedField`, `interfaceAdds`, and `attributeAdds` Patch operations.
This transaction record did not promote Gate 11 M4 by itself. Combined with the
generated source-map record cited below, the published first-version M4 surface
is proven; Gate 11 M5 remains open.

## Contract

- Patch shape, operation budgets, collisions, typed diagnostics, interface
  identities, and attribute schemas are validated before transaction setup.
- Every GeneratedField and its provenance metadata object is constructed before
  semantic-symbol or member publication.
- Native member, semantic-symbol, interface-relation, and decorator arrays are
  cloned into detached backing storage with their final capacities. Allocation
  failure never reallocates or overwrites a live target array.
- Attribute additions build a detached copy-on-write metadata object. Existing
  metadata fields are copied and verified before new attribute entries are
  added; the target metadata value changes only during final commit.
- A failed commit leaves member, symbol, inherits, implements, and decorator
  pointers, capacities, lengths, the next SymbolId, the metadata-presence flag,
  and the exact previous metadata value unchanged. The previous metadata object
  is never mutated. Successful publication consists only of no-fail swaps and
  scalar assignments after every observer has accepted the detached state.
- Detached strings and metadata objects are held by temporary GC roots until
  publication completes. Full GC during the generated and attribute stages does
  not collect unpublished state.
- Successful fields still enter ordinary semantic binding and layout. The
  transaction does not introduce a second generated-declaration representation.

## TDD and review evidence

1. Generated multi-add RED compiled the new test but failed only at link time
   because `ZrParser_CompileTime_CommitGeneratedFieldsAtomic` did not exist.
2. Generated GREEN moved field construction out of the oversized executor,
   prebuilt all fields into detached member/symbol arrays, and proved that a
   forced failure before publication leaves member/symbol state and
   `nextSymbolId` unchanged.
3. Allocator review added a real allocator wrapper that rejects the first
   native-array growth. The two-field transaction returns false, leaves no
   member or symbol visible, and succeeds on the following non-faulted commit.
4. Cross-kind RED failed only because
   `ZrParser_CompileTime_CommitDeclarationPatchAtomic` did not exist.
5. Cross-kind GREEN removed the executor's separate interface/attribute apply
   calls and routed all current Patch operations through one commit boundary.
6. Copy-on-write review preloaded target metadata, forced failure after the
   attribute stage, and proved that the exact old object and field remain while
   the generated attribute key is absent. A following successful mixed commit
   publishes one member, an interface relation pair, the attribute and transform
   decorators, a cloned old field, and a new attribute entry.
7. Generated provenance and attribute object setters root each temporary key
   through `ZrCore_Object_SetValue` and verify the stored key afterward.
   Transient `HASH_PAIR` allocation failures at the root-holder,
   generated-provenance, attribute-entry, and attribute-envelope sites force a
   full-GC retry and then preserve the expected metadata. A permanent managed
   allocation failure is a core memory exception and therefore fails closed
   rather than returning a false successful transaction result.
8. Fault, GC, backing-identity, overflow, and mixed-operation fixtures live in
   the transaction and hash-failure cases headers; the existing large
   compile-time runner owns only the includes and test registrations.
9. Review hardening makes both transaction entry points fail closed when the
   compiler state is absent, and rejects addition budgets or declaration-order
   values that cannot be represented before allocating publication state.
10. MSVC ASan identified a test-fixture use-after-free: a pre-existing metadata
    sentinel was referenced only by native test storage across forced full GC.
    Explicit public GC root handles now protect that sentinel and the post-GC
    lookup key; the same ASan run then completed all 66 tests with no report.

## Focused evidence

The initial GCC 11.4 REDs were exact undefined-reference failures for the two
new transaction entry points. Isolated GCC 11.4, Clang 14, and MSVC 19.44.35228
Debug builds then compiled and ran the same focused matrix:

```text
target                                            GCC       Clang     MSVC
zr_vm_compile_time_test                          66 / 0    66 / 0    66 / 0
zr_vm_comptime_contract_test                      2 / 0     2 / 0     2 / 0
zr_vm_attribute_contract_test                     3 / 0     3 / 0     3 / 0
zr_vm_declaration_transform_contract_test         6 / 0     6 / 0     6 / 0
zr_vm_comptime_runtime_contract_test             10 / 0    10 / 0    10 / 0
```

Each cell is `tests / failures`; every executable returned zero. Linux used
separate `/tmp/zrvm-syntax11-red-linux-build` and
`/tmp/zrvm-syntax11-clang-build` trees. MSVC 19.44.35228 used an isolated source
snapshot overlaid only with this slice's production/test files. A separate
`/Od /fsanitize=address` static MSVC build also ran
`zr_vm_compile_time_test` at `66 / 0` after the fixture-root correction and
attribute hash-retry coverage.

## Remaining Gate 11 blockers

- compiler sandbox/cache-key integration, formatter, and remaining M5 consumer
  acceptance; the phase-separated v2 build-dependency manifest/lock foundation
  is covered separately.

The first-version public descriptor intentionally exposes only
`GeneratedField`. `GeneratedType`, `GeneratedMethod`, and `GeneratedProperty`
remain absent until each independently passes the plan's public reference gate;
their absence is not an M4 blocker. Generated source-map projection is covered
by `2026-08-01-syntax-11-m4-generated-source-map.md`.
