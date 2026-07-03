# AOT 10-S5M / 12-S5L Bound TypeRef Dynamic Dependency Layout Root

Time: 2026-06-30 17:56:52 +08:00

Status: complete for the current embedded zrp bound-TypeRef sub-slice.

Completed:
- `dynamicDependencyTypeToken` now accepts a `TYPE_REF` token when embedded zrp `TOKEN_RECORDS` has a unique matching `SZrMetadataTokenRecord`.
- The TypeRef record must point `targetMetadataToken` at a current blob `TYPE_DEF`.
- The target TypeDef row supplies the retained `typeLayoutId`, preserving generated type-layout descriptor, registration entry, stats, root marker, and root-only token table fallback after the original function is trimmed.
- Source and type-layout contracts cover `TYPE_REF`, `TOKEN_RECORDS`, `SZrMetadataTokenRecord`, and `record->targetMetadataToken`.

RED/GREEN:
- RED: the TypeRef generated-C fixture failed because the writer rejected unsupported `TYPE_REF` roots.
- GREEN: `dynamicDependencyTypeToken = 0x05000001` resolves via `targetMetadataToken = 0x02000001` to `typeLayoutId = 2`; `zr_aot_fn_2` is trimmed while `ZrTypeLayout_2` and `zr_aot_type_layout_tokens[2] = 0x02000001u` remain.

Validation:
- WSL GCC: code stripping 10/0, source contracts 24/0, type-layout contracts 1/0, focused CTest 2/2, global smoke 10/0, call smoke 5/0, dynamic deopt smoke 7/0.
- WSL Clang: code stripping 10/0, source contracts 24/0, type-layout contracts 1/0, focused CTest 2/2, global smoke 10/0, call smoke 5/0, dynamic deopt smoke 7/0.
- Windows MSVC Debug: code stripping 10/0, source contracts 24/0, type-layout contracts 1/0, focused CTest 2/2; Windows smoke binaries returned OK with 10/5/7 ignored and 0 failures.
- `git diff --check` exited 0 with only LF/CRLF warnings.

Not claimed:
- Cross-module provider loading or version compatibility.
- Runtime TypeRef-to-layout resolution.
- FieldInfo object materialization or field value read/write.
- `@dynamically_accessed` dataflow, warning policy, DESCRIPTION promotion, or complete metadata sweep.
