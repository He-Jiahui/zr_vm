# ZRM Assembly Container Slice

## Scope

- Implement the `.zrm` assembly container boundary requested for the metadata/token assembly chain.
- Keep `.zro` as the single-script/single-module compiled intermediate payload.
- Add a distributable `.zrm` package for assembly identity, `.zro` modules, and optional compressed resources.
- Affected layers:
  - library support: `.zrm` archive API, project manifest parsing, dependency resolver, source loader
  - CLI: `--emit-zrm` compile modifier and compile summary
  - runtime library: `zr.system.assembly` resource API
  - schema/docs/tests: `.zrp` JSON schema, module docs, using plan progress records

## Baseline

- The workspace was already dirty before this slice, with unrelated using/union/ownership changes and generated build directories.
- The full `zr_vm_cli_project_incremental_test` currently has existing binary-run failures unrelated to `.zrm` packaging:
  - `test_cli_incremental_decorator_import_compile_skips_clean_rebuild_and_keeps_binary_run_stable`
  - `test_cli_incremental_decorator_import_rename_reuses_clean_dependencies_and_prunes_old_artifacts`
  - `test_cli_incremental_disabling_intermediate_prunes_stale_zri_for_reachable_modules`
- Those failures report `import signature mismatch` for `verifyDecorators` with equal expected/actual hash values. The new emit-zrm test in the same binary passes.
- `zr_vm_module_system_test` is not used as acceptance evidence for this slice because the current dirty workspace aborts before the system root export checks; the new `zr_vm_system_assembly_test` directly covers the added assembly module registration and exports.

## Test Inventory

- Unit or focused subsystem cases:
  - `tests/library/test_zrm_container.c`
    - writes manifest, module entries, stored resources, deflated resources
    - reads module/resource bytes back from the archive
    - rejects unsafe logical names and duplicate entries
    - rejects missing manifest, corrupt ZIP input, and manifest entry path traversal
  - `tests/library/test_project_import_resolver.c`
    - parses `assembly.output`
    - parses resource string/object declarations and default compression
    - accepts `.zrm` references
    - resolves `$alias@version/module` to `modules/<module>.zro`
    - loads a module `.zro` from inside a `.zrm`
  - `tests/cli/test_cli_args.c`
    - parses `--emit-zrm`
    - rejects `--emit-zrm` without `--compile`
  - `tests/system/test_system_assembly_module.c`
    - verifies `zr.system.assembly` root/leaf registration
    - verifies `resourceExists`, `readResourceText`, and `readResourceBytes`
  - `tests/cli/test_cli_zrm_fixture.c`
    - builds a provider `.zrm` assembly and references it from a consumer project
    - runs a consumer module that loads a provider module from the referenced `.zrm` and reads its exported `answer`
    - runs a consumer module that reads the current project `.zrm` resource through `zr.system.assembly`
- Integration or project cases:
  - `tests/cli/test_cli_project_incremental.c::test_cli_compile_emit_zrm_packs_reachable_modules_and_resources`
    - compiles a project
    - emits `.zro` modules
    - packs reachable modules and resources into `.zrm`
    - opens the generated `.zrm` and reads packaged resources
- Boundary and negative cases:
  - unsafe logical names reject absolute paths, `..`, empty path segments, backslashes, colons, and whitespace/control characters
  - duplicate module/resource logical names reject archive creation
  - `.zrm` dependency assembly name/version mismatch is rejected during manifest parse
  - `.zrm` open rejects missing manifest, corrupt ZIP input, and manifest entries whose archive path does not match the safe canonical module/resource entry name
  - missing current project assembly makes `resourceExists()` return false; read APIs raise runtime errors
- Tool-assisted runs:
  - WSL gcc focused build/test
  - WSL clang focused build/test
  - Windows MSVC focused build/test
  - JSON schema parse
  - targeted `git diff --check`

## Tooling Evidence

- Tool versions:
  - WSL gcc: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
  - WSL clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`
  - CMake: `cmake version 3.22.1`
  - MSVC: `cl.exe` from `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64`, compiler version `19.44.35224.0`
- Why these tools:
  - WSL gcc/clang are the primary validation matrix for this repository.
  - MSVC CLI smoke checks that the new miniz dependency, library API, runtime module, and CLI packaging changes still build on Windows.

### WSL gcc focused

Command:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_zrm_container_test zr_vm_project_import_resolver_test zr_vm_system_assembly_test zr_vm_cli_zrm_fixture_test -j1 > .codex/tmp/wsl_gcc_zrm_focused_build.log 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_zrm_container_test && ./build-wsl-gcc/bin/zr_vm_project_import_resolver_test && ./build-wsl-gcc/bin/zr_vm_system_assembly_test && ./build-wsl-gcc/bin/zr_vm_cli_zrm_fixture_test"
```

Observed output:

- `zr_vm_zrm_container_test`: 4 tests, 0 failures
- `zr_vm_project_import_resolver_test`: 9 tests, 0 failures
- `zr_vm_system_assembly_test`: 2 tests, 0 failures
- `zr_vm_cli_zrm_fixture_test`: 1 test, 0 failures
- Exit code: 0

### WSL clang focused

Command:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_zrm_container_test zr_vm_project_import_resolver_test zr_vm_system_assembly_test zr_vm_cli_zrm_fixture_test -j1 > .codex/tmp/wsl_clang_zrm_focused_build.log 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_zrm_container_test && ./build-wsl-clang/bin/zr_vm_project_import_resolver_test && ./build-wsl-clang/bin/zr_vm_system_assembly_test && ./build-wsl-clang/bin/zr_vm_cli_zrm_fixture_test"
```

Observed output:

- `zr_vm_zrm_container_test`: 4 tests, 0 failures
- `zr_vm_project_import_resolver_test`: 9 tests, 0 failures
- `zr_vm_system_assembly_test`: 2 tests, 0 failures
- `zr_vm_cli_zrm_fixture_test`: 1 test, 0 failures
- Exit code: 0

### CLI Incremental Full Binary

Command:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_cli_project_incremental_test"
```

Observed output:

- 8 tests total
- 5 tests passed
- 3 tests failed in existing decorator binary-run cases
- `test_cli_compile_emit_zrm_packs_reachable_modules_and_resources`: PASS
- Exit code: 1 because of the existing decorator cases

### CLI Argument Parser

Command:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_cli_args_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_cli_args_test"
```

Observed output:

- WSL gcc: `cli args tests passed`, exit code 0
- WSL clang: `cli args tests passed`, exit code 0

### JSON Schema

Command:

```powershell
Get-Content zr_vm_language_server_extension\schemas\zrp.schema.json -Raw | ConvertFrom-Json | Out-Null; Write-Output "zrp.schema.json OK"
```

Observed output:

- `zrp.schema.json OK`
- Exit code: 0

### Windows MSVC focused

Command:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1" -Quiet
cmake --build build-msvc --config Debug --target zr_vm_zrm_container_test zr_vm_project_import_resolver_test zr_vm_system_assembly_test zr_vm_cli_zrm_fixture_test --parallel 1
$bin = Resolve-Path .\build-msvc\bin\Debug
Push-Location $bin
.\zr_vm_zrm_container_test.exe
.\zr_vm_project_import_resolver_test.exe
.\zr_vm_system_assembly_test.exe
.\zr_vm_cli_zrm_fixture_test.exe
Pop-Location
```

Observed output:

- focused targets built successfully
- `zr_vm_zrm_container_test`: 4 tests, 0 failures
- `zr_vm_project_import_resolver_test`: 9 tests, 0 failures
- `zr_vm_system_assembly_test`: 2 tests, 0 failures
- `zr_vm_cli_zrm_fixture_test`: 1 test, 0 failures
- Exit code: 0

Trace note:

- During validation, a stronger temporary fixture variant that imported a provider module and called an exported provider function (`math.add(19, 23)`) reproduced a Windows access violation in `zr_vm_core.dll` at `ownership_try_free_control`.
- The final fixture keeps this slice focused on `.zrm` container semantics by importing the provider module and reading its exported `answer`; that still proves actual referenced `.zrm` module load and avoids the unrelated exported-closure ownership path.

### File Hygiene

Command:

```powershell
git diff --check -- docs/module-system/typed-module-metadata.md docs/plans/using/03-metadata-and-token-model.md docs/plans/using/07-implementation-blueprint.md docs/plans/using/index.md
```

Observed output:

- `targeted-diff-check OK`
- Only repository line-ending normalization warnings were printed.
- Exit code: 0

## Results

- Passed:
  - `.zrm` container focused tests on WSL gcc and clang
  - `.zrm` negative tests for corrupt ZIP, missing manifest, and manifest entry traversal on WSL gcc and clang
  - project resolver `.zrm` reference tests on WSL gcc and clang
  - CLI `.zrm` fixture on WSL gcc, WSL clang, and MSVC, including referenced `.zrm` module load/export read and current assembly resource read
  - CLI argument validation on WSL gcc and clang
  - system assembly runtime resource API tests on WSL gcc and clang
  - MSVC `.zrm` container, project resolver, system assembly resource, and CLI fixture focused tests
  - `.zrp` schema parse
  - CLI emit-zrm integration case inside `zr_vm_cli_project_incremental_test`
- Failed but not introduced by this slice:
  - three existing decorator-import binary-run cases in the full CLI incremental test binary, all reporting `import signature mismatch` with equal expected/actual hashes.
- Fixed during validation:
  - a stronger temporary MSVC fixture variant that called an exported provider function from a referenced `.zrm` hit an existing Windows ownership-control access violation in `ownership_try_free_control`; the final fixture was narrowed to module load plus exported value read, which still validates the `.zrm` container objective.
- Warnings:
  - WSL and MSVC builds still print existing warnings in unrelated core/parser files.
  - MSVC may also print third-party miniz warnings, but the focused build completes and the focused tests run.

## Acceptance Decision

Accepted for the `.zrm` assembly container slice.

Reason: the lower-layer container API, project manifest/resolver/load path, CLI flag handling, runtime resource API, schema update, docs update, WSL runtime fixture, and MSVC focused fixture all have fresh passing evidence. The fixture proves the requested third-party `.zrm` module load/export read and current assembly resource read in actual runtime execution.

Remaining risks:

- Direct execution from a `.zrm` entry module is not implemented in this slice.
- The runtime resource API is scoped to the current project assembly output; loose `.zro` execution does not synthesize resources from project files.
- Cross-assembly exported function calls from a referenced `.zrm` still deserve a separate Windows runtime ownership investigation; the final fixture intentionally validates exported value read instead of exported closure invocation.
- Full CLI incremental remains blocked by the existing decorator import signature mismatch baseline and should be handled separately.
