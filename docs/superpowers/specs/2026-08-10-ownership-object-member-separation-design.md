# Ownership Intrinsics And Object Member Separation Design

**Status:** Implementation complete; final integrated acceptance is pending the
stable three-toolchain replay, generated-artifact verification, and cleanup.

## Scope

This design removes the semantic collision between ownership control operations
and ordinary object member access. Ownership control becomes a closed set of
reserved intrinsic calls:

```zr
share(owner)
degrade(shared)
wake(weak)
intoGc(owner)
drop(owner)
```

The `.` and `?.` operators are exclusively target-access operators. They never
select an ownership operation by member spelling. Weak and nullable receivers
use explicit receiver guards, and direct access to an absent receiver raises the
named `NullReferenceError` exception.

This is an intentional one-time source break. The implementation must remove the
old member-name classifier instead of keeping two source languages alive.

## Current Problem

The production parser and compiler currently recognize ownership behavior from
ordinary member expressions whose property text is `share`, `weak`, `upgrade`,
or `intoGc`. `ZrParser_OwnershipMemberNameToBuiltinKind` is consulted by type
inference, compiler lowering, dataflow, and throw profiling. As a result:

- `value.wake()`, `value.degrade()`, and similar expressions cannot have a
  stable meaning as ordinary object methods.
- `GET_MEMBER` and ownership lowering compete for the same AST shape.
- correctness depends on source text rather than canonical TypeRef, PlaceId,
  LoanId, and operation facts.
- adding optional weak-target access would create more string-based special
  cases in member and call lowering.

The redesign makes ownership intent explicit at parse time and makes receiver
absence explicit in semantic facts. Ordinary member dispatch no longer knows
any ownership operation names.

## Goals

- Give ownership transitions a syntax and AST that cannot collide with object
  members.
- Support direct and optional member/method/call access on nullable and weak
  receivers with one evaluation and a precisely bounded guard lifetime.
- Make `NullReferenceError` a catchable, named runtime exception shared by the
  interpreter and AOT backends.
- Preserve canonical type, place, loan, effect, and exception facts through
  SemIR, ExecBC, artifacts, LSP, and diagnostics.
- Delete old source compatibility branches after structured migration fixes are
  available.
- Keep successful object access on the existing member/property/call paths.

## Non-Goals

- This design does not add `.(args)` syntax.
- It does not add optional computed access such as `?.[index]` in the first
  implementation. A guarded `receiver?.items[index]` remains expressible.
- It does not make unknown or dynamic receivers choose a guard at runtime.
- It does not change missing-member behavior on a live object into
  `NullReferenceError`.
- It does not make ownership intrinsics first-class callable values or overload
  candidates.
- It does not preserve the old member-call ownership syntax as a compatibility
  mode.

## Source Language

### Reserved ownership intrinsics

The five intrinsic calls have these contracts:

```text
share(owner: Unique<T>) -> Shared<T>
degrade(shared: Shared<T>) -> Weak<T>
wake(weak: Weak<T>) -> Shared<T>?
intoGc(owner: Unique<resource T>) -> GcBox<T>
drop(owner: Unique<T> | Shared<T> | Weak<T>) -> void
```

Their ownership effects are:

| Intrinsic | Input effect | Failure/result contract |
| --- | --- | --- |
| `share` | consumes the `Unique<T>` place | returns the initial `Shared<T>` owner |
| `degrade` | borrows/preserves `Shared<T>` | returns a `Weak<T>` observer |
| `wake` | borrows/preserves `Weak<T>` | returns `null` when the target is expired; never throws for expiry |
| `intoGc` | consumes `Unique<resource T>` | returns the explicit GC bridge handle |
| `drop` | consumes the supplied owner handle | releases that handle deterministically and returns `void` |

Each intrinsic accepts exactly one positional argument. It is parsed into a
dedicated ownership-intrinsic expression before lexical identifier lookup.
Intrinsic names cannot be declared, imported, captured, shadowed, referenced as
values, or overloaded in a lexical namespace.

The live-handle transitions `share`, `degrade`, `wake`, and `intoGc` require a
definitely-live, non-null operand. The nullable result of `wake` must therefore
be explicitly handled or unwrapped before it participates in another live
transition. `drop` is the cleanup exception: it may consume a nullable owner
result and is a defined no-op when that result is null.

Member names use a separate namespace. A type may declare members named
`share`, `degrade`, `wake`, `intoGc`, or `drop`, and those members remain legal
after `.` or `?.`. Therefore `object.wake()` is always an ordinary target method
call, while `wake(weak)` is always the ownership intrinsic.

### Direct and optional target access

The supported postfix forms are:

```zr
receiver.member
receiver.method(args)
receiver?.member
receiver?.method(args)

callable(args)
callable?.(args)
```

There is no `callable.(args)` form. A direct call retains ordinary `(args)`
syntax. `?.(` is the optional-call segment.

The static receiver domain is:

| Receiver | Direct access/call | Optional access/call |
| --- | --- | --- |
| non-null object/callable `T` | ordinary access | compile diagnostic: redundant optional access |
| non-null target owner `Unique<T>` / `Shared<T>` / `GcBox<T>` | ordinary target access through the owner | compile diagnostic: redundant optional access |
| nullable object/callable `T?` | null throws `NullReferenceError` | null returns `null` and skips the guarded path |
| `Weak<T>` | expired target throws `NullReferenceError` | expired target returns `null` and skips the guarded path |
| unknown/dynamic receiver | existing direct rules | rejected; no runtime guard-kind guessing |

A Weak receiver is never passed to normal member dispatch. The guard first
produces a hidden live `Shared<T>` value, and member/property/call resolution
then operates on `T` through that owner.

### Chain semantics

A postfix chain is the contiguous sequence rooted at one primary expression.
Parentheses, an infix operator, a comma, or statement termination ends the
chain. For example:

```zr
weak?.service.client.send(makePayload())
```

has the following semantics:

1. evaluate `weak` exactly once;
2. atomically attempt one wake;
3. if the target is expired, return `null` without evaluating `makePayload()`;
4. if live, retain the hidden `Shared<T>` through all member and call segments;
5. release the hidden owner on normal completion and every exceptional exit.

The guard is not re-run for every segment. It dominates the guarded suffix and
merges once at the chain result. This avoids a time-of-check/time-of-use gap and
keeps the target alive while a property getter, method body, computed index, or
call argument is evaluated.

An optional segment guards the remaining suffix in the same postfix chain.
Thus `receiver?.method(args)` skips both lookup and argument evaluation when the
receiver is absent. `receiver?.method?.(args)` first guards the receiver and then
guards a nullable callable returned by the member.

`weak?.a.b` only guards absence of the weak target. If the target is live but
`a` evaluates to null, the ordinary direct `.b` segment throws
`NullReferenceError`. A second optional segment, `weak?.a?.b`, is required to
guard both boundaries.

For a successful optional access whose normal result is `R`, the result type is
canonical `R?`. If `R` is already nullable, canonicalization does not create
nested nullable wrappers. A `void` call remains `void`; an absent receiver is a
no-op. Direct guarded access preserves `R` because absence exits by exception.

References and ref-like values derived from the temporarily woken target cannot
escape the guard lifetime. The borrow/escape checker must either prove their
use ends inside the chain or reject the expression. Code that needs a longer
lifetime must call `wake(weak)`, bind the resulting `Shared<T>?`, and explicitly
retain the unwrapped owner.

### Evaluation order

- The base receiver is evaluated once before any postfix segment.
- A direct nullable/weak guard throws before evaluating a guarded member key or
  call argument.
- An optional failed guard evaluates neither the remaining member/index
  expressions nor call arguments.
- On success, existing left-to-right member and argument evaluation order is
  preserved.
- Hidden owner cleanup participates in the normal cleanup stack, including
  return, throw, and nested-call unwinding.

## Syntax And AST Model

### Lexer and parser

The lexer adds `ZR_TK_QUESTION_DOT` as one token. Treating `?` and `.` as two
independent tokens would make whitespace and recovery behavior ambiguous and
would prevent the parser from assigning one source range to the guard segment.

The primary-expression parser recognizes a bare reserved intrinsic name only
in intrinsic-call form and builds `ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION`.
Postfix parsing builds one chain from a base plus ordered segments:

```text
PostfixChainSyntax
  base: Expression
  segments: PostfixSegment[]

MemberSegment
  accessMode: DIRECT | OPTIONAL
  property
  computed
  sourceRange

CallSegment
  accessMode: DIRECT | OPTIONAL
  arguments
  sourceRange
```

`receiver?.method(args)` is an OPTIONAL member segment followed by a DIRECT call
segment within the same guarded suffix. `receiver?.(args)` is an OPTIONAL call
segment. Recovery diagnostics must distinguish a missing member name from a
missing optional-call parenthesis.

The existing `SZrMemberExpression.computed` distinction can remain inside the
member segment, but access mode cannot be inferred later from source text.

### Ownership intrinsic AST

The intrinsic node contains:

```text
OwnershipIntrinsicExpression
  operation: SHARE | DEGRADE | WAKE | INTO_GC | DROP
  argument
  nameRange
  callRange
```

It does not contain a callee identifier or a member expression. AST writers,
serializers, structural equality, clone/destroy logic, diagnostics, and debug
printers must handle the new node explicitly.

## Canonical Semantic Facts

### OwnershipIntrinsicFact

Binding and type inference produce one fact per valid or recoverable intrinsic:

```text
OwnershipIntrinsicFact
  operation
  inputTypeRef
  resultTypeRef
  inputPlaceId
  activeLoanId
  consuming
  effectSet
  sourceRange
```

The fact, not the spelling, controls move checking, loan conflicts, result type,
throw profiling, lowering, LSP hover, and code actions. `share`, `intoGc`, and
`drop` require an available movable place and reject active incompatible loans.
`degrade` and `wake` preserve their input handles and record the required
readonly access. All invalid cases fail closed; lowering cannot reconstruct a
missing PlaceId or TypeRef from a display string.

### ReceiverGuardFact

Type inference produces a receiver guard when receiver type and access mode
require one:

```text
ReceiverGuardFact
  kind: NULL_GUARD | WEAK_WAKE_GUARD
  mode: DIRECT | OPTIONAL
  receiverTypeRef
  targetTypeRef
  chainStartSegment
  chainEndSegment
  resultTypeRef
  resultLift: NONE | NULLABLE | VOID_NOOP
  sourceRange
```

The fact identifies the full dominated suffix so all consumers agree on skipped
side effects and hidden-owner lifetime. Unknown/dynamic types do not receive a
fact; they receive a compile diagnostic. A non-nullable receiver with OPTIONAL
mode receives the redundant-optional-access diagnostic and is not silently
lowered to a branch.

Member symbols, property accessors, callable signatures, and effects continue to
come from the existing canonical lookup facts after the guard exposes the target
type.

## SemIR, ExecBC, And Artifact Contract

Ownership lowering consumes only `OwnershipIntrinsicFact`. The current
`ZrParser_OwnershipMemberNameToBuiltinKind` route and all equivalent string
comparisons are deleted from type inference, dataflow, compiler lowering, and
throw profiling.

Semantic operation names converge on source terminology:

```text
OWN_SHARE
OWN_DEGRADE
OWN_WAKE
OWN_INTO_GC_BOX
OWN_DROP
```

Existing internal `OWN_WEAK` and `OWN_UPGRADE` names become `OWN_DEGRADE` and
`OWN_WAKE`. Serialized numeric discriminants may remain stable when the artifact
format requires compatibility, but no old source spelling, member classifier,
or semantic alias remains reachable. If serialized textual enum names exist,
the artifact format version must be bumped rather than accepting both names.

Receiver guards lower as structured control flow:

```text
evaluate receiver
guard receiver
  absent + DIRECT   -> throw named NullReferenceError
  absent + OPTIONAL -> branch to null/void merge
  present           -> evaluate guarded suffix
cleanup hidden Shared owner, when present
merge result
```

A `WEAK_WAKE_GUARD` emits exactly one atomic wake and stores its success value in
a hidden Shared slot registered with cleanup metadata. Both interpreter ExecBC
and AOT lowering consume the same guard and cleanup facts. The success block
then uses normal member/property/call operations; `GET_MEMBER`, `META_GET`, and
`FUNCTION_CALL` never inspect ownership operation names.

Artifact round trips preserve the canonical executable projection, not the
compiler's AST-backed fact objects. The projection contains intrinsic opcodes,
Weak wake versus nullable copy, direct `REQUIRE_NON_NULL` versus optional
`JUMP_IF_NULL`, patched chain bounds, merge-slot result lifting, serialized
TypeRefs and typed-binding TypeId/PlaceId identities, exception tables, and
cleanup/reset instructions. The five intrinsic facts currently carry no live
loan (`loanId == 0`); a compiler-local LoanId must not be copied into an
artifact without a runtime consumer and a stable artifact identity model.
Readers and backends consume only this structured projection and never
reconstruct ownership behavior from syntax text.

## Runtime Exception Model

The system exception registry adds:

```text
NullReferenceError extends RuntimeError
```

Interpreter, AOT C, and LLVM use the same named-runtime-error helper and the
same prototype identity. The exception is catchable by exact type and through
`RuntimeError`.

`NullReferenceError` is raised only when a direct target access or call cannot
produce a receiver because a nullable value is null or a Weak target is expired.
It is not raised when:

- an optional guard fails;
- `wake(weak)` returns null;
- a live object lacks the requested member;
- a member exists but its getter or method throws another exception.

A live receiver with a missing member retains the existing missing-member error
category. A receiver of the wrong non-null runtime kind retains the existing
type/dispatch error. Weak handles are resolved before `GET_MEMBER`, so member
dispatch does not need a Weak-specific case.

## One-Time Migration

The source migration map is:

| Removed ownership spelling | Replacement |
| --- | --- |
| `owner.share()` | `share(owner)` |
| `shared.weak()` or `shared.degrade()` | `degrade(shared)` |
| `weak.upgrade()` or `weak.wake()` | `wake(weak)` |
| `owner.intoGc()` | `intoGc(owner)` |
| `owner.drop()` if accepted by old paths | `drop(owner)` |
| existing `drop(owner)` | same text, new intrinsic AST and facts |

After the switch, every removed member spelling is parsed as an ordinary target
member call. If the canonical target type actually declares that member, the
call is valid and must not be rewritten. If member lookup fails on a canonical
Unique/Shared/Weak receiver and the expression matches a removed ownership form,
the compiler may emit a dedicated migration diagnostic with a structured safe
fix.

The migration diagnostic is fact-driven: it requires the canonical receiver
type, resolved call shape, and failed target-member lookup. It cannot trigger
from name or message text alone. LSP exposes only the structured edit stored on
the diagnostic. It must not add its own AST, spelling, regex, or display-type
fallback.

The repository migration is atomic at the source-language boundary:

1. add intrinsic AST/facts and structured migration diagnostics;
2. migrate production sources, fixtures, tests, examples, and documentation;
3. remove the member-name ownership classifier and old expectations in the same
   integration sequence;
4. reject remaining old ownership use unless it resolves as a real target
   member;
5. do not add a feature flag or dual parser/lowering mode.

## Diagnostics

Diagnostics must be stable semantic categories with precise primary ranges and,
where safe, structured fixes:

- reserved intrinsic declared or shadowed in a lexical namespace;
- intrinsic referenced without its required call;
- wrong intrinsic arity;
- wrong owner kind or resource-class requirement;
- nullable operand supplied to a live-handle ownership transition;
- use after move, non-place consuming argument, or incompatible active loan;
- optional access on a statically non-null receiver;
- optional access on unknown/dynamic or otherwise unsupported receiver;
- ref/ref-like result escaping a temporary Weak wake guard;
- removed ownership member syntax with a fact-proven replacement;
- direct absent target, represented at runtime by `NullReferenceError`.

Member declarations using the five spellings do not receive reserved-name
diagnostics because member and lexical namespaces are intentionally distinct.

## Reference-Language Evidence

The design uses repository-local reference trees as evidence, not as syntax to
copy verbatim:

| Reference | Repository evidence | Adopted principle |
| --- | --- | --- |
| Rust | `lua/rust/library/alloc/src/sync.rs` and `lua/rust/library/alloctests/tests/sync.rs` | downgrade and wake/upgrade are ownership-handle operations; a failed Weak wake is an optional result |
| QuickJS | `lua/QuickJS-master/quickjs.c` and `lua/QuickJS-master/tests/test_language.js` | optional-chain guarding precedes member/call evaluation and skips the guarded suffix |
| Mono C# | `lua/mono/mcs/mcs/codegen.cs`, `lua/mono/mcs/mcs/ecore.cs`, and `lua/mono/mcs/tests/test-null-operator-*.cs` | conditional access has explicit lowering; direct null access raises a named null-reference exception |
| CPython | `lua/cpython/Objects/weakrefobject.c` and `lua/cpython/Lib/test/test_weakref.py` | observing an expired weak target produces an absent value rather than reviving or throwing |

ZR deliberately combines these principles with its existing canonical
TypeRef/Place/Loan model and explicit VM/AOT fact pipeline.

## Validation Matrix

### Lexer, parser, and AST

- Tokenize `?.` as one token, including whitespace and malformed-sequence cases.
- Parse every intrinsic into the dedicated AST and reject value-reference,
  shadowing, arity, and malformed-call cases.
- Parse direct/optional member, method, and callable segments with exact ranges.
- Prove that member declarations and calls named `share`, `degrade`, `wake`,
  `intoGc`, and `drop` remain ordinary members.
- Round-trip, clone, compare, print, and destroy all new AST shapes.

### Types, places, loans, and effects

- Verify every intrinsic input/result contract and consuming bit.
- Cover non-place arguments, moved owners, active shared/mutable loans, wrong
  owner kinds, and non-resource `intoGc` targets.
- Verify canonical nullable flattening and `void` no-op results.
- Reject optional unknown/dynamic receivers and redundant optional access.
- Reject escaping ref/ref-like results tied to hidden wake owners.
- Verify throw profiles: direct guards include `NullReferenceError`; optional
  guards and `wake` do not add it.

### Evaluation and lifetime

- Receiver expressions execute exactly once.
- Failed optional guards skip getters, computed indexes, arguments, and nested
  calls with observable side effects.
- Weak access performs one wake per chain, including deep mixed direct/optional
  chains.
- The hidden Shared owner survives getters, calls, allocation, GC pressure, and
  nested execution, then releases on success and exception.
- Direct weak/nullable member and callable access throws catchable
  `NullReferenceError`.
- A live target missing a member keeps the distinct missing-member error.
- Repeated wake/expire races never expose a reclaimed target.

### Lowering and artifacts

- SemIR and ExecBC snapshots contain intrinsic and guard operations without
  member-name classification.
- Interpreter, AOT C, and LLVM produce the same value, side-effect order,
  cleanup, and exception prototype.
- Artifact serialize/read/execute round trips preserve operation IDs, guarded
  control-flow bounds, result slots, TypeRefs, typed-binding TypeId/PlaceId,
  cleanup instructions, and exception tables. Source-only AST pointers and the
  currently invalid intrinsic LoanId are deliberately not serialized.
- Old source forms have no lowering entry after migration; any preserved numeric
  artifact IDs do not re-enable old syntax.

### Tooling and repository acceptance

- LSP hover/signature data for intrinsics comes from canonical intrinsic facts.
- Completion does not advertise old ownership member operations.
- Rename and symbol lookup do not treat intrinsic names as user symbols.
- Migration actions are published only from structured diagnostic fixes and
  remain correct after document-version rebinding.
- Formatter, syntax highlighter, documentation, examples, CLI fixtures, and
  package tests use the new spellings.
- GCC, Clang, and MSVC run focused parser/compiler/runtime suites and the full
  current acceptance matrix with zero failures.

## Performance Contract

- There is one receiver test or atomic wake per guarded chain, not per segment.
- The live path performs no source-name comparison or runtime guard-kind
  dispatch.
- Optional failure branches should remain cold and skip all suffix work.
- Hidden Shared storage uses existing owner retain/release and cleanup machinery;
  it does not allocate a wrapper object solely for chaining.
- Type inference computes guard kind and chain bounds once and shares the fact
  with all downstream consumers.

Benchmarks must compare direct non-null access, direct Weak access, optional
Weak success/failure, and deep chains before and after the change. A regression
must be explained by required safety work rather than string dispatch or
duplicated wakes.

## Delivery Milestones

### Milestone 1: syntax and canonical ownership facts

- add the `?.` token, postfix access modes, intrinsic AST, and AST infrastructure;
- bind/type the five intrinsic contracts with PlaceId/LoanId checks;
- add fact-driven migration diagnostics and safe edits;
- migrate parser/compiler fixtures without enabling receiver guards yet.

Gate: intrinsic parsing, typing, move/loan diagnostics, AST round trips, and
member-name collision regressions pass on all three toolchains.

### Milestone 2: receiver guards and chain semantics

- produce `ReceiverGuardFact` for nullable and Weak receivers;
- implement result lifting, skipped suffix evaluation, and escape checks;
- lower one hidden Shared lifetime across a Weak chain.

Gate: parser, type, dataflow, evaluation-order, lifetime, and exception-profile
tests pass, including side-effect and GC-pressure cases.

### Milestone 3: runtime and backend parity

- register `NullReferenceError extends RuntimeError`;
- converge interpreter, AOT C, and LLVM guard/error/cleanup lowering;
- update artifact schemas or versions and verify round trips.

Gate: named exception catching, live missing-member distinction, cleanup on all
exits, and backend parity pass on GCC, Clang, and MSVC.

### Milestone 4: destructive source migration

- migrate all repository sources, tests, fixtures, examples, and documentation;
- remove `ZrParser_OwnershipMemberNameToBuiltinKind` and every downstream
  spelling-based branch;
- remove old completion, hover, diagnostic, and lowering expectations.

Gate: repository search finds no old ownership-member use except deliberate
ordinary-member collision tests and migration-diagnostic inputs.

### Milestone 5: tooling and full acceptance

- consume canonical intrinsic/guard facts in LSP and formatting paths;
- validate structured fixes and document-version rebinding;
- update Syntax 04, Syntax 05, central status, detailed records, and acceptance
  evidence only after fresh end-to-end validation.

Gate: focused and full suites pass under GCC, Clang, and MSVC; build products and
logs are removed; exact-path commits contain no unrelated shared-worktree
changes.

## Completion Criteria

The design is implemented only when all of the following are true:

- source ownership control is available only through the five intrinsic calls;
- `.` and `?.` always mean target access and never select ownership by spelling;
- direct nullable/Weak absence throws the named `NullReferenceError`;
- optional nullable/Weak absence returns null or performs a `void` no-op while
  skipping the full guarded suffix;
- a Weak target is woken once and retained through the complete postfix chain;
- intrinsic and guard behavior flows through canonical facts into every backend;
- member names matching intrinsic names remain legal and unambiguous;
- old syntax and string-based compatibility branches are removed;
- artifacts, LSP, documentation, tests, and status records describe one language;
- validation evidence is fresh, reproducible, and clean on all supported
  toolchains.
