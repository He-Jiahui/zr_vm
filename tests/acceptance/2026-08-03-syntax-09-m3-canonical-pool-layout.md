# Syntax 09 M3 canonical pool layout acceptance

## Status

- State: `proven` for M3; Gate 09 remains open at M4 and M5.
- Plan: `docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md`.
- Scope: canonical TypeLayout admission, initialization/copy rollback, deferred
  exactly-once Drop, layout-driven GC visitor routing, canonical native argument
  views for artifacts and ordinary interpreter functions, production
  closed-layout provider convergence, and deterministic guard cleanup across
  normal and abrupt scope exits.

## RED evidence

1. The first focused test could not compile or link because
   `ZrPool_CreateFromTypeLayout` did not exist.
2. After the initial bridge, a non-blittable bitwise layout without a canonical
   copy path and a layout with a dangling nested registry were both admitted.
   The two fail-closed assertions reproduced the gap before recursive admission
   validation was added.
3. Review then showed that canonical `SZrTypeValue` storage accepted a null VM
   state. The new test returned `ZR_POOL_STATUS_OK` instead of
   `ZR_POOL_STATUS_INVALID_ARGUMENT` before recursive state-dependency admission
   was added.
4. Independent review reproduced two nested-layout bypasses: a valid GcFree/
   DropNone root admitted a managed nested value without a visitor, and a raw-copy
   root admitted a move-only nested layout. Both focused tests returned
   `ZR_POOL_STATUS_OK` before recursive lifecycle consistency checks were added.
5. Follow-up review found that a direct ownership value-slot could still declare
   `DropNone`. The focused test reproduced successful admission before the same
   recursive check began rejecting direct ownership/Drop downgrades.
6. MSVC ASan then reproduced a deterministic 1/4 failure after a read guard
   closed: `CLOSE_SCOPE` invoked its native close callback while the parent call
   frame still stored the previous property-load PC. Nested execution resumed at
   that stale PC and loaded through the already closed view. The existing
   close-then-recycle source regression was RED before the dispatcher saved the
   next PC and used the native-call resume protocol.
7. The ordinary-interpreter registry test removed its fabricated
   `SZrAotCodeRegistration` before production changes. The focused WSL GCC run
   then reported 3/4 passing: only writable native struct copyback failed at
   `ZrLib_CallContext_InlineArgumentView`, proving that a source function without
   artifact metadata could not expose its canonical prototype layout.

## Implemented contract

- `ZrPool_CreateFromTypeLayout` derives element size, alignment, GC scan class,
  copy, Drop, and scan callbacks from one validated `SZrTypeLayout`.
- Admission rejects move-only layouts, non-blittable layouts without fieldwise
  copy, missing nested registry entries, raw roots over non-raw nested layouts,
  root scan/Drop downgrades, managed layouts without a visitor, and value/
  value-slot layouts without a VM state. A root GC offset table is also rejected
  when it would bypass managed nested traversal.
- Stateful canonical layouts reject concurrent mode because the current bridge
  borrows one VM state for its lifetime. GcFree native layouts that do not need a
  state retain the existing concurrent pool path.
- Initialization uses canonical zero initialization followed by registry-aware
  copy. A failed copy or non-fine VM thread status drops initialized storage,
  clears the slot, increments construction-failure accounting, and publishes no
  handle.
- Recycle plus outstanding guards retains the initialized slot until the final
  guard closes; canonical Drop then runs exactly once.
- GcMapped layouts visit only initialized live/retired slots through the
  canonical visitor. GcFree layouts report zero scanned slots and bytes.
- The public API copies the root layout value but borrows the VM state, nested
  layouts, field/offset tables, registry backing, and callback user data. Those
  objects must outlive the pool.
- An ordinary entry function lazily owns one stable prototype-layout pointer
  registry. Resolving a required id also resolves reachable local nested layouts;
  callers borrow the published table, and repeated calls preserve its identity.
  Invalid ids fail with a cleared view. The registry is released with the
  function's prototype-layout cache.

## Production convergence

`ZrLib_CallContext_InlineArgumentView` resolves an inline parameter span against
the attached artifact registry or, for source functions with no artifact
registration, the function-owned prototype-layout registry. It fails closed on
missing registry, invalid layout, pointer drift, or size/alignment mismatch.
The presence of any artifact registration forbids source-cache fallback, so a
corrupt artifact cannot be repaired accidentally from prototypes.
`Pool<T>.deliver` consumes that view, fixes the pool to the registry identity and
layout id selected by the first delivery, and rejects even a structurally
identical layout from another registry.

The production provider stores the canonical bytes in the slab and no longer
creates `__zr_pool_values`. A temporary object projection exists only while a
read/write guard is active. Writable close copies the projection back into
inline storage, propagates copy failure, and releases the guard exactly once.
Return, throw, break, continue, ordinary block exit, and `out`-argument view
replacement all release the active guard before the next borrow/recycle.

`PROPERTY_REF_STORE` keeps the registered to-be-closed stack mirror synchronized
when an `out` destination is represented by a separate argument value. The
interpreter's `CLOSE_SCOPE` stores `programCounter + 1` before invoking close
metadata and then applies the normal native-call exception/base/trap refresh.
A nested native close therefore cannot resume at an instruction that consumed
the just-closed view.

`SZrRawObject.traceGcFunction` separates external child enumeration from the
existing finalizer callback. Pool owners use it to visit initialized live and
retired slots. Classic mark, minor live-young checks, and forwarding rewrite
share the visitor contract; publication/copyback and metadata binding issue
write barriers. A direct external-storage regression proves full-compaction and
barriered-minor survival/rewrite plus exactly-once finalization.
Because this changes the public raw-object layout, native runtime ABI is v4 and
the exact plugin descriptor ABI is v6; older binaries must be rebuilt and fail
registration rather than using stale field offsets.

This is compact-safe native stable slab storage, not movable managed slab
allocation. Ordinary interpreter child functions now use their entry function's
canonical prototype-layout registry and exercise the same non-boxing provider
route without fabricating artifact registration.

## Reference evidence

- QuickJS `lua/QuickJS-master/quickjs.c:5966` and `:5981` route marking through
  value/object layout, while `tests/test_builtin.js:920` and adjacent cases force
  collections around lifecycle behavior.
- Mono `lua/mono/mono/sgen/sgen-descriptor.c:3` and `:116` make object scanning a
  descriptor/layout fact; `mono/tests/critical-finalizers.cs:47` forces collection
  and waits for finalizers.
- Rust `lua/rust/library/alloc/src/vec/in_place_drop.rs:8` and `:21` use scoped
  Drop guards for initialized destinations; `tests/ui/drop/conditional-drop-10734.rs:15`
  asserts that a value is not dropped twice.
- .NET `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Span.cs`
  models a borrowed by-reference plus length in a `readonly ref struct`; its
  pinnable reference is meaningful only while the backing storage remains live.
- Rust `lua/rust/library/core/src/pin.rs` makes stable backing and projection
  lifetime explicit. ZR similarly owns registry backing on the entry function
  and exposes only borrowed views rather than copying transient pointer tables.
- Mono `lua/mono/mono/metadata/class-init.c` keeps canonical class layout/init
  state on the long-lived runtime class object. This supports publishing the ZR
  registry only after the required layout is completely resolved.
- CPython `lua/cpython/Objects/obmalloc.c:2063` and `:2302` separate arena/pool
  allocation and reuse bookkeeping. It informs slab organization but is not the
  source of ZR's managed scan or Drop semantics.

## Focused evidence

- Final WSL GCC 11.4, WSL Clang 14.0, and MSVC 19.44 Debug replay: each toolchain
  passed the same 21 affected executables and 530/530 Unity assertions. This
  includes native inline view 7/7, external inline/GC layout 2/2, canonical pool
  TypeLayout 14/14, production closed-layout runtime 4/4, property reference
  23/23, property lowering 22/22, type inference 122/122, and pool lifecycle
  13/13.
- MSVC 19.44 ASan replay passed GC 67/67, concurrent GC 10/10, external inline
  layout 2/2, native inline view 7/7, pool lifecycle 13/13, pool GC stress 3/3,
  canonical pool TypeLayout 14/14, and production closed-layout runtime 4/4:
  120/120 assertions total.
- The ordinary-interpreter registry slice was replayed as the same 13 executables
  under WSL GCC 11.4, WSL Clang 14.0, and MSVC 19.44 Debug. Each toolchain passed
  271/271 assertions: core inline layout 38, layout metadata 9, inline array 2,
  native inline view 7, metadata runtime 11, pool lifecycle 13, pool GC stress 3,
  canonical pool layout 14, production runtime 4, artifact 3, property lowering
  22, property ref 23, and type inference 122. A static MSVC `/Od
  /fsanitize=address` replay passed the four registry/lifetime targets 60/60
  (4 + 7 + 38 + 11) with no sanitizer report.
- The production runtime test covers mirror absence, registry-identity
  rejection, source writable `ref T` member-chain mutation, ordinary value
  consumption, explicit close/readback, and real prototype plus canonical
  registry writable copyback.
- WSL GCC 11.4 Debug: canonical TypeLayout 14/14, pool 13/13, GC stress 3/3,
  artifact 3/3, core inline TypeLayout 38/38, and TypeLayout metadata contracts
  9/9.
- WSL Clang 14.0 Debug: canonical TypeLayout 14/14; the focused pool sources
  produced no new warning.
- MSVC 19.44 Debug shared-library build: canonical TypeLayout 14/14; the focused
  pool sources produced no C4xxx warning.
- The new test is part of the registered `containers` aggregate rather than an
  unregistered direct-only executable; the GCC `ctest -R ^containers$` replay
  passed 1/1 with all nine aggregate executables.
- `generational_pool.c` was reduced from 1044 to 850 lines by extracting the
  canonical bridge into a cohesive module and a private 44-line header.

## Storage and concurrency decision

The design permits managed slabs with a compact-updated base handle or
native/ownership slabs with stable allocation. This implementation deliberately
selects the latter: active slabs never move, managed children are traced and
rewritten through the canonical external visitor, and source views retain only
a guard pointer into that stable native allocation rather than an interior
pointer into movable managed storage. Full compaction and barriered minor
collection tests prove that embedded children survive and are rewritten while
guard identity remains stable. A movable managed-slab variant is optional future
work, not an M3 promotion requirement.

The design also separates thread-local and concurrent capabilities. Stateful
canonical layouts borrow one VM state and are rejected at concurrent-pool
admission. GcFree layouts with no state dependency use the existing atomic
concurrent path, which has direct churn coverage. Per-operation isolation-domain
state is required only before widening concurrent admission; fail-closed
rejection is the accepted current boundary.

The final pause/allocation/scan-byte comparison belongs to M5 and remains open
until M4 consumer acceptance is complete.

## Acceptance decision

M3 is `proven` after closing the nested-layout bypasses, permanent mirror,
external GC trace/rewrite, registry-identity, writable-copyback, guard cleanup,
stale `CLOSE_SCOPE` resume, ordinary-interpreter registry, compact-safe stable
slab, and concurrency-capability findings. Gate 09 remains open because M4
consumer acceptance and the M5 pause/allocation/scan-byte matrix are separate
promotion gates.
