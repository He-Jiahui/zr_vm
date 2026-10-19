---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_member.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_member.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - tests/parser/test_syntax_hygiene_receiver_members.c
  - tests/parser/test_syntax_hygiene_inherited_members.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_member.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_member.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_syntax_hygiene_receiver_members.c
  - tests/parser/test_syntax_hygiene_inherited_members.c
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/parser/test_syntax_hygiene_generic_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_definitions.c
  - tests/parser/test_type_inference.c
doc_type: module-detail
---

# Macro Receiver Member Identity

Receiver inference must carry the declaration identity and ordered generic
arguments through field and method selection. A correct TYPE binding or
specialization cache does not prove that its consumers retain this identity.
This module documents the ordinary member-chain support boundary, not the
complete macro system or constructor emission.

## Producer And Consumer Boundary

The fixture parses actual source declarations. Extern struct publication produces
field metadata without loading or executing a native library. Ordinary struct
compilation produces real method functions, parameter types, return types and
declaration pointers. Extern publication does not produce methods or generic
parameter metadata, so it cannot serve as their fixture producer.

Every receiver check first proves the contextual TYPE binding and source AST,
canonical nominal token, converted annotation, keyed prototype and intended
member metadata. It then registers the parsed variable through the existing
contextual environment API and checks identifier inference's retained token.
Only after these prerequisites does it call the ordinary member-chain helper
or public expression inference. No receiver constructor or runtime object is
needed to isolate this support layer.

## Lookup Contracts

A receiver with a nonzero nominal token or structured generic arguments is an
explicit type reference. Its member lookup selects the structured prototype;
an invalid identity cannot recover a same-spelling Root member. Missing members
retain the existing successful unknown-OBJECT inference behavior, not a newly
invented diagnostic.

The private receiver APIs preserve the existing name-only field/call interfaces
as Root compatibility wrappers. Ordinary Root lookup, mixed-site ITEM
normalization, source/import precedence, relation traversal and hidden property
accessor behavior must remain unchanged. Relation traversal still uses the
existing first-local relation rule rather than replacing it with the nominal
finder's source-preferring rule.

Recursive field and overload lookup resolve each related prototype before
rejecting a self-cycle. Equal display spelling is not evidence of a cycle.
Closed generic prototypes compare their canonical instance IDs; open and closed
forms cannot be equal merely because they borrow one declaration AST. Source
prototypes otherwise compare unique, valid TYPE SymbolIds found by exact AST
pointer in the current semantic context. If a unique valid source identity is
unavailable, including an unresolved or ambiguous symbol scan, declaration
equality is a conservative self-cycle guard, not a member-selection fallback.
Unkeyed metadata retains the legacy name guard and earns no cross-module
artifact-identity coverage. Field and overload
scanners retain their respective pre-existing related-prototype precedence.

Method lookup shares one overload scanner and diagnostic implementation for
selected and Root prototypes. Parameter ordering, generic signature resolution,
typed mismatch reporting, ambiguity checks and call binding facts remain on
the established path. Imported metadata keeps null declaration pointers; a
synthetic imported-null compatibility companion does not claim real binary
artifact acceptance.

## Lifetime Boundaries

Argument inference and generic signature resolution can add prototypes and
relocate the compiler table. A receiver's inferred type must be copied before
these operations. Selected prototype metadata is snapshotted or reacquired by
identity, and member/relation arrays are independently snapshotted before
recursive materialization. No table-element pointer may survive an operation
that can grow that table.

The test retains parsed ASTs until compiler teardown, frees inferred types and
the current function before compiler ownership, then releases AST/context/runtime
resources. Closed generic companions also check the surviving instance key.
A growth companion fills table capacity using real parsed const-generic
annotations. Inference of a type-literal argument then registers its reflection
metadata during actual call-argument inference.
It must observe capacity growth before receiving coverage credit. Its two
methods intentionally share signatures and results, so it proves lifetime
robustness only; distinct-signature fixtures separately prove owner isolation.

## Native Evidence

The immutable `m1-msvc-receiver-members-red-r1` driver naturally exits1 after
build0. Its original eighteen cases have 8 PASS, 10 FAIL and zero ignored.
Root/direct/public, mixed-site and Root-exclusion controls pass. Five field and
five method consumers fail only after all producer, annotation, metadata,
variable and identifier prerequisites pass: opaque-only receivers become
unknown OBJECT; same-spelling Root metadata supplies the wrong primitive field
or rejects the selected method's argument. Independent static fixture review
reports no actionable setup or ownership issue.

The production repair is limited to private nominal-member selection, shared
overload scanning and ordinary native receiver calls. Fresh green-r1 naturally
exits1 after build0: the original18 and fourteen generic/argument/invalid/import
companions pass, while two growth cases fail before observing the intended
boundary. All twelve related programs pass352 cases with zero ignored.

The growth fixture incorrectly treated `Growth<1000>.value` as a static field
access. Expression inference instead gives `Growth<1000>` the existing compile
type-descriptor role; its unknown `value` member is OBJECT and cannot satisfy
`read(int)`. This failed attempt remains immutable and earns no growth credit.
The corrected fixture uses a genuine untyped source method parameter and passes
the type descriptor directly. It still requires observed capacity/length growth;
no production change or weakened growth assertion follows this fixture error.
Corrected `m1-msvc-receiver-members-green-r2` naturally exits0 after build0:
all thirteen programs pass386 cases with zero ignored, including all34 receiver
cases. Both corrected growth cases observe capacity and length increases during
argument inference. The five production hashes remain unchanged. Independent
static review reports no additional direct-selection, cleanup, precedence or
overload finding, but flags same-spelling class inheritance as a separate
unproved candidate. Full `m1-msvc-receiver-members-final-r1` replay then naturally
exits1: both builds exit0, all41 foundation entries pass, aggregate exits8 and
the remaining121 original programs have108PASS/13FAIL. A structured comparison
to compiler-prototype-lookup-final-r1 finds zero exit/footer/failed-test-message
changes and zero missing footers, normalizing only memory pointers. Original
failures and unavailable WSL matrix remain acceptance obligations; neither the
focused result nor this unchanged baseline accepts M1.

## Separate Obligations

Nominal-valued field results still reconstruct from `fieldTypeName`, and generic
field metadata currently substitutes that spelling. Preserving the receiver
does not prove that the resulting field value keeps its nominal identity.
The genuine producer/annotation/result gate is tracked in
[field result identity](macro-field-result-identity.md).
The separate inherited-members fixture publishes actual class declarations,
checks unequal exact SymbolId/TypeId values and the actual direct BASE_TYPE
relation's endpoints, and proves the Root base annotation and base member
metadata. The derived prototype has no direct members; its receiver must obtain
the result through inherited field/method lookup. Root, opaque distinct-name
and mixed-site cases are companions for the opaque same-name candidate.
Inherited-members-red-r1 naturally exits1 after build0,14cases5PASS9FAIL/0ignored.
Its two same-spelling field consumers reach expected INT64 versus actual OBJECT
after all exact source/hierarchy/receiver prerequisites. Seven method cases
instead stop at an invalid fixture assumption: read(1) parses as a member plus
a call, not one member node. This failed attempt is preserved and earns no
method RED credit; only the node-count prerequisite is corrected for red-r2.
Corrected red-r2 naturally exits1/build0/test4:14cases10PASS4FAIL/0ignored.
All Root/mixed-site/distinct opaque companions pass. Only the four same-spelling
inherited field/method direct/public consumers get OBJECT rather than INT64,
after all actual source, canonical hierarchy and receiver prerequisites pass.
Independent fixture review finds no additional ownership/prerequisite issue.
The repair is confined to private recursive member/call self-cycle checks;
base-annotation resolution and field-value projection are separate obligations.
The stronger fixture also checks parsed member/call kinds and its integer
argument. A fifteenth companion publishes a real const-generic struct and
materializes two parsed closed annotations. It checks open/closed inequality,
different canonical instance IDs despite a shared AST, and identity equality
between a copied metadata snapshot and the reacquired same instance. These are
prototype-identity checks, not a claim that generic class inheritance is covered.
Fresh inherited-members-green-r1 naturally exits0/build0: all fourteen programs
pass401 cases with zero failures/ignored, including all15 inherited/identity
cases. Production hashes are unchanged, and independent static review finds no
actionable regression. Its MSVC build warns that two fixture snapshot variables
may be uninitialized despite the bounded two-iteration producer loop. Root
explicitly zero-initializes snapshots and repeats the same gate under green-r2;
no production change or weakened prerequisite follows that warning. Actual
cyclic hierarchy and AST-null metadata-cycle coverage remain unclaimed.
Green-r2 naturally exits0/build0 with the same401PASS/0FAIL/0ignored and no C4701
fixture warnings. Frozen production hashes are unchanged. Full immutable
inherited-members-final-r1 replay naturally exits1: both builds0, all42 foundation
entries pass, aggregate8, remaining121 original programs108PASS13FAIL. Exact
structured comparison against receiver-members-final-r1 finds zero exit/footer/
failed-test-message changes and zero missing footers. Root also independently
reconstructs all three pre-repair hashes in memory. Original failures and the
unexecuted WSL matrix remain obligations; this result does not accept M1.
Parsed hierarchy support is not assumed from a synthetic relation record,
constructor or hand-built prototype.
Computed/meta calls, qualified-type chains, constructor/compiler member emission,
artifact identities and cross-platform acceptance require their own gates.

## Reference Evidence

Rust's `Instance` couples its definition and generic arguments in
`lua/rust/compiler/rustc_middle/src/ty/instance.rs` rather than resolving a
displayed name again. `SyntaxContextData` keeps expansion ancestry and
transparency separately in `lua/rust/compiler/rustc_span/src/hygiene.rs`.
ZR follows its existing semantic symbols and canonical instance keys, while
retaining ZR relation and overload semantics.

See [compiler prototype lookup](macro-compiler-prototype-lookup.md),
[generic identity](macro-generic-type-identity.md) and the
[validation ledger](../testing-and-validation/macro-foundation-validation.md).
