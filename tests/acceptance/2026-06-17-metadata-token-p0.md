# Metadata Token P0 Slice

## Scope

- Implement the next P0 metadata/token structure slice from `docs/plans/using`.
- Covered this round:
  - Function-level import member effects generate `ASSEMBLY_REF`, `TYPE_REF`, and `MEMBER_REF` records.
  - Token records carry an explicit `ownerToken`.
  - Import refs form `AssemblyRef <- TypeRef <- MemberRef`.
  - Paired `SIGNATURE` records point back to the entity they sign through `ownerToken`.
  - `.zro` patch 22 persists `ownerToken`; patch 20/21 readers set missing owner values to 0.
  - Import effects and metadata token records preserve target signature identity through `targetMetadataToken`, `targetSignatureToken`, and `targetSignatureHash`.
  - `.zro` patch 23 persists target signature identity; patch 20/21/22 readers set missing target values to 0.
  - `.zro` patch 24 persists metadata record `targetMetadataToken` and `targetSignatureToken`; patch 23 and older readers set missing record target tokens to 0.
  - `.zro` patch 25 persists entry-function-level `moduleMetadataTokenRecords`; patch 24 and older readers leave the module ref table empty.
  - `.zro` patch 26 persists function-level `moduleSignatureHash`; patch 25 and older readers leave the module ABI hash at 0.
  - `.zro` patch 27 persists module effect and metadata record `targetModuleSignatureHash`; patch 26 and older readers leave the target module ABI hash at 0.
  - `.zro` patch 28 persists runtime ref-to-def `moduleMetadataBindings`; patch 27 and older readers leave the binding result table empty.
  - `.zro` patch 29 persists provider `moduleVersion` and AssemblyRef requested/min/max version strings; patch 28 and older readers leave version fields null.
  - Future `.zro` patch values above `ZR_IO_SOURCE_PATCH_CURRENT` are rejected at IO header read time with actual/supported patch diagnostics.
  - Runtime import signature verification now checks AssemblyRef semantic version ranges before target module/token/hash/blob confirmation.
  - Runtime import signature verification now consumes `moduleMetadataTokenRecords` before falling back to function-level `metadataTokenRecords`.
  - Runtime import signature verification now checks nonzero target module ABI hashes before target token/hash/blob confirmation.
  - Runtime import signature verification now uses matching module ref `MEMBER_REF` records as a fallback source for missing effect target identity.
  - Runtime import signature verification now directly scans matching module ref `MEMBER_REF` records even when entry effects are missing, and verifies their owner-chain plus target module/token/hash/blob identity.
  - Runtime import signature verification now records successful caller `MEMBER_REF` to provider `MEMBER_DEF` / `SIGNATURE` bindings in `SZrFunction.moduleMetadataBindings`.
  - Runtime TypeRef binding status now reports stable provider `TYPE_REF -> TYPE_DEF` unmatched, definition mismatch, and layout mismatch context without writing partial sidecars.
  - Runtime import signature verification now records stable TypeRef binding drift as `type_ref_mismatch` in the module-load diagnostic channel while preserving best-effort import success.
  - Module-qualified explicit typed local annotations such as `provider.Option<int>` now emit an explicit `ASSEMBLY_REF` and stable provider `TYPE_REF` even when no import member effect exists.
  - Runtime ref-to-def binding results are queryable through `ZrCore_Function_FindModuleMetadataBinding(function, refToken)`.
  - Runtime import signature verification now binds nonzero target metadata/signature tokens against the provider typed export tokens before hash/blob confirmation.
  - Runtime target token mismatches are strict when both caller and provider tokens are present; hash/blob fallback is reserved for older artifacts with missing token fields.
  - Required import target token mismatches now include expected/actual metadata and signature token fields in the `import signature mismatch` diagnostic.
  - Required import AssemblyRef version mismatches now raise `assembly_version_mismatch` with min/max/actual version context.
  - Required import module ABI hash mismatches now raise `assembly_signature_mismatch` diagnostics with expected/actual module hash context.
  - Module ABI signature hash is computed from provider module version, sorted public typed exports, export identity, stable signature hashes, and canonical signature blob bytes.
  - `MEMBER_REF` blobs append target `METHOD_SIG` / `FIELD_SIG` when the provider export can be resolved from source or binary summary.
  - Module-level import ref aggregation deduplicates duplicate entry/callable import refs while preserving paired `SIGNATURE` records and target identity.
  - `compiler_metadata_signature.c/.h` split signature hash/type/symbol encoding helpers out of `compiler_metadata_token.c` without behavior changes.
  - Guard-style `%import` now emits the import-guard native helper and uses target signature hashes to reject mismatched providers at runtime.
  - Native `%import` compile-time module members now expose synthetic `MEMBER_DEF` / `SIGNATURE` tokens and stable signature hashes.
  - Guard runtime validation now covers nested callable child effects, so target hash drift inside `pub func run(){ using (%import(...)) ... }` falls back to `else`.
  - Typed export `MEMBER_DEF` and import `MEMBER_REF` target sub-signatures now share the same canonical `METHOD_SIG` / `FIELD_SIG` encoder.
  - Guard runtime validation is now hash-first and signature-confirmed when target sub-signature bytes are available.
  - Native guarded import effects record target signature identity through native compile-info fallback when no source/binary summary exists.
  - Source/binary imports retain same-name function candidates and choose the matching signature at member-call sites.
  - Runtime required/guard import verification chooses the matching provider typed export from same-name candidates by target hash/blob/token instead of binding the first same-name export.
  - Source summary primitive annotations are canonicalized to `PRIMITIVE` signature nodes so import ref target signatures match provider typed export signatures.
  - Typed export metadata uses callable child identity/source ranges to preserve the correct overload signature.
  - Required `%import` signature mismatch now raises a structured runtime `import signature mismatch` diagnostic with module/member/hash context.
  - Plugin guard escape scanning distinguishes plugin handle/callable member reference escape from ordinary member values and member-call results.

## Evidence

- Timestamp: 2026-06-17 19:50:28 +08:00.
- Build directory: `build/codex-using-exhaustive-wsl-gcc-debug`.
- TDD red check:
  - Command: `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_metadata_token_model_test -j2`
  - Result before implementation: compile failed because `SZrMetadataTokenRecord` had no `ownerToken` member.
- Focused target build:
  - Command: `wsl.exe -e sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_metadata_token_model_test -j1'`
  - Result: built `zr_vm_metadata_token_model_test`.
  - Note: build emitted two existing unused-function warnings in `compile_expression_types.c`; no metadata token compile/link errors.

### 2026-06-18 03:09:16 +08:00: Module ABI Signature Hash Golden

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4'`
  - Result before implementation: compile failed because `SZrFunction` and `SZrIoFunction` had no `moduleSignatureHash` member.
- Implementation:
  - `SZrFunction` and `SZrIoFunction` now carry `moduleSignatureHash`.
  - `.zro` patch 26 writes/reads the hash after the signature blob heap and before `moduleMetadataTokenRecords`.
  - Runtime copy preserves the hash when loading `.zro` functions.
  - `compiler_metadata_token.c` computes `zr.md.mod.v1\0` stable `XXH3_64bits` over public typed exports sorted by name, including export identity, typed export `signatureHash`, and canonical signature blob bytes.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `zr_vm_project_import_canonicalization_test`: 15 tests, 0 failures.
  - `zr_vm_metadata_token_model_test`: 6 tests, 0 failures.
  - `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 07:15:51 +08:00: Runtime Binding Result .zro Persistence

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4'`
  - Result before implementation: compile failed because `SZrIoFunction` did not expose `moduleMetadataBindingLength` or `moduleMetadataBindings`.
- Implementation:
  - `SZrIoFunction` now carries a runtime binding result table matching `SZrFunction.moduleMetadataBindings`.
  - `.zro` patch 28 writes/reads each binding's caller ref token/signature identity, expected provider identity, and resolved provider identity.
  - Runtime `.zro` load copies persisted binding results back into `SZrFunction.moduleMetadataBindings` so `ZrCore_Function_FindModuleMetadataBinding(function, refToken)` works after binary roundtrip.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test'`
  - `zr_vm_metadata_token_model_test`: 7 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `ctest -R metadata_token_model`: 1/1 passed.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test 2>&1 | tail -n 35'`
  - `zr_vm_project_import_canonicalization_test`: 23 tests, 0 failures.

### 2026-06-18 07:34:01 +08:00: Loader Future Patch Compatibility Diagnostic

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test'`
  - Result before implementation: `test_reader_rejects_future_metadata_patch_with_version_diagnostic` failed because a `.zro` header patched to current+1 was not rejected.
- Implementation:
  - `zr_io_conf.h` exposes `ZR_IO_SOURCE_PATCH_CURRENT`.
  - `writer_binary.c` writes the current patch through that shared constant.
  - `io.c` rejects future `.zro` patch values in `ZrCore_Io_ReadSourceNew()` before reading patch-dependent payload and reports `actualPatch` / `supportedPatch`.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test'`
  - `zr_vm_metadata_token_model_test`: 8 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `ctest -R metadata_token_model`: 1/1 passed.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test 2>&1 | tail -n 35'`
  - `zr_vm_project_import_canonicalization_test`: 23 tests, 0 failures.

### 2026-06-18 08:13:03 +08:00: AssemblyRef Semantic Version Constraints

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_required_import_runtime_rejects_assembly_ref_version_mismatch` failed because a caller `AssemblyRef` range changed to `[2.0.0, 3.0.0)` still accepted provider version `1.0.0`.
- Implementation:
  - `SZrFunction` / `SZrIoFunction` now carry provider `moduleVersion`.
  - `SZrFunctionModuleEffect`, `SZrIoFunctionModuleEffect`, and `SZrMetadataTokenRecord` now carry `requestedModuleVersion`, `minModuleVersionInclusive`, and `maxModuleVersionExclusive`.
  - `module_init_analysis.c` derives provider versions from project metadata or dependency module keys and applies the default semver range `[compiledVersion, nextMajor.0.0)`.
  - `.zro` patch 29 persists module version and AssemblyRef version range fields.
  - Runtime import verification rejects providers outside the range before module hash/token/signature checks; required imports report `assembly_version_mismatch`.
  - `moduleSignatureHash` now includes provider module version in its `zr.md.mod.v1\0` hash input.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 24 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test'`
  - `zr_vm_metadata_token_model_test`: 9 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R "metadata_token_model|project_import_canonicalization" --output-on-failure'`
  - `ctest -R metadata_token_model`: 1/1 passed.
  - `git diff --check` reported no whitespace errors; only existing LF/CRLF conversion warnings.

### 2026-06-18 08:39:06 +08:00: Native Registry Load/Verify Diagnostics

- TDD checks:
  - Native target-hash drift guard coverage passed before production changes, confirming registered native providers already shared the runtime verifier path.
  - `test_required_import_runtime_reports_native_provider_unavailable` first failed because required `%import("zr.math")` without a runtime provider did not report `import_load_unavailable`.
- Implementation:
  - `ZrCore_Module_ImportNativeEntry` now raises `import_load_unavailable: module '<path>'` when a required import resolves to null and no earlier runtime error is pending.
  - `ZrCore_Module_ImportGuardNativeEntry` keeps the existing null fallback so guarded imports still execute `else`.
  - The regression removes the compile-time registered native provider from the runtime registry and cache before execution to exercise provider unavailable behavior.
- Green check:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 26 tests, 0 failures.

### 2026-06-18 09:06:55 +08:00: Dependency Manifest Explicit AssemblyRef Version Range

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_project_compile_applies_dependency_import_version_range_to_assembly_ref` failed because the dependency import still wrote the default AssemblyRef range `requested=1.2.3,min=1.2.3,max=2.0.0`.
- Implementation:
  - Dependency declaration objects accept `minVersionInclusive` and `maxVersionExclusive`.
  - `SZrLibrary_ProjectDependencyReference` stores explicit min/max range and rejects inconsistent ranges for the same owner package/version.
  - `ZrLibrary_Project_GetDependencyImportVersionRange()` resolves the requested provider version and explicit range from current module key plus canonical `$pkg@version/path`.
  - `module_init_analysis.c` applies that range to source/binary import effects and their `AssemblyRef` metadata records without adding `%import` parser syntax.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 27 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test'`
  - `zr_vm_metadata_token_model_test`: 9 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R "metadata_token_model|project_import_canonicalization" --output-on-failure'`
  - `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 09:32:13 +08:00: Loader Source And Descriptor Diagnostics

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_required_import_runtime_reports_source_loader_attempts` failed because `import_load_unavailable` did not include `loader=project-source` attempted source/binary detail.
- Implementation:
  - `SZrGlobalState` now carries a module-load diagnostic buffer with clear/set/get APIs.
  - `ZrCore_Module_ImportByPath()` clears diagnostics at the start of a real load attempt and records loader detail for source loader, binary/source reader, and source compiler failure points.
  - Required `%import` appends the current module-load diagnostic to `import_load_unavailable`.
  - `ZrLibrary_Project_SourceLoadImplementation()` records the canonical module plus attempted `.zr` and `.zro` paths when project source/binary lookup misses.
  - `native_registry_loader()` turns registry descriptor plugin load errors into `loader=native-plugin result=descriptor-load-failed ...` diagnostics without letting later source fallback overwrite the real plugin error.
- Green check:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 29 tests, 0 failures.

### 2026-06-18 10:04:14 +08:00: TypeSpec Baseline Records

- TDD red checks:
  - `test_union_return_type_emits_type_spec_token_record` first failed because `Option<int>` appeared only inside the method signature blob and had no `TYPE_SPEC` entity record.
  - `test_generic_return_type_emits_generic_inst_type_spec` first failed because `Box<int>` was encoded as a bare `TYPE_REF("Box<int>")` instead of `GENERIC_INST`.
- Implementation:
  - Added `compiler_metadata_type_spec.c/.h` as a narrow TypeSpec plan/emit helper for metadata token construction.
  - The token builder now emits `TYPE_SPEC` + paired `SIGNATURE` records for exported signature return/parameter types that are nullable, ownership-qualified, arrays, or closed generic/composite names.
  - Non-union generic instances now encode as `GENERIC_INST(TYPE_REF(open), args...)`; known union instances retain the existing `UNION` node.
  - The slice reuses `SZrMetadataTokenRecord` and `signatureBlobHeap`; no `.zro` patch increment was required.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test'`
  - `zr_vm_metadata_token_model_test`: 11 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 29 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R "metadata_token_model|project_import_canonicalization" --output-on-failure'`
  - `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 10:17:58 +08:00: TypeSpec Deduplication

- TDD red check:
  - `test_duplicate_generic_type_spec_is_deduplicated` first failed because a signature using `Box<int>` as both return type and parameter type emitted two `TYPE_SPEC` records.
- Implementation:
  - `compiler_metadata_type_spec.c` now deduplicates TypeSpec entries by canonical type signature blob before writing records.
  - Duplicate closed/composite types share one `TYPE_SPEC` / `SIGNATURE` pair and one heap entry.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test'`
  - `zr_vm_metadata_token_model_test`: 12 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 29 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R "metadata_token_model|project_import_canonicalization" --output-on-failure'`
  - `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 10:52:36 +08:00: TypeSpec Cross-Module Binding Baseline

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4'`
  - Result before implementation: link failed with undefined reference to `ZrCore_Function_BindMatchingTypeSpecMetadata`.
- Implementation:
  - Added `zr_vm_core/src/zr_vm_core/function_metadata_binding.c` for metadata binding query/upsert and TypeSpec binding helpers.
  - `ZrCore_Function_UpsertModuleMetadataBinding()` centralizes sidecar allocation/reuse.
  - `ZrCore_Function_BindMatchingTypeSpecMetadata()` matches caller/provider `TYPE_SPEC` records by canonical signature hash plus signature blob bytes and records caller `TYPE_SPEC` to provider `TYPE_SPEC` / paired `SIGNATURE` identity in `moduleMetadataBindings`.
  - `module_import_signature.c` calls the TypeSpec binder after successful member import verification; no `.zro` patch increment was required.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test && ctest --test-dir build-wsl-clang -R "metadata_token_model|project_import_canonicalization" --output-on-failure'`
  - WSL clang `zr_vm_metadata_token_model_test`: 13 tests, 0 failures.
  - WSL clang `zr_vm_project_import_canonicalization_test`: 29 tests, 0 failures.
  - WSL clang `ctest -R metadata_token_model`: 1/1 passed.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_token_model_test zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-gcc/bin/zr_vm_metadata_token_model_test && ./build-wsl-gcc/bin/zr_vm_project_import_canonicalization_test && ctest --test-dir build-wsl-gcc -R "metadata_token_model|project_import_canonicalization" --output-on-failure'`
  - WSL gcc `zr_vm_metadata_token_model_test`: 13 tests, 0 failures.
  - WSL gcc `zr_vm_project_import_canonicalization_test`: 29 tests, 0 failures.
  - WSL gcc `ctest -R metadata_token_model`: 1/1 passed.
  - `git diff --check` on touched tracked files reported no whitespace errors; only existing LF/CRLF normalization warnings.

### 2026-06-18 03:41:32 +08:00: Target Module ABI Hash Runtime Binding

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test -j4'`
  - Result before implementation: compile failed because `SZrFunctionModuleEffect` had no `targetModuleSignatureHash` member.
- Implementation:
  - `SZrFunctionModuleEffect`, `SZrIoFunctionModuleEffect`, and `SZrMetadataTokenRecord` now carry `targetModuleSignatureHash`.
  - Source/binary module-init summaries preserve provider `moduleSignatureHash`; import effects and `AssemblyRef` records retain the expected provider module ABI fingerprint.
  - `.zro` patch 27 writes/reads module effect and metadata record `targetModuleSignatureHash`; older patch readers set it to 0.
  - Runtime import verification compares nonzero expected target module hash with provider entry function `moduleSignatureHash` before target token/hash/blob checks.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `zr_vm_project_import_canonicalization_test`: 17 tests, 0 failures.
  - `zr_vm_metadata_token_model_test`: 6 tests, 0 failures.
  - `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 04:03:33 +08:00: Module Ref Target Identity Fallback

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_using_import_guard_runtime_uses_module_ref_identity_when_effect_targets_are_missing` failed with `Expected 77 Was 40`.
- Implementation:
  - `module_import_signature.c` now matches module ref `MEMBER_REF` records by module name, symbol name, and effect kind.
  - When the matching module effect has zero target identity fields, runtime verification fills expected target module hash, metadata token, signature token, and signature hash from the module ref record before provider checks.
  - Existing provider module hash, typed export token/hash, and signature byte confirmation paths are reused.
- Green check:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 19 tests, 0 failures.
- Focused regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `zr_vm_project_import_canonicalization_test`: 19 tests, 0 failures.
  - `zr_vm_metadata_token_model_test`: 6 tests, 0 failures.
  - `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 04:19:39 +08:00: Assembly Signature Mismatch Diagnostic

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_required_import_runtime_rejects_target_module_hash_mismatch` failed because the required import still raised the generic `import signature mismatch` diagnostic instead of `assembly_signature_mismatch` with module hash context.
- Implementation:
  - `SZrModuleImportSignatureMismatch` now carries a mismatch kind.
  - `module_import_signature.c` records target module ABI hash mismatches as `ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_SIGNATURE`.
  - `module_loader.c` formats those required-import failures as `assembly_signature_mismatch`, including canonical module/member and `expectedModuleHash` / `actualModuleHash`.
- Green check:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 19 tests, 0 failures.
- Focused CTest:
  - Command: `wsl.exe -e sh -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure'`
  - Result: `1/1 Test #35: metadata_token_model ... Passed`; `100% tests passed, 0 tests failed out of 1`.
- Direct metadata token binary:
  - Command: `wsl.exe -e sh -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test'`
  - Result: `4 Tests 0 Failures 0 Ignored OK`.
  - Covered cases:
    - `.zro`/runtime roundtrip preserves token/signature heap/hash.
    - Signature hash is stable and changes with normalized signature changes.
    - Union return type uses the union signature node.
    - Import member ref records preserve AssemblyRef/TypeRef/MemberRef owner-chain and paired Signature ownership.
- Diff hygiene:
  - Command: `git diff --check -- zr_vm_core/include/zr_vm_core/metadata_token.h zr_vm_common/include/zr_vm_common/zr_io_conf.h zr_vm_common/include/zr_vm_common/zr_version_info.h zr_vm_core/src/zr_vm_core/io.c zr_vm_parser/src/zr_vm_parser/writer.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c tests/module/test_metadata_token_model.c docs/plans/using/index.md docs/plans/using/03-metadata-and-token-model.md docs/plans/using/05-migration-and-phasing.md docs/plans/using/07-implementation-blueprint.md docs/module-system/typed-module-metadata.md`
  - Result: exit code 0; only LF/CRLF normalization warnings.

### 2026-06-18 05:01:39 +08:00: Runtime Same-Name Export Candidate Binding

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_required_import_runtime_resolves_same_name_signature_candidate` failed because required import bound `helper.pick(true)` to provider `pick(int)` and raised a signature mismatch instead of selecting `pick(bool)`.
- Implementation:
  - `module_init_analysis.c` preserves same-name source function exports and records import-call effects against the source/binary export selected by call argument types.
  - Source summary type refs canonicalize primitive annotations as `PRIMITIVE` nodes, matching provider typed export `METHOD_SIG` encoding.
  - `compiler_metadata_token.c` resolves target signatures against same-name summary exports by target hash/token.
  - `module_import_signature.c` searches provider same-name typed exports by target hash, target signature blob, and target token before falling back to legacy name-first compatibility.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 20 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `zr_vm_metadata_token_model_test`: 6 tests, 0 failures.
  - `ctest -R metadata_token_model`: 1/1 passed.
- Diff hygiene:
  - Command: `git diff --check -- zr_vm_core/src/zr_vm_core/module/module_import_signature.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c tests/parser/test_project_import_canonicalization.c`
  - Result: exit code 0; only LF/CRLF normalization warnings.

### 2026-06-18 05:28:40 +08:00: Runtime Module Ref Table Direct Verification

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_using_import_guard_runtime_verifies_module_ref_table_without_entry_effects` failed with `Expected 77 Was 40`; the old verifier ignored a corrupted module ref table when `moduleEntryEffects` were empty.
- Implementation:
  - `module_import_signature.c` now verifies a single resolved import ref through a shared helper and reuses that path for both `moduleEntryEffects` and decoded `moduleMetadataTokenRecords`.
  - Matching module ref table `MEMBER_REF` records are decoded into temporary effects, checked for `AssemblyRef <- TypeRef <- MemberRef` owner-chain integrity, and verified against provider module hash, target tokens, target signature hash, and target signature bytes.
  - `SZrModuleImportSignatureMismatch` stores an effect snapshot so required-import diagnostics do not hold a pointer to a temporary decoded effect.
  - Target token mismatches are strict when both caller and provider tokens are present; hash/blob fallback remains only for older artifacts missing token fields.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 21 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `zr_vm_metadata_token_model_test`: 6 tests, 0 failures.
  - `ctest -R metadata_token_model`: 1/1 passed.
- Diff hygiene:
  - Command: `git diff --check -- tests/parser/test_project_import_canonicalization.c zr_vm_core/src/zr_vm_core/module/module_import_signature.c zr_vm_core/src/zr_vm_core/module/module_import_signature.h docs/plans/using/index.md docs/plans/using/03-metadata-and-token-model.md docs/plans/using/05-migration-and-phasing.md docs/plans/using/07-implementation-blueprint.md docs/module-system/typed-module-metadata.md tests/acceptance/2026-06-17-metadata-token-p0.md .codex/sessions/20260617-1932-metadata-p0-assemblyref.md`
  - Result: exit code 0; only LF/CRLF normalization warnings.

### 2026-06-18 06:11:16 +08:00: Runtime Ref-to-Def Binding Result Sidecar

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4'`
  - Result before implementation: compile failed because `SZrMetadataTokenBinding` and `SZrFunction.moduleMetadataBindings` did not exist.
- Implementation:
  - `SZrMetadataTokenBinding` records the caller ref token, expected token/hash/module identity, and resolved provider metadata/signature token/hash/module identity.
  - `SZrFunction` owns `moduleMetadataBindings`, initialized and freed with the rest of runtime function metadata.
  - `module_import_signature.c` records or updates a binding only after target module hash, target tokens, target signature hash, and target signature bytes have all been confirmed.
  - Both the normal `moduleEntryEffects` verification path and the direct decoded `moduleMetadataTokenRecords` path share the binding recorder.
  - `module_init_analysis.c` refreshes current source module summary exports from final typed export token/hash/value-type identity after metadata token construction, preventing stale source summaries from producing false guard fallback.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 22 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `zr_vm_metadata_token_model_test`: 6 tests, 0 failures.
  - `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 06:39:26 +08:00: Required Import Target Token Mismatch Diagnostics

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - Result before implementation: `test_required_import_runtime_reports_target_token_mismatch` failed because required import target token drift still reported only hash context and did not include expected/actual token fields.
- Implementation:
  - `SZrModuleImportSignatureMismatch` now carries expected/actual metadata token and signature token fields for member-level token drift.
  - `module_import_signature.c` records those fields when effect/provider target tokens are both present and differ.
  - `module_loader.c` appends token context to required import `import signature mismatch` diagnostics.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 23 tests, 0 failures.
  - Prior adjacent regression remains valid: `zr_vm_metadata_token_model_test` 6 tests, 0 failures; `ctest -R metadata_token_model`: 1/1 passed.

### 2026-06-18 06:59:44 +08:00: Runtime Binding Query Exposure

- TDD red check:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4'`
  - Result before implementation: link failed with undefined reference to `ZrCore_Function_FindModuleMetadataBinding` after the existing binding-result test was changed to use the core query API instead of a private test helper.
- Implementation:
  - `function.h` exposes `ZrCore_Function_FindModuleMetadataBinding(const SZrFunction *function, TZrMetadataToken refToken)`.
  - `function.c` implements a read-only linear lookup over `function->moduleMetadataBindings`, returning `ZR_NULL` for null functions, zero tokens, missing sidecars, or unbound refs.
  - `test_using_import_guard_runtime_records_module_ref_binding_result` now validates the sidecar through the public core API and asserts ref signature, resolved metadata/signature identity, signature hash, and module hash.
- Green checks:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4 && ./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test'`
  - `zr_vm_project_import_canonicalization_test`: 23 tests, 0 failures.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4 && ./build-wsl-clang/bin/zr_vm_metadata_token_model_test && ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure'`
  - `zr_vm_metadata_token_model_test`: 6 tests, 0 failures.
  - `ctest -R metadata_token_model`: 1/1 passed.
- Follow-up target signature slice:
  - Timestamp: 2026-06-17 20:28:55 +08:00.
  - Build: `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test -j1` succeeded after full dependency rebuild.
  - Direct project import test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `6 Tests 0 Failures 0 Ignored OK`.
  - Direct metadata token test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`, including patch 23 import target identity write/read/runtime-copy coverage.
  - CTest: `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` passed 1/1.
  - Type inference identity checks: `test_type_inference_source_import_function_member_preserves_metadata_signature_identity` and `test_type_inference_binary_import_function_member_preserves_metadata_signature_identity` both PASS. Full `zr_vm_type_inference_test` still reports existing `113 Tests 7 Failures 0 Ignored`.
  - Diff hygiene: `git diff --check` on touched metadata/token files exited 0 with only LF/CRLF normalization warnings.
- Signature codec split:
  - Timestamp: 2026-06-17 20:55:47 +08:00.
  - Baseline before edit:
    - `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_metadata_token_model_test zr_vm_project_import_canonicalization_test -j1` built both targets.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `6 Tests 0 Failures 0 Ignored OK`.
  - Implementation: `compiler_metadata_signature.c/.h` now owns `metadata_signature_hash_v1`, type/symbol signature sizing/writing, union signature type recognition, and low-level signature blob writes; `compiler_metadata_token.c` keeps token/RID and import effect orchestration.
  - Size boundary: `compiler_metadata_token.c` reduced from 1103 lines to 811 lines; new `compiler_metadata_signature.c` is 293 lines.
  - Post-split build: `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_metadata_token_model_test -j1` rebuilt `compiler_metadata_signature.c` and linked successfully.
  - Post-split focused tests:
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
    - `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test -j1 && ./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `6 Tests 0 Failures 0 Ignored OK`.
    - `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` passed `1/1`.
- Guard import target hash validation:
  - Timestamp: 2026-06-17 21:13:25 +08:00.
  - RED source: newly added `test_using_import_guard_runtime_rejects_signature_hash_mismatch` initially executed the guarded body and returned provider value `40` instead of fallback `77`, because plugin guard lowering still used the ordinary import helper.
  - Implementation: guard-style `%import` now lowers through `ZrParser_Compiler_EmitImportGuardModuleExpression()` to `ZrCore_Module_ImportGuardNativeEntry`; ordinary `%import` remains on `ZrCore_Module_ImportNativeEntry`.
  - Build: `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test -j1` succeeded.
  - Direct project import test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `7 Tests 0 Failures 0 Ignored OK`, including the target hash mismatch fallback case.
  - Metadata token test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
  - CTest: `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` passed 1/1.
- Native import identity and nested guard validation:
  - Timestamp: 2026-06-17 21:50:33 +08:00.
  - RED source: newly added `test_type_inference_native_import_function_member_preserves_metadata_signature_identity` initially failed because the native `zr.math.sqrt` member had metadata table `0` instead of `MEMBER_DEF`.
  - Implementation: `type_inference_native.c` now assigns synthetic module-member `MEMBER_DEF` / `SIGNATURE` tokens and stable native signature hashes for native module-level functions/constants/modules/types.
  - Runtime implementation: `module_loader.c` now recursively validates child function `moduleEntryEffects`, so nested guarded imports are checked against provider target hashes.
  - Direct project import test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `9 Tests 0 Failures 0 Ignored OK`, including nested caller target hash fallback and unavailable provider fallback.
  - Direct metadata token test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
  - CTest: `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` passed 1/1.
  - Type inference identity check: `test_type_inference_native_import_function_member_preserves_metadata_signature_identity` PASS. Full `zr_vm_type_inference_test` still reports the existing `114 Tests 7 Failures 0 Ignored`.
  - Diff hygiene: `git diff --check` on touched metadata/guard/type-inference files exited 0 with only LF/CRLF normalization warnings.
- Def/ref canonical MethodSig/FieldSig:
  - Timestamp: 2026-06-17 22:13:28 +08:00.
  - RED source: newly added target-hash assertion in `test_project_compile_records_using_import_guard_method_signature` initially failed because provider typed export defs still hashed the older `FUNC` encoding while `MEMBER_REF` appended `METHOD_SIG`.
  - Implementation: `compiler_metadata_signature.c/.h` now provides shared `metadata_token_method_signature_*` and `metadata_token_field_signature_*` helpers; typed exports and import-ref targets both use those bytes.
  - Direct project import test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `9 Tests 0 Failures 0 Ignored OK`.
  - Direct metadata token test: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
  - CTest: `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` passed 1/1.
  - Type inference identity checks for source, binary, and native import members all PASS; full `zr_vm_type_inference_test` still reports the existing `115 Tests 7 Failures 0 Ignored`.
- Guard signature-confirmed runtime validation:
  - Timestamp: 2026-06-17 22:40:36 +08:00.
  - RED source: newly added `test_using_import_guard_runtime_rejects_signature_blob_mismatch_with_matching_hash` initially returned provider value `40` instead of fallback `77` because guard runtime only compared `targetSignatureHash`.
  - Implementation: `module_loader.c` now locates the caller `MEMBER_REF` target sub-signature and compares it against the provider typed export signature blob after hash match.
  - Native coverage: `test_using_import_guard_records_native_target_signature_hash` verifies native `zr.math.abs` guarded effects receive nonzero target signature hash through native compile-info fallback.
  - Focused build and direct tests:
    - `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test -j1` succeeded.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `11 Tests 0 Failures 0 Ignored OK`.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
  - CTest: `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R "metadata_token_model|project_import_canonicalization" --output-on-failure` matched metadata token and passed `1/1`.
  - Type inference identity checks: source, binary, and native import signature identity tests all PASS inside `zr_vm_type_inference_test`; full suite still reports the existing `115 Tests 7 Failures 0 Ignored`.
  - Diff hygiene: `git diff --check` on touched metadata/token/docs/session files exited 0.
- Source/binary same-name import overload candidates:
  - Timestamp: 2026-06-17 23:49:41 +08:00.
  - RED source: newly added source and binary same-name candidate tests first failed with `Expected 2 Was 1`, because import metadata collapsed same-name functions into one member; after retaining candidates they failed again because both overloads reused the first declaration signature/hash.
  - Implementation: `compiler_typed_metadata.c` now resolves exported function declarations by `callableChildIndex` / child function source range; `type_inference_import_metadata.c` retains same-name function candidates and preserves typed export `parameterTypes` while copying runtime/IO parameter metadata; `type_inference_member_resolution.c` adds signature-aware member-call candidate selection, with compile/type-inference call paths using it only when the next member is a call.
  - Build: `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_type_inference_test -j1` completed and built `zr_vm_type_inference_test`.
  - Type inference: `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_type_inference_test` exited 0 and reported `117 Tests 7 Failures 0 Ignored`; new source/binary same-name candidate tests PASS, source/binary/native import mismatch tests PASS, and remaining failures match the existing baseline group.
  - Metadata/token regressions:
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `11 Tests 0 Failures 0 Ignored`.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
    - `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` passed `1/1`.
  - Diff hygiene: `git diff --check` on touched source files exited 0 with only LF/CRLF normalization warnings.

- Required import runtime signature verification:
  - Timestamp: 2026-06-18 00:09:54 +08:00.
  - RED source: newly added `test_required_import_runtime_rejects_signature_hash_mismatch` initially failed with `Expected FALSE Was TRUE`, because ordinary `%import(".helper.math")` ignored the corrupted `targetSignatureHash` and still returned provider value `40`.
  - Implementation: `module_loader.c` renamed the guard-only import verification helpers to generic import helpers and made ordinary `ZrCore_Module_ImportNativeEntry` share the same actual-caller `targetSignatureHash` + target signature blob confirmation path as guard imports.
  - Runtime behavior at this intermediate step: required import mismatch returned null and the following member read failed through the existing runtime error path; the later 2026-06-18 00:48:54 slice replaced this with a structured `import signature mismatch` diagnostic. Guard mismatch still returns null so `using ... else` executes.
  - Build/regression:
    - `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test -j1` succeeded.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` passed `12 Tests 0 Failures 0 Ignored`.
    - `cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_metadata_token_model_test zr_vm_type_inference_test -j1` succeeded.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
    - `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` passed `1/1`.
    - `./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_type_inference_test` still reports the existing `117 Tests 7 Failures 0 Ignored`; import identity/candidate cases remain PASS.

- Required import structured mismatch diagnostics:
  - Timestamp: 2026-06-18 00:48:54 +08:00.
  - Build directory: `build-wsl-clang`.
  - RED source: `test_required_import_runtime_rejects_signature_hash_mismatch` was changed to assert the exception message contains `import signature mismatch`, the canonical module `feature/app/helper/math`, and member `answer`; before implementation the captured exception came from the existing downstream member-access failure path.
  - Implementation: `module_loader.c` records the first import signature mismatch while comparing effect `targetSignatureHash` and target `METHOD_SIG` / `FIELD_SIG` bytes. Ordinary required import restores the native helper frame and raises `ZrCore_Debug_RunError("import signature mismatch: ...")`; guard import still returns null for `using ... else`.
  - Scanner regression fix: `compile_statement.c` now passes the `%import(...)` module name into plugin guard escape scanning. The scanner rejects bare plugin handle and callable member reference escape such as `return math.abs`, while allowing ordinary member values and call results such as `return helper.answer` and `return math.abs(-3.0)`.
  - Build/regression:
    - `cmake --build . --target zr_vm_project_import_canonicalization_test zr_vm_compiler_integration_test -j4` succeeded in `build-wsl-clang`.
    - `./bin/zr_vm_project_import_canonicalization_test` passed `12 Tests 0 Failures 0 Ignored OK`.
    - `./bin/zr_vm_compiler_integration_test` reached and passed `Ownership Builtin Compile Rejects Invalid Operands`; the full binary still later reports existing ownership runtime failures and aborts on an existing assertion.
    - `cmake --build . --target zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test zr_vm_type_inference_test -j4` succeeded.
    - `./bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
    - `ctest -R metadata_token_model --output-on-failure` passed `1/1`.
    - `./bin/zr_vm_type_inference_test` still reports the existing `117 Tests 7 Failures 0 Ignored`.

- Metadata record target token identity:
  - Timestamp: 2026-06-18 01:17:54 +08:00.
  - Build directory: `build-wsl-clang`.
  - RED source: `test_metadata_token_model.c` was changed to assert `MEMBER_REF` and paired `SIGNATURE` records preserve `targetMetadataToken` and `targetSignatureToken`; before implementation the build failed because `SZrMetadataTokenRecord` had no `targetMetadataToken` / `targetSignatureToken` members.
  - Implementation: `metadata_token.h` adds the two record fields; `compiler_metadata_token.c` writes them from explicit effect target tokens or provider summary fallback; `.zro` patch 24 writes/reads the fields in `writer.c` / `io.c`, while patch 23 and older readers default them to 0. Runtime copy keeps them through the existing record copy.
  - Build/regression:
    - `cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4` succeeded.
    - `./build-wsl-clang/bin/zr_vm_metadata_token_model_test` passed `5 Tests 0 Failures 0 Ignored OK`.
    - `ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure` passed `1/1`.
    - `cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4` succeeded.
    - `./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test` passed `12 Tests 0 Failures 0 Ignored OK`.

- Module-level import ref aggregation table:
  - Timestamp: 2026-06-18 02:02:57 +08:00.
  - Build directory: `build-wsl-clang`.
  - RED source: `test_metadata_token_model.c` was extended so duplicate import refs from entry effects and exported callable summary effects must aggregate into one module-level `AssemblyRef` / `TypeRef` / `MemberRef` chain; before implementation the build failed because `SZrFunction` / `SZrIoFunction` had no `moduleMetadataTokenRecords` / `moduleMetadataTokenRecordLength` members.
  - Implementation: `function.h` and `io.h` add `moduleMetadataTokenRecords`; `compiler_metadata_ref.c` builds the table from function-level import ref records and deduplicates by table, owner, hash, target identity, and signature blob bytes; `.zro` patch 25 writes/reads the table in `writer.c` / `io.c`; runtime copy preserves it in `io_runtime.c`.
  - Build/regression:
    - `cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j2` succeeded after implementation.
    - `./build-wsl-clang/bin/zr_vm_metadata_token_model_test` passed `6 Tests 0 Failures 0 Ignored OK`.
    - `cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test zr_vm_project_import_canonicalization_test -j4` succeeded.
    - `./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test` passed `12 Tests 0 Failures 0 Ignored OK`.
    - `ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure` passed `1/1`.
    - `git diff --check` on touched metadata/token/docs/session files reported no whitespace errors; only LF/CRLF conversion warnings.

- Runtime module ref table signature verification:
  - Timestamp: 2026-06-18 02:25:41 +08:00.
  - Build directory: `build-wsl-clang`.
  - RED source: added `test_using_import_guard_runtime_consumes_module_ref_signature_blob`, which corrupts only the module-level `MEMBER_REF` target signature bytes and leaves function-level `metadataTokenRecords` intact; before implementation the guard ignored the module ref table and returned provider value `40` instead of fallback `77`.
  - Implementation: import signature verification moved from `module_loader.c` into `module_import_signature.c/.h`; expected target signature lookup now searches `moduleMetadataTokenRecords` first, then falls back to function-level `metadataTokenRecords` for old patch compatibility.
  - Build/regression:
    - `cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4` succeeded.
    - `./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test` passed `13 Tests 0 Failures 0 Ignored OK`.
    - `cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4` succeeded.
    - `./build-wsl-clang/bin/zr_vm_metadata_token_model_test` passed `6 Tests 0 Failures 0 Ignored OK`.
    - `ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure` passed `1/1`.
    - `git diff --check` reported no whitespace errors; only LF/CRLF conversion warnings.

- Runtime target token binding verification:
  - Timestamp: 2026-06-18 02:36:35 +08:00.
  - Build directory: `build-wsl-clang`.
  - RED source: added `test_using_import_guard_runtime_rejects_target_token_mismatch`, which corrupts only the compiled effect `targetMetadataToken` and leaves target hash/signature bytes intact; before implementation the guard ignored the token mismatch and returned provider value `40` instead of fallback `77`.
  - Implementation: `module_import_signature.c` now checks effect `targetMetadataToken` / `targetSignatureToken` against provider typed export `metadataToken` / `signatureToken` before hash/blob confirmation. The check only fails when both sides have nonzero tokens, preserving old patch/hash-only compatibility.
  - Build/regression:
    - `cmake --build build-wsl-clang --target zr_vm_project_import_canonicalization_test -j4` succeeded.
    - `./build-wsl-clang/bin/zr_vm_project_import_canonicalization_test` passed `15 Tests 0 Failures 0 Ignored OK`.
    - `cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j4` succeeded.
    - `./build-wsl-clang/bin/zr_vm_metadata_token_model_test` passed `6 Tests 0 Failures 0 Ignored OK`.
    - `ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure` passed `1/1`.
    - `git diff --check` reported no whitespace errors; only LF/CRLF conversion warnings.
  - Follow-up coverage: `test_using_import_guard_runtime_rejects_module_ref_target_token_mismatch` now corrupts only a copied `moduleMetadataTokenRecords` entry's target metadata token while leaving function-level records intact; the guard still falls back to `else`, confirming module table target-token drift is covered by the runtime verifier.

- Shared metadata string heap indexing:
  - Timestamp: 2026-06-18 11:42:19 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`.
  - RED source: added `test_metadata_strings_are_indexed_in_shared_heap_through_binary_and_runtime`; initial build failed because `SZrFunction` / `SZrIoFunction` had no `metadataStringHeap` / `metadataStringHeapLength` fields.
  - Implementation: `SZrMetadataStringHeapEntry` now records function-level string index + value entries; `compiler_metadata_token.c` collects module/type/symbol strings; `compiler_metadata_signature.c` and `compiler_metadata_type_spec.c` write fixed `u32` string indexes in signature blobs; `.zro` patch 30 writes the heap after `signatureBlobHeap`; `io.c` / `io_runtime.c` read and copy it; `module_import_signature.c` decodes patch 30 heap-indexed strings and keeps legacy inline string fallback.
  - Build/regression:
    - WSL clang focused command built `zr_vm_metadata_token_model_test` and `zr_vm_project_import_canonicalization_test`; direct runs passed `zr_vm_metadata_token_model_test` 14/0 and `zr_vm_project_import_canonicalization_test` 29/0; metadata CTest passed 1/1.
    - WSL gcc focused command built the same targets; direct runs passed `zr_vm_metadata_token_model_test` 14/0 and `zr_vm_project_import_canonicalization_test` 29/0; metadata CTest passed 1/1.
    - Warning grep over the clang/gcc build logs for touched metadata/import files produced no warnings or errors.
    - `git diff --check` on touched tracked files reported only existing LF/CRLF normalization warnings.

- TypeSpec mismatch status diagnostics baseline:
  - Timestamp: 2026-06-18 12:16:33 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`.
  - RED source: added `test_type_spec_binding_reports_unmatched_caller_signature`; initial build failed because `SZrMetadataTypeSpecBindStatus` and `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus` did not exist.
  - Implementation: `metadata_token.h` adds `SZrMetadataTypeSpecBindStatus`; `function_metadata_binding.c` adds a status-returning TypeSpec binder that reports caller/matched/unmatched counts plus the first unmatched caller token/hash. The existing `ZrCore_Function_BindMatchingTypeSpecMetadata()` wrapper keeps best-effort behavior for import verification.
  - Build/regression:
    - WSL clang focused command built `zr_vm_metadata_token_model_test` and `zr_vm_project_import_canonicalization_test`; direct runs passed `zr_vm_metadata_token_model_test` 15/0 and `zr_vm_project_import_canonicalization_test` 29/0; metadata CTest passed 1/1.
    - WSL gcc focused command built the same targets; direct runs passed `zr_vm_metadata_token_model_test` 15/0 and `zr_vm_project_import_canonicalization_test` 29/0; metadata CTest passed 1/1.
    - Warning grep over the clang/gcc build logs for touched metadata/token files produced no warnings or errors.
    - `git diff --check` on touched tracked files reported only existing LF/CRLF normalization warnings.

- Descriptor plugin safe invalidation/reload baseline:
  - Timestamp: 2026-06-18 12:49:27 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`.
  - RED source: added `tests/library/test_native_registry_descriptor_invalidation.c`; first RED compile failed because `ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE` did not exist, proving the registry had no public in-use failure state for safe invalidation.
  - Implementation: `native_registry.h` adds `ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE`; native binding support adds shared path comparison, live descriptor-plugin record lookup, and in-use error formatting. `ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource()` now refuses to clear module cache, release plugin handles, or remove descriptor plugin records while any affected descriptor plugin module has `ownerRefCount > 0`. `native_registry_load_plugin_descriptor()` also refuses same-name descriptor plugin reload before replacing a live descriptor/handle.
  - Test shape: the new suite uses the real descriptor plugin fixture and public `ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin()` registration path, then simulates a live owner ref on the registered descriptor plugin record. It asserts invalidation returns false with `MODULE_IN_USE`, keeps the module info/source record intact, and succeeds after the simulated owner ref returns to zero.
  - Build/regression:
    - WSL clang focused command built `zr_vm_native_registry_descriptor_invalidation_test`, `zr_vm_metadata_token_model_test`, and `zr_vm_project_import_canonicalization_test`; direct runs passed native registry invalidation 1/0, metadata token 15/0, and project import canonicalization 29/0.
    - WSL gcc focused command built the same targets; direct runs passed native registry invalidation 1/0, metadata token 15/0, and project import canonicalization 29/0.
    - CTest with `native_registry_descriptor_invalidation|metadata_token_model|project_import_canonicalization` passed the registered metadata token and native registry invalidation suites 2/2 on both build directories.
    - Warning grep over clang/gcc build logs for touched native registry/test files produced no warnings or errors.
    - `git diff --check` on touched tracked files reported only existing LF/CRLF normalization warnings.

- Loader-facing TypeSpec mismatch diagnostics:
  - Timestamp: 2026-06-18 13:00:54 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`.
  - RED source: added `test_using_import_signature_reports_typespec_mismatch_diagnostic`; initial run passed import verification but left `moduleLoadDiagnostic` null, so unmatched caller/provider TypeSpec sidecars were invisible to the loader diagnostic channel.
  - Implementation: `module_import_signature.c` now calls `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()` after successful member import verification and ref-to-def binding. If unmatched caller TypeSpec records exist, it records `type_spec_mismatch` in the global module-load diagnostic with canonical module/member context, caller/matched/unmatched counts, and the first unmatched TypeSpec token/hash. Import verification still succeeds, preserving best-effort TypeSpec sidecar semantics.
  - Build/regression:
    - WSL clang focused command built and ran `zr_vm_project_import_canonicalization_test`: 30/0.
    - WSL clang focused command built and ran `zr_vm_metadata_token_model_test`: 15/0.
    - WSL clang focused CTest `metadata_token_model`: 1/1.
    - WSL gcc focused command built and ran `zr_vm_project_import_canonicalization_test`: 30/0.
    - WSL gcc focused command built and ran `zr_vm_metadata_token_model_test`: 15/0.
    - WSL gcc focused CTest `metadata_token_model`: 1/1.
    - A broader rebuild/run of `zr_vm_compiler_integration_test` still failed in existing frame-layout/quickening/import baseline cases; no new TypeSpec diagnostic assertion failed.

- AOT descriptor diagnostics and loader bridge:
  - Timestamp: 2026-06-18 13:33:34 +08:00.
  - Build directory: `build/codex-metadata-token-wsl-gcc-debug`.
  - RED source: added `test_project_import_reports_aot_descriptor_load_error`; strict AOT import of `feature/app/helper/math` without AOT artifacts failed with AOT runtime `lastError`, but global `moduleLoadDiagnostic` was null.
  - Implementation: `ZrLibrary_AotRuntime_ModuleLoader()` forwards descriptor prepare and entry execute failures into core module-load diagnostics as `loader=aot-runtime backend=... result=descriptor-load-failed|module-execute-failed module=... detail=...`. Descriptor validation continues to report field-level ABI/backend/module/entry/blob/thunk failures through AOT runtime `lastError`.
  - Build/regression:
    - WSL GCC build and direct run of `zr_vm_project_import_canonicalization_test`: 31/0.
    - WSL GCC build and direct run of `zr_vm_metadata_token_model_test`: 15/0.
    - WSL GCC build and direct run of `zr_vm_native_closure_value_test`: 3/0.
    - WSL GCC build and direct run of `zr_vm_aot_c_descriptor_diagnostics_test`: 1/0.
    - WSL GCC CTest `aot_c_descriptor_diagnostics|metadata_token_model`: 2/2.

- Local union TypeDef baseline:
  - Timestamp: 2026-06-18 14:09:55 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`.
  - RED source: added `test_union_return_type_emits_type_def_token_record`; before implementation the union export fixture generated `MEMBER_DEF` and `TYPE_SPEC` records but no `TYPE_DEF`, failing with `Expected Non-NULL`.
  - Implementation: added `compiler_metadata_type_def.c/.h`; the metadata token builder plans/emits local union `TYPE_DEF` records by scanning exported return/parameter types, resolving current-script union declarations, deduplicating by base name + generic arity, and writing a paired `SIGNATURE` blob `TYPE_DEF <metadataStringHeap name index> <genericArity>`.
  - Build/regression:
    - WSL clang focused command built `zr_vm_metadata_token_model_test` and `zr_vm_project_import_canonicalization_test`; direct runs passed metadata token 16/0 and project import 31/0; metadata CTest passed 1/1.
    - WSL gcc focused command built the same targets; direct runs passed metadata token 16/0 and project import 31/0; metadata CTest passed 1/1.

- Local union TypeDef variant/field contract:
  - Timestamp: 2026-06-18 14:35:52 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`.
  - RED source: extended `test_union_return_type_emits_type_def_token_record` to parse the TypeDef signature contract. The previous baseline blob ended after `TYPE_DEF <nameIndex> <genericArity>`, so the new `variantCount` read failed.
  - Implementation: `compiler_metadata_type_def.c` now writes `TYPE_DEF <nameIndex> <genericArity> <variantCount> ...`; each union variant writes heap-indexed variant name, variant kind, default-using flag, and field count; each payload field writes heap-indexed field name, passing mode, and canonical TypeSig. `compiler_metadata_type_def_collect_strings()` adds variant/field/type names to the shared metadata string heap before emit.
  - Build/regression:
    - WSL clang focused command built `zr_vm_metadata_token_model_test` and `zr_vm_project_import_canonicalization_test`; direct runs passed metadata token 16/0 and project import 31/0; metadata CTest passed 1/1.
    - WSL gcc focused command built the same targets; direct runs passed metadata token 16/0 and project import 31/0; metadata CTest passed 1/1.
    - MSVC Debug smoke initially reached link and failed because `compiler_build_function_metadata_tokens` was not exported from `zr_vm_parser.dll`; adding `ZR_PARSER_API` to the existing internal declaration/definition fixed the Windows shared-library boundary. Rebuild and direct run passed `zr_vm_metadata_token_model_test` 16/0. Existing MSVC warnings remain in unrelated core/parser files.
    - After the export fix, WSL clang/gcc metadata token rebuilds and direct runs still passed 16/0.

- Local union TypeDef layout identity:
  - Timestamp: 2026-06-18 15:28:05 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`, `build-msvc`.
  - RED source: extended the TypeDef assertions with `layoutVersion` / `layoutHash` and added `test_union_type_def_layout_identity_roundtrips_through_binary_and_runtime`; before implementation `cmake --build build-wsl-clang --target zr_vm_metadata_token_model_test -j 4` failed because `SZrMetadataTokenRecord` had no `layoutVersion` or `layoutHash` members.
  - Implementation: `SZrMetadataTokenRecord` now has record-level `layoutVersion`, reserved space, and `layoutHash`; `.zro` patch 31 writes/reads these fields after `signatureHash`, and patch 30 or earlier reads them as zero. `compiler_metadata_type_def_layout.c/.h` computes the union layout fingerprint from tag size, payload offset/size/align, overall size/align, and each variant payload field offset/size/align. The TypeDef signature blob remains logical-only and does not include physical layout.
  - Build/regression:
    - WSL clang focused build and direct run passed `zr_vm_metadata_token_model_test` 17/0 and `zr_vm_project_import_canonicalization_test` 31/0; metadata CTest passed 1/1.
    - WSL gcc focused build and direct run passed `zr_vm_metadata_token_model_test` 17/0 and `zr_vm_project_import_canonicalization_test` 31/0; metadata CTest passed 1/1.
    - MSVC Debug build and direct run passed `zr_vm_metadata_token_model_test` 17/0. The build system reran CMake because the new helper file changed the source glob.
    - File-size hygiene: the layout helper was split out; `compiler_metadata_type_def.c` is 957 lines and `compiler_metadata_type_def_layout.c` is 256 lines.

- Union TypeSpec to TypeDef definition/layout binding:
  - Timestamp: 2026-06-18 16:11:43 +08:00.
  - Build directories: `build-wsl-clang`, `build-wsl-gcc`, `build-msvc`.
  - RED sources:
    - Added `test_union_type_spec_binding_records_type_def_layout_identity`; initial build failed because `SZrMetadataTokenBinding` had no expected/resolved layout fields.
    - Added `test_union_type_spec_binding_reports_layout_mismatch_without_partial_binding`; the first implementation still wrote a partial TypeSpec binding before detecting TypeDef layout drift.
    - Added `test_union_type_spec_binding_reports_type_def_contract_mismatch_without_partial_binding`; TypeDef signature drift still returned success before the definition check was added.
  - Implementation:
    - `SZrMetadataTokenBinding` now stores expected/resolved layout version/hash fields.
    - `.zro` patch 32 writes and reads those binding layout fields; older patch readers initialize them to zero.
    - `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()` now binds matching union `TYPE_SPEC` records only after the corresponding caller/provider `TYPE_DEF` signature hash/blob and layoutVersion/layoutHash all match.
    - `SZrMetadataTypeSpecBindStatus` now reports definition and layout mismatch counts plus first expected/actual signature/layout context.
    - `module_import_signature.c` includes those definition/layout fields in `type_spec_mismatch` diagnostics.
  - Build/regression:
    - WSL clang focused build and direct run passed `zr_vm_metadata_token_model_test` 20/0 and `zr_vm_project_import_canonicalization_test` 31/0; metadata CTest passed 1/1.
    - WSL gcc focused build and direct run passed `zr_vm_metadata_token_model_test` 20/0 and `zr_vm_project_import_canonicalization_test` 31/0; metadata CTest passed 1/1.
    - MSVC Debug build and direct run passed `zr_vm_metadata_token_model_test` 20/0.
    - File-size hygiene: `function_metadata_binding.c` is 542 lines; `tests/module/test_metadata_token_model.c` is 2152 lines and should be split before further large fixture growth.

- Module ABI TypeDef/TypeSpec identity hash:
  - Timestamp: 2026-06-18 17:08:15 +08:00.
  - Build directories: `build/codex-metadata-type-def-hash-wsl-gcc-debug`, `build/codex-metadata-type-def-hash-wsl-clang-debug`, `build/codex-metadata-type-def-hash-msvc-debug`.
  - RED source: added `test_module_signature_hash_changes_with_union_type_def_contract`. Before implementation, two providers that both exported `choose(): Option<int>` kept the same `moduleSignatureHash` even when local `Option<T>` changed its `TYPE_DEF` variant contract; the export symbol signature hash stayed equal as expected, but module ABI did not drift.
  - Implementation: added `compiler_metadata_module_hash.c/.h`; `compiler_metadata_token.c` now delegates module ABI fingerprint calculation to that helper. The module hash schema is `zr.md.mod.v2\0` and includes provider module version, sorted public typed exports, and sorted `TYPE_DEF` / `TYPE_SPEC` entity record identity: table tag, signature hash, signature blob, layout version, and layout hash. No `.zro` patch was needed because patch 26 already persists `moduleSignatureHash`.
  - Build/regression:
    - WSL GCC focused build and direct run passed `zr_vm_metadata_token_model_test` 21/0 and `zr_vm_project_import_canonicalization_test` 31/0; metadata CTest passed 1/1.
    - WSL clang focused build and direct run passed `zr_vm_metadata_token_model_test` 21/0 and `zr_vm_project_import_canonicalization_test` 31/0; metadata CTest passed 1/1.
    - MSVC Debug build and direct run passed `zr_vm_metadata_token_model_test` 21/0.
    - File-size hygiene: module hash logic is split out; `compiler_metadata_token.c` is 1519 lines, `compiler_metadata_module_hash.c` is 476 lines, and `tests/module/test_metadata_token_model.c` is 2214 lines. Future broad metadata fixture growth should split test helpers first.

- AssemblyRef to MODULE runtime binding sidecar:
  - Timestamp: 2026-06-18 17:36:50 +08:00.
  - Build directory: `build/codex-assemblyref-binding-wsl-gcc-debug`.
  - RED source: extended `test_using_import_guard_runtime_records_module_ref_binding_result` to walk `MEMBER_REF.ownerToken -> TYPE_REF.ownerToken -> ASSEMBLY_REF`, then query `ZrCore_Function_FindModuleMetadataBinding(function, assemblyRefToken)`. Before implementation the guard import still returned provider value `40`, but the AssemblyRef binding query returned null.
  - Implementation: `module_import_signature_binding.c/.h` now owns successful verifier binding sidecar writes. After import verification and the existing member binding succeed, it records an AssemblyRef binding that preserves the caller `ASSEMBLY_REF` token, paired `SIGNATURE` token/hash, and expected provider module ABI hash, and resolves to virtual provider `MODULE` RID 1 with the provider entry function `moduleSignatureHash`. No `.zro` patch was needed because patch 28 already persists the binding list and patch 32 already persists layout fields.
  - Follow-up: the 2026-06-18 18:28:42 slice below replaced the virtual-only MODULE identity with a real provider `MODULE` record and paired `SIGNATURE` when available.
  - Build/regression:
    - WSL GCC focused build and direct run passed `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL GCC focused build and direct run passed `zr_vm_metadata_token_model_test` 21/0.
    - WSL clang focused build and direct run passed `zr_vm_project_import_canonicalization_test` 31/0 and `zr_vm_metadata_token_model_test` 21/0.
    - MSVC Debug focused build/link passed after exporting `zr_module_import_signature_verify`; `zr_vm_metadata_token_model_test` passed 21/0. The full MSVC `zr_vm_project_import_canonicalization_test` still has the existing Windows-only source-loader diagnostic text assertion failure in `test_required_import_runtime_reports_source_loader_attempts`, while the AssemblyRef binding test passes.
    - File-size hygiene: the new binding helper keeps `module_import_signature.c` at 1252 lines and isolates 151 lines of sidecar/TypeSpec diagnostic binding logic.
    - Scope note: current import `TYPE_REF` remains a module-member owner placeholder and is not yet claimed as a stable provider `TYPE_DEF` binding.

- Provider MODULE record and AssemblyRef resolved signature:
  - Timestamp: 2026-06-18 18:28:42 +08:00.
  - Build directories: `build/codex-assemblyref-binding-wsl-gcc-debug`, `build/codex-assemblyref-binding-wsl-clang-debug`, `build/codex-assemblyref-binding-msvc-debug`.
  - RED source: extended `test_metadata_tokens_and_signature_blob_roundtrip_through_binary_and_runtime` so provider metadata must contain `MODULE` RID 1, a paired `SIGNATURE`, and a `ZR_METADATA_SIGNATURE_NODE_MODULE` blob with heap-indexed entry name/module version. Before implementation the focused build failed because `ZR_METADATA_SIGNATURE_NODE_MODULE` did not exist. The project-import binding test was also tightened to require nonzero resolved signature token/hash for caller `ASSEMBLY_REF -> MODULE`.
  - Implementation: added `compiler_metadata_module_record.c/.h`. The token builder now emits one provider `MODULE` record plus a paired `SIGNATURE` record. The MODULE signature blob is `MODULE <entryName> <moduleVersion>` using the metadata string heap and deliberately does not include `moduleSignatureHash`, avoiding recursive module identity. `module_import_signature_binding.c` now resolves AssemblyRef bindings to the provider MODULE paired signature token/hash when the provider record exists; older provider artifacts without MODULE records remain compatible with zero resolved signature identity.
  - Build/regression:
    - WSL GCC focused build and direct run passed `zr_vm_metadata_token_model_test` 21/0 and `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL clang focused build and direct run passed `zr_vm_metadata_token_model_test` 21/0 and `zr_vm_project_import_canonicalization_test` 31/0.
    - MSVC focused build did not reach test execution: `zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c` currently fails to compile on MSVC with `execution_inline_frame_copy_slot_to_nested_field` first implicitly declared and then redefined with a different base type. This file is outside the metadata/token slice.
    - File-size hygiene: provider MODULE record logic is isolated in a 131-line helper; `compiler_metadata_token.c` remains about 1563 lines and should be split before further token-orchestration growth. Metadata/project-import tests are both over 2000 lines and should be split before broad fixture expansion.

- TypeRef to TypeDef consumer-side binding:
  - Timestamp: 2026-06-18 19:02:36 +08:00.
  - Build directories: `build/codex-assemblyref-binding-wsl-gcc-debug`, `build/codex-assemblyref-binding-wsl-clang-debug`, `build/codex-assemblyref-binding-msvc-debug`.
  - RED source: added `tests/module/test_metadata_type_ref_binding.c` and target `zr_vm_metadata_type_ref_binding_test`; the first build failed because `ZrCore_Function_BindMatchingTypeRefMetadata()` did not exist.
  - Implementation: `function_metadata_binding.c` now exposes `ZrCore_Function_BindMatchingTypeRefMetadata()`. It binds only caller `TYPE_REF` records whose `targetMetadataToken` names a provider `TYPE_DEF`, and it requires matching target metadata token, target signature hash, optional target signature token, optional target module signature hash, optional layoutVersion/layoutHash, and TypeRef/TypeDef base name before recording a `moduleMetadataBindings` sidecar. `module_import_signature_binding.c` calls the helper opportunistically after successful import verification. No `.zro` patch was needed because patch 28/32 already persist the binding sidecar and layout fields.
  - Build/regression:
    - WSL GCC focused build and direct runs passed `zr_vm_metadata_type_ref_binding_test` 1/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL clang focused build and direct runs passed `zr_vm_metadata_type_ref_binding_test` 1/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - MSVC Debug focused build and direct run passed `zr_vm_metadata_type_ref_binding_test` 1/0 after importing the Visual Studio developer environment.
    - Scope note: this accepts the consumer-side binding API for stable TypeRef records. Current import `TYPE_REF` records in the member owner-chain remain module-member owner placeholders; the later producer-side slices below cover provider summary backed target `METHOD_SIG` / `FIELD_SIG` TypeSig extraction, nested generic arguments, module-qualified typed local annotations such as `provider.Option<int>`, and destructured/unqualified alias annotations such as `Option<int>`.

- Producer-side stable provider TypeRef generation:
  - Timestamp: 2026-06-18 20:11:43 +08:00.
  - Build directories: `build/codex-metadata-typeref-producer-wsl-gcc-debug`, `build/codex-metadata-typeref-producer-wsl-clang-debug`, `build/codex-metadata-typeref-producer-msvc-debug`.
  - RED source: extended `tests/module/test_metadata_type_ref_binding.c` with `test_import_target_signature_emits_stable_provider_type_ref`. The fixture creates provider `Option<T>` TypeDef metadata and a caller import target signature returning `Option<int>`. Before implementation, the caller token stream had only the existing `AssemblyRef <- TypeRef(symbol owner) <- MemberRef` placeholder and no `TYPE_REF` whose `targetMetadataToken` pointed to the provider `TYPE_DEF`.
  - Implementation:
    - Added `compiler_metadata_type_ref.c/.h` as a producer-side TypeRef plan/emit helper.
    - The metadata token builder keeps existing import owner-chain placeholders, then appends separate stable provider `TYPE_REF` + paired `SIGNATURE` records for provider types found in import target signature return/parameter types.
    - Stable TypeRef records use the provider `ASSEMBLY_REF` as owner and carry provider `TYPE_DEF` token, paired `SIGNATURE` token/hash, provider module ABI hash, `layoutVersion`, and `layoutHash`.
    - `module_init_analysis.c/.h` now stores `SZrParserModuleInitSummary.typeDefs` and refreshes source summaries from final provider metadata token records and the shared metadata string heap after metadata token construction.
  - Build/regression:
    - WSL GCC focused build and direct runs passed `zr_vm_metadata_type_ref_binding_test` 2/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL clang clean configure/build and direct runs passed `zr_vm_metadata_type_ref_binding_test` 2/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0. Existing clang warnings remain in unrelated files.
    - MSVC Debug clean configure/build and direct run passed `zr_vm_metadata_type_ref_binding_test` 2/0 after importing the Visual Studio developer environment. Existing MSVC include/warning noise remains unrelated.
  - Scope note: no `.zro` patch was needed; the records reuse existing metadata target/layout fields and the existing patch 28/32 binding sidecar. Module-qualified explicit typed local annotations are covered by the later 2026-06-18 22:35:57 slice; destructured/unqualified imported type alias source mapping was later covered by the 2026-06-18 23:07:30 slice.

- Nested generic provider TypeDef/TypeRef extraction:
  - Timestamp: 2026-06-18 20:44:02 +08:00.
  - Build directories: `build/codex-metadata-typeref-producer-wsl-gcc-debug`, `build/codex-metadata-typeref-producer-wsl-clang-debug`, `build/codex-metadata-typeref-producer-msvc-debug`.
  - RED source: extended `tests/module/test_metadata_type_ref_binding.c` with `test_nested_generic_import_target_signature_emits_provider_type_ref`. The fixture uses a provider target signature shaped as `Box<Option<int>>`; before implementation the provider TypeDef producer scanned only the top-level type, so no `TYPE_DEF` record was emitted for nested `Option`, and caller stable provider TypeRef production could not bind it.
  - Implementation:
    - `compiler_metadata_type_def.c` now recursively scans array elements and generic argument type names when discovering exported signature TypeDefs.
    - `metadata_type_def_max_candidate_count()` now uses the same recursive type walk, so the unique-entry table has enough capacity for nested provider TypeDefs.
    - `compiler_metadata_type_ref.c` now recursively scans generic argument type names in import target signatures when producing stable provider `TYPE_REF` records.
  - Build/regression:
    - WSL GCC focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 3/0.
    - WSL GCC adjacent direct regressions passed `zr_vm_metadata_token_model_test` 21/0 and `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL clang focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 3/0.
    - MSVC Debug focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 3/0 after importing the Visual Studio developer environment. Existing MSVC include/warning noise remains unrelated.
  - Scope note: no `.zro` patch was needed; this extends the existing provider summary backed target signature producer to nested generic arguments. Module-qualified explicit typed local annotations are covered by the later 2026-06-18 22:35:57 slice; destructured/unqualified imported type alias source mapping was later covered by the 2026-06-18 23:07:30 slice.

- TypeRef binding mismatch status and loader diagnostic:
  - Timestamp: 2026-06-18 21:22:00 +08:00.
  - Build directories: `build/codex-metadata-typeref-producer-wsl-gcc-debug`, `build/codex-metadata-typeref-producer-wsl-clang-debug`, `build/codex-metadata-typeref-producer-msvc-debug`.
  - RED source:
    - Added `test_type_ref_binding_reports_layout_mismatch_without_partial_binding`; the first build failed because `SZrMetadataTypeRefBindStatus` and `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus()` did not exist.
    - Added `test_type_ref_binding_mismatch_records_loader_diagnostic`; before implementation the focused test failed at `Expected Non-NULL` because successful import verification did not record a TypeRef mismatch diagnostic.
  - Implementation:
    - `function_metadata_binding.c` now exposes `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus()`.
    - The status reports caller/matched/unmatched counts, definition mismatch count, layout mismatch count, and first unmatched/definition/layout mismatch expected/actual token/hash/layout context.
    - TypeRef mismatch paths do not write partial `moduleMetadataBindings`; the legacy wrapper remains available for existing callers.
    - `module_import_signature_binding.c` now records `type_ref_mismatch` module-load diagnostics after successful import verification when stable TypeRef binding drifts.
  - Build/regression:
    - WSL GCC focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 5/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL clang focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 5/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - MSVC Debug focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 5/0 after importing the Visual Studio developer environment.
  - Scope note: no `.zro` patch was needed; the slice reuses existing record target/layout fields and patch 28/32 binding sidecar. Module-qualified explicit typed local annotations are covered by the later 2026-06-18 22:35:57 slice; destructured/unqualified imported type alias source mapping was later covered by the 2026-06-18 23:07:30 slice, and provider-summary backed TypeRef mismatch diagnostics are covered.

- Explicit module-qualified type annotation TypeRef surface:
  - Timestamp: 2026-06-18 22:35:57 +08:00.
  - Build directories: `build-wsl-gcc`, `build-wsl-clang`, `build-msvc`.
  - RED source:
    - Added `test_explicit_import_type_annotation_emits_provider_type_ref` with a caller typed local annotation `provider.Option<int>` and no import member effects.
    - Before implementation the caller produced no stable provider `TYPE_REF`; after removing the fake import effect, the old token builder also returned early and had no `ASSEMBLY_REF` owner for the explicit type.
  - Implementation:
    - `compiler_metadata_type_ref.c/.h` now exposes top-level module-qualified type splitting and scans `typedLocalBindings`.
    - The stable TypeRef producer recursively scans generic arguments from typed local annotations and target signatures, resolves provider `TYPE_DEF` identity, and writes provider TypeDef/signature/module/layout target fields.
    - `compiler_metadata_token.c` collects explicit type modules, writes their metadata string heap entries, emits explicit `ASSEMBLY_REF` records even without import member effects, and passes an AssemblyRef RID resolver into TypeRef emit.
  - Build/regression:
    - WSL GCC focused build/direct runs passed `zr_vm_metadata_type_ref_binding_test` 6/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL clang focused build/direct runs passed `zr_vm_metadata_type_ref_binding_test` 6/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - MSVC Debug focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 6/0 after importing the Visual Studio developer environment. Existing MSVC warnings remain unrelated.
  - Scope note: no `.zro` patch was needed; this reuses existing record target/layout fields, metadata string heap, and patch 28/32 binding sidecar. Module-qualified explicit typed local annotations are covered; destructured/unqualified imported type alias source mapping was later covered by the 2026-06-18 23:07:30 slice.

- Destructured/unqualified imported type alias TypeRef surface:
  - Timestamp: 2026-06-18 23:07:30 +08:00.
  - Build directories: `build-wsl-gcc`, `build-wsl-clang`, `build-msvc`.
  - RED source:
    - Added `test_unqualified_import_type_alias_annotation_emits_provider_type_ref` with caller typed local annotation `Option<int>` and a compile-time alias binding `Option -> provider.Option`.
    - Before implementation, the caller produced no stable provider `TYPE_REF` because explicit TypeRef production only consumed module-qualified typed local names.
    - Added `test_nested_unqualified_import_type_alias_annotation_emits_provider_type_ref` for `Box<Option<int>>`, so alias resolution also covers nested generic arguments when the outer generic is not a provider TypeDef.
  - Implementation:
    - `compiler_metadata_type_ref.c/.h` now exposes `compiler_metadata_type_ref_resolve_unqualified_alias()`.
    - The resolver parses unqualified generic bases, resolves the base through `SZrCompilerState.typeValueAliases`, splits the resolved module-qualified type, and rebuilds the member type with the original generic arguments.
    - The TypeRef producer uses this resolver when a typed local annotation has no effect module context and is not module-qualified.
    - `compiler_metadata_token.c` uses the same resolver while collecting metadata string heap entries and explicit TypeRef AssemblyRef modules.
  - Build/regression:
    - WSL GCC focused build/direct runs passed `zr_vm_metadata_type_ref_binding_test` 8/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
    - WSL clang focused build/direct runs passed `zr_vm_metadata_type_ref_binding_test` 8/0, `zr_vm_metadata_token_model_test` 21/0, and `zr_vm_project_import_canonicalization_test` 31/0.
  - MSVC Debug focused build/direct run passed `zr_vm_metadata_type_ref_binding_test` 8/0.
  - Scope note: no `.zro` patch was needed; alias source remains compile-time `typeValueAliases`, and runtime artifacts reuse existing metadata token records, string heap, target/layout fields, and patch 28/32 binding sidecar.

## 2026-06-18 23:45:06 +08:00 - module ABI hash golden and layout canonicalization

- Scope:
  - Added `tests/module/test_metadata_module_hash_golden.c` and CTest suite `metadata_module_hash_golden`.
  - Fixed two module-level ABI fingerprints: `sum(i64,i64)->i64` stays `0xE701BC33ECB6BF89`; local generic union `Option<T>` plus `choose(): Option<int>` stays `0x485AE44EE06010E4`.
  - Canonicalized metadata-only TypeDef layout fingerprint inputs for unknown/generic payload slots and scalar fallback alignment so host `ZR_ALIGN_SIZE` differences do not change `moduleSignatureHash`.
  - Runtime union layout generation and `.zro` schema were not changed.
- Verification:
  - WSL GCC CTest passed `metadata_token_model` 21/0, `metadata_type_ref_binding` 8/0, `metadata_module_hash_golden` 2/0.
  - WSL clang CTest passed `metadata_token_model` 21/0, `metadata_type_ref_binding` 8/0, `metadata_module_hash_golden` 2/0.
  - WSL GCC and WSL clang direct `zr_vm_project_import_canonicalization_test` both passed 31/0.
  - MSVC Debug CTest passed `metadata_token_model`, `metadata_type_ref_binding`, and `metadata_module_hash_golden` 3/3.
- Follow-up note:
  - At this point MSVC direct `zr_vm_project_import_canonicalization_test` reported 31/1 at `test_required_import_runtime_reports_source_loader_attempts`; that source-loader diagnostic portability gap was closed on 2026-06-19 00:41:15 +08:00, and MSVC direct project-import now passes 31/0.

## 2026-06-19 00:12:05 +08:00 - runtime metadata record query surface

- Scope:
  - Added core read-only token record lookup APIs for function-level records and entry-function `moduleMetadataTokenRecords`.
  - Added `tests/module/test_metadata_runtime_query.c` and CTest suite `metadata_runtime_query`.
  - No `.zro` patch or binding sidecar schema change.
- Baseline:
  - RED on WSL GCC: `zr_vm_metadata_runtime_query_test` linked with undefined references to `ZrCore_Function_FindMetadataTokenRecord`, `ZrCore_Function_FindMetadataSignatureRecord`, `ZrCore_Function_FindModuleMetadataTokenRecord`, and `ZrCore_Function_FindModuleMetadataSignatureRecord`.
- Verification:
  - WSL GCC direct `zr_vm_metadata_runtime_query_test` passed 3/0.
  - WSL GCC CTest passed `metadata_token_model`, `metadata_type_ref_binding`, `metadata_runtime_query`, and `metadata_module_hash_golden` 4/4.
  - WSL clang direct `zr_vm_metadata_runtime_query_test` passed 3/0, and the same metadata CTest set passed 4/4.
  - MSVC Debug CTest passed the same metadata set 4/4.
- Boundary coverage:
  - Null function and zero token return null.
  - Function-level records and module ref table records are queried separately.
  - Signature lookup requires both `relatedToken` and `ownerToken` to pair the signature record with the entity token.

## 2026-06-19 00:41:15 +08:00 - source-loader diagnostic path portability

- Scope:
  - `ZrLibrary_Project_SourceLoadImplementation()` now keeps native filesystem paths for actual load attempts, but normalizes diagnostic-only `source=` / `binary=` path display to `/` separators.
  - This closes the MSVC direct project-import diagnostic gap recorded during the module ABI hash golden slice.
  - No metadata token schema, `.zro` patch, resolver, or runtime binding sidecar changed.
- Baseline:
  - RED on MSVC Debug: direct `zr_vm_project_import_canonicalization_test` was 31/1, failing only `test_required_import_runtime_reports_source_loader_attempts` at the `feature/app/helper/math.zr` substring assertion.
- Verification:
  - MSVC Debug direct `zr_vm_project_import_canonicalization_test` passed 31/0.
  - WSL GCC direct `zr_vm_project_import_canonicalization_test` passed 31/0.
  - WSL clang direct `zr_vm_project_import_canonicalization_test` passed 31/0.
  - WSL GCC/clang and MSVC Debug metadata CTest `metadata_token_model|metadata_type_ref_binding|metadata_runtime_query|metadata_module_hash_golden` passed 4/4.
  - `git diff --check` on touched files exited 0 with only LF/CRLF normalization warnings.

## 2026-06-19 00:56:14 +08:00 - metadata/token structure completion review

- Scope:
  - Re-read `docs/plans/using/03-metadata-and-token-model.md`, the typed metadata module document, and the metadata acceptance record against current code/tests.
  - No production code change was needed in this review slice.
- Finding:
  - Current metadata/token structure coverage includes token records, module ref table, binding sidecar, provider MODULE records, TypeSpec/TypeDef identity/layout binding, stable TypeRef producer/binder/status/diagnostic paths, module ABI hash golden coverage, and runtime record query APIs.
  - The early TypeRef consumer-side note that explicit import type references were future syntax was historical; later module-qualified typed local annotation and destructured/unqualified alias annotation slices now cover that stable TypeRef surface.
- Verification:
  - WSL GCC CTest passed `metadata_token_model`, `metadata_type_ref_binding`, `metadata_runtime_query`, and `metadata_module_hash_golden` 4/4.
  - WSL clang CTest passed the same metadata set 4/4.
  - MSVC Debug CTest passed the same metadata set 4/4.

## Acceptance

- Accepted for this slice:
  - `ownerToken` is part of the token record schema.
  - `.zro` patch 22 persists and reads the field with backward compatibility for patch 20/21.
  - Import member effects now have the minimum `AssemblyRef <- TypeRef <- MemberRef` structure needed by later ref-to-def binding.
  - The plan files and typed metadata module document were updated in place with timestamp, status, evidence, and remaining work.
  - Import member refs now carry a target signature hash suitable for later hash-first ref-to-def binding.
  - Guarded source imports can record target MethodSig/FieldSig facts even before full module-level ref binding.
  - Signature codec modularization is accepted as a no-behavior-change refactor; focused metadata tests stayed green before and after the split.
  - Guarded runtime imports consume the compiled target signature hash and take the `else` fallback when the provider hash mismatches.
  - Native compile-time import members now carry token/signature/hash identity in the same `SZrTypeMemberInfo` surface used by source and binary imports.
  - Nested callable guarded imports are covered by runtime target hash fallback.
  - Def/ref canonical `METHOD_SIG` / `FIELD_SIG` encoding is unified for typed exports and import-ref target sub-signatures.
  - Guarded runtime imports compare both target hash and target signature bytes when caller/provider blobs are available.
  - Native guarded imports can record target signature hashes from native compile info.
  - Source/binary import member views retain same-name function candidates instead of name-deduplicating overloads.
  - Member-call inference/compile paths now use signature-aware candidate resolution for typed import members.
  - Typed export overload signatures are selected by callable child identity/source range, not just by name.
  - Required import runtime now rejects target signature hash/blob mismatch instead of silently accepting provider ABI drift.
  - Required import mismatch now reports a structured `import signature mismatch` diagnostic instead of relying on null/member-access failure.
  - Plugin guard escape checks reject callable member reference escape while allowing ordinary member values and member-call results.
  - Metadata token records now preserve target metadata/signature tokens in addition to target signature hashes, and patch 24 roundtrips them through `.zro` and runtime copy.
  - Entry-function `moduleMetadataTokenRecords` now aggregates duplicate import refs from entry effects and exported callable summary effects, and patch 25 roundtrips the table through `.zro` and runtime copy.
  - Runtime import verification now reads target signature bytes from `moduleMetadataTokenRecords` first and falls back to function-level records for old artifacts.
  - Runtime import verification now rejects nonzero target metadata/signature token mismatches before hash/blob confirmation.
  - Module ABI `moduleSignatureHash` now roundtrips through `.zro` patch 26 and runtime copy, stays stable for equivalent exported signatures, and changes when an exported return type changes.
  - Target module ABI `targetModuleSignatureHash` now roundtrips through `.zro` patch 27 for module effects and metadata records, and runtime guard/required import verification rejects nonzero provider module hash drift before member-level checks.
  - Runtime import verification now uses module ref `MEMBER_REF` records to recover missing effect target identity and reject provider drift from the module ref table rather than only from `moduleEntryEffects`.
  - Runtime import verification now binds same-name provider typed exports by target hash/blob/token and no longer accepts the first same-name export when another overload matches the compiled ref.
  - Runtime import verification now records successful ref-to-def binding results in `moduleMetadataBindings`, including the original `MEMBER_REF` token and the resolved provider `MEMBER_DEF` / `SIGNATURE` identity, and exposes them through `ZrCore_Function_FindModuleMetadataBinding()`.
  - Runtime binding results now roundtrip through `.zro` patch 28 via `SZrIoFunction.moduleMetadataBindings` and are queryable after binary load.
  - Future `.zro` patch values are rejected before patch-dependent payload reads and report actual/supported patch values.
  - AssemblyRef default semantic version ranges now roundtrip through `.zro` patch 29 and are verified before runtime module/member ABI binding.
  - Dependency manifest declarations can now override AssemblyRef min/max range with `minVersionInclusive` / `maxVersionExclusive`.
  - Module ABI `moduleSignatureHash` now changes when the provider module version changes, even if exported signatures are otherwise identical.
  - Required import version mismatches now report `assembly_version_mismatch` with `minVersionInclusive`, `maxVersionExclusive`, and `actualVersion`.
  - Required import target token mismatch diagnostics now include expected/actual metadata token and signature token fields.
  - Source summary primitive annotations now use canonical primitive signature nodes, aligning source import refs with provider typed export `METHOD_SIG` blobs.
  - Source module summaries are refreshed from final typed export token/hash/value type identity after metadata token construction.
  - TypeSpec baseline records, generic-instance signatures, local TypeSpec deduplication, and baseline cross-module TypeSpec binding by canonical signature are covered.
  - TypeSpec mismatch status is covered at the core binding layer: unmatched caller TypeSpec records report count, first token, and first signature hash through `SZrMetadataTypeSpecBindStatus`; the existing import-time wrapper remains best-effort.
  - Loader-facing TypeSpec mismatch diagnostics are covered: successful import verification records `type_spec_mismatch` through the module-load diagnostic channel with module/member context, counts, first unmatched token, and first signature hash while preserving best-effort import behavior.
  - Shared string heap indexing is covered at the project-import verification boundary: metadata blob helpers resolve string refs through `SZrFunction.metadataStringHeap` and retain legacy inline string fallback. Verification at 2026-06-18 11:40:20 +08:00: WSL GCC `zr_vm_project_import_canonicalization_test` 29/0, `zr_vm_metadata_token_model_test` 14/0, `zr_vm_gc_test` 66/0.
  - Shared metadata string heap persistence is covered by `.zro` patch 30 and runtime copy: signature blobs write `u32` string indexes, IO roundtrip preserves `metadataStringHeap`, and runtime verifier decodes heap-indexed strings with legacy fallback. Verification at 2026-06-18 11:42:19 +08:00: WSL clang/gcc `zr_vm_metadata_token_model_test` 14/0, `zr_vm_project_import_canonicalization_test` 29/0, metadata CTest 1/1.
  - Descriptor plugin safe invalidation/reload baseline is covered: native registry reports `MODULE_IN_USE` and preserves cache/handle/record state while a descriptor-plugin module has live owner refs, and invalidation succeeds once the refcount returns to zero.
  - AOT descriptor diagnostics are covered: bad descriptors report the first failing ABI/backend/module/entry/blob/thunk field through AOT runtime `lastError`, and module-loader failures forward that detail through the core module-load diagnostic channel.
  - Local union TypeDef baseline and variant/field contract are covered: exported union signatures now produce a `TYPE_DEF` + paired `SIGNATURE` record with heap-indexed type name, generic arity, variant name/kind/default flag/field count, and payload field name/passing mode/TypeSig, without adding a new `.zro` patch.
  - Local union TypeDef layout identity is covered: `TYPE_DEF` records carry record-level `layoutVersion` / `layoutHash`, `.zro` patch 31 roundtrips them through IO/runtime copy, and the physical layout identity remains outside the logical `TYPE_DEF` signature blob.
  - Union TypeSpec to TypeDef definition/layout binding is covered: matching union `TYPE_SPEC` records bind corresponding `TYPE_DEF` definition/layout identity only after signature and layout checks pass; `.zro` patch 32 roundtrips binding layout fields; definition/layout drift reports status/diagnostic context without partial binding.
  - Module ABI TypeDef/TypeSpec identity hash is covered: `moduleSignatureHash` now changes when exported `TYPE_DEF` / `TYPE_SPEC` identity or TypeDef layout changes, even if the public function signature blob and export symbol `signatureHash` stay the same.
  - AssemblyRef runtime binding sidecar is covered: successful import verification now records caller `ASSEMBLY_REF` to provider `MODULE` RID 1 binding with expected/resolved module ABI hash, alongside the existing `MEMBER_REF` to provider `MEMBER_DEF` / `SIGNATURE` binding.
  - Provider MODULE record identity is covered: provider metadata now emits real `MODULE` RID 1 plus paired `SIGNATURE`, and AssemblyRef runtime binding resolves the provider MODULE signature token/hash when available.
  - TypeRef to TypeDef consumer-side binding is covered: caller `TYPE_REF` records that already carry stable provider `TYPE_DEF` target identity now bind to provider `TYPE_DEF` / paired `SIGNATURE` / module/layout identity through `ZrCore_Function_BindMatchingTypeRefMetadata()` and the existing binding sidecar.
  - Producer-side stable provider TypeRef generation is covered: import target signature return/parameter types now produce separate stable provider `TYPE_REF` records with provider TypeDef/signature/module/layout target identity when the provider summary exposes matching `TYPE_DEF` records.
  - Nested generic provider TypeDef/TypeRef extraction is covered: exported/provider and import target signatures now recursively scan generic arguments, so `Box<Option<int>>` can still produce/bind the nested provider `Option` TypeDef/TypeRef identity.
  - TypeRef mismatch status and loader diagnostics are covered: stable provider `TYPE_REF -> TYPE_DEF` binding drift reports unmatched/definition/layout context through `SZrMetadataTypeRefBindStatus`, does not write partial sidecars, and records `type_ref_mismatch` after successful import verification.
  - Module-qualified explicit typed local annotations are covered: `provider.Option<int>` can produce/bind a stable provider `TYPE_REF` and explicit `ASSEMBLY_REF` even without import member effects.
  - Destructured/unqualified imported type alias annotations are covered: `Option<int>` and `Box<Option<int>>` can resolve through `typeValueAliases` such as `Option -> provider.Option` and produce/bind stable provider `TYPE_REF` records plus explicit `ASSEMBLY_REF` owners without import member effects.
  - Module ABI hash golden coverage is accepted: `metadata_module_hash_golden` fixes simple exported function and local generic-union module hashes, and the TypeDef layout fingerprint no longer depends on host `ZR_ALIGN_SIZE` for generic/unknown payload fallback alignment.
  - Runtime metadata record query coverage is accepted at the core layer: function-level token/signature records and entry-function module ref table token/signature records are queryable through read-only APIs, with null/zero-token and loose signature-pair rejection covered.
  - Source-loader diagnostic path portability is accepted: required import unavailable diagnostics include stable `/`-separated source/binary attempted paths on MSVC and WSL, while file access still uses platform-native paths.

- Not yet accepted for full P0:
  - Registered native provider load/verify, required-unavailable diagnostics, project source loader attempted paths, native descriptor plugin load-error passthrough, registry owner refcount, descriptor plugin live-owner safe invalidation, and AOT descriptor field diagnostics are covered.
  - TypeSpec baseline binding, core mismatch status, loader-facing mismatch diagnostics, shared string heap indexing/persistence, local union TypeDef variant/field contract, local union TypeDef layout identity, union TypeSpec→TypeDef definition/layout binding, module ABI TypeDef/TypeSpec identity hash, provider MODULE record/signature, AssemblyRef→MODULE runtime sidecar binding, TypeRef→TypeDef consumer-side binding/status/diagnostic, producer-side stable provider `TYPE_REF` generation from target signatures including nested generic arguments, module-qualified explicit typed local annotations, and destructured/unqualified imported type alias annotations are covered. Remaining work is broader non-metadata using/ownership/loader integration called out in the plans.
  - `compiler_metadata_ref.c` handles aggregation, `compiler_metadata_module_hash.c` handles module ABI hashing, `module_import_signature.c` handles verifier flow, and `module_import_signature_binding.c` records runtime binding sidecars; any additional loader diagnostics should extend verifier/loader/binding boundaries without regrowing `compiler_metadata_token.c`.
