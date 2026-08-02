# Syntax 11 M5 CompileTool artifact resolution acceptance

Date: 2026-08-02

Scope: the compiler-owned package/ZRM resolution and cache-identity handoff
layer for external v2 `buildDependencies`. This record does not claim that the
ordinary import path executes an external CompileTool provider, that the
persistent incremental cache is integrated, or that Gate 11 M5 is promoted.

## Accepted contract

- Only a v2 package specifier declared uniquely in `buildDependencies` can
  enter this resolver. Runtime `dependencies` cannot satisfy the request even
  when they use the same package name.
- Ordinary compiler callers can use
  `ZrParser_CompileToolArtifact_OpenProjectBuildDependency`, which consumes the
  lock graph previously admitted into `SZrLibrary_Project`. The older explicit
  lock-array entry remains available for lock producer/tests, but compiler
  integration no longer needs to manufacture a parallel temporary lock graph.
- The lock match is package-root based, so both `@derive` and
  `@derive.tools.derive` use the same CompileTool package lock. Missing,
  duplicate, wrong-phase, wrong-source, non-canonical hash, or manifest-version
  mismatch facts fail closed.
- The selected ZRM must publish CompileTool phase, the imported package name,
  the locked version, and a nonempty public-contract hash. A Runtime archive
  or another package with otherwise plausible metadata is rejected.
- Root imports select the archive entry module. Package submodules map their
  canonical dotted identity to the ZRM module path without changing the public
  ModuleIdentity.
- The resolver reads the archive exactly once into compiler-owned bytes,
  computes its canonical SHA-256, checks the lock, and initializes the ZRM
  reader from those same immutable bytes. The selected entry is then read and
  checked against its own recomputed SHA-256. Path replacement cannot make
  validation and use observe different archives.
- Every CompileTool lock entry requires canonical `sha256:` base64url content
  and transitive assertions. The resolver sorts the complete CompileTool lock
  section by package identity and stores its canonical SHA-256; entry order is
  irrelevant, any CompileTool entry change invalidates it, and Runtime entries
  do not participate. This is a lock-section identity, not proof that every
  transitive provider artifact has already been loaded and verified.
- Successful close frees the entry bytes and ZRM reader and clears the complete
  resolved record. Bindings borrow that record and must be destroyed before it
  is closed.
- Runtime project package storage is not populated by this path. The existing
  runtime provider resolver cannot observe the resolved CompileTool archive.
- A descriptor may bind a resolved artifact only when both phases are
  CompileTool and the descriptor public-contract hash exactly matches the ZRM.
  Cache schema v5 hashes package/module identity, source kind, version, package
  hash, lock-section hash, entry name/hash, public contract, and current-module
  source digest in addition to the lexical provider contract. Cache lookup
  compares the complete 32-byte SHA-256 digest.

## Failure inventory

The focused resolver cases directly reject:

- an undeclared package;
- source-kind mismatch;
- Runtime or duplicate CompileTool lock entries;
- stale locked version or package content hash;
- a manifest requirement that does not admit the locked version;
- a non-SHA or non-canonical base64url transitive assertion;
- Runtime-phase ZRM;
- ZRM assembly/package identity mismatch;
- stale selected-entry hash;
- descriptor/ZRM public-contract mismatch.

The positive matrix covers package-root and package-submodule lookup, owned
byte count and entry identity, explicit close, unchanged runtime dependency
storage, canonical multi-entry lock ordering, CompileTool graph invalidation,
Runtime-entry exclusion, and cache-key divergence when only the canonical lock
graph changes. Independent SHA vectors cover empty input, `abc`, 55/56/63/64/65
bytes, and a 129-byte multi-block input. The ZRM memory test deletes the source
path after open and still reads from the borrowed immutable byte buffer.

## TDD and review evidence

The initial MSVC RED failed to compile because
`SZrParserCompileToolResolvedArtifact`,
`ZrParser_CompileToolArtifact_OpenBuildDependency`,
`ZrParser_CompileToolBinding_DeclareResolvedProvider`, and the close API did
not exist. GREEN added the compiler-owned resolver, resolved binding, and cache
identity. Review then found that a package submodule was incorrectly
compared as a full module identity against the package-root manifest entry;
the resolver now matches the package domain/name and retains module segments
only for ZRM entry selection.

The project-owned bridge RED then failed because the artifact resolver had no
entry that consumed the admitted project lock. GREEN delegates directly to the
existing resolver with the project-owned entry pointer/count; it does not copy
or weaken any identity, phase, version, archive, or hash validation.

Independent review then found a path re-open race, absent version-requirement
validation, untrusted transitive labels, output initialization gaps, FNV-only
cache equality, and host-dependent raw encodings. The final implementation owns
one archive snapshot, uses exact/caret SemVer checks, rejects non-canonical
hashes, hashes the sorted CompileTool lock section, and advances the cache to
canonical SHA-256 schema v4 with full-digest lookup. A final review found no
remaining scoped Critical, Important, or Minor findings and marked this
resolver/cache slice Ready.

The fixture-heavy cases were split into
`test_compile_tool_artifact_resolution_cases.h`, keeping the general comptime
runtime contract source below the repository's large-file warning boundary.

## Reference-language evidence

Repository-local upstream snapshots were checked before implementation:

- JDK `JavacProcessingEnvironment` uses a compiler-owned processor
  module/class path and service loader, while `TestClose` requires that loader
  to remain alive through compilation completion.
- Rust `rustc_metadata/creader.rs` separates host proc-macro loading from target
  crates; `cstore_impl.rs` registers crate-hash dependencies, and the
  incremental proc-macro fixture preserves discovery across metadata identity
  changes.
- Roslyn `IAnalyzerAssemblyLoader` and `AnalyzerAssemblyLoader.Core` require
  explicit dependency locations, stable same-path identity, isolated loader
  contexts, and fail-closed dependency resolution.

ZR follows the shared compiler-owned/phase-separated/content-identified model,
but narrows the provider surface to typed ZRM bytes and registered contracts;
it does not expose arbitrary parser tokens, source mutation, or a second macro
language.

## Validation

- MSVC 19.44.35228 built the five Gate 11 focused targets. Their direct
  executables pass 93/93: compile-time execution 69, comptime contract 2,
  attribute contract 3, declaration transform 6, and comptime runtime 13.
- The resolver-focused runtime executable passes 13/13 after the final lock
  graph and base64url canonicalization changes. The expanded 13/13 replay under
  WSL GCC also exercises project-owned lock admission and resolver handoff.
  ZRM container coverage passes
  7/7, including the memory-reader lifetime/error-cleanup case.
- The strict percent cutover executable remains 6/6, and the milestone census
  remains 55 canonical records, 55 completion markers, 0 missing; the separate
  task-level support record is also complete.
- The complete seven-executable matrix is 106/106 under isolated WSL GCC 11.4,
  isolated WSL Clang 14, and MSVC 19.44.35228, all with zero failures.
- A WSL GCC AddressSanitizer replay passes the resolver/runtime target 13/13
  and ZRM target 7/7 with `detect_leaks=0`; neither log contains an
  AddressSanitizer or undefined-runtime error. Leak detection is not promotion
  evidence because the existing compiler/test harness baseline reports
  unrelated retained allocations outside this resolver slice.
- The Gate 11 formatter RED produced one failure because full/range formatting
  returned an edit containing `%compileTime` and `%func`. The WSL GCC GREEN
  replay passes the complete advanced-editor executable with zero failures:
  removed syntax produces no edit, while canonical declaration-transform
  metadata, `comptime fn`, `import("@derive")`, `%`, and `%=` are preserved.

## Remaining M5 work

- connect the resolved bytes to ordinary compile-only import binding and typed
  transform execution;
- load and verify the actual transitive provider artifact graph rather than
  treating the lock producer's transitive assertion as artifact proof;
- complete remaining artifact/reflection consumer acceptance.

Until those items close, Gate 11 M5 remains `indirect` in the upper-gate ledger
and the root Syntax redesign remains open.
