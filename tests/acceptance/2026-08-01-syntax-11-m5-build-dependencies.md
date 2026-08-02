# Syntax 11 M5 build dependency manifest and lock acceptance

Date: 2026-08-01

Scope: the canonical v2 project manifest and dependency lock foundation for
CompileTool build dependencies. This record does not claim compiler sandbox,
transform execution, cache-key invalidation, formatter, or full Gate 11 M5
promotion.

## Contract

- `dependencies` and `buildDependencies` use separate project arrays. The same
  package identity may appear in both with distinct version and source facts.
- Both sections require an `@package` root, a nonempty version, and exactly one
  `path`, `registry`, or `git` source. Legacy `$` roots, missing versions, and
  ambiguous sources are rejected. Duplicate section names and duplicate
  `version` members are rejected instead of relying on cJSON first-key lookup;
  child-count allocation is guarded against `SIZE_MAX` multiplication overflow.
- A build-only package cannot satisfy a phase-neutral manifest alias target;
  compile-only imports must remain explicit until compiler-phase resolution.
- The canonical v2 writer emits both sections independently and sorts each by
  canonical package identity. Runtime and CompileTool declarations are never
  merged during roundtrip.
- Lock entries carry an explicit provider phase. Runtime entries can satisfy
  only `dependencies`; CompileTool entries can satisfy only
  `buildDependencies`. Wrong-phase, missing, duplicate, or source-kind-mismatched
  entries fail closed and leave the output buffer empty.
- The lock graph retains a separate `buildDependencies` section with resolved
  version, content hash, transitive identity, and provider kind. It does not
  serialize a machine-local provider locator.
- `ZrLibrary_ProjectManifestV2_ReadDependencyLock` admits that projection into
  project-owned storage only after the complete Runtime and CompileTool graph
  matches the manifest exactly. The reader rejects duplicate sections, package
  entries, entry fields, undeclared packages, missing packages, and provider
  source drift. It also requires the JSON document to terminate after the root
  object instead of accepting trailing bytes. Replacement is atomic: a rejected
  lock leaves the previously admitted graph intact.
- Lock entry text is copied into one project-owned allocation. Resolver callers
  do not borrow cJSON input storage, and project destruction frees the complete
  entry/text block with its exact allocation size.

## TDD evidence

1. The first RED failed compilation because `SZrLibrary_Project` did not expose
   independent build-dependency storage.
2. The second RED failed compilation because lock entries had no provider
   phase. GREEN added phase-typed matching and separate lock sections.
3. The focused test covers a package named `@shared` in both sections, an
   additional registry build dependency, deterministic writer/reader roundtrip,
   phase-separated hashes, wrong-phase/source-kind rejection, duplicate and
   undeclared lock entries, and malformed declarations.
4. Lock projection was moved out of the 1500-line manifest parser/writer into
   `project_manifest_v2_lock.c`; it reuses canonical internal helpers rather
   than duplicating package ordering or identity formatting.
5. Independent review found duplicate-key normalization and an unguarded
   allocation product. New RED cases accepted duplicate build-dependency
   sections/version fields; GREEN rejects both and guards the allocation size.
6. The project-owned lock RED failed because `SZrLibrary_Project` had no lock
   storage and no reader API. GREEN covers valid phase-separated admission,
   source-buffer destruction, invalid replacement atomicity, wrong version,
   missing/undeclared packages, provider drift, duplicate entry fields, and
   trailing-garbage rejection.

## Focused validation

The original writer/manifest matrix passed 9 tests with zero failures under
GCC 11.4, Clang 14, and MSVC 19.44.35228. A separate MSVC
`/Od /fsanitize=address` build also passed the original 9/9 with no sanitizer
report. After adding project-owned lock admission, the expanded executable
passes 10/10 under WSL GCC 11.4; Clang/MSVC expansion remains part of the final
Gate 11 matrix and is not claimed by this record yet.

## Remaining M5 work

- activate compiler-owned resolved provider bytes only through the ordinary
  compile-only import and transform execution path;
- integrate the identity with the persistent incremental cache;
- validate the actual transitive provider graph rather than treating the lock
  producer's canonical transitive hash as artifact-graph proof;
- complete formatter and remaining cross-consumer acceptance.
