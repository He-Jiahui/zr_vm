# AOT 10-S5H / 12-S5G Dynamic Dependency Method Name Signature-Hash Root

## Scope
- Added optional signature-hash disambiguation for the `@dynamic_dependency` carrier represented by function decorator metadata `dynamicDependencyMethodName`.
- The companion metadata field is `dynamicDependencyMethodSignatureHash` and is interpreted as a uint64 value.
- Affected layers: AOT reachability root collection, opt-in generated C code stripping diagnostics, source-contract coverage, AOT plan records, session coordination, and typed module metadata documentation.
- The resolver remains intentionally narrow: it only matches current root-module `typedExportedSymbols` entries that are function exports and maps their `callableChildIndex` back to the pre-trim function table.

## Baseline
- Before the change, `dynamicDependencyMethodName` resolved only by exported symbol name.
- RED evidence: a reachability fixture with two exported `target` symbols and distinct signature hashes resolved to the first same-name export, failing with `Expected 2 Was 1`.
- Known repository baseline remains scoped by the AOT plan: cross-module annotations, non-exported member dependencies, field/type dependencies, `@dynamically_accessed` dataflow, warning promotion/suppression policy, unannotated reflection warnings, and complete metadata sweep are still open.

## Test Inventory
- Focused unit/subsystem:
  - `tests/parser/test_aot_reachability.c` adds `test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_name_signature_hash`.
  - `tests/parser/test_aot_reachability.c` adds `test_collect_reflection_annotation_roots_rejects_ambiguous_dynamic_dependency_method_name`.
  - `tests/parser/test_aot_c_reflection_annotation_preserve.c` adds `test_aot_c_code_stripping_preserves_dynamic_dependency_method_name_signature_hash_metadata`.
  - `tests/parser/test_aot_c_source_contracts.c` locks the uint64 metadata helper, signature-hash field, name+signature resolver call, `symbol->signatureHash` match, and matched-symbol counting.
- Integration/project:
  - AOT C code-stripping CTest group: `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`.
  - AOT shared-library smoke tests: global, call, and dynamic deopt bridge.
- Boundary cases covered:
  - Two same-name exported functions can be disambiguated by `dynamicDependencyMethodSignatureHash`.
  - Duplicate same-name exported functions without a hash are rejected as ambiguous.
  - A present `dynamicDependencyMethodSignatureHash = 0` is treated as an explicit hash and can match an exported symbol whose `signatureHash` is zero.
  - Existing function-index, method-token, and single method-name dynamic dependency roots continue to pass.
- Boundary cases not covered:
  - Cross-module method names, non-exported method names, field/type dependencies, malformed non-uint64 hash metadata, and complete annotation dataflow.

## Tooling Evidence
- WSL gcc RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_reachability_test'`
  - Observed 11 tests with 1 failure: `Expected 2 Was 1` for the signature-hash method-name root.
- WSL gcc focused GREEN:
  - Same target after implementation passed reachability 13/0.
- WSL integration GREEN:
  - `ctest --test-dir build-wsl-gcc -R "aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve" --output-on-failure` reported 3/3.
  - Direct runs reported reachability 13/0, annotation preserve 10/0, source contracts 23/0, global shared-library smoke 10/0, call shared-library smoke 5/0, and dynamic deopt bridge smoke 7/0.
- WSL clang GREEN:
  - Same CTest group reported 3/3.
  - Direct runs reported reachability 13/0, annotation preserve 10/0, source contracts 23/0, global shared-library smoke 10/0, call shared-library smoke 5/0, and dynamic deopt bridge smoke 7/0.
- Windows MSVC Debug GREEN:
  - Built `zr_vm_aot_reachability_test`, `zr_vm_aot_c_reflection_annotation_preserve_test`, and `zr_vm_aot_c_source_contracts_test`.
  - CTest group reported 3/3.
  - Direct runs reported reachability 13/0, annotation preserve 10/0, and source contracts 23/0.

## Results
- Passed:
  - `dynamicDependencyMethodName = "target"` plus `dynamicDependencyMethodSignatureHash = 0x2222` preserves the matching exported child by flat function index.
  - Generated C emits `code_stripping.annotationRoot[0] = 2` and keeps `zr_aot_fn_2`.
  - Same-name exported methods without a signature hash are rejected instead of silently binding the first symbol.
  - Explicit zero-hash metadata matches only the exported method whose `signatureHash` is zero.
- Failed and fixed:
  - RED showed the old name-only resolver selected the wrong overload. The fix added uint64 metadata lookup, optional signature-hash filtering, and exact match counting.
- Not claimed:
  - No cross-module annotation semantics, no non-exported member binding, no field/type dependency handling, no full `@dynamically_accessed` dataflow, and no complete metadata sweep.

## Acceptance Decision
- Accepted for 10-S5H / 12-S5G current-module exported method-name dynamic dependency signature-hash disambiguation.
- The acceptance boundary is explicit: this closes only the optional signature-hash disambiguation and ambiguous-name rejection slice, while the larger 07-12 AOT goal remains active.
