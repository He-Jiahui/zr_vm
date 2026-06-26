# AOT 08-S3 Generic Monomorphization

Date: 2026-06-24 11:38:33 +08:00

## Scope

This slice validates the first value-generic AOT monomorphization path.

Covered:

- A closed source generic struct instance, `Pair<int,int>`.
- Closed inline struct layout emission as generated C `ZrLayout_<id>` declarations.
- A generated monomorphization table marker and specialized wrapper symbol.
- A generated shared library returning the same result as the interpreter-facing expectation.

Not covered:

- Reference-type generic sharing dictionaries.
- Generic `CALL_TYPED` monomorphized/shared dual routing.
- Dynamic-instance deopt for uncollected generic instantiations.
- Full-AOT missing-instance diagnostics.

## Implementation

- Added `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.h`.
- Added `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.c`.
- Registered the monomorphization writer from `backend_aot_c_emitter.c`.
- Added `tests/parser/test_aot_c_generic_monomorphization.c`.
- Registered `zr_vm_aot_c_generic_monomorphization_test` and CTest suite `aot_c_generic_monomorphization`.

Supporting fixes:

- Closed generic prototype inference now finalizes concrete inline layouts for primitive type arguments.
- Typed metadata lookup now prefers an exact closed generic layout name before falling back to an open generic base.
- Function layout resolution can conservatively resolve an open generic layout id to the unique closed instance layout in the same prototype table.
- Inline constructor receiver copy-back now checks resolved layout compatibility rather than requiring identical open/closed layout ids.
- Signed scalar AOT arithmetic writes back scalar locals before value-slot synchronization, so generated direct returns can observe scalar results.

## Tooling Evidence

RED:

- Initial 08-S3 test failed because generated C did not contain the monomorphization marker, closed generic type name, specialized wrapper, or closed layout declaration.
- After marker/layout emission, generated AOT failed the inline struct typed call path.
- After closed layout lookup, execution failed with `COPY_STACK: missing inline layout`.
- After generic layout fallback, execution completed but returned `0` instead of expected `81`, proving the constructor receiver was not copied back across open/closed layout ids.

GREEN:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_monomorphization_test -j 2 && timeout 120s build-wsl-gcc/bin/zr_vm_aot_c_generic_monomorphization_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "aot_c_generic_monomorphization|aot_c_source_contracts" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 120s build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "generic_instantiation|generic_constraints|type_inference|parser" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 120s build-wsl-gcc/bin/zr_vm_type_inference_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 120s build-wsl-gcc/bin/zr_vm_parser_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 120s build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 120s build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && timeout 120s build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test'
```

Results:

- `zr_vm_aot_c_generic_monomorphization_test`: 1 test, 0 failures; generated shared library returned 81.
- CTest `aot_c_generic_monomorphization`: 1 test, passed.
- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.
- CTest `generic_instantiation` and `generic_constraints`: 2 tests, passed.
- `zr_vm_parser_test`: 75 tests, 0 failures.
- `zr_vm_type_inference_test`: executable passed.
- `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1 test, 0 failures.

## Acceptance Decision

Accepted as completed 08-S3.

This completes only the closed value-generic monomorphized C generation slice. The remaining 08 work is 08-S4 generic dictionary layout/lazy resolution, 08-S5 generic `CALL_TYPED`, 08-S6 dynamic-instance deopt, and 08-S7 full-AOT missing-instance enforcement.
