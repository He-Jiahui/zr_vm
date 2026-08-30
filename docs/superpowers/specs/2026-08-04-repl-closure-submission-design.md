# REPL Closure-Submission Design

**Status:** Approved for implementation on 2026-08-05

## Scope

This design implements LSP 04 E5 from `docs/plans/lsp/04-debug-and-repl.md`.
Every REPL cell has explicit module, environment, and cell generations. A later
cell reads the real value and canonical semantic identity of an earlier binding.
Concatenated-source replay in a fresh global state is not persistence.

The design starts with the CLI REPL. DAP evaluation remains a separate,
capability-controlled consumer of the same parser and canonical binding facts.

## Decision

One REPL session owns one long-lived `SZrGlobalState` and a rooted submission
closure. Every successful cell produces its successor closure. The successor is
passed into the next cell as explicit closure captures and uses existing
`GET_CLOSURE` and `SET_CLOSURE` bytecode. No REPL-specific opcode or alternate
parser is added.

The compiler gets a submission-only binding map. Each row contains a capture
index, SymbolId, TypeId, PlaceId, declaration range, inferred type, and a
module/environment/cell generation token. Normal source compilation never sees
that map.

## Session And Binding Model

`ReplSession` owns the global state, main state, module generation, environment
generation, next cell generation, active rooted closure, and an ordered binding
table. The table is keyed by canonical SymbolId. A source spelling is only used
for parser lookup; it cannot replace a row with incompatible identity.

The parser/type environment receives verified prior rows as external closure
captures. The compiler receives the same rows to seed its closure-variable table.
Both use the same capture index and generation token.

## Cell Lifecycle

1. Parse and bind the source through the normal parser and binder.
2. Preflight top-level declarations and assignments that affect persistence.
3. Build a successor closure with stable slots for old rows and reserved slots
   for accepted new declarations.
4. Compile in submission mode: an injected old row loads and stores its matching
   closure slot; a reserved top-level declaration lowers directly to its slot,
   not a short-lived local.
5. Execute the cell closure against the successor environment.
6. On success publish the successor and advance all required generations. On
   preflight, compile, or setup failure discard the successor and release its
   owner values.

User-object effects before a runtime exception retain normal language behavior.
The REPL does not invent rollback. Pending new rows remain unpublished on every
failed cell and must be cleaned up.

## Ownership Policy

The policy consumes structured inferred-type and canonical capability fields,
never source text or display strings.

| Category | Cross-cell behavior |
| --- | --- |
| Plain value or GC reference | Store in the closure capture. |
| `Unique<T>` | Move to the closure on successful declaration or assignment. |
| `Shared<T>` or `Weak<T>` | Use the normal value-copy, retain, and release path. |
| `ref` or `readonly ref` | Reject before creating a successor environment. |
| ref-like, including `ref struct` and `PoolRef` | Reject before execution. |

Active loans, invalid owner operations, stale generations, and incomplete
canonical identity fail closed. The REPL never creates raw pointers, a name-only
lookup, or a synthetic TypeId to make a value persist.

## Generations And Reset

Every row carries current module, environment, and cell generations. A lookup,
closure load, and `:type` query must validate all three. Mismatch means the
binding is unavailable.

`:reset` releases rooted captures by normal ownership cleanup, disposes the
active closure, clears the binding table, increments module and environment
generations, and creates an empty environment. Old closures and type facts are
not reusable.

## Parser And Runtime Boundary

E5 adds a narrow submission-binding adapter. It seeds external closure captures
in the type environment and compiler from one verified table. It also adds a
submission-only top-level lowering path for persistent declarations and
assignments. Ordinary locals, lexical closures, and source compilation keep
their current behavior.

The runtime only uses existing closure storage, ownership operations, and GC
roots. The session roots its active submission closure, allowing normal mark and
compaction handling.

## Semicolon And Type Query

The REPL may wrap a bare expression in a controlled return expression. It never
inserts a semicolon into a simple statement. Declarations and assignments still
require the language `;`.

`:type` does not execute code or mutate the active environment. It consumes the
same generation-validated rows to inject canonical identities into a formal query
context. It reports stale or ref-like values as unavailable.

## Verification

- A later cell reads and assigns an earlier value without re-running its initializer.
- Unique move, Shared/Weak lifecycle, and reset cleanup use canonical ownership paths.
- Ref, readonly-ref, ref-like, and PoolRef persistence fail before execution.
- Failed cells publish no pending binding and leak no owner value.
- Reset invalidates old generations; `:type` remains read-only and canonical.
- Bare-expression termination remains controlled; missing statement semicolons remain parser errors.

Acceptance requires focused CLI and lower-layer tests on GCC, Clang, and MSVC,
then the current LSP/CLI validation matrix. Completion writes an E5 record in
`docs/plans/lsp/04-debug-and-repl/` and updates the central status table only
after all evidence is green.

## Non-Goals

- Source-text replay as a persistence mechanism.
- A second REPL parser, binder, or auto-semicolon grammar.
- Name, AST, display-text, raw-pointer, slot-only, or fabricated-TypeId fallback.
- Cross-process or cross-GC-domain environment transfer.
- Transactional rollback of arbitrary object mutation after a runtime exception.
