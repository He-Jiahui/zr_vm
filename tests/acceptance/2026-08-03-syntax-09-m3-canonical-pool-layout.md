# Syntax 09 M3 canonical pool layout acceptance

## Status

- State: accepted as a bounded M3 slice; Gate 09 M3 remains open.
- Plan: `docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md`.
- Scope: canonical TypeLayout admission, initialization/copy rollback, deferred
  exactly-once Drop, layout-driven GC visitor routing, and production erased-value
  provider convergence.

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

`pooling_generational_runtime.c` no longer hard-codes `sizeof(SZrTypeValue)`,
alignment, copy, or Drop callbacks. It initializes the canonical value layout and
creates the native pool through `ZrPool_CreateFromTypeLayout`.

The provider still mirrors values into the hidden `__zr_pool_values` array, which
is the current GC root owner. Its canonical visitor is therefore deliberately a
no-op to avoid double ownership. This slice does not claim closed-`T` inline
moving slabs or language-level early-exit cleanup for native guards.

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

- WSL GCC 11.4 Debug: canonical TypeLayout 14/14, pool 13/13, GC stress 3/3,
  artifact 3/3, core inline TypeLayout 38/38, and TypeLayout metadata contracts
  9/9.
- WSL Clang 14.0 Debug: canonical TypeLayout 14/14; the focused pool sources
  produced no new warning.
- MSVC 19.44 Debug shared-library build: canonical TypeLayout 14/14; the focused
  pool sources produced no C4xxx warning.
- The new test is part of the registered `containers` aggregate rather than an
  unregistered direct-only executable; the GCC `ctest -R ^containers$` replay
  passed 1/1 with all eight aggregate executables.
- `generational_pool.c` was reduced from 1044 to 850 lines by extracting the
  canonical bridge into a cohesive module and a private 44-line header.

## Remaining Gate 09 M3 work

- Replace the erased-value mirror with compact-safe, closed-`T` managed slab
  storage and prove moving/full-GC behavior.
- Prove language-level return/break/continue/throw cleanup and view replacement
  ordering for native pool guards.
- Run the final pause/allocation/scan-byte promotion matrix after M2-M4 close.
- Define isolation-domain-safe per-operation state handling before admitting
  stateful canonical layouts to concurrent pools.

## Acceptance decision

The canonical layout/lifecycle slice is accepted after closing the independent
review's two Critical nested-layout bypasses and one Important state-lifetime
documentation gap. No known Critical or Important issue remains. Gate 09 M3
stays `indirect`, because moving slabs and language early-exit cleanup are
explicitly outside this slice.
