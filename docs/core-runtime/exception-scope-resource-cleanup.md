---
related_code:
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.h
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.c
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.h
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.h
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.c
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.h
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-07-19 按 docs/plans/syntax 严格执行并逐里程碑提交
  - user: 2026-09-27 继续 docs/plans/ssa、完成验证并逐子任务提交
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/core/test_close_proxy.c
  - tests/library/test_close_proxy_aot_runtime.c
  - tests/core/test_close_meta_exception.c
  - tests/cmake/close-proxy-tests.cmake
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_resource_shared_weak.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/parser/test_aot_c_call_shared_library_smoke.c
doc_type: module-detail
---

# Exception Scope Resource Cleanup

## Purpose

`using(resource)` resources are represented by the VM's to-be-closed stack chain. Normal
scope exit already closes registrations, but exception transfer must also close
the resources created inside the abandoned try scope before control reaches a
catch or finally block. Syntax03 M5 makes that ordering explicit for PoolLease and
other close-meta providers.

## Handler Checkpoints

Every pushed `SZrVmExceptionHandlerState` saves the current to-be-closed chain as
a stack-relative offset. The offset survives stack relocation. During exception
unwind, the VM resolves the saved boundary and repeatedly closes the top
registration while it is above the boundary.

Cleanup runs before:

- entering a matching catch;
- entering a finally block with a pending exception;
- popping a handler that cannot handle the exception;
- leaving a handler whose finally phase throws again.

Outer registrations remain linked because their stack positions are at or below
the saved boundary. Nested handlers therefore close only the resources whose
lexical scope is being abandoned, in LIFO order.

## Close Call Scratch Contract

A close meta call needs three values: callable, resource receiver, and error
argument. Scratch reservation may grow or relocate the stack, so the resource is
first saved as a stack offset and reloaded after reservation. The error object is
built directly in the third scratch slot. It is never constructed next to the
registered resource, where it could overwrite another live local or cached
callable.

The close function runs without yield during exception unwind. A normal close
receives null; exceptional close receives the current exception status projected
as an error value. The existing close-registration pop remains the single source
of truth, so the same registration cannot be invoked twice by one unwind.

## Pending-error close callbacks

When an exceptional close calls script `@close`, the outer Error must stay
available to the callback as its error argument without remaining the VM's
active exception during that nested call. Otherwise `RESUME_AFTER_NATIVE_CALL`
can resume exception dispatch into the outer catch from inside `@close` and
clear the Error before the original unwind reaches it. The same failure occurs
for an ordinary `using(new Resource())` registration and for a close proxy.

`closure_value_call_close_meta` reserves one additional scratch slot only
when an Error is pending. `closure_close_meta_guard.c` initializes a real native
call-info frame in that slot. Exception dispatch stops at this frame, while a
nested `TryRun` captures a new error raised by the callback. The guard saves the
original exception value and status and roots its Error object through an AOT
root frame before clearing the ambient exception. If `@close` returns normally,
the guard restores the original Error and its thread status only when `TryRun`
returns `FINE`, the thread status is still `FINE`, and no exception is active.
If a budget poll terminates the callback call, the guard preserves the
execution-terminated state and does not restore the saved Error after the poll
clears `hasCurrentException`. If `@close` throws, the new Error remains active and
takes precedence. The callback argument is a separate rooted stack value
throughout the call. A status without an active Error continues through the
existing callback path: nested exception dispatch and `CATCH` only act when
`hasCurrentException` is set.

The guard restores the outer call-info node, native-call yield count,
execution-budget native-frame marker, and logical stack top after the callback.
It trims callback handlers to the entry depth and rejects a handler underflow.
The logical top is reconstructed from the scratch-slot byte
offset; `outer->functionTop` remains a frame high-water boundary and cannot be
used as the logical top because repeated cleanup would advance it every time.
Stack offsets survive stack growth. A nested AOT root frame may be left on the
state chain when a native callback longjumps past its own Pop. Immediately after
`TryRun`, the guard cuts that chain back to its still-live root frame without
following pointers into returned C stack frames, then pops its own root. A
normal-return callback that leaves extra roots is rejected as an error.

A direct native `@close` throw can also bypass the callback's ordinary
`PostCall`. The guard discards exactly one directly linked native child frame
when it has the expected callback slot, no return destination, no inline frame
metadata, and no open upvalues or close registrations. The reusable `next`
chain remains linked. Other residual frame shapes, handler underflow, and
unexpected pending control are diagnosed rather than silently skipped; these
paths require a separate cleanup protocol before they can safely resume the
outer catch.

## Ownership handles

The same chain directly closes `Unique`, `Shared`, `Weak`, and `Loan` values. Frame-layout locals
can keep their physical value outside the dense stack slot used by the close chain, so ownership
operations synchronize a retained cleanup mirror before and after overwrite, move, share, weak,
upgrade, release, and loan transitions. This prevents an exception from releasing a stale control
or leaving an extra strong/weak count alive.

Shared/Weak value parameters are also balanced across calls. After a callee successfully copies a
non-borrowed parameter, the caller staging owner is released. An exception then closes only the
callee copy plus other live lexical registrations. The final strong release marks the stable
control dead before resource Drop, which makes an upgrade attempted during Drop return empty.

## Distinct physical frame values

A frame-layout VALUE slot can have a dense registered cleanup cell and a distinct
byte-frame physical `SZrTypeValue`. Close processing resolves the physical cell
through the active `SZrCallInfo` chain. When both cells retain the same ownership
control, or the dense cell aliases a direct resource owner, cleanup clears the
dense registration before releasing the physical value. This ordering is
required because a resource destructor may re-enter the VM and grow or relocate
the stack. Re-entrant code must observe the registration as null and must not
retain a pointer that became stale during the destructor call.

The physical value is the release source when it is distinct and still owns the
resource. The dense mirror is then reloaded from its stack offset and reset, so
ordinary objects, resources, loans, Shared/Weak controls, overwritten physical
slots, and pre-close stack relocation all converge on one release without a
stale alias or duplicate Drop.

## Close proxies for an existing local

An inner `using(existingLocal)` must add a cleanup registration above the current
to-be-closed marker, even when the local already has an outer registration. The
core API `ZrCore_Closure_MarkCloseProxy(state, proxySlot, sourceSlot)` registers
an empty, rooted high stack slot while retaining the original dense local as the
source. It fails without changing the close chain if the proxy slot is occupied,
is below the source or current marker, or lies outside the active stack. The
source may have an outer marker or no marker. A body-free `using existingLocal;`
in a nested lexical scope has the same need for a high registration.

The proxy slot contains a private, GC-scanned NativeData token with byte offsets
for the source and proxy slots relative to the stack base. Offsets survive stack
relocation. A file-private address identifies the token only inside the live
process; neither that address nor the NativeData representation is part of the
bytecode or artifact ABI. A token copied to another stack slot fails its slot
identity check. The private token functions live in `closure_close_proxy_token.c`
so `closure.c` retains ownership of the close chain and callback protocol.

At close, a plain value without ownership cleanup or a callable `@close`
leaves the logical source and its physical mirror readable. A borrowed view is
cleared in both locations, following `OWN_DROP` without invoking the object's
`@close` or releasing its owner. For values that require cleanup, the VM moves
the source into the rooted high slot, clears the source and any distinct VM
frame-layout physical mirror, then invokes its close meta or ownership release.
A callback that grows the stack or throws therefore sees the original local as
null. The close receiver is copied into scratch before the high slot is reset,
so it remains rooted for the callback.
When dense and physical cells both retain an ownership control, the redundant
mirror reference is released once; a direct owner alias is only tombstoned to
avoid duplicate Drop. Ordinary legacy close registrations retain their existing
physical-mirror lookup behavior; only proxy lookup requires an active VM frame
to avoid interpreting an inactive native call-info layout.

The proxy occupies one node in the existing marker chain. Normal scope exit
consumes that node through `CLOSE_SCOPE(1)`; exception unwind closes it above the
handler checkpoint before catch. Older markers remain linked, and their source
is already null when later popped. Nested proxies for a closable value see a
null source after the first close and are inert.

`MARK_CLOSE_PROXY` is appended at opcode 245. Its E operand names the new high
proxy slot, and A1 names the existing dense source slot. The interpreter and
frame-slot scan recognize both operands. AOT C and LLVM lower it to
`ZrLibrary_AotRuntime_MarkCloseProxy(state, frame, E, A1)`. The AOT helper uses
the ordinary cleanup registration preparation to select the high physical
VALUE slot, then passes the dense source to the core API so cleanup clears its
physical mirror as well. Invalid or out-of-order slots fail before registration.
The generated helper symbol is required when linking a module that contains
this opcode against the runtime; an older runtime cannot execute such a module.

## Generated-call exception transfer

AOT C and LLVM calls complete through resume-aware runtime boundaries. A normal
return finishes the prepared call. A caught exception that resumes in the same
generated caller refreshes that caller frame and dispatches at the resume
instruction. An exception that has already unwound beyond that caller is left
unchanged for the outer handler; it is not rewritten as an AOT runtime failure.
This keeps nested direct/meta-call cleanup on the same exception-scope contract
as interpreter execution.

## Verification

`zr_vm_close_proxy_aot_runtime_test` is a manual helper fixture for an ordinary
closable object. It constructs a `ZrAotGeneratedFrame` and active VM call-info;
it does not enter a generated AOT function. Dense logical source/proxy slots
0/1 map to separate physical VALUE storage at stack-slot offsets 2/3. The
outer registration occupies the physical source, while the higher physical
proxy token targets the dense source. The first `CloseScope(1)` clears both
source representations before the native close callback; the second removes
the already-null physical source without another callback, ending at the
`stackBase` close-chain sentinel. `CallWithoutYield(..., 0)` reaches ordinary
native pre-call dispatch and the prepared native-frame invocation, rather than
SingleResultFastRestore. The callback observes source nullness and call count;
it does not force GC, stack growth or reentry, and the fixture has no GC API or
GC-before/after assertions. The 2026-10-04 paired clang-cl check compiled both
original and comment candidate successfully; this is compile-only evidence,
with no runtime, GC, CTest or generated-entry execution credit.

`zr_vm_close_proxy_core_test` exercises 19 focused cases: one close with an older
marker, an unmarked source, plain source and distinct mirror preservation,
borrowed view reset without `@close`, exceptional close and handler boundary,
nested proxies, distinct dense/physical mirrors, full GC with an active token
and during the close callback, original Error survival across full GC, native
replacement Error and repeated call-info reuse, an AOT root frame abandoned by
a throwing native callback, cancelled budget preservation without callback
invocation, registration order rejection, AOT-like physical marker ordering,
a stale native-frame layout, copied-token rejection, and both retained control
and direct-owner mirror aliases. `zr_vm_type_layout_inline_copy_test`
protects the legacy physical-mirror path; `zr_vm_native_closure_value_test`
protects native closure metadata handling.

`zr_vm_close_meta_exception_test` covers four script-level paths: a plain
throw/catch control, repeated cleanup and call-info chain reachability, the
original Error reaching its catch after script `@close`, and a callback's new
Error replacing the original one.

`zr_vm_buffer_pool_ffi_test` throws from inside `using(lease)`, catches outside,
then rents the same size again. The expected generation and return/reuse counters
prove that cleanup ran exactly once before catch and did not corrupt adjacent VM
state. Parent using/escape tests protect normal close and structured cleanup
behavior.

`zr_vm_resource_shared_weak_test` throws after creating Shared clones and a Weak observer, then
asserts that unwind runs resource Drop once. It also covers value-parameter copies, nested
Shared/Weak fields, final-strong behavior, and wake failure after the last
strong owner is dropped.

`zr_vm_type_layout_inline_copy_test` covers distinct dense/physical ownership
cells, including a resource Drop callback that verifies the dense alias is
already null and then forces stack growth. The AOT call and receiver shared
library suites cover caught nested exceptions, tail callable propagation, and
post-call Weak expiry through generated C and LLVM.
