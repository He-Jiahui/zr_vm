# .zrp Assembly Configuration Design

## Status

- Date: 2026-06-19
- Decision: add `assembly` and `references` to `.zrp`, while preserving compatibility for existing top-level `name`, `version`, and `dependencies`.
- Implementation status: design only. No production code change is approved by this document alone.
- Review gate: implementation planning starts only after this design is reviewed.

## Problem

Current `.zrp` project configuration already describes local layout and dependency packages:

- local identity fields: `name`, `version`
- project layout fields: `source`, `binary`, `entry`
- local path alias field: `pathAliases`
- dependency fields: `dependencies`, `dependency`, `local`

The metadata/token model already has the lower-layer concepts needed for typed cross-module access:

- provider `MODULE` records
- provider `moduleVersion`
- provider `moduleSignatureHash`
- `ASSEMBLY_REF` rows with requested, minimum, and maximum version fields
- binding sidecars such as `ASSEMBLY_REF -> MODULE`, `TYPE_REF -> TYPE_DEF`, and `MEMBER_REF -> MEMBER_DEF`

The missing piece is a stable `.zrp` manifest surface that can express assembly-like identity and references directly, similar to how C# assemblies separate:

- the current assembly's identity
- the referenced assemblies' identities
- the source alias used by code
- the physical path used by the loader

Without that layer, cross-module access can only infer identity from dependency aliases and paths. That is enough for simple imports, but weak for version checks, same-assembly/different-version scenarios, runtime binding diagnostics, and future binary metadata validation.

## Goals

- Define a first-class assembly identity for the current `.zrp` project.
- Define explicit references from one project assembly to other project assemblies.
- Keep existing `.zrp` projects working through compatibility mapping.
- Align manifest fields with the existing metadata/token model instead of introducing a parallel module system.
- Preserve the existing dependency package scope behavior: each referenced `.zrp` owns its own `source`, `binary`, `entry`, `pathAliases`, and references.
- Allow one source project to reference the same real assembly name at different versions through different local aliases.
- Make source import aliases stable and separate from real assembly identity.
- Make runtime validation able to report clear diagnostics for identity mismatch, version mismatch, signature mismatch, missing reference, and access denial.
- Keep implementation incremental across project loading, import resolution, metadata emission, runtime binding, schema/LSP, and tests.

## Non-Goals

- Do not implement network package restore or package registries.
- Do not implement C# strong-name cryptographic validation in this round. `publicKeyToken` is identity text for now.
- Do not add new source syntax for imports in this design.
- Do not make transitive references source-visible by default.
- Do not replace existing `pathAliases`; they remain local path aliases, not assembly references.
- Do not redesign visibility rules. Cross-assembly access still depends on existing typed metadata visibility and exported symbols.
- Do not force old `.zrp` files to move to the new shape immediately.

## Current Evidence

Repository evidence:

- `zr_vm_language_server_extension/schemas/zrp.schema.json` already defines the current `.zrp` surface and allows additional fields.
- `zr_vm_library/include/zr_vm_library/project.h` already has project, dependency package, and dependency reference structures.
- `zr_vm_library/src/zr_vm_library/project_import_resolver.c` already resolves dependency imports such as `&math.aaa.bbb` into canonical dependency module keys.
- `.codex/plans/Import Relative And ZRP Alias Path Plan.md` defines alias and canonical module key behavior.
- `.codex/plans/zrp dependencies 第三方封装模块管理计划.md` defines dependency package scoping, `$name@version/path` canonical keys, and dependency cycle handling.
- `docs/plans/using/03-metadata-and-token-model.md` and `docs/module-system/typed-module-metadata.md` define the current metadata/token architecture with `MODULE`, `ASSEMBLY_REF`, version fields, signature hash, and runtime binding sidecars.

Reference-language evidence:

- Mono has explicit `AssemblyRef` metadata and assembly reference loading/verification paths.
- JDK module descriptors separate module identity, `requires`, and exports, and keep invalid descriptor diagnostics explicit.
- Rust crate loading separates source crate names from explicit `--extern name=path` resolution and validates upstream identity/hash.
- QuickJS delegates module-name resolution to the host loader, which fits `zr_vm` resolving `.zrp` references before runtime binding.

## Manifest Shape

New `.zrp` projects should use this shape:

```json
{
  "manifestVersion": 1,
  "assembly": {
    "name": "app.render",
    "version": "1.2.0",
    "culture": "neutral",
    "publicKeyToken": null,
    "kind": "application"
  },
  "source": "src",
  "binary": "bin",
  "entry": "main",
  "pathAliases": {
    "@app": "app",
    "@shared": "shared"
  },
  "references": {
    "math": {
      "assembly": "zr.math",
      "version": "1.0.0",
      "path": "deps/math/math.zrp",
      "minVersionInclusive": "1.0.0",
      "maxVersionExclusive": "2.0.0"
    },
    "plugins": {
      "assembly": "app.plugins",
      "path": "deps/plugins/plugins.zrp"
    }
  }
}
```

The `references` object key is the source alias. There is no separate `alias` property in v1. This avoids two sources of truth.

## Assembly Fields

`manifestVersion`

- Optional for compatibility.
- Defaults to `1` when absent.
- Future incompatible manifest changes must increment this value.

`assembly.name`

- Required in the new canonical form.
- Stable assembly identity for metadata.
- Maps to the provider `MODULE` identity.
- Must not be inferred from file paths once explicitly declared.

`assembly.version`

- Optional in syntax but normalized to a concrete value.
- Defaults from top-level `version` when absent.
- Defaults to `0.0.0` only when neither new nor old version fields exist.
- Maps to provider `moduleVersion`.

`assembly.culture`

- Optional.
- Defaults to `neutral`.
- Stored as assembly identity metadata for future compatibility.
- Runtime validation may ignore it until the metadata record has a stable field for it, but the project loader must preserve it.

`assembly.publicKeyToken`

- Optional string or `null`.
- Defaults to `null`.
- Treated as identity text in this round.
- If present, it should be normalized as lowercase hexadecimal text by the project loader.

`assembly.kind`

- Optional.
- Allowed values for v1: `library`, `application`.
- Defaults to `library`.
- Does not change import or metadata token semantics in v1; it is a manifest-level declaration for tooling and future packaging.

## Reference Fields

Each `references` entry is keyed by a local source alias:

```json
{
  "references": {
    "mathV1": {
      "assembly": "zr.math",
      "version": "1.0.0",
      "path": "deps/math-v1/math.zrp"
    },
    "mathV2": {
      "assembly": "zr.math",
      "version": "2.0.0",
      "path": "deps/math-v2/math.zrp"
    }
  }
}
```

`alias`

- The object key, such as `mathV1`, is the only source alias.
- Source imports use this alias through existing dependency import syntax, for example `&mathV1.vector`.
- Aliases must be unique in the normalized project.

`assembly`

- Required in the new canonical form.
- Declares the target assembly identity expected at `path`.
- A referenced `.zrp` whose normalized `assembly.name` differs from this field is rejected.

`version`

- Optional requested target version.
- Maps to `ASSEMBLY_REF.requestedVersion`.
- If absent, range fields may still constrain the target.

`minVersionInclusive`

- Optional lower version bound.
- Maps to the existing minimum version field in `ASSEMBLY_REF`.

`maxVersionExclusive`

- Optional upper version bound.
- Maps to the existing maximum version field in `ASSEMBLY_REF`.

`path`

- Required for v1 project references.
- Points to the referenced `.zrp` file.
- Resolved relative to the current `.zrp` file directory.
- The loader must normalize the path and reject paths that cannot be opened.

## Compatibility Mapping

Existing `.zrp` files remain valid:

```json
{
  "name": "app.render",
  "version": "1.2.0",
  "source": "src",
  "binary": "bin",
  "entry": "main",
  "dependencies": {
    "$math": {
      "path": "deps/math/math.zrp",
      "version": "1.0.0"
    }
  }
}
```

The project loader normalizes this into the new internal model:

- `assembly.name` comes from top-level `name` when `assembly.name` is absent.
- `assembly.version` comes from top-level `version` when `assembly.version` is absent.
- `dependencies.$math` becomes a reference with alias `math`.
- `dependencies.$math.path` becomes `references.math.path`.
- `dependencies.$math.version` becomes `references.math.version`.
- If an old dependency entry has `assembly` or `name`, that value becomes the declared target assembly.
- If an old dependency entry has no declared target assembly, the loader treats it as path-first compatibility mode and fills the assembly identity from the referenced `.zrp` after loading it.

Conflict rules:

- If both `references.math` and `dependencies.$math` exist and normalize to identical values, the loader keeps one reference.
- If they normalize to different values, the loader reports `zrp_reference_conflict`.
- New `references` entries should not start with `$`.
- Old `dependencies` entries may keep the existing `$` prefix.

## Resolution Rules

The source alias and real assembly identity are deliberately separate:

- `math` is the local source alias.
- `zr.math` is the real assembly name.
- `deps/math/math.zrp` is the physical project manifest path.

Imports resolve through the local alias first:

- `&math` resolves to the referenced project's entry module.
- `&math.vector` resolves to module path `vector` inside the referenced project's source root.
- `@alias` values remain local to the current project or referenced project that declares them.
- `zr.*` intrinsic/global imports keep their current resolver behavior.

Canonical module keys keep the current dependency-key pattern, but the `$name` portion now means the local reference alias:

- `&math.vector` in the caller becomes `$math@1.0.0/vector`.
- `&mathV1.vector` becomes `$mathV1@1.0.0/vector`.
- `&mathV2.vector` becomes `$mathV2@2.0.0/vector`.

The real assembly name is stored in metadata and binding records, not encoded as the only canonical key. This is what permits different aliases for the same real assembly name at different versions in one caller.

## Cross-Module Access

A reference grants the compiler and loader permission to resolve modules from that target assembly. It does not bypass typed metadata access rules.

The access model is:

- Source code can import modules only from direct references declared in the current `.zrp`.
- A referenced assembly's own dependencies are loaded to satisfy that referenced assembly, but they are not source-visible to the caller unless the caller also declares them directly.
- Public exported symbols are accessible across assemblies.
- Internal or assembly-private symbols are accessible only within the same normalized `assembly.name` and compatible identity.
- Failed access is reported as an access diagnostic, not as a generic missing symbol.

Assembly-level export lists are deferred. v1 uses existing symbol/module visibility metadata as the access boundary.

## Metadata And Token Mapping

The normalized `.zrp` manifest is the source of truth for metadata emission.

Provider assembly mapping:

- `assembly.name` maps to the provider `MODULE` record identity.
- `assembly.version` maps to provider `moduleVersion`.
- Publicly visible provider surface contributes to `moduleSignatureHash`.
- `assembly.culture` and `assembly.publicKeyToken` are preserved in the normalized project model and should be added to the module identity payload when the metadata record has fields for them. Until then, they are loader-side identity attributes.

Reference mapping:

- Each normalized reference produces one `ASSEMBLY_REF` row for the consuming module when the reference is used.
- `references[alias].assembly` maps to the assembly reference name.
- `references[alias].version` maps to requested version.
- `minVersionInclusive` and `maxVersionExclusive` map to the existing version range fields.
- The resolved target provider's `moduleSignatureHash` maps to `ASSEMBLY_REF.targetModuleSignatureHash`.

Type and member mapping:

- Imports that cross an assembly boundary attach their type/member references to the corresponding `ASSEMBLY_REF`.
- `TYPE_REF` binds to the target `TYPE_DEF` only after the `ASSEMBLY_REF -> MODULE` binding succeeds.
- `MEMBER_REF` binds to the target member only after both assembly and type binding succeed.
- Binding results are stored in existing `moduleMetadataBindings` sidecars.

Runtime query mapping:

- Runtime query APIs should expose the normalized assembly name, version, signature hash, and reference binding status.
- A caller querying a failed binding receives a structured failure status and diagnostic code.

## Runtime Verification

Runtime or load-time verification should follow this order:

1. Resolve the source alias to a normalized project reference.
2. Open and normalize the referenced `.zrp`.
3. Validate declared assembly identity if the reference declared one.
4. Validate requested version and version range.
5. Compile or load the target provider metadata.
6. Validate the provider `moduleSignatureHash` against the `ASSEMBLY_REF.targetModuleSignatureHash` when a hash was recorded.
7. Bind `ASSEMBLY_REF -> MODULE`.
8. Bind `TYPE_REF` and `MEMBER_REF` records that depend on that assembly.

No silent fallback should occur after a declared assembly mismatch. The loader may only use path-first identity derivation for old `dependencies` entries that did not declare an assembly name.

## Diagnostics

Stable diagnostic codes should be used across CLI, tests, and LSP.

- `zrp_manifest_version_unsupported`: unsupported `manifestVersion`.
- `zrp_assembly_name_invalid`: invalid or missing normalized assembly name.
- `zrp_assembly_version_invalid`: invalid normalized assembly version.
- `zrp_public_key_token_invalid`: invalid public key token text.
- `zrp_reference_alias_invalid`: invalid reference alias.
- `zrp_reference_duplicate_alias`: duplicate reference alias after compatibility normalization.
- `zrp_reference_conflict`: old and new reference fields normalize to different values.
- `zrp_reference_path_not_found`: referenced `.zrp` path cannot be opened.
- `zrp_reference_identity_mismatch`: referenced `.zrp` assembly name does not match the declared reference.
- `zrp_reference_version_mismatch`: referenced version is outside the requested version or range.
- `zrp_reference_cycle`: project reference cycle is found.
- `zrp_reference_not_declared`: source imports an undeclared reference alias.
- `assembly_signature_mismatch`: recorded target signature hash differs from loaded provider metadata.
- `assembly_access_denied`: symbol exists but is not visible from the consuming assembly.
- `metadata_binding_missing`: a metadata token could not be bound after assembly resolution.

Each diagnostic should include:

- the `.zrp` file or source file
- the field or import span when available
- the requested alias and assembly name
- the resolved path when available
- the target assembly name/version when a target was loaded
- a short remediation message

## Data Model Boundary

Implementation may reuse existing structs, but the normalized model should be equivalent to:

```c
typedef struct SZrProjectAssemblyIdentity {
    char *name;
    char *version;
    char *culture;
    char *publicKeyToken;
    int kind;
    bool fromCompatibilityFields;
} SZrProjectAssemblyIdentity;

typedef struct SZrProjectReference {
    char *alias;
    char *declaredAssemblyName;
    char *resolvedAssemblyName;
    char *requestedVersion;
    char *minVersionInclusive;
    char *maxVersionExclusive;
    char *manifestPath;
    bool pathFirstCompatibility;
} SZrProjectReference;
```

The key rule is that project loading produces one normalized reference table. Later compiler and runtime layers should not need to know whether an entry came from `references` or old `dependencies`.

## Implementation Layers

Implementation should move bottom-up and stop at each gate for validation.

### Layer 1: Manifest Normalization

Scope:

- Parse `manifestVersion`, `assembly`, and `references`.
- Preserve old fields and map them into the normalized model.
- Validate names, aliases, versions, path shape, and duplicate/conflicting aliases.
- Update `zrp.schema.json` for tooling.

Exit gate:

- Loader tests cover new-only manifests, old-only manifests, mixed compatibility, conflict handling, invalid aliases, invalid assembly names, missing paths, and version defaults.
- No import resolver or metadata behavior depends on raw JSON field shape.

### Layer 2: Reference Resolution

Scope:

- Resolve `&alias` and `&alias.path` through normalized references.
- Keep canonical keys in the `$alias@version/path` shape.
- Preserve dependency-local path aliases.
- Reject undeclared reference aliases early.

Exit gate:

- Import resolver tests cover new `references`, old `dependencies`, same real assembly at two versions, dependency-local aliases, and intrinsic `zr.*` behavior.

### Layer 3: Metadata Emission

Scope:

- Emit provider `MODULE` identity from normalized `assembly`.
- Emit provider `moduleVersion` from normalized `assembly.version`.
- Emit `ASSEMBLY_REF` rows from normalized references when cross-assembly types or members are used.
- Preserve target signature hash behavior.

Exit gate:

- Metadata/token tests assert provider module identity, requested versions, version ranges, signature hash propagation, and cross-assembly type/member reference ownership.

### Layer 4: Runtime Binding And Verification

Scope:

- Bind `ASSEMBLY_REF -> MODULE` through normalized reference identity.
- Validate identity, version, and signature hash before type/member binding.
- Return stable diagnostic codes for all failure modes.

Exit gate:

- Runtime query tests cover successful binding, identity mismatch, version mismatch, signature mismatch, missing reference, missing token binding, and access denial.

### Layer 5: Tooling And Incremental Behavior

Scope:

- Update language server schema validation and project refresh.
- Update CLI diagnostics and any project inspection output.
- Ensure project graph caching invalidates when referenced `.zrp` identity or version changes.
- Update docs/plans/using progress files in place when implementation milestones complete, including timestamp, completion status, and notes.

Exit gate:

- LSP/schema tests cover completion/validation for `assembly` and `references`.
- CLI or integration tests show clear diagnostics for invalid `.zrp`.
- Incremental tests show reference graph refresh when a target `.zrp` changes.

## Test Matrix

Project loader:

- new manifest with `assembly + references`
- old manifest with `name/version/dependencies`
- mixed manifest with identical old/new reference
- mixed manifest with conflicting old/new reference
- missing `assembly.name` with old `name` fallback
- invalid alias, invalid assembly name, invalid version, invalid token
- path-first old dependency whose target identity is read from target `.zrp`

Import resolver:

- `&alias`
- `&alias.module.path`
- same real assembly at two versions through different aliases
- dependency-local `@alias`
- relative imports inside referenced project
- undeclared reference alias
- `zr.*` global imports

Metadata/token:

- provider `MODULE` identity and version
- `ASSEMBLY_REF` requested version and range
- target signature hash recorded and compared
- type/member references attached to the correct assembly reference
- old dependency emits the same metadata after normalization

Runtime:

- successful cross-assembly type/member binding
- declared assembly mismatch
- requested version mismatch
- version range mismatch
- signature hash mismatch
- duplicate assembly identity loaded once when same name/version/path repeat
- same assembly name loaded twice when versions differ and aliases differ
- access denied for internal/private symbol

Tooling:

- JSON schema validates canonical fields
- LSP reports stable `.zrp` diagnostics
- project graph refresh responds to referenced `.zrp` identity/version changes
- CLI displays alias, assembly, version, and path in diagnostics

## Deferred Decisions

- Strong-name cryptographic validation can be added after identity text fields are stable.
- Assembly-level export lists can be added later if existing symbol/module visibility is not enough.
- Transitive source-visible references can be added later with an explicit field such as `transitive`, but v1 should keep source visibility direct-only.
- Binary-only assembly references can be added later. v1 project references point to `.zrp`.
- Package restore and registry resolution are outside this design.

## Review Checklist

- The manifest has one authoritative local alias per reference.
- Assembly identity and source alias are separate.
- Existing `.zrp` files have a compatibility path.
- The design maps directly onto `MODULE`, `ASSEMBLY_REF`, version fields, signature hashes, and binding sidecars.
- Same assembly name at different versions is representable.
- Old path-first dependencies do not become stricter than before unless they opt into declared assembly identity.
- Direct cross-module access is explicit and metadata visibility still controls symbol access.
- Implementation can proceed layer by layer with testable gates.
