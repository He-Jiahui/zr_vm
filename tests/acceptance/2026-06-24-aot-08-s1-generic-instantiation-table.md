# AOT 08-S1 Generic Instantiation Table

Date: 2026-06-24 10:18:45 +08:00

## Scope

This slice implements the first generic-sharing foundation only: compile-time generic instantiation records, deduplication, and reference/value share-kind classification.

- Reference-shaped type arguments share one generic body.
- Value-shaped type arguments force monomorphized instances.
- Duplicate `(baseToken, argument signature, resolved shape)` records reuse the existing `cInstanceId`.
- Compiler-resolved type shape can be supplied explicitly so later class/struct metadata integration can distinguish source classes from inline structs.

## Reference Evidence

- HybridCLR `GenericSharing::IsShareable` rejects sharing when any generic argument is not a reference type.
- NativeAOT `GenericDictionaryNode` models concrete generic type/method instantiations and their canonical dictionary layout.
- Mono `MonoGenericInst` is copied/canonicalized through equality/hash caching.
- Roslyn TypeSpec/MethodSpec writers deduplicate generic instantiation signatures structurally.

## Implementation

- Added `zr_vm_parser/include/zr_vm_parser/generic_instantiation.h`.
- Added `zr_vm_parser/src/zr_vm_parser/generic_instantiation.c`.
- Added `tests/parser/test_generic_instantiation.c`.
- Registered `zr_vm_generic_instantiation_test` and CTest suite `generic_instantiation`.

The new table stores:

```c
baseToken
arguments
shareKind
cInstanceId
```

The default API infers reference/value shape from `EZrValueType`; the resolved API accepts `EZrGenericInstantiationTypeShape` so later compiler prototype data can supply source class/struct shape directly.

## Tooling Evidence

RED:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_generic_instantiation_test -j 2'
```

Result: expected build failure because `zr_vm_parser/generic_instantiation.h` did not exist yet.

GREEN:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_generic_instantiation_test -j 2 && build-wsl-gcc/bin/zr_vm_generic_instantiation_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm/build-wsl-gcc && ctest -N -R generic_instantiation && ctest -R generic_instantiation --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_type_inference_test -j 2 && build-wsl-gcc/bin/zr_vm_type_inference_test'
```

Results:

- `zr_vm_generic_instantiation_test`: 3 tests, 0 failures.
- CTest `generic_instantiation`: 1 test, passed.
- `zr_vm_type_inference_test`: 118 tests, 0 failures.

## Acceptance Decision

Accepted as completed 08-S1.

This does not complete the full generic-sharing stage. Remaining 08 work: constraint solving, monomorphized value/const generic C generation, generic dictionary layout/lazy resolution, generic `CALL_TYPED`, dynamic-instance deopt, and full-AOT missing-instance enforcement.
