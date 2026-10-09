---
related_code:
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/support/ssa_source_descriptor_oom_shim.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
implementation_files:
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/support/ssa_source_descriptor_oom_shim.h
plan_sources:
  - .codex/plans/20261009-ssa-descriptor-oom-source-tests-design-v2.md
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
tests:
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/support/ssa_literal_script_fixture.c
doc_type: testing-guide
status: source-descriptor-oom-functional-red-established
---

# Descriptor OOM source test protocol

## Current evidence boundary

Actual RED r1 executes all sixteen cases: four PASS, twelve diagnostic FAIL and zero ignores. Both real9/8 prerequisites pass. Each of cases3-14 reports expected OUT_OF_MEMORY15 but actual INVALID_ARGUMENT1 after the bounded attempt/partial-release/old-owner/source checks have passed. Those failures stop before retry; optional-diagnostic case16 completes its cleanup and retry path. No strict ledger or UBSan error occurs.

The frozen test SHA256 is `735303ACBDCB32EC83C4B95B32CB8EB54D34F5806CFC5E5686D978D7E401DE39`; shim SHA256 is `97301B28D29663665C645EAE9FB93D049ECED981A135F252AE63F916DACD370B`; production descriptor SHA256 is `A2D3867B562DC71BF7F3B755FB8BD6746C760A19CEDB98A2E4EC829370D99BC2`.

Historical MSVC receipt `C3EC86DBF58AD81F1E4D83CCDE2308B5D6E8E9BC0D81144A741BD2D0F59630A8` covers module/test/literal fixture only. Fresh finite MSVC r2 again covers those three units, all exit0/0warnings, receipt `FD3A27DF94197D1B1BD36977710B33733A0B3A8AD11F9A4A217CEBE5AE4DBCEE`. Actual RED gate receipt is `8A87D49620840D4F1BC0ECBC71C164DF8D95FE6C4E23C4927337B05BA7245AC6`, raw CTest `E6331347C08E310AEEB86063F15E3EF7AC87F920BCFF5F070B6BCFC690281913`. The selected build has0warnings and the map supplies both descriptor symbols once from the intended private OBJECT, with no Oracle/network/FFI member. See [acceptance state](../../docs/acceptance/ssa-source-descriptor-oom.md).

## Exact default inventory

Default noargs `main` contains exactly sixteen RUN_TEST calls. There are no ignored, WILL_FAIL or inverted-pass cases.

| # | Actual test name | Evidence required |
| --- | --- | --- |
| 1 | `test_actual_nine_descriptor_allocation_prerequisite` | actual literal 9 chain, six requests, contents/ABI/views and explicit balanced release |
| 2 | `test_actual_eight_descriptor_allocation_prerequisite` | actual literal 8 chain with its own real metadata |
| 3 | `test_first_descriptor_instruction_oom_retry` | first attachment, site 1 |
| 4 | `test_first_descriptor_block_oom_retry` | first attachment, site 2 |
| 5 | `test_first_descriptor_frame_slot_oom_retry` | first attachment, site 3 |
| 6 | `test_first_descriptor_source_map_oom_retry` | first attachment, site 4 |
| 7 | `test_first_descriptor_constant_oom_retry` | first attachment, site 5 |
| 8 | `test_first_descriptor_layout_oom_retry` | first attachment, site 6 |
| 9 | `test_replace_descriptor_instruction_oom_retry` | old 9/new 8 replacement, site 1 |
| 10 | `test_replace_descriptor_block_oom_retry` | replacement, site 2 |
| 11 | `test_replace_descriptor_frame_slot_oom_retry` | replacement, site 3 |
| 12 | `test_replace_descriptor_source_map_oom_retry` | replacement, site 4 |
| 13 | `test_replace_descriptor_constant_oom_retry` | replacement, site 5 |
| 14 | `test_replace_descriptor_layout_oom_retry` | replacement, site 6 |
| 15 | `test_null_descriptor_arguments_preserve_existing_owner` | each of four required NULL arguments, zero requests, old owner/source preserved |
| 16 | `test_optional_diagnostic_oom_cleanup_then_retry` | replacement site 6 with NULL diagnostic; partial cleanup and retry |

Cases 1 and 2 must establish the real producer chain and clean lifecycle before failures can be accepted as functional RED. Twelve primary fault cases cover six nonzero sites in two destination states. Case 16 is diagnostic optionality at site 6; it is not a seventh allocation site.

## Actual producer admission

`prepare` owns real Global/State and the existing literal fixture. It binds the module contract, verifies the original graph, resolves the canonical callable/I64 type, eliminates dead source places, verifies CONSTANT/NOP/RETURN, constructs a primitive layout/frame, verifies again, lowers with the actual canonical callable and obtains the current supported host target.

It checks projection geometry `3/1/1/3/1/1` for instructions/blocks/frame slots/maps/constants/layouts, zero successor/predecessor/phi incoming counts and a present-empty state map. Compiler runtime pool, module/projection constant rows and actual integer AST records witness 9/8. No expected-return field is an input to production construction. The producer stays live until descriptor destruction.

## Actual allocation sequence

| Nonzero ordinal | Count | Width |
| --- | ---: | --- |
| 1 | 3 | `sizeof(SZrAotIrInstruction)` |
| 2 | 1 | `sizeof(SZrAotIrBlock)` |
| 3 | 1 | `sizeof(SZrAotIrFrameSlot)` |
| 4 | 3 | `sizeof(SZrAotIrSourceMap)` |
| 5 | 1 | `sizeof(SZrExecIrConstant)` |
| 6 | 1 | `sizeof(SZrExecIrLayout)` |

Edge allocation is earlier in the production chain and phi allocation lies between blocks and frame slots. Both skip `calloc` at count zero. Each event records the observed count, width, bytes and call-relative ordinal. A geometry or sequence discrepancy is an infrastructure issue requiring a reviewed inventory update.

## Ledger protocol

The actual shim includes real CRT and descriptor declarations first, redirects only `calloc/free` in the actual descriptor OBJECT, and lets the test TU disable redirection. Root proves that OBJECT supplies both descriptor symbols and that a duplicate production archive member is absent.

The descriptor ledger has 256 records and 256 events. The VM ledger has 8192 records; the design's proposed 64 descriptor records are historical design capacity rather than actual frozen code. Descriptor wrappers allocate no bookkeeping, assert nothing and print nothing. Unknown frees, capacity exhaustion, incorrect geometry/site sequence and unexpected CRT failure become deferred errors. A live pointer is cleared before real free; numeric record IDs and sequence counters remain usable after release.

The VM allocator independently checks exact original byte size and memory flag. It clears its old address before realloc/free, restores only on failed realloc, records a successful returned pointer without rereading an invalidated old pointer, and defers errors. Descriptor injection cannot fault VM construction or producer allocation. `setUp` requires both live counts zero before clearing ledgers.

## Per-fault observations

For site `k`, save attempt/success/free/live baselines and live source observations, arm `baselineAttempts + k`, call the actual builder once, and disarm before assertions. Require false return, exactly `k` attempts, one injected failure, exactly `k-1` new successful records and each freed once before assertion. Live count returns to baseline and errors remain zero.

First attachment preserves the zeroed descriptor. Replacement saves a whole live header and span digest, keeps six old records untouched, validates old literal 9 after failure, and preserves both real sources. Source observations include compiler/runtime/AST/module/compacted graph/projection/target and bounded relevant pools while they are live.

After a nonzero diagnostic seed, compare `status`, `functionId`, `blockId`, `instructionId`, `index`, `expected`, `actual` separately. Required OOM is OUT_OF_MEMORY with observed failed request bytes in `expected`; all other fields are zero. NULL arguments require INVALID_ARGUMENT/zero payload and no allocator hit. Successful retry requires OK/all zero. Case 16 passes NULL diagnostic on failure and a seeded diagnostic on retry.

The disarmed retry makes six further calls, validates actual module and noargs/I64 ABI, and checks constant/layout/instruction/block/frame/source-map contents plus borrowed views. Replacement allocates all six new records before any old release, changes descriptor owner to actual projection 8 and rebinds `module.functions` to its own function. Pointer-free semantic witnesses and record IDs permit later comparison; old descriptor pointers are never inspected after successful replacement.

## Explicit cleanup and recovery

`release` frees the current descriptor, compares it with a zeroed shell, checks all successful records freed once and ledger balance/errors, then calls FreeDescriptor again and requires no extra record releases. It frees projections, compacted functions and fixtures, restores saved GC state and destroys actual Global; only independent VM counters are inspected afterward.

`tearDown` also has a bounded recovery path for assertions that interrupt a case. Recovery is not credited as production partial-candidate cleanup. Accepted fault cases must establish partial releases, old owner preservation, retry and explicit balance before fallback teardown.

## Root validation route and limits

Target `zr_vm_ssa_source_descriptor_oom_test` and CTest `ssa_source_descriptor_oom` are the intended finite route: actual exit, timeout 120, assertions/static C11, current compiler/platform definitions, UBSAN halt/stack settings, four PROFILE variables zero, and OUT/trace unset. Root supplies concrete current registration, link graph, source/header/settings pins and raw test receipts.

All artifacts stay under `E:/cargo-targets/zr_vm/{build,cache,reports,tests,tmp}`; actual checkout sources stay at `E:/Git/zr_vm`. No source snapshot/copy/worktree or install route is used. Fixture-linked unused Oracle support may be present but is not executed. No SourceExecute, generated-native program, network, FFI, provider or security operation occurs.

This gate covers current supported host metadata and six small nonzero products. It does not test nonempty edges/phi, forged overflow, 32-bit geometry, independent descriptor retention, returned source ownership, backend parity, general platform/performance or full SSA47.
