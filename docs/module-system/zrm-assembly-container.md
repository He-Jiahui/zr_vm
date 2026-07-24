---
related_code:
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.h
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c
  - zr_vm_library/src/zr_vm_library/project/project_import_provider_location.c
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
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c
  - zr_vm_library/src/zr_vm_library/project/project_import_provider_location.c
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
  - docs/plans/syntax/10-native-ffi-module-package/m2-manifest-artifact-implementation-plan.md
tests:
  - tests/library/test_zrm_container.c
  - tests/library/test_project_import_resolver.c
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_project_incremental.c
  - tests/cli/test_cli_zrm_fixture.c
  - tests/system/test_system_assembly_module.c
  - tests/module/test_module_system.c
  - tests/acceptance/2026-06-19-zrm-assembly-container.md
  - tests/acceptance/2026-07-02-aot-11-s7zj-provider-import-location-discovery.md
  - tests/acceptance/2026-07-02-aot-11-s7zk-provider-aot-load-request.md
  - tests/acceptance/2026-07-02-aot-11-s7zl-provider-aot-runtime-load-request.md
  - tests/acceptance/2026-07-02-aot-11-s7zm-provider-aot-dynamic-library-success.md
  - tests/acceptance/2026-07-02-aot-11-s7zn-provider-version-selection-range-guard.md
  - tests/library/test_project_import_aot_provider_runtime.c
  - tests/library/test_project_import_provider_version_selection.c
  - tests/library/test_project_manifest_v2.c
  - tests/parser/test_aot_c_provider_shared_library_smoke.c
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

### V2 Base Admission

Syntax 10R M2.1 additionally admits a versioned project envelope with `manifestVersion: 2`. A v2 manifest must
provide nonempty `name`, `version`, `kind`, `source`, `binary`, and `entry` strings; the parsed project retains
its manifest version instead of treating it as an untyped JSON detail. Missing `manifestVersion` and explicit
version 1 remain the migration-reader path. Fractional, unsupported, or incomplete v2 envelopes fail before
project lifecycle parsing.

### V2 Declaration Admission

Syntax 10R M2.2 now admits v2 `aliases`, `package`, and `dependencies` as identity-only declarations. Aliases
retain their `#` root and parsed target domain; package exports are explicit `.` / `./logical.module` mappings;
and every dependency has a root `@package`, version requirement, and exactly one structured `path`, `registry`,
or `git` source. Alias targets pointing at packages must name the project package or a declared dependency.

This does not select a provider, open a dependency path, fetch a registry or git source, infer unexported
submodules, choose a `.zrm` default entry, or emit a lock. V2 rejects legacy `pathAliases`, `references`,
`dependency`, and `local` fields rather than translating old `$`/`&` behavior into the new declaration layer.
M2.3 provides canonical writer and lock projection; M2.4 adds artifact entries and provider phase checks.

### V2 Canonical Writer And Lock Projection

`ZrLibrary_ProjectManifestV2_Write` writes only an already-complete v2 envelope. It serializes base fields in a fixed
order and sorts aliases, package exports, and dependencies by their canonical identity keys. Its output uses only v2
`#alias` and `@package` spellings, and it rejects v1 projects or any machine-local locator rather than publishing an
environment-specific source location. `path` stays relative; `registry` admits a package ID or a non-loopback HTTP(S)
URI; and `git` admits a non-loopback HTTP(S), SSH, or git URI with an authority. Empty-authority and loopback URI
forms are not publishable.

Resolved dependency state is not fed back into `.zrp`. Instead,
`ZrLibrary_ProjectManifestV2_WriteDependencyLock` accepts a lock entry for each declared dependency and emits its
resolved version, content hash, transitive identity, and source-kind provider fact. Each entry must use the exact
declared `@package` identity and provider kind. The lock API intentionally has no locator field, so it cannot write a
machine-local cache path. Artifact default-entry selection and actual provider loading remain M2.4 responsibilities.

`references.<alias>.path` accepts either a `.zrp` project manifest or a `.zrm` assembly. A `.zrm` reference is opened during manifest parse, validated against the declared `assembly` and optional `version`, and then used to resolve imports such as `$mathLocal@2.1.0/ops/sum` to `modules/ops/sum.zro` inside the archive. When the actual provider version and declared min/max are strict `major.minor.patch` values, manifest parsing also rejects invalid or out-of-range `[min, max)` declarations for both `.zrp` and `.zrm` references.

### Artifact Entry And Provider Phase

Syntax 10R M2.4 adds provider facts to the `.zrm` assembly record without expanding module identity. The optional
`assembly.providerPhase` is one of `runtime`, `test`, or `compileTool`; old `zr.zrm/v1` manifests without it load as
`runtime`. `assembly.publicContractHash` is an optional artifact contract fact. Unknown, empty, null, or non-string
phases reject the archive. The declared default `entry` must name exactly one entry in `modules`; both writer and
reader reject an archive whose default entry cannot be opened as a `.zro` payload.

`ZrLibrary_Project_ResolveImportProviderLocation()` returns the selected provider kind, phase, exact archive entry,
and public contract hash in addition to the canonical provider module key. The matching AOT request carries the same
facts. They are deliberately separate from `SZrLibrary_ModuleIdentity`: identity identifies the logical module, while
the entry, phase, and hash describe the selected artifact. Runtime source loading and AOT request construction reject
non-runtime archive providers before opening the selected `.zro`; no filename or locator fallback can reinterpret a
CompileTool or Test provider as Runtime.

`ZrLibrary_Project_ResolveImportProviderLocation()` is the AOT-facing discovery API for referenced providers. It resolves the same import specifier to the canonical provider module key plus the declared assembly identity/version range. For `.zrm` references it returns the open archive and module entry; for `.zrp` project references it returns source, binary, and intermediate module paths. Exact aliases can point at multiple versions of the same assembly and resolve to distinct canonical provider keys. This is location discovery only: automatic range-based candidate selection remains separate.

`ZrLibrary_Project_ResolveImportProviderAotLoadRequest()` converts that provider location into a loader-facing request record. For `.zrp` references it carries the backend kind, descriptor-local module name, source/binary/intermediate module paths, and the backend-specific dynamic-library path under `aot_c` or `aot_llvm`. For `.zrm` references it mirrors the archive and entry pointers and deliberately leaves `libraryPath` empty so archive entries are not treated as filesystem dynamic libraries. This is request planning only; runtime dynamic loading is still a later AOT/runtime stage.

Strict AOT module loading now consumes that request for canonical provider imports. For `.zrp` references, the runtime uses the provider project's AOT library path and validates the descriptor against the provider-local module name while keeping the canonical `$alias@version/module` cache key. `.zrm` archive entries remain unsupported as dynamic libraries at this layer and fail closed with an explicit diagnostic.

The generated provider dynamic-library success path is covered by `tests/parser/test_aot_c_provider_shared_library_smoke.c`: it builds a provider `.zrp` module into the exact `bin/aot_c/lib/zrvm_aot_<module>.so` path, imports the canonical provider key through strict AOT C, and verifies the canonical cache entry plus exported value publication. This confirms `.zrp` provider filesystem artifacts work; `.zrm` archive entries still require a separate archive-aware AOT loader design.

`tests/library/test_project_import_provider_version_selection.c` covers exact alias/version selection for multiple `.zrp` references to the same assembly and the strict declared range guard shared by `.zrp` and `.zrm` references. It does not implement automatic candidate search across installed provider versions.

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

- `tests/library/test_zrm_container.c` verifies manifest writing, module/resource entry names, compression mode, byte extraction, duplicate rejection, unsafe logical name rejection, missing manifest rejection, corrupt ZIP rejection, manifest entry path traversal rejection, provider-phase round-trip/defaulting, and unknown-phase rejection.
- `tests/library/test_project_import_resolver.c` verifies `assembly.output`, project resources, `.zrm` references, `$alias@version/module` resolution, provider-location discovery and AOT load-request planning for `.zrp` and `.zrm` references, provider entry/phase/hash facts, Runtime-versus-CompileTool rejection, and loading a Runtime module `.zro` from inside the container.
- `tests/library/test_project_import_provider_version_selection.c` verifies multi-version `.zrp` provider exact alias/version selection and strict declared range rejection.
- `tests/library/test_project_manifest_v2.c` verifies canonical v2 manifest ordering/read-write-read equivalence,
  migration, local-locator/loopback writer rejection, and separate dependency-lock projection without source locators.
- `tests/cli/test_cli_args.c` verifies `--emit-zrm` parsing and compile-only validation.
- `tests/cli/test_cli_project_incremental.c` verifies `--emit-zrm` packages reachable modules and resources and that the resulting `.zrm` can be opened and read.
- `tests/cli/test_cli_zrm_fixture.c` builds a provider `.zrm`, references it from a consumer project, runs a consumer module that imports the provider module from the referenced assembly and reads its exported `answer`, then runs a second consumer module that reads the current project `.zrm` resource through `zr.system.assembly`.
- `tests/system/test_system_assembly_module.c` verifies `zr.system.assembly` registration and current-project resource text/byte reads.
- `tests/module/test_module_system.c` tracks the `zr.system` root export list so the new `assembly` leaf remains visible through native module metadata.

## Open Issues

- `.zrm` currently packages compiled `.zro` bytes and resources; it does not introduce a new metadata-token schema beyond the `.zro` module payloads.
- Runtime project execution still starts from the project entry `.zro` file. Referenced `.zrm` modules can be loaded by the project loader, but direct launching of a `.zrm` entry module is a separate packaging/runtime entrypoint.
- The container manifest is JSON inside ZIP and not yet signed or authenticated. Assembly identity and hashes are validation metadata, not a security boundary.
