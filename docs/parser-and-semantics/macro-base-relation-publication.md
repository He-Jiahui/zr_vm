---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_use.c
  - zr_vm_parser/include/zr_vm_parser/semantic_type_use.h
  - tests/parser/test_syntax_hygiene_base_publication.c
  - tests/parser/test_syntax_hygiene_base_contract_cases.h
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_syntax_hygiene_base_publication.c
  - tests/parser/test_syntax_hygiene_base_contract_cases.h
  - tests/parser/test_syntax_hygiene_inherited_members.c
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/parser/test_nominal_declaration_publication.c
doc_type: module-detail
---

# Macro Base Relation Publication

This M1 support gate checks the semantic BASE relation produced by an actual
class declaration. It is separate from receiver lookup, inherited member
traversal, virtual slots, property identities and physical layout.

## Boundary

The pre-repair class producer extracts the displayed name of an inheritance
annotation and looks up a Root prototype before publishing its hierarchy
relation. Displayed spelling alone does not identify an opaque definition.
The same Base spelling may refer to a Root class or a class defined at another
syntax context. Publishing the wrong relation would give semantic queries the
wrong target even if the annotation converter itself selected the right type.

The required contract is to preserve the actual annotation's item-namespace
identity when selecting the prototype passed to the existing relation publisher.
Root and item-normalized mixed-site annotations retain established Root lookup
precedence. Opaque contexts must not silently retry the same spelling in Root.
Invalid raw contexts fail closed. The derived declaration has its own independent
context; it does not supply or replace the base annotation's context.

## Real Producer Fixture

The eleven-case fixture parses either two or three empty classes, marks their
actual declaration identifiers, and publishes each real Base through the normal
class compiler. The opaque-only source contains no spare Root Base declaration.
The following positive cases use actual inferred types and bindings:

- Root-only and opaque-only Base declarations.
- Root then opaque, and opaque then Root, each selecting Root or opaque.
- Mixed-site annotation selecting the Root item identity.
- Two distinct opaque definitions selecting the second.
- Independently marked Derived and Base declarations.

Before compiling Derived, each positive case converts its actual inheritance
annotation, verifies the selected binding's canonical type and definition token,
and finds the exact source prototype through the nominal lookup primitive.
It then compiles the real Derived and requires one BASE relation with exact
source and target symbol/type IDs. Both semantic symbols must be TYPE symbols
with the expected source ASTs. No relation or prototype metadata is fabricated,
and no failed query is cleared and replayed through an alternate path.

Two negative cases keep a valid Root Base while giving the annotation a missing
opaque binding or an invalid raw context. They compile Derived directly, require
an error, no new prototype or BASE relation, and restoration of the three current
type-state fields. The producer may retain its earlier named binding; these
tests do not promise semantic rollback. The retained provisional binding must
still have valid IDs and the exact Derived AST/TYPE symbol before an empty BASE
query counts as evidence. State restoration uses the initial top-level state;
nested non-null current-type restoration is separate coverage.

Empty classes may still record the normal default Object inheritance. The
fixture checks zero declared members, not zero inheritance entries. Binding
values are copied before later publication may grow arrays; borrowed prototype
and symbol pointers are not reused across such growth. ASTs and context tables
remain alive until compiler teardown, including on Unity assertion failure.

Five compatibility companions extend the fixture to sixteen cases. The Root
forward case compiles Derived before Base and requires successful publication
with no immediate BASE edge; it does not manually replay final contracts. The
plain opaque alias uses `compiler_register_type_value_alias`, copying a carrier
converted from a real parsed `Evidence : Base` annotation and checked against
the published Base canonical identity. Its parsed `let Chosen = Base` initializer
is an identifier, not a TYPE_LITERAL; the case does not claim automatic alias
registration from that ordinary initializer. The Root generic alias case uses
actual parsed `Base<Widget>` TYPE_LITERAL nodes, creates a Root instance first,
then a distinct instance whose argument has an opaque nominal identity. Owned
snapshots and scalar IDs survive subsequent cache growth. A direct selected
prototype-instance check is now authored for both the alias and a fourth,
direct `Derived : Base<Widget>` companion with an actual marked argument. These
post-RED assertions have no prior RED credit; the BASE endpoint alone identifies
only the open Base declaration. A fifth Root `Derived : Base<int>` forward
companion requires successful helper and normal producers without assuming an
immediate prototype count or relation endpoint.

## Current Status

The frozen candidate adds `compiler_resolve_type_annotation_prototype` to the
existing 511-line lookup module; class compilation retains only orchestration.
The helper returns status separately from its borrowed nullable prototype.
Raw and normalized named ITEM contexts are validated before lookup, including
recursive named generic arguments under the existing depth bound. LOCAL alias
normalization failure is distinguished from absence, and an alias's inferred
carrier is copied before any generic materialization can grow tables.
The checked alias lookup uses the complete annotation spelling, so an array or
qualified annotation cannot match a stored bare alias just by its first name.
After checked LOCAL absence, ITEM Root fallback disables only the initial Root
value-alias query. This prevents mixed-site references from reopening an invisible
Root LOCAL alias. Exact items, imports, generic materialization and valid forward
absence retain their existing paths. The public name-only wrapper still enables
the initial alias query, including its existing recursive/module behavior.

Bare Root/mixed ITEM names keep compiler exact-first precedence. Plain token-zero
alias targets use the compiler name wrapper. Direct generic annotations convert
their actual argument ASTs before cache lookup; marked keys require structured
selection. Compatible complete Root instance keys then select the first compiler
registration by exact `genericInstanceTypeId`, preserving imported-null priority.
Direct Root generic forwards can succeed with no prototype only after successful
conversion, checked absence of a published Root base and no materialization error.
Bound structured-alias forward deferral is not covered by this candidate.

The class handoff immediately shallow-snapshots the selected record before BASE
publication. The snapshot borrows metadata and must not be freed. Failed
resolution preserves the original diagnostic, frees only unpublished owned
metadata, and restores all three saved current-type fields. Named bindings are
not transactionally rolled back. The old name-only API, module/member display
builder, and later slot/layout/contracts paths remain unchanged. Root's in-memory
inverse reconstructed every pre-lease byte and SHA for all three files; no
filesystem source copy was created.

The seventeen-case frozen fixture adds an imported-null precedence control by
transferring real source closed metadata ownership to a later slot and replacing
the earlier slot with zero-array borrowed import metadata bearing the same real
instance key. This is explicit synthetic compatibility setup, not a claim about
artifact import lifetime or a fabricated successful Derived BASE path.
GREEN-r1 native16490 naturally exits0/build0. Root independently accounts for15
footers and418PASS0FAIL0ignored:17 direct and401 related. Independent peer then
identifies two missing boundaries. Five real companions bring the fixture to22:
Root bare alias, matching mixed LOCAL alias, mixed LOCAL miss with a visible Root
alias, direct Root Base[] compatibility, and Root Chosen[] outer-shape preservation.
The array cases first require actual ARRAY conversion/canonical identity and a
Base element, plus the old complete-name lookup returning no nominal prototype.
RED-r5 native19247 naturally exits1/build0/test2,22=20PASS2FAIL0ignored. Only the
mixed LOCAL miss and wrapped alias actual producer assertions fail, each observing
one wrong BASE edge. The lookup.c-only follow-up GREEN-r2 native52095 naturally
exits0/build0:15 unique footers423PASS0FAIL0ignored,22 direct plus401 related.
Header/class pins remain unchanged. This accepts only the tested producer boundary.
Root's independent six-fragment in-memory inverse matches every pre-follow-up
byte and SHA. No source copy or snapshot is written.

Native RED-r1 (session97142) naturally exits1, build0: eleven tests, seven
failures, zero ignored. Four Root/mixed-site controls pass; five opaque positive
cases and both negative cases fail at the intended BASE publication assertions.
Production GO is restricted to lookup `.c/.h` and the class producer after the
corrected RED-r4. RED-r2 stops at a test include path build error,
with no behavioral credit. Corrected RED-r3 builds and runs14/8: seven intended
failures and one plain-alias prerequisite failure (metadata-only class compilation
does not create a value-variable binding). Root corrects that prerequisite to
the real annotation conversion described above. RED-r4 (session48817) naturally
exits1/build0: fourteen tests, eight failures, zero ignored; all eight fail at
the intended BASE assertions after real prerequisites. Root-forward and generic
alias controls pass. Root adds the direct helper selected-instance assertion
after this RED; that new assertion has no prior RED credit. The preceding field/array support replay
passes all46 foundation CTests and introduces no changes in any of the original
121 program exits, footers or failure messages, but M1 remains unaccepted.

Forward base materialization, imported bases, generic bases, interfaces,
contextual aliases, normalization-budget fault injection, and downstream member
or layout behavior are separate coverage. A base helper must preserve existing
Root forward behavior and alias support, including a contextual alias to a Root
type with definition token zero. A nonzero-token requirement on every opaque
annotation is not valid proof of identity.

## Post-Contract Replay Gate

`ZrParser_SemanticRelations_PublishCompilerContracts` traverses prototype
inheritance names in `compiler_semantic_relations.c` and republishes them through
Root lookup. Eight companions extend the fixture to30 and invoke this real
publisher twice after actual producer checks. They cover Root/opaque targets,
both same-name orders, late Root forward publication, mixed LOCAL alias absence,
private alias-storage cleanup and real closed-source metadata. RED contracts-r1
native34192 naturally exits1/build0/test3:27 controls PASS; both same-name cases
gain a wrong Root BASE edge and the mixed LOCAL miss captures a Root alias.

Two more cases bring the fixture to32. The alias case proves no TYPE reference
fact exists for the Derived annotation before real compilation, then requires
the producer to persist its exact resolved Base TypeId/SymbolId. It truncates the
real alias store and re-queries facts and BASE endpoints after cleanup and each
normal replay, without retaining borrowed facts over publication. Plain
identifier conversion currently does not publish this TYPE fact; the public
converter's TYPE-use publication is limited to generic annotations. The test
therefore isolates producer provenance, not a previously demonstrated fake fact.
The second case compiles a real Known interface and Derived with Known plus a
forward class; Known-only publication must become exactly Known+Forward twice.
It prevents a repair from skipping all inheritance whenever any BASE exists.
RED contracts-r2 native84649 naturally exits1/build0/test4:28 controls PASS;
the added failure is the missing producer TYPE fact after successful exact BASE
publication. The multi-target forward companion passes. The later strengthened
fact-requery and interface prerequisites have no separate RED credit.

The intended bounded repair reuses existing semantic TYPE-use facts and the
actual annotation, rather than adding public prototype metadata or retrying
displayed spelling in Root. The bounded producer/replay candidate is frozen at
lookupB512AF86/relations54790C7C. Expired aliases, closed generic
owners, valid forward absence and imported-null precedence must remain intact.
The post-contract cases are extracted into a private `_cases.h` using the local
test pattern; Root's RAM inverse proves all original raw characters and SHA
unchanged by the extraction. Two further compatibility cases bring the registered
inventory to34: imported-first generic NULL-target replay, and an actual forward
generic interface with a typed method, exact raw instance key, substituted `int`
return metadata and BASE/IMPLEMENTATION endpoints after two normal replays.
These post-RED controls have no claimed pre-repair native result. GREEN contracts-r1
native15856 naturally exits0/build0;15 unique footers contain435PASS0FAIL0ignored,
including all34 direct cases and401 related checks. Both generic controls pass.
Root independently reconstructs both complete preimages via eight RAM inverse
fragments; no filesystem source copy is created.

Independent review identifies optional TYPE-use publication as a separate
boundary: the actual parser accepts a null source name and produces nonempty
positions, while navigation TYPE publication requires a nonnull source. The
35th case uses that real parser input without modifying locations, proves
navigation publication returns false with no appended fact, then requires the
successful exact BASE to survive alias cleanup and two contracts replays.
The complete native67316 RED3 log records35 tests/1 failure/0 ignored, with all
prior34 passing. Only the first contracts replay fails after successful producer
prerequisites. The exec handle is now missing; its natural exit code was not
observed and is not claimed. A lookup-only follow-up must retain the selected
canonical identity without inventing navigation ranges or retrying Root names.
The lookup-only follow-up is frozen at6F81AD3E (598LF/0CR); relations54790C7C,
header and class stay unchanged. It first uses the normal navigation publisher,
then reuses or appends an idempotent range-free TYPE fact for the actual selected
canonical TypeId and unique declaration SymbolId. Conflicting IDs are not
overwritten, raw generic instance IDs are not folded, and no definition range
is invented. Root's five-fragment RAM inverse hashes the full reconstructed
23923 bytes to priorB512AF86. Additional post-RED assertions require exact IDs,
raw syntax context, no navigation ranges and one fact across cleanup/replays;
they have no separate prior RED credit. Fresh GREEN contracts-r2 native94400
naturally exits0/build0:15 unique footers,436 tests/436 PASS rows/0 failures/0
ignored (35 direct plus401 related). All four production/test pins remain
unchanged. Independent review finds no new blocking identity, lifetime,
conflict or idempotency issue. Full native replay59266 naturally exits1 with
original126 pipeline build0, foundation build0 and47/47 PASS. Aggregate exit8
retains the known diagnostic catalogue failures; remaining121 programs are
108 PASS/13 FAIL. Comparator36925 naturally exits0 and checks all121 exits,
footers, failed names and non-pointer messages: zero changes, missing footers
or unaccounted failures. This bounded repair introduces no original-program
regressions. Original failures and remaining M1/platform gates are not waived.
The range-free fact does not represent a navigable source range. Current query
APIs can still match an all-zero fact with an all-zero no-source sentinel query;
actual parsed cursor positions have positive lines and cannot match that range.
This is not a claim of universal invisibility to every position query, and the
query API is not changed by this repair.
Closed-generic BASE endpoints currently identify the open declaration; the edge
alone does not prove preservation of its structured argument key.

See [prototype lookup](macro-compiler-prototype-lookup.md),
[receiver identity](macro-receiver-member-identity.md), and the
[validation ledger](../testing-and-validation/macro-foundation-validation.md).
