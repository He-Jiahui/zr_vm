---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.h
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_value_alias.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/parser/test_syntax_hygiene_base_publication.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.h
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_value_alias.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - tests/parser/test_syntax_hygiene_base_publication.c
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/parser/test_syntax_hygiene_generic_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_definitions.c
  - tests/parser/test_syntax_hygiene_aliases.c
  - tests/parser/test_syntax_hygiene_import_aliases.c
doc_type: module-detail
---

# Macro Compiler Prototype Lookup

M1's hygiene identity must survive consumers after declaration publication and
generic specialization. The compiler and inference prototype finders are
different consumers. Passing the inference finder does not prove that a later
compiler metadata lookup selects the same declaration.

## Query Contracts

A name-only query is a Root query. It excludes opaque source declarations and
closed specializations whose base or recursively nested arguments are opaque.
Mixed-site ITEM names normalize to Root. In-progress metadata obeys the same
eligibility as registered prototypes. Imported metadata keeps its existing null
declaration pointer and remains eligible for ordinary Root lookup.

A visible local type-value alias is an explicit reference. The alias name is
resolved in the LOCAL namespace; its stored inferred type contains the actual
nominal token and ordered generic arguments. A Root alias may deliberately name
an opaque type. Resolving that target by its display spelling instead would
either select a same-spelling Root type or reject the valid reference. Invalid
stored identity must fail closed rather than select a Root substitute.

Compiler lookup retains first-registration precedence. Inference lookup retains
its source-over-import preference. The alias repair must preserve this difference
for unmarked nongeneric targets, not replace the entire compiler wrapper with
the inference wrapper.

## Lifetime Boundaries

Aliases own inferred-type copies. A query snapshots the inferred target before
generic materialization can grow compiler arrays. Closed prototype keys are
context-local canonical IDs; callers reacquire prototype records after table
growth instead of retaining a pointer across it. Imported declaration pointers
remain null because the import parser's temporary AST is freed.

The shared module/member display-name builder is still used by later member
compilation. Moving lookup-only helpers must preserve that caller and its
existing generic-suffix behavior. Filtering exact Root candidates must apply
to direct, current, materialized and qualified module/member scans alike.

The private `compiler_type_prototype_lookup.c` now owns the compiler name finder,
eligible exact scan and lookup-only display-name helpers. Its narrow header
also exposes the unchanged canonical module/member display-name builder to the
existing expression compiler. The extraction removes 263 lines from
`compile_expression_types.c` without changing unrelated callers. Inference's
alias branch copies its target as well; ordinary unstructured aliases retain
their native import fallback and source preference.

## Declaration Identity And Completion

LSP bootstrap shells retain the actual declaration AST, including extern
declarations inside a block. Neither a temporary AST copy, a metadata-only null
declaration, a source range nor a matching name substitutes for that identity:
the ordinary duplicate check must distinguish a repeated phase visit from a
genuinely different declaration.

The explicit `isDeclarationShell` state separates preview availability from
normal completion. Extern struct, extern enum and ordinary enum can complete
an open same-AST shell after normal validation and binding without publishing
a second declaration row. Completed declarations remain idempotent. Closed
generic records retain separate canonical instance keys and cannot be selected
for open-shell completion even when their declaration AST matches.

Failure keeps the old preview and releases only the unpublished candidate.
Success re-finds the old slot and transfers independent owned metadata; callers
must not retain member pointers across that handoff. See
[metadata ownership](type-prototype-metadata-ownership.md) for cleanup details.
This does not alter Root lookup precedence, opaque identity eligibility, aliases
or imported metadata AST lifetimes.

The real bootstrap identity case passes before and after repeated extern
predeclaration. Additional enum runtime, struct layout, normal field-AST,
failed-candidate and ordinary-enum cases have exact pre-repair RED evidence.
The normal GCC96 repaired replay passes with six natural exit0 children and
stable full inputs (receipt62CD3602). This lower result does not replace original
LSP interface, full matrix or M1 acceptance.

## Native Evidence

The later [BASE producer gate](macro-base-relation-publication.md) adds a private
annotation selector in this lookup module. Unlike the unchanged name-only API,
it receives the actual inheritance annotation, distinguishes failure from valid
Root-forward absence, and must preserve both ITEM identity and LOCAL alias
payload. Direct generic annotations cannot consult a display-name cache before
converting their recursive arguments. Root plain-target selection still uses
compiler first-registration precedence rather than inference source preference.
GREEN-r1 validates17 direct plus401 related cases, but peer review finds two
additional actual-producer regressions. RED-r5 proves mixed LOCAL absence can
reopen a Root alias and a bare alias match can erase Chosen[] array shape.
The lookup-only correction matches aliases by complete spelling and disables the
initial Root alias query only after a checked LOCAL miss. The public name API
keeps its original policy. GREEN-r2 native52095 naturally exits0/build0 with
22 direct plus401 related cases,423PASS0FAIL0ignored and15 accounted footers.
The later contracts gate has actual RED at32cases/4failures: both same-name
opaque replays, mixed LOCAL miss, and missing alias-producer TYPE-use fact.
Earlier name-only evidence below does not prove that later replay or M1.

The fixture parses actual union declarations, compiles their contextual TYPE
bindings, verifies canonical nominal tokens, converts real generic alias ASTs,
and checks structured specialization before exercising name-only consumers.
Its current-prototype test temporarily hides the real published entry but
restores the owned arrays during teardown. Its import precedence companion
transfers source metadata ownership to a second slot before replacing the first
slot with a null-declaration imported descriptor.

The first native attempt is invalid setup: a missing private function declaration
causes pointer-return narrowing, and independent review catches the wrong AST
union arm for a generic head. The owned probe is stopped; its report is preserved
without behavioral credit. Corrected red-r2 naturally exits1 after build0 with
19 cases, 10 PASS, 9 FAIL and zero ignored. Red-r3 strengthens the compiler-first
lazy query, materializes the same-spelling Root target before an explicit marked
alias, and adds generic-argument and alias-precedence companions. It naturally
exits1 after build0 with 22 cases, 10 PASS, 12 FAIL and zero ignored.

The six failed compiler Root queries cover an opaque declaration, opaque-first
insertion, current metadata, opaque specialization and two marked-argument cache
paths. Six alias failures cover inference's opaque-only target, same-spelling
Root/opaque target selection, a marked argument and invalid-token fallback.
Root, mixed-site, imported-null and both precedence prerequisites pass.

Immutable report stems are
`E:/cargo-targets/zr_vm/reports/m1-msvc-compiler-prototype-lookup-red-r1/r2/r3`.
The four-file repair passes fresh `m1-msvc-compiler-prototype-lookup-green-r1`:
build0, twelve programs and 352 cases PASS with zero ignored. Independent
read-only review reports no actionable finding. The full
`m1-msvc-compiler-prototype-lookup-final-r1` replay is terminal1: both builds0,
foundation40/40PASS, aggregate8 and inventory108PASS/13FAIL. All 121 inventory
exit codes, Unity footer tuples and failed-test messages match the preceding
generic-support baseline. Only hexadecimal memory pointers are normalized;
there are zero changed failures and zero missing footers. The comparison is
preserved in `m1-msvc-compiler-prototype-lookup-final-r1-compare.log`.

Constructor emission, receiver member selection, artifact identities and
complete M1 acceptance are not proved by this fixture and remain separate
gates. The next read-only audit finds a structured receiver reduced to its name
at ordinary field/call lookup; genuine parsed producer fixtures must establish
that lower support failure before another production lease.

## Reference Evidence

Rust's `Instance` stores definition and generic arguments together in
`lua/rust/compiler/rustc_middle/src/ty/instance.rs`; its hygiene context separates
spelling from expansion ancestry in
`lua/rust/compiler/rustc_span/src/hygiene.rs`. The generic hygiene UI fixtures
`lua/rust/tests/ui/hygiene/generic_params.rs` and
`lua/rust/tests/ui/const-generics/early/const-param-hygiene.rs` exercise generated
type/const parameter references. ZR uses its own existing semantic symbols and
canonical graph, not text reconstruction as the macro compilation boundary.

See [generic identity](macro-generic-type-identity.md) and the
[validation ledger](../testing-and-validation/macro-foundation-validation.md).
