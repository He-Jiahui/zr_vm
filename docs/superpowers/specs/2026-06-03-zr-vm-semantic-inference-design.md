# ZR VM Semantic Inference And Diagnostics Design

## Summary

This design establishes the first implementation stage for improving ZR_VM semantic inference and diagnostic quality. The first stage builds a shared semantic fact and diagnostic layer for compiler-time checks. Later stages will expose the same facts through LSP, Debug, and REPL instead of duplicating inference logic in each tool.

The full goal remains broader than this first stage: expression type inference, reference inference, numeric inference, unreachable-code inference, logical semantic inference, ownership inference, token/reference tracking, richer syntax and semantic diagnostics, robust localized LSP inference, Debug expression/data inference and conditional breakpoints, and stronger REPL expression execution.

## Current Evidence

- `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c` is already over 4000 lines.
- `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c` is already close to 3000 lines.
- `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c` is already over 3000 lines.
- Existing tests already cover exact expression types, unreachable code, short-circuit diagnostics, ownership mismatches, local symbols, generic call inference, hover, completion, references, and parser diagnostics.
- Existing LSP APIs already include diagnostics, hover, completion, references, semantic tokens, inlay hints, formatting, code actions, folding ranges, selection ranges, document links, code lens, and hierarchy requests.
- Debug already has breakpoint conditions and an expression evaluator, but that evaluator is a separate small parser and should not become a second language frontend.
- Local reference-language sources support this direction:
  - CPython uses a two-pass symbol/scope table model.
  - Rust borrow checking models borrow facts as indexed, queryable data and attaches span suggestions to diagnostics.
  - Mono keeps debugger frame/local/value snapshots separate from breakpoint processing.
  - QuickJS exposes evaluation as a runtime entry point rather than growing a separate complete debugger expression language.

## First-Stage Goal

Create a shared compiler-time semantic fact and diagnostic layer that can be consumed by parser/type inference, language server semantic analysis, and future Debug/REPL expression execution.

This stage must make the following more true:

- Expression type inference stores exact, queryable facts for literal, identifier, binary, unary, call, member, assignment, conditional, array, object, lambda, and ownership builtin expressions.
- Reference inference records declaration/use relationships with precise token ranges.
- Numeric inference records constant numeric facts when values are statically known, including numeric kind and safe range information.
- Reachability inference records deterministic unreachable statements and branches.
- Logical inference records deterministic short-circuit facts and boolean constant consequences.
- Ownership inference records qualifier and state facts such as unique, shared, weak, borrowed, loaned, moved-from, weak-needs-upgrade, borrow escape, and loan escape.
- Diagnostics explain the concrete problem and a suggested next action, not only the expected token or symbol.
- LSP callers can ask for local facts and receive either a fact, a diagnostic-backed failure, or a controlled unknown result without crashing.

## Non-Goals For First Stage

- Do not rewrite the parser, type inference engine, or language server from scratch.
- Do not implement all Debug and REPL behavior in the first stage.
- Do not add a second complete expression parser inside Debug.
- Do not change core stack/type-layout/native inline payload paths unless a lower-layer semantic fact requires it and the active inline-struct session is reconciled first.
- Do not keep adding unrelated helpers to already oversized source files.

## Architecture

### Semantic Facts

Add a parser-owned semantic facts module, with a narrow public API under `zr_vm_parser/include/zr_vm_parser/`.

The facts module records:

- `expressionFacts`: AST node, range, inferred type, exactness, value kind, optional constant value.
- `symbolFacts`: symbol id, declaration range, name token range, type id, ownership qualifier, scope id.
- `referenceFacts`: declaration symbol id, use range, reference kind, access mode.
- `numericFacts`: expression node, numeric kind, exact constant value where available, range-check result.
- `reachabilityFacts`: statement or expression node, reachability state, cause range.
- `logicalFacts`: short-circuit state, constant condition state, skipped branch range.
- `ownershipFacts`: value symbol or expression, qualifier, state, source owner, borrow/loan region, escape cause.

The existing `SZrSemanticContext` should become the owning aggregate for these arrays. New facts should be append-only during a single analysis pass and queryable by AST node, symbol id, range, or source position.

### Diagnostic Builder

Add a diagnostic builder module that creates structured diagnostics for parser and semantic callers.

Each diagnostic should carry:

- stable code
- severity
- primary range
- short message
- concrete cause
- suggested fix text
- optional related ranges
- optional edit payload for future LSP code actions

Examples:

- Syntax: `missing_expression_after_assignment`
  - Message: `Missing expression after '='.`
  - Cause: `The assignment starts at '=' but the next token is ';'.`
  - Suggestion: `Add an expression before ';' or remove the assignment.`

- Semantic: `weak_value_requires_upgrade`
  - Message: `Weak value must be upgraded before borrowed use.`
  - Cause: `'%weak Resource' cannot flow into '%borrowed Resource'.`
  - Suggestion: `Upgrade/check the weak reference before passing it here.`

- Ownership: `borrow_escape`
  - Message: `Borrowed value escapes its owner scope.`
  - Cause: `This return stores a borrowed value beyond the lifetime of its owner.`
  - Suggestion: `Return an owned/shared value, or keep the borrowed value inside the current scope.`

Parser diagnostics can keep the current callback path, but should use richer messages and future structured fields instead of only formatting `expected token`.

### Local Semantic Query

Add a small local query API that LSP, Debug, and REPL can eventually share:

- query fact by file position
- query expression type at position
- query symbol/reference at position
- query constant/numeric fact
- query ownership state
- query reachability at statement position
- query diagnostics affecting a range

Unknown or partial analysis must be represented explicitly. A failed query must not crash and must not silently return a misleading object type.

### LSP Integration Path

First-stage LSP work is limited to consuming the new fact/query layer for existing diagnostics and local semantic requests. LSP should:

- preserve the last good semantic snapshot on parser failure
- merge parser diagnostics and semantic diagnostics in stable order
- expose richer diagnostic code/message/detail once facts are available
- avoid crashing when the requested position is inside incomplete syntax
- return controlled unknowns for incomplete local inference

Full LSP feature upgrades such as richer code actions, pull diagnostics, inline values, and completion resolve remain follow-up stages, but must use this same fact/query layer.

### Debug And REPL Integration Path

Debug and REPL should eventually evaluate expressions by reusing parser/type inference/runtime entrypoints:

- Debug keeps safe, side-effect-free read evaluation by default.
- Conditional breakpoints use the same expression parser/fact model as REPL where possible.
- Debug snapshots continue to own frame/local/value data separately.
- REPL expression execution should parse snippets as expressions or small statements with the same diagnostics as normal compilation.

The first stage should only design and expose the facts needed by these tools. It should not yet replace the debug evaluator unless the implementation plan explicitly promotes that later stage.

## Data Flow

1. Parser produces AST with precise token ranges.
2. Type inference and semantic analysis append facts into `SZrSemanticContext`.
3. Diagnostic builder emits structured parser/semantic diagnostics using the same ranges and facts.
4. LSP semantic analyzer owns or borrows the current semantic context and exposes localized queries.
5. Debug and REPL later call into the same parser/type/fact path rather than copying inference logic.

## Module Boundaries

Preferred new files:

- `zr_vm_parser/include/zr_vm_parser/semantic_facts.h`
- `zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c`
- `zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h`
- `zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c`
- `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c`
- `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h`

Build files must be updated in the same change when these modules are added.

Existing oversized files should remain orchestration points where possible. New fact collection, diagnostic formatting, and local query logic should not be appended wholesale to `type_inference_core.c`, `semantic_analyzer.c`, or `lsp_semantic_query.c`.

## Testing Strategy

Focused parser/semantic tests:

- expression facts for literals, identifiers, arithmetic, calls, member access, assignment, conditional, lambda, array/object literals
- numeric constant facts, widening, range failures, division/modulo-by-zero diagnostics
- reference facts for local variables, parameters, fields, imported symbols, and shadowing
- reachability after return/throw and constant `if` branches
- logical short-circuit facts for deterministic `true || expr` and `false && expr`
- ownership facts for weak upgrade requirements, borrow escape, loan escape, use-after-move, owner-to-plain escape
- syntax diagnostic messages for missing expression, missing delimiter, legacy syntax, and invalid ownership directive

Focused LSP tests:

- diagnostics carry richer messages and stable codes
- hover/type query uses exact expression facts
- position inside incomplete syntax returns diagnostics or unknown, not a crash
- last good semantic snapshot remains usable after local syntax breakage
- reference lookup uses precise token ranges

Future Debug/REPL tests:

- conditional breakpoint expression uses same truthiness and diagnostics
- debug evaluate reports the same expression error class as parser diagnostics
- REPL expression snippets can evaluate expressions without wrapping boilerplate
- REPL parser errors include cause and suggestion

## Validation

First-stage implementation must run focused tests before broader matrix validation:

- `zr_vm_type_inference_test`
- `zr_vm_language_server_semantic_analyzer_test`
- `zr_vm_language_server_lsp_interface_test`
- `tests/language_server/stdio_smoke.js` through the configured test target

After CMake/shared headers/parser/LSP changes, run the repository validation matrix required by `zr-vm-dev`:

- WSL gcc configure/build/ctest and CLI smoke
- WSL clang configure/build/ctest and CLI smoke
- Windows MSVC CLI smoke

Known baseline failures must be reported as baseline only after confirming the failure set did not change.

## Acceptance Criteria

First stage is complete only when:

- semantic facts are stored in a shared parser semantic context, not only transient analyzer locals
- expression type, reference, numeric, reachability, logical, and ownership facts have focused tests
- diagnostics include concrete cause and suggestion for representative syntax and semantic failures
- LSP can query local facts for a position and handles incomplete syntax without crashing
- oversized files are not expanded with unrelated new responsibilities
- focused parser/LSP tests pass, or any remaining failures are proven pre-existing baseline failures
- design documentation and implementation documentation are updated to match current behavior

## Risks

- Existing dirty worktree touches parser, core, LSP, CLI, library, Rust binding, docs, and tests. Implementation must avoid reverting or overwriting unrelated work.
- Active inline-struct/type-layout work may overlap core runtime metadata. First-stage semantic work should avoid those paths unless explicitly required.
- Fact duplication can create inconsistent answers if old semantic analyzer data and new facts are both treated as authoritative. The implementation should migrate consumers toward the new fact layer and delete redundant paths when a new path supersedes them.
- Diagnostic message quality can regress if structured facts are added but callers keep using old generic strings. Tests must assert user-facing message content, not only diagnostic codes.
