# AOT 10-S5P / 12-S7ZZZC Annotation Warning Source Attribution

Date: 2026-07-03 05:30:18 +08:00

Status: complete for this sub-slice. The broader AOT 07-12 goal remains active.

## Scope

- Affected layers: trim annotation warning diagnostics, generated-C marker text, and source-contract coverage.
- Visible `requires-unreferenced-code` annotation warnings now carry caller source attribution.
- Existing reason-text escaping and callsite/global annotation suppression behavior remain compatible.

## Baseline

- `requiresUnreferencedCode` callees already produced visible annotation warning markers for retained static callers.
- Those markers identified `function`, `instruction`, and `targetFunction`, but did not identify the source file, line
  span, or column span for the warning.
- Runtime fallback warnings already carried richer source attribution, leaving annotation warnings less actionable.

## Test Inventory

- `tests/parser/test_aot_c_reflection_annotation_preserve.c`
  - visible annotation warnings now assert `sourceFile`, `sourceLine/sourceLineEnd`, and
    `sourceColumn/sourceColumnEnd`
  - reason-text warnings assert that source attribution remains present before `message="..."`
  - suppressed annotation warning fixtures continue to hide visible per-warning markers
- `tests/parser/test_aot_c_source_contracts.c`
  - source guard requires the annotation warning writer to use ExecIR source-location helpers and emit source marker
    fields

## Tooling Evidence

- WSL GCC:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 2`
- WSL GCC:
  `ctest --test-dir build-wsl-gcc --output-on-failure -R "aot_c_reflection_annotation_preserve"`
- WSL GCC:
  `./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test`
- WSL clang:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 2`
- WSL clang:
  `ctest --test-dir build-wsl-clang --output-on-failure -R "aot_c_reflection_annotation_preserve"`
- WSL clang:
  `./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test`
- Windows MSVC Debug, Visual Studio environment:
  `cmake --build build-msvc --config Debug --target zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 2`
- Windows MSVC Debug:
  `ctest --test-dir build-msvc -C Debug --output-on-failure -R "aot_c_reflection_annotation_preserve"`
- Windows MSVC Debug:
  `build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe`

## Results

- RED: WSL GCC `aot_c_reflection_annotation_preserve` failed with `Expected Non-NULL` after the visible warning fixture
  expected `sourceFile/sourceLine/sourceColumn` fields; the old marker had only function/instruction/target/reason.
- RED: WSL GCC source contracts failed on the new ExecIR source-location helper needle.
- Fix: `backend_aot_c_annotation_warnings.c` now writes quoted caller `sourceFile` and caller
  `sourceLine/sourceLineEnd/sourceColumn/sourceColumnEnd` from existing source-location helpers.
- GREEN: WSL GCC reflection annotation preserve passed 12/0 and source contracts passed 24/0.
- GREEN: WSL clang reflection annotation preserve passed 12/0 and source contracts passed 24/0.
- GREEN: Windows MSVC Debug reflection annotation preserve passed 12/0 and source contracts passed 24/0.

## Acceptance Decision

- Accepted for this sub-slice: visible `requires-unreferenced-code` trim annotation warnings are now source-locatable in
  generated-C diagnostics and guarded by source-contract coverage.
- Remaining work: this does not claim complete `@dynamically_accessed` dataflow, complete annotation/promotion policy,
  unannotated reflection warning coverage, the full trim analyzer, or the broader 07-12 goal.
