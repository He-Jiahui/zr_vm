# AOT 10-S5N / 11-S4S / 12-S5M Runtime Bound TypeRef Layout Resolver

Time: 2026-06-30 18:17:22 +08:00

Status: complete for the attached/current-runtime bound TypeRef layout resolver sub-slice.

Completed:
- `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` now accepts a `TYPE_REF` token when the attached metadata runtime has a matching `SZrMetadataTokenRecord`.
- The TypeRef record must target a `TYPE_DEF` token that the same runtime can resolve through the existing TypeDef layout binding view.
- The resolver validates optional target signature token/hash, optional target module signature hash, and layout version/hash before accepting the binding.
- Successful TypeRef layout resolution is cached under the TypeRef token, so later lookups do not depend on the registration slot still being present.
- `tests/module/test_metadata_runtime_typespec_layout.c` now has focused positive, cache, layout identity mismatch, and module identity mismatch coverage.

RED/GREEN:
- RED 1: the new TypeRef positive test failed because `ResolveTypeTokenLayout()` returned NULL for `TYPE_REF` tokens.
- GREEN 1: the resolver accepted a bound TypeRef -> TypeDef record and returned the TypeDef layout/id.
- RED 2: the module identity mismatch test failed because `targetModuleSignatureHash` was not checked.
- GREEN 2: mismatched module identity now rejects the TypeRef layout and clears the output layout id.

Validation:
- WSL GCC direct/focused: `metadata_runtime_typespec_layout` 17/0, `metadata_runtime_query` 24/0, `reflection_token_resolve` 7/0, `metadata_type_ref_binding` 8/0.
- WSL GCC CTest: `metadata_runtime_typespec_layout|metadata_runtime_query|reflection_token_resolve|metadata_type_ref_binding` 4/4.
- WSL Clang direct/focused: `metadata_runtime_typespec_layout` 17/0, `metadata_runtime_query` 24/0, `reflection_token_resolve` 7/0, `metadata_type_ref_binding` 8/0.
- WSL Clang CTest: same focused set 4/4.
- Windows MSVC Debug direct/focused: `metadata_runtime_typespec_layout` 17/0, `metadata_runtime_query` 24/0, `reflection_token_resolve` 7/0, `metadata_type_ref_binding` 8/0.
- Windows MSVC Debug CTest: same focused set 4/4.
- `git diff --check` exited 0 with only LF/CRLF warnings for touched files.

Transient note:
- One first WSL GCC combined CTest run reported `metadata_type_ref_binding` wrapper `No such file or directory`.
- Direct binary execution, the same wrapper invocation, single-test CTest, and a later combined CTest all passed. This was treated as a transient wrapper/process issue, not a code assertion failure.

Not claimed:
- Cross-module provider lookup, provider runtime context selection, or provider version compatibility.
- Independent runtime scanning of embedded zrp `TOKEN_RECORDS` without attached function metadata.
- FieldInfo object materialization or field value read/write.
- `@dynamically_accessed` dataflow, warning policy, DESCRIPTION promotion, or complete metadata sweep.
