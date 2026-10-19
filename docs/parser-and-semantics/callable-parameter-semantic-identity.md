---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic/place.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/place.h
  - zr_vm_core/include/zr_vm_core/function.h
  - tests/parser/test_callable_parameter_place_cases.h
  - tests/parser/test_weak_owner_release_evidence_cases.h
  - tests/parser/test_primary_expression_source_range_cases.h
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_aot_c_generic_struct_return_cases.h
  - tests/parser/test_aot_c_dynamic_return_representation_cases.h
  - tests/parser/aot_c_ownership_call_result_projection_cases.h
  - tests/cli/test_cli_project_incremental.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic/place.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.h
plan_sources:
  - "user: 2026-10-16 document source-backed callable parameter SemanticIR identity, strict registration, and actual RED evidence without granting native or M1 acceptance"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
tests:
  - tests/parser/test_callable_parameter_place_cases.h
  - tests/parser/test_weak_owner_release_evidence_cases.h
  - tests/parser/test_primary_expression_source_range_cases.h
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_aot_c_generic_struct_return_cases.h
  - tests/parser/test_aot_c_dynamic_return_representation_cases.h
  - tests/parser/aot_c_ownership_call_result_projection_cases.h
  - tests/cli/test_cli_project_incremental.c
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
doc_type: module-detail
---

# Callable Parameter Semantic Identity

## Purpose and Current Status

A parameter declaration must retain its own symbol, canonical type, parameter
place, source range, and stack slot even when its body never reads that parameter.
A name-only lookup during metadata emission cannot substitute for this identity:
an outer declaration may have the same name but a different symbol and type.

The private [parameter registration helper](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c)
establishes this support-layer identity after a real parameter binding and local
slot have been registered, before body compilation can incidentally create a
place through a read. It does not manufacture a read to make an unused parameter
look used, and does not repair missing identity in the AOT metadata reader.

The helper and all three consumers were frozen and executed in lower r3. All four
source-backed parameter tests, the Stamp module build, and the existing owned-copy
case passed. That run also exposed a separate integration failure: declaration
identity created a sidecar without a value, while direct ownership lowering
assumed an existing slot already had one. In r4 the direct incoming-parameter
test reached its exact compilation behavior RED; the four older weak fixtures
still failed prerequisites. Those dated failures remain unchanged.

The current helper now publishes a typed external entry value for the real
incoming parameter and records it in the sidecar. This bounded producer change
was executed in r5: direct-degrade and all three weak controls passed without
fixture failures, while the main weak release-evidence case still reached its
exact behavior RED. R6 retained those results and passed all 17 default AOT cases,
including parameter4, Stamp, owned-copy, and both scalar regressions. Its
controlled observer exited 0, not an overall behavior or platform GREEN; the main
weak case and three ownership cases still failed in that dated r6 run.

On 2026-10-17 UTC, r8 passed the default query72, ownership25, and Windows AOT17
suites after only the two PRIMARY parent-range assignments changed from r7.
This focused integration GREEN does not establish a complete incoming-parameter
ABI. The later full Windows r11 remains FAIL, Linux remains unpassed, and M1
remains unaccepted. The later R11-support R4 verified the narrow definite-assignment
declaration-identity repair while retaining lambda and BOOL support failures at
that boundary. R11-support R6 then passed query77, original LSP84 and ownership25;
the independent BLOCK, UNKNOWN/OBJECT, mixed-source and BOOL failures below
remain outside that lambda success.
The user has now approved retaining original TokenSpan provenance, AST-identity
attribution, and cross-source diagnostic anchors with related origins. That
direction has now reached a real mixed-postfix behavior RED, not a validated
repair; an unanswered product decision is no longer the blocker. The narrow
BLOCK/default-UNKNOWN repairs have a later R7 result, but its strict receipt
remains FAIL for the OBJECT test expectation and mixed-postfix behavior below.

## Ownership and Data Flow

| Layer | Responsibility | Output or boundary |
| --- | --- | --- |
| Callable consumer | Allocate the real local slot and register the source parameter type/binding | Name, syntax context, stack slot, and actual declaration range |
| Private parameter helper | Validate current binding, symbol, canonical type, and existing registration | ERROR, UNAVAILABLE, or REGISTERED |
| SemanticIR and place graph | Publish the parameter local, root place, and typed external entry value | Real symbol/type/place/value IDs, parameter base, source range, and scalar classification |
| Compiler slot sidecar | Connect the runtime stack slot to that SemanticIR identity and incoming value | `SZrCompilerSemanticIrSlot`, including `valueId` |
| Typed metadata reader | Read existing slot identity and preserve parameter/receiver roles | `SZrFunctionTypedLocalBinding` |

The declaration binding, SemanticIR local, graph place, and slot sidecar describe
one declaration in the current callable. They are not interchangeable caches in
which any same-name entry is acceptable. Numeric IDs are meaningful only in their
own semantic context and graph; equal slot numbers in two functions do not imply
equal declarations.

## Inputs and Three-State Result

The [private interface](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.h)
accepts the compiler state, parameter name, syntax context, allocated stack slot,
and `SZrFileRange`. It does not allocate a replacement symbol or canonical type.

| Result | Meaning | Consumer behavior |
| --- | --- | --- |
| `ZR_COMPILER_PARAMETER_IDENTITY_ERROR` | Invalid state, failed lookup/comparison, inconsistent identity, conflicting registration, or failed publication | Report a compiler error and use the consumer's existing scope/isolation cleanup path |
| `ZR_COMPILER_PARAMETER_IDENTITY_UNAVAILABLE` | The real binding and declaration identity exist, but their actual `typeId` is `ZR_SEMANTIC_ID_INVALID` (0) and no conflicting partial registration exists | Preserve the unsupported/unmodeled state without inventing a type or place ID |
| `ZR_COMPILER_PARAMETER_IDENTITY_REGISTERED` | A consistent new parameter identity was published, or an exactly compatible existing registration was found | Continue compilation |

UNAVAILABLE is not a generic error suppression result. A nonzero type ID whose
canonical record is missing or has a different ID is ERROR, not UNAVAILABLE.
Likewise an invalid symbol, failed syntax-context normalization, duplicate slot,
or dangling graph entry cannot be converted into a successful unsupported case.
REGISTERED describes this local registration only, not a successful whole compile,
AOT module build, native test, or platform acceptance.

## Validate the Real Declaration

The helper first checks compiler/state/context ownership, initialized SemanticIR,
array validity and element sizes, array bounds, and the parallel lengths of
`localVars` and `localBindingIdentities`. The type environment must belong to the
same semantic context. The shape checks include the values array as well as
instructions, places, locals, and sidecars: each array must have valid storage,
the expected element size, length no greater than capacity, and nonoverflowing
capacity multiplication. An invalid stack-slot sentinel is rejected.

[Current-environment lookup](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c)
is deliberately used instead of a full parent-chain lookup. Its BOOL result is
separate from the returned binding: lookup/comparison failure is not a miss.
The [context comparator](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h)
checks validity, accepts actual equal contexts directly, and otherwise normalizes
both in the LOCAL namespace. Normalization failure aborts registration.

Exactly one allocated local must occupy the requested stack slot, with the same
name and equivalent syntax context. The binding's symbol must exist in the same
semantic context, retain the same name and type ID, have PARAMETER or VARIABLE
kind, and have a matching syntax context. VARIABLE is allowed for injected
bindings such as `this`; it is not permission to borrow a parent variable.

When the binding has a declaration range, that range must equal the supplied
range in source object identity, both offsets, both lines, and both columns.
The helper also verifies that a nonzero type ID resolves to the actual canonical
record with that ID. This is an identity/existence check, not a new structural
type inference or an attempt to substitute a nearby canonical type.

## Parameter Local, Graph, and Slot Sidecar

For a modeled canonical type, publication uses the real
[`ZrParser_SemanticIr_AddLocal`](../../zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c)
and [place graph](../../zr_vm_parser/src/zr_vm_parser/semantic/place.c) APIs:

- The root base kind is `ZR_PARSER_PLACE_BASE_PARAMETER` and its identity is the allocated stack slot.
- The graph entry retains the real canonical `typeId` and supplied source range, with no projections.
- The SemanticIR local retains the same symbol, type, and place and has `isParameter = TRUE`.
- `isScalar` follows whether that canonical node is PRIMITIVE. This does not independently prove an AOT C storage representation or a global GC property.
- `ZrParser_SemanticIr_AddValue` creates an actual value with the canonical type and the same declaration range; its definition instruction is INVALID, identifying an external entry value rather than an instruction-produced value.
- The compiler sidecar records the same stack slot, symbol, type, graph-issued place ID, and newly allocated `valueId`.

The sidecar is zero-initialized before these fields are assigned. This is a real
incoming parameter value, not a synthetic lexical READ, LOAD, or INITIALIZE, and
registration emits no executable instruction or extra runtime stack slot. Later
body compilation remains responsible for actual reads, loads, stores, ownership
consumption, and derived values. A new registration invalidates the previous
SemanticIR validation result so later validation must inspect the new state.

The current helper SHA256 is
`8D730AC043790766E63207012868F316AE4FAF9BB042EED72DEC1ADB81BA69CD`.
Its external value uses the existing AddValue contract; it is not a change to the
generic ExecIR native-VM adapter or a claim that every external-value shape is
supported by that adapter.

An injected `this` is also a callable parameter for graph/frame purposes and uses
BASE_PARAMETER, not BASE_THIS. Its public typed-local role remains RECEIVER.
Parameter-base classification and receiver/passing-mode role are separate facts;
changing the base does not turn a receiver into a by-value explicit parameter.

## Strict Idempotence and Failure Cleanup

An existing slot is accepted only when its symbol and type match and its place
ID is valid. The graph entry must have the same ID, a root parameter base for the
same stack slot, the same type and full source range, and zero projections.
Another LOCAL, THIS, or PARAMETER root competing for that stack slot is rejected.
The symbol cannot already occupy a different sidecar slot.

The existing sidecar's `valueId` must resolve to an actual value with that ID,
the same canonical type and full declaration range, and an INVALID definition
instruction. A zero/consumed value ID, missing value, incompatible type/range, or
instruction-defined value is ERROR. Repeated declaration registration must not
repopulate a slot cleared by DROP/MOVE or disguise an arbitrary body value as an
incoming parameter.

Exactly one corresponding SemanticIR local must agree on symbol, type, place,
parameter flag, and scalar classification. A graph entry without a matching
local, a local without the expected slot, duplicate matches, or a partially
compatible tuple is ERROR. Repeated registration does not silently append a
second local or reuse only the convenient subset of an earlier registration.

For a new identity, the helper saves the original graph/local/sidecar/value
lengths. Place and value counts must be below `UINT32_MAX` before their one-based
ID append; local and sidecar lengths must also permit an increment. It reserves
all four arrays before publishing. A single `ZrCore_Exception_TryRun` callback
performs AddLocal followed by AddValue only when a real place was published.

Success requires exactly one new place, local, and value, with the value ID equal
to the previous value count plus one. The helper rechecks the actual value's ID,
type, definition-free status, and range before scalar marking and checked sidecar
append. On failure it restores all four logical lengths and frees projection
buffers belonging to graph entries appended by this attempt. It does not leave
a successful-looking declaration-only sidecar when entry-value publication fails.

This is bounded local cleanup. Capacity growth may remain after a reserve, and
the helper does not roll back the consumer's earlier type binding or local-slot
allocation. Consumer error paths still own callable scope/isolation cleanup.
The implementation does not introduce a global OOM transaction, allocator
rollback, or repository-wide exception-safety guarantee.

## Callable Consumers

The inspected consumers are:

- [Named functions](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c): allocate the source-context local slot, register the annotated/default type with the parameter AST and its range, then register identity before the body.
- [Class members](../../zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c): register the injected receiver and explicit parameters within the member's callable type/SemanticIR isolation; manual property parameters use their actual accessor/member range.
- [Lambdas](../../zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c): register parameters in the child callable type scope, preserving the parameter's syntax context and range rather than inheriting a same-name outer binding.

The helper returns status; consumers produce the appropriate compiler diagnostic
on ERROR and follow their existing cleanup labels. UNAVAILABLE does not cause a
consumer to manufacture an ID. The original identity integration passed its four
source-backed cases in r3/r4 and retained those passes in r5/r6. The entry-value
producer also passed the direct incoming-parameter and three weak controls in
r5/r6, but the main weak evidence assertion remained a failure in those runs.
The later r8 focused suites passed after the separate PRIMARY range repair;
there was no new parameter-producer change between r7 and r8. The four parameter tests do not directly
cover every property-accessor, calling-convention, annotation, or failure path in
these consumers.

## Metadata Reader Boundary

The existing [slot identity reader](../../zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c)
returns FALSE when the slot, symbol/type/place IDs, or graph place are unavailable.
This producer change must not relax that check or make a missing place look
complete. [Typed local metadata](../../zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c)
copies a successful slot identity's IDs and declaration coordinates, while its
existing role logic keeps passing modes and implicit receiver roles separate.

The existing typed metadata builder also has a name-based fallback that can fill
symbol/type fields when no slot symbol was read. That fallback does not create a
place or declaration range; this work does not claim it was removed or globally
redesigned. It cannot stand in for the complete current-parameter tuple required
by these regressions. Unsupported parameters may retain incomplete metadata;
they are not upgraded to complete AOT/frame support by assigning arbitrary IDs.

Raw callable-return metadata and AOT C representation are separate concerns.
Nothing in this helper changes raw return types to satisfy a generated-C text
expectation. The independent scalar-signature behavior failures in r3-r5 passed
in r6 after a bounded C-signature representation change, not a raw-type rewrite.
The distinct contract and controlled scalar tests are described in
[AOT C return representation](../testing-and-validation/aot-c-return-representation.md).

## Definite-Assignment Declaration Identity

Parameter entry identity and definite-assignment ownership are separate support
layers. The bounded
[definite-assignment resolver](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c)
executed in R11-support R4 has SHA256
`DD206F9BF593DEAAD3C6530F9084BABF232F318C5914E57E0EA6CC8BBEA69EF1`.
For a simple variable declaration it selects the resolved DECLARATION fact by
the actual declaration or identifier-pattern AST node first, not by a same-name
symbol elsewhere in the context. Only if that identity pass finds no declaration
does it accept a known, exact pattern range under the existing same-source
comparison. Offset, line, and column endpoints must all match. This preserves
the existing precise-range synthetic-fact tests without treating broad range
containment as declaration identity; conflicting symbol candidates are rejected.

The source-backed
[callable-identity regressions](../../tests/parser/test_definite_assignment_callable_identity_cases.h)
keep the actual Parse/compile AST and resolved references. The negative control
requires the real return identifier's READ facts to resolve to the declaration's
symbol and all carry UNINIT, together with descriptor 3002 at the exact source
range. It requires evidence to exist but does not lock the number of duplicate
READs or diagnostics: the observed compiler can infer the same return AST twice.
The existing [facts suite](../../tests/parser/test_semantic_facts.c) separately
retains precise synthetic ranges, self-initializer behavior, and sourced-AST
exclusion of sourceless facts.

The declaration fix alone does not establish that every nested lambda's READ
belongs to its own callable CFG. The separate private
[AST index](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.c)
now matches actual owning-tree identity and nearest-callable ownership, allowing
range fallback only for a non-indexed fact with a NULL node or no ownership object.
It excludes a real lambda's parameter/body facts from the enclosing initializer
and schedules the actual lambda block independently. R6 passed the existing
lambda/using case and the new body-uninitialized and conditional-nesting cases,
as well as the original nested-metadata LSP case. Its new standalone BLOCK entry
test still failed in R6 and then passed with facts20 in R7. The
[dataflow module](dataflow-engine-foundation.md) describes these entry and lifetime
contracts. Neither layer grants a complete incoming ABI, mixed-source projection
implementation, or global OOM transaction.

## Source-Backed Tests and Resource Lifetime

The four cases in [the parameter test header](../../tests/parser/test_callable_parameter_place_cases.h)
compile real source with the public `ZrParser_Source_Compile` API. They temporarily
enable `emitCompileTimeRuntimeSupport` and restore its previous value, parse the
same source separately for real AST ranges, and build the production AOT function
table. Callable selection requires exactly one compiled function with the actual
AST start/end lines and expected name, not a fabricated function index.

| Case suffix | Source distinction | Intended assertions after prerequisites |
| --- | --- | --- |
| `unused_function_parameter` | Unused `int value` with same-name outer `bool value` | Own slot, role, symbol/type/place, parameter range, and distinct outer identity |
| `unused_generic_parameter` | Unused `T value` with same-name outer `RefA value` | Generic canonical identity/place and no borrowing of the outer nominal type |
| `unused_member_and_receiver` | Const member with unused explicit `int value`, implicit `this`, and outer `bool value` | Explicit slot 1, receiver slot 0, distinct places/symbols, RECEIVER role and member range |
| `unused_lambda_parameter` | Unused lambda `int value` shadowing an outer function's `bool value` | Exact nested callable identity, each parameter's own range/place, and distinct declarations |

The fixture owns state, root function, separately parsed AST, and function table
in static storage so Unity longjmp does not lose the cleanup handles. `tearDown`
restores a still-pending emit flag, releases the table, root and AST, then destroys
the state. No test inserts a fake read, writes generated C, runs the VM, or creates
fixture files for these four cases.

The [Stamp regression](../../tests/parser/test_aot_c_generic_struct_return_cases.h)
remains a separate real module-build test of the CLI source. Its diagnostics and
cleanup contract are described in
[AOT generic struct return](../testing-and-validation/aot-generic-struct-return.md).
The new parameter cases do not replace that module-build boundary.

## Actual r2 RED and Acceptance Boundary

The dated run is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-red-r2-receipt.json`,
SHA256 `7D3E8EDC7B17A9A587C179F3CA5E5468A248139F9D057F5E02A98796D0E3FF1D`.
The receipt remains FAIL. Its 4793 source inputs and HEAD were unchanged, and its
children reached natural terminal states; those gates do not make failed behavior
pass.

The Windows default AOT run completed 15 ordered unique results with
`15 Tests 7 Failures 0 Ignored`. All four new cases passed compilation, Parse,
function-table/unique-callable lookup, slot/role, and valid symbol/type ID checks,
then failed at the canonical place assertion. Their later declaration-range,
base-type, same-name outer, and receiver assertions were not reached and have no
PASS credit. The Stamp module-build assertion also remained FAIL.

Two additional existing failures were observed and are not waived:

- `test_aot_c_method_info_infers_bool_u64_f64_script_return_signatures`: the I64 signature text expectation failed.
- `test_aot_c_never_scalarizes_owned_call_result_copy`: its existing TRUE assertion failed.

The prediction that all other old cases would pass was therefore false. The
controller's exact expected-failure comparison correctly retained overall FAIL;
the old assertions and raw logs must not be weakened or rewritten.

At r2/r3 the main kept all original 14 default registrations, including three
guarded by `ZR_PLATFORM_UNIX`, then appended the four parameter cases: Unix 18
and Windows 15. Two later scalar-representation cases raised those defaults
to Unix 20 and Windows 17 at the r6/r8 boundary without changing those ordered
prefixes. The later R11-support R4 also executed one appended BOOL proof case,
giving Windows 18; that added case failed and has no GREEN credit. The exact
`--generic-struct-return-only` selector still runs only
the Stamp case; it is not a four-case parameter selector and does not replace the
default suite or later matrix. These are registration counts, not new passing
results.

## Actual r3 and Remaining Integration Failure

`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-mixed-r3-receipt.json`
has SHA256 `0384CAD8E9B52ECF1E08B36D57C441924635F3118D23A7589CF0CC414AEA5E7B`
and remains FAIL. The normal build exited 0; all 4799 source inputs, HEAD,
six support inputs, and three binaries were stable, and all eight children
terminated naturally. Default Windows AOT completed 15 tests with 1 failure.
The four parameter cases reached their later range, base, shadowing, and receiver
assertions and passed; Stamp and the old owned-copy case also passed. The old
scalar-signature assertion remained a mandatory failure at this dated r3 boundary;
its later r6 pass does not rewrite that result or waive the assertion.

The query suite instead completed 70 tests with 6 failures: four weak fixture
prerequisite failures and two independent using behavior failures. A declaration
sidecar's valid symbol/type/place does not imply a valid `valueId`. The direct
identifier ownership path must establish an actual initialized slot value before
consuming it. A real incoming parameter can supply a canonical typed entry value;
an arbitrary slot cleared by DROP/MOVE cannot be repopulated that way. No lexical
READ or INITIALIZE should be fabricated to make a declaration look used. The
entry-value producer repair was still pending at this dated r3 boundary; its
current implementation and later r5 observation are separate from r3.

A new source-backed direct-degrade test is registered in the query suite after
the original 70, giving 71 cases in r4-r6. The subsequent PRIMARY regression
appends one case without reducing that prefix, giving the r7/r8 default 72.
The R11-support R3/R4 evidence below uses the later 75-case registration.
Parsing, source-name allocation,
and compiler initialization are prerequisites; successful ownership compilation
is the target assertion, including hasError, hasStructuredError,
hadRecoverableError, and hasFatalError. Its actual first run was r4, below.

The current entry value uses the existing definition-free external-value
contract; it does not add a new runtime slot. Ordinary ownership lowering emits
bytecode with the real source/result stack slots and does not invoke the generic
ExecIR native-VM adapter. That adapter currently rejects unused external values
and live external values outside its dead PLACE_BASE provenance subset. This
work does not relax that gate or claim complete external-parameter ABI support.

## Actual r4, r5, and r6

`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r4-receipt.json`
has SHA256 `EF7C4826C3578AD1D624D1E84C6F435F7AC08F6728DE40281308A8E3D26C5E44`
and remains FAIL. All 4800 source inputs, HEAD
`2e2c95d004317014334c9a09de599734d8da9746`, support6, and binary3 were stable;
all eight children reached natural terminal states. These gates do not convert
failed behavior or prerequisites into PASS.

| Actual r4 suite | Result and boundary |
| --- | --- |
| Query | 71 ordered cases: 66 PASS / 5 FAIL / 0 IGNORE. Original64 and using2 passed; weak4 failed prerequisites. The direct incoming-parameter case reached exact behavior RED: `degrade must accept a real incoming Shared parameter without a prior load`. |
| Ownership | 25 ordered results: 22 PASS / 3 FAIL. The three weak release/alias cases remained failures. |
| AOT default | 17 ordered cases: 15 PASS / 2 FAIL / 0 IGNORE. The old scalar assertion failed; the new scalar positive failed its compound raw-metadata fixture prerequisite, not its representation behavior assertion. The negative Unity case completed all five controlled writer-input shapes and passed. |

The four parameter cases, Stamp, and the old owned-copy case remained PASS in r4.
That does not prove the subsequently changed helper's entry-value behavior. The
new scalar positive has no downstream assertion credit, and its failed complete
plain-DYNAMIC expectation is not a reason to change the compiler's separate raw
script-entry DYNAMIC contract.

### Controlled r5 Observation

`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r5-receipt.json`
has SHA256 `A84C6B57F6E8A722F625E559756D53FD3C4287221F7A36AE425D7A365649C938`.
Unified handle `27138` naturally exited 0 and was drained at
`2026-10-17T04:28:47Z`; all eight children reached natural terminal states.
Normal build `47160` exited 0. All 4800 source inputs, HEAD
`2e2c95d004317014334c9a09de599734d8da9746`, support6, and binary3 were stable.

| Actual r5 suite | Result and boundary |
| --- | --- |
| Query | 71 ordered cases: 70 PASS / 1 FAIL / 0 IGNORE. Direct-degrade, using2, and all three weak controls passed. Only `test_weak_call_owner_set_error_retains_release_evidence` failed: `Rejected weak ref calls must retain the branch owner release without a resolved CALL fact`. No fixture failure remained. |
| Ownership | 25 ordered results: 22 PASS / 3 FAIL. The same weak use/alias/owner-set cases remained failures. |
| AOT default | 17 ordered cases: 15 PASS / 2 FAIL / 0 IGNORE. The old scalar text assertion and the new positive representation assertion were behavior failures, with no fixture failure. The positive reason was `A known dynamic i64 return must refine only its C signature representation`. The negative case completed all five controlled shapes; parameter4, Stamp, and owned-copy also passed. |

The receipt state is `CONTROLLED_LOWER_SUPPORT_OBSERVATIONS_NOT_PLATFORM_ACCEPTED`.
Controller exit 0 means the preregistered observation contract completed, not that
the three native suites passed: query, ownership, and AOT exited 1, 1, and 2.
The r5 RAM controller SHA256 was
`A5A231A5D53714865E66EC4C5A2E8BDDC4CFC9C9CB04E68DC7E89052314BBE76`.
It retained normal builds of `zr_vm_compiler_semantic_query_diagnostics_test`,
`zr_vm_language_server_ownership_diagnostics_test`, and
`zr_vm_aot_c_method_info_signature_test`, with default query71 / ownership25 /
Windows AOT17. Whole-source, HEAD, support, binary, order, and prerequisite gates
remained strict. These dated local results neither rewrite r3/r4 failures nor
replace the complete matrix.

### Controlled r6 Observation

`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r6-receipt.json`
has SHA256 `353AE08DEE38402F1C851F4E608D989B4F48ACD5091E472476A2CF56B3B767E2`.
Root handle `40266` naturally exited 0 and was drained. Normal build `29420`
exited 0; all eight children terminated naturally. All 4800 whole-source inputs,
HEAD `2e2c95d004317014334c9a09de599734d8da9746`, support6, and binary3 were stable.

Query retained 71 ordered cases, 70 PASS / 1 FAIL / 0 IGNORE: only the main weak
release-evidence assertion failed, without a fixture failure. Direct-degrade,
using2, weak3 controls, and original64 passed. Ownership remained 25 ordered
results, 22 PASS / 3 weak FAIL. Default AOT completed
`17 Tests 0 Failures 0 Ignored`, including parameter4, Stamp, owned-copy, old
scalar, the immutable new scalar positive, and the single negative case's five
controlled shapes. This is dated focused AOT GREEN, not a complete matrix result.

The receipt still states `CONTROLLED_LOWER_SUPPORT_OBSERVATIONS_NOT_PLATFORM_ACCEPTED`.
Query and ownership exited 1; AOT exited 0. A failure-only probe recorded a real
resolved READ whose ERROR retained `requiresWake=1`, outside the statement
expression PRIMARY's recorded range. At that r6 boundary the separate parser
range repair had not been implemented, and shared weak behavior was not GREEN.
The later dated r7/r8 evidence below does not rewrite this failed observation.

## Dated R7/R8 Integration and Full R11 Boundary

The 2026-10-17 UTC r7 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r7-receipt.json`,
SHA256 `4EBF6E3F8CD04D58B80894E24C025206AD7786DB38EB27D2C0F299D7B4B5E7C3`.
Query72 had two behavior failures (main weak evidence and the new PRIMARY case),
ownership25 retained three weak failures, and AOT17 passed. The five parse-only
shapes and nested PRIMARY reached their range assertions, not fixture failures.

The r8 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r8-receipt.json`,
SHA256 `FFC86B539C59CB9FD4FE1CFA9F3E19741793DA1AEC8BEE319B17A2E99E9E907D`.
Only two successful-adoption parent-range assignments in
[parser_primary_members.c](../../zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c)
changed between those source pins. All default query72 / ownership25 / Windows
AOT17 cases then passed with no fixture, IGNORE, count, or order drift. Each run
independently held 4801 source inputs, HEAD, support6, and binary3 stable, and all
eight children terminated naturally. R8's state is
`FOCUSED_DEFAULT_SUITES_ALL_PASS_NOT_PLATFORM_ACCEPTED`. The
[range contract](primary-expression-source-ranges.md) covers a common physical
coordinate system only. Mixed-source projection was still undecided at that
dated r8 update; the user has since approved the provenance/AST-attribution
direction above, while its implementation and regression validation remain pending.

The subsequent full Windows r11 inner receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-receipt.json`, SHA256
`7C7D2DDCDCCE2BC12135D53B927DE86D3D5783A9621728E118626C47DB0CFF78`;
the outer receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-environment-launch-receipt.json`,
SHA256 `3AE3BD1E7E4317BD7FEF8D9BB256F3C0ACBD5E9485AEF89D3CBAA98C11D17995`.
Foundation52, lower323, and original127 all passed, but full55 had 53 PASS / 2 FAIL
in `language_pipeline` and `language_server`. Within the original semantic
analyzer84, `Semantic Analyzer Nested Source Metadata Is Complete` failed with
the case7 diagnostic, and `Semantic Analyzer Preserves Owner Generic Context In
Member Signatures` also failed. These are actual integration failures, not proof
of their cause or permission to broaden this helper's ABI contract.

All 4801 source pins were unchanged, but HEAD changed from
`2e2c95d004317014334c9a09de599734d8da9746` to
`d7d4c8f28a4dd78920ce5ab1bb9e882f68e51dc0`; both receipts remain FAIL.
The later CLI gate, outer auxiliary native3+4, and source-final were not reached.
No Windows/Linux/M1 acceptance, commit, or M2 advancement follows from r8 or r11.

## R11-Support R3 and R4 Current Evidence

These are later R11-support runs, not the earlier r10 lower r3/r4 receipts.
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lower-support-red-r3-receipt.json`
has SHA256 `8D63E96790549D636DAC7B901EFF6ABEDB8E9C8AAB5BC6690F98B0BD443F06B5`
and remains FAIL. Query75 completed with three failures: the same-name callable
positive and lambda/using positive reached genuine behavior RED; the new
uninitialized-local negative instead failed its mistaken `count==1` prerequisite
when two READs came from two inference visits to the same return AST. Correcting
that control does not retroactively turn its R3 prerequisite failure into behavior
RED or PASS.

`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-declaration-identity-r4-receipt.json`
has SHA256 `8BA8F1F51B8DFC3ED5712305476948EF0FBDAE2AE578929852C5D39B99D0ADE2`
and state `DECLARATION_IDENTITY_VERIFIED_OTHER_SUPPORT_RED_NO_PLATFORM_CREDIT`.
Normal build exited 0. Query75 had 74 PASS / 1 lambda behavior FAIL; ownership25
and facts19 all passed. AOT18 had 17 PASS / 1 BOOL FAIL, original LSP84 had
83 PASS / 1 nested-lambda metadata FAIL, and guardrail6 had 5 PASS / 1 BOOL FAIL.
The original owner-generic signature case passed. The negative now checks real
AST/symbol/state/range evidence without a duplicate-count assumption.

All 4805 source inputs, HEAD `676c3c88bc39d155c16cae873ee565207b3c166b`,
support6, and binary6 were stable; all eleven children terminated naturally.
This verifies the narrow declaration-identity contract, not the remaining lambda
or BOOL support, full Windows/Linux, or M1. At that R4 boundary the private lambda
attribution module and unknown/explicit-OBJECT regressions had no new native
credit. The later R6 observations below do not rewrite those R4 inputs or
failures. No commit was made and released space remains 0 B.

## R11-Support R6 Lambda Result And Remaining RED

`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lambda-and-origin-red-r6-receipt.json`
has SHA256 `5C66CA772D337D99291669926CBADE3ACC072E94E61C2ADD1869EF04E1496637`.
Unified `24087` naturally exited 0 and was drained; normal build exited 0.
All 4810 source inputs, HEAD `83e88fad7c70876c63b1fca86457259ba300d647`,
support6 and binary8 were stable, and all thirteen children terminated naturally.

Default query77, original LSP84 and ownership25 all passed. Query77 preserves the
old query75 prefix and appends the two real-source lambda body/conditional-nesting
controls; the original LSP nested-metadata and owner-generic cases also passed.
This supplies dated lambda attribution evidence, not a complete incoming ABI.
Facts20 retained the original nineteen passes but the new standalone BLOCK
resolver case reached missing-state behavior FAIL. SemIR4's original two cases
passed and its new missing-UNKNOWN/explicit-OBJECT separation cases both failed.
Token7 retained six passes and a mixed-postfix behavior FAIL: original Span
prerequisites passed, all five AST anchors were false and the parent was `0..70`.
AOT18 and guardrail6 each retained their existing single BOOL failure.

The exact state is
`LAMBDA_IDENTITY_VERIFIED_AOT_AND_PROVENANCE_RED_BLOCK_REGRESSION_NO_PLATFORM_CREDIT`.
The zero controller exit is not overall behavior GREEN. Root's narrow BLOCK and
default-UNKNOWN repairs do not rewrite the R6 failures.

The subsequent receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-da-aot-repair-r7-receipt.json`, SHA256
`985AF370C14738D6AD637272505A4DC3348F8A7E9A1898E1063C3B1CE9229B80`,
naturally exited 1 and was drained with state FAIL. Normal build exited 0;
query77, ownership25, AOT18, original LSP84, guardrail6 and facts20 all passed.
SemIR's original two cases and missing-UNKNOWN case passed. The new OBJECT case
instead failed at header line 160, `Expected 0 Was 12`: it incorrectly required
plain DYNAMIC where the existing producer annotates explicit OBJECT as GC_REF.
This is a fixture expectation failure, not established dangling identity or
downstream OBJECT-control success. Token7 retained six passes and the same
mixed-source behavior RED. All 4810 source inputs, the same HEAD, support6 and
binary8 were stable; all thirteen children terminated naturally. An OBJECT
fixture/identity correction is being prepared without R8 credit. Mixed-source
repair, full Windows/Linux and M1 remain unaccepted; there is no commit credit.

This document grants no full Windows, Linux, M1, or commit credit. The complete
M1-M6 scope remains in the
[design plan](../plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md)
and [M1 acceptance document](../../tests/acceptance/2026-10-16-macro-m1-token-foundation.md).
