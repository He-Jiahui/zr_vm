# AOT 12-S7ZU / 10-S5E Annotation Warning Suppression

时间：2026-06-30 12:54:26 +08:00

## Scope

- Writer-level suppression for the compile-time annotation warning stream.
- Public option: `SZrAotWriterOptions.suppressAnnotationWarnings`.
- Output contract:
  - default path keeps `trim_warnings.annotationCount` and per-warning `trim_warning.annotation[]`;
  - suppressed path writes `trim_warnings.annotationCount = 0`;
  - suppressed path writes `trim_warnings.annotationSuppressedCount = <total>`;
  - suppressed path omits per-warning `trim_warning.annotation[]` markers.

This does not implement attribute-level per-warning suppression, warning promotion, or reflection dataflow analysis.

## RED

Command:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_reflection_annotation_preserve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_reflection_annotation_preserve_test'
```

Result before implementation:

- Build failed because `SZrAotWriterOptions` had no `suppressAnnotationWarnings` member.

## GREEN

- Production: `writer.h` exposes `suppressAnnotationWarnings`.
- Production: `backend_aot_option_suppress_annotation_warnings()` normalizes the writer option.
- Production: AOT C emission computes total annotation warnings, splits visible/suppressed counts, writes
  `trim_warnings.annotationSuppressedCount`, and skips per-warning annotation markers when suppression is enabled.
- Tests: `test_aot_c_reflection_annotation_preserve.c` adds a suppressed requires-unreferenced-code static-call fixture.
- Tests: `test_aot_c_source_contracts.c` locks the public option, internal helper, and emitted suppression marker plumbing.
- Adjacent regression coverage: `test_aot_c_global_shared_library_smoke.c` now asserts the existing TYPEOF reflection
  runtime fallback warning marker (`reasonFlag=8 reason=reflection`) and aggregate reason mask.

## Verification

Full validation before the final source-contract marker addition:

- WSL gcc:
  - CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`: 3/3.
  - Annotation preserve: 7/0.
  - Global shared-library smoke: 10/0.
  - Call shared-library smoke: 5/0.
  - Dynamic deopt bridge smoke: 7/0.
  - Source contracts: 22/0.
- WSL clang:
  - CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`: 3/3.
  - Annotation preserve: 7/0.
  - Global shared-library smoke: 10/0.
  - Call shared-library smoke: 5/0.
  - Dynamic deopt bridge smoke: 7/0.
  - Source contracts: 22/0.
- Windows MSVC Debug:
  - CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`: 3/3.
  - Annotation preserve: 7/0.
  - Source contracts: 22/0.
  - Global/call/dynamic Unix-only smoke executables: 0 failures / 10 ignored, 5 ignored, and 7 ignored.

Final quick recheck after adding the source-contract marker:

- WSL gcc source contracts 22/0 and annotation preserve 7/0.
- WSL clang source contracts 22/0 and annotation preserve 7/0.
- Windows MSVC Debug source contracts 22/0 and annotation preserve 7/0.

## Acceptance Decision

Accepted for 12-S7ZU / 10-S5E.

The annotation warning stream now has a writer-level suppression control with visible and suppressed count separation.
Remaining work: attribute-level per-warning suppression, warning promotion, `@dynamically_accessed` dataflow,
token/name dynamic dependencies, cross-module annotation rules, unannotated reflection warnings, and complete metadata sweep.
