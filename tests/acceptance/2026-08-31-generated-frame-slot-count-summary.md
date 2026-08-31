---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
tests:
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/parser/test_compiler_w2_performance_quickening.c
plan_sources:
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
  - docs/plans/benchmark/optimize/05-execution-roadmap.md
doc_type: testing-guide
---

# Generated Frame-Slot Count Summary Acceptance

## Scope

This record accepts the generated frame-slot count summary selected from the
post-copy-probe `mixed_service_loop` ranking. It replaces a complete instruction
stream scan at every VM pre-call with an immutable function summary, without
changing the scan rules or trusting serialized derived state.

## Runtime Contract

- `generatedFrameSlotCountPlusOne` reserves zero for dynamic scan fallback.
  Unfinalized and hand-built functions therefore continue to reflect current
  `stackSize`, instructions, operands, and opcodes.
- `ZrCore_Function_FinalizeDirectFrameValueSlots` clears the summary, scans the
  complete current instruction stream, and publishes `count + 1` only when the
  result is representable. Re-finalization refreshes the summary.
- The field is initialized and tombstoned with `SZrFunction`, is append-only at
  the public structure tail, and is not part of the `.zro` schema.
- The loader rebuilds the summary only after instructions and child functions
  have been copied. Compiler layout construction publishes it even for a
  zero-stack function, and quickening republishes it after all instruction
  rewrites and constant-function rebinding.
- A missing summary retains the original full scan. The scan implementation and
  opcode accounting remain the single source of truth.

## Test-First Evidence

The focused precall contract was added before the implementation. Its first
MSVC run passed the preceding 16 tests and failed the new case with
`Expected 33 Was 2`: changing `instructionsLength` after finalization still
changed the public count. After adding the summary, the case proves all three
states: an unfinalized function tracks instruction mutation, finalization makes
the result immutable, and re-finalization refreshes it.

The loader test now copies a `GET_GLOBAL` instruction whose destination requires
33 slots and asserts that the runtime function publishes `34` while the public
getter returns `33`. The quickening test clears its published summary, rescans
the rewritten stream, and requires both results to match.

## Deterministic Performance

Both sides use the same GCC 11.4 Release ext4 build, CPU 2 affinity, Valgrind
3.18.1 Callgrind, scale 1, and the accepted copy-probe projects. Checksums are
unchanged.

| Case | Before Ir | After Ir | Change | Checksum |
|---|---:|---:|---:|---:|
| `mixed_service_loop` | 325,175,994 | 314,490,481 | -3.286% | 408940136 |
| `numeric_loops` | 111,767,607 | 111,742,361 | -0.023% | 48943705 |
| `object_field_hot` | 110,881,829 | 110,868,508 | -0.012% | 623146080 |

The target saves `10,685,513 Ir` and passes the plan's `3%` deterministic slice
gate. Both representatives improve and remain inside the `1%` regression gate.
Relative to the original mixed-service baseline `868,860,510 Ir`, the retained
result saves `554,370,029 Ir` (`-63.80%`).

The public generated-slot getter falls from `10,624,761` to `163,904 Ir`
(`-98.46%`) across 20,488 hot calls. The once-per-function scan accounts for
`17,347 Ir`. The inclusive frame-storage count path falls from `10,810,376` to
`554,399 Ir` (`-94.87%`).

Appending the four-byte summary grows `libzr_vm_core.so` from `2,657,032` to
`2,661,192` bytes (`+4,160`, `+0.157%`). Because the field belongs to the public
function header, the GCC target rebuild spans 644 steps and takes `10:52.47`
at `-j2`, with peak RSS `1,277,960 KiB`. This dependency cost is accepted with
the runtime gain.

Raw artifacts are retained at:

- `/home/hejiahui/.cache/codex/callgrind.mixed-copy-probe.final.out`
- `/home/hejiahui/.cache/codex/callgrind.mixed-generated-slot-summary.after.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-copy-probe.final.out`
- `/home/hejiahui/.cache/codex/callgrind.numeric-generated-slot-summary.after.out`
- `/home/hejiahui/.cache/codex/callgrind.object-copy-probe.final.out`
- `/home/hejiahui/.cache/codex/callgrind.object-generated-slot-summary.after.out`

## Validation Matrix

| Toolchain | frame | precall | postcall | tail | VM closure | type layout | type metadata | member | shape | GC | relocation | numeric | quickening |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| GCC 11.4 Release | 28/28 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| Clang 14 Debug | 28/28 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| Clang 14 ASan/UBSan/LSan | 28/28 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |
| MSVC 19.44 Debug, fresh | 28/28 | 17/17 | 3/3 | 4/4 | 6/6 | 40/40 | 9/9 | 102/102 | 3/3 | 67/67 | 20/20 | 11/11 | 20/20 |

The sanitizer matrix uses `setarch x86_64 -R`, leak detection, and halt-on-error
for ASan and UBSan. All binaries exit zero with no sanitizer report. The Clang
build directories were repaired after an orchestration-level concurrent-Ninja
interruption; `ninja -t recompact` recovered each log, the single-Ninja rebuilds
completed, and a final target rerun reported `no work to do`. The MSVC result
comes from a newly configured 744-step Debug target build.

## Decision

The immutable generated frame-slot count summary is accepted for correctness,
deterministic instruction reduction, and sanitizer coverage. Calibrated paired
wall time and the remaining call/return specialization gates stay open.
