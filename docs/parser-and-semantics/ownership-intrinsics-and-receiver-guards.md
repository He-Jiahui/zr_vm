---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_completion_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_signature_semantic_facts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_ownership.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_ownership.c
plan_sources:
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
  - docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
tests:
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/parser/test_ownership_optional_callable_cases.h
  - tests/parser/test_ownership_receiver_guard_contract_cases.h
  - tests/parser/test_ownership_receiver_guard_performance.c
  - tests/parser/test_legacy_migration.c
  - tests/parser/test_cfg_throw_effects.c
  - tests/parser/test_resource_unique_drop.c
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_resource_owner_borrow_receiver.c
  - tests/parser/test_semir_pipeline.c
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - tests/language_server/test_lsp_expression_fact_hover.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/acceptance/2026-08-10-ownership-object-member-separation.md
doc_type: module-detail
---

# Ownership Intrinsics And Receiver Guards

## Source contract

Ownership control is a closed set of reserved intrinsic calls:

```zr
var owner: Unique<Resource> = own Resource();
var shared: Shared<Resource> = share(owner);
var weak: Weak<Resource> = degrade(shared);
var live: Shared<Resource>? = wake(weak);
drop(shared);

var gc: GcBox<Resource> = intoGc(own Resource());
```

The five names are reserved in bare call position. They are not lexical
functions, overload candidates, or first-class values. Their contracts are:

| Intrinsic | Input | Result | Effect |
| --- | --- | --- | --- |
| `share(owner)` | `Unique<T>` | `Shared<T>` | consumes the unique place |
| `degrade(shared)` | `Shared<T>` | `Weak<T>` | preserves the shared owner |
| `wake(weak)` | `Weak<T>` | `Shared<T>?` | preserves the weak handle; expiry returns `null` |
| `intoGc(owner)` | `Unique<resource T>` | `GcBox<T>` | consumes the unique place and crosses into the GC world |
| `drop(owner)` | `Unique<T>`, `Shared<T>`, or `Weak<T>` | `void` | consumes and releases the supplied handle |

The current consuming-lowering boundary is narrower than the abstract PlaceId
model: `share`, `intoGc`, and `drop` accept a local owner binding only. A field
or index projection such as `share(holder.owner)` is rejected until the
compiler has a canonical load-and-clear/writeback operation for projected
places. Move the projected owner into a local binding first. Non-consuming
`degrade` and `wake` may read a field or index projection because they preserve
the source handle.

The same spellings remain legal after `.` or `?.` as ordinary object member
names. `service.wake()` dispatches a method named `wake`; only `wake(weak)` is an
ownership transition. No `GET_MEMBER`, property, or call path classifies member
text as an ownership operation.

Removed member forms such as `owner.share()`, `shared.weak()`,
`weak.upgrade()`, and `owner.intoGc()` are hard errors when the receiver's
canonical ownership type proves the old operation. The structured diagnostic
publishes a replacement edit such as `share(owner)`; the compiler never lowers
the removed form. Ordinary objects may still declare and call members with the
same names, so migration is type/fact driven rather than a textual rewrite.

## Target access

`.` and `?.` exclusively access or call the receiver target:

```zr
receiver.member
receiver.method(args)
receiver?.member
receiver?.method(args)
callable(args)
callable?.(args)
```

There is no direct `.(args)` form. Direct calls use `(args)`; `?.(args)` is the
optional-call segment.

A resource class can make its target callable through `@call`. Calling through
`Weak<T>` requires that meta function to publish a readonly receiver effect, so
the declaration uses the same `const` receiver contract as an ordinary method:

```zr
resource class Service {
    pub const @call(value: int): int { return value + 10; }
}

var result = weak?.(makeValue());
```

`const @call` may be combined with declaration modifiers in the normal order,
for example `pub virtual const @call(...)`. `static const @call` is invalid
because a static meta function has no receiver. A const meta body cannot mutate
`this`, and a live Weak call uses that readonly receiver effect after its single
wake.

For nullable and weak receivers:

- direct `.` or direct call requires a live target and raises the catchable
  `NullReferenceError` when no target exists;
- optional `?.` produces `null`, or a `void` no-op, and skips the complete
  guarded suffix including member keys and call arguments;
- a live target with a missing member retains the ordinary missing-member error;
- a weak receiver is woken once into a hidden `Shared<T>` for the full postfix
  chain, then released on normal and exceptional exits.

`weak?.a.b` guards only the weak target. If `a` is nullable and absent, direct
`.b` throws. Use `weak?.a?.b` to guard both boundaries. Optional access on a
known non-null target and optional access on an unknown/dynamic receiver are
rejected rather than silently adding runtime guessing.

That rule also applies to the first-call fast path. If a bare function name is
resolved in either the runtime or compile-time type environment, `name?.(args)`
is rejected with `redundant_optional_access` before overload selection. Normal
`name(args)` resolution is unchanged. A visible variable binding keeps lexical
precedence over same-name runtime and compile-time functions and follows the
ordinary callable receiver path; unresolved/dynamic callables do not gain a
name-based compatibility path.

## Parser and facts

The lexer emits one `ZR_TK_QUESTION_DOT` token. The parser records `DIRECT` or
`OPTIONAL` on each member and call segment and creates
`ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION` for the five reserved calls. The
intrinsic node stores the operation and its argument; it does not contain a
callee identifier or member-expression surrogate.

Construction ASTs carry an `ownershipQualifier` field for their declared result
shape, but that field is not an ownership-operation selector. Type inference,
expression-fact publication, wrapper selection, and compiler lowering use only
the explicit `builtinKind` produced by current syntax such as `own` and `ref`.
A construct with `builtinKind == NONE` remains an ordinary call even if stale or
externally synthesized AST metadata contains a non-`NONE` qualifier.

Type inference publishes two canonical fact families:

- `SZrOwnershipIntrinsicFact` carries the operation, input/result canonical
  types, input PlaceId, active LoanId, consuming bit, effects, and source range;
- `SZrReceiverGuardFact` carries null-versus-weak guard kind, direct-versus-
  optional mode, target type, guarded segment bounds, result lift, and range.
  Optional value-producing chains use `NULLABLE`; optional chains whose final
  call returns `void` use `VOID_NOOP`, so downstream consumers never infer a
  nullable value for a statement-only no-op.

Publishing a receiver guard also publishes the exact expression fact for its
receiver node. This gives lowering an independent canonical receiver type; the
guard fact cannot validate itself by presenting a mutually consistent but stale
`receiverType`, guard kind, and guarded type.

`chainSegmentStart` identifies the guarded postfix segment and
`chainSegmentEnd` is the exclusive end of the complete dominated suffix. The
current source contract makes that end equal to the postfix-chain segment
count. Lowering validates the fact's node and receiver identity, start and end,
AST access mode, canonical receiver type, receiver-derived guard kind and
guarded type, and chain-result lift before emitting instructions. A stale fact
with a shortened suffix, direct/optional mismatch, receiver-type or
nullable/Weak kind mismatch, guarded-type drift, or value/void lift mismatch is
a compile error. Optional member segments always require a guard fact. Member
and function-call segments also require one when the current or canonical
receiver is nullable/Weak. A known non-null optional callable must be rejected
as redundant optional access during inference; lowering never invents a guard
fact or a compatibility branch for it.

Move checking, loan conflicts, throw profiling, compiler lowering, LSP hover,
signature help, completion, diagnostics, and migration edits consume these
facts. Missing or inconsistent facts fail closed; consumers do not recover a
contract from source text or display type names.

## Lowering and runtime

Intrinsic lowering emits the canonical operations `OWN_SHARE`,
`OWN_DEGRADE`, `OWN_WAKE`, `OWN_INTO_GC_BOX`, and `OWN_DROP`. Numeric artifact
IDs remain stable where required, but old semantic names and source aliases are
not accepted.

Historical construct builtin id `8` is not a source compatibility route. The
compiler opcode mapper, pre-execution Semantic IR builder, type inference, and
semantic-fact classifier all reject that id instead of translating it to
`OWN_RETURN_TO_GC`. The `OWN_RETURN_TO_GC` instruction and its SemIR/AOT/runtime
readers remain available only for loading older artifacts, as required by the
Syntax 04 M7 artifact contract; no current source AST can emit it. The public
enum/type-helper declaration is removed together with the coordinated type
system update, while the compiler behavior already fails closed.

Ordinary construction cannot enter this lowering from a qualifier fallback.
Only an explicit construct `builtinKind` may select the older `own`/`ref`
construction lowering, while the five ownership-control calls lower from their
dedicated intrinsic AST and semantic facts.

A receiver guard evaluates the base once. Optional guards branch to one merge
slot; direct guards emit `REQUIRE_NON_NULL`. Weak guards emit exactly one
`OWN_WAKE`, retain its Shared result across the complete guarded suffix, and use
ordinary member/property/call instructions for the live path. The guard merge
does not repeat lookup, argument evaluation, or wake operations. When the live
path copies its result into the merge slot, it also resets any distinct guarded
slot before leaving the frame. The null path and exceptional cleanup retain the
same release requirement. Therefore a successful chain such as
`weak?.child.value` cannot keep the hidden Shared owner alive after the chain.
The lowering frame stores the fact-owned exclusive chain end and result-lift
mode. Finalization requires the compiler's reached chain end to match the fact,
and the absent block explicitly selects nullable versus void-no-op merge
behavior from that stored mode.

Every guard-owned `OWN_WAKE` is immediately followed by
`MARK_TO_BE_CLOSED` for the same destination slot. Normal completion closes
those registrations in reverse guard order after releasing the hidden Shared
values on both live and absent branches. If a direct inner guard throws before
normal finalization, the VM exception handler closes registrations above its
saved scope boundary. This prevents an outer live Weak guard from retaining its
target after a caught inner `NullReferenceError`.

`degrade(shared)` and `wake(weak)` read an identifier from its original local
slot instead of first value-copying the ownership wrapper into a compiler
temporary. Compound and projected operands may still require a temporary, but
that slot is reset immediately after the non-consuming ownership operation.
This prevents compiler-generated Shared copies from extending a weak target's
lifetime; reset instructions are optimizer read/write operations because the
clear releases the wrapper previously stored in the slot.

The runtime registers `NullReferenceError` as a subtype of `RuntimeError`.
Interpreter, AOT C, and LLVM route direct absent-receiver failures through the
same named exception identity. The semantic CFG therefore treats every direct
nullable or weak receiver guard as an unknown throw source and connects it to
enclosing catch/finally control flow. Optional guards are not throw sources for
an absent receiver. `wake(weak)` itself never throws for expiry.

Runtime tests that catch `NullReferenceError` must materialize the standard
exception hierarchy with `ZrParser_ToGlobalState_Register` and
`ZrVmLibSystem_Register` before execution. Without that registration, a test is
exercising the generic status-normalization fallback rather than the named
receiver-guard exception contract.

## Failure modes

- Wrong intrinsic arity or owner kind is a compile error.
- `share`, `intoGc`, and `drop` reject unavailable/moved places and incompatible
  active loans.
- Consuming intrinsics reject field and index projections rather than lowering
  a copied temporary while leaving the original owner in place.
- `intoGc` rejects Shared owners and non-resource targets.
- Optional access rejects unknown/dynamic and statically non-null receivers.
- References tied to the hidden wake owner cannot escape the guarded chain.
- Old ownership-member syntax has no lowering fallback.
- Historical construct builtin id `8` has no compiler, inference, or fact
  fallback even though old artifacts may still contain `OWN_RETURN_TO_GC`.
- Live missing-member, wrong-runtime-kind, and user getter/method exceptions are
  not relabeled as `NullReferenceError`.

## Performance boundary

The live guard path performs no source-name comparison and no runtime guard-kind
classification. One guard test or atomic wake covers one postfix chain. Hidden
Shared storage reuses the existing ownership control and cleanup machinery; it
does not allocate an object wrapper for chaining.

`zr_vm_ownership_receiver_guard_performance_test` compiles and repeatedly runs
equal-checksum workloads for direct non-null access, weak direct access,
optional weak success, optional weak expiry, and a deep guarded field chain. It
prints iterations, samples, nanoseconds per operation, ratio to direct, and
checksum. Ratios are evidence for review, not pass/fail thresholds; semantic
checksums are mandatory.

## Intentional boundaries

- Optional computed syntax `?.[index]` is not part of this surface; a guarded
  receiver may still access an index later in the same suffix.
- Ownership intrinsics are not first-class callables or overloadable functions.
- The feature does not add cycle collection for Shared ownership.
- `wake(weak)` is the explicit way to retain a Shared owner beyond one guarded
  postfix chain.
- Historical plan and acceptance documents may quote removed syntax as migration
  evidence, but current examples and executable fixtures use only this contract.

Repository-local QuickJS evidence implements optional calls by testing the
receiver before call evaluation and branching over the guarded suffix. Mono's
conditional-access lowering likewise separates the guarded path from direct
null-reference behavior, while Rust's `Weak::upgrade` returns `Option` when a
target cannot be retained. ZR adopts those control/lifetime principles but
deliberately rejects optional access when its canonical type already proves the
target non-null.

## Test coverage

`test_ownership_intrinsic_member_separation.c` owns syntax, semantic facts,
collision names, direct/optional behavior, and backend-facing integration.
`test_resource_shared_weak.c` includes the modular receiver-guard contract cases:
fact drift must fail closed, canonical receiver drift and guarded-type drift are
independent failures, nullable/Weak callable segments cannot omit their guard
facts, every emitted guard wake must have an adjacent scope-close marker, mixed
`weak?.a.b` and `weak?.a?.b` chains must release in normal and caught-exception
paths, direct and nested mixed chains must preserve ownership-valued `Shared<T>`
results, and 32 live/expire/wake iterations must remain stable.

On the isolated 2026-08-26 review snapshots representing main `075d68c` plus
the exact ownership overlay, GCC 11.4, Clang 14, and MSVC 19.44 each passed
Shared/Weak 19/19, ownership separation 37/37, type inference 123/123,
expression facts 28/28, and compiler integration 127/127. GCC and Clang passed
SemIR 13/13; MSVC passed its registered SemIR set 12/12. The detailed commands,
baseline distinction, and remaining full-graph gate are recorded in
`tests/acceptance/2026-08-10-ownership-object-member-separation.md`.

A later exact review snapshot based on main `b1f6884` adds the readonly class
meta-call and deep Weak-result boundary cases. GCC 11.4, Clang 14, and MSVC
19.44 each directly pass ownership separation 39/39, owner/borrow receivers
8/8, semantic query relations 19/19, and compiler integration 127/127. The
39-case runner executes both live and expired `Weak<Service>?.(args)`, proves
argument skipping and catchable direct expiry, and verifies the canonical
readonly receiver effect of `virtual const @call`.

The 2026-08-27 named-callable follow-up closes the remaining statically non-null
call gate. A TDD regression registers the same `callback(int): int` signature
independently in the runtime and compile-time type environments and requires
`callback?.(1)` to report `redundant_optional_access`. A second regression
registers a nullable function-typed variable with that name and proves it
shadows either function environment. GCC 11.4, Clang 14, and MSVC 19.44 each
pass the resulting ownership runner 41/41. On the same exact source overlay,
type inference passes 123/123, expression facts 28/28, and compiler integration
127/127 on all three toolchains.
