# Syntax 09 M3 canonical pool layout acceptance

## Status

- State: accepted as a bounded M3 slice; Gate 09 M3 remains open.
- Plan: `docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md`.
- Scope: canonical TypeLayout admission, initialization/copy rollback, deferred
  exactly-once Drop, layout-driven GC visitor routing, canonical native argument
  views, production closed-layout provider convergence, and deterministic guard
  cleanup across normal and abrupt scope exits.

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

## Production convergence

`ZrLib_CallContext_InlineArgumentView` resolves an inline parameter span against
the attached canonical registry and fails closed on missing registry, invalid
layout, pointer drift, or size/alignment mismatch. `Pool<T>.deliver` consumes
that view, fixes the pool to the registry identity and layout id selected by the
first delivery, and rejects even a structurally identical layout from another
registry.

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
allocation. Ordinary interpreter child functions may also lack canonical code
registration and retain materialized-value dispatch; source success alone is
not accepted as non-boxing evidence for every backend.

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

## Remaining Gate 09 M3 work

- Attach canonical inline argument registries to ordinary interpreter call
  frames, so all source backends exercise the same non-boxing provider route.
- Decide and prove movable managed slabs if promotion requires slab relocation;
  the accepted provider currently uses stable native slabs with traced/re-written
  embedded managed values.
- Run the final pause/allocation/scan-byte promotion matrix after M2-M4 close.
- Define isolation-domain-safe per-operation state handling before admitting
  stateful canonical layouts to concurrent pools.

## Acceptance decision

The canonical layout/lifecycle and closed-layout production-provider slice is
accepted after closing the nested-layout bypasses, permanent mirror, external
GC trace/rewrite, registry-identity, writable-copyback, guard cleanup, and stale
`CLOSE_SCOPE` resume findings. Gate 09 M3 stays `indirect`: writable
reference-chain and source cleanup semantics are now proven; interpreter-wide
canonical dispatch and final performance promotion remain open.
