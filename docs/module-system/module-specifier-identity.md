---
related_code:
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project_module_specifier.c
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.h
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - tests/library/test_project_module_specifier.c
  - tests/library/test_project_import_resolver.c
  - tests/library/test_project_manifest_v2.c
implementation_files:
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project_module_specifier.c
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.h
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c
  - zr_vm_library/src/zr_vm_library/project/project.c
plan_sources:
  - user: execute docs/plans/syntax milestones with a completion record and one commit per milestone
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - docs/plans/syntax/10-native-ffi-module-package/m1-specifier-foundation-implementation-plan.md
  - docs/plans/syntax/10-native-ffi-module-package/m2-manifest-artifact-implementation-plan.md
tests:
  - tests/library/test_project_module_specifier.c
  - tests/library/test_project_import_resolver.c
  - tests/library/test_project_manifest_v2.c
  - tests/acceptance/2026-07-24-syntax-10r-m1-specifier-foundation.md
  - tests/acceptance/2026-07-24-syntax-10r-m2-v2-declarations.md
  - tests/acceptance/2026-07-24-syntax-10r-m2-v2-writer-lock.md
doc_type: module-detail
---

# Module Specifier Identity

## Purpose

Syntax 10R M1 establishes a narrow, library-owned parser for the source spelling of a module. It separates
the semantic identity of a module from its provider and physical location before the manifest, artifact, and
native-provider layers make a selection.

The API lives beside the project resolver because later manifest resolution will consume the same data. It
does not replace the existing 06A migration resolver in this milestone.

## Data Model

`SZrLibrary_ModuleIdentity` stores only three semantic fields:

- `domain`: `OfficialNative`, `RegisteredNative`, `Workspace`, or `Package`.
- `segments`: canonical dot-separated logical module segments.
- `packageName`: the package root for the `Package` domain, otherwise empty.

`SZrLibrary_ModuleSpecifier` adds source-form data that is not part of identity: specifier kind, alias root,
relative parent count, and `file:` locator. `ZrLibrary_ModuleIdentity_Equals` compares only valid structured
identity fields, so `native:engine.render` and `engine.render` cannot collide even though their segments
match.

## Parsing Rules

| Source spelling | Kind | Identity/result |
| --- | --- | --- |
| `zr.task`, `zr/task` | Official native | `OfficialNative`, `zr.task` |
| `native:engine.render`, `native:engine/render` | Registered native | `RegisteredNative`, `engine.render` |
| `engine.render`, `engine/render` | Workspace | `Workspace`, `engine.render` |
| `./mesh`, `../mesh`, `../../mesh`, `...mesh` | Relative | unresolved segments plus parent count; resolution inherits the caller domain |
| `#lib.tool` | Alias | alias root `lib`, remaining segments `tool`; no identity until manifest expansion |
| `@math`, `@math.matrix`, `@math/matrix` | Package | `Package`, root `math`, optional export path |
| `file:///E:/...`, `file:///opt/...`, `file://host/share/...` | File locator | locator only; target declaration later supplies its identity |

Dot and slash are equivalent only in logical segment areas. A `file:` URI is not split as module segments.
The parser rejects empty segments, ambiguous native `zr.*` declarations, invalid package roots, bare physical
paths, malformed aliases, incomplete or mixed relative paths, and incomplete UNC locators. Repeated `../`
prefixes and their continuous-dot spelling both map to the same parent count.

## Relative Resolution

`ZrLibrary_ModuleSpecifier_ResolveRelative` accepts an already-canonical Workspace or Package identity and
a parsed relative specifier. It removes the current module leaf, applies the requested parent traversal, and
appends the parsed target segments. The caller domain and package root are preserved. Escaping above the
module root fails instead of silently searching filesystem paths.

A Package root such as `@math` is a valid canonical identity whose empty segment list denotes the package
default entry. It is intentionally not a relative-resolution caller because it has no current module leaf.
The resolver snapshots structured input fields before it writes its output, so callers may resolve in place
through either the current identity or the relative specifier identity.

## Scope Boundary

M1 neither selects source, binary, descriptor, or artifact providers nor validates the existence or terminal
shape of a `file:` target. Syntax 10R M2.2 consumes M1's spelling parser for v2 declaration admission; M2.3
publishes the admitted declarations deterministically. M2.4 selects a `.zrm` provider entry and returns provider
kind, phase, and contract hash as separate location facts. `SZrLibrary_ModuleIdentity` remains only the semantic
identity and never absorbs an archive path, artifact entry, phase, or contract hash.

## V2 Manifest Declarations

Syntax 10R M2.2 makes three v2 manifest fields structured project facts rather than legacy resolver strings:

- `aliases` preserves its full `#root` spelling and a parsed `SZrLibrary_ModuleSpecifier` target. Targets may
  be workspace, official-native, registered-native, package-root, or canonical `file:` specifiers. Relative
  targets and alias-to-alias recursion are rejected. A package target must be the current package or a declared
  dependency. `ZrLibrary_Project_ResolveManifestAlias` expands canonical child segments into the target domain;
  file targets receive URI path segments only. It never chooses a provider or validates a terminal artifact.
- `package.name` is exactly one `@package` root. `package.exports` maps `.` or `./logical.module` keys to
  workspace module specifiers. `ZrLibrary_Project_ResolvePackageExport` resolves only an explicitly exported
  key, so `@package/hidden` fails instead of falling back to a filename or raw module text.
- `dependencies` stores a package identity, nonempty version requirement, exactly one source kind (`path`,
  `registry`, or `git`), and its source spelling. The reader does not load a path, fetch a registry, clone git,
  or serialize a lock at this stage.

The v2 reader rejects v1-only `pathAliases`, `references`, `dependency`, and `local` fields. That keeps old
`@` aliases plus `$`/`&` dependency resolution in the v1 migration adapter instead of silently translating
them into the v2 identity model. `project.c` owns only lifecycle installation/freeing; all JSON loops and
cross-declaration checks stay in `project_manifest_v2.c`.

## Canonical V2 Writer And Lock Projection

`ZrLibrary_ProjectManifestV2_Write` accepts only a complete v2 project and emits the required base envelope followed
by v2 `aliases`, `package.exports`, and `dependencies`. Base fields have a fixed order; declaration keys are ordered
by their canonical `#root`, export key, or `@package` identity. Logical segment output uses slash separators, but
the in-memory `ModuleSpecifier`/`ModuleIdentity` remains structured and dot-normalized.

The publisher rejects v1 migration projects and every machine-local source locator: absolute drive, POSIX, UNC, and
`file:` locations are never publishable. A `path` dependency must therefore remain relative; a `registry` dependency
may use a package ID or a non-loopback HTTP(S) URI; and a `git` dependency must use a non-loopback HTTP(S), SSH, or
git URI with an authority. Empty-authority URIs, drive-like authorities, `localhost`, and IPv4/IPv6 loopback hosts are
rejected. The writer does not emit `pathAliases`, `$dependency`, `&reference`, `local`, or a machine-local cache path.

`ZrLibrary_ProjectManifestV2_WriteDependencyLock` receives independently resolved
`SZrLibrary_ProjectManifestDependencyLockEntry` facts. Each lock entry must match exactly one declared package and
the declaration's source kind. The generated lock contains only resolved version, content hash, transitive identity,
and structured provider kind. It deliberately does not receive or write a provider locator, so lock data cannot
accidentally publish a local cache path.

## Test Coverage

`test_project_module_specifier.c` fixes absolute-domain separator equivalence, native/workspace distinction,
relative domain-preserving resolution, alias and package decomposition, canonical POSIX/drive/UNC locators,
and malformed-input rejection. `test_project_manifest_v2.c` covers structured alias/package/dependency storage,
domain-preserving alias and export resolution, deterministic v2 writer and lock projection output, unexported package
rejection, v1-field isolation, local locator and loopback publication rejection, ambiguous dependency source
rejection, and malformed roots. The existing `test_project_import_resolver.c` remains a regression guard for the
untouched legacy resolver path.
