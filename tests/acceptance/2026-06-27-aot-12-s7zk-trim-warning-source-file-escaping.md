# 12-S7ZK Trim Warning Source File Escaping

## Scope
- Changed runtime-fallback trim warning comments emitted by the AOT C backend.
- Affected layers: AOT C diagnostics generation, generated-C warning marker tests, and code-stripping validation docs.
- `sourceFile` is now emitted as a quoted/escaped field so warning consumers can parse paths containing spaces, backslashes, quotes, or control characters.

## Baseline
- Before this slice, `backend_aot_c_runtime_fallback.c` printed `sourceFile=%s` directly.
- That made plain paths readable but ambiguous for machine consumers and unsafe for paths containing quotes or separators.
- Existing repository baseline remains: Windows dynamic deopt shared-library smoke tests are ignored by design because they validate Unix shared-library execution.

## Test Inventory
- Focused subsystem: `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`
- Regression coverage: `tests/parser/test_aot_c_source_contracts.c`
- Code-stripping coverage: `tests/parser/test_aot_c_code_stripping.c`
- Boundary case: source path `src\quoted "module".zr` must emit as `sourceFile="src\\quoted \"module\".zr"`.
- Negative path: unquoted/raw `sourceFile=...` no longer satisfies the focused warning assertions.

## Tooling Evidence
- WSL gcc Debug:
  - `cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`
  - `cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_code_stripping_test -j2`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test`
  - `ctest --test-dir build-wsl-gcc -R 'aot_c_code_stripping|aot_c_source_contracts' --output-on-failure`
- WSL clang Debug:
  - same focused dynamic/source/code-stripping commands under `build-wsl-clang`
  - `ctest --test-dir build-wsl-clang -R 'aot_c_code_stripping|aot_c_source_contracts' --output-on-failure`
- Windows MSVC Debug:
  - imported Visual Studio environment with `using-vsdevcmd`
  - `cmake --build build-msvc --config Debug --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_code_stripping_test --parallel 2`
  - `.\build-msvc\bin\Debug\zr_vm_aot_c_dynamic_deopt_bridge_smoke_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_aot_c_code_stripping_test.exe`
  - `ctest --test-dir build-msvc -C Debug -R "aot_c_code_stripping|aot_c_source_contracts" --output-on-failure`
- Format check:
  - `git diff --check`

## Results
- RED: WSL gcc dynamic deopt bridge smoke failed 4/7 after tests required quoted existing markers and the escaped source-path fixture.
- GREEN: WSL gcc dynamic deopt bridge smoke 7/0, source contracts 21/0, code stripping 5/0, CTest `aot_c_code_stripping` 1/1.
- GREEN: WSL clang dynamic deopt bridge smoke 7/0, source contracts 21/0, code stripping 5/0, CTest `aot_c_code_stripping` 1/1.
- GREEN: Windows MSVC Debug dynamic deopt bridge smoke 0 failures / 7 ignored, source contracts 21/0, code stripping 5/0, CTest `aot_c_code_stripping` 1/1.
- `git diff --check` exited 0 and reported only existing LF/CRLF normalization warnings.

## Acceptance Decision
- Accepted for 12-S7ZK.
- The warning record now keeps the existing source location fields while making `sourceFile` unambiguous for downstream tooling.
- Remaining open work: full trim analyzer, `@requires_unreferenced_code`, reflection data-flow annotations, annotation-based warning suppression/promotion, cross-module/export token handling, and dump/diff tooling.
