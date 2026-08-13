---
related_code:
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_ownership.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_profile.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_ownership.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_profile.c
plan_sources:
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_type_inference.c
  - tests/gc/gc_tests.c
  - tests/core/test_object_call_known_native_fast_path.c
doc_type: module-detail
---

# Resource Shared / Weak Ownership

## Source contract

Syntax 04 M2 extends a direct `Unique<Resource>` owner with process-local shared ownership:

```zr
var unique: Unique<Session> = own Session();
var shared: Shared<Session> = share(unique);
var clone: Shared<Session> = shared;
var observer: Weak<Session> = degrade(shared);
var live = wake(observer);

drop(shared);
drop(clone);
drop(live);
var expired = wake(observer);
```

`Shared<T>` assignment and parameter passing clone the strong handle. `Weak<T>` can only be
created from `Shared<T>`, is independently copyable, and does not keep `T` alive. `wake(weak)`
returns a nullable Shared owner. Direct `weak.member` wakes once for the target chain and throws
`NullReferenceError` on expiry; `weak?.member` returns null and skips the suffix. Both owner types
are non-nullable declarations. Explicit `drop` and lexical cleanup release exactly one handle.
The first implementation is isolation-domain local and uses non-atomic counters; cross-domain
copy or wake is rejected. `AtomicShared<T>` is not part of M2.

`wake(weak)` uses the nullable `Shared<T>` niche in the existing type/value ABI: live controls
produce a Shared handle and dead controls produce null. There is no member-style upgrade surface
or source-only Option wrapper.

## Stable control lifetime

A Shared resource owns one stable `SZrOwnershipControl`:

- `object` is present only while the target is alive;
- `strongRefCount` counts Shared/Unique strong handles;
- `weakRefCount` includes one implicit weak while strong count is non-zero;
- `isolationDomainId` binds the control to its creating state/domain;
- `objectIsAlive` and `dropInProgress` close the upgrade race before Drop;
- `usesAtomicRefCounts` remains false on the M2 path.

Releasing the final strong handle first sets `objectIsAlive=false`, sets `dropInProgress=true`, and
clears `control.object`. Only then does the runtime run resource Drop and reverse field cleanup.
Consequently a Weak wake attempted from the Drop body observes an expired target. After Drop,
the implicit weak is removed; explicit Weak handles retain the control with a null object until the
last Weak release frees it. Weak values therefore remain real handles after target death instead of
being proactively rewritten to null stack slots.

`ownership_shared.c` owns this state machine and count accounting. `ownership.c` keeps public
owner operations and GC/resource orchestration. The legacy linked weak-slot list and
`isDetachedFromGc` inference are not used by the M2 path.

## Cleanup and calls

Compiler scope metadata registers `Unique`, `Shared`, and `Weak` locals and value parameters for
structured cleanup. Normal scope exit, return, break/continue, and exception unwind all consume
the same close-registration chain. Frame-layout physical values may have a dense cleanup mirror;
the VM clears and refreshes that mirror around materialization and ownership transitions so it
cannot keep an extra strong/weak count or release a stale control.

By-value Shared/Weak arguments create a callee copy. Once parameter binding succeeds, both generic
call staging and known-VM frame binding release the caller staging copy. Borrowed aliases and
Unique direct moves do not enter this release path. Ownership builtins are no-throw operations in
CFG throw profiling, while their receiver/argument expressions still contribute their own throw
edges.

## Strong-cycle lint

Shared does not include a cycle collector. The compiler publishes the structured warning
`resource_shared_strong_cycle` for a process-local resource `Shared<Self>` field or a reciprocal
two-resource Shared field edge. A Weak reverse edge does not warn. The producer consumes resource
modifier, field ownership qualifier, and canonical inner type identity; it does not inspect field
names or source text. The warning is stored as a persistent semantic diagnostic fact for normal
query consumers.

## M2-M4 boundary

M2 covers non-atomic process-local Shared/Weak control lifetime, last-strong Drop, many surviving
Weak handles, repeated wakes, drop-time wake failure, nested owner fields, exception cleanup,
value parameters, and the first strong-cycle lint. M3 supplies compile-time owner
borrow/receiver rules.

M4 binds the control's live resource object to the current `GcDomain` and keeps it alive through
the same explicit ownership-root table used by direct Unique. Last-strong release clears the
control and unregisters that root before Drop cleanup. Shared cannot enter the consuming
`intoGc(owner)` bridge, and Weak remains an observation handle rather than a GC root
handle. Multi-mutator atomic ownership and cross-domain transfer remain outside this milestone.
