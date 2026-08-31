# VS Code Extension WASM Package Build

## Scope

- Compile and package the `zr-vm-language-server` VS Code extension from the current `main` checkout.
- Repair the web/WASM language-server source aggregation needed by VSIX packaging.
- Affected layers: CMake build graph, built-in iteration/container registration, parser test-manifest support, Emscripten output, and extension packaging.

## Baseline

- `npm run compile` and the incremental MSVC native language-server build completed during the first packaging attempt.
- `npm run build:wasm` failed while compiling `zr_vm_lib_container/module.c` because `zr_vm_lib_iteration/module.h` was absent from the WASM include path.
- After adding the iteration dependency, the next WASM link exposed `ZrParser_TestManifest_Encode`, `ZrParser_TestManifest_Decode`, and `ZrParser_TestManifest_Free` as undefined because the production `test_contract.c` source was removed by a broad `.*test.*` filter.
- The known repository-wide C/C++ test baseline from `zr-vm-dev` was not used as evidence for this extension-only packaging regression.

## Test Inventory

- Focused lower-layer build: `npm run build:wasm`.
- Extension TypeScript/browser worker build: `npm run compile` through the VSIX prepublish path.
- Native server and CLI build plus built-in smoke checks: `npm run build:native` through the VSIX prepublish path.
- WSL gcc and clang native LSP/CLI builds plus `hello_world` CLI smoke checks.
- Full native/WASM asset synchronization and VSIX packaging: `scripts/package-vsix.ps1`.
- Extension unit suite: `npm run test:unit`.
- Package boundary checks: new VSIX timestamp, archive content for native and WASM assets, file size, and SHA-256.
- Negative regression checks: missing iteration include and missing parser test-manifest symbols must no longer occur.

## Tooling Evidence

- Node.js `v22.13.1`, npm `11.1.0`, and dependencies installed with `npm ci` from `package-lock.json`.
- MSVC `14.44.35207` selected through `E:\Visual Studio\Common7\Tools\VsDevCmd.bat`.
- WSL Emscripten `4.0.23`, CMake `3.22.1`, and Ninja `1.10.1` used for the Release WASM target.
- Baseline command: `npm run build:wasm`.
  - Exit 1 at `zr_vm_lib_container/module.c:6` with a missing `zr_vm_lib_iteration/module.h` diagnostic.
- First post-change command: `npm run build:wasm`.
  - Reached final linking, then exited 1 with the three missing `ZrParser_TestManifest_*` symbols.
- Focused green command: `npm run build:wasm`.
  - Exit 0 after compiling `test_contract.c` and linking `wasm/zr_vm_language_server.js`.
- Full package command: `$env:VSDEVCMD_PATH = 'E:\Visual Studio\Common7\Tools\VsDevCmd.bat'` followed by `scripts/package-vsix.ps1 -SkipNpmInstall -Jobs 8`.
  - Exit 0 after TypeScript compilation, native CLI/LSP smoke checks, incremental WASM verification, asset synchronization, and `vsce package`.
- Unit command: `npm run test:unit`.
  - Exit 0 with 30 passed, 0 failed, 0 skipped.
- WSL gcc command: configure `build/codex-wsl-gcc-debug` with gcc 11.4.0, then build `zr_vm_language_server_stdio` and `zr_vm_cli_executable`.
  - Exit 0 after 779 build steps; the CLI smoke exited 0 and printed `hello world`.
- WSL clang command: configure `build/codex-wsl-clang-debug` with clang 14.0.0, then build `zr_vm_language_server_stdio` and `zr_vm_cli_executable`.
  - Exit 0 after 779 build steps; the CLI smoke exited 0 and printed `hello world`.
- Package inspection used `System.IO.Compression.ZipFile` and `Get-FileHash`.
  - The VSIX contains 379 entries and all eight required extension, native, and web/WASM assets.
  - Size: 15,942,785 bytes.
  - SHA-256: `E31EE3742B6A2A9D7988AAE7B09049FA5A4A2513DFED3C8D0DAA830FFA19200E`.

## Results

- PASS: the focused WASM target includes `zr_vm_lib_iteration` sources and headers.
- PASS: parser production sources with `test` in their names remain in the WASM source set.
- PASS: the focused WASM target links successfully.
- PASS: complete VSIX packaging with native and WASM assets.
- PASS: extension unit suite, 30/30.
- PASS: package boundary inspection, 0 required assets missing.
- PASS: WSL gcc and clang native LSP/CLI builds and CLI execution smokes.
- WARN: `npm ci` reported 14 dependency audit findings: 1 low, 5 moderate, and 8 high.
- WARN: `vsce` reported that the extension root lacks a packaged license file and contains many unbundled JavaScript files.
- WARN: CMake emitted its existing Emscripten shared-library-to-static developer warnings.

## Acceptance Decision

- Accepted for the requested extension compile and packaging scope.
- Desktop and web end-to-end editor smoke suites were not required for this compile-only request and were not run.
- Dependency audit findings and packaging-efficiency warnings remain follow-up work; neither prevented deterministic compilation or required-asset packaging.
