# AOT 11-S4Q - generated TypeSpec-backed type layout token population

## Scope

11-S4Q extends generated `zr_aot_type_layout_tokens[]` population from the 11-S4P TypeDef-backed subset to current-function `TYPE_SPEC` records.

Affected layers:
- AOT C code generation
- generated code-registration metadata token table
- parser AOT generated-C smoke coverage
- plan/module documentation

Affected code:
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c`
- `tests/parser/test_aot_c_generic_call_typed.c`

## Baseline

11-S4P populated generated token entries only for local named layouts that could be uniquely matched to `TYPE_DEF` metadata. Generic layouts backed by `TYPE_SPEC` records still emitted `0u`, even when the current function already had a canonical `GENERIC_INST` TypeSpec signature.

Pre-change RED evidence:
- `aot_c_generic_call_typed` failed after requiring `0x07000001u`.
- Generated `zr_aot_type_layout_tokens[]` for `Pair<int, int>` contained only `0u` entries.

Known repository baseline:
- The worktree contains many unrelated existing modified and untracked files/build directories.
- Windows value-type/shared-library smoke tests keep Unix-only branches ignored by design.

## Test Inventory

Focused subsystem cases:
- `tests/parser/test_aot_c_generic_call_typed.c`
  - Requires generated token table presence.
  - Requires generated `TYPE_SPEC` token entry `0x07000001u` for `Pair<int, int>`.
  - Compiles the generated C shared library on Unix.

Regression cases:
- `tests/parser/test_aot_c_type_layout_contracts.c`
- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`

Boundary and negative cases:
- TypeDef-backed union token population remains covered by the value-type smoke test.
- Missing metadata, ambiguous matches, cross-module records, and unsupported signature shapes still conservatively emit `0u`.
- The matcher is structural for supported signature nodes and does not claim runtime generic layout construction.

## Tooling Evidence

Tool versions:
- WSL GCC: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
- WSL clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`
- MSVC: `19.44.35227.0`

RED command:
```powershell
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j2 && ctest --test-dir build-wsl-gcc -R '^aot_c_generic_call_typed$' --output-on-failure"
```

RED output summary:
- `test_aot_c_generic_call_typed_emits_monomorphized_and_shared_method_forms:FAIL: Expected Non-NULL`
- Failing assertion was the new generated `0x07000001u` TypeSpec token check.

GREEN commands:
```powershell
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_type_layout_contracts_test -j2 && ctest --test-dir build-wsl-gcc -R '^(aot_c_generic_call_typed|aot_c_type_layout_contracts)$' --output-on-failure && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_generic_call_typed_test zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_type_layout_contracts_test -j2 && ctest --test-dir build-wsl-clang -R '^(aot_c_generic_call_typed|aot_c_type_layout_contracts)$' --output-on-failure && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
cmake --build build-msvc --config Debug --target zr_vm_aot_c_generic_call_typed_test zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_type_layout_contracts_test --parallel 2
ctest --test-dir build-msvc -C Debug -R '^(aot_c_generic_call_typed|aot_c_type_layout_contracts)$' --output-on-failure
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_value_type_shared_library_smoke_test.exe
```

Generated artifact evidence:
- `build-wsl-gcc/tests_generated/aot_c_generic_call_typed/aot_c/src/main.c` contains `zr_aot_type_layout_tokens[]`.
- The generated table entry for layout slot 4 is `0x07000001u`.
- The generated code-registration mirrors `.typeLayoutTokens = zr_aot_type_layout_tokens` and `.typeLayoutTokenCount = 5`.

## Results

Passed checks:
- WSL GCC CTest: `aot_c_type_layout_contracts|aot_c_generic_call_typed` passed 2/2.
- WSL GCC direct executables: source contracts 19/0, value-type shared-library smoke 3/0.
- WSL clang CTest: `aot_c_type_layout_contracts|aot_c_generic_call_typed` passed 2/2.
- WSL clang direct executables: source contracts 19/0, value-type shared-library smoke 3/0.
- Windows MSVC Debug CTest: `aot_c_type_layout_contracts|aot_c_generic_call_typed` passed 2/2.
- Windows MSVC Debug direct executables: source contracts 19/0, value-type shared-library smoke 2/0/1 ignored.

Fixes made:
- Added TypeSpec token lookup after TypeDef lookup.
- Added a small generated-token signature matcher for supported canonical type signatures.
- Preserved `0u` fallback for unmatched, ambiguous, cross-module, or unsupported cases.

## Acceptance Decision

Accepted for 11-S4Q.

The change has direct RED/GREEN evidence and focused gcc, clang, and MSVC validation. It closes current-function generated TypeSpec token population, but not cross-module token-table population, runtime generic layout construction, ownership-offset emission, MethodSpec materialization, or public generic reflection entity support.
