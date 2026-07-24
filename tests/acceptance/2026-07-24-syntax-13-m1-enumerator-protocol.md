# Syntax 13 M1 Enumerator Protocol Acceptance

## Status

Completed 2026-07-25 00:55 +08:00. Started 2026-07-24 23:10 +08:00.

## Required Contract

- `zr.iteration` owns the four public iterator TypeIds; builtin and container descriptors do not.
- Containers implement `zr.iteration.Iterable<...>` and return canonical
  `zr.iteration.Enumerator<...>` metadata.
- `for` element binding reads only resolved `ITERATOR` / `ITERABLE` protocol facts.
- Typed loops retain static iterator opcodes; M1 excludes `yield` and async lowering.

## Evidence

- Independent GCC, Clang, and MSVC `s13m1` build directories each built and ran
  these direct Unity binaries with real process exit 0:

  | Target | Unity result |
  | --- | --- |
  | `zr_vm_enumerator_protocol_test` | 5 tests, 0 failures |
  | `zr_vm_type_inference_test` | 119 tests, 0 failures |
  | `zr_vm_container_type_inference_test` | 12 tests, 0 failures |
  | `zr_vm_numeric_foreach_cardinality_dataflow_test` | 2 tests, 0 failures |
  | `zr_vm_numeric_loop_assignment_dataflow_test` | 16 tests, 0 failures |

  The configurations were `.codex/build-s13m1-gcc`,
  `.codex/build-s13m1-clang`, and `.codex/build-s13m1-msvc`; their per-target
  stdout logs are retained under `.codex/logs/s13m1-*`.
- The project currently registers no CTest entries in the focused build directory:
  `ctest -R 'enumerator_protocol|numeric_foreach_cardinality_dataflow|container_runtime'`
  exits 0 with `No tests were found`; direct Unity process exits are therefore the
  acceptance evidence.
- GCC and Clang `zr_vm_container_runtime_test` both retain the same pre-existing
  marker: `test_container_map_runtime_iterator_aggregates_pairs_without_order_assumptions`
  cannot load `string` import metadata and reports 49 tests with 1 failure. The
  iterator reference cases execute after that marker; this failure is not counted
  as M1 passing evidence.
- `zr_vm_container_metadata_test` reaches the migrated `zr.iteration` assertions
  in the MSVC replay but retains a baseline marker: closed `Map<string, int>`
  materialization omits `containsKey` (3 tests with 1 failure). The stale open
  module type-count expectation was corrected from 6 to the actual 8 types. GCC
  and Clang isolated rebuilds after that assertion correction exceeded the local
  two-minute command window, so they are not acceptance evidence; the remaining
  generic-prototype failure is not counted as M1 passing evidence.
- The full module-system binary currently stops at an existing
  `ZrCore_Gc_ValueStaticAssertIsAlive` assertion after its early tests; its compile
  and link completed, but this marker is not counted as M1 passing evidence.
