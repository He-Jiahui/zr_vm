# AOT 10-S5I / 12-S5H Dynamic Dependency Non-Exported Method Token Root

## Scope
- Extended the `@dynamic_dependency` carrier represented by function decorator metadata `dynamicDependencyMethodToken`.
- The token-backed resolver now accepts current-module typed function symbols that are not module exports.
- Affected layers: AOT reachability root collection, opt-in generated C code stripping diagnostics, source-contract coverage, AOT plan records, session coordination, and typed module metadata documentation.
- The resolver remains intentionally narrow: it only resolves current root-module `MEMBER_DEF` tokens through `typedExportedSymbols` entries that identify function symbols and map to valid callable children.

## Baseline
- Before the change, `dynamicDependencyMethodToken` required a root typed symbol whose `exportKind` was `ZR_MODULE_EXPORT_KIND_FUNCTION`.
- RED evidence: a reachability fixture using a typed function symbol with `exportKind = ZR_MODULE_EXPORT_KIND_VALUE` failed with `Expected TRUE Was FALSE`.
- The generated-C fixture failed the same way because the old collector could not turn the non-exported token binding into `code_stripping.annotationRoot[0] = 2`.
- Known repository baseline remains scoped by the AOT plan: cross-module annotations, field/type dependencies, non-method member tokens, `@dynamically_accessed` dataflow, warning promotion/suppression policy, unannotated reflection warnings, and complete metadata sweep are still open.

## Test Inventory
- Focused unit/subsystem:
  - `tests/parser/test_aot_reachability.c` adds `test_collect_reflection_annotation_roots_keeps_non_exported_dynamic_dependency_method_token`.
  - `tests/parser/test_aot_c_reflection_annotation_preserve.c` adds `test_aot_c_code_stripping_preserves_non_exported_dynamic_dependency_method_token_metadata`.
  - `tests/parser/test_aot_c_source_contracts.c` locks the token resolver helper, function-symbol gate, `MEMBER_DEF` gate, matched token count, and function-table lookup.
- Integration/project:
  - AOT C code-stripping CTest group: `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`.
  - AOT shared-library smoke tests: global, call, and dynamic deopt bridge.
- Boundary cases covered:
  - Current-module non-exported typed function symbol can be preserved by exact `MEMBER_DEF` token.
  - Existing exported method-token, method-name, signature-hash, function-index, and reflectable annotation roots continue to pass.
  - Method-token resolver now rejects non-unique token matches instead of silently selecting one.
- Boundary cases not covered:
  - Cross-module method tokens, field/type dependency tokens, non-method member tokens, raw metadata records without a callable-child binding, and complete annotation dataflow.

## Tooling Evidence
- Tool versions:
  - WSL gcc: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
  - WSL clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`
  - WSL cmake: `cmake version 3.22.1`
  - Windows MSVC: `cl.exe 19.44.35228.0`
  - Windows cmake: `cmake version 3.23.0-rc2`
- WSL gcc RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test zr_vm_aot_c_reflection_annotation_preserve_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_reachability_test'`
  - Observed reachability 14 tests with 1 failure: `Expected TRUE Was FALSE` for the non-exported method-token root.
  - `./build-wsl-gcc/bin/zr_vm_aot_c_reflection_annotation_preserve_test` observed 11 tests with 1 failure on the generated-C non-exported method-token retention fixture.
- WSL gcc focused GREEN:
  - Built `zr_vm_aot_reachability_test`, `zr_vm_aot_c_reflection_annotation_preserve_test`, and `zr_vm_aot_c_source_contracts_test`.
  - Direct runs reported reachability 14/0, annotation preserve 11/0, and source contracts 23/0.
- WSL integration GREEN:
  - `ctest --test-dir build-wsl-gcc -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" --output-on-failure` reported 3/3.
  - Direct smoke runs reported global shared-library 10/0, call shared-library 5/0, and dynamic deopt bridge 7/0.
- WSL clang GREEN:
  - Same CTest group reported 3/3.
  - Direct runs reported reachability 14/0, annotation preserve 11/0, source contracts 23/0, global shared-library 10/0, call shared-library 5/0, and dynamic deopt bridge 7/0.
- Windows MSVC Debug GREEN:
  - Built `zr_vm_aot_reachability_test`, `zr_vm_aot_c_reflection_annotation_preserve_test`, and `zr_vm_aot_c_source_contracts_test`.
  - CTest group reported 3/3.
  - Direct runs reported reachability 14/0, annotation preserve 11/0, and source contracts 23/0.

## Results
- Passed:
  - `dynamicDependencyMethodToken = 0x03000008` on a non-exported typed function symbol preserves the matching callable child by flat function index.
  - Generated C emits `code_stripping.annotationRoot[0] = 2` and keeps `zr_aot_fn_2`.
  - Existing dynamic dependency method token/name/signature roots continue to pass.
- Failed and fixed:
  - RED showed the old token resolver required exported function symbols. The fix added a token-specific typed-function-symbol matcher and removed the export-kind requirement from the token path.
  - MSVC initially warned that the test helper assigned a wide `TZrUInt32` to the `TZrUInt8 exportKind` field. The helper parameter was narrowed to `TZrUInt8`, and the focused gcc/MSVC checks were rerun.
- Not claimed:
  - No cross-module annotation semantics, no field/type dependency handling, no non-method member-token handling, no full `@dynamically_accessed` dataflow, and no complete metadata sweep.

## Acceptance Decision
- Accepted for 10-S5I / 12-S5H current-module non-exported MethodDef token dynamic dependency roots.
- The acceptance boundary is explicit: this closes only token-to-non-exported-function-symbol preservation, while the larger 07-12 AOT goal remains active.
