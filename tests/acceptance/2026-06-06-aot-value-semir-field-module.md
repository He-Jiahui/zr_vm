# AOT Value SemIR Field Module

## Scope

- Extracted AOT C value-SemIR field address/load/store lowering from `backend_aot_c_value_semir.c` into `backend_aot_c_value_semir_fields.c/.h`.
- Affected layers: AOT C backend organization, AOT C source-contract tests, parser target source glob, value-type SemIR documentation, and the language-pipeline aggregate test list.

## Baseline

- `backend_aot_c_value_semir.c` was 1023 lines and held orchestration, field lowering, inline copy, typed call, and typed return responsibilities.
- Existing behavior already covered primitive POD field load/store, embedded `SZrTypeValue` fields, nested inline struct fields, and explicit unsupported resolved-field boundaries.
- RED: after adding `tests/parser/test_aot_c_value_semir_contracts.c`, GCC first failed because the new `backend_aot_c_value_semir_fields.c/.h` module did not exist:
  - `Expected Non-NULL`
  - `1 Tests 1 Failures 0 Ignored`
- A first attempted RED build exposed missing Unity `setUp` / `tearDown` boilerplate; that test harness issue was fixed before accepting the RED signal.

## Test Inventory

- Focused contract:
  - `tests/parser/test_aot_c_value_semir_contracts.c`
- Existing source contract:
  - `tests/parser/test_aot_c_source_contracts.c`
- Generated-C smoke:
  - `tests/parser/test_aot_c_shared_library_smoke.c`
- Boundary cases:
  - field module owns primitive POD field load/store markers and byte copies
  - field module owns embedded `SZrTypeValue` field `ZrCore_Value_Copy` markers
  - field module owns nested inline struct `memmove` markers
  - field module owns explicit unsupported resolved-field boundaries
  - value-SemIR orchestrator retains copy/call/return lowering and no moved field helper definitions
  - generated shared-library smoke confirms the parser target builds with the globbed new field source

## Tooling Evidence

- GCC 11.4.0 in `build-wsl-gcc`.
- Clang 14.0.0 in `build-wsl-clang`.
- MSVC 19.44.35227.0 in `build-msvc`.

Commands run:

```bash
cmake -S . -B build-wsl-gcc -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build-wsl-gcc --target zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_source_contracts_test -j 8
./build-wsl-gcc/bin/zr_vm_aot_c_value_semir_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test
cmake --build build-wsl-gcc --target zr_vm_aot_c_shared_library_smoke_test -j 8
./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test

cmake -S . -B build-wsl-clang -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-wsl-clang --target zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
./build-wsl-clang/bin/zr_vm_aot_c_value_semir_contracts_test
./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test
./build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test
```

```powershell
cmake -S . -B build-msvc -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build-msvc --config Debug --target zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test --parallel 8
.\build-msvc\bin\Debug\zr_vm_aot_c_value_semir_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_shared_library_smoke_test.exe
git diff --check
```

Production scans:

```powershell
Get-ChildItem zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot -Filter backend_aot_c*.c |
    Select-String -Pattern 'ZrLibrary_AotRuntime_'
Get-ChildItem zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_c_value_semir.c |
    Select-String -Pattern 'backend_aot_write_c_value_field_addr|backend_aot_write_c_value_load\(|backend_aot_write_c_value_store\(|backend_aot_try_write_c_value_field|backend_aot_c_resolve_field_layout'
Get-ChildItem zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_c_value_semir_fields.c |
    Select-String -Pattern 'ZrLibrary_AotRuntime_'
```

## Results

- RED: GCC focused contract failed on missing field module files after the Unity boilerplate was fixed.
- PASS: GCC `zr_vm_aot_c_value_semir_contracts_test`, 1 test, 0 failures.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 17 tests, 0 failures.
- PASS: GCC `zr_vm_aot_c_shared_library_smoke_test`, 8 tests, 0 failures.
- PASS: Clang `zr_vm_aot_c_value_semir_contracts_test`, 1 test, 0 failures.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 17 tests, 0 failures.
- PASS: Clang `zr_vm_aot_c_shared_library_smoke_test`, 8 tests, 0 failures.
- PASS: MSVC `zr_vm_aot_c_value_semir_contracts_test`, 1 test, 0 failures.
- PASS: MSVC `zr_vm_aot_c_source_contracts_test`, 17 tests, 0 failures.
- PASS: MSVC `zr_vm_aot_c_shared_library_smoke_test`, 8 tests, 0 failures, 8 ignored Unix-only runtime paths.
- PASS: `backend_aot_c_value_semir.c` no longer contains the moved field helper definitions.
- PASS: `backend_aot_c_value_semir_fields.c` contains no `ZrLibrary_AotRuntime_` emission.
- PASS: `backend_aot_c*.c` helper scan still reports only the known scaffolding/export boundaries: `ZrLibrary_AotRuntime_FailGeneratedFunction`, `ZrLibrary_AotRuntime_BeginGeneratedFunction`, and `ZrLibrary_AotRuntime_PublishModuleExports`.
- PASS: `git diff --check` reported only the repository's existing LF-to-CRLF warnings and no whitespace error.
- File sizes after split:
  - `backend_aot_c_value_semir.c`: 388 lines
  - `backend_aot_c_value_semir_fields.c`: 671 lines
  - `test_aot_c_value_semir_contracts.c`: 159 lines

## Acceptance Decision

- Accepted for the focused value-SemIR field module extraction.

Remaining risks:

- This is a behavior-preserving module split. It does not add a new value-type field lowering shape.
- The C backend still has the known generated-function scaffolding/export runtime boundaries listed above.
- LLVM keeps its existing helper-backed value/member routes until LLVM parity work.
