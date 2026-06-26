# AOT 11-S7N / 12-S4H / 08-S7C Manifest Generic Preserve Writer Roots

Timestamp: 2026-06-25 03:27:16 +08:00

## Scope

- Connect parsed `.zrp` generic preserve declarations to the AOT writer surface.
- Preserve rule shape covered: `kind: "generic"`, `target: "List"`, `arguments: ["Foo", "Bar.Baz"]`.
- This slice intentionally stops at writer options and generated-C manifest diagnostics.

## RED

- Added `test_cli_aot_writer_options_bind_generic_preserve_arguments_to_writer_options`.
- Initial WSL gcc focused build failed because:
  - `SZrCliAotPreserveRoots` had no `genericRoots` / `genericRootCount`.
  - `SZrAotWriterOptions` had no `manifestPreserveGenericRoots` / `manifestPreserveGenericRootCount`.

## GREEN

- Added `SZrAotManifestGenericRoot`.
- Added `manifestPreserveGenericRoots` and `manifestPreserveGenericRootCount` to `SZrAotWriterOptions`.
- Extended `SZrCliAotPreserveRoots` to own generic root carrier arrays while borrowing project string text.
- `ZrCli_Compiler_ApplyProjectAotPreserveRules()` now forwards feature-matched generic preserve target and arguments into writer options.
- `backend_aot_c_emitter.c` now emits:
  - `manifest.genericRoots`
  - `manifest.genericRoot[i] target=... argumentCount=...`
  - `manifest.genericRoot[i].argument[j] = ...`

## Validation

- WSL gcc: `zr_vm_cli_aot_writer_options_test` 6/0.
- WSL clang: `zr_vm_cli_aot_writer_options_test` 6/0.
- Windows MSVC Debug: `zr_vm_cli_aot_writer_options_test` 6/0.
- WSL gcc/clang and Windows MSVC Debug: CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed` 3/3.
- `python -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json` passed.
- `git diff --check` exited 0 with only LF/CRLF warnings.

## Notes

- This closes only the manifest generic preserve writer-root bridge.
- Still open: MethodSpec/TypeSpec token resolution, generic instantiation table materialization, mark-and-sweep generic closure, cross-module generic targets, annotation roots, and default-minimal metadata output.
