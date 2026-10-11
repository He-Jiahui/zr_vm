---
related_code:
  - tests/core/test_ssa_owner_try_capture.c
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity.h
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/cmake/ssa-owner-try-capture.cmake
  - tests/cmake/ssa-owner-try-capture-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-owner-try-capture-check.cmake
  - tests/cmake/ssa-execir-owned-module.cmake
  - tests/cmake/ssa-execir-module-copy.cmake
  - tests/cmake/ssa-function-execir-owner.cmake
  - tests/cmake/ssa-source-finalized-module.cmake
  - tests/cmake/ssa-source-returned-execir-retention.cmake
implementation_files:
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
plan_sources:
  - .codex/plans/20261010-ssa-nonasserting-owner-observers.md
  - 'Root: 2026-10-10 finite pure TryCapture RED authoring'
tests:
  - tests/core/test_ssa_owner_try_capture.c
  - tests/cmake/ssa-owner-try-capture-check.cmake
  - docs/testing-and-validation/ssa-owner-try-capture.md
  - docs/acceptance/ssa-owner-try-capture.md
doc_type: module-detail
status: finite-validated
---

# Nonasserting logical owner capture

## Purpose and current state

The approved plan extracts the existing logical Capture traversal into one shared test support module and adds `ZrTests_ExecOwner_TryCapture`. Functional RED was executed against the always-refusing adapter before the positive implementation. Fresh default GREEN passes eleven cases with one deliberate prior Unity failure; the exact negative-protocol wrapper passes. Three actual MSVC units compile, and separate lower owned-module/module-copy/CoreModel regression passes. These are finite support results. See the [testing guide](../testing-and-validation/ssa-owner-try-capture.md) and [acceptance boundary](../acceptance/ssa-owner-try-capture.md).

The pure observer supports teardown after an earlier Unity failure, where asserting Capture cannot reliably provide a complete witness. It observes genuine typed ExecIR storage. It neither constructs nor verifies executable IR, authenticates a returned owner, nor establishes Source/VM identity. Header access to ownership or state enum values gives no security, network or lifecycle execution credit.

## Public contract

`int ZrTests_ExecOwner_TryCapture(const SZrExecIrModule *module, SZrOwnerContents *contents)` uses a genuine live readable typed graph and independent writable output disjoint from that graph. Every selected pool must actually contain a readable span for its count. Present GC map, state map and frame layout objects must themselves be readable. Count/capacity checks do not prove pointer validity, ownership, schema validity or owner identity. The gate uses registered real CRT pools and initialized typed objects; it supplies no arbitrary address or aliased output.

The implemented GREEN adapter returns 1 with logical fields in legacy order. Its output has `words[4096]` and a count; unused words are zero. Refusal returns 0 with the entire initialized output representation unchanged. Null inputs, count greater than capacity, nonzero count with absent storage, multiplication overflow and a traversal exceeding 4096 words are refusal conditions. The actual GREEN matrix exercises valid captures, null/shape/budget refusals, exact word content and whole-output preservation. Historical RED refused every request, including valid ones.

The checked visitor already validates shapes before iterating selected storage and checks remaining word budget before evaluating each payload expression. It checks optional-object geometry when that readable object is reached. It is one traversal, with no second preflight traversal. The GREEN adapter zeroes a local candidate, accumulates through the checked visitor and copies the entire candidate to output only after complete success; emitted candidate words from a later refusal never reach caller output. A 4096-word candidate costs 32 KiB for word storage plus count/padding, and that automatic-storage cost must remain explicit.

The adapter performs no observer allocation, Unity assertion, reset, fault scheduling or Core call. The pure child checks graph preservation and actual failed/ignored flags; it does not link an allocator ledger and therefore cannot demonstrate armed-ledger purity. Actual COFF review separates six owned GREEN functions from seven emitted SDK inline COMDAT wrappers and confines the sole formatting import to the SDK wrapper graph. That import admission is graph evidence, with no additional execution-purity credit.

## Shared visitor and legacy adapter

`ZrTests_ExecOwner_VisitContents` has a trusted internal emitter/context and a literal checkedGeometry 0 or 1 precondition. The checked route validates pool shapes and the 4096-word bound before payload reads. Its emitter may already have received words when later geometry refuses, so the visitor alone is not a transactional public-output API.

The legacy fixture's asserting Capture resets only output count and calls this shared visitor with checkedGeometry 0. Its original word writer retains the Unity bound assertion and append order. That route bypasses the added shape/budget guards and preserves original readable-graph preconditions; it does not use TryCapture or gain new graceful refusal semantics. Earlier failure/ignore abort behavior of the asserting writer remains part of the separate legacy-consumer review.

Logical order is module id/token/hash, twelve execution-contract fields and four pool counts, then constants, layouts, source maps and functions. Function order includes scalar/contract/count fields; values, instructions and blocks; numeric pools; phi/deopt/source/binding rows; then GC map, state map and frame layout presence and payload. Logical snapshots exclude storage addresses and capacities. Two independent one-constant graphs with capacities 1/3 must yield the same 22 words; changing one logical constant value must change its corresponding word.

## Authored matrix and cleanup

The authored main uses no constructor or verifier. It fills real typed module/function/pool objects with independently listed expected facts: empty/scalar 19 words, module pools 35, ordinary function plus module 182, and module pools plus the full ordinary function and all optional nested maps 255. Exact budget 4096 and over budget 4099 use real constant pools. Structural refusals cover 29 pool shapes twice and two GC object/count inconsistencies, sixty calls in one case.

Every successful CRT allocation is registered immediately. Ordinary cases release all registered roots before supporting or feature assertions. Case 11 deliberately fails Unity, then nonasserting teardown calls TryCapture under the failed flag, saves its actual result/output and frees the real root without resetting Unity flags. Case 12 independently verifies that saved witness after Unity conclusion; it makes no new capture call or allocation. Final drain frees residual registered roots before UNITY_END. Counts show numerical registered-root cleanup; complete OOM/leak instrumentation remains OPEN.

## Descriptor changes and limits

Target `zr_vm_ssa_owner_try_capture_test` selects exactly main, new captureC and actual UnityC. Actual Common/Core headers are available, with normal static/debug/Windows frontend definitions; no Core library, allocatorC, fixture lifecycle provider, Source/VM bootstrap, forced include, archive or fake provider is linked.

Each of the five existing fixture-consumer descriptors received exactly one captureC source line beside its fixtureC entry. Separate actual regression validates lower owned-module and module-copy consumers, with CoreModel support. Function-owner, finalized-module and returned-execir-retention still require their genuine integration gates. The pure three-unit child itself gives these consumers no link, runtime, sole-provider or combined armed-ledger credit. Genuine finalizer integration, Source's twenty cases, opaque owner/VMFunction identity, actual ignored-path execution, full47, normal full-project defaults, Linux, full MSVC runtime and network/FFI remain OPEN.
