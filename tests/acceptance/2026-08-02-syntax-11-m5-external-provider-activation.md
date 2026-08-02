---
scope:
  - Syntax 55 leaf status records recount
  - production parser percent-syntax cutover
  - Syntax 11 M5 external source-provider activation
status: accepted-in-scope
last_verified: 2026-08-02
---

# Syntax 11 M5 external provider activation acceptance

## Decision

The canonical selector still returns 55 historical milestone records and all
55 retain a completion status in their declared leaf scope. The production
parser breaking cutover also remains accepted: removed percent-prefixed forms
are rejection/migration inputs, not a second production grammar.

This pass additionally accepts ordinary compiler-only activation of a
materialized v2 `buildDependencies` source provider. It does not promote Gate
11 M5 or the root Syntax redesign. The final versioned compile-tool executable
section, actual transitive provider-graph acceptance, and remaining
artifact/reflection/LSP consumers are still open.

## Accepted behavior

- Project import canonicalization preserves a declared build-dependency package
  specifier, including a package submodule, while an unknown package remains a
  hard resolver error.
- Module-init analysis excludes that import from the runtime static-import
  graph and runtime dependency package inventory.
- A materialized path ZRM is admitted only through the project-owned lock and
  existing package/version/phase/hash/public-contract checks.
- The selected `.zrs` entry is parsed and owned by the compiler. Its imports
  and private helpers exist only inside the provider execution scope.
- An external provider source cannot recursively activate another
  build-dependency provider until the phase-cycle graph gate is promoted; it
  fails before recursive preparation with
  `compiletool.provider.transitive_not_promoted`.
- Function access modifiers now survive parsing. Only `pub` and `pro`
  functions are projected through a module alias; private/default-private
  transforms are rejected from consumer source.
- A public provider transform can call a private provider-local helper, return
  a typed Patch, generate a field, and pass the ordinary semantic/layout/read
  path. The transform/provider never enter the runtime module graph.

## Review closure

The review found and fixed four correctness issues before acceptance:

1. Ordinary function parsing consumed `pub`/`pro`/`pri` but discarded the
   modifier, while module prescan exported every top-level function. The AST
   now retains the modifier and source exports use it.
2. Imported source/binary declaration collection freed only the pointer array
   after a partial failure. It now releases each partially built function,
   inferred return/parameter type, and owned parameter array first.
3. Reusing an already opened project provider could leave a new binding behind
   if alias registration failed. The path now marks and restores the binding
   table transactionally.
4. A provider source could recursively activate itself or another build
   dependency even though transitive graph validation is not promoted. The
   source-module loader now rejects that edge before nested build-fact
   preparation, and the provider test fixes the fail-closed diagnostic.

The provider fixture was also corrected from a misleading `.zro` suffix to
`.zrs`; the current implementation is source activation and is not documented
as the final executable-section format.

## Fresh validation

| Toolchain | Focused results |
|---|---|
| WSL GCC 11.4 Debug shared | provider 8/8, parser 74/74, module 78/78, project import 35/35, compile-time 69/69, comptime runtime 14/14, declaration transform 6/6, decorator rejection 3/3, compiler integration 127/127 |
| WSL Clang 14 Debug shared | provider 8/8, parser 74/74, module 78/78, project import 35/35, compile-time 69/69, comptime runtime 14/14, declaration transform 6/6, decorator rejection 3/3 |
| MSVC 19.44 Debug shared | provider 8/8, comptime runtime/cache snapshot 14/14, percent cutover 6/6, parser 74/74 |

The first MSVC invocation was invalidated because a one-second orchestration
timeout overlapped a second MSBuild process and caused shared-PDB `C1041`
errors. After the overlapping process ended, the single-concurrency rebuild
completed and all four executables passed. No unresolved shared-library symbol
was observed.

## Promotion boundary

The result keeps the three claims separate: 55/55 leaf records are confirmed
in scope; the one-time production parser cutover is complete; the root Syntax
redesign remains open. Gate 11 M5 stays `indirect` until the final executable
section and remaining consumers have independent evidence, and 07B remains
blocked by its owner-gated `design-pending` entries.
