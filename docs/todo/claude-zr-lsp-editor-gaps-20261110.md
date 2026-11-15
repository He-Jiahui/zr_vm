# zr_vm LSP editor-feature gaps — read-only dig (2026-11-10 request, written 2026-11-14)

## RESULT_CARD

- **STATUS:** SETTLED (diagnosis and plan only; nothing built, run, staged or committed)
- **TARGET:** zr_vm LSP editor gaps (static analysis, highlighting, inference, format suggestions)
- **TIP:** `8fbbf4bb99a7c12d20faf6a88173a7b09914fa49` (`/workspace/zr_vm-pr7-review`, detached, clean before and after)
- **MODEL:** Opus 5.5 for the first part of the session; the harness switched the session to **Fable 5.2** part-way through (not my choice, disclosed here rather than silently). Formatting/position evidence was read on Opus; semantic tokens, inlay, diagnostics and this write-up on Fable.
- **A–F:**
  - **A** All four areas have a real, advertised protocol surface. Diagnostics, semantic tokens, documentHighlight, inlay hints, full/range formatting and code actions are core entries shared with WASM; onType formatting is a stdio-only adapter; `colorProvider` is withdrawn (MethodNotFound).
  - **B** Seven product-visible problems, ranked in §B. The three I would act on first: the formatter rewrites multi-line template strings and comment bodies; formatting ignores `tabSize`/`insertSpaces`; semantic tokens colour ordinary identifiers such as `move`/`native`/`init` as keywords and shadowed import aliases as namespaces.
  - **C** Out of scope listed in §C. No architecture rewrite, no parallel type system.
  - **D** Five ordered LOCAL_ONLY units in §D, all startable from `4c5eebcbb`/`8fbbf4bb` on a fresh worktree; none depends on PR #8.
  - **E** No VM ABI, opcode, interpreter or AOT blast radius. All edits are inside `zr_vm_language_server/`. One public language-server API addition (U2). Risks are stdio smoke expectations and the UTF-16 position contract (§E).
  - **F** High (≈90%) that U1 is implementable with no language semantic change.
- **KEY FINDINGS:**
  - The formatter is a brace-depth re-indenter, not a syntax formatter. It trims and re-indents every line regardless of lexical mode, so it changes the value of multi-line template strings and flattens doc-comment and continuation-line alignment.
  - New Bot's first pass is **confirmed**: formatting is fixed at 4 spaces and stdio never reads `FormattingOptions`. Every existing test sends `{tabSize: 4, insertSpaces: true}`, so the non-default path is untested.
  - New Bot's import-highlighting suspicion is **confirmed and is worse than stated**: the merge priority table makes NAMESPACE beat VARIABLE/PARAMETER, so a shadowing local is always recoloured.
  - New finding: the token keyword list contains 18 words the lexer treats as plain identifiers, and KEYWORD outranks function/method/variable. The repo's own demo (`systems.move(...)`, `pub fn move`) and fixtures (`let native = import(...)`) are miscoloured.
  - While a document has a syntax error, the server keeps the last-good AST. Position requests reject it; semantic tokens and inlay hints do not, so they are computed from stale offsets against new text. This is inferred from code, not observed at runtime.
  - Semantic diagnostics are deliberately suppressed whenever the document has a parse error on a fallback AST. That is a design choice, not a bug, but it is the most likely explanation for "static analysis shows nothing while I type".
  - New Bot's third point (analyzer not fully sharing compiler facts) is real as a **foundation gap** but I found no bounded, user-visible fix for it. It is not a Codex unit.
- **CODEX FEED:** U1 formatter lexical-mode preservation → U2 FormattingOptions → U3 semantic-token keyword/alias priority → U4 stale-AST guard for tokens/inlay + content-derived resultId → U5 UTF-16 columns in the shared editor position helper. All LOCAL_ONLY.
- **NEXT:** await New Bot / John before any Codex implement; no push/PR.

---

## Method and limits

Read-only source reading at the tip above, plus `git diff --stat` / `git diff` against PR #8 tip `3618cd10f` without checking it out. No cmake, ninja, ctest, cargo or rustc was run, so **nothing in this report is a runtime observation**. Every claim is from code paths, in-code review notes, fixtures and acceptance records. Where I reason about what an editor would show, I label it "inferred".

The tree carries many in-code `BUG:` / `TODO:` review notes. I treated them as leads, not findings: each item in §B was re-derived from the code path, and several notes turned out to be masked in practice (see B7) or to be allocation-failure hygiene with no normal-path symptom (listed in §B "not promoted").

Paths below are relative to `/workspace/zr_vm-pr7-review`. `LS/` abbreviates `zr_vm_language_server/src/zr_vm_language_server/`.

---

## A) Current product surface

Registry: `LS/protocol/lsp_capability_registry.c:36-281` (30 descriptors). The registry is metadata; native publication is hand-built in `zr_vm_language_server/stdio/stdio_initialize.c` and `stdio_initialize_capabilities.c`, and the registry's own note (`:31-33`) says `runtimeMask` does not automatically prevent native/Web drift.

| Area | Capability | Status | Where |
| --- | --- | --- | --- |
| Static analysis | `textDocument/publishDiagnostics` + pull `textDocument/diagnostic` + `workspaceDiagnostics` | Implemented, core, native+WASM | `LS/interface/lsp_interface.c:1602` (`GetDiagnostics`), `stdio/stdio_diagnostics.c`, `stdio_initialize_capabilities.c:93-95` |
| Static analysis | Diagnostic quick fixes via `codeAction` (`quickfix`, `source.organizeImports`, `source.removeUnused`), with `resolveProvider` on native | Implemented, core; resolve is native-only | `LS/features/lsp_code_actions.c`, `lsp_code_action_imports.c`, `LS/interface/lsp_diagnostic_fixes.c`, `stdio_initialize_capabilities.c:31-52` |
| Highlighting | `semanticTokens` full, full/delta, range; 13 token types, 1 modifier (`declaration`) | Implemented. Full is core (native+WASM). Delta and range are computed in the stdio adapter from the full stream. | `LS/semantic/query/lsp_semantic_tokens.c:675`, `stdio/stdio_semantic_tokens.c`, `stdio_semantic_tokens_json.c`, `stdio_initialize.c:231-235` |
| Highlighting | `documentHighlight` | Implemented, core, native+WASM. Projects the same semantic query as references. | `LS/interface/lsp_interface.c:2514` |
| Highlighting | `colorProvider` / `documentColor` / `colorPresentation` | **Withdrawn.** Not published, not routed, returns `-32601`. | `docs/cli-and-tooling/lsp-advanced-editor-features.md:288-289,410`; `tests/language_server/stdio_color_capability_smoke.js` |
| Inference | `inlayHint` — type hints only, for un-annotated `var`/`let`, class/struct fields, and implicit function/method return types. Numeric-range and logical facts are appended to the label when the compiler recorded them. | Implemented, core, native+WASM. No parameter-name hints. `resolveProvider` published from the registry (none registered, so false). | `LS/interface/lsp_inlay_hints.c:250-365,369` |
| Inference | `hover` type facts, `signatureHelp`, `inlineValue` | Hover is core (native+WASM). signatureHelp is core but native-only. inlineValue is a stdio-only adapter. | registry `:59-74`, `:210-215` |
| Formatting | `documentFormattingProvider` | Implemented, core, native+WASM. Brace-depth re-indent + trailing-whitespace trim only. | `LS/features/lsp_editor_features.c:407-512,518` |
| Formatting | `documentRangeFormattingProvider` (and `rangesSupport` when the client negotiates it) | Implemented. Single range is core; multi-range is a stdio loop over the single-range entry. | `lsp_editor_features.c:587`, `stdio/stdio_editing.c:211,242`, `stdio_initialize_capabilities.c:25-29,58-64` |
| Formatting | `documentOnTypeFormattingProvider` (`}` and `;`) | **Native-adapter only.** Reuses range formatting on the current line. Not in WASM. | registry `:176-181`, `stdio/stdio_editing.c:295-323` |

Not stubs, but worth stating plainly so nobody over-reads the surface:

- The formatter does not parse. It tracks `{`/`}` depth through a small lexical-mode scanner (`lsp_editor_features.c:193-288`) and rewrites leading whitespace per line.
- Semantic tokens are a hybrid. Declarations come from compiler facts (`lsp_semantic_tokens.c:246-296`); everything else is a hand-written text scan (`:332-672`) that asks the compiler-fact query per identifier and adds keyword/decorator/import-alias tokens from text.
- Formatting refuses to produce edits (returns success with no edits) if the legacy-migration planner finds any legacy syntax in the file (`lsp_editor_features.c:40-61,541-544`). A user sees "format did nothing". This is intentional.

---

## B) Product-visible problems, ranked

None of these is fixed on PR #8 unless stated. PR #8 touches only `incremental_parser.c` and two test files (verified with `git diff --stat`).

### B1. Formatting changes program text inside multi-line template strings, and flattens comment and continuation alignment — high confidence

**Symptom.** Format Document (or range, or typing `;`/`}` with format-on-type) on a file containing a multi-line backtick template string rewrites the leading and trailing whitespace of the string's interior lines. The program's string value changes. Doc comments in the `/** ... */` style lose the space before each ` *`. Multi-line call arguments, array literals and chained calls are flattened to the enclosing block's indent.

**Root cause.** `lsp_editor_format_segment` trims leading and trailing spaces/tabs on every line and prepends block indent, without consulting the scanner mode at the start of the line (`LS/features/lsp_editor_features.c:440-475`). The scanner does know it is inside a template string or block comment (`:193-288`), but that state is only used to decide whether `{`/`}` count. The lexer accepts newlines inside template strings: it loops until the closing backtick or end of input (`zr_vm_parser/src/zr_vm_parser/syntax/lexer.c:327`). The in-code note at `lsp_editor_features.c:449` names the template case. Only `{`/`}` affect depth, so lines inside `(`…`)` or `[`…`]` get the block indent.

The template-string part is a correctness bug: a formatter must not change runtime values. The comment and continuation parts are quality problems that follow from the formatter's design.

### B2. Formatting ignores the editor's `tabSize` / `insertSpaces` — high confidence

**Symptom.** A user whose editor is set to tabs or 2-space indent runs Format Document and every line is re-indented with 4 spaces. With format-on-type enabled, each `;` or `}` rewrites the current line to 4-space indent.

**Root cause.** Indent is a literal `"    "` per level (`lsp_editor_features.c:313-320`). The stdio handlers never read `params.options`: `handle_formatting_request` passes only the URI (`stdio/stdio_editing.c:185-196`), and the range, ranges and onType handlers pass only URI and range (`:211-226`, `:242-276`, `:295-318`). The core entries have no options parameter (`include/zr_vm_language_server/lsp_interface.h:483,489`). In-code TODOs at `lsp_editor_features.c:516,585` flag exactly this. Every existing test sends `{ tabSize: 4, insertSpaces: true }` (`tests/language_server/stdio_smoke.js:2483-2535`, `stdio_optional_capabilities_smoke.js:96,106`, `stdio_protocol_conformance.js:545`, extension smoke suites), so the gap has no failing test today.

This confirms New Bot's first-pass statement.

### B3. Semantic tokens colour ordinary identifiers as keywords — high confidence

**Symptom.** A function, method, variable or field whose name is one of `scoped, readonly, resource, property, init, own, move, native, extern, async, await, comptime, import, loadModule, loadPlugin, case, default, where` is highlighted as a keyword at its declaration and at every use. Conversely the reserved words `abstract, virtual, override, final, shadow, test, intermediate, using, const` (and `true/false/null`, `Infinity/NegativeInfinity/NaN`) get no semantic keyword token at all, so in clients without a TextMate grammar they are uncoloured.

**Root cause.** The token scanner has its own keyword list (`LS/semantic/query/lsp_semantic_tokens.c:304-318`) that is not the lexer's reserved-word table (`zr_vm_parser/src/zr_vm_parser/syntax/lexer_tokens.c:133-190`). The 18 words above are in the scanner list but the lexer returns `ZR_TK_IDENTIFIER` for them; the parser recognises them contextually (for example `parser_types.c:1135` for `where`). Any identifier matching the list is emitted as KEYWORD before semantic resolution (`lsp_semantic_tokens.c:503-511`). The merge step then keeps KEYWORD because it has priority 3 while function/method/property have 2 and variable/parameter have 0–1 (`LS/semantic/query/lsp_semantic_token_entries.c:13-35,52-69`); it also discards the `declaration` modifier when it replaces the type (`:64-67`).

**Evidence in the repo's own sources.** `demos/swarm_arena/game/src/lanes/dod/systems.zr:69` declares `pub fn move(...)` and `demos/swarm_arena/game/src/game_session.zr:75` calls `systems.move(...)`; `tests/fixtures/projects/lsp_language_feature_matrix/src/main.zr:3` and `project_pair_shadowing/src/main.zr:3` have `let native = import(...)`. All of these take the KEYWORD path.

### B4. A local that shadows an import alias is coloured as a namespace — high confidence on the mechanism, medium on how often users hit it

**Symptom.** With `let net = import("network");` at module level, a parameter or local also named `net` in some function is highlighted as a namespace everywhere, and the member after its dot is resolved as if it were a module member chain.

**Root cause.** Import bindings are collected once for the whole AST and looked up by spelling only (`lsp_semantic_tokens.c:152-165`, lookup at `:563`). When the spelling matches, NAMESPACE is added unconditionally (`:587-593`). The scan has already added the correct VARIABLE/PARAMETER token for the same span (`:513-523`), but NAMESPACE has priority 3 against 0/1, so the merge replaces it (`lsp_semantic_token_entries.c:13-35,64-67`). The in-code TODO at `:561-562` asks for a test; reading the priority table settles it without one. A second text heuristic (`:168-244`) registers any `name = import…` or `name = %import…` on one line as an alias binding for the rest of the scan, again by spelling.

This confirms New Bot's second first-pass statement and adds the priority-table reason it is deterministic.

### B5. While the document has a syntax error, semantic tokens and inlay hints are computed from the previous AST against the new text — medium confidence (inferred, not observed)

**Symptom (inferred).** During editing, once a syntax error exists and lines have shifted, inlay type hints appear at the old line/column of their declarations (so against unrelated text), and identifier colours are missing or attached to the wrong words until the file parses again.

**Root cause.** On a parse with errors the incremental parser keeps the last-good AST and sets `usesFallbackAst` (`LS/incremental/incremental_parser.c:826-841`); the document update path keeps the old analyzer too (`LS/interface/lsp_interface.c:1479-1482`). The header states the contract: "语义请求须据此拒绝旧结果" (`include/zr_vm_language_server/incremental_parser.h:73`). Position-based requests honour it by zeroing the offset (`lsp_interface.c:430,501,1324,1762`), and code lens checks it (`LS/features/lsp_code_lens.c:422`). `lsp_inlay_hints.c` and `lsp_semantic_tokens.c` contain no reference to `usesFallbackAst` (grep-verified). Inlay hints take declarations from the stale analyzer (`lsp_inlay_hints.c:386-397`) and project stale file positions onto the current content (`:276-345`). Semantic tokens query compiler facts by current-text byte offset against stale facts (`LS/semantic/query/lsp_semantic_token_canonical.c:144-170`).

Related, same area, also from code: when there is no AST at all the snapshot is NULL and the token `resultId` degrades to `zr-snapshot:0:<length>` (`LS/snapshot/lsp_semantic_snapshot.c:346,418-436`); the delta handler then answers "no edits" when the previous id string matches even though token values changed (`stdio/stdio_semantic_tokens_json.c:112-120`). The reachable window is a document that has never parsed successfully, so it is narrow.

### B6. Semantic diagnostics disappear whenever the file has a syntax error — high confidence on the behaviour; it is by design

**Symptom.** With one syntax error anywhere in the file, all type/ownership/unresolved-reference diagnostics vanish and only the parser error remains. Users may read this as "static analysis stopped working".

**Root cause.** `GetDiagnostics` appends parser diagnostics and returns immediately when the AST is a fallback (`lsp_interface.c:1634-1644`). When an AST exists but no analyzer does, a single hint "Semantic analysis is unavailable for this document version…" is emitted instead (`:1646-1665`). This is consistent with the stale-AST contract and I am **not** proposing to change it; it belongs in the answer to John because it is the most likely thing he noticed. Changing it means error-tolerant parsing, which is foundation work.

Two adjacent items that are already known:
- A lex error in the first token produced no diagnostic at this tip. **Fixed on unmerged PR #8** (`collect_initial_lex_diagnostic`; the diff also removes the `:679` stdout note and moves `suppressErrorOutput` earlier). Settled; not reopened.
- The 2026-09-05 inventory lists "missing `possibly_uninitialized_read` in diagnostic smoke" as a pre-existing baseline failure. The producer exists in source at this tip (`zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c:565`, registry `diagnostic_registry.c:141`). Whether the smoke is green now cannot be told without a build; treat the old record as historical.

### B7. Editor ranges use byte columns where the protocol contract is UTF-16 — medium confidence, lower impact than the in-code note suggests

**Symptom.** On a line that has non-ASCII text (a Chinese comment or string) before the target, document-link underlines, code-lens anchors, folding start/end columns and some code-action edits are shifted right by the extra UTF-8 bytes.

**Root cause.** `lsp_editor_position_from_offset` counts every byte as one column (`lsp_editor_features.c:64-81`). The internal contract is UTF-16 columns, and under the default UTF-16 negotiation responses pass through unconverted (`stdio/stdio_position_encoding.c:391-400`). Callers with mid-line offsets: `lsp_document_links.c:307,486,660`, `lsp_code_lens.c:318`, `lsp_folding_ranges.c:305`, `lsp_property_code_actions.c:263,437`, `lsp_code_actions.c:527`, `lsp_code_action_imports.c:438,512`. A correct helper already exists and is what semantic tokens use: `ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position` (`LS/interface/lsp_position_codec.h:11`).

**Why I rank it low for formatting specifically.** The in-code note at `:63` cites the formatting edit range. But formatting edits start at column 0 and end at end-of-line or end-of-document, where the byte count is always ≥ the true UTF-16 length; LSP says a column past the line end is clamped to the line end, so conforming clients apply the edit correctly. The visible damage is in the mid-line callers above.

### Not promoted to findings

Review notes I checked and am deliberately not listing as product problems:

- `Array_Init` / string-creation / hash-insert failure handling (`stdio_diagnostics.c:85,255,300,359`; `lsp_code_actions.c:518,692,751`; `lsp_interface.c:1190`). Out-of-memory hygiene; no normal-path symptom.
- Multi-range formatting can return overlapping edits for overlapping input ranges (`stdio/stdio_editing.c:263`). Real, but only when a client sends overlapping ranges under the optional `rangesSupport` negotiation.
- Inlay range end is inclusive (`lsp_inlay_hints.c:27-32`). At worst a duplicate hint at a page boundary; clients de-duplicate by position.
- Semantic-token range drops a token that starts before the range (`stdio_semantic_tokens_json.c:174`). Edge of viewport only.
- Semantic token `data` is not re-encoded when the client negotiates UTF-8 positions (`stdio_semantic_tokens_json.c:36`). Real, but only for UTF-8-negotiating clients (not VS Code); candidate for a later unit, see §D "deferred".
- selectionRange parent/child containment notes (`lsp_editor_features.c:673,685,779`). Not one of John's four areas.
- Missing-import code action suppressed by a same-named local import elsewhere (`lsp_code_actions.c:149`), and `${alias.member}` inside templates ignored by unused-import (`lsp_code_action_imports.c:204`). Same spelling-not-scope family as B4; real but lower frequency.

### On "inference / analyzer not sharing compiler facts"

`docs/plans/lsp/00-current-state.md` lists this as structural gap 1–3. At this tip the editor-facing inference features I read are already projections of compiler facts: inlay hints only emit when a canonical declaration type exists (`lsp_inlay_hints.c:61-99`), and token classification goes through `ZrParser_SemanticQuery_SymbolAt` / canonical types. The LSP analyzer directory (`LS/semantic/analyzer/`, 20+ files including `semantic_analyzer_typecheck.c`) still carries its own checks, which is the duplication the plan describes. I found no specific wrong-type hover or wrong inlay label attributable to it by reading alone; finding one needs a running server and targeted fixtures. So: real foundation gap, no evidence-backed user symptom from this dig, no unit.

---

## C) Out of scope / do not fix in the follow-on units

- Merging or reopening PR #8, Cap C, L2-U1, L2-U2. None of the units below conflicts with PR #8's files except U4, which touches nothing in `incremental_parser.c` (it only reads `usesFallbackAst`).
- AOT gaps D/E, the interp fused-add loop, ZE, StormBound.
- Inventing Cap.
- Any architecture rewrite: no new type system or scope resolver in the LSP. U3 uses the existing compiler-fact query to decide; it does not add scope tracking to the text scanner.
- Error-tolerant parsing / keeping semantic diagnostics alive across syntax errors (B6). Foundation.
- Unifying the LSP analyzer with compiler facts, TypeRef/loan/region query contract, ModuleSpecifier/ModuleIdentity import identity. Foundation, per `00-current-state.md`.
- A real syntax-aware formatter (operator spacing, line breaking, `(`/`[` continuation indent). U1 only stops the current one from damaging text.
- Windows Unicode-path and class-setter leftovers from the LSP2 brief.
- WASM/Web parity work and the strict worker typecheck gate.

---

## D) Codex-ready plan

Common rules for every unit:

- **tip_at_start MUST be `8fbbf4bb99a7c12d20faf6a88173a7b09914fa49`** (tree-identical to main `4c5eebcbb`) on a **fresh** worktree. Not the interp-loop worktree, not dirty `zr_vm-gd-h2-e5`, not `zr_vm-pr7-review`, not the PR #8/#9 branches.
- LOCAL_ONLY. Commit only after acceptance. No push, no PR.
- RED first: add the failing test, show it fails for the stated reason, then fix.
- Gates are the named existing CTest entries only. No full-repo Release requirement.
- Units are independent and can be applied in order on one branch; U2 edits lines adjacent to U1, so do U1 first.
- I have not run any of these gates. Their current state at the tip is unverified; each unit must record the baseline of its gate before editing and HARD_STOP if it is already red for an unrelated reason.

### U1 — Formatter must not rewrite lines that begin inside a template string or block comment

- **Files:** `zr_vm_language_server/src/zr_vm_language_server/features/lsp_editor_features.c` (`lsp_editor_format_segment`, ~`:440-500`); tests in `tests/language_server/test_lsp_advanced_editor_features.c` (or a new `*_cases.h` included by it, following `test_lsp_current_syntax_formatting_cases.h`).
- **Approach:** capture `scanState.mode` at the start of each line, before trimming. If it is `ZR_LSP_EDITOR_SCAN_TEMPLATE_STRING`, append the line bytes verbatim (no leading/trailing trim, no indent) and still run the structural scan over the whole line so mode and depth stay correct. If it is `ZR_LSP_EDITOR_SCAN_BLOCK_COMMENT`, also append verbatim. Lines that start in CODE keep today's behaviour. Keep CRLF handling as is.
- **Decision already made in this plan:** block-comment interior lines are preserved verbatim rather than re-aligned. That is the conservative choice; re-aligning ` *` continuation lines is a style feature and is not in this unit.
- **Tests to add (RED first):** (1) a multi-line template string whose interior lines have leading and trailing spaces, inside a mis-indented block: the interior bytes are unchanged and surrounding code lines are re-indented; (2) a `/** … */` comment with ` * ` continuation lines is unchanged; (3) a `}` at the start of a line inside a template string does not reduce indent; (4) range formatting starting on a line inside a template string returns no change for that line.
- **Gate:** `language_server_lsp_advanced_editor_features_tests`. Keep green: `language_server_stdio_smoke`, `language_server_stdio_optional_capabilities_smoke`, `language_server_stdio_protocol_conformance`.
- **HARD_STOP:** an existing test asserts that comment or template interior lines are re-indented; the fix needs a change outside `lsp_editor_features.c` and tests; the lexer turns out not to accept multi-line templates on a real fixture (then drop the template half and report).

### U2 — Honour `FormattingOptions.tabSize` / `insertSpaces`

- **Files:** `features/lsp_editor_features.c`; `include/zr_vm_language_server/lsp_interface.h` (add declarations); `zr_vm_language_server/stdio/stdio_editing.c` (four handlers); tests in `tests/language_server/test_lsp_advanced_editor_features.c` and one stdio assertion in `tests/language_server/stdio_optional_capabilities_smoke.js`.
- **Approach:** add an options struct `{ tabSize, insertSpaces }` and two new entries, `ZrLanguageServer_Lsp_GetFormattingWithOptions` and `…GetRangeFormattingWithOptions`. Keep the two existing functions with unchanged signatures as wrappers that pass `{4, true}`, so WASM exports (`wasm/wasm_exports.cpp:1782,1823`), the registry strings and all current tests are untouched. Thread the options into `lsp_text_builder_append_indent`: `insertSpaces` → `tabSize` spaces per level, otherwise one `\t` per level. In stdio, parse `params.options` in the formatting, rangeFormatting, rangesFormatting and onTypeFormatting handlers; missing or invalid options fall back to `{4, true}`; clamp `tabSize` to a sane range (1–16).
- **Not in this unit:** `trimTrailingWhitespace`, `insertFinalNewline`, `trimFinalNewlines`; WASM option plumbing.
- **Tests to add (RED first):** Unity: same input formatted with `{2,true}` yields 2-space indent, with `{4,false}` yields tabs, default wrapper output is byte-identical to today. stdio: one request with `{ tabSize: 2, insertSpaces: true }` asserting 2-space `newText`.
- **Gate:** `language_server_lsp_advanced_editor_features_tests`, `language_server_stdio_optional_capabilities_smoke`. Keep green: `language_server_stdio_smoke`, `language_server_stdio_protocol_conformance`, `language_server_lsp_capability_registry`, `language_server_stdio_protocol_inventory` (entry-point names must not change).
- **HARD_STOP:** the capability registry or inventory tests require the core entry-point string to change; any need to change a WASM export signature; an existing test fails with default options.

### U3 — Semantic tokens: resolved symbols win over the text keyword list and over spelling-matched import aliases

- **Files:** `semantic/query/lsp_semantic_tokens.c` (`semantic_token_is_keyword_word`, the identifier branch `~:487-600`); possibly `semantic/query/lsp_semantic_token_entries.c` only if a helper is needed; tests in `tests/language_server/test_lsp_interface.c` or `test_lsp_project_features.c` (both already call `GetSemanticTokens`).
- **Approach, three parts:**
  1. Split the keyword list into *reserved* (words the lexer reserves) and *contextual* (the 18 the lexer returns as identifiers). Reserved words are emitted as KEYWORD as today. For a contextual word, call `ZrLanguageServer_LspSemanticToken_ResolveCanonical` first; if it resolves to a symbol or type, emit that and do not emit KEYWORD; otherwise emit KEYWORD.
  2. Add the reserved words the list is missing (`abstract, virtual, override, final, shadow, test, intermediate, using, const`). Prefer deriving "is reserved" from `ZrParser_Lexer_FindKeyword` (`lexer_tokens.c:183`) if that symbol is linkable from the language server; otherwise extend the local table and say so in the commit.
  3. For an identifier whose spelling matches an import binding: if `ResolveCanonical` returns VARIABLE or PARAMETER for that span, treat it as that symbol (no NAMESPACE token, no module member-chain walk). Otherwise keep today's NAMESPACE behaviour.
- **Do not** change the priority table globally and do not add scope tracking to the scanner; the decision is delegated to the existing compiler-fact query.
- **Tests to add (RED first):** `pub fn move()` declaration and call are FUNCTION (declaration modifier kept); `obj.move()` member is METHOD; `let native = import("…")` alias is NAMESPACE, not KEYWORD; a parameter named like a module-level import alias is PARAMETER inside the function while the module-level alias remains NAMESPACE; `where` in a generic constraint and `init` in a constructor position are still KEYWORD; `abstract`/`override` are KEYWORD.
- **Gate:** `language_server_lsp_interface_tests`, `language_server_lsp_project_features_tests`. Keep green: `language_server_stdio_smoke`, `language_server_lsp_semantic_snapshot`, `language_server_wasm_capability_inventory` (legend must not change — 13 types, 1 modifier).
- **HARD_STOP:** fixing a case requires a new token type or modifier (legend/ABI change); `ResolveCanonical` does not resolve contextual-word declarations (then report which fact is missing — that is a compiler-fact gap, not something to patch in the scanner); an existing smoke asserts `import`-alias tokens in a way that contradicts part 3.

### U4 — Do not project stale-AST facts into inlay hints and semantic tokens; make the no-snapshot `resultId` content-derived

- **Files:** `interface/lsp_inlay_hints.c`; `semantic/query/lsp_semantic_tokens.c`; `snapshot/lsp_semantic_snapshot.c` (`FormatResultId`) or `stdio/stdio_semantic_tokens.c`; tests in `tests/language_server/test_lsp_incremental_equivalence.c` / `test_lsp_interface.c` and `test_lsp_semantic_snapshot.c`.
- **Step 0 (required, because B5 is inferred):** write the RED test first — open a valid document, apply an edit that inserts a line and introduces a syntax error, then request inlay hints and semantic tokens. If the hints are *not* misplaced and tokens are *not* misattributed, HARD_STOP and report; the inference was wrong.
- **Approach:** in `GetInlayHints`, if the file version reports `usesFallbackAst`, return success with an empty array (matches the header contract). In `GetSemanticTokens`, if `usesFallbackAst`, skip `CollectImportBindings`/`add_symbol_tokens` and pass a NULL analyzer to the scan, so only text-derived tokens (reserved keywords, decorators, meta-methods) are emitted for that version. For `resultId`: when the snapshot is NULL, derive the id from a hash of the encoded token array instead of only its length.
- **Product trade-off to flag to John before implementing:** during a syntax error, identifier colours and inlay hints will blank instead of being stale. With a TextMate grammar underneath (the VS Code extension) that is a mild flicker. If John prefers stale-but-present, do only the `resultId` half.
- **Gate:** `language_server_lsp_interface_tests`, `language_server_lsp_semantic_snapshot`, `language_server_incremental_parser_tests`. Keep green: `language_server_stdio_smoke`, `language_server_stdio_resolve_capabilities_smoke`.
- **HARD_STOP:** Step 0 does not reproduce; an existing test requires semantic tokens or hints on a fallback AST; the fix needs to touch `incremental_parser.c` (that file is PR #8 territory — stop and report rather than edit).

### U5 — UTF-16 columns in the shared editor position helper

- **Files:** `features/lsp_editor_features.c` (`lsp_editor_position_from_offset`, `:64-81`); tests alongside `tests/language_server/test_lsp_code_lens_utf16_ranges.c` / `test_lsp_decorator_utf16_ranges.c` and `test_lsp_advanced_editor_features.c`.
- **Approach:** delegate to `ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position` (`interface/lsp_position_codec.h:11`), keeping the clamp of `offset` to `contentLength`. Then audit each caller listed in B7 for places that compensate for the old byte behaviour. `lsp_code_actions.c:310` adds a UTF-16 column to a byte line-start; fix it in the same unit only if the change is local, otherwise leave it and report.
- **Tests to add (RED first):** a document link, a folding range and an organize-imports edit on a line with non-ASCII text before the target each report UTF-16 columns; a formatting edit on a file whose last line is non-ASCII ends at the UTF-16 line length.
- **Gate:** `language_server_lsp_advanced_editor_features_tests`, `language_server_lsp_project_features_tests`, `language_server_stdio_position_encoding_smoke`. Keep green: `language_server_stdio_smoke`, `language_server_stdio_resolve_capabilities_smoke`.
- **HARD_STOP:** the codec helper treats CR/LF differently from the old helper in a way that changes ASCII-only output; `language_server_stdio_position_encoding_smoke` (UTF-8 negotiation) regresses — that would mean a caller was double-converting and needs individual review.

### Deferred, not units yet

- UTF-8 position-encoding of semantic-token `data` (`stdio_semantic_tokens_json.c:36`): bounded, but only affects UTF-8-negotiating clients; do after U5.
- Multi-range formatting overlap de-duplication (`stdio_editing.c:263`).
- Scope-aware missing-import / unused-import code actions (same family as B4).

---

## E) Side-effect / risk

- **VM ABI / opcode / interpreter / AOT:** none. Every edit is under `zr_vm_language_server/` and `tests/language_server/`. No file in `zr_vm_core`, `zr_vm_parser`, `zr_vm_aot` or `zr_vm_jit` is modified. U3 may *call* an existing parser symbol (`ZrParser_Lexer_FindKeyword`) but does not change it.
- **Language-server public API:** U2 adds two functions and one struct to `lsp_interface.h`; existing signatures are unchanged. No WASM export changes. No capability, legend or registry change in any unit — `language_server_lsp_capability_registry`, `language_server_stdio_protocol_inventory` and `language_server_wasm_capability_inventory` must stay green and are the tripwire for accidental surface drift.
- **stdio smoke:** `stdio_smoke.js` formatting requests all use `{4, true}`, which stays byte-identical after U1/U2 unless a smoke fixture contains a multi-line template or block comment that today gets re-indented (U1 HARD_STOP covers this). U3 changes token types for specific identifiers; any smoke that pins a token stream for a fixture using `native`/`move` as an identifier would change — `lsp_language_feature_matrix/src/main.zr:3` is such a fixture, so U3 must check the matrix test (`test_lsp_language_feature_matrix.c`) expectations before editing.
- **UTF-16 contract:** U1–U4 do not touch position arithmetic. U5 is the only unit that does, and it moves a helper *toward* the stated contract; the risk is a caller that silently relied on bytes, hence the caller audit and the position-encoding smoke in its gate.
- **Editor behaviour:** U4 trades stale decorations for blank ones during syntax errors; flagged for John's decision. U2 changes output only for clients that send non-default options, which is the point.
- **PR #8 interaction:** none of U1–U5 edits `incremental_parser.c`, `test_lsp_parser_diagnostics.c` or `test_lsp_workspace_path_security.c`. No rebase needed either way.

## F) Confidence in U1

High, about 90%. The change is confined to one function, uses scanner state that already exists, and needs no parser, compiler or language change. The residual risk is an existing test or smoke fixture that encodes the current re-indentation of comment interiors, which is a HARD_STOP rather than a design problem.

---

## 中文摘要（给 John）

**结论：** 四个方向（静态分析、高亮、推理、格式化）协议面都在，也都对外声明了；问题集中在"实现比声明粗糙"。只读源码得出，没有做任何构建或运行，下面凡是写"推断"的都没有运行验证。会话中途模型被框架从 Opus 5.5 切到 Fable 5.2，报告里已注明。

**现状（A）：** 诊断、语义 token（full/delta/range）、documentHighlight、inlay 类型提示、全文/区间格式化、code action 都是核心实现；onType 格式化只有 stdio 适配层；`colorProvider` 已撤销。格式化器不是语法格式化器，只是按 `{}` 深度重排缩进。

**用户能看到的问题（B，按优先级）：**
1. **格式化会改坏代码**：跨行反引号模板字符串内部的行也被去空白、重加缩进，字符串的运行时值变了；`/** */` 注释里 ` *` 前的空格、多行实参的对齐也被抹平。
2. **格式化忽略编辑器的 tabSize / insertSpaces**：固定 4 空格，stdio 根本不读 `options`。New Bot 的初判属实；现有测试全部只发 `{4, true}`，所以没有红测试。
3. **普通标识符被染成关键字**：`move`、`native`、`init`、`default`、`where` 等 18 个词在词法器里是普通标识符，但 token 扫描器自带的关键字表把它们当关键字，且优先级压过函数/方法/变量。仓库自己的 demo（`systems.move(...)`）和夹具（`let native = import(...)`）就中招。反过来 `abstract`、`override`、`const` 等保留字没有语义 token。
4. **遮蔽 import 别名的局部变量被染成 namespace**：按拼写匹配全文件的 import 绑定，合并优先级表让 namespace 必胜。New Bot 的初判属实，而且是确定性的。
5. **有语法错误时，inlay 提示和语义 token 用的是上一版 AST**（推断）：位置类请求会拒绝旧 AST，这两个功能没有检查，提示可能错位。
6. **有语法错误时语义诊断全部消失，只剩语法错误**：这是有意设计，不建议改，但很可能就是"静态分析没反应"的来源。首 token 词法错误无诊断的问题已在未合并的 PR #8 修掉，不重开。
7. **部分编辑范围按字节算列而非 UTF-16**：行内有中文时，文档链接、折叠、部分 code action 的范围会右偏。对格式化本身影响很小（客户端会把越界列收敛到行尾）。

**"分析器未完全共享编译器事实"**：是真实的基础缺口，但这次没找到可界定的、用户可见的修复点，不立单元。

**给 Codex 的计划（D，均 LOCAL_ONLY，起点 `8fbbf4bb` / `4c5eebcbb` 新工作树，不依赖 PR #8）：**
- U1 格式化器不改模板字符串和块注释内部的行
- U2 支持 `tabSize` / `insertSpaces`（新增带 options 的入口，旧签名保留为包装）
- U3 语义 token：已解析符号优先于关键字表和按拼写匹配的 import 别名
- U4 旧 AST 不投影到 inlay / 语义 token，并修无快照时的 resultId（先写红测试确认推断；需要你决定"语法错误期间宁可空白还是宁可过期"）
- U5 共享位置辅助函数改用 UTF-16 列

**风险（E）：** 不动 VM ABI、opcode、解释器、AOT；改动全部在 `zr_vm_language_server/` 和对应测试。U2 给语言服务器头文件加两个函数。U3 需先核对语言特性矩阵测试对 `native` 的 token 期望。各单元的门禁都是已有的 CTest 名称，我没有运行过，当前红绿状态未验证。

**U1 把握（F）：** 约 90%，不需要任何语言语义改动。

**下一步：** 等 New Bot / John 确认后再让 Codex 动手；不 push、不开 PR。
