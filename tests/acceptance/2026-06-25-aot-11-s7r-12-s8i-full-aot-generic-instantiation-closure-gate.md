# 2026-06-25 AOT 11-S7R / 12-S8I / 12-S3B / 08-S7G full-AOT generic instantiation closure gate

## Scope

- Tighten full-AOT manifest generic preserve closure validation from "has TypeSpec binding" to "has TypeSpec binding and generic instantiation identity".
- Treat a TypeSpec-only `SZrAotManifestGenericRoot` as incomplete for full-AOT generation, even when the root carries TypeSpec token/signature metadata.
- Keep hybrid behavior unchanged: text/TypeSpec diagnostics can still be emitted without claiming a closed full-AOT generic instance.

## RED

- Added `test_cli_aot_writer_options_rejects_typespec_only_generic_preserve_root_in_full_aot`.
- Initial WSL gcc run failed because `ZrParser_Writer_WriteAotCFileWithOptions()` still returned true for a full-AOT generic root with `hasTypeSpecBinding = true` and `hasGenericInstantiationBinding = false`.

## GREEN

- `backend_aot_manifest_generic_roots_closed_for_full_aot()` now requires both `hasTypeSpecBinding` and `hasGenericInstantiationBinding` for every manifest generic preserve root when `requireFullAot` is true.
- Existing CLI materialization path remains valid because TypeSpec-bound `.zrp` generic preserve roots now receive `genericInstantiationBaseToken`, `genericInstantiationInstanceId`, and `genericInstantiationShareKind` before writer generation.

## Verification

- WSL gcc direct: `zr_vm_cli_aot_writer_options_test` 10/0.
- WSL gcc CTest: `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model` 5/5.
- WSL clang CTest: same focused set 5/5.
- Windows MSVC Debug CTest: same focused set 5/5.

## Notes

- This closes only the writer-side full-AOT gate for manifest generic roots that are TypeSpec-bound but not materialized as generic instances.
- Open generic base-token resolution, MethodSpec binding, missing TypeSpec synthesis, cross-module generic targets, annotation roots, reflection-driven generic construction, and complete mark-and-sweep generic closure remain future work.
