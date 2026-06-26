# AOT 08-S2 Generic Constraints

Date: 2026-06-24 10:28:48 +08:00

## Scope

This slice validates compile-time generic constraint solving for the 08 generic-sharing plan.

Existing type inference already covered:

- named constraints, including source interface implementation checks;
- class and struct constraints;
- owner constraints;
- exact `unique` / `shared` / `weak` ownership constraints;
- native generic constraint mismatch diagnostics.

This slice adds the missing focused `new()` constraint acceptance at the type inference layer.

## Implementation

- Added `tests/parser/test_generic_constraints.c`.
- Registered `zr_vm_generic_constraints_test`.
- Registered CTest suite `generic_constraints`.

The focused test compiles:

```zr
interface AbstractThing { }
class DefaultCtor { }
class NeedNew<T> where T: new() { var value: T; }
new NeedNew<DefaultCtor>();
new NeedNew<AbstractThing>();
```

Expected behavior:

- `DefaultCtor` satisfies `new()` and produces `NeedNew<DefaultCtor>`.
- `AbstractThing` fails with a `new() constraint` diagnostic.

## Tooling Evidence

Initial target check:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_generic_constraints_test -j 2'
```

Result: the new target was not available before reconfiguring CMake.

GREEN:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc && cmake --build build-wsl-gcc --target zr_vm_generic_constraints_test -j 2 && build-wsl-gcc/bin/zr_vm_generic_constraints_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm/build-wsl-gcc && ctest -N -R generic_constraints && ctest -R generic_constraints --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_parser_test -j 2 && build-wsl-gcc/bin/zr_vm_parser_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_type_inference_test -j 2 && build-wsl-gcc/bin/zr_vm_type_inference_test'
```

Results:

- `zr_vm_generic_constraints_test`: 1 test, 0 failures.
- CTest `generic_constraints`: 1 test, passed.
- `zr_vm_parser_test`: 75 tests, 0 failures.
- `zr_vm_type_inference_test`: 118 tests, 0 failures.

## Acceptance Decision

Accepted as completed 08-S2.

This slice does not modify AOT code generation. The next generic-sharing work item is 08-S3: using the collected instantiation and constraint results to generate monomorphized value/const generic C.
