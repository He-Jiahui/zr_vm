---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_scalar_result.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_instruction.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_locals.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - tests/parser/test_lambda_semantic_ir_isolation.c
  - tests/parser/test_lambda_local_range_cases.h
  - tests/parser/test_typed_call_binding_member_semantics.inc
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_dynamic_object_local.inc
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_typed_call_binding_member_semantics.inc
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_scalar_result.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_lambda_semantic_ir_isolation.c
  - tests/parser/test_lambda_local_range_cases.h
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_dynamic_object_local.inc
doc_type: module-detail
---

# Lambda Semantic IR Isolation

Each callable owns its semantic IR, place graph, CFG, source map, slot identities
and supporting facts. Compiling a child lambda must not append its body facts
to the parent or reuse the parent's slot identity. Its outer closure creation
still belongs to the parent bytecode, constants and execution stack.

## Local Metadata Range Witness

The focused companion `test_lambda_local_range_cases.h` keeps the original
real-source metadata witness and all ten test registrations. It exercises
zero and INT32_MAX positive boundaries, then independently pairs each signed
AST coordinate -1 with unsigned metadata UINT32_MAX. All four outcomes are
collected before assertion so an early failure cannot hide another coordinate.
The intended predicate rejects negative coordinates before unsigned conversion;
blindly casting would retain the false match. The first frozen GCC RED run
retains the old predicate. Focused attempt r1
was interrupted at80/554 before native execution; its driver exit and the
interruption actor are unknown. Attempt r2 reuses the same build and completed
objects under new immutable report stems. R2 now naturally builds949/949 with
exit0, then nativePID4397 executes all ten ordered cases in421ms: nine PASS and
only the final intended boundary assertion FAIL, exact mask0xF, zero ignored.
All preceding real-source assertions and positive controls are reached. Both
lambda source pre/post hashes and the native binary remain unchanged.

The raw r2 receipt remains FAIL, SHA-256
`7B2F91868161B87E4BCDE56392F08330004C85E24826E5B5CE13ED31052E2B09`,
because66 global inputs change during the long build, including foreign shared
code and two authorized independent LSP test paths. This is a dated native
counterexample, not a source-stable platform pass. Core read-only review
supports the exact comparator repair: reject four signed negatives, then
compare UInt32 conversions. Only that comparator changes; the header is now
D6FA7297 while main stays915FE17B. Normal same-build GREEN r2 naturally fails
at837/838: ranlib reports No space left on device. Build490 exits1, no native
test starts, and source-post3922 exits0 with61 global input changes; both own
lambda pins remain unchanged. ReceiptFF5CFF5C remains FAIL, not GREEN.
Only the known failed100524694-byte archive is removed through the guarded
lifecycle plan; reusable objects and all raw evidence remain. After capacity
recovers, normal same-build GREEN r3 completes 951/951 steps with build PID 539
exiting 0. Native PID 4455 exits 0 with all original ten ordered cases PASS,
zero failures and zero ignored. Both lambda source pre/post pins and binary
6630345A (41217544 bytes) remain unchanged.

The r3 receipt at
`E:/cargo-targets/zr_vm/reports/m1-gcc-range-witness-green-r3-receipt.json`
has SHA-256
`580B32C2C240F2C720C238BEF4644066AD6D40F126AA528A54357A3C61B0AECB`.
Its raw state remains FAIL: source-post PID 4456 exits 0 but records 62 global input
changes, and head_post is absent. Root/core independently accept only this
dated functional GREEN, not a source-stable GCC or M1 pass. No gate is weakened
or raw receipt relabeled; parallel foreign source editing remains the user's
choice.

## Block Type Environment Restoration

Real consumer-local regressions expose a separate block boundary defect: both
block statements and expressions enter an execution scope without creating a
child type environment. Registering an inner variable with the same name then
replaces the outer binding in the legacy type environment. Canonical public
symbols and IR places still distinguish both declarations, so a successful
query alone does not prove that subsequent name resolution is restored.

The pre-repair 24-case baseline naturally completes with 21 passes and three failures at
the outer binding restoration assertion: successful expression block, successful
if-statement block, and a failed expression block. The last requires an actual
inner IR local before reaching a unique missing-identifier diagnostic, excluding
an earlier parse/import failure. The original22 cases and observation cleanup
remain unchanged. LLDB observes outer symbol4/FUNCTIONtype3 and inner symbol5/
INTtype2, with only the inner name binding left in the current environment.

The production repair reuses `SZrCompilerCallableTypeScope` in both
block compilation entries. Its owner creates a child environment sharing the
canonical context and parent lookup chain. Every entered normal/error path
first runs execution-scope cleanup while the child is available, then restores
the parent and frees the owned child even when compilation has failed. The
existing helper and global execution scope functions remain unchanged.

Root and independent peer approve the exact two-function change. The historical
twelve-target replay completes 285 tests with two failures: typed binding passes
24/24, alias hygiene 15/15 and pre-semantic IR 121/121, while lambda isolation
completes 9/2. The two failures inspect the actual child function's typed local
metadata: declared integer and double initializer locals fall back to OBJECT.
All 69 source/launcher pins and HEAD remain stable in that replay, and every
native process exits naturally. The following metadata regression isolates the
remaining lifetime boundary; original integration acceptance remains pending.

## Typed Local Scalar Metadata (2026-10-15)

`compiler_build_typed_local_bindings` reads the real execution local's stack slot
and copies its semantic slot identity into the function's typed local row:
TypeId, SymbolId, PlaceId and declaration coordinates. Lambda compilation keeps
the child semantic IR alive through this metadata construction. The shared
semantic context also owns the canonical type pool until compiler cleanup.

The block owner restores and frees the body type environment before the lambda
builds its typed locals. A later name lookup therefore cannot recover that
declaration's type reliably: an absent name falls back to OBJECT, while an outer
binding with the same spelling can supply a different type. The semantic row
still carries the actual child identity in both cases.

The appended real-source regression normally compiles a parent `value: float`
with canonical DOUBLE and a child `value: int` initialized to 42. It checks the
child's actual metadata against public SymbolAt/DeclarationOf results, canonical
INT64, the real identifier range and a valid child place. The parent binding and
public identity must remain DOUBLE. The existing complete parent IR snapshot
assertion runs while the fixture is live. Observations are then reduced to
scalars, the idempotent teardown releases snapshots/compiler/AST/VM, and final
assertions check every prerequisite before the child's legacy base type.

Historical RED-r3 completes 10 unique cases, three failures and zero ignored, with
natural native exit 3. The original integer and double cases require base types
5 and 11 but receive OBJECT 18. The new same-name case passes its canonical,
public identity and parent-restoration prerequisites, then requires INT64 5 but
receives the parent's DOUBLE 11. This distinguishes a false successful name
lookup from an ordinary missing-name fallback. Binary 9D90E6F1, native log
6AE1AB6E and final build log 2228A5EC have full identities in the task manifest.

The reviewed metadata change gives the existing semantic TypeId priority after
injected-receiver handling and before name lookup. Pure `CanonicalType_Find`
must find a PRIMITIVE node accepted by the existing NULL, BOOL, NUMBER or STRING
predicates. The builder initializes the normal unknown type reference, copies
the exact primitive base type and finishes that row. It neither allocates nor
interns a type and leaves the semantic identity fields unchanged.

The subsequent nominal hook runs after this scalar check and before name lookup.
`typed_metadata_find_plain_nominal_type` accepts only a NOMINAL node with no
module identity and a matching TypeId in the same context's semantic type records.
The inferred row must be OBJECT with a name, no element types, TYPE generic
argument kind, and the exact canonical name and definition token. Ownership,
GC bridge and reference access must be NONE; readonly and nullable qualifiers
must be absent. The helper borrows that row during metadata assembly and the
existing converter copies its reference. It allocates and interns nothing.
FUNCTION, wrappers, imported or unmatched nominal types, generics and opaque
shapes retain the existing lookup/parameter/slot-hint fallback.

Root and independent peer approve the primitive baseline at
`37A70BA70E00BD422355B063EA64C30220CA414B54AB6B598CEDB42FE9030F3C`
(113975 bytes, 2711 LF, zero CR). Its only change is 16 LF/776 ASCII bytes; the
whole RAM inverse restores E24ABD04 exactly and finite forward reconstruction
matches every baseline byte.

The subsequent production candidate has Root/peer SOURCE PASS at
`475D8D8CFD3E6EF26414849DB27DE42DBB4BA43C0152F2B5CACD76B4C82AF914`
(115687 bytes, 2747 LF, zero CR). Its 36 LF/1712 ASCII bytes comprise the
30-line nominal helper and six-line hook. Whole-RAM inverse restores 37A70BA7
exactly and finite forward reconstruction matches every current byte.

Lower-r4 passes all 286 native cases and all twelve processes exit naturally,
but its driver exits 1 because two foreign Math files change during the run.
That evidence is retained separately from the completed source-stability gate.

Earlier lower-r5 (2026-10-15) exits 0 with build exit 0, twelve natural native exits 0 and
286 tests/zero failures/zero ignored. Lambda isolation passes all ten cases,
including both original primitive locals and the same-name INT64 child.
Unique test rows match every expected footer count. All binaries, all 76 source
pins and HEAD `af5f4a7bc7de7d22ba545e91dffd982c2d3454bf` remain stable.
The lambda log SHA256 is
`0A1A79D4832B2551E8F78EF3986427B13165145A21FA1541D728A8B78B10B92C`;
its binary is
`AFE16F89DAB1E06D8259C8BD31A4502F34A50D2F03E9C347960B909BC519F2A2`.
The complete final build log has SHA256
`9819DB7BFEE856C4986E0C7E9D0E3D1F16E5B25C83C557AC14D209BD5B24872B`.
The historical original127 replay before 475D completes 120 PASS/seven failures;
focused support RED-r1 completes 33 cases with 30 PASS/three failures.
New source-reviewed nominal and four-array cases then produce RED-r2 35/5/0:
the same-name nominal child expects OBJECT 18 but receives parent DOUBLE 11;
the W2 case executes to integer zero before its missing-FILL assertion fails.

Fresh lower-r6 completes 14 targets and 321 cases: 320 PASS, one failure and zero
ignored, build exit 0 and wrapper exit 1. All 100 source pins, HEAD, unique rows
and binaries remain stable. The prior twelve targets retain 286/0/0 and escape
metadata passes 14/0. The new nominal child now has OBJECT 18, TypeId 23 and
name CallbackHandle, while the parent remains DOUBLE 11/TypeId 18. Seed/capture
escape flags are 258 and the native flag is 256. W2 preserves its old 20 passes;
only the new public four-array FILL assertion fails after FINE/integer-zero execution.

Original127 has not been replayed after 475D. Original51 and platform obligations
remain open; the latest WSL true probe exits -1 with E_ACCESSDENIED. M1 remains
unaccepted and M2-M6 have not started. The foundation ledger retains full receipts.

## Existing Boundary and Observed Failure

Named functions and class members already use the private
`compiler_semantic_ir_isolation_begin/end` pair. The observed lambda path saves
ordinary compiler arrays and resets the child execution stack to zero, but
does not isolate the semantic sidecar. Native tests observe integer and float
child-local initializers appending two places to the parent's three-place
graph. The same mutation remains after a child reaches a real invalid `break`
diagnostic. Both successful child metadata checks and the failure diagnostic
precede the parent-state assertion, so these are actual ownership failures.

Direct return expressions already have their own isolation in
`compile_statement.c`: the original five controls pass. The earlier slot-zero
collision prediction did not account for that boundary. The ordinary local
initializer path exercises the whole-body sharing problem. Scalar-result
binder guards remain part of the existing contract; they are not weakened to
accept a child place in its parent's graph.

## Actual Parsed Fixture

CTest `macro_lambda_semantic_ir_isolation` uses the existing 30-second timeout.
The first five standalone cases parse actual ZR source and compile real AST nodes:

- A named function provides a control for the existing isolation path.
- A float lambda checks complete parent semantic-state preservation.
- A parameterless integer lambda checks the real parent LOCAL at slot zero
  before compiling the child literal at its reset execution slot zero.
- Two sibling lambdas check that neither child's facts remain in the parent.
- A captured integer followed by a parent assignment must compile and return8.

Three companions use actual child-local integer/float initializers and a child
that initializes a local before a rejected `break`. Successful children must
publish their own typed local metadata. The rejected child must report the
canonical diagnostic and the actual break AST's exact seven-field range before
the fixture checks full parent-state restoration. Source `float` is the
canonical DOUBLE type; only an explicit `f32` maps to FLOAT.

The seed variable is compiled normally with canonical INT64, a real symbol,
place/value IDs and its real source range. The fixture reads its actual local
slot after normal function predeclaration. It does not assume that the named
control's seed occupies zero: predeclaring that control's helper uses a slot.
Only the parameterless integer collision case explicitly requires seed zero.

Snapshots cover semantic IR arrays, nested place projections and CFG edges,
source maps, loans, escape/contiguous/bounds/scalar-scratch facts, slot/receiver
records, graph flags and IDs. They compare headers, allocation identities and
contents. These are in-memory assertion buffers, not filesystem source copies.
Ordinary parent bytecode, constants, child-function lists and stack counters
are excluded because creating a closure legitimately changes them.

## Ownership and Current Validation

Unity teardown releases registered assertion buffers, compiler state, any live
parser state, AST and runtime even after an assertion exits through Unity.
Functions are raw GC objects owned by the runtime; the fixture does not free
them a second time. No generated source or archive fixture is created.

Independent review found a seed-slot prerequisite error before native testing.
The corrected fixture71EB2A6C has496 LF lines and five cases. Root's read-only
six-fragment inverse restores its complete prior22377-byte hashF56AAFD0.
Review confirms that the current outer `CREATE_CLOSURE` emission changes only
ordinary execution arrays, so the full semantic-state equality assertions
permit that legitimate work. First native7350 naturally exited1/build0/test4:
the first four cases fail a range-containment prerequisite, while the captured
lambda/store execution control returns8. The declaration node's range covers
the `var` keyword; the fixture incorrectly assumed it covered the entire
declaration and its real identifier binding range. No child isolation assertion
was reached in those four cases. The r1 log37E7C18A remains immutable. A
fixture-only correction grounds the range in the actual binding/identifier;
fresh r2 evidence is preregistered. Production remains4DD34B08 unchanged, and
no production repair is authorized by this prerequisite failure. Corrected
fixture96EAAC6E uses exact identifier/binding/place range comparisons and the
legacy input-string API convention. Native94659 naturally exits0/build0/test0,
all5PASS/0FAIL/0ignored, logF504FE6C. The actual return-expression path has its
own semantic isolation at `compile_statement.c`, which the initial collision
hypothesis omitted. These passing controls do not establish a whole-body
lambda failure. Further real child-local initializer and error-restoration
companions now form an eight-case fixture7A4B0B07/29982B/642LF. They compile
actual integer and float child-local initializers, check the generated child's
typed local identity, and require a rejected child to reach the real out-of-loop
`break` diagnostic before checking parent restoration. Original five cases are
unchanged. Root's complete RAM inversefa834b and independent peer review restore
fixture96EAAC6E exactly. Fresh r3 logs are preregistered; production remains
unchanged and no repair is authorized until an actual relevant failure is
observed.

Native r3 session21046 naturally exits1/build0/test3: integer and rejected-child
cases reach the parent assertion and fail places3-to5, while the float case
first reveals the incorrect FLOAT expectation. The actual annotation producer
maps `float` to DOUBLE. A one-enum fixture correction27FA953E/29983B has a
complete RAM inverseffe2c9 restoring7A4B0B07 byte-for-byte. Native r4 session34389
then naturally exits1/build0/test3, with five PASS and three genuine parent-state
failures, zero ignored. Its complete logC031B66B is independently reconciled.

The bounded repair reuses the existing isolation lifecycle around lambda
parameters and body through child typed metadata construction. Closure analysis
must read the saved parent graph, and unified cleanup must restore it on both
success and failure. Frozen candidate04EC2C2F/56518B/1082LF preserves all549
original CR bytes. Four-fragment Root RAM inverse9b0f95 restores the complete
56092-byte preimage4DD34B08; independent peer review verifies the same inverse
and all common cleanup paths. Child local/closure metadata is serialized before
the child graph is freed. Later parameter/frame builders consume AST and stored
metadata, while canonical types and symbols remain in the shared semantic
context.

First GREEN native9999 naturally exits0/build0/test0. All eight records PASS,
with zero failures and ignored cases, complete log224E0A24. Source/test pins
are unchanged after exit, and independent review reconciles the entire footer.
The captured-lambda path still executes8. This focused gate starts from an
ordinary real parent seed; it does not inject OOM, active catch/finally or loan
state. Full native61000 naturally exits1 after original126 outputs build0 and
foundation50/50 PASS in33.84s. Remaining121 programs are110PASS/11FAIL. Complete
comparisona6212b intentionally exits1 because two original programs improve:
literal_surface57/1-to0 and lexer_parser_compiler_execution14/1-to0. Its eight
differences are only their exits, footers and failure removals; all other119
pairs match the prior definition-environment run, with no missing footer or
unaccounted failure. Source/test pins remain unchanged. This repairs the real
cast-fixture initializer failures while broader M1 acceptance remains open.

## Conditional Parent Stores (2026-10-13)

An appended source case preserves the original public enum equality condition,
captures an integer through an immediately invoked lambda, and assigns the
parent local inside the conditional arm. Its expected result is8. All original
eight cases remain. The first rebuilt run executes9 cases and fails only the
new source compilation with the existing STORE lowering diagnostic; the
straight-line captured-lambda assignment still passes.

Three appended lower API tests use actual parsed declarations and assignment
nodes, real binding/symbol/context identities, and production isolation to
clear the current IR slots. They compile RHS42 as a defined INT64 CONSTANT.
The inactive untyped STORE fails after every prerequisite; explicit INT64
slot rematerialization and actual BOOL/begin_if ACTIVE refusal pass. The full
original118-case suite remains and the candidate executes121/1/0. ACTIVE
refusal is an internal API boundary, not a claim that legal ZR source must fail.
All observations are captured before fixture cleanup and behavioral assertions.

The bounded STORE candidate reuses the existing LOAD policy before attempting
typed destination materialization: an initialized compiler, non-NONE RHS,
missing destination slot, inactive CFG, real indexed local identity and a
context binding with a valid symbol but INVALID canonical type. It emits the
normal SET_STACK instruction. A valid untyped declaration remains queryable;
registration success does not imply that it can produce a typed IR slot.
Typed destinations and ACTIVE CFG retain their original lowering path.

The candidate does not construct a type or place from an unknown producer,
alter the shared materialization contract, or recover from arbitrary lowering
failure. Normal bytecode ownership and execution budget accounting still apply.
Contiguous-view lookup requires a current slot; the no-slot branch has no
reachable destination view fact. Existing typed slots retain normal propagation,
and historical facts belonging to other place IDs remain intact.

The author, Root and independent core reviewer verify the whole raw inverse:
the only change is12LF/513B, and every other byte restores the original98939B
module. Rebuilt native39312 naturally exits1 because integration remainsFAIL;
build0 and all seven lower targets pass311 cases/0ignored. This includes121
pre-semantic IR cases,9 lambda cases and the new source's actual result8.
Cleanup62, resources84, exceptions15, abrupt exits7 and comptime aliases13
also pass. The actual production hash stays0B2AD1FD after execution. The
original two RED logs and all full-foundation, remaining-program and platform
obligations are retained; this is focused support GREEN, not M1 acceptance.

See [foundation validation](../testing-and-validation/macro-foundation-validation.md)
for actual native results and remaining M1 obligations.
