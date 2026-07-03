# AOT 10-S5O / 12-S7ZZZB Callsite Annotation Warning Suppression

Date: 2026-07-03 05:13:46 +08:00

Status: complete for this sub-slice. The broader AOT 07-12 goal remains active.

## Scope

- Affected layers: trim annotation warning scan, generated-C trim warning markers, and source-contract coverage.
- Retained callers can now suppress their own static-call warning when they call a callee marked
  `requiresUnreferencedCode`.
- The global writer-level `suppressAnnotationWarnings` option remains supported and still suppresses all annotation
  warnings.

## Baseline

- `requiresUnreferencedCode` callees already produced a visible `trim_warning.annotation[]` marker when called by a
  retained static caller under code stripping.
- The writer-level `suppressAnnotationWarnings` option could hide all annotation warning entries and move them into
  `annotationSuppressedCount`.
- There was no per-caller annotation-driven suppression path, so a locally acknowledged callsite still emitted the
  visible warning.

## Test Inventory

- `tests/parser/test_aot_c_reflection_annotation_preserve.c`
  - emits a visible warning for a retained caller invoking a `requiresUnreferencedCode` callee
  - suppresses all annotation warnings with the writer-level option
  - suppresses only the callsite warning when the caller has `suppressRequiresUnreferencedCodeWarning: true`
  - keeps unannotated static callees warning-free
  - preserves quoted reason text for visible warnings
- `tests/parser/test_aot_c_source_contracts.c`
  - source guard requires the suppressed annotation counter API and the callsite suppression metadata key

## Tooling Evidence

- WSL GCC:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_reflection_annotation_preserve_test -j 4`
- WSL GCC:
  `./build-wsl-gcc/bin/zr_vm_aot_c_reflection_annotation_preserve_test`
- WSL GCC:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j 4`
- WSL GCC:
  `./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test`
- WSL clang:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 2`
- WSL clang:
  `./build-wsl-clang/bin/zr_vm_aot_c_reflection_annotation_preserve_test`
- WSL clang:
  `./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test`
- Windows MSVC Debug, Visual Studio environment:
  `cmake --build build-msvc --target zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test --config Debug -j 4`
- Windows MSVC Debug:
  `build-msvc\bin\Debug\zr_vm_aot_c_reflection_annotation_preserve_test.exe`
- Windows MSVC Debug:
  `build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe`

## Results

- RED: the new callsite-suppression fixture failed on WSL GCC with `Expected Non-NULL`; the generated C still had
  `annotationCount = 1`, `annotationSuppressedCount = 0`, and a visible `trim_warning.annotation[0]`.
- Fix: annotation warning scanning now separates visible and suppressed warnings. A caller with
  `suppressRequiresUnreferencedCodeWarning: true` moves matching `requires-unreferenced-code` warnings into
  `annotationSuppressedCount` without emitting visible warning entries.
- Fix: the global `suppressAnnotationWarnings` option now folds both visible and callsite-suppressed warnings into the
  suppressed count.
- GREEN: WSL GCC reflection annotation preserve passed 12/0 and source contracts passed 24/0.
- GREEN: WSL clang reflection annotation preserve passed 12/0 and source contracts passed 24/0.
- GREEN: Windows MSVC Debug reflection annotation preserve passed 12/0 and source contracts passed 24/0.

## Acceptance Decision

- Accepted for this sub-slice: per-caller suppression for `requires-unreferenced-code` trim annotation warnings is
  observable in generated-C diagnostics and guarded by source-contract coverage.
- Remaining work: this does not claim complete `@dynamically_accessed` dataflow, complete annotation/promotion policy,
  unannotated reflection warning coverage, or the full trim analyzer.
