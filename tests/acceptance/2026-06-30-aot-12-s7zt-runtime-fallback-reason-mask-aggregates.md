# AOT 12-S7ZT Runtime Fallback Reason-Mask Aggregates

## Scope
- Adds generated-C header markers for visible and suppressed runtime fallback trim-warning reason masks.
- Affected layers: AOT C emitter diagnostics and parser smoke tests.

## Baseline
- Before this slice, generated C exposed `trim_warnings.runtimeFallbackCount`,
  `trim_warnings.runtimeFallbackSuppressedCount`, and per-visible-warning `reasonFlag`.
- When a warning was suppressed, consumers could see only the suppressed count; the hidden reason bits were not
  available without disabling suppression.

## Test Inventory
- `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`
  - Visible dynamic-call warning requires `runtimeFallbackReasonMask = 1`.
  - Global warning suppression requires visible mask `0` and suppressed mask `1`.
  - Reason-mask suppression requires visible mask `0` and suppressed mask `1`.
  - Visible dynamic-value-access warnings require `runtimeFallbackReasonMask = 2`.
- `tests/parser/test_aot_c_code_stripping.c`
  - Adjacent AOT C code-stripping diagnostics remain stable.

## Tooling Evidence
- RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'`
  - Observed 7 tests, 4 failures, 0 ignored, all failing at the newly required reason-mask markers.
- GREEN and regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test zr_vm_aot_c_code_stripping_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test zr_vm_aot_c_code_stripping_test -j 8 && ./build-wsl-clang/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_code_stripping_test'`
  - MSVC Debug with imported Visual Studio environment:
    `cmake --build build-msvc --config Debug --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test zr_vm_aot_c_code_stripping_test -j 8`
    plus both focused executables.
  - `ctest --test-dir build-wsl-gcc -R "^(aot_c_code_stripping|language_pipeline_smoke)$" --output-on-failure`
  - `ctest --test-dir build-msvc -C Debug -R "^(aot_c_code_stripping|language_pipeline_smoke)$" --output-on-failure`
  - `git diff --check -- tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_runtime_fallback.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_runtime_fallback.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`

## Results
- WSL gcc: dynamic deopt bridge smoke 7/0; code stripping 5/0; focused CTest `aot_c_code_stripping` 1/1.
- WSL clang: dynamic deopt bridge smoke 7/0; code stripping 5/0.
- Windows MSVC Debug: dynamic deopt bridge smoke 0 failures / 7 ignored; code stripping 5/0; focused CTest
  `aot_c_code_stripping` 1/1.
- `git diff --check` exited 0 and reported only LF/CRLF line-ending warnings.

## Acceptance Decision
- Accepted for 12-S7ZT.
- This closes only the header-level reason-mask observability gap for runtime fallback trim warnings.
- Full trim analyzer, reflection data-flow annotations, annotation-based warning suppression/promotion,
  cross-module export-token rewrite, and complete metadata sweep remain open.
