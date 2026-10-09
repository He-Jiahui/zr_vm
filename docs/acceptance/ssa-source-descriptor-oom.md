---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/support/ssa_source_descriptor_oom_shim.h
  - tests/parser/support/ssa_literal_script_fixture.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
plan_sources:
  - .codex/plans/20261009-ssa-descriptor-oom-source-tests-design-v2.md
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
tests:
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/support/ssa_source_descriptor_oom_shim.h
  - tests/acceptance/ssa-source-descriptor-oom.md
doc_type: milestone-detail
status: source-descriptor-oom-functional-red-established
---

# Descriptor OOM finite acceptance

## Current decision

**Functional RED established.** Actual sixteen-case default execution has four PASS, twelve diagnostic FAIL and zero ignores. Both real9/8 allocation prerequisites pass. Production allocation faults report INVALID_ARGUMENT1 instead of required OUT_OF_MEMORY15, after the candidate cleanup and old/source preservation checks pass. GREEN remains pending.

This is retention support task B. It does not close task A, returned-source retention, arbitrary descriptor support or any full SSA leaf. The [module documentation](../parser-and-semantics/ssa-source-descriptor-oom.md) describes current ownership and diagnostics; the [test protocol](../../tests/acceptance/ssa-source-descriptor-oom.md) names all sixteen cases.

## Frozen source evidence

| Input | SHA256 |
| --- | --- |
| Actual unchanged descriptor C | `A2D3867B562DC71BF7F3B755FB8BD6746C760A19CEDB98A2E4EC829370D99BC2` |
| Actual test C | `735303ACBDCB32EC83C4B95B32CB8EB54D34F5806CFC5E5686D978D7E401DE39` |
| Actual private shim H | `97301B28D29663665C645EAE9FB93D049ECED981A135F252AE63F916DACD370B` |

Root's historical MSVC receipt is `C3EC86DBF58AD81F1E4D83CCDE2308B5D6E8E9BC0D81144A741BD2D0F59630A8`, covering only module/test/literal-fixture compilation: three units with zero warnings. It does not establish fresh link identity, execution outcomes, allocation balance or sanitizer acceptance.

## Gates and required receipts

| Gate | Current state | Acceptance evidence required |
| --- | --- | --- |
| Source/test freeze | recorded | matching hashes, exact sixteen RUN_TEST calls, current dependency/settings pins |
| Actual 9/8 lower prerequisites | passed in actual RED r1 | real compiler, binder, compaction, primitive frame, canonical AOT projection and host target; six exact successful allocations; ValidateModule/RequireABI; strict teardown |
| Functional RED | actual sixteen cases, twelve diagnostic failures, zero ignores | current actual descriptor OBJECT linked; real injected nonzero allocation returns false/INVALID_ARGUMENT and fails required OUT_OF_MEMORY assertion; every actual outcome recorded |
| Finite RED commit | pending | Root-reviewed raw evidence and actual commit receipt |
| Production diagnostic repair | pending separate source GO | checked private products, exact OOM bytes, preserved candidate cleanup and old owner |
| Fresh GREEN | pending | sixteen tests, zero failures/ignores, real exit, zero strict ledger errors/live records, required current warning/sanitizer receipts |
| Finite GREEN commit | pending | reviewed exact-path diff, source/header/settings/link pins and actual commit receipt |

Missing symbols, incorrect fixtures, unsupported host target, ledger errors, source drift or sanitizer failures are infrastructure failures. They do not establish the intended functional RED. Actual cases1/2 pass; cases3-14 fail exactly at the required status assertion; cases15/16 pass. No ledger or sanitizer error occurs. The twelve failed cases stop before retry; candidate cleanup and preservation were already checked, while only optional-diagnostic case16 completes its fault/retry path in RED.

## Actual RED r1 evidence

Configure22872/build4900 exit0; CTest7108 exits8 as expected for functional RED.
The selected incremental build has0 warnings. Current finite MSVC module33756,
test33200 and literal-fixture22580 each exit0 with0 warnings. The actual map
selects BuildDescriptor and FreeDescriptor once from the forced-shim OBJECT,
with no Oracle/network/FFI member. Source/header/settings/graph/binary/map pins
remain unchanged across the run.

Reports are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`:

| Evidence | SHA256 |
| --- | --- |
| source-descriptor-oom-red-r1-receipt.json | 8A87D49620840D4F1BC0ECBC71C164DF8D95FE6C4E23C4927337B05BA7245AC6 |
| source-descriptor-oom-red-r1-ctest.log | E6331347C08E310AEEB86063F15E3EF7AC87F920BCFF5F070B6BCFC690281913 |
| source-descriptor-oom-msvc-red-r2-receipt.json | FD3A27DF94197D1B1BD36977710B33733A0B3A8AD11F9A4A217CEBE5AE4DBCEE |

Sol independently reviewed all six allocation sites, both failure exits,
candidate cleanup, old destination publication and the actual frozen tests.
The arithmetic repair remains a separate source GO after this RED commit.

## Allocation and ownership denominator

The actual finite profile expects three instructions, one block, one frame slot, three source maps, one constant and one layout. Edge and phi counts are zero. Six nonzero `calloc` sites are checked in both first attachment and old-9/new-8 replacement; twelve primary fault cases cover this denominator. Null argument refusal and optional diagnostic cleanup complete the sixteen cases.

At fault site `k`, require exactly `k` candidate attempts and `k-1` successful spans freed once before assertions. Initial attachment preserves the zeroed destination. Replacement preserves the entire old descriptor, its six live spans, old projection ownership and both source chains. The old descriptor remains valid and retains literal 9 while the candidate literal 8 construction fails.

Disarmed retry must succeed, make six actual requests, validate the module and noargs/I64 ABI, and preserve source observations. For replacement, all six new allocations precede old frees; old records are released once and the published descriptor borrows the actual second projection. Explicit final release zeros the descriptor and balances its ledger; a second release adds no record frees. Fallback teardown is recovery, never proof of production candidate cleanup.

## Diagnostic denominator

All seven fields are compared individually after nonzero-byte seeding. OOM requires status OUT_OF_MEMORY, `expected` equal to the failed attempt's admitted `count * width`, and `actual`, `functionId`, `blockId`, `instructionId`, `index` all zero. Required NULL arguments remain INVALID_ARGUMENT with zero payload. Success is OK with all fields zero. A NULL diagnostic does not change cleanup or preservation.

Private arithmetic refusal is distinct from genuine OOM. Product guards are required by source review; this finite small x64 profile provides no forged overflow/32-bit evidence.

## Provenance and route

Root must verify `BuildDescriptor` and `FreeDescriptor` resolve from the single instrumented actual descriptor OBJECT. Only that TU has forced `calloc/free` redirection. Link-selected unused Oracle support from the literal fixture must be disclosed without claiming it executed. Ordinary Parser/Core/Common/Unity and concrete existing compile-wrapper dependencies remain the actual static graph.

Record current compiler/platform/assertion/forced-include/settings, actual checkout source paths, compiled header resolution, link map and archive selection, hashes before and after runs, raw counts, ignores, exits, warnings and sanitizer output. Build outputs stay under `E:/cargo-targets/zr_vm/{build,cache,reports,tests,tmp}`. Source snapshots, copies, worktrees and installation are outside the gate.

No source/Oracle execution, generated-native program, network, FFI, provider or security action is included. Current Win64 host metadata validation earns no Linux/general ABI/backend/native/platform/performance credit. Returned ownership/retention and all forty-seven full SSA leaves remain open.
