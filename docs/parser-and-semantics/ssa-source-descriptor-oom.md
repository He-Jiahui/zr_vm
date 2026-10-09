---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/support/ssa_source_descriptor_oom_shim.h
  - tests/parser/support/ssa_literal_script_fixture.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
plan_sources:
  - .codex/plans/20261009-ssa-descriptor-oom-source-tests-design-v2.md
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
tests:
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/support/ssa_source_descriptor_oom_shim.h
  - tests/acceptance/ssa-source-descriptor-oom.md
doc_type: module-detail
status: source-descriptor-oom-finite-green
---

# Source descriptor allocation diagnostics

## Purpose and current state

This finite support gate checks descriptor construction from actual literal SCRIPT sources containing `return 9;` and `return 8;`. It requires an allocation failure to report the requested byte size, preserve the existing descriptor and live source, and release every successful partial candidate allocation.

The private allocation helper now checks the representable product before `calloc` and publishes its storage slot only on success. A genuine NULL result reports `ZR_AOT_IR_OUT_OF_MEMORY` with the exact admitted byte count. Both failure exits preserve that diagnostic, candidate cleanup and the old descriptor. Actual GREEN executes all sixteen default cases with zero failures or ignores.

The earlier actual RED had four PASS and twelve diagnostic FAIL, each expected OUT_OF_MEMORY15 but actual INVALID_ARGUMENT1 after cleanup and preservation checks. Its tests and registration are committed as `d3e8268b1eb22554c6753ce2448d31080c3dc454`.

## Interface and caller requirements

`ZrParser_AotIrProjection_BuildDescriptor` accepts a live projection, target contract, module contract, initialized descriptor destination and optional diagnostic. It returns a boolean. `ZrParser_AotIrProjection_FreeDescriptor` releases the descriptor's owned spans and zeros the whole descriptor; NULL is permitted.

The caller keeps projection and contract owners live, supplies independently readable storage, prevents overlap with writable destination/diagnostic storage, and serializes mutation and destruction. Count and product checks establish bounded arithmetic; they do not establish arbitrary pointer readability. This gate does not broaden projection admission or change the public interface.

## Ownership and publication

Construction initializes a local candidate. The candidate owns converted instructions, blocks, combined edge storage, phi incoming rows, frame slots, source maps, constants and layouts. It borrows the projection through `owner`, and its function borrows operand, result, value-slot, memory-token, logical-state-map, GC-root and deoptimization views.

Existing edge range checks run before candidate allocation. The descriptor's allocation chain short-circuits at the first failure. Later chain failures release the candidate with `FreeDescriptor`; edge allocation failure occurs before any other successful candidate allocation. Neither path frees the old destination descriptor.

After conversion and copying, actual Core module validation must succeed before the old descriptor is released. Successful assignment then rebinds `module.functions` to the destination's own `function` member. Failed construction must preserve the old descriptor header and contents while its projection remains live.

The required destruction order is descriptor, projections, compacted graphs, fixture module/compiler/AST owners, then actual Global. A descriptor is not an independently retained Core arena. Freeing a projection before its descriptor is outside this gate's contract.

## Actual source producer chain

The frozen tests use the existing lower literal fixture, actual Global/State construction and current compiler metadata. They prepare the literal, bind the source module contract, verify the original graph, resolve its canonical callable and I64 return, eliminate the dead source place, construct a host primitive layout and frame, verify the compacted graph, lower with the canonical callable, and construct the host noargs/I64 AOT target.

The compacted profile retains three instructions: CONSTANT, NOP and RETURN. Source maps and the actual present-empty logical state map remain visible. It has one block, one frame slot, three source maps, one constant and one layout. Successor, predecessor and phi incoming counts are zero. Validation and `RequireExecutableAbi` inspect metadata; neither executes a source program or generated native code.

Fixture preparation allocates unused Oracle support. The test does not invoke its Oracle helpers or SourceExecute. Constants are compared across actual compiler/runtime/module/projection rows and against the independently known safe literal inputs; the descriptor implementation does not import fixture expectations.

## Six nonzero allocation sites

The production chain contains eight possible spans. Edge and phi counts are zero in this finite profile, so only these six calls reach `calloc`:

| Ordinal | Destination | Count | Required OOM `expected` byte payload |
| --- | --- | ---: | --- |
| 1 | instructions | 3 | `3 * sizeof(SZrAotIrInstruction)` |
| 2 | blocks | 1 | `sizeof(SZrAotIrBlock)` |
| 3 | frame slots | 1 | `sizeof(SZrAotIrFrameSlot)` |
| 4 | source maps | 3 | `3 * sizeof(SZrAotIrSourceMap)` |
| 5 | constants | 1 | `sizeof(SZrExecIrConstant)` |
| 6 | layouts | 1 | `sizeof(SZrExecIrLayout)` |

The frozen wrappers check each actual attempt's count, width, ordinal and product. A successful prerequisite must establish this sequence before fault results count as functional RED or GREEN. The six-site inventory describes this scalar profile; nonempty edges and phi storage are not covered.

## Required diagnostic behavior

Tests seed diagnostics with nonzero bytes and compare all seven fields individually: `status`, `functionId`, `blockId`, `instructionId`, `index`, `expected` and `actual`.

| Situation | Required result | Payload |
| --- | --- | --- |
| Required NULL argument | false / INVALID_ARGUMENT | all numeric fields zero |
| Nonzero `calloc` returns NULL | false / OUT_OF_MEMORY | `expected` equals the observed request bytes; `actual` and all four identity/index fields zero |
| Successful construction/retry | true / OK | every field zero |
| Optional diagnostic is NULL | same ownership/cleanup result | no diagnostic access |

The private helper returns immediately for count zero without allocating. For a nonzero count it rejects zero width, then compares count as `uintmax_t` against `SIZE_MAX / width` before narrowing or multiplication. Arithmetic refusal is INVALID_RANGE with the maximum admitted count and supplied count; zero width has expected1/actual0. The failure helper clears the entire diagnostic before writing status/expected/actual. These arithmetic branches are source-reviewed, not exercised by forged oversized arrays in this gate. Cleanup preserves the precise helper status.

## Independent allocator observations

Only the actual descriptor translation unit receives the private forced-include shim that redirects `calloc` and `free`. The test translation unit disables redirection and its wrappers call the real CRT functions. No public allocator seam is added. Root must prove the descriptor symbols resolve from that actual instrumented OBJECT and that a duplicate archive member is not extracted.

The actual frozen descriptor ledger has 256 record slots and 256 attempt events; the VM ledger has 8192 record slots. The design's earlier 64-record recommendation is not the frozen implementation's capacity. Descriptor records store live pointers, count/width/bytes, site ordinal and numeric allocation/free sequence. Only one call-relative site is armed, and injection is disarmed before assertions, retry or cleanup.

Descriptor wrappers defer errors and perform no Unity assertions, output or allocating bookkeeping. They clear a live address before freeing it. The separate VM allocator checks original size and flag, clears the old live address before realloc/free, and restores it only on a genuine failed realloc. Descriptor fault injection never faults VM or fixture allocations. No ledger is reset with live allocations.

For failed site `k`, the test expects exactly `k` attempts, one injected failure, `k-1` successful candidate records, and those records freed once by the production builder before assertions. Live allocations return to the pre-call baseline. Replacement keeps all six old records live and untouched; initial attachment keeps an all-zero destination.

Retry makes six further successful requests. Successful replacement allocates all six candidate spans before freeing any old span, changes the owner to the actual second projection, and preserves both producers. Freed old addresses are never read or compared; only pointer-free witnesses and record IDs remain valid observations.

## Frozen inputs and actual RED evidence

| Input | SHA256 |
| --- | --- |
| GREEN descriptor implementation C292 | `2866981C237884057E8153257094F9FD1B6C4FB1B279574A7F8D7E6D6FA75F7B` |
| Dedicated test | `735303ACBDCB32EC83C4B95B32CB8EB54D34F5806CFC5E5686D978D7E401DE39` |
| Private shim | `97301B28D29663665C645EAE9FB93D049ECED981A135F252AE63F916DACD370B` |

Root supplied historical finite MSVC receipt `C3EC86DBF58AD81F1E4D83CCDE2308B5D6E8E9BC0D81144A741BD2D0F59630A8`: descriptor module, dedicated test and literal fixture, three units, zero warnings. It is not a current runtime, link, sanitizer or sixteen-case acceptance receipt.

Fresh GREEN configure/build/CTest exit0; sixteen cases PASS, zero failures/ignores, no strict ledger or UBSan error and zero incremental warnings. Both descriptor symbols resolve once from the actual instrumented OBJECT; no Oracle/network/FFI symbol is selected. Current finite MSVC module/test/literal-fixture three units exit0 with zero warnings. All twelve six-site initial/replacement faults now complete exact OOM payload, cleanup, preservation, disarmed retry and explicit release checks. The optional-NULL-diagnostic case also completes cleanup/retry.

All build/cache/report/test/tmp outputs belong under `E:/cargo-targets/zr_vm`. See [acceptance state](../acceptance/ssa-source-descriptor-oom.md) for actual receipt hashes and [test protocol](../../tests/acceptance/ssa-source-descriptor-oom.md) for the complete inventory.

Returned-source retention, arbitrary descriptor graphs, backend execution/parity, general ABI/platform/performance coverage and all forty-seven full SSA leaves remain open.
