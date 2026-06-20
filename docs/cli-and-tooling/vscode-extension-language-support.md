---
related_code:
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/syntaxes/zr.tmLanguage.json
  - zr_vm_language_server_extension/scripts/build-native-server.js
  - zr_vm_language_server_extension/scripts/package-vsix.ps1
  - zr_vm_language_server_extension/scripts/sync-native-server.js
  - zr_vm_language_server_extension/scripts/sync-wasm-server.js
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/nativeAssets.ts
  - zr_vm_language_server_extension/src/debug/configProvider.ts
  - zr_vm_language_server_extension/src/debug/dapSession.ts
  - zr_vm_language_server_extension/src/debug/cliLauncher.ts
implementation_files:
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/syntaxes/zr.tmLanguage.json
  - zr_vm_language_server_extension/scripts/build-native-server.js
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/nativeAssets.ts
  - zr_vm_language_server_extension/src/debug/configProvider.ts
  - zr_vm_language_server_extension/src/debug/dapSession.ts
  - zr_vm_language_server_extension/src/debug/cliLauncher.ts
plan_sources:
  - user: 2026-06-20 完善 union 后构建 VSIX plugin，并确认 union 高亮、成员解析、debug 功能状态
  - docs/plans/using/04-union-types.md
  - docs/plans/using/06-syntax-and-semantic-checks.md
tests:
  - zr_vm_language_server_extension/test/syntaxGrammar.test.js
  - zr_vm_language_server_extension/test/extensionContributions.test.js
  - zr_vm_language_server_extension/test/dapSessionBreakpoints.test.js
  - zr_vm_language_server_extension/test/nativeAssets.test.js
  - tests/debug/test_debug_variable_child_shape.c
  - zr_vm_language_server_extension/package.json
doc_type: module-detail
---

# VSCode Extension Language Support

This page records the VSCode extension surface that sits around the native language server: TextMate grammar, contributed commands, debug adapter registration, native asset packaging, and VSIX build behavior.

## Grammar Surface

`syntaxes/zr.tmLanguage.json` owns editor-side lexical highlighting before semantic tokens arrive. It now covers the Rust-like union surface:

- `union` is highlighted as a declaration keyword.
- Union declaration names such as `union Shape` are highlighted as type declarations.
- Lowercase primitive aliases such as `int`, `float`, `bool`, `string`, and `void` are highlighted as support types.
- A default union variant marker such as `@Available` is highlighted as a variant declaration marker and variant type name.
- Variant declaration names such as `Circle`, `Rect`, and `Empty` are highlighted as union variant names when they appear in a union body.
- Variant payload field names such as `radius:`, `width:`, and `height:` are highlighted as field declarations.
- Variant member references such as `.Some` and `.Rect` are highlighted as member variant names.

This grammar is intentionally lexical. It does not decide whether `.Some` names a valid variant; parser/compiler/LSP semantic analysis remains responsible for member resolution, payload binding, diagnostics, and type inference.

## Debug Surface

Desktop extension activation registers the `zr` debug type, launch/attach commands, and an inline debug adapter:

- `zr.debugCurrentProject`
- `zr.debugSelectedProject`
- `zr.attachDebugEndpoint`
- launch configuration provider for `.zrp` projects
- inline DAP adapter backed by `src/debug/dapSession.ts`
- CLI launch helper that starts `zr_vm_cli` with `--debug`, `--debug-address`, `--debug-wait`, and `--debug-print-endpoint`

These debug commands use the same native CLI asset resolution path as run/project actions. When the bundled `server/native/win32-x64/zr_vm_cli.exe` is refreshed, debug launch uses that fixed executable unless the user explicitly configures `zr.debug.cli.path`.

Union values now have a debug-specific runtime shape below the VSIX adapter:

- inline union locals and runtime union carrier objects preview as `<union Type.Variant>`
- expanding a union value returns `variant` first, followed by payload fields named from union metadata
- `evaluate` can read synthetic members such as `circle.variant`, `circle.radius`, `rect.width`, and `rect.height`

The VSIX does not implement this logic in TypeScript. It is provided by the bundled `zr_vm_debug.dll` and exposed through the existing variables/evaluate protocol path.

## Native And VSIX Packaging

`scripts/build-native-server.js` builds and verifies native assets for the extension. Before invoking CMake/MSVC it removes the extension `node_modules/.bin` directory from `PATH`; otherwise the npm `rc` package can shadow the Windows resource compiler `rc.exe` and make native packaging fail during resource compilation.

The VSIX payload includes:

- native Windows CLI and language server binaries under `server/native/win32-x64`
- native runtime/debug DLL dependencies
- web/WASM language server assets under `out/web`
- TextMate grammar and `.zrp` schema
- compiled extension JavaScript
- debug contribution metadata from `package.json`

For the 2026-06-20 union slice, the generated package was:

- `zr_vm_language_server_extension/zr-vm-language-server-0.0.1-union.vsix`
- SHA256: `977DB3F214A6978CEE04D8A0F0E8C9AF61AFC91A36BE58D14BE60CA06D610C0E`
- Final native assets were synced from `build/codex-msvc-union-final-debug/bin` after that build passed the focused union debug regression and the using checkout project fixtures.

## Verification

The extension-side validation for this slice includes:

- `npm run compile`
- `npm run test:unit`
- grammar unit assertions for `union`, primitive type names, `@Available`, `.Some`, and `.Rect`
- grammar unit assertions for union declaration names, variant names, and payload field declaration names
- focused MSVC debug regression `test_debug_protocol_expands_union_variant_payloads`
- focused WSL debug regression `test_debug_protocol_expands_union_variant_payloads`
- VSIX content inspection for the grammar, native CLI, and WASM asset
- bundled CLI smoke against `tests/fixtures/projects/using_real_world_checkout/using_real_world_checkout.zrp`
- bundled CLI smoke against `tests/fixtures/projects/using_real_world_checkout/using_union_normal.zrp`
- bundled CLI smoke against `tests/fixtures/projects/using_real_world_checkout/using_union_edge_cases.zrp`
