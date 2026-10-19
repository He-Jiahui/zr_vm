---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_array_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_array_type.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_substitution.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor_support.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - tests/parser/test_open_array_annotation.c
  - tests/parser/test_type_inference.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_array_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_array_type.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_open_array_annotation.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_syntax_hygiene_field_results.c
doc_type: module-detail
---

# Open Array Annotation Conversion

This is a lower compatibility gate for [field result identity](macro-field-result-identity.md).
Capturing actual field annotations exposed the shared converter's requirement
that every array extent expression already have a concrete compile-time value.
An open class `Matrix<T, const N: int>` previously published before its later
specialization. Eager capture of `Array<T>[N]` now reports an unknown identifier
and rejects that class before its prototype is published.

## Candidate Boundaries

The path is source field capture, AST annotation conversion, named element
resolution, array wrapping, compile-time extent evaluation, and later generic
copying. The actual existing regression fails before boxed construction, so no
constructor-specific branch can repair it. The concrete extent evaluator still
needs to reject missing identifiers, negative values and nonintegers.

The proposed narrow shared repair defers only a parsed bare identifier that
names an actual in-scope CONST_INT generic parameter. Parentheses disappear in
the parser and do not broaden that rule. Matching must use valid normalized
LOCAL syntax contexts, preserve nearer-owner shadowing, and reject invalid or
different opaque contexts. Concrete compile-time variable bindings take
precedence, even when their values have not yet been evaluated. Presence checks
must not mistake a context-normalization failure for an absent binding. No
evaluate-fail/clear-error fallback or whole-field capture bypass is allowed.

## Actual Source Fixtures

The fixture parses Root and opaque same-spelling Widget declarations, a real
`Holder<T, const N: int>` with a Widget-valued array field, an independent scalar
Widget annotation, and a const variable. It compiles both Widget declarations,
copies their real bindings, and proves the scalar annotation's exact nominal
token and canonical type ID before attempting the array conversion. The active
generic context points to the actual parsed Holder AST, not fabricated metadata.

Three direct cases require open Root/marked elements and parenthesized N to
convert. Two class producer cases require actual published field metadata to
own the same element identity. Thirteen controls cover concrete `2 + 2` and
`0`, missing/out-of-owner/type-parameter identifiers, negative and boolean
values, unsupported open arithmetic, invalid/different opaque contexts, and
concrete binding precedence with integer/negative/boolean source values. The
binding controls call the existing registration helper on a real parsed const
declaration; they do not claim a new language form or synthesize registry rows.

Parsed literal `[0]` is folded into numeric AST fields with a null extent
expression. Other fixture expressions retain their actual parsed nodes. Tests
assert those shapes and leave the expression pointer unchanged. Inferred values
are freed before compiler teardown, then AST, syntax context and runtime.

## Evidence

The first field replay naturally ends driver1/build0: all 28 field cases pass,
but the existing Matrix case fails at test_type_inference.c:5100 with
`Unknown compile-time identifier: N`. Fifteen programs total 429 cases,
428 PASS, one FAIL, zero ignored. This is not field GREEN acceptance.

`m1-msvc-open-array-red-r1-*`, native32115, naturally ends build1 with no tests:
the new fixture used the wrong class member collection name and omitted its
private lookup declaration. Root fixes those fixture-only errors after terminal.
RED-r2, native87874, naturally ends build0/test6: eighteen cases, twelve PASS,
six FAIL. Five failures are the real open-extent paths; the zero control has an
incorrect nonnull-expression prerequisite and receives no behavioral RED credit.

After correcting only that parsed-zero prerequisite, RED-r3 native11165 naturally
ends driver1/build0/test5: eighteen cases, thirteen PASS, five FAIL, zero ignored.
All binding, AST, scalar element, generic parameter and diagnostic controls pass.
Only the three open converters and two source producers fail. At that point
production remains unchanged while the narrow repair is prepared. Independent
review accepts the actual fixture. Root adds explicit Root binding and canonical
ID prerequisites and checks the N identifier AST tag; fresh RED-r4-prerequisites
naturally closes build0/test5 with the same thirteen PASS and five FAIL, zero ignored.

## Private Repair

The exact repair has three source files. type_inference_array_type.c owns extent
resolution, with its private header defining a numeric SZrArrayTypeExtent and
resolver. The main converter replaces its previous inline extent branch with
that handoff; array wrapping and error cleanup retain the same ownership order.
The native concrete evaluator and existing generic copier remain unchanged.

The helper checks matching compile-time variable names before generic owners.
Its context comparison reports invalid raw or failed normalized contexts rather
than treating either as absence. Concrete binding presence uses the evaluator's
ITEM namespace, so unevaluated variables still reach normal evaluation. Generic
parameter matching uses LOCAL namespace and actual parameter ASTs, function
owner before type owner. A nearer matching type parameter stops an outer const
parameter. Non-identifiers and absent parameters use the original evaluator.
Only an unbound matching CONST_INT parameter returns unresolved numeric bounds.

The source is frozen at main7D7A106B/newC E38CA8DE/newH DFC6529F. Root's first
independent whole-file in-memory inverse removes the new include and replaces
the extent handoff with the original raw branch. It matches the full baseline
452DC6EE hash, 4041 lines and 3915 CR bytes; no filesystem source copy or source
restoration occurs. The seven field files and existing headers are unchanged.
First native GREEN-r1, native42187, naturally closes driver0/build0: fifteen
programs, 419 PASS, zero FAIL, zero ignored, with all eighteen direct cases and
the existing Matrix producer restored. Root verifies all fifteen unique test
footers. Independent full three-source review finds no actionable defect.

Seven post-repair companions extend the fixture to twenty-five cases. Genuine
trailing function ASTs supply a const owner and a nearer type-parameter owner;
their parameter tags, names and kinds are asserted. Cases cover function-only
const scope, nearer matching type-parameter shadowing, a different opaque inner
parameter, matching opaque const-parameter contexts, an invalid parameter
context, a distinct opaque compile-time binding and an invalid binding context.
The last negative mutates the context of an already genuinely registered row;
it must not become an absent binding eligible for deferral. These cases receive
no invented earlier RED credit. GREEN-r2-contexts native10925 naturally ends
driver1/build0: the 401 related cases pass, but all twenty-five fixture cases
stop at the parsed statement-count prerequisite. The trailing function name
`shadow` is a reserved ZR keyword, so the parser recovers an extra statement.
This is a fixture-only prerequisite failure and gives no context feature credit.
Root renames only that source function to `inner_type`; fresh standalone
GREEN-r3-contexts native32148 naturally ends driver0/build0/test0: all twenty-five
cases PASS, zero FAIL, zero ignored. This is a standalone twenty-five-case
replay, not a new combined 426-case result. Independent review accepts the
owner/context tests statically; the preceding parse failure earns no credit.
Injected normalization-budget failure remains a separate coverage gap; invalid
raw contexts do not prove that failure mode.

## Excluded Claims

SZrInferredType stores numeric bounds, not a symbolic extent expression. An
open constrained array with zero numeric bounds is an unresolved approximation,
not proof of symbolic-size identity; it shares that carrier with concrete zero.
The canonical array adapter also ignores numeric bounds. Existing generic copying
copies those numbers and cannot turn an open N into a closed numeric 4. This gate
does not claim closed-size materialization, arbitrary const arithmetic, layout
specialization or a new public type ABI. Those require separate lower contracts
and tests. M1 and its full platform matrix remain unaccepted.

See the [validation ledger](../testing-and-validation/macro-foundation-validation.md).
