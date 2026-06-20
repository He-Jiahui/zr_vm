---
related_code:
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_system/include/zr_vm_lib_system/assembly.h
  - zr_vm_lib_system/include/zr_vm_lib_system/assembly_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_language_server_extension/schemas/zrp.schema.json
implementation_files:
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
plan_sources:
  - user: 2026-06-19 .zro 保留单脚本中间文件，metadata/token 程序集语义链新增独立 .zrm 容器
  - docs/plans/using/03-metadata-and-token-model.md
  - docs/plans/using/07-implementation-blueprint.md
  - docs/plans/using/index.md
tests:
  - tests/library/test_zrm_container.c
  - tests/library/test_project_import_resolver.c
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_project_incremental.c
  - tests/cli/test_cli_zrm_fixture.c
  - tests/system/test_system_assembly_module.c
  - tests/module/test_module_system.c
  - tests/acceptance/2026-06-19-zrm-assembly-container.md
doc_type: module-detail
---

# ZRM Assembly Container

## Purpose

`.zro` now remains the compiled artifact for one script module. It still carries the typed metadata, metadata token records, signature blob heap, module reference table, and runtime binding sidecar for that module.

`.zrm` is the assembly-level container. It groups one or more `.zro` modules, assembly identity, the entry module, and optional resources into a single distributable file that can be referenced as a third-party module assembly, similar to the role a DLL or JAR plays for a library package.

## Container Layout

The container is a ZIP archive written by `ZrLibrary_Zrm_WriteArchive()` and read by `ZrLibrary_Zrm_Open()`.

- `META-INF/zrm.json`: manifest with `format: "zr.zrm/v1"`, assembly identity, entry module, module entries, and resource entries.
- `modules/<module-key>.zro`: compiled module bytes. Module keys are slash-separated logical names such as `ops/sum`.
- `resources/<logical-name>`: optional resource bytes. Logical names use safe slash-separated names such as `config/default.txt`.

Modules are stored without compression. Resources can be stored or deflated; `.zrp` resources default to compression unless `compress: false` is specified.

Logical names reject absolute paths, `..`, empty path segments, backslashes, colons, and control whitespace. This keeps archive entries independent from host paths and prevents container traversal.

## Project Manifest Surface

Project manifests can declare assembly output and resources:

```json
{
  "assembly": {
    "name": "zr.math",
    "version": "2.1.0",
    "output": "dist/zr.math.zrm"
  },
  "source": "src",
  "binary": "bin",
  "entry": "main",
  "resources": {
    "config/default.txt": {
      "path": "resources/default.txt",
      "compress": true
    }
  }
}
```

If `assembly.output` is omitted, `ZrLibrary_Project_ResolveAssemblyOutputPath()` resolves the default to `<binary>/<assembly-name>.zrm` relative to the project manifest directory.

`references.<alias>.path` accepts either a `.zrp` project manifest or a `.zrm` assembly. A `.zrm` reference is opened during manifest parse, validated against the declared `assembly` and optional `version`, and then used to resolve imports such as `$mathLocal@2.1.0/ops/sum` to `modules/ops/sum.zro` inside the archive.

## CLI Packaging

`zr_vm_cli --compile --emit-zrm <project.zrp>` first compiles the reachable module graph into `.zro` files under the configured binary root, then packages those modules and declared resources into the project assembly container.

The compile summary records:

- `packedAssembly = true` when packaging succeeds.
- `zrmPath` as the resolved output file.

`--emit-zrm` is a compile-only modifier. It is rejected without `--compile`, and it does not change `.zro` generation.

## Runtime Resource API

`zr.system.assembly` exposes the current project assembly resource helpers:

- `resourceExists(name: string): bool`
- `readResourceText(name: string): string`
- `readResourceBytes(name: string): array`

The implementation resolves the current project from `SZrGlobalState.userData`, resolves the current assembly output path with `ZrLibrary_Project_ResolveAssemblyOutputPath()`, opens the `.zrm`, and reads resources by logical name. If no current project assembly exists, `resourceExists()` returns `false`; read functions raise a runtime error.

This is intentionally project-assembly scoped. `.zro` execution without an emitted `.zrm` does not synthesize resources from loose files.

## Test Coverage

- `tests/library/test_zrm_container.c` verifies manifest writing, module/resource entry names, compression mode, byte extraction, duplicate rejection, unsafe logical name rejection, missing manifest rejection, corrupt ZIP rejection, and manifest entry path traversal rejection.
- `tests/library/test_project_import_resolver.c` verifies `assembly.output`, project resources, `.zrm` references, `$alias@version/module` resolution, and loading a module `.zro` from inside the container.
- `tests/cli/test_cli_args.c` verifies `--emit-zrm` parsing and compile-only validation.
- `tests/cli/test_cli_project_incremental.c` verifies `--emit-zrm` packages reachable modules and resources and that the resulting `.zrm` can be opened and read.
- `tests/cli/test_cli_zrm_fixture.c` builds a provider `.zrm`, references it from a consumer project, runs a consumer module that imports the provider module from the referenced assembly and reads its exported `answer`, then runs a second consumer module that reads the current project `.zrm` resource through `zr.system.assembly`.
- `tests/system/test_system_assembly_module.c` verifies `zr.system.assembly` registration and current-project resource text/byte reads.
- `tests/module/test_module_system.c` tracks the `zr.system` root export list so the new `assembly` leaf remains visible through native module metadata.

## Open Issues

- `.zrm` currently packages compiled `.zro` bytes and resources; it does not introduce a new metadata-token schema beyond the `.zro` module payloads.
- Runtime project execution still starts from the project entry `.zro` file. Referenced `.zrm` modules can be loaded by the project loader, but direct launching of a `.zrm` entry module is a separate packaging/runtime entrypoint.
- The container manifest is JSON inside ZIP and not yet signed or authenticated. Assembly identity and hashes are validation metadata, not a security boundary.
