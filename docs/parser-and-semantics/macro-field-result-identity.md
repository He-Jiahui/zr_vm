---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - tests/parser/test_syntax_hygiene_field_results.c
  - tests/parser/test_compiler_features.c
  - tests/module/test_module_system.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - tests/parser/test_syntax_hygiene_field_results.c
  - tests/parser/test_compiler_features.c
  - tests/module/test_module_system.c
  - tests/parser/test_syntax_hygiene_receiver_members.c
  - tests/parser/test_syntax_hygiene_inherited_members.c
  - tests/parser/test_syntax_hygiene_generic_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_definitions.c
  - tests/parser/test_type_inference.c
doc_type: module-detail
---

# Macro Field Result Identity

Selecting the right receiver prototype and member is only one part of field
inference. The resulting field value must retain its own declaration identity,
which can be different from its owner's identity. This is a lower support gate
for M1 hygiene, not complete macro generation or constructor acceptance.

## Original Loss Boundary

Ordinary STRUCT_FIELD and CLASS_FIELD projection in type_inference_native.c
reconstructs its value from fieldTypeName. Source field annotations retain an AST
pointer, while closed generic copies substitute only the displayed field name
when no structured result is present. Correct field/member selection therefore
does not itself prove retention of an opaque nominal field type.

The existing member record has an owned structuredReturnType slot and flag used
by methods and properties. Generic prototype copying already deep-substitutes
that slot, and member destruction frees it for all member kinds. This is a
shared storage boundary. The repair below uses it without adding a public ABI.
Field publication must preserve fieldTypeName's presentation/layout role and
must not invent a method returnTypeName for a field. AST-null imported metadata
requires its established name-only fallback; source ASTs cannot become imported
metadata's borrowed lifetime dependency.

## Genuine Producer Fixtures

The twenty-case fixture uses three actual declaration producers: extern struct
metadata registration without loading a native library, ordinary struct
compilation, and ordinary class compilation. It publishes Root and opaque
same-spelling Widget declarations and marks the parsed Holder.value type
annotation. Before field projection it proves both exact source bindings and
distinct nominal IDs, the field annotation's converted token/canonical ID,
the intended Holder prototype and actual field AST pointer, and the receiver's
contextual variable and inferred identifier type.

Root and mixed-site ITEM contexts are controls. Marked-first source order and
an independently marked Holder context distinguish field identity from receiver
identity. Direct member-chain and public expression entry points consume the
same genuine metadata; no constructor object, injected member or fabricated
canonical identity is used to make a result appear.

The generic cases publish actual Parcel<T> with a T-valued field, materialize
the Root Parcel<Widget> cache first, then mark the parsed Widget argument.
They prove its canonical argument identity and selected instance key, including
key inequality for an opaque argument, before consumption. Closed metadata
continues to borrow the original T field AST, so merely reconverting that AST
at the consumer is insufficient to perform closed substitution.

## Ownership And Validation

Eight initializer companions additionally parse `var seed: Widget` and an
unannotated `pub var value = seed` in actual source struct/class producers.
They publish both same-spelling Widget declarations, mark only the seed type
annotation, and prove the stored variable binding and independently inferred
initializer have the expected canonical identity before compiling Holder.
Root/marked contexts and direct/public consumers are covered. No constructor
execution or synthesized member metadata is used; the real fieldType is null.

Parsed ASTs remain alive until compiler teardown. Inferred types and the current
function are freed before compiler ownership, followed by AST, context table
and runtime cleanup. All binding values that survive publication are copied;
prototype/member pointers are not kept across a potential materializer growth.

Native RED-r1 naturally terminates with build exit 0 and test exit 11: twenty
cases, nine PASS, eleven FAIL and zero ignored. All source, annotation, canonical
identity, selected member and receiver prerequisites pass. Failures occur at
the result token check: expected the opaque definition token, received Root 0.
The raw logs are `m1-msvc-field-results-red-r1-*` under the external reports
directory. Independent fixture review finds no false RED or ownership issue.

At the original twenty-case RED, no production field-value lease was granted.
The original lower
function-type conversion failure left its parameter arrays owned by no caller.
That lower gate is now GREEN: nine lifetime cases and 401 related cases pass after
the private signature cleanup repair. The upward field RED-r2-after-converter
still naturally terminates1/build0/test11 with the same nine PASS/eleven FAIL.
After prototype lifecycle cleanup passes, RED-r3-after-prototype-lifecycle
native4010 has that same twenty-case result. The new RED-r4-initializers
native98555 naturally terminates1/build0/test15: twenty-eight cases, thirteen
PASS, fifteen FAIL, zero ignored. Four additional Root controls pass; all
marked initializer prerequisites pass and only final result token checks fail.
Independent full fixture review finds no actionable issue.
Explicit
annotation capture, initializer-inferred fields, generic substitution, failed
publication, imported metadata and later recompilation remain separate checks.

The dedicated [function type ownership](function-type-conversion-ownership.md)
gate records that lower primitive's allocator evidence independently.
The [prototype metadata ownership](type-prototype-metadata-ownership.md) gate
now independently proves five source failure/recovery leaks after both valid
publication controls pass. Its fourteen-case lifecycle fixture and 401 related
cases now pass, with independent production review and complete raw inverses.
Root therefore grants a seven-path field capture/substitution/projection lease:
three producers, two new private field helper files, generic prototype copying
and the shared native field projection. No field production GREEN is claimed.

## Frozen Repair And First Replay

The new private field helper converts actual annotations into owned inferred
types and transfers successful initializer results into the existing structured
slot. The source struct/class and extern struct producers call it before member
publication. Failure frees the temporary, pending member and unpublished
prototype; source producers also restore their previous current-type state.
The helper does not set returnTypeName or change display, layout or qualifiers.

Generic prototype copying deep-substitutes this slot. For fields, its substituted
display updates fieldTypeName, never callable returnTypeName. Native field
projection deep-copies owned metadata, retains the established ownership and
GC-bridge overlays, and uses the prior name fallback only when the slot is
absent. Source AST ownership and imported name-only metadata remain separate.

Root independently reconstructs all five pre-lease raw file hashes in memory;
only the new hooks, copy selection and generic field display assignment are
removed by those inverses. Existing mixed line endings and all unrelated bytes
match. No source snapshot or restoration is written to the filesystem.

The first replay, `m1-msvc-field-results-green-r1-*`, naturally closes with driver
exit 1/build exit 0: fifteen programs, 429 cases, 428 PASS, one FAIL, zero ignored.
All twenty-eight field cases pass. The existing const-generic Matrix producer
fails at test_type_inference.c:5100 because eager annotation conversion evaluates
the still-open `N` in `Array<T>[N]` and reports an unknown compile-time identifier.
This is a new lower field-capture regression, not a constructor failure and not
accepted GREEN. Production remains frozen while the open-parameter contract is
diagnosed. Nested-owned field results and full failure/recovery cleanup still
require their own companions. The earlier three native unused-parameter warnings
and existing build-option/path warnings are not newly repaired or waived.

The separate [open array annotation](open-array-annotation.md) gate diagnoses
that shared prerequisite and records its narrow three-source repair. Four new
post-repair companions parse a real T[] field, prime the Root generic cache,
then convert an independent expected Widget[] annotation with the selected
argument context. They check the closed member and result canonical IDs,
different owned element-array storage, and successful inference again after
freeing the first result. Independent fixture review finds no issue. Upward
`m1-msvc-field-results-green-r2-nested-*` naturally ends build0/test0: thirty-two
field cases and fourteen related programs total 433 PASS, zero FAIL, zero ignored.
Root independently verifies all fifteen unique footers. The four new companions
are post-repair coverage, not prior RED evidence. The existing Matrix producer
also passes after the separate lower array repair.

The prototype lifecycle fixture also adds ten post-repair companions: actual
source struct/class and extern struct success, invalid annotation and distinct
declaration recovery, plus abstract/final class rejection after array capture.
They pre-prove int[] and fn(int[]) -> Missing conversion, then retain the strict
ARRAY ledger through full runtime teardown. The owned prior int[] field and
temporary callback-conversion buffers are tested separately. Current closure
annotations retain signature display, not nested parameter/return children;
these companions do not claim retained closure signatures or callability.
The independent twenty-four-case metadata replay and nine-case function
conversion replay also pass under native42537, all builds/drivers0.

## Ownership Target Prerequisites

Full native92840 naturally ends with driver exit 1: all original 126 targets
build, all 46 foundation CTests pass, and the remaining 121 programs report
108 passing and 13 failing programs. The compiler and module programs have
three new failing cases, despite unchanged program exit classifications.
Field capture reaches the ordinary annotation converter, which recursively
validates the inner type of Unique<T> and Shared<T>. All three legacy sources
use Resource without declaring or importing it. Resource is not an implicit
builtin, and ownership wrappers have no undeclared-inner-type exemption.

The log comparator originally missed CRLF Unity failure rows and the harness's
two-line ZR_TEST_FAIL output. Its LF/CRLF and custom-failure self-tests now pass;
all 121 logs reconcile parsed failure records with footer failure counts.
The corrected comparison identifies exactly the two ownership metadata cases
and test_module_restores_owned_field_prototype_metadata as new failures.
The three new failures are not accepted or waived.

The fixtures now declare an actual public resource class Resource before the
two original types. This adds one serialized prototype, so compiled and runtime
creation counts become three. Compiler cases check the named resource class,
resource flag and zero declared members; the runtime case checks its exported
class, resource flag and zero managed fields, not all runtime member descriptors.
All original ownership, display-name, destructor, field-order and layout checks
remain unchanged. Independent exact diff review accepts the prerequisite repair.
Targeted native28620 naturally ends with driver exit 1 and both builds successful:
the compiler program returns to 127 tests/13 failures, the module program to
78 tests/2 failures, and all three corrected cases explicitly pass. These are
not passing programs; their original failures remain open. Full native18591
naturally ends with driver exit1 and both build stages successful under
`m1-msvc-owned-field-prerequisites-final-r1-*`: all46 foundation CTests pass,
and the original121 programs remain108PASS/13FAIL. The corrected comparison
naturally ends0: all121 exits, footers, failed names and non-pointer messages
match the inherited-members baseline, with no missing footers or unaccounted
failures. Both modified test hashes and field/array source pins remain unchanged.
The three new fixture failures are repaired. Production remains unchanged.
None of these focused results accepts M1 or waives existing failures or platform
gates.

## Enum Member Canonical Facts

The existing enum producer represents runtime members as `CLASS_FIELD`
metadata with a real `ENUM_MEMBER` declaration AST. Resolved member publication
accepts that representation without fabricating a class-field AST. The enum
name, raw syntax context, AST owner and identifier-leaf range evidence are
checked before publication. The canonical symbol remains `FIELD`, with the
actual enum member declaration, native identifier definition range and valid
inferred `TypeId`. Existing same-declaration canonical symbols are reused.

Normal extern struct fields also retain their real field declaration AST when
their annotation, structured result and layout metadata are published.
Bootstrap completion uses that producer, not a separate preview-only metadata
builder. Cleanup owns inferred types while continuing to borrow the ASTs.

The native-range companion parses a real TokenStream, predeclares its enum and
calls public expression inference. It requires exactly one resolved canonical
member-use fact and the owned name-leaf target, not a hover fallback. A fresh
CompilerState companion checks both real extern struct field ASTs after proving
the normal metadata types, sizes and offsets. Both have genuine pre-repair RED.
The subsequent normal GCC96 repaired replay passes all cases with identical
source/HEAD and other input closures (receipt62CD3602); broader gates remain.

See [receiver member identity](macro-receiver-member-identity.md) and the
[validation ledger](../testing-and-validation/macro-foundation-validation.md).
