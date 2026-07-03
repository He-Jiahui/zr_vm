# AOT 10-S5G / 12-S5F Dynamic Dependency Method Name Root

## Scope
- Added current-module exported method-name support for the `@dynamic_dependency` carrier surface represented by function decorator metadata `dynamicDependencyMethodName`.
- Affected layers: AOT reachability root collection, opt-in generated C code stripping diagnostics, source-contract coverage, AOT plan records, and typed module metadata documentation.
- The resolver is intentionally narrow: it only matches root-module `typedExportedSymbols` entries that are function exports and maps their `callableChildIndex` back to the pre-trim function table.

## Baseline
- Before the change, annotation root collection handled `reflectable`, `dynamicDependencyFunctionIndex`, and `dynamicDependencyMethodToken`, but ignored `dynamicDependencyMethodName`.
- RED evidence: a reachability fixture with `dynamicDependencyMethodName = "target"` kept `annotationRootCount` at 0, and the generated-C fixture could not retain the otherwise unreachable target.
- Known repository baseline remains scoped by the AOT plan: cross-module annotations, non-exported member dependencies, overload/signature disambiguation, `@dynamically_accessed` dataflow, warning promotion/suppression policy, unannotated reflection warnings, and complete metadata sweep are still open.

## Test Inventory
- Focused unit/subsystem:
  - `tests/parser/test_aot_reachability.c` adds `test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_name`.
  - `tests/parser/test_aot_c_reflection_annotation_preserve.c` adds `test_aot_c_code_stripping_preserves_dynamic_dependency_method_name_metadata`.
  - `tests/parser/test_aot_c_source_contracts.c` locks the string metadata helper, method-name resolver, typed exported symbol name match, and function-table flat-index lookup.
- Integration/project:
  - AOT C code-stripping CTest group: `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`.
  - AOT shared-library smoke tests: global, call, and dynamic deopt bridge.
- Boundary cases covered:
  - Only exported function symbols are considered.
  - The matched exported name must map through `callableChildIndex` to a valid child function and then to a pre-trim flat function index.
  - Existing token/function/reflection annotation root paths continue to pass after adding name binding.
- Boundary cases not covered:
  - Cross-module method names, non-exported method names, overloaded name/signature disambiguation, field/type dependencies, malformed non-string metadata, and complete annotation dataflow.

## Tooling Evidence
- WSL gcc focused RED/GREEN:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_reachability_test'`
  - RED observed 10 tests with 1 failure: `Expected 1 Was 0` for the method-name annotation root count.
  - GREEN observed reachability 10/0.
- WSL gcc focused generated-C/source-contract GREEN:
  - `cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test zr_vm_aot_c_reflection_annotation_preserve_test zr_vm_aot_c_source_contracts_test -j 2`
  - `./build-wsl-gcc/bin/zr_vm_aot_reachability_test`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_reflection_annotation_preserve_test`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test`
  - Results: reachability 10/0, annotation preserve 9/0, source contracts 23/0.
- WSL clang focused GREEN:
  - Rebuilt and ran `zr_vm_aot_c_source_contracts_test`; direct runs passed reachability 10/0, annotation preserve 9/0, and source contracts 23/0.
- WSL integration GREEN:
  - `ctest --test-dir build-wsl-gcc -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" --output-on-failure`
  - `ctest --test-dir build-wsl-clang -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" --output-on-failure`
  - Both reported 3/3.
  - `zr_vm_aot_c_global_shared_library_smoke_test` reported 10/0, `zr_vm_aot_c_call_shared_library_smoke_test` reported 5/0, and `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` reported 7/0 on both WSL toolchains.
- Windows MSVC Debug GREEN:
  - Built `zr_vm_aot_reachability_test`, `zr_vm_aot_c_reflection_annotation_preserve_test`, and `zr_vm_aot_c_source_contracts_test`.
  - CTest group reported 3/3.
  - Direct runs reported reachability 10/0, annotation preserve 9/0, and source contracts 23/0.
  - Unix-only smoke executables reported zero failures with expected ignored counts.

## Results
- Passed:
  - The method-name resolver preserves the target exported child by flat function index.
  - Generated C emits `code_stripping.annotationRoot[0] = 2` and keeps `zr_aot_fn_2`.
  - Existing reflectable, dynamic function-index, dynamic method-token, annotation warning, and code-stripping smoke paths stayed green in the verified matrix.
- Failed and fixed:
  - RED showed the new carrier was ignored. The fix added string metadata lookup, exported symbol name matching, callable-child to function-table resolution, and annotation root collection.
- Not claimed:
  - No cross-module annotation semantics, no non-exported member binding, no overload/signature disambiguation, no field/type dependency handling, no full `@dynamically_accessed` dataflow, and no complete metadata sweep.

## Acceptance Decision
- Accepted for 10-S5G / 12-S5F current-module exported method-name dynamic dependency roots.
- The acceptance boundary is explicit: this closes only the exported method-name carrier slice and leaves the larger 07-12 AOT plan active.
