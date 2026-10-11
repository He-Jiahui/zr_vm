---
related_code:
  - tests/core/test_ssa_owner_arm_event.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-owner-arm-event.cmake
  - tests/cmake/ssa-owner-arm-event-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-owner-arm-event-check.cmake
implementation_files:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - .codex/plans/20261010-ssa-nonasserting-owner-observers.md
  - 'Root: 2026-10-10 finite nonasserting Arm/Event primitive gate'
tests:
  - tests/core/test_ssa_owner_arm_event.c
  - tests/cmake/ssa-owner-arm-event-check.cmake
  - docs/testing-and-validation/ssa-owner-arm-event.md
  - docs/acceptance/ssa-owner-arm-event.md
doc_type: module-detail
status: finite-support-accepted
---

# Nonasserting owner Arm and Event observations

## Purpose and current state

Root accepted this finite support gate on 2026-10-10: functional RED, an independently fresh default-GREEN runtime under Clang UBSan, and three-unit MSVC compilation. Actual GREEN has eleven passes and one deliberate prior Unity failure; the exact checker accepts child exit1 with wrapper exit0. The actual allocator header declares both APIs and current providers implement scheduling and event copies. See the [testing guide](../testing-and-validation/ssa-owner-arm-event.md) and [acceptance record](../acceptance/ssa-owner-arm-event.md) for receipts and limits.

Unity's numeric assertion functions can abort immediately under an earlier failed or ignored test. Existing asserting Arm and Event therefore cannot supply nonasserting teardown witnesses. The approved plan adds TryArm and TryEvent over the same authoritative rows, events, counters and pending fault. Existing asserting wrapper bodies and allocator behavior must retain their original semantics. Provider changes are separately owned by Root; these descriptors do not implement an observer or substitute a provider.

## Contracts

These GREEN contracts are implemented and exercised by the finite matrix. Historical RED retained the real guards but refused positive requests; it produced the required missing-feature failures after actual cleanup.

`int ZrTests_ExecOwner_TryArm(unsigned relativeAttempt)` explicitly schedules one fault. It accepts only when no fault is pending, attempts is within the private event bound, relativeAttempt is positive, and relativeAttempt fits the remaining event capacity. Check bounds before subtraction or addition can wrap. Success returns 1 and changes the existing pending schedule only; refusal returns 0 without modifying the schedule or ledger. Attempts, records, live, frees, errors, failures and existing events stay unchanged during this call. It allocates nothing.

`int ZrTests_ExecOwner_TryEvent(unsigned index, SZrOwnerHeapEvent *output)` copies one actual event into genuine independent writable output when output is nonnull and index is within both current attempts and the private event bound. Success returns 1 and copies kind, record, failed, count, width and bytes. Refusal returns 0 and preserves the complete initialized output object representation. No query allocates, rebases, disarms, appends an event or mutates counters.

These APIs coordinate a serialized test ledger. Genuine output readability and independence are caller preconditions, not inferred from a numeric address. The finite test uses actual typed local objects and registered CRT allocations; it probes no arbitrary address or output alias. A ledger record or numeric live span establishes neither VM identity nor typed owner authenticity.

## Real allocations and cleanup

The main immediately registers malloc/calloc/realloc returns. Before ordinary feature assertions, it disarms and frees registered roots, saves all six named counters and verifies actual per-record free queries. Stats supplies independent health and purity observations; a positive feature still requires the public TryArm/TryEvent success and exact event fields.

Cases 5 and 6 use the original asserting Arm while Unity is clean. They check a failed realloc retains the registered pointer/content and that successful event queries leave the next armed allocation failure intact. Case 10 arms the next real request before invalid event queries, verifies null/index==attempts/UINT_MAX refusal and sentinel preservation, then registers the real next allocation result and requires its original fault to fire. Its post-drain counters are attempts2, records1, frees1 and failures1 in both phases.

Case 11 deliberately fails Unity after registering a genuine allocation. Nonasserting teardown then calls TryArm and TryEvent, attempts the next real allocation, reads actual record free counts while live, disarms and frees registrations, and records post-free counts and unchanged failed/ignored flags. Teardown contains no Unity assertion, reset or recovery. Case 12 runs after actual Unity conclusion without resetting the ledger; it compares saved live-stage query success/free0 and released-stage success/free1 against current real queries and exact saved events.

The final drain disarms and frees remaining registrations, reads the final real records, and preserves Unity flags before UNITY_END. It performs no recovery or error reset. Healthy support requires live0/errors0 and one actual free for every successful allocation record.

## Finite closure and boundaries

The target has exactly three C translation units: the authored main, actual allocator C and actual Unity C. It selects no Core/Parser library, fixture lifecycle provider, Source compiler, VM bootstrap, archive, forced include, fake Unity, network or FFI. Unity uses actual repository configuration and stdout flushing.

The twelve-case protocol intentionally includes one deliberate prior Unity failure in both phases. Actual RED produced seven further missing-feature failures and four genuine guard passes; actual default GREEN passed all eleven other cases. Both had zero ignored tests, empty stderr and exact numerical drain witnesses. A successful CTest checker means this negative protocol matched; it does not mean the Unity child exited zero. MSVC compiled the same three source units without warnings, linking or runtime execution.

The private 65536-event capacity boundary is not exercised by these twelve cases. The authored matrix covers UINT_MAX refusal at nonzero attempts; exhaustion with real requests, complete OOM behavior, arbitrary pointer validation and ignored-path execution remain OPEN. Pure TryCapture, Source's twenty cases and genuine finalizer wiring require separate work. Source/VM/Function, full47, normal full-project defaults, Linux and full MSVC runtime receive no credit from this finite gate.
