---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.h
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_return.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_return.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc_roots.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - tests/parser/test_aot_c_dynamic_return_representation_cases.h
  - tests/parser/test_aot_c_scalar_call_result_proof_cases.h
  - tests/parser/test_aot_c_guardrail_contracts.c
  - tests/parser/test_semir_dynamic_call_deopt.c
  - tests/parser/test_semir_call_result_type_identity_cases.h
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_aot_c_method_info_return_projection_cases.h
  - tests/parser/test_aot_c_direct_inline_return_layout_projection_cases.h
  - tests/parser/aot_c_ownership_call_result_projection_cases.h
  - tests/parser/test_ssa_source_script_entry_identity.c
  - tests/harness/path_support.c
  - tests/harness/path_support.h
  - tests/harness/runtime_support.h
  - tests/third_party/zr_unity/Unity/src/unity.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.h
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_return.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_return.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
plan_sources:
  - "user: 2026-10-16 document raw callable-return metadata, projected ExecIR, AOT C signature representation, controlled regressions, and actual incomplete Windows evidence"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
tests:
  - tests/parser/test_aot_c_dynamic_return_representation_cases.h
  - tests/parser/test_aot_c_scalar_call_result_proof_cases.h
  - tests/parser/test_aot_c_guardrail_contracts.c
  - tests/parser/test_semir_dynamic_call_deopt.c
  - tests/parser/test_semir_call_result_type_identity_cases.h
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_aot_c_method_info_return_projection_cases.h
  - tests/parser/test_aot_c_direct_inline_return_layout_projection_cases.h
  - tests/parser/aot_c_ownership_call_result_projection_cases.h
  - tests/parser/test_ssa_source_script_entry_identity.c
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
doc_type: module-detail
---

# AOT C Return Representation

## Purpose and Current Status

A callable's compiler-owned return type, its projected AOT execution metadata,
and the C representation emitted for a signature are related but distinct
contracts. A scalar body helper is not permission to rewrite the compiler's raw
type or source identity. Conversely, retaining raw DYNAMIC metadata does not by
itself settle whether a C signature may use a separately proven scalar
representation.

The original [method-metadata writer](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c)
executed in r4/r5 had SHA256
`9F2CCB523850C93801B6272505CF16745227B2C3C74756016E06067A08B633DA`.
The bounded candidate is now frozen at
`21CE3C18E401B3F76B97D943DF1CDC421D779408AB76BC8A7F1B6166BB848B3B`,
with independent static review and a dated r6 default-AOT result of 17 PASS /
0 FAIL / 0 IGNORE. The two new tests were first executed in observation r4. The new
positive stopped at a fixture prerequisite, not its representation assertion;
the negative test completed its five controlled shapes. The receipt remains
FAIL. The subsequent fixture correction is frozen at header SHA256
`113BB20E4DE4CD8B163354FE91CFA20F5BC2DAD203A1E1D8BF1D38D3B4C3396F`.
Fixed-input observation r5 then reached the positive's actual representation
assertion and failed there. The bounded writer repair was authorized after
that genuine RED and executed in r6: old scalar, immutable positive, the negative
case's five shapes, parameter4, Stamp, and owned-copy all passed. This is focused
test evidence, not native compilation of the emitted C or complete ABI validation.

R8 later retained AOT17 PASS while query72 and ownership25 also passed after only
two PRIMARY parent-range assignments changed from r7. The full Windows r11 still
finished FAIL, including a bool typed-call value-free guardrail failure. This
does not expand the bounded signature-copy contract into complete incoming ABI
support or supply a validated mixed-source projection implementation.

The later R11-support R4 executed AOT18 with 17 PASS / 1 BOOL proof FAIL and
guardrail6 with 5 PASS / 1 BOOL value-free FAIL. The original representation,
parameter, Stamp, and owned-copy passes do not waive that remaining failure.
The later R6 executed the unknown/explicit-OBJECT SemIR regressions and reached
two real behavior failures while retaining the single BOOL failure in each AOT
suite. R7 then passed AOT18 and guardrail6, but its OBJECT test expectation and
mixed-postfix behavior still failed, so its strict receipt remains FAIL.
The user-approved original TokenSpan/AST-identity/diagnostic-anchor direction
has a real mixed-postfix counterexample, not a completed repair.

This topic does not grant complete callable ABI support, platform acceptance,
Linux acceptance, M1 acceptance, or commit credit.

## Checked-Addition Guardrail Fixture

The original full GCC R10 and Windows R3 runs reached a real failure in the
sixth existing AOT guardrail case: it expects checked arithmetic state. The
fixture still declared and called a bitwise `combine_i64`, despite the preceding
case requiring `ZrCore_Integer_Add`. Only three fixture lines are restored to
`add_i64` and `left + right`; all seven case names, order and assertions remain
unchanged. This correction does not change the C emitter or relax the checked
operation contract. Fresh execution of the normal seven-case target is pending;
the earlier full-matrix failures remain failures.

## Layer Responsibilities

| Layer | Owned input and output | Boundary |
| --- | --- | --- |
| Source compiler | SemanticIR/CFG proof to raw `SZrFunction.callableReturnType` and source callable identity | Preserve the compiler's actual semantic contract |
| AOT ExecIR projection | Copy a known raw return type into `SZrAotExecIrFunction` and validate frame/typed-return evidence | Do not read later raw mutations as if they were projected metadata |
| C method-metadata writer | Select a signature return row and connect it to `SZrAotSignature.returnType` | A C representation decision must not mutate the raw function or source identity |
| C body lowering | Prove the returned slot at the specific instruction and select a scalar, value, or inline-struct return path | Signature text cannot substitute for body, ownership, or frame proof |

The data structures are declared in
[function.h](../../zr_vm_core/include/zr_vm_core/function.h),
[backend_aot_exec_ir.h](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h),
and the public [AOT ABI header](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h).
Semantic TypeIds and `staticCTypeId` frame-layout IDs have different meanings;
one must not be invented from the other to make a signature look complete.

## Raw Script Return Metadata

[compiler_script_callable_return.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c)
publishes a deliberately narrow script-entry witness. It requires the actual
top-level script/current function, no parameters or varargs, and no unsupported
imports, exports, child functions, closures, or submission context. Its
pre-execution SemanticIR and CFG must be initialized and validated.

The proof walks reachable CFG blocks with an iterative DFS. It checks supported
value/place/branch instructions, well-formed edges, terminal return operands and
their definitions, and one compatible canonical INT64 type across proven
returns. Cycles and unsupported effect/ownership shapes are not promoted into
this witness. Malformed proof inputs fail publication; an unsupported shape may
leave the return metadata unproven rather than inventing a result.

On a proven result, the compiler converts an inferred INT64 type through
[compiler_typed_metadata.c](../../zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c).
This is not an instruction to force raw `staticCType` to I64. The existing
[script-entry identity regression](../../tests/parser/test_ssa_source_script_entry_identity.c)
explicitly requires its eligible INT64 witness to retain raw
`ZR_STATIC_C_TYPE_DYNAMIC` and layout ID zero, together with its full type and
source-identity fields. That regression contract must remain intact.

The witness is not a universal promise that every source returning an integer
has the same complete raw struct. A test must establish its actual compiled
fixture before attributing a later C-signature mismatch to production behavior.

## ExecIR Projection and Current Signature Selection

[backend_aot_exec_ir.c](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c)
rejects a noncanonical boolean return-type flag. When the flag is exactly TRUE,
it copies the complete raw type into projected `callableReturnType` and sets
`callableReturnTypeKnown`. The accessor returns that projected member only when
the known flag is exactly TRUE. Frame construction and direct inline-return
layout projection remain independent required steps of module construction.

The original r4/r5 C signature selector first asks that accessor for a known projected
type. If present, it uses the type as-is, including DYNAMIC. Only when no known
projected type exists does it attempt scalar inference. That historical version
did not refine a known DYNAMIC type into I64 for signature emission. The current candidate
retains that accessor boundary and attempts only the bounded refinement below.

The writer emits the selected return row at index zero of
`zr_aot_signature_N_types`, then points `zr_aot_signature_N.returnType` at that
same row and sets `hasReturnValue`. Parameter rows and their passing modes are
separate. Finding an I64 token anywhere in generated C is therefore insufficient
to prove that the actual return row is correct.

## Existing All-Return Scalar Proof

The method-metadata writer's existing fallback scans every explicit execution
`FUNCTION_RETURN`. At least one must exist, and every encountered return must
pass the same scalar proof for its source slot at its instruction index. It
tries I64, BOOL, U64, and F64 in that order. One proven return does not authorize
ignoring a different or unproven return.

The proof uses
[scalar-local analysis](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c),
not the desired signature text. The relevant predicates require a matching slot
kind and a value proven written before the return. They reject unsupported
constructor, exception-handler, and exported-slot cases. Some direct-return
predicates also require matching projected callable metadata; the inference
predicates deliberately have their own requirements. These are existing local
proof rules, not a repository-wide scalarization or exception-safety guarantee.

## Body, Ownership, and Typed-Return Boundaries

[backend_aot_c_typed_return.c](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_return.c)
selects body return helpers at the actual return instruction. A proven I64 slot
could use the I64 helper in r3-r5 even when the old signature selector retained
known DYNAMIC metadata. This explains how signature and body text could disagree
without proving that raw metadata should be rewritten.

Owned Shared/Weak call results, copied values, nullable values, and arrays must
not be treated as plain scalar slots solely because an earlier value in a reused
slot was scalar. The existing
[owned-call-result regression](../../tests/parser/aot_c_ownership_call_result_projection_cases.h)
checks this separate projection boundary. A C-signature change cannot bypass
that ownership proof or repair a missing frame identity.

`RETURN_TYPED` has a different path. The
[inline-return layout projector](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.c)
checks every typed return's type-table entry, actual inline frame slot, semantic
identity, and resolved layout. Unknown, union, alias, or differing compatible
layouts do not automatically become one published direct-inline ABI. Invalid
metadata is rejected rather than accepted as an arbitrary scalar return.

The [value-SemIR dispatcher](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c)
and its [typed-return consumer](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c)
also require the caller to permit typed return and require a matching proven
inline layout. The new scalar tests neither relax these checks nor establish
complete struct, generic, alias, capture, or owned-return ABI support.

## Missing And Explicit Call-Result Types

The separate [SemIR producer](../../zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semir.c)
must distinguish absent result-type evidence from an explicit OBJECT binding.
The current `semir_init_default_type_ref` initializes the missing entry with
base UNKNOWN, C representation DYNAMIC, layout ID NONE and an unused OBJECT
element. This is a bounded metadata default, not permission to scalarize an
unknown result. An explicit OBJECT binding must remain a separate type entry
and valid nonprimitive evidence for the
[call-result proof reader](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_call_result.c).
The existing producer annotates explicit OBJECT with C representation GC_REF;
it is not required to remain plain DYNAMIC like missing UNKNOWN.

The new [type-identity header](../../tests/parser/test_semir_call_result_type_identity_cases.h)
adds two cases to [SemIR's existing main](../../tests/parser/test_semir_dynamic_call_deopt.c),
giving four ordered cases without removing the original two. These are controlled
single-ExecBC-instruction producer fixtures, not source compilation tests. They
use the real SemIR metadata builder and ExecIR module projection. One has no
result binding; the other registers an actual OBJECT binding/local through the
compiler APIs and builds typed-local metadata. They do not fake the produced
SemIR type table. Static fixture cleanup releases the projected module, compiler,
function and leases before destroying the state after a Unity jump.

R6 failed the missing case with `Expected 23 Was 18` at the UNKNOWN base assertion;
the explicit OBJECT case failed because no separate missing-UNKNOWN entry was
found. Both had passed their real producer/projection prerequisites. After the
default-UNKNOWN repair, R7 passed the missing case but failed the explicit OBJECT
test's shared plain-DYNAMIC assertion (`Expected 0 Was 12`, header line 160).
That new-test expectation conflicts with the existing OBJECT-to-GC_REF annotation.
It is not evidence of a production dangling pointer, nor proof that the later
OBJECT identity/nonprimitive checks completed. An exact expectation/by-value
identity correction is being prepared; it has no R8 result here. No raw callable
return is rewritten to satisfy this control, and no ABI or ownership gate is
relaxed by distinguishing UNKNOWN from OBJECT.

## Controlled Regression Design

The new
[test header](../../tests/parser/test_aot_c_dynamic_return_representation_cases.h)
adds two Unity cases through the existing
[method-info main](../../tests/parser/test_aot_c_method_info_signature.c).
The dated r4 inputs were header SHA256
`F8FE8423FB4831D43F2D14ADF195E8A4DDED7F55D04923DE12805EC9A08EFEA7`
and main SHA256
`34A924A51B5BB7F01A4121F4EA44C430883DEBEEAAFE8372E7967ACF52D6ABB7`.
Those identify that executed version, not a later authorized fixture revision.

The positive uses public `ZrParser_Source_Compile`, roots the returned function,
and requires a script entry with no child functions. It snapshots the full raw
typed-return struct and all source callable-identity fields, including their
presence flags. Its intended behavior check compares the actual signature-zero
return row with a copied expected type whose C representation alone is I64,
checks that the signature points to row zero, checks the I64 body helper, and
requires the raw type and source identity to remain unchanged.

In the r4 version, a prerequisite first compared the compiled raw return against
a fully initialized plain-DYNAMIC expectation. That compound expectation failed.
It does not establish which individual field differed, and the later signature,
body, and unchanged-metadata checks were not reached in that positive case.

Read-only producer and regression inspection then identified the incorrect
expectation: the raw script producer zero-initializes `staticCTypeId`, while the
general test initializer assigns `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`
(`0xFFFFFFFF`). The existing script-entry test explicitly requires raw ID zero.
The corrected positive changes only its expected field to zero; it preserves
the complete nine-field prerequisite and full raw/source-identity snapshot.
It does not rewrite the actual function to resemble an expected layout type.
This correction does not retroactively make r4 a behavior RED. The separate r5
run below provides the first actual new-positive representation RED.

The negative case loops over five controlled writer-input shapes:

| Shape | Deliberate input distinction |
| --- | --- |
| Base mismatch | BOOL metadata with the integer source body |
| Nullable | Nullable INT64 metadata |
| Array | ARRAY metadata with INT64 element metadata |
| Owned | Shared OBJECT metadata |
| Partial proof | A real source retaining at least two returns, then controlled INT64 metadata |

These inputs are built from real compiled functions, then their raw return
metadata is intentionally changed at the writer boundary. They are not five
additional Unity cases, nor evidence that the altered type agrees with the
source's canonical semantic identity. The test requires the writer to preserve
the controlled fields and the snapshot across emission. Any public-writer
rejection is explicitly `fixture prerequisite`, never a successful negative
control or a representation behavior RED.

The existing selector `--generic-struct-return-only` still runs only the Stamp
case. Default registration preserves the original Windows 15 / Unix 18 ordered
cases and appends these two, giving Windows 17 / Unix 20 at the r6/r8 boundary.
The later R11-support R4 appended the
[plain BOOL call-result proof case](../../tests/parser/test_aot_c_scalar_call_result_proof_cases.h)
and executed Windows 18. It reached its proof behavior assertion and failed;
planned additional controls do not receive execution credit from that run.
The selector is not a
replacement for those defaults or the complete matrix.

## Fixture Lifetime and Output Ownership

The fixture keeps state, function, root lease, generated-text buffer, and path
in static storage. The [public writer entry](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c)
is called with source input and executable lowering required. Output paths and
text reading use the existing
[path harness](../../tests/harness/path_support.c); the tests write/read generated
C but do not compile that C or run the VM.

The shared cleanup frees the text buffer, releases the temporary GC root lease,
frees the function, destroys the runtime state, then zeros the fixture. The lease
API in [gc_roots.c](../../zr_vm_core/src/zr_vm_core/gc/gc_roots.c) accepts an empty
lease during cleanup. The main invokes cleanup from `tearDown`, so a Unity
assertion longjmp does not strand these new fixture-owned resources. The
[Unity runner](../../tests/third_party/zr_unity/Unity/src/unity.c) protects teardown
separately from the test body. This local lifecycle does not claim to repair all
older cases' local-resource cleanup or provide a global OOM transaction.

## Actual Evidence and Open Work

The dated r3 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-mixed-r3-receipt.json`,
SHA256 `0384CAD8E9B52ECF1E08B36D57C441924635F3118D23A7589CF0CC414AEA5E7B`.
Its Windows default AOT run was `15 Tests 1 Failures 0 Ignored`. The remaining
old scalar assertion expected I64 in the `i64_neg_expr` signature, while the
observed signature retained DYNAMIC and the body used an I64 return helper.
That is a bounded representation discrepancy, not new scalar GREEN or proof of
complete native ABI support.

Observation r4 is recorded by
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r4-receipt.json`,
SHA256 `EF7C4826C3578AD1D624D1E84C6F435F7AC08F6728DE40281308A8E3D26C5E44`.
Its AOT raw log SHA256 is
`E2DB694C75CD5D1346E1DCF7E06A3518FD1D5429E32803CA9117B561B556F37A`.
The actual result was `17 Tests 2 Failures 0 Ignored`: the old scalar behavior
failure remained, the new positive failed its plain-DYNAMIC fixture prerequisite,
and the negative case completed all five shapes and passed. The new positive is
not a behavior RED and has no downstream assertion credit.

The r4 receipt records 4800 unchanged source inputs, unchanged HEAD
`2e2c95d004317014334c9a09de599734d8da9746`, stable support6/binary3, and naturally
terminal children, but remains FAIL. Its companion query run was 71/5 failures
(old64 plus using2 passed; weak4 prerequisites and direct-degrade behavior failed),
and ownership had 25 ordered results, 22 PASS and three weak failures. Those
companion diagnostics do not grant this return-representation module acceptance.

Observation r5 is recorded by
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r5-receipt.json`,
SHA256 `A84C6B57F6E8A722F625E559756D53FD3C4287221F7A36AE425D7A365649C938`.
Its AOT raw log SHA256 is
`F0E53C0A98CA009A7D2BA225B67312E2EB47A1E3F7894083A4125A09606C03E4`.
All 4800 source inputs, HEAD, support6 and binary3 were unchanged and all eight
children terminated naturally. Default AOT again completed 17 tests with two
failures, now both actual scalar behavior failures: the old assertion and the
corrected positive's exact representation assertion. No fixture prerequisite
failed; the negative case's five shapes and all other 14 cases passed. Query
completed 71 tests with one weak owner-set behavior failure; direct-degrade,
using2 and weak3 controls passed. Ownership remained 22 PASS / 3 FAIL. The
controller's exit zero confirms only these preregistered controlled observations,
not an overall Windows, Linux, or M1 pass.

The candidate changes only the known-return selection branch. It requires
DYNAMIC, nonnullable, unowned, nonarray, unnamed metadata with an unused OBJECT
element and no element name; its unused layout ID must be zero or NONE. The
existing complete all-return scalar proof must succeed and its base type must
exactly match the known semantic base. A successful refinement copies the entire
known nine-field struct to a local C-signature value and changes only `staticCType`, retaining
the original zero/NONE ID and every other field. Known-static and unknown-return
branches, body proof, raw/projected metadata, identity and typed-return validation
remain unchanged. No nullable, array, owned, mismatched-base or partial-proof shape
may acquire scalar representation merely from its requested type.

Root and a separate core reviewer read the complete selection/proof branches;
the author's exact RAM inverse restores the original writer hash. These static
checks alone are not native acceptance. Actual r6 used the same normal three
targets and default query71 / ownership25 / Windows AOT17, without reducing cases
or weakening its full source/HEAD/support/binary gates.

The r6 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r6-receipt.json`,
SHA256 `353AE08DEE38402F1C851F4E608D989B4F48ACD5091E472476A2CF56B3B767E2`.
Root handle `40266` naturally exited 0 and was drained; normal build `29420`
exited 0 and all eight children terminated naturally. All 4800 whole-source
inputs, HEAD `2e2c95d004317014334c9a09de599734d8da9746`, support6, and binary3
were stable. AOT native `37788` exited 0 with `17 Tests 0 Failures 0 Ignored`;
its raw log SHA256 is
`E478A6F9ACCCF08DCA39EF4D26A29CB38EFCBA1B5E1E85A6F84B32E0DC81BF63`.
Both scalar behavior regressions reached their assertions and passed without
fixture failures. The immutable positive checked the complete raw type and source
identity snapshots; the single negative case completed all five controlled
shapes. All four parameter cases, Stamp, and owned-copy also passed.

This supplies the actual r5 behavior RED to r6 focused AOT GREEN sequence for
the bounded writer candidate. It does not alter the old r4 fixture failure or
the compiler's raw ID-zero/DYNAMIC contract. Query still completed 71 cases with
one weak behavior failure, and ownership remained 22 PASS / 3 weak FAIL. The r6
state is `CONTROLLED_LOWER_SUPPORT_OBSERVATIONS_NOT_PLATFORM_ACCEPTED`, not a
platform PASS. At that dated r6 boundary, the weak failure-only range observation
was being isolated in a separate parse-only regression; no parser range repair
credit comes from r6. Its failed companion results remain unchanged.

## Dated R7/R8 Focused Evidence and Full R11

The 2026-10-17 UTC r7 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r7-receipt.json`,
SHA256 `4EBF6E3F8CD04D58B80894E24C025206AD7786DB38EB27D2C0F299D7B4B5E7C3`.
AOT17 passed, but query72 had the main weak and new PRIMARY behavior failures,
and ownership25 retained three weak failures. R8 is recorded by
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r8-receipt.json`,
SHA256 `FFC86B539C59CB9FD4FE1CFA9F3E19741793DA1AEC8BEE319B17A2E99E9E907D`.
Only the two PRIMARY successful-adoption parent-range assignments changed between
those source pins; the scalar writer did not change. Default query72 / ownership25 /
Windows AOT17 all passed, without fixture, IGNORE, count, or order drift.
Each run independently held its 4801 source inputs, HEAD, support6, and binary3
stable and all eight children terminated naturally. R8's state remains
`FOCUSED_DEFAULT_SUITES_ALL_PASS_NOT_PLATFORM_ACCEPTED`. The
[PRIMARY range contract](../parser-and-semantics/primary-expression-source-ranges.md)
is limited to common physical coordinates. Mixed-source projection was undecided
at that dated update; the user-approved provenance/AST-attribution direction is
now established, but its implementation and regressions remain unvalidated.
No broader range or ABI guarantee follows from AOT17.

The actual full Windows r11 inner receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-receipt.json`, SHA256
`7C7D2DDCDCCE2BC12135D53B927DE86D3D5783A9621728E118626C47DB0CFF78`;
the outer receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-environment-launch-receipt.json`,
SHA256 `3AE3BD1E7E4317BD7FEF8D9BB256F3C0ACBD5E9485AEF89D3CBAA98C11D17995`.
Foundation52, lower323, and original127 passed. Full55 had 53 PASS / 2 FAIL in
`language_pipeline` and `language_server`. The pipeline's guardrail run completed
`6 Tests 1 Failures 0 Ignored`; the failed assertion was
`test_aot_c_guardrail_generated_scalar_typed_direct_calls_stay_value_free`,
with the observed bool typed-call value-free failure. The original semantic
analyzer84 also had nested metadata case7 and owner generic member signature
failures. Their causes and future repair outcomes are not established here.

All 4801 source inputs were unchanged, but HEAD changed from
`2e2c95d004317014334c9a09de599734d8da9746` to
`d7d4c8f28a4dd78920ce5ab1bb9e882f68e51dc0`. Both r11 receipts remain FAIL;
the later CLI gate, outer auxiliary native3+4, and source-final were not reached.
The r5-to-r6 scalar result still covers only a copied C-signature representation,
not a mutation of raw/compiler metadata or a complete incoming ABI. It neither
waives the mandatory guardrail failure nor authorizes changing its expectation.
Linux remains unpassed; there is no M1 acceptance, commit, or M2 advancement.

## R11-Support R3/R4 Call-Result History

The later R3 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lower-support-red-r3-receipt.json`,
SHA256 `8D63E96790549D636DAC7B901EFF6ABEDB8E9C8AAB5BC6690F98B0BD443F06B5`.
It remains FAIL: query75's original two positive cases reached behavior RED,
whereas its new negative failed an incorrect `count==1` prerequisite. The real
compiler inferred one return AST twice and published two READs. Revising the
control to inspect actual identity/state/range evidence does not rewrite R3.

The R4 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-declaration-identity-r4-receipt.json`,
SHA256 `8BA8F1F51B8DFC3ED5712305476948EF0FBDAE2AE578929852C5D39B99D0ADE2`.
Its state is `DECLARATION_IDENTITY_VERIFIED_OTHER_SUPPORT_RED_NO_PLATFORM_CREDIT`.
Normal build exited 0; AOT completed `18 Tests 1 Failures 0 Ignored`.
`test_aot_c_plain_bool_call_result_preserves_scalar_proof` was the sole failure;
the existing 17-case prefix passed. The real source compiled and production
module/function-table prerequisites completed before the BOOL result proof failed.
This is a call-result support behavior RED, not another raw-return fixture failure.
The guardrail's six cases retained the single mandatory BOOL value-free failure.

The same R4 had query75 = 74 PASS / 1 lambda FAIL, ownership25 and facts19 all
PASS, and original LSP84 = 83 PASS / 1 nested-lambda metadata FAIL. The owner-generic
signature case recovered after the separate declaration-identity fix. Its
negative control now requires actual AST/resolved-symbol identity, all matching
READs UNINIT, and exact descriptor 3002 range rather than a fixed duplicate count.
All 4805 source inputs, HEAD `676c3c88bc39d155c16cae873ee565207b3c166b`,
support6, and binary6 were stable; all eleven children terminated naturally.

At that R4 boundary, work on unknown-return and explicit-OBJECT call-result
regressions and the private lambda module had no new native result. The scalar
signature repair remains a local copied representation; it does not authorize
inventing a raw type, relaxing result ownership, or accepting unproven call writes.
Full Windows/Linux/M1 remain unaccepted, no commit was made, and released space
remains 0 B.

## R11-Support R6/R7 Actual Boundary

R6 receipt:
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lambda-and-origin-red-r6-receipt.json`,
SHA256 `5C66CA772D337D99291669926CBADE3ACC072E94E61C2ADD1869EF04E1496637`.
Unified `24087` naturally exited 0 and was drained; normal build exited 0 and
all thirteen children terminated naturally. All 4810 source inputs, HEAD
`83e88fad7c70876c63b1fca86457259ba300d647`, support6 and binary8 were stable.
Query77, original LSP84 and ownership25 passed. Facts20 retained nineteen passes
and a new standalone-BLOCK missing-state failure. SemIR4 retained two original
passes and two new UNKNOWN/OBJECT behavior failures. Token7 retained six passes
and a mixed-postfix behavior failure despite intact original-Span prerequisites;
all five AST anchors were false and the parent range was `0..70`.
AOT18 had 17 PASS / 1 BOOL FAIL; guardrail6 had 5 PASS / 1 BOOL FAIL.
Its state was
`LAMBDA_IDENTITY_VERIFIED_AOT_AND_PROVENANCE_RED_BLOCK_REGRESSION_NO_PLATFORM_CREDIT`.
The zero observer exit did not make those native failures pass.

After the narrow BLOCK and default-UNKNOWN repairs, R7 receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-da-aot-repair-r7-receipt.json`, SHA256
`985AF370C14738D6AD637272505A4DC3348F8A7E9A1898E1063C3B1CE9229B80`,
naturally exited 1 and was drained with state FAIL. Build exited 0;
query77, ownership25, AOT18, original LSP84, guardrail6 and facts20 all passed.
This includes the mandatory BOOL proof/value-free assertions, not a waiver or
case reduction. SemIR completed 4 tests with 1 failure: the original two and
UNKNOWN case passed, while the explicit-OBJECT case stopped at the incorrect
DYNAMIC expectation described above. Token7 retained its six passes and the same
mixed-postfix RED. The same 4810 source inputs and HEAD, support6 and binary8
were stable; all thirteen children terminated naturally. R7 remains FAIL;
the new OBJECT fixture correction has no R8 credit. Neither run establishes
native compilation of emitted C, complete ABI/ownership support, provenance
repair, full Windows/Linux/M1 acceptance or commit credit.

Full Windows and Linux validation, the complete M1-M6 scope, and M1 acceptance
remain governed by the
[design plan](../plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md)
and [acceptance document](../../tests/acceptance/2026-10-16-macro-m1-token-foundation.md).
No focused receipt in this document substitutes for those gates.
