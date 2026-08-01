---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
plan_sources:
  - user: 2026-08-01 complete Syntax 05 M5 Task4 parser property/import support
  - docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration-implementation-plan.md
tests:
  - tests/parser/test_property_consumer_contracts.c
  - tests/parser/test_property_consumer_runtime_bootstrap_cases.h
doc_type: testing-guide
---

# Syntax 05 M5 Task4 Property Import Bootstrap

## Scope

- Parser/runtime-prototype import metadata only.
- Public `ZrParser_TypeInference_RegisterRuntimePrototypes` input and no-data behavior.
- Source and reloaded `.zro` property carrier parity through an empty imported placeholder.
- Structured property identity, accessor role, canonical TypeId, and reference-access projection.
- Debug and LSP consumers are excluded from this acceptance.

## Baseline

The structured compiled-row merge was already present from `4bdaad6`, and the public function was
exported from `3d67352`, but no parser test called the public API directly. On frozen
`c09091b + parser test overlay`, GCC 11.4 produced a real `exit 1`: 10 existing cases passed and
the new bootstrap case failed because a null compiler incorrectly returned success. Final focused
validation refreshed the same exact overlay onto `HEAD 1c50bad` after concurrent AOT commits.

## Test Inventory

- Invalid null compiler and null function calls fail closed.
- A valid function with no prototype rows is a successful no-op.
- A source-compiled `ref readonly` property merges into one pre-existing empty imported placeholder.
- The same property is written to `.zro`, reloaded, and registered through the same public API.
- Visible property and getter retain the same structured `propertyIdentity`; PropertyQuery exposes
  canonical TypeId, readonly reference access, and getter SymbolId.
- An ordinary method named `__get_fake` remains an ordinary method with no property identity.
- Corrupting the real getter's `propertyIdentity` leaves the property query unavailable; the
  importer does not recover it from a hidden accessor name.

## Tooling Evidence

The final source snapshot was `HEAD 1c50bad` plus these exact parser overlays; the RED evidence above
was captured before the HEAD refresh on `c09091b`:

- `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c`
- `tests/parser/test_property_consumer_contracts.c`
- `tests/parser/test_property_consumer_runtime_bootstrap_cases.h`

GCC and Clang used the same ext4 source bytes:

```sh
cmake -S /home/hejiahui/zr_vm-s05m5-task4-red \
  -B /home/hejiahui/build-s05m5-task4-red-gcc \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc
cmake --build /home/hejiahui/build-s05m5-task4-red-gcc \
  --target zr_vm_property_consumer_contracts_test -j 4
./bin/zr_vm_property_consumer_contracts_test

cmake -S /home/hejiahui/zr_vm-s05m5-task4-red \
  -B /home/hejiahui/build-s05m5-task4-green-clang \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
cmake --build /home/hejiahui/build-s05m5-task4-green-clang \
  --target zr_vm_property_consumer_contracts_test -j 4
./bin/zr_vm_property_consumer_contracts_test
```

MSVC used a byte-identical Windows snapshot and the repository VsDevCmd wrapper. The snapshot
directory retained its original RED-baseline suffix, but its tracked bytes were refreshed to
`HEAD 1c50bad` before this final run:

```powershell
cmake -S .codex/snapshots/s05m5-task4-green-msvc-c09091b `
  -B .codex/build-s05m5-task4-green-msvc `
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl
cmake --build .codex/build-s05m5-task4-green-msvc `
  --target zr_vm_property_consumer_contracts_test -j 4
.codex/build-s05m5-task4-green-msvc/bin/zr_vm_property_consumer_contracts_test.exe
```

## Results

- GCC 11.4.0: 11 tests, 0 failures, real process `exit 0`.
- Clang 14.0.0: 11 tests, 0 failures, real process `exit 0`.
- MSVC 19.44.35228.0 (toolset 14.44.35207): 11 tests, 0 failures, real process `exit 0`.
- The main test runner stays near the repository size boundary; the cohesive bootstrap fixtures and
  cases live in `test_property_consumer_runtime_bootstrap_cases.h`.

## Acceptance Decision

Accepted. The public bootstrap is directly covered, invalid calls fail closed, source and binary
property contracts converge through structured rows, and missing identity remains unavailable.
No Debug, LSP, generated artifact, shared CMake, or unrelated parser path contributes to this result.
