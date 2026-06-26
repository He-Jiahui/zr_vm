# AOT 11-S7V / 12-S3F / 12-S4N / 08-S7K Manifest Generic MethodSpec Binding

Timestamp: 2026-06-25 06:26:16 +08:00

## Scope

- Bind `.zrp` `kind: "generic"` preserve roots whose target names a current-module exported method to an existing writer-visible MethodSpec-shaped signature record.
- Carry MethodSpec identity through `SZrAotManifestGenericRoot` as method-spec token, open method token, and instantiation signature hash.
- Let full-AOT closure accept MethodSpec-bound generic method roots without requiring TypeSpec/generic type-instantiation identity.

## RED

- Added `test_cli_aot_writer_options_binds_generic_method_preserve_to_method_spec`.
- First focused WSL gcc build failed because `SZrAotManifestGenericRoot` did not have `hasMethodSpecBinding`, `methodSpecToken`, `methodSpecMethodToken`, or `methodSpecSignatureHash`.

## GREEN

- Added MethodSpec identity fields to the AOT writer generic root carrier.
- The CLI AOT preserve bridge now matches `GENERIC_INST(MEMBER_REF methodToken, args...)` signatures against current-module typed exported method symbols.
- Generated AOT C manifest diagnostics now report MethodSpec token, method token, and signature hash.
- Full-AOT generic-root closure accepts roots that have MethodSpec binding.

## Verification

- WSL gcc direct: `zr_vm_cli_aot_writer_options_test` 14/0.
- Focused CTest matrix: WSL gcc, WSL clang, and Windows MSVC Debug all passed `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model` 5/5.

## Notes

- This is a current-module writer-visible MethodSpec binding slice.
- It does not export a persistent zrp MethodSpec table, collect generic method code bodies transitively, bind cross-module generic method targets, or complete the full mark-and-sweep generic closure.
