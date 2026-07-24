---
related_code:
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project_module_specifier.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - tests/library/test_project_module_specifier.c
  - tests/library/test_project_import_resolver.c
implementation_files:
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project_module_specifier.c
plan_sources:
  - user: execute docs/plans/syntax milestones with a completion record and one commit per milestone
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - docs/plans/syntax/10-native-ffi-module-package/m1-specifier-foundation-implementation-plan.md
tests:
  - tests/library/test_project_module_specifier.c
  - tests/library/test_project_import_resolver.c
  - tests/acceptance/2026-07-24-syntax-10r-m1-specifier-foundation.md
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

M1 neither reads manifests nor selects source, binary, descriptor, or artifact providers. It also does not
validate `file:` targets, assign package versions, expand aliases, record lock facts, or rewrite legacy
`&`/`$`/`@` migration inputs. Those responsibilities remain with Syntax 10R M2 and the existing migration
adapter until their promotion gates are complete.

## Test Coverage

`test_project_module_specifier.c` fixes absolute-domain separator equivalence, native/workspace distinction,
relative domain-preserving resolution, alias and package decomposition, canonical POSIX/drive/UNC locators,
and malformed-input rejection. The existing `test_project_import_resolver.c` remains a regression guard for
the untouched legacy resolver path.
