# Syntax 13 M3 Iterator Frame Runtime Implementation Plan

> **For agentic workers:** Execute this plan task-by-task with TDD. Keep all
> work in the existing `main` checkout; this repository uses shared exact-path
> ownership and isolated Git indexes for commits.

**Goal:** Establish a synchronous, GC-safe iterator frame runtime that can
represent yielded values, completion, faults, early close, and reuse without
connecting it to compiler bytecode, async scheduling, artifacts, or LSP.

**Architecture:** A caller-owned `SZrIteratorFrame` is the stack path. A typed
`SZrIteratorFramePool` supplies the reuse path. A frame holds only its explicit
state, current `SZrTypeValue`, optional `SZrGcRootHandle`, cleanup callback,
and reentrancy flag. The public `MoveNext` entry accepts a producer callback;
the callback may publish exactly one current value, complete, or fault. The
runtime owns state transitions and teardown, while a later milestone maps M2
pre-SemIR facts and compiled function code to this primitive.

**Tech Stack:** C17, zr_vm core allocator, `SZrTypeValue`, `GcRootHandle`,
Unity, CMake/Ninja, GCC, Clang, and MSVC.

## Exact Initial Surface

- `zr_vm_core/include/zr_vm_core/iterator_runtime.h`
- `zr_vm_core/src/zr_vm_core/iterator/frame.c`
- `zr_vm_core/src/zr_vm_core/iterator/dispatch.c`
- `tests/iterator/test_iterator_runtime.c`
- `tests/iterator/test_iterator_gc_drop.c`
- `tests/CMakeLists.txt`
- `docs/core-runtime/iterator-frame-runtime.md`
- `docs/core-runtime/index.md`
- `docs/plans/syntax/13-iterator-enumerator-yield/m3-iterator-frame-runtime-implementation-plan.md`
- `docs/plans/syntax/13-iterator-enumerator-yield/m3-iterator-frame-runtime.md`
- `tests/acceptance/2026-07-25-syntax-13-m3-iterator-frame-runtime.md`

## Contract

- The state machine is `READY -> YIELDED -> READY`, then terminal
  `COMPLETED`, `FAULTED`, or `CLOSED`. A terminal frame never invokes its
  producer again.
- `MoveNext` rejects a recursive entry on the same frame and does not alter
  the last current value on that rejection.
- `Publish` is legal only while the producer owns an active `MoveNext` call;
  it copies one current value, roots a GC object through `GcRootHandle`, and
  makes the next `Current` query stable across collection.
- `Complete`, `Fault`, and `Close` clear the current value/root and invoke the
  cleanup callback exactly once. Closing an already terminal frame is a no-op.
- The caller-storage path has no frame allocation. `SZrIteratorFramePool`
  reuses matching frame storage through its free list and exposes acquisition
  and reuse counters for test evidence.
- M3 deliberately does not add compiler lowering, iterator function bytecode,
  execution-dispatch opcodes, async wait states, artifact rows, AOT handling,
  dynamic-object fallback, or LSP facts.

## Task 1: Public Frame Contract RED

**Files:**
- Create: `zr_vm_core/include/zr_vm_core/iterator_runtime.h`
- Test: `tests/iterator/test_iterator_runtime.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Define failing Unity cases for the state machine.**

  Add tests that construct caller-owned storage, publish `1`, `2`, and `3`
  through a producer callback, and assert the sequence of `MoveNext` results
  is `true`, `true`, `true`, then `false`; `Current` is available only after
  each true result. Add zero-yield, fault, early-close, double-close, and
  recursive-producer cases with an integer cleanup counter.

- [x] **Step 2: Register the focused core Unity target and prove RED.**

  Add `zr_vm_iterator_runtime_test`, link it only with `zr_vm_core`, and run:

  ```powershell
  wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/build-s13m3-gcc --target zr_vm_iterator_runtime_test -j4 && ./.codex/build-s13m3-gcc/bin/zr_vm_iterator_runtime_test"
  ```

  Expected result: compilation fails only because `iterator_runtime.h` and its
  `SZrIteratorFrame` API do not exist.

- [x] **Step 3: Publish the narrow public declarations.**

  Declare `EZrIteratorFrameState`, `SZrIteratorFrame`,
  `SZrIteratorFramePool`, `TZrIteratorFrameProducer`, and
  `TZrIteratorFrameCleanup`. Export `Init`, `MoveNext`, `Current`, `Publish`,
  `Complete`, `Fault`, `Close`, `Pool_Init`, `Pool_Acquire`, `Pool_Release`,
  and `Pool_Free`. Producer callbacks receive `state`, `frame`, and opaque
  user data; they must invoke one of publish/complete/fault before returning.

## Task 2: Caller-Owned Frame GREEN

**Files:**
- Create: `zr_vm_core/src/zr_vm_core/iterator/frame.c`
- Create: `zr_vm_core/src/zr_vm_core/iterator/dispatch.c`
- Test: `tests/iterator/test_iterator_runtime.c`

- [x] **Step 1: Implement value/root ownership in `frame.c`.**

  Initialize current values as null. `Publish` first clears the old value/root,
  copies the provided `SZrTypeValue`, and creates a `GcRootHandle` when the
  copied value is a GC object. `Current` resolves that root before returning
  an object value so collection relocation cannot leave a stale pointer.
  `frame_clear_current` releases the root, calls
  `ZrCore_Ownership_ReleaseValue`, and resets the value to null.

- [x] **Step 2: Implement transition and cleanup control in `dispatch.c`.**

  Set `isMoving` before calling the producer and clear it on every return.
  Convert `YIELDED` to `READY` before the next producer call. Reject producer
  return without a publish/complete/fault transition as `FAULTED`. Terminal
  transitions call one private `frame_finish` helper that clears current state
  and invokes cleanup once.

- [x] **Step 3: Run focused GREEN.**

  Re-run `zr_vm_iterator_runtime_test`; expected Unity result is all state,
  close, fault, and reentrancy cases passing with no runtime allocation on the
  caller-owned path.

## Task 3: GC And Ownership Regression RED/GREEN

**Files:**
- Test: `tests/iterator/test_iterator_gc_drop.c`
- Modify: `zr_vm_core/src/zr_vm_core/iterator/frame.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Add object-root and owner-cleanup RED cases.**

  Yield a GC object, force collection while the frame is `YIELDED`, and assert
  `Current` resolves a live object. Add a cleanup callback that owns a direct
  resource value and assert it runs once for completion, fault, and early
  close, including after a repeated close.

- [x] **Step 2: Implement the minimum root-release and cleanup ordering.**

  Keep the root alive until the terminal transition; release the root before
  the callback so cleanup cannot observe stale frame storage. Do not add a new
  GC object kind or a generic object-member fallback.

- [x] **Step 3: Run the GC/drop target.**

  Build and run `zr_vm_iterator_gc_drop_test`. Expected Unity result: every
  forced-GC current-value case and every exactly-once cleanup case passes.

## Task 4: Typed Pool Reuse RED/GREEN

**Files:**
- Modify: `zr_vm_core/include/zr_vm_core/iterator_runtime.h`
- Modify: `zr_vm_core/src/zr_vm_core/iterator/frame.c`
- Test: `tests/iterator/test_iterator_runtime.c`

- [x] **Step 1: Add pool counter RED cases.**

  Acquire a frame, publish/close/release it, then acquire another frame from
  the same pool. Assert the storage address is reused, `allocationCount`
  remains one, `reuseCount` becomes one, and no current/root/cleanup state
  leaks into the second lease.

- [x] **Step 2: Implement a typed free-list.**

  Allocate a fixed `SZrIteratorFrame` node through the core allocator only
  when the pool free list is empty. `Pool_Release` requires terminal state,
  clears the frame, then returns it to that pool's free list. `Pool_Free`
  releases every free-list allocation with its original typed size.

- [x] **Step 3: Re-run both core targets.**

  Expected Unity results: the runtime state target and GC/drop target both
  pass, proving the caller-storage and pooled paths remain separate.

## Task 5: Evidence, Documentation, And Commit

**Files:**
- Create: `docs/core-runtime/iterator-frame-runtime.md`
- Modify: `docs/core-runtime/index.md`
- Modify: both M3 plan/status files
- Create: `tests/acceptance/2026-07-25-syntax-13-m3-iterator-frame-runtime.md`

- [x] **Step 1: Document exact ownership and M3 exclusions.**

  Add YAML front matter with exact code/test paths. State that the runtime is a
  core primitive and consumes producer callbacks only; compiler mapping of M2
  SemIR, async iteration, execution dispatch, artifact/AOT, debug/LSP, and
  migration are excluded.

- [x] **Step 2: Run independent three-toolchain evidence.**

  Configure `.codex/build-s13m3-gcc`, `.codex/build-s13m3-clang`, and
  `.codex/build-s13m3-msvc`. Build and run both direct Unity targets in each
  directory. Record real exits, Unity totals, and any pre-existing markers.

- [x] **Step 3: Complete the status record and commit.**

  Write completion time, status, implementation list, exact acceptance results,
  and exclusions under `## 状态与产出记录`. Stage only the final M3 paths with
  `GIT_INDEX_FILE=.git/index-syntax13-m3-stage`, verify cached paths and
  `git diff --cached --check`, then create one `feat(syntax):` milestone commit.

## Self-Review

- The plan consumes M2 facts only conceptually; it does not require a compiler
  or execution-dispatch edit, so the runtime can be tested independently.
- Every terminal state owns the same root/value cleanup path, avoiding separate
  completion, fault, and close cleanup semantics.
- Async, artifact, AOT, debug/LSP, legacy generator migration, and an
  object-property iterator fallback remain outside the exact surface.
