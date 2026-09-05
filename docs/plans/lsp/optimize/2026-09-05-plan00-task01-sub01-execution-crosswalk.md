---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
plan_sources:
  - docs/plans/lsp/00-current-state.md
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/plans/lsp/05-implementation-blueprint.md
  - docs/plans/lsp/semantic-inference/status-and-output.md
  - docs/plans/lsp/optimize/index.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/syntax/README.md
  - user: 2026-09-05 approved semantic analysis and LSP optimization execution plan
tests:
  - tests/language_server/test_lsp_capability_registry.c
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/test_lsp_semantic_query_parity.c
doc_type: milestone-record
---

# Optimize Execution Crosswalk

## 状态与产出记录

| Started | Completed | Status | Completed items | Evidence |
| --- | --- | --- | --- | --- |
| 2026-09-05 16:24 +08:00 | | in-progress | Source and active-session inventory; dependency-ordered execution contract. | Initial HEAD c95e5387; production parent 56837dbd. Current runtime verification remains pending. |
| 2026-09-05 16:50 +08:00 | | in-progress | Read main plans, the semantic-inference ledger and all 330 dated LSP records, including this inventory and the resolve leaf; reconciled historical completion, superseded clauses and remaining task ownership below. | Source inventory c95e53871aa38a884d80a23873ef9251d81f71d9; resolve leaf subsequently committed as ce04018c. This is a documentation crosswalk, not completion of Plan 00 Task 1. |
| 2026-09-05 16:50 +08:00 | 2026-09-05 17:24 +08:00 | completed (crosswalk subitem only) | Exhaustive linked leaf inventory, completed/pending/superseded clauses, source/evidence limitations and continuing task owners. | 331 dated records linked; link/count/table/whitespace validation passes. Full Task 1 baseline remains in-progress, with active semantic commits and runtime replay still required. |
| 2026-09-05 16:50 +08:00 | 2026-09-05 17:14 +08:00 | completed (documentation inventory only); Plan 00 Task 1 in-progress | Crosswalk for original 00-05, semantic-inference ledger, optimize 00-06, 331 dated records and two inline-only Task 7 entries. Included the newly active navigation-alias leaf without promoting its state. | Read-record review plus local link, reference-label, dated-leaf coverage and whitespace checks. No new runtime acceptance is claimed. |

## Execution Contract

Subsequent execution: navigation alias withdrawal completed as `670e3cd0` on
2026-09-05 17:22 +08:00. Its historical `P` inventory row below records the
17:14 capture; the [leaf record](2026-09-05-plan00-task04-sub02-navigation-aliases.md)
now supplies its completion and separate pending semantic/protocol defects.

- Syntax child designs, then the syntax master design, take precedence over LSP
  plans. Record conflicting clauses, the selected contract and its tests in the
  owning leaf. Do not invent partial declarations or other language syntax to
  satisfy editor-only examples.
- Keep the original optimize Task numbers. A completed implementation leaf
  includes code, regression tests, module documentation and this directory's
  timestamped completion record in one commit. Completion of a leaf does not
  complete its parent Task or permit skipping the next dependency gate.
- The user selected continuation from the existing symbol-projection and
  ResolveTypeAtPosition session commits. Their dirty overlays remain with their
  owners; the full semantic baseline is re-established after those commits.
- Desktop native, browser WASM and desktop Web are required. Auto selects native
  on desktop and WASM in browser. Protocol 3.17 is the stable baseline; 3.18
  additions require explicit negotiation.
- Core diagnostics and navigation remain required deliverables. Withdrawing an
  unsupported declaration fixes capability truth but does not finish the feature.

## Plan Crosswalk

| Plan | Historical implementation evidence | Current status and next gate |
| --- | --- | --- |
| [Original LSP 00](../00-current-state.md) | Its four linked baselines are retained in the original-leaf inventory. | The five structural gaps are not five new designs: duplicate inference and query coverage go to [O03 Tasks 1-7][o03]; module/provider identity to [O02 Tasks 2/3/5][o02] and [O03 Task 3][o03]; diagnostic schema/edit safety to [O03 Task 6][o03] and [O04 Tasks 3/6][o04]; evidence/promotion to [O00 Tasks 1/5][o00] and [O06 Tasks 6/7][o06]. |
| [Original LSP 01](../01-semantic-inference-core.md) and [semantic-inference ledger](../semantic-inference/status-and-output.md) | Q1-Q6 query/display/callable slices; 22 semantic leaves, plus shared module/property leaves. | Reuse the contracts. Q1/Q2/Q4 implementation gates map to O03 Tasks 1/2/4/5; Q3 Place/flow, Q5 module and Q6 provider completeness remain under O03 Tasks 3/7/8 and the required syntax coverage below. |
| [Original LSP 02](../02-diagnostics-and-errors.md) | 22 diagnostic leaves; [O03 Task 6 closure](2026-09-01-plan03-task06-structured-diagnostic-closure.md) accepts the structured producer/projector boundary. | Registry/range/fix and compiler/LSP parity are historically implemented in that scope. Current missing diagnostic publication remains [O03 Tasks 7/8][o03]; migration, replacement, manifest/multidocument actions and edit atomicity remain [O04 Tasks 3/6/8][o04]. |
| [Original LSP 03](../03-lsp-robustness-and-position.md) | 32 robustness leaves; L6 native acceptance explicitly closed with [33/33 per toolchain](../03-robustness/2026-08-10-l6-final-stdio-cli-matrix.md). | Preserve completed native L6 evidence. Strict transport, dependency fences and real incremental parsing were later added by O01/O02; current source and all-runtime replay remain [O00 Task 1][o00], [O05 Tasks 3/6/7][o05] and [O06 Tasks 4-7][o06]. |
| [Original LSP 04](../04-debug-and-repl.md) | All 22 E1-E5 implementation leaves have scoped completion evidence; the ledger's earlier "next E2b" paragraph is superseded by its later E2-E5 closure paragraph. | Debug/DAP remains independently owned. Shared snapshot/query changes require [O02 Task 3][o02], [O03 Tasks 7/8][o03] and [O06 Task 6][o06] integration checks; inline value presentation belongs to [O04 Task 7][o04] and [O05 Task 4][o05]. |
| [Original LSP 05](../05-implementation-blueprint.md) | L1-L8 dependency and promotion ledger; local L6 closure and sixteen L8 contracts are retained without changing their historical scope. | L1/L2/L5/L8 -> O03 then O04; L3 -> O03 Task 6 and O04 Tasks 3/6; L4 -> O02/O03 Task 3; L6 -> O01/O02/O06 replay; L7 -> independent E1-E5 plus shared-query integration. Full blueprint acceptance remains pending. |
| [Optimize 00][o00] | [Baseline](2026-08-22-baseline-contract.md), [registry](2026-08-22-capability-registry.md), [RED driver](2026-08-22-protocol-conformance-red.md); [identity-resolve withdrawal](2026-09-05-plan00-task04-sub01-identity-resolve.md), ce04018c. | in-progress. Historical inventory no longer permits identity-only publication after ce04018c; other overclaims, exact integrated baseline and Task 5 zero-orphan/full failure ownership gates remain open. |
| [Optimize 01][o01] | [Tasks 1-4 acceptance](2026-08-23-protocol-lifecycle-transport-acceptance.md) and [Tasks 5/6 teardown acceptance](2026-08-22-stdio-deterministic-teardown.md). | Tasks 1-6 were accepted historically despite unchecked original wording. Current negative/sanitizer/toolchain replay remains required; the 2026-09-05 Clang cancel-known timeout remains a Task 4/6 investigation item. |
| [Optimize 02][o02] | Seven Task leaves, ending with [10,000 edits and native multi-root acceptance](2026-08-23-plan02-acceptance-gates.md). | Tasks 1-7 have historical native evidence. WASM diagnostic projection later has a real runtime test, but complete browser/workspace parity and the current integrated replay remain open under O05/O06. |
| [Optimize 03][o03] | Tasks 1/2/4/5/6 checked; [Task 7.62](2026-09-02-plan03-task07-canonical-symbol-at-reference-resolution.md) is the latest committed consumer leaf in c95e5387. | Tasks 3/7/8 pending: sourceless origins, actual nonzero provider generations, consumer closure and original 16-target/stdio matrix. Astra symbol projection and Task 7.63 type query remain active with their owners until exact commits arrive. |
| [Optimize 04][o04] | Local implementation/navigation/hierarchy, structured quickfix, rename fences and canonical semantic-token consumers already have leaves below. | Historical local slices supersede "all aliases/text scans" wording only in their own scope. All eight Task acceptance gates remain open for real distinct navigation, cross-provider edits/hierarchies, parser-aware selection/formatting/links and negotiated presentations. |
| [Optimize 05][o05] | Worker diagnostic hashing/enumeration was replaced by the shared store; WASM structured diagnostics ran on 2026-08-26; ce04018c removes identity resolvers. | Tasks 1-7 remain open: generated manifest/client negotiation, shared status/error/edit contract, full workspace lifecycle, remaining providers, all three runtime modes, golden corpus, packaged smoke/version parity. |
| [Optimize 06][o06] | Historical native latency/memory/lifecycle evidence and several responsibility extractions exist. | Tasks 1-7 remain open: module/test boundary inventory, remaining cohesive extractions, fuzz/fault/sanitizer, scale workloads and a final same-commit GCC/Clang/MSVC/WASM/editor matrix and review. |

### Status Interpretation And Inventory Boundary

`C` below means the linked leaf's stated output is historically completed; its
test scope, timestamp and source identity remain exactly those in that record.
`P` means a required gate remains pending. `S` applies to an old assertion that
has been superseded by named later evidence, not to the entire historical leaf.
`G/C/M` denote GCC/Clang/MSVC. A compiler-only, source-contract-only or audit
record is not runtime acceptance. A completed leaf may therefore be `C` while
its parent and current integrated verification remain `P`.

The 2026-09-05 17:14 +08:00 inventory contains 347 Markdown files: six original main plans,
the original index, the semantic-inference ledger, 98 original dated leaves,
eight optimize main/index files, and 233 optimize dated records. The latter
include 230 historical records, this crosswalk, the ce04018c resolve record and
the active navigation-alias record. Every dated record in this inventory is linked below; there are no archived subdirectories
to exclude. The semantic-inference ledger contains 91 output rows linking 90
distinct leaves, so it alone is not a complete original-leaf inventory.

Task numbers are not unique record identities: O03 Task 1.3/1.4 and Task 5.1-5.4
were reused in later dated records. Both records are retained and distinguished
by date and title. O03 Task 7.25 (RED) and Task 7.34 (audit) exist only in the
main-plan narrative and are mapped separately below; Task 7.63 has no committed
completion leaf at this baseline.

### Remaining Responsibility Ledger

| Pending responsibility or superseded wording | Evidence and disposition | Continuing owner |
| --- | --- | --- |
| Full current semantic baseline | c95e5387 excludes the active symbol-projection/type-query overlays; ce04018c changes only the resolve leaf. Wait for their exact commits, then record source/status/toolchains, all actual failures and current extension/WASM assets. | [O00 Tasks 1/5][o00], then [O03 Task 8][o03] and [O06 Tasks 6/7][o06]. |
| Source/binary/native no-source relations and generation changes | Task 3.13 only adds the virtual URI carrier; Tasks 3.18/3.19 test generation identity and synthetic relations. Tasks 3.20-3.25 add external/import consumers but explicitly leave parser-owned no-source virtual URI production and real multiprovider nonzero-generation reload pending. | [O03 Task 3][o03]; consumers [O04 Tasks 1-3/8][o04], Web [O05 Tasks 3/4/6][o05]. |
| Remaining local semantic reconstruction | Task 7.62 deletes use-site collection; its boundary still names `Lsp_FindSymbolAtUsageOrDefinition` scope/range fallback and retained reference storage. Active Astra projection and Task 7.63 `ResolveTypeAtPosition` are not completed evidence. | [O03 Task 7][o03]; all-source/stale/unresolved matrix in Task 8; module boundary acceptance in [O06 Tasks 1-3][o06]. |
| Closed generic detail and missing `possibly_uninitialized_read` | Both fail in the unchanged broad GCC baseline and still fail after ce04018c. Task 7.61's focused generic producer completion does not prove this protocol path. | [O03 Tasks 7/8][o03], then [O04 Tasks 6/8][o04]. |
| Legacy short-circuit/tuple/import/fixture failures | Task 7.39 changes the obsolete diagnostic expectation, Task 5.17 repairs the keywordless tuple fixture, Tasks 3.20/3.21 close local import/token/reference failures, Tasks 7.60/7.61 close scoped analyzer fixture/generic failures. These supersede those exact old causes; the original 10/16 audit is still historical, not a new all-green result. | [O03 Task 8][o03] must rerun the original 16 targets and three stdio/CLI suites on the integrated commit. |
| Remaining capability truth | Identity-only resolve is C at ce04018c. Declaration/typeDefinition aliases, null-only willCreate/willDelete, 3.18 negotiation, typed color contract and server/extension version parity are separate pending obligations. Local implementation and native workspace-folder handling already have evidence and are not removed by stale review wording. | [O00 Tasks 2/4/5][o00]; real contracts [O04 Tasks 1/6/7][o04], generated negotiation [O05 Tasks 1/4/5][o05]. |
| Navigation-alias leaf positive fixture | The new [Task 4 Sub2 record](2026-09-05-plan00-task04-sub02-navigation-aliases.md) reproduces the declaration overclaim and records an unresolved reaching-write definition expectation on ce04018c. Neither the alias withdrawal nor that semantic investigation is accepted at this inventory snapshot. | [O00 Task 4][o00]; definition fact/query investigation [O03 Tasks 3/7/8][o03], full semantic navigation [O04 Task 1][o04]. |
| Native transport/cancel replay | O01 historical sanitizer/Valgrind/Helgrind/MSVC evidence is C. First current Clang cancel-known timeout followed by isolated pass is not an explained closure and does not relax the 50 ms cancellation budget. | [O01 Tasks 4/6][o01] and [O06 Task 4][o06]. |
| Web error/strict-type/workspace parity | Shared diagnostic store and WASM serializer are C slices, not proof of full workspace synchronization or shared errors. Configured unit/compile/noEmit pass; explicit strict worker check still has 17 baseline/current errors, delta zero. | [O05 Tasks 1-7][o05]; packaged native/browser/desktop-Web acceptance [O06 Task 6][o06]. |
| Migration, safe replacements and module actions | The 22 original diagnostic leaves close their named baseline/insertion contracts; O03 Task 6 closes structured semantic diagnosis. They do not close registry-wide migration/replacement applicability or manifest/multidocument edits. | Syntax 06 contract first; [O03 Tasks 6/7][o03], [O04 Tasks 3/6/8][o04], shared Web edit contract [O05 Task 2][o05]. |
| Debug/REPL integration | Original E1-E5 leaves are C; stale "next E2b/E3/E4 children handles" wording is S by the later dated leaves. There is no authorization here to redesign debugger-owned runtime work. | Existing [LSP 04 E1-E5](../04-debug-and-repl.md); shared query regression [O03 Task 8][o03], inline values [O04 Task 7][o04], final integration [O06 Task 6][o06]. |
| Old baseline record's "Task 6" aggregate gap | O00 has only Tasks 1-5. The unnamed "Task 6" in the 2026-08-22 baseline record is an obsolete reference, not an extra milestone. | Current aggregate evidence belongs to [O00 Tasks 1/5][o00] and final [O06 Task 6][o06]. |
| Old `partial` and optional desktop-Web/budget language | `partial` in O03 Task 3 is not permission to invent syntax. User-approved syntax precedence, required desktop Web, and frozen existing latency/memory limits govern this execution. O06's suggested alternative thresholds do not silently replace those limits. | Existing syntax contracts; [O03 Task 3][o03], [O05 Task 5][o05] and [O06 Tasks 5/6][o06]. |

No residual responsibility in the read records is left without a plan owner.
The independently owned Debug and active semantic work remain explicit
dependencies, not completed optimize outputs or omitted requirements.

## Original Leaf Inventory

### Original Semantic Leaves

These 22 leaves map to [O03 Tasks 1-7][o03]; all-provider/stale/unresolved acceptance remains [O03 Task 8][o03], editor behavior [O04][o04] and runtime parity [O05][o05]. Older L8 display records without a status table are supported by their Evidence/Validation Matrix/Open Scope sections and the dated semantic-inference ledger.

| Dated leaf | Output and evidence boundary |
| --- | --- |
| [2026-06-20 Semantic Fact Query Baseline](../01-semantic-core/2026-06-20-semantic-fact-query-baseline.md) | C: initial fact/query mechanism; full Q1-Q6 coverage P. |
| [2026-07-06 Numeric Range Microcase Evidence](../01-semantic-core/2026-07-06-numeric-range-microcase-evidence.md) | C: numeric-range microcase evidence only; no parent promotion. |
| [2026-07-20 Native Descriptor Function Callable Parity](../01-semantic-core/2026-07-20-native-descriptor-function-callable-parity.md) | C: descriptor module functions; G/C/M 16-target and stdio/CLI. |
| [2026-07-20 Native Generic Receiver Callable Parity](../01-semantic-core/2026-07-20-native-generic-receiver-callable-parity.md) | C: unconstrained generic instance methods; G/C/M 16-target and stdio/CLI; constrained/effectful/provider completeness P. |
| [2026-07-20 Native Receiver Method Callable Parity](../01-semantic-core/2026-07-20-native-receiver-method-callable-parity.md) | C: native/descriptor instance methods; G/C/M 16-target and stdio/CLI. |
| [2026-07-24 Canonical Property Consumer Parity](../01-semantic-core/2026-07-24-canonical-property-consumer-parity.md) | C: source/.zro property M1-M5 and imported-placeholder support; G/C/M evidence linked from Syntax 05. |
| [2026-08-10 Canonical Declaration Type Display](../01-semantic-core/2026-08-10-canonical-declaration-type-display.md) | C: source declaration display; G/C/M analyzer/interface/stdio. |
| [2026-08-10 Canonical Inlay Declaration Type](../01-semantic-core/2026-08-10-canonical-inlay-declaration-type.md) | C: exact declaration identity; G/C/M query 29/29, inlay 10/10, interface/stdio. |
| [2026-08-10 Canonical Member Token Fail-Closed](../01-semantic-core/2026-08-10-canonical-member-token-fail-closed.md) | C: source member classification; G/C/M interface/stdio; provider completeness P. |
| [2026-08-10 Canonical Owner Type Token Identity](../01-semantic-core/2026-08-10-canonical-owner-type-token-identity.md) | C: exact owner type token; G/C/M query 28/28 and interface. |
| [2026-08-10 Canonical Project Symbol Type](../01-semantic-core/2026-08-10-canonical-project-symbol-type.md) | C: project-source imported symbol type; G/C/M facts, local/interface/project and stdio/CLI. |
| [2026-08-10 Canonical Property Signature Type](../01-semantic-core/2026-08-10-canonical-property-signature-type.md) | C: canonical PropertyDef display; G/C/M expression/local/interface/project/stdio. |
| [2026-08-10 Canonical Receiver Member Type](../01-semantic-core/2026-08-10-canonical-receiver-member-type.md) | C: source receiver declaration type; b452bb5 support precedes G/C/M facts/local/interface/project/stdio. |
| [2026-08-10 Canonical Symbol Documentation Type](../01-semantic-core/2026-08-10-canonical-symbol-documentation-type.md) | C: exact symbol documentation type; G/C/M expression 9/9, inlay 10/10 and consumer/stdio gates. |
| [2026-08-11 Canonical Native Construct Completion Expression Fact](../01-semantic-core/2026-08-11-canonical-native-construct-completion-expression-fact.md) | C: native construct completion exact-fact/fail-closed slice; G/C/M consumer/stdio evidence. |
| [2026-08-11 Canonical Native Construct Receiver Expression Fact](../01-semantic-core/2026-08-11-canonical-native-construct-receiver-expression-fact.md) | C: native construct receiver exact expression fact; G/C/M acceptance record. |
| [2026-08-12 Canonical Direct-Call Signature Expression Fact](../01-semantic-core/2026-08-12-canonical-direct-call-signature-expression-fact.md) | C: free-call 04:40 and receiver-call 11:42 contracts; separate G/C/M coverage in one leaf. |
| [2026-08-12 Canonical Generic Receiver Signature Fact](../01-semantic-core/2026-08-12-canonical-generic-receiver-signature-fact.md) | C: closed generic receiver fact-only signature; G/C/M consumer/project/stdio evidence. |
| [2026-08-12 Canonical Native Construct Signature Expression Fact](../01-semantic-core/2026-08-12-canonical-native-construct-signature-expression-fact.md) | C: native struct-init signature; G/C/M exact-fact acceptance. |
| [2026-08-13 Canonical Callable-Value Signature Fact](../01-semantic-core/2026-08-13-canonical-callable-value-signature-fact.md) | C: source callable values; G/C/M canonical 18/18 plus consumer/project/stdio evidence. |
| [2026-08-13 Canonical Closure Callable-Value Signature Fact](../01-semantic-core/2026-08-13-canonical-closure-value-signature-fact.md) | C: lambda identity/signature; G/C/M canonical 19/19; reported MSVC latency retries retain unchanged budgets. |
| [2026-08-14 External Callable-Value Canonical Facts](../01-semantic-core/2026-08-14-external-callable-value-canonical-facts.md) | C at 2026-08-22 18:23: external value TypeId/signature, explicitly unresolved SymbolId; G/C/M full leaf gates; L8 P. |

### Original Diagnostic Leaves

These 22 leaves map to [O03 Task 6][o03] for the producer/query contract and [O04 Tasks 3/6/8][o04] for remaining edit applicability and protocol safety. The older leaf's statement that the next named delimiter is unfinished is S only where a later named leaf below completes it; replacement/migration/multidocument responsibilities remain P.

| Dated leaf | Output and evidence boundary |
| --- | --- |
| [2026-07-19 Structured Diagnostic Baseline](../02-diagnostics/2026-07-19-structured-diagnostic-baseline.md) | C: registry/JSON/fix baseline; later O03 Task 6 closure supplies the broader structured producer gate. |
| [2026-07-21 Array-Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-array-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Array Element-Separator Safe-Fix Convergence](../02-diagnostics/2026-07-21-array-element-separator-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Call-Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-call-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Condition-Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-condition-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Conditional Colon Safe-Fix Convergence](../02-diagnostics/2026-07-21-conditional-colon-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Group-Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-group-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Index-Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-index-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Object-Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-object-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Object Computed-Key Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-object-computed-key-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Object Property-Colon Safe-Fix Convergence](../02-diagnostics/2026-07-21-object-property-colon-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Object Property-Separator Safe-Fix Convergence](../02-diagnostics/2026-07-21-object-property-separator-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Parameter-List-Close Safe-Fix Convergence](../02-diagnostics/2026-07-21-parameter-list-close-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-07-21 Semicolon Safe-Fix Convergence](../02-diagnostics/2026-07-21-semicolon-safe-fix-convergence.md) | C: named local insertion/fail-closed contract; G/C/M 18-target, stdio/CLI and diagnostic-fix smoke. |
| [2026-08-05 Block-Close Safe-Fix Convergence](../02-diagnostics/2026-08-05-block-close-safe-fix-convergence.md) | C: integrated parser 38/38 and advanced editor G/C/M; complete stdio/CLI replay P. |
| [2026-08-05 Catch-Pattern Close Safe-Fix Convergence](../02-diagnostics/2026-08-05-catch-pattern-close-safe-fix-convergence.md) | C: integrated parser 38/38 and advanced editor G/C/M; complete stdio/CLI replay P. |
| [2026-08-05 Declaration-Body Close Safe-Fix Convergence](../02-diagnostics/2026-08-05-declaration-body-close-safe-fix-convergence.md) | C: parser 34/34 and advanced editor G/C/M; this leaf's complete stdio/CLI replay P. |
| [2026-08-05 Declaration-Body Open Safe-Fix Convergence](../02-diagnostics/2026-08-05-declaration-body-open-safe-fix-convergence.md) | C: integrated parser 38/38 and advanced editor G/C/M; complete stdio/CLI replay P. |
| [2026-08-05 Statement-Body Open Safe-Fix Convergence](../02-diagnostics/2026-08-05-statement-body-open-safe-fix-convergence.md) | C: integrated parser 38/38 and advanced editor G/C/M; complete stdio/CLI replay P. |
| [2026-08-05 Using-Resource Close Safe-Fix Convergence](../02-diagnostics/2026-08-05-using-resource-close-safe-fix-convergence.md) | C: parser 39/39 and advanced editor G/C/M; complete stdio/CLI replay P. |
| [2026-08-08 For/Foreach Header Safe-Fix Convergence](../02-diagnostics/2026-08-08-for-foreach-header-safe-fix-convergence.md) | C: four insertion contracts, ae63bef integration; parser 43/43 and advanced editor G/C/M; full protocol gate P. |
| [2026-08-08 Switch/Extern Close Safe-Fix Convergence](../02-diagnostics/2026-08-08-switch-extern-close-safe-fix-convergence.md) | C: three insertion contracts; parser 46/46 and advanced editor G/C/M; full protocol gate P. |

### Original Robustness Leaves

These 32 leaves preserve original L6 acceptance. Unchecked later work is owned by [O01][o01], [O02][o02], [O05 Tasks 3/6/7][o05] and [O06 Tasks 4-7][o06]. Public type/layout/package/artifact identity completeness additionally depends on [O03 Task 3][o03]. The historical 256 MiB limit measures cache storage, while 512 MiB measures native process peak.

| Dated leaf | Output and evidence boundary |
| --- | --- |
| [2026-06-20 Position And Robustness Baseline](../03-robustness/2026-06-20-position-robustness-baseline.md) | C: initial range/position evidence; later strict snapshot/protocol gates remain distinct. |
| [2026-07-19 Canonical Signature Help Provider Parity](../03-robustness/2026-07-19-canonical-signature-help-provider-parity.md) | C: extern parameter facts; G/C/M canonical 5/5, signature 9/9, interface 87/87 and stdio/CLI. |
| [2026-07-19 Identical-Content Snapshot And Semantic Cache Reuse](../03-robustness/2026-07-19-identical-content-snapshot-cache-reuse.md) | C: unchanged-text reuse/counters; G/C/M 14-target/stdio; later declaration reparse supersedes full-reparse-only boundary. |
| [2026-07-19 Minimal Change Range And Declaration Classification](../03-robustness/2026-07-19-minimal-change-range-and-declaration-classification.md) | C: old/new byte range and declaration classification; G/C/M 14-target/stdio. |
| [2026-07-19 Owning-Function Scoped Query Cache Preservation](../03-robustness/2026-07-19-owning-function-scoped-query-cache-preservation.md) | C: stable untouched scope reuse/ownership; G/C/M 14-target, incremental and stdio. |
| [2026-07-19 Scoped Query Semantic Cache](../03-robustness/2026-07-19-scoped-query-semantic-cache.md) | C: scoped cache isolation/reuse; G/C/M 14-target, local/interface/incremental/stdio. |
| [2026-07-19 Scoped Semantic Analysis Foundation](../03-robustness/2026-07-19-scoped-semantic-analysis-foundation.md) | C: declaration-scoped query support; G/C/M 14-target; not a claim of full incremental analysis. |
| [2026-07-19 Strict Document Version Rejection](../03-robustness/2026-07-19-strict-document-version-rejection.md) | C: same/stale version rejection; G/C/M 14-target/incremental/stdio. |
| [2026-07-19 Token-Equivalent Semantic Snapshot Reuse](../03-robustness/2026-07-19-token-equivalent-semantic-snapshot-reuse.md) | C: exact token/value/coordinate reuse; G/C/M 14-target/incremental/stdio. |
| [2026-07-20 Binary Export Declaration Identity](../03-robustness/2026-07-20-binary-export-declaration-identity.md) | C: artifact range and metadata coordinate adapter; G/C/M 16-target/stdio; sourceless non-ASCII precision P. |
| [2026-07-20 Canonical Source Public-Contract Hash](../03-robustness/2026-07-20-canonical-source-public-contract-hash.md) | C: source hash v1; G/C/M 16-target/stdio; broad type/layout/artifact/provider hash parity P. |
| [2026-07-20 Descriptor Plugin Type Member Parity](../03-robustness/2026-07-20-descriptor-plugin-type-member-parity.md) | C: descriptor type-member projection and last-good snapshot; G/C/M 16-target/stdio. |
| [2026-07-20 Local Signature And Generic Direct-Dependency Invalidation](../03-robustness/2026-07-20-local-signature-generic-direct-dependency-invalidation.md) | C: direct-call dependency invalidation; G/C/M 16-target/stdio. |
| [2026-07-20 ModuleIdentity Edge Migration](../03-robustness/2026-07-20-module-identity-edge-migration.md) | C: same-root source rename old/new identity propagation; G/C/M 16-target/stdio; package/artifact replacement P. |
| [2026-07-20 ModuleIdentity Reverse-Dependency Invalidation](../03-robustness/2026-07-20-module-identity-reverse-dependency-invalidation.md) | C: measured importer preservation/invalidation; G/C/M 15-target/project/stdio. |
| [2026-07-20 Resolved Callable Consumer Convergence](../03-robustness/2026-07-20-resolved-callable-consumer-convergence.md) | C: source receiver call identity/display/diagnostics; G/C/M 16-target/stdio; full providers P. |
| [2026-07-20 Source Rename Workspace Edit Snapshot Revalidation](../03-robustness/2026-07-20-source-rename-workspace-edit-snapshot-revalidation.md) | C: batch fingerprint/captured-version revalidation; G/C/M 16-target/stdio. |
| [2026-07-20 Canonical Source Rename Workspace Edits](../03-robustness/2026-07-20-source-rename-workspace-edits.md) | C: same-root source rename planning; G/C/M 16-target/stdio; its later snapshot gate is separately linked. |
| [2026-07-21 Code Action Workspace Edit Snapshot Revalidation](../03-robustness/2026-07-21-code-action-workspace-edit-snapshot-revalidation.md) | C: native code-action snapshot/resolve checks; G/C/M 17-target/stdio; retained material resolver at ce04018c. |
| [2026-07-21 General Rename Workspace Edit Snapshot Revalidation](../03-robustness/2026-07-21-general-rename-workspace-edit-snapshot-revalidation.md) | C: ordinary/source rename capture/validate; G/C/M 16-target/stdio. |
| [2026-08-08 100 File Workspace Incremental Latency Budget](../03-robustness/2026-08-08-100-file-workspace-incremental-latency-budget.md) | C: 20 samples, 500 ms p95; MSVC repeated evidence, later G/C/M broad replay. |
| [2026-08-08 Rapid Stdio Stale-Response Churn](../03-robustness/2026-08-08-rapid-stdio-stale-response-churn.md) | C: 100 edit/cancel/close cycles; 2026-08-10 correction accepts only exact linearized version or lifecycle error. |
| [2026-08-08 Single Document Diagnostics Latency Budget](../03-robustness/2026-08-08-single-document-diagnostics-latency-budget.md) | C: 20 versioned reports, 250 ms p95; MSVC repeated evidence, later G/C/M broad replay. |
| [2026-08-08 Stdio Cancellation Lifecycle](../03-robustness/2026-08-08-stdio-cancellation-lifecycle.md) | C: exact-id cancellation, 50 ms bound; initial Linux harness limitation superseded by later full native matrices. |
| [2026-08-08 Stdio Content-Modified Fence](../03-robustness/2026-08-08-stdio-content-modified-fence.md) | C historically; global input-generation design S by O01 typed registry and O02 actual-dependency fence. |
| [2026-08-08 Two Historical Text Snapshots](../03-robustness/2026-08-08-two-historical-text-snapshots.md) | C: current plus two historical text snapshots; held references survive rollover. |
| [2026-08-08 Warm Request Latency Budget](../03-robustness/2026-08-08-warm-request-latency-budget.md) | C: hover 50 ms, completion/signature 100 ms p95; 20 samples and ten MSVC repetitions. |
| [2026-08-09 Semantic Cache Storage Accounting](../03-robustness/2026-08-09-semantic-cache-storage-accounting.md) | C: exact cache-storage accounting/release/rehydration, not total semantic/AST memory. |
| [2026-08-09 Two Historical Semantic Snapshots](../03-robustness/2026-08-09-two-historical-semantic-snapshots.md) | C: two semantic histories and safe AST ownership; G/C/M interface/local-query. |
| [2026-08-10 L6 Final Stdio and CLI Matrix](../03-robustness/2026-08-10-l6-final-stdio-cli-matrix.md) | C: L6 native gate, G/C/M 33/33 registered stdio/CLI; current integrated replay P. |
| [2026-08-10 Stdio Process Peak Memory](../03-robustness/2026-08-10-stdio-process-peak-memory.md) | C: OS process high-water 512 MiB gate; G/C/M broad stdio. |
| [2026-08-10 Workspace Semantic Cache LRU](../03-robustness/2026-08-10-workspace-semantic-cache-lru.md) | C: 256 MiB exact cache-storage LRU; G/C/M interface/local-query. |

### Original Debug Leaves

These 22 E1-E5 leaves are C at their stated Debug/DAP/parser/runtime boundary. They remain owned by [Original LSP 04](../04-debug-and-repl.md); changes to their shared facts/snapshots must pass [O03 Task 8][o03] and [O06 Task 6][o06], and inline values belong to [O04 Task 7][o04]/[O05 Task 4][o05].

| Dated leaf | Output and evidence boundary |
| --- | --- |
| [2026-07-28 E1a Canonical Local Binding Artifact](../04-debug-and-repl/2026-07-28-e1a-canonical-local-binding-artifact.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-07-28 E1b1 Paused Frame Canonical Bindings](../04-debug-and-repl/2026-07-28-e1b1-paused-frame-canonical-bindings.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-07-28 E1b2a Receiver Canonical Carrier](../04-debug-and-repl/2026-07-28-e1b2a-receiver-canonical-carrier.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-07-28 E1b2b1 Reflection Generic Context](../04-debug-and-repl/2026-07-28-e1b2b1-reflection-generic-context.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-07-28 E2a Formal Expression Fragment Parser](../04-debug-and-repl/2026-07-28-e2a-formal-expression-fragment-parser.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-07-28 E2b0 Canonical Binding Injection](../04-debug-and-repl/2026-07-28-e2b0-canonical-binding-injection.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-07-29 E2b1 Paused-Frame Canonical Binding Integration](../04-debug-and-repl/2026-07-29-e2b1-paused-frame-canonical-binding-integration.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-07-31 E2b2 Canonical Place Reference Carrier](../04-debug-and-repl/2026-07-31-e2b2-canonical-place-reference-carrier.md) | C: PlaceId carrier; isolated G/C/M parser/debug overlay and final clean-head evidence remain distinguished. |
| [2026-08-01 E2b3 Generation-Checked Runtime Root](../04-debug-and-repl/2026-08-01-e2b3-generation-checked-runtime-root.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-01 E2b4 Structured Runtime-Root Reference Origin](../04-debug-and-repl/2026-08-01-e2b4-structured-runtime-root-reference-origin.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-01 E2b5 Generation-Checked Runtime-Root Consumer](../04-debug-and-repl/2026-08-01-e2b5-generation-checked-runtime-root-consumer.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E2b6a Canonical Closure-Capture Artifact Identity](../04-debug-and-repl/2026-08-02-e2b6a-canonical-closure-capture-artifact-identity.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E2b6b Generation-Checked Paused-Frame Closure Resolver](../04-debug-and-repl/2026-08-02-e2b6b-generation-checked-paused-frame-closure-resolver.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E2b6c Closure-Capture Origin And Token Facts](../04-debug-and-repl/2026-08-02-e2b6c-closure-capture-origin-token-facts.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E2b6d Closure-Capture Formal Consumer](../04-debug-and-repl/2026-08-02-e2b6d-closure-capture-formal-consumer.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E3a DAP Evaluate Capability Context](../04-debug-and-repl/2026-08-02-e3a-dap-evaluate-capability-context.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E3b Conditional Breakpoint Pure Policy](../04-debug-and-repl/2026-08-02-e3b-conditional-breakpoint-pure-policy.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E3c Logpoint Pure Policy](../04-debug-and-repl/2026-08-02-e3c-logpoint-pure-policy.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E4a Canonical Evaluate Result Transport](../04-debug-and-repl/2026-08-02-e4a-canonical-evaluate-result-transport.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-02 E4b Structured Evaluate Failure Transport](../04-debug-and-repl/2026-08-02-e4b-structured-evaluate-failure-transport.md) | C: named canonical Debug contract; detailed focused toolchains, scope and source identity remain in the linked record. |
| [2026-08-05 E4c Generation-Checked Children Handles](../04-debug-and-repl/2026-08-05-e4c-generation-checked-children-handles.md) | C: resume invalidation/monotonic handle allocation/exhaustion; closes earlier E4 children-handle pending clause. |
| [2026-08-05 E5 REPL Closure Generations](../04-debug-and-repl/2026-08-05-e5-repl-closure-generations.md) | C: submission binding 11/11, facts 12/12, escape 13/13, query 27/27 and CLI/stdio G/C/M; debugger-owned scope. |

## Optimize Leaf Inventory

### Baseline, Transport And Snapshot Records

These 25 records preserve [O00][o00], [O01][o01] and [O02][o02] history.
Original unchecked lists are S only for the exact completed outputs below.
Current baseline and zero-overclaim checks remain O00 Tasks 1/4/5, current
transport replay remains O01 Task 6, and complete Web/integrated snapshot
acceptance remains [O05 Tasks 3/6/7][o05] and [O06 Tasks 4-7][o06].

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-22 LSP Optimize Baseline Contract Record](2026-08-22-baseline-contract.md) | C: 130907d baseline with ceadabbf ancestry; inventory/evidence output only. Current full baseline P in O00 T1/5. |
| [2026-08-22 LSP Bounded Stdio Frame Reader](2026-08-22-bounded-frame-reader.md) | C: limits/CRLF/Content-Length/charset/error categories; revalidated after teardown. |
| [2026-08-22 LSP Call Hierarchy Partial Results](2026-08-22-call-hierarchy-partial-results.md) | C: incoming/outgoing partial-array transport; semantic target correctness is separate. |
| [2026-08-22 LSP Capability Registry Record](2026-08-22-capability-registry.md) | C: 33-entry descriptor/validation/test foundation; live initialize/manifest migration P in O00 T4/5 and O05 T1. |
| [2026-08-22 LSP JSON-RPC Envelope And Params](2026-08-22-json-rpc-envelope.md) | C: shared envelope/status, strict numeric/range params; G/C-asan/M 29-case replay. |
| [2026-08-22 LSP Protocol Conformance RED Driver](2026-08-22-protocol-conformance-red.md) | C: shared client and 16-case RED driver, originally 8 implemented/8 gaps; those transport gaps S by O01 acceptance. |
| [2026-08-22 LSP Stdio Lifecycle State Machine](2026-08-22-protocol-lifecycle.md) | C: explicit lifecycle/error/exit state; revalidated 2026-08-23 in 29-case driver. |
| [2026-08-22 LSP Stdio Protocol Trace](2026-08-22-protocol-trace.md) | C: stderr-only trace modes; remaining transport replay O01 T6. |
| [2026-08-22 LSP References Partial Results](2026-08-22-references-partial-results.md) | C: reference partial-array schema and final null response. |
| [2026-08-22 LSP Typed Request Registry](2026-08-22-request-registry.md) | C: typed id registry and exact cancellation; global inputGeneration removed. Later dependency fence is O02 T3. |
| [2026-08-22 LSP Protocol Task 5 Deterministic Teardown](2026-08-22-stdio-deterministic-teardown.md) | C at 2026-08-23 03:27: 100 cycles/fault points, G/C ASan+UBSan, Valgrind/Helgrind, M Debug/ASan. Earlier in_progress rows retained as history. |
| [2026-08-22 LSP Protocol Task 4 Request Progress And Cancellation](2026-08-22-task4-request-progress-and-cancellation.md) | C: O01 T4 aggregate; known-id cancellation/document-sync replay 2026-08-23. |
| [2026-08-22 LSP Type Hierarchy Partial Results](2026-08-22-type-hierarchy-partial-results.md) | C: super/subtype partial-array transport; semantic target correctness is separate. |
| [2026-08-22 LSP Unified Request Cancellation](2026-08-22-unified-request-cancellation.md) | C: unified callback and long-loop checks; known-id protocol replay added 2026-08-23. |
| [2026-08-22 LSP Request Work-Done Progress](2026-08-22-workdone-progress.md) | C: typed request progress tokens and framed begin/end; per-result partial leaves below. |
| [2026-08-22 LSP Workspace Diagnostic Partial Results](2026-08-22-workspace-diagnostic-partial-results.md) | C: report-shaped items batches and final null response. |
| [2026-08-22 LSP Workspace Symbol Partial Results](2026-08-22-workspace-symbol-partial-results.md) | C: bounded 64-item batches, exact token/cancellation, final null response. |
| [2026-08-23 Plan 02 Task 1: Canonical File URI Paths](2026-08-23-canonical-file-uri-paths.md) | C: O02 T1, G/C URI 14/14, M 16/16 plus protocol/document sync. |
| [2026-08-23 Plan 02 Task 4: Strict Document Synchronization](2026-08-23-document-sync-validation.md) | C: O02 T4, transactional sync and desynchronized recovery; G focused plus eight CTests; later O02 T7 covers native G/C/M. |
| [2026-08-23 Plan 02 Task 5: Incremental Declaration Reparse](2026-08-23-incremental-declaration-reparse.md) | C: O02 T5, declaration reparse or explicit full fallback; G four direct tests; later O02 T7 differential coverage. |
| [2026-08-23 Plan 02 Task 7: Acceptance Gates](2026-08-23-plan02-acceptance-gates.md) | C: O02 T7 native G/C/M URI/snapshot, 10,000 edits, 7/7 workspace diagnostics, 12/12 folders and stdio; browser acceptance P. |
| [2026-08-23 LSP Protocol Lifecycle And Transport Acceptance](2026-08-23-protocol-lifecycle-transport-acceptance.md) | C: O01 T1-4, G Debug/C ASan+UBSan/M Debug five CTests and 29 protocol cases. |
| [2026-08-23 Plan 02 Task 6: Pull/Push Diagnostics](2026-08-23-pull-push-diagnostics.md) | C: O02 T6, shared store/push version cache/workspace enumeration; G/C/M native; initial WASM object-only limitation retained. |
| [2026-08-23 Plan 02 Task 3: Semantic Snapshot Dependency Fence](2026-08-23-semantic-snapshot-dependency-fence.md) | C: O02 T3, immutable identity/actual dependencies/shared resultId; G snapshot 28/28 and interface/stdio. |
| [2026-08-23 Plan 02 Task 2: Workspace Folder Project Collection](2026-08-23-workspace-folder-project-collection.md) | C: O02 T2, multi-root/root-event/overlay retention; G/C/M focused protocol; current native workspace test also passes. |

### Plan03 Query Contract Records

Eight records preserve [O03 Task 1][o03]. Task 1's four implementation boxes are C; later strict source-identity corrections retain their narrower evidence. Pending owner: O03 Task 8 for the final original matrix and current source replay.

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-24 Task 1: Query Purity And Parity](2026-08-24-plan03-task01-query-purity.md) | C: final Task 1 record at 02:56; G/C/M contract 3/3, query 29/29, compiler diagnostics 46/46, call/member 4/4 and parity 3/3. |
| [2026-08-26 Task 1.3: CanonicalTypeAt Exactness Gate](2026-08-26-plan03-task01-canonical-type-exactness.md) | C: exactness/output fail-closed correction; its reused Task 1.3 id is distinct from the later CFG leaf. |
| [2026-08-26 Task 1.4: ReferencesOf Output Clearing](2026-08-26-plan03-task01-reference-output-clear.md) | C: reused reference-array RED Expected 0 Was 2, query GREEN 30/30; distinct from later Task 1.4. |
| [2026-08-31 Task 1.3: CFG Dataflow Source Identity](2026-08-31-plan03-task01-cfg-dataflow-source-identity.md) | C: CFG/definition source gate; G/C focused including type inference 124/124; M/full matrix P. |
| [2026-08-31 Task 1.4: Ownership Dataflow Source Identity](2026-08-31-plan03-task01-ownership-dataflow-source-identity.md) | C: ownership range/source gate; G/C focused, interface fixed8 delta zero; M/full matrix P. |
| [2026-08-31 Task 1.5: Property Query Source Identity](2026-08-31-plan03-task01-property-query-source-identity.md) | C: PropertyAt/source-bound code-action request; G/C focused and property refactor PASS; M/full matrix P. |
| [2026-08-31 Task 1.1: Query Contract State Reconciliation](2026-08-31-plan03-task01-query-contract-state-reconciliation.md) | C: four stale Task 1 checkboxes reconciled to 2026-08-24 evidence; G/C contract/parity replay, not new full M acceptance. |
| [2026-08-31 Task 1.2: Semantic Fact Source Identity](2026-08-31-plan03-task01-semantic-fact-source-identity.md) | C: exact optional source identity and corrected reaching-definition fixtures; G/C focused, interface fixed8 delta zero; M/full matrix P. |

### Plan03 Symbol Records

Eighteen records preserve [O03 Task 2][o03]. The August 31 reconciliation closes the stale Task 2 wording but does not turn early Windows compiler/link limitations into historical passes. Pending owners: O03 Tasks 3/7 for external relations and consumer closure, Task 8 for source/binary/native/stale/full toolchain acceptance.

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-24 Task 2.2b: Source Lexical Scope Facts](2026-08-24-plan03-task02-source-scopes.md) | C: source lexical-scope producer; G/M executable evidence, historical Clang static link blocked. |
| [2026-08-24 Task 2.1: Canonical SymbolAt](2026-08-24-plan03-task02-symbol-at.md) | C: canonical SymbolAt, G/C/M new 2/2 and query/diagnostic gates; owner identity initially unavailable by contract. |
| [2026-08-24 Task 2.2a: Scope Facts and Visible Symbols](2026-08-24-plan03-task02-visible-symbols.md) | C: scope/candidate API and filtering; G/M executable evidence, historical Clang static link blocked. |
| [2026-08-25 Task 2.3c: Binary Generic Callable Identity](2026-08-25-plan03-task02-binary-generic-callable-identity.md) | C: artifact generic wire rows and exact callable child binding; binary inferred/explicit generic RED/GREEN. |
| [2026-08-25 Task 2.2l: Destructured Import and Type-Value Alias Facts](2026-08-25-plan03-task02-destructured-import-type-value-alias.md) | C: destructured/type-value alias facts; M focused, G/C historical execution gate unavailable. |
| [2026-08-25 Task 2.2k: Direct Import Alias Visibility Facts](2026-08-25-plan03-task02-direct-import-alias.md) | C: import alias visibility/opt-in; M focused, G/C not replayed in this leaf. |
| [2026-08-25 Task 2.3b: Native Generic Receiver Identity](2026-08-25-plan03-task02-native-generic-receiver-identity.md) | C: native generic stable identity/closed TypeId; M focused; binary/full toolchain gates deferred. |
| [2026-08-25 Task 2.3a: Native Module Function Identity](2026-08-25-plan03-task02-native-module-function-identity.md) | C: native non-generic function producer; M focused; origin/visible/provider/full toolchain gates deferred. |
| [2026-08-25 Task 2.2g: Source Class Method Generic Scope Facts](2026-08-25-plan03-task02-source-class-method-generics.md) | C: class method generics/parentage; G/M executable evidence, C source compile only. |
| [2026-08-25 Task 2.2h: Source Const Generic Scope Facts](2026-08-25-plan03-task02-source-const-generics.md) | C: const generic scope/type identity; G/M executable evidence, C source compile only. |
| [2026-08-25 Task 2.2e: Source Function Generic Scope Facts](2026-08-25-plan03-task02-source-function-generics.md) | C: free-function generic scopes; G/M executable evidence, C source compile only. |
| [2026-08-25 Task 2.2i: Source Interface Method Generic Scope Facts](2026-08-25-plan03-task02-source-interface-method-generics.md) | C: interface method generic scopes; G/M executable evidence, C source compile only. |
| [2026-08-25 Task 2.2f: Source Struct Method Generic Scope Facts](2026-08-25-plan03-task02-source-struct-method-generics.md) | C: struct method generics/parentage; G/M executable evidence, C source compile only. |
| [2026-08-25 Task 2.2d: Source Type Generic Scope Facts](2026-08-25-plan03-task02-source-type-generics.md) | C: enclosing generic type scopes; G/M executable evidence, C source compile only. |
| [2026-08-25 Task 2.2j: Source Type Member Visibility Facts](2026-08-25-plan03-task02-source-type-members.md) | C: field/member/static visibility; M focused; G/C executable acceptance not obtained in this leaf. |
| [2026-08-25 Task 2.2c: Source Type Scope Facts](2026-08-25-plan03-task02-source-types.md) | C: source type scope/identity; G/M executable evidence, C static link blocked. |
| [2026-08-31 Task 2.3: Symbol Query State Reconciliation](2026-08-31-plan03-task02-symbol-query-state-reconciliation.md) | C: Task 2 checkbox reconciliation; G/C symbols 21/21, parity 15/15 and source contracts 70/70. |
| [2026-08-31 Task 2.4: Visible Symbol Source Identity](2026-08-31-plan03-task02-visible-symbol-source-identity.md) | C: strict source match and reused output; G/C symbols 22/22, parity 15/15, source contracts 70/70; M/full gate P. |

### Plan03 Relation Records

Twenty-five records preserve [O03 Task 3][o03]. The relation API/graph boxes are C; the two no-source/provider-matrix boxes remain P. Every leaf below inherits O03 Task 3 as the owner of actual multiprovider generation, metadata-owned virtual origin and sourceless relation completeness, with consumers in [O04 Tasks 1-3][o04] and final O03 Task 8.

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-25 Task 3.2: Property Accessor Relations](2026-08-25-plan03-task03-property-accessor-relations.md) | C: graph/property producer foundation and scoped regression; full relation/provider matrix P. |
| [2026-08-25 Task 3.1: Relation Graph Foundation](2026-08-25-plan03-task03-relation-graph-foundation.md) | C: graph/property producer foundation and scoped regression; full relation/provider matrix P. |
| [2026-08-26 Task 3.10: Canonical Alias Target Relations](2026-08-26-plan03-task03-canonical-alias-target-relations.md) | C: exact alias SymbolId/TypeId edge; G/C/M relation 16/16; no independent target identity is invented. |
| [2026-08-26 Task 3.9: Canonical Relation Module Identities](2026-08-26-plan03-task03-canonical-relation-module-identities.md) | C: nominal/generic-definition module identity projection; G/C/M relation 15/15; no URI/name reconstruction. |
| [2026-08-26 Task 3.13: External Relation Locations](2026-08-26-plan03-task03-external-relation-locations.md) | C: owned origin/virtual URI carrier and no-source precondition; G/C/M relation 18/18. Actual sourceless metadata producer P. |
| [2026-08-26 Task 3.4: Source Import Origin Relations](2026-08-26-plan03-task03-import-origin-relations.md) | C: source import origin and exact binding producer, 0f1d59a plus direct-import coverage; external producer completeness P. |
| [2026-08-26 Task 3.11: Override Implementation Query](2026-08-26-plan03-task03-override-implementation-query.md) | C: reverse implementation/override query; G/C/M relation 16/16; transitive/provider completeness P. |
| [2026-08-26 Task 3.3: Declaration Definition Relations](2026-08-26-plan03-task03-reference-definition-relations.md) | C: resolved-write declaration/definition edges and compile_script publication-order coverage. |
| [2026-08-26 Task 3.5: Relation Output Clearing](2026-08-26-plan03-task03-relation-output-clear.md) | C: invalid-id reusable output RED/GREEN, relation 9/9. |
| [2026-08-26 Task 3.8: Source Constructor Relations](2026-08-26-plan03-task03-source-constructor-relations.md) | C: explicit constructor identity; M focused and G relations 14/14; synthesized/source-free constructor identity P. |
| [2026-08-26 Task 3.12: Source Interface Member Relations](2026-08-26-plan03-task03-source-interface-member-relations.md) | C: validated class/interface member pair; G/C/M relation 17/17; no new struct inheritance syntax. |
| [2026-08-26 Task 3.7: Source Override Relations](2026-08-26-plan03-task03-source-override-relations.md) | C: compiler-selected override pair; M relation 12/12 and adjacent focused gates; provider provenance P. |
| [2026-08-26 Task 3.6: Source Type Hierarchy Relations](2026-08-26-plan03-task03-source-type-hierarchy-relations.md) | C: compiler prototype/base/interface edges; M relation 11/11 and adjacent focused gates. |
| [2026-08-30 Task 3.14: Relation Append Idempotence](2026-08-30-plan03-task03-relation-append-idempotence.md) | C: G/C focused relation integrity/scope/order gate; current M/full provider/runtime acceptance P. |
| [2026-08-30 Task 3.15: Relation Scope Source Identity](2026-08-30-plan03-task03-relation-scope-source-identity.md) | C: G/C focused relation integrity/scope/order gate; current M/full provider/runtime acceptance P. |
| [2026-08-30 Task 3.16: Relation Stable Output Order](2026-08-30-plan03-task03-relation-stable-order.md) | C: G/C focused relation integrity/scope/order gate; current M/full provider/runtime acceptance P. |
| [2026-08-31 Task 3.19: Canonical Relation Identity Matrix](2026-08-31-plan03-task03-canonical-relation-identity-matrix.md) | C: characterization for same-name modules/open-closed generic/alias/overload; G/C 28/28. Real multiproject reload P. |
| [2026-08-31 Task 3.17: Relation Endpoint Identity Integrity](2026-08-31-plan03-task03-relation-endpoint-identity-integrity.md) | C: G/C focused relation integrity/scope/order gate; current M/full provider/runtime acceptance P. |
| [2026-08-31 Task 3.18: Relation Provider Generation Identity](2026-08-31-plan03-task03-relation-provider-generation-identity.md) | C: explicit generation field, equality/order and generation-zero unavailable; G/C focused. Real nonzero provider reload P. |
| [2026-09-01 Task 3.23: Canonical Import Literal Definition](2026-09-01-plan03-task03-canonical-import-literal-definition.md) | C: exact literal range, ImportOriginAt and relation consistency; G/C focused/interface. |
| [2026-09-01 Task 3.22: Canonical Import-Origin Definition](2026-09-01-plan03-task03-canonical-import-origin-definition.md) | C: source/binary/native module-entry metadata projection and fail-closed import alias; G/C focused/interface. |
| [2026-09-01 Task 3.21: Cross-Snapshot Project References](2026-09-01-plan03-task03-cross-snapshot-references.md) | C: exact external identity across two importers; G/C focused, full interface zero failures; Task 7.25 source-reference gap S in this scope. |
| [2026-09-01 Task 3.20: External Member Identity](2026-09-01-plan03-task03-external-member-identity.md) | C: exact metadata owner/token/hash/kind projection; G/C focused, interface fixed3 -> fixed1. |
| [2026-09-01 Task 3.25: Import-Chain Module Hop Identity](2026-09-01-plan03-task03-import-chain-module-hop-identity.md) | C: AST-independent module hop; G/C focused/interface. Completion cursor, virtual URI, actual generation and sourceless matrix P. |
| [2026-09-01 Task 3.24: Import-Chain Terminal Identity](2026-09-01-plan03-task03-import-chain-terminal-identity.md) | C: AST-independent terminal identity, b18ff691; G/C focused/interface; intermediate hop/completion separate. |

### Plan03 Call Records

Twenty-nine records preserve [O03 Task 4][o03]. Task 4's four implementation boxes are C through the September 1 callable parity leaf. Earlier pending receiver/mapping/source-binary-native callable clauses are S only for that closed contract. Pending owner: O03 Task 8 plus [O04 Task 2][o04] for complete cross-provider hierarchy behavior.

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-26 Task 4.6: Call-Edge Exactness Gate](2026-08-26-plan03-task04-call-edge-exactness.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-26 Task 4.1: Source Call-Edge Foundation](2026-08-26-plan03-task04-call-edge-foundation.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-26 Task 4.3: Canonical Call Metadata Projection](2026-08-26-plan03-task04-call-metadata.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-26 Task 4.8: Call Output Clearing](2026-08-26-plan03-task04-call-output-clear.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-26 Task 4.7: Overload Candidate Exactness Gate](2026-08-26-plan03-task04-candidate-exactness.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-26 Task 4.4: Lambda Caller Scope Facts](2026-08-26-plan03-task04-lambda-callers.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-26 Task 4.2: Resolved Overload Candidate Membership](2026-08-26-plan03-task04-overload-candidates.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-26 Task 4.5: Returned Lambda Caller Fails Closed](2026-08-26-plan03-task04-returned-lambda-fail-closed.md) | C: call-edge/candidate/exactness scoped producer/query regression; exact toolchains remain in the leaf. |
| [2026-08-30 Task 4.13: Call Candidate Consistency](2026-08-30-plan03-task04-call-candidate-consistency.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.11: Call Edge Range Identity](2026-08-30-plan03-task04-call-edge-range-identity.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.9: Call Edge Refinement Merge](2026-08-30-plan03-task04-call-edge-refinement-merge.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.10: Call Edge Source Identity](2026-08-30-plan03-task04-call-edge-source-identity.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.12: Call Edge Stable Order](2026-08-30-plan03-task04-call-edge-stable-order.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.19: Call Expression Refinement](2026-08-30-plan03-task04-call-expression-refinement.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.15: Call Overload Member Completeness](2026-08-30-plan03-task04-call-overload-member-completeness.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.14: Call Overload-Set Exactness](2026-08-30-plan03-task04-call-overload-set-exactness.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.17: Call Reference Conflict](2026-08-30-plan03-task04-call-reference-conflict.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.16: Call Reference Refinement](2026-08-30-plan03-task04-call-reference-refinement.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-30 Task 4.18: Call Source Identity](2026-08-30-plan03-task04-call-source-identity.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-31 Task 4.27: Ambiguous Call Expression Fail-Closed](2026-08-31-plan03-task04-ambiguous-call-expression.md) | C: equal-rank conflicting call shapes fail closed; G/C calls 30/30 and adjacent focused gates; M/full runtime P. |
| [2026-08-31 Task 4.26: Ambiguous Caller Identity](2026-08-31-plan03-task04-ambiguous-caller-identity.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-31 Task 4.23: Argument Mapping Contract Integrity](2026-08-31-plan03-task04-argument-mapping-contract-integrity.md) | C: mapping structural integrity; earlier claim that source in/ref/out lacked a producer is explicitly corrected in this record. |
| [2026-08-31 Task 4.20: Call Argument Mapping](2026-08-31-plan03-task04-call-argument-mapping.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-31 Task 4.21: Constructor Argument Mapping](2026-08-31-plan03-task04-constructor-argument-mapping.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-31 Task 4.24: Source Argument Passing Ranges](2026-08-31-plan03-task04-source-argument-passing-ranges.md) | C: structured ref/out marker range joins and in/ref/out mapping; G/C expanded gates; later receiver/provider evidence closes the stated support wait. |
| [2026-08-31 Task 4.22: Super Constructor Argument Mapping](2026-08-31-plan03-task04-super-constructor-argument-mapping.md) | C: G/C focused call identity/refinement/mapping gate; M/full matrix/stdio deferred to O03 T8. |
| [2026-08-31 Task 4.25: Unresolved Call Reason Matrix](2026-08-31-plan03-task04-unresolved-call-reason-matrix.md) | C: characterization of unavailable/unresolved/non-function targets; G/C expanded gates, no production change needed. |
| [2026-09-01 Task 4.28: Call Receiver Type Identity](2026-09-01-plan03-task04-call-receiver-type-id.md) | C: receiver TypeId public contract and producer; G/C 12-target focused; full current acceptance P. |
| [2026-09-01 Task 4.29: Callable Source, Binary, And Native Parity](2026-09-01-plan03-task04-callable-source-binary-native-parity.md) | C: Task 4 callable-contract closure, source/.zro/native cases; G/C 12-target and four new project cases. Other project failures/full M/stdio P. |

### Plan03 Display Records

Twenty-two records preserve [O03 Task 5][o03], including the independently dated reused 5.1-5.4 numbers. The four implementation boxes are C at the September 1 display closure. Pending owner: O03 Tasks 7/8 for consumer/source-provider/current-source completeness, then [O04][o04]/[O05][o05] presentation/runtime acceptance.

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-26 Task 5.1: Canonical Display Facade](2026-08-26-plan03-task05-display-facade.md) | C: canonical display/exactness/coherence scoped regression; later full display closure is separate. |
| [2026-08-26 Task 5.4: Documentation Facts](2026-08-26-plan03-task05-documentation-facts.md) | C: exact SymbolId documentation, owned text and reset/conflict tests; G/C/M scoped acceptance. |
| [2026-08-26 Task 5.3: FormatCall Fact Coherence](2026-08-26-plan03-task05-format-call-coherence.md) | C: canonical display/exactness/coherence scoped regression; later full display closure is separate. |
| [2026-08-26 Task 5.2: FormatCall Exactness Gate](2026-08-26-plan03-task05-format-call-exactness.md) | C: canonical display/exactness/coherence scoped regression; later full display closure is separate. |
| [2026-08-31 Task 5.4: Callable Effect Display Integrity](2026-08-31-plan03-task05-callable-effect-display-integrity.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.1: Canonical Display Integrity](2026-08-31-plan03-task05-canonical-display-integrity.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.17: Canonical Tuple Fixture Contract](2026-08-31-plan03-task05-canonical-tuple-fixture-contract.md) | C: keywordless fixture replaced by current fn syntax; G/C graph/parser/display 19/74/22; no production change. |
| [2026-08-31 Task 5.3: Composite Display Integrity](2026-08-31-plan03-task05-composite-display-integrity.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.2: Const Generic Display Identity](2026-08-31-plan03-task05-const-generic-display-identity.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.11: Const-Generic Expression Alias](2026-08-31-plan03-task05-const-generic-expression-alias.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.14: GcBridge Type-Value Alias Producer](2026-08-31-plan03-task05-gc-bridge-type-value-alias-producer.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.10: Generic Type-Use Alias Range](2026-08-31-plan03-task05-generic-type-alias-range.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.5: Nominal Display Identity Integrity](2026-08-31-plan03-task05-nominal-display-identity-integrity.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.8: Ownership Wrapper Inner Type Alias Producer](2026-08-31-plan03-task05-owner-inner-type-alias-producer.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.16: Owner Variant Display Acceptance](2026-08-31-plan03-task05-owner-variant-display-acceptance.md) | C: Unique/Shared/Weak/AtomicShared display; G/C focused/expanded. Its old name-mapper pending clause S by the next closure leaf. |
| [2026-08-31 Task 5.7: Primitive Type-Use Alias Producer](2026-08-31-plan03-task05-primitive-type-use-alias-producer.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.9: Qualified Type-Use Alias Producer](2026-08-31-plan03-task05-qualified-type-alias-producer.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.15: Reference/Readonly Type-Value Alias Producer](2026-08-31-plan03-task05-reference-readonly-type-value-alias-producer.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.12: Type-Value Alias Producer](2026-08-31-plan03-task05-type-value-alias-producer.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.6: Use-Site Type Display Alias Facts](2026-08-31-plan03-task05-use-site-type-display-alias-facts.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-08-31 Task 5.13: Wrapped Type-Value Alias Producer](2026-08-31-plan03-task05-wrapped-type-value-alias-producer.md) | C: G/C focused/expanded canonical display or alias producer evidence; integrated M/full matrix/stdio P. |
| [2026-09-01 Task 5: LSP Type Identity Display](2026-09-01-plan03-task05-lsp-type-identity-display.md) | C: Task 5 closure; primitive name mapper removed, identity/display separated; G/C/M focused source-contract/interface. |

### Plan03 Diagnostic Records

Forty-two records preserve [O03 Task 6][o03]: 6.1-6.41 plus the final closure. The four producer/projector boxes are C; older waits on a later named diagnostic producer are S only within that later leaf. Pending owners: O03 Tasks 7/8 for actual missing facts/current full tests and [O04 Tasks 3/6/8][o04] for remaining safe actions/protocol acceptance.

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-26 Task 6.8: Diagnostic Completeness Gate](2026-08-26-plan03-task06-diagnostic-completeness-gate.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.12: Function Call Mismatch Query Projection](2026-08-26-plan03-task06-function-call-mismatch-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.9: LSP Diagnostic Projection](2026-08-26-plan03-task06-lsp-diagnostic-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.1: Explicit No-Fix Reason](2026-08-26-plan03-task06-no-fix-reason.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.5: Ownership And Legacy No-Fix Producers](2026-08-26-plan03-task06-ownership-no-fix.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.4: Pattern And Import No-Fix Producers](2026-08-26-plan03-task06-pattern-import-no-fix.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.6: Query Diagnostic Disposition](2026-08-26-plan03-task06-query-diagnostic-disposition.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.7: Rule Diagnostic Disposition](2026-08-26-plan03-task06-rule-diagnostic-disposition.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.2: Syntax No-Fix Producers](2026-08-26-plan03-task06-syntax-no-fix-producers.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.3: Syntax Recovery No-Fix Producers](2026-08-26-plan03-task06-syntax-recovery-no-fix.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.11: Type Mismatch Query Projection](2026-08-26-plan03-task06-type-mismatch-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-26 Task 6.10: WASM Diagnostic Projection](2026-08-26-plan03-task06-wasm-diagnostic-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.19: Assignment Compatibility Query Projection](2026-08-27-plan03-task06-assignment-compatibility-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.20: Compiler/LSP Diagnostic Golden Parity](2026-08-27-plan03-task06-compiler-lsp-diagnostic-golden-parity.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.14: Const Assignment Query Projection](2026-08-27-plan03-task06-const-assignment-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.21: Duplicate Type Query Projection](2026-08-27-plan03-task06-duplicate-type-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.25: Extern Callable Decorator Query Projection](2026-08-27-plan03-task06-extern-callable-decorator-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.27: Extern Enum Decorator Query Projection](2026-08-27-plan03-task06-extern-enum-decorator-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.26: Extern Struct Decorator Query Projection](2026-08-27-plan03-task06-extern-struct-decorator-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.28: FFI Wrapper Decorator Query Projection](2026-08-27-plan03-task06-ffi-wrapper-decorator-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.22: Initializer Annotation Query Projection](2026-08-27-plan03-task06-initializer-annotation-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.16: Interface Const-Field Query Projection](2026-08-27-plan03-task06-interface-const-field-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.24: Method Call Mismatch Query Projection](2026-08-27-plan03-task06-method-call-mismatch-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.18: Named-Call Compatibility Query Projection](2026-08-27-plan03-task06-named-call-compatibility-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.13: Reachability Query Projection](2026-08-27-plan03-task06-reachability-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.23: Return Type Query Projection](2026-08-27-plan03-task06-return-type-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.17: Unresolved-Reference Query Projection](2026-08-27-plan03-task06-unresolved-reference-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-27 Task 6.15: Variance Query Projection](2026-08-27-plan03-task06-variance-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-28 Task 6.30: Cannot Infer Exact Type Query Projection](2026-08-28-plan03-task06-cannot-infer-exact-type-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-28 Task 6.34: Const Assignment Fact Producer Migration](2026-08-28-plan03-task06-const-assignment-fact-producer-migration.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-28 Task 6.29: Extern Parameter Decorator Query Projection](2026-08-28-plan03-task06-extern-parameter-decorator-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-28 Task 6.35: Interface Const Fact Producer Support](2026-08-28-plan03-task06-interface-const-fact-producer-support.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-28 Task 6.31: Local Diagnostic Escape Hatch Removal](2026-08-28-plan03-task06-local-diagnostic-escape-hatch-removal.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-28 Task 6.32: Ownership Return Escape Query Projection](2026-08-28-plan03-task06-ownership-return-escape-query-projection.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-28 Task 6.33: Variance Fact Producer Migration](2026-08-28-plan03-task06-variance-fact-producer-migration.md) | C: G/C/M focused structured diagnostic producer/query/projection gate; independent stdio evidence where stated in the leaf. |
| [2026-08-31 Task 6.36: Canonical Diagnostic Replacement](2026-08-31-plan03-task06-canonical-diagnostic-replacement.md) | C: G/C focused identity/deduplication/cutover gate; final G/C/M closure is separate. |
| [2026-08-31 Task 6.38: Diagnostic Multiplicity Collapse](2026-08-31-plan03-task06-diagnostic-multiplicity-collapse.md) | C: G/C focused identity/deduplication/cutover gate; final G/C/M closure is separate. |
| [2026-08-31 Task 6.37: Diagnostic Source Identity](2026-08-31-plan03-task06-diagnostic-source-identity.md) | C: G/C focused identity/deduplication/cutover gate; final G/C/M closure is separate. |
| [2026-08-31 Task 6.41: Interface Const Publisher Cutover](2026-08-31-plan03-task06-interface-const-publisher-cutover.md) | C: G/C focused identity/deduplication/cutover gate; final G/C/M closure is separate. |
| [2026-08-31 Task 6.39: Query Scope Source Identity](2026-08-31-plan03-task06-query-scope-source-identity.md) | C: G/C focused identity/deduplication/cutover gate; final G/C/M closure is separate. |
| [2026-08-31 Task 6.40: Unresolved Diagnostic Source Identity](2026-08-31-plan03-task06-unresolved-diagnostic-source-identity.md) | C: G/C focused identity/deduplication/cutover gate; final G/C/M closure is separate. |
| [2026-09-01 Task 6: Structured Diagnostic Closure](2026-09-01-plan03-task06-structured-diagnostic-closure.md) | C: Task 6 closure; ten-source producer/projector audit; G/C/M 64/13/19 plus source-contract/parity. Full analyzer/ownership failures not counted green. |

### Plan03 Consumer Records

These 60 records preserve [O03 Task 7][o03]. Only the reference-tracker
checkbox is checked in the parent; the migration, complete provider/stale/
unresolved tests, removal of local semantic reconstruction and final production
source-contract gate remain P. Every record below retains O03 Tasks 7/8 as
its pending owner, with cross-provider editing/hierarchy in [O04][o04], runtime
parity in [O05][o05] and final boundaries/full acceptance in [O06][o06].

| Dated record | Output and evidence boundary |
| --- | --- |
| [2026-08-28 Task 7.8: Local Call Hierarchy](2026-08-28-plan03-task07-local-call-hierarchy.md) | C: canonical caller/target/version and fromRanges; G/C/M focused/stdio; external hierarchy P. |
| [2026-08-28 Task 7.5: Local Definition Exactness](2026-08-28-plan03-task07-local-definition-exactness.md) | C: fact-only local definitions; G/C/M focused, interface delta zero. |
| [2026-08-28 Task 7.6: Local Implementation Relations](2026-08-28-plan03-task07-local-implementation-relations.md) | C: canonical ImplementationsOf, not a definition alias; G/C/M focused; full external/runtime coverage P. |
| [2026-08-28 Task 7.3: Local Reference Consumers](2026-08-28-plan03-task07-local-reference-consumers.md) | C: references/highlights with detached tracker and version changes; G/C/M focused; external aggregation separate. |
| [2026-08-28 Task 7.7: Local Type Hierarchy](2026-08-28-plan03-task07-local-type-hierarchy.md) | C: canonical base/derived data and stale rejection; G/C/M focused/stdio; external hierarchy P. |
| [2026-08-28 Task 7.9: Method And Lambda Call Hierarchy](2026-08-28-plan03-task07-method-lambda-call-hierarchy.md) | C: receiver/lambda stable identity and re-resolution; G/C/M focused/stdio. |
| [2026-08-28 Task 7.4: Project Reference Fallback](2026-08-28-plan03-task07-project-reference-fallback.md) | C: source-symbol fact projection and tracker-call removal; G/C/M focused, project/interface delta zero. Later dead API removal supersedes this adapter's existence. |
| [2026-08-28 Task 7.1: Reference Source Identity](2026-08-28-plan03-task07-reference-source-identity.md) | C: missing-source/exact URI equality; G/C/M focused tracker/reaching/parity/source-contract. |
| [2026-08-28 Task 7.2: Reference SymbolId Index](2026-08-28-plan03-task07-reference-symbol-id-index.md) | C: SymbolId index and same-name isolation; G/C/M 5/2/3/55. |
| [2026-08-29 Task 7.19 Canonical Callable-Value Shadow](2026-08-29-plan03-task07-canonical-callable-value-shadow.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.14 Canonical CodeLens Declarations](2026-08-29-plan03-task07-canonical-code-lens-declarations.md) | C: canonical declarations/reference count and module extraction; G/C/M focused/advanced; recorded broad failures excluded. |
| [2026-08-29 Task 7.17 Canonical Hierarchy Prepare](2026-08-29-plan03-task07-canonical-hierarchy-prepare.md) | C: canonical SymbolAt prepare and exact declaration identity; G/C/M focused/advanced/stdio. |
| [2026-08-29 Task 7.10 Canonical Inlay Declarations](2026-08-29-plan03-task07-canonical-inlay-declarations.md) | C: declaration enumeration/type formatting; G/C/M focused/stdio; recorded broad interface/stdio failures excluded. |
| [2026-08-29 Task 7.20 Canonical Receiver Signature](2026-08-29-plan03-task07-canonical-receiver-signature.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.21 Canonical Source Constructor Signature](2026-08-29-plan03-task07-canonical-source-constructor-signature.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.22 Canonical Super Constructor Signature](2026-08-29-plan03-task07-canonical-super-constructor-signature.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.23 Canonical Visible Symbol Completion](2026-08-29-plan03-task07-canonical-visible-symbol-completion.md) | C: fact-only lexical completion and detached AST/symbol-table tests; G/C/M focused; broad failure set unchanged. |
| [2026-08-29 Task 7.11 Read-Only Completion Facts](2026-08-29-plan03-task07-read-only-completion-facts.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.13 Read-Only Local Semantic Query](2026-08-29-plan03-task07-read-only-local-semantic-query.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.12 Read-Only Signature Facts](2026-08-29-plan03-task07-read-only-signature-facts.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.18 Snapshot-Only Canonical Signature Help](2026-08-29-plan03-task07-snapshot-only-canonical-signature.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.15 Symbol-Table-Free Call Hierarchy Follow-Up](2026-08-29-plan03-task07-symbol-table-free-call-hierarchy-follow-up.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-29 Task 7.16 Symbol-Table-Free Type Hierarchy Follow-Up](2026-08-29-plan03-task07-symbol-table-free-type-hierarchy-follow-up.md) | C: named canonical/read-only consumer contract; G/C/M focused evidence and unchanged broad failure set, full Task 8 P. |
| [2026-08-30 Task 7.46: Analyzer Completion Surface Removal](2026-08-30-plan03-task07-analyzer-completion-surface-removal.md) | C: unused analyzer completion API/helper closure and associated tests removed; G/C fixed-source relink/focused runtime. |
| [2026-08-30 Task 7.31: Canonical Completion Documentation](2026-08-30-plan03-task07-canonical-completion-documentation.md) | C: exact SymbolId metadata copied during snapshot lifetime; G parity 14/14/source-contract; full matrix P. |
| [2026-08-30 Task 7.32: Canonical Hover Documentation](2026-08-30-plan03-task07-canonical-hover-documentation.md) | C: exact SymbolId documentation copied during snapshot lifetime; G parity 14/14/source-contract; full matrix P. |
| [2026-08-30 Task 7.30 Canonical Implementation Identity](2026-08-30-plan03-task07-canonical-implementation-identity.md) | C: copied SymbolId implementation projection; G/C/M focused; no parent/provider promotion. |
| [2026-08-30 Task 7.29 Canonical Local Navigation Identity](2026-08-30-plan03-task07-canonical-local-navigation-identity.md) | C: copied SymbolId definition/reference/highlight projection; G/C/M focused; later Tasks 7.55/7.56 repair the named producer identity conflicts. |
| [2026-08-30 Task 7.39: Canonical Reachability Smoke Contract](2026-08-30-plan03-task07-canonical-reachability-smoke.md) | C: expected canonical diagnostic contract edit only; node syntax/diff checked. Leaf runtime replay P; current smoke stops later at generic detail. |
| [2026-08-30 Task 7.41: Canonical Semantic Token Identity](2026-08-30-plan03-task07-canonical-semantic-token-identity.md) | C: canonical token code/source-contract output; that leaf's runtime assertion was not executed due build timeout. Later 7.58/3.20 provide scoped runtime evidence. |
| [2026-08-30 Task 7.24 Canonical Source Hover](2026-08-30-plan03-task07-canonical-source-hover.md) | C: canonical SymbolAt/range hover and external structured metadata; G/C/M focused and unchanged interface markers. |
| [2026-08-30 Task 7.52: Dead Imported Member Position Scan Removal](2026-08-30-plan03-task07-dead-imported-member-position-scan-removal.md) | C: dead position scan removed, still-live binding walker restored after link diagnosis; G/C relink/marker comparison. |
| [2026-08-30 Task 7.43: Dead Project Navigation Fallback Removal](2026-08-30-plan03-task07-dead-project-navigation-fallback-removal.md) | C: dead API/call-graph/source-contract removal; G/C syntax verification at this leaf; integrated runtime gate P. |
| [2026-08-30 Task 7.54: Dead Project URI Ensure Removal](2026-08-30-plan03-task07-dead-project-uri-ensure-removal.md) | C: dead URI ensure API removed; G/C relink/project/interface marker comparison. |
| [2026-08-30 Task 7.49: Dead Semantic Text Helper Removal](2026-08-30-plan03-task07-dead-semantic-text-helper-removal.md) | C: three zero-caller text helpers removed; G/C relink/focused runtime. |
| [2026-08-30 Task 7.53: Dead Virtual Declaration Query Removal](2026-08-30-plan03-task07-dead-virtual-declaration-query-removal.md) | C: two dead query wrappers removed; G/C relink, G virtual-document case; full M/matrix P. |
| [2026-08-30 Task 7.50: Dead Virtual Document Format Helper Removal](2026-08-30-plan03-task07-dead-virtual-document-format-helper-removal.md) | C: dead formatter/include removed; G/C relink, G virtual-document cases; full M/matrix P. |
| [2026-08-30 Task 7.33: Diagnostics and Semantic Token Consumer Audit](2026-08-30-plan03-task07-diagnostics-token-consumer-audit.md) | C: audit only; canonical diagnostics ownership confirmed. Token migration wait S in later 7.41/7.58 and 3.20 scopes; complete Task 7/8 still P. |
| [2026-08-30 Task 7.26 External Member Reference Identity](2026-08-30-plan03-task07-external-member-reference-identity.md) | C: exact external declaration matching; supplementary 10:26 fail-closed closure, G/C/M parity/source-contract; no name recovery. |
| [2026-08-30 Task 7.42: Imported Reference Canonical Identity](2026-08-30-plan03-task07-imported-reference-canonical-identity.md) | C: canonical imported identity/metadata cross-check and removed name helper; G/C syntax verification only at this leaf. Later 3.21 supplies cross-snapshot runtime evidence. |
| [2026-08-30 Task 7.36: Local Ownership Projection](2026-08-30-plan03-task07-local-ownership-projection.md) | C: ownership node/canonical-range projection; G/C/M focused; remaining fact-generation errors not counted green. |
| [2026-08-30 Task 7.37: Local Reachability Projection](2026-08-30-plan03-task07-local-reachability-projection.md) | C: logical relatedNode short-circuit query; G/C/M focused; complete matrix/stdio replay still red at that point. |
| [2026-08-30 Task 7.38: Local Reference Cross-Project Fail-Closed](2026-08-30-plan03-task07-local-reference-cross-project-fail-closed.md) | C: name-based aggregation removed, G/C/M source-contract 70/70. Historical same-source full gate 10/16; current full gate P. |
| [2026-08-30 Task 7.40: Local Rename Canonical Identity](2026-08-30-plan03-task07-local-rename-canonical-identity.md) | C: local rename stable-id projection/source contract; complete provider/stale/three-toolchain matrix P. |
| [2026-08-30 Task 7.35: Reference Diagnostic Bridge](2026-08-30-plan03-task07-reference-diagnostic-bridge.md) | C: existing structured compiler error publisher/query bridge; G/C/M focused; accompanying full gate remained red. |
| [2026-08-30 Task 7.44: Reference Tracker Query Surface Removal](2026-08-30-plan03-task07-reference-tracker-query-surface-removal.md) | C: three dead query APIs and index fields removed; G/C fixed-source relink/focused runtime; full M/matrix P. |
| [2026-08-30 Task 7.27 Semantic Query Document Version](2026-08-30-plan03-task07-semantic-query-document-version.md) | C: copied document-version fence; G/C/M focused, retained no-source-version metadata boundary. |
| [2026-08-30 Task 7.48: Symbol Add Wrapper Removal](2026-08-30-plan03-task07-symbol-add-wrapper-removal.md) | C: unused wrapper removed, five legacy test setups migrated; G/C fixed-source relink/focused runtime. |
| [2026-08-30 Task 7.45: Symbol Reference Count Getter Removal](2026-08-30-plan03-task07-symbol-reference-count-getter-removal.md) | C: dead getter removed; G/C fixed-source relink/focused runtime. Reference storage was explicitly retained. |
| [2026-08-30 Task 7.51: Symbol Reference Wrapper Removal](2026-08-30-plan03-task07-symbol-reference-wrapper-removal.md) | C: dead Symbol-to-SymbolId wrapper removed; G/C relink/local/parity and interface marker check. |
| [2026-08-30 Task 7.47: Symbol Table Query Surface Removal](2026-08-30-plan03-task07-symbol-table-query-surface-removal.md) | C: unused symbol-table query APIs removed; G/C fixed-source relink/focused runtime. |
| [2026-08-30 Task 7.28 Unresolved Value Exactness](2026-08-30-plan03-task07-unresolved-value-exactness.md) | C: non-TYPE unresolved fact fail-closed; G/C/M focused; unresolved TYPE producer was a separate support wait. |
| [2026-08-31 Task 7.56: Canonical Extern Callable Identity](2026-08-31-plan03-task07-canonical-extern-callable-identity.md) | C: extern fact/parent-chain identity and extracted binding producer; G/C focused, interface fixed7 -> fixed6. |
| [2026-08-31 Task 7.55: Canonical Local Binding Identity](2026-08-31-plan03-task07-canonical-local-binding-identity.md) | C: reused canonical local identity; G/C focused, interface fixed8 -> fixed7. |
| [2026-08-31 Task 7.58: Canonical Semantic Token Specialization](2026-08-31-plan03-task07-canonical-semantic-token-specialization.md) | C: specialized SymbolAt/owner token and modularization; G/C focused, interface fixed5 -> fixed3. |
| [2026-08-31 Task 7.57: Canonical Typecheck Receiver Bindings](2026-08-31-plan03-task07-canonical-typecheck-receiver-bindings.md) | C: canonical this/super and constructor fact ordering; G/C focused, interface fixed6 -> fixed5. |
| [2026-09-01 Task 7.59: Declared Primitive Type Identity](2026-09-01-plan03-task07-declared-primitive-type-identity.md) | C: primitive/const-bound producer; G/C/M parity/interface/source-contract; full analyzer still had fourteen recorded failures. |
| [2026-09-02 Task 7.62: Canonical SymbolAt Reference Resolution](2026-09-02-plan03-task07-canonical-symbol-at-reference-resolution.md) | C per the leaf status: AST use-site collection removed; canonical SymbolAt consumed. Its Verification section lists required targets without concrete run results, so acceptance evidence is P in Task 8. Upper scope/range fallback and reference bookkeeping remain. |
| [2026-09-02 Task 7.60: Source-aware Fact Query Fixtures](2026-09-02-plan03-task07-source-aware-fact-query-fixtures.md) | C: twelve analyzer fixture failures repaired by source-bound ranges; M isolated full analyzer replay; two generic support gaps then remained. G/C/current full gate P. |
| [2026-09-02 Task 7.61: Structured Generic Member Returns](2026-09-02-plan03-task07-structured-generic-member-returns.md) | C: structured class/struct/interface generic return facts; G/C/M analyzer/parity/source-contract/interface; final protocol gate P. |

### Audits And Current Records

| Record | Status and continuing owner |
| --- | --- |
| [2026-08-30 final-gate audit](2026-08-30-plan03-final-gate-audit.md) | C as a failed-gate audit, not implementation acceptance. G/C/M historical 10/16 and stdio failures remain recorded; named later repairs supersede individual causes only. [O03 Task 8][o03] owns the current complete replay. |
| [O03 Task 7.25 inline RED](03-canonical-semantic-query.md) | No separate dated leaf. The main-plan current-boundary paragraph records zero imported source declaration range. S for the source imported-reference case by [Task 3.21](2026-09-01-plan03-task03-cross-snapshot-references.md); actual sourceless origins/generations still P under O03 Task 3/8. |
| [O03 Task 7.34 inline audit](03-canonical-semantic-query.md) | No separate dated leaf. The 2026-08-30 05:56 audit records parity passes but interface/analyzer/project/stdio failures. Retained by the final-gate audit; no all-green promotion. Owner O03 Task 8. |
| [2026-09-05 Task 1 Sub1 crosswalk](2026-09-05-plan00-task01-sub01-execution-crosswalk.md) | C as a documentation inventory only; full [O00 Task 1][o00] remains in-progress until the integrated baseline and missing matrix evidence exist. |
| [2026-09-05 Task 4 Sub1 identity-resolve withdrawal](2026-09-05-plan00-task04-sub01-identity-resolve.md) | C at 16:56 +08:00, commit ce04018c. G/M focused 7/7; C six focused passes plus isolated protocol pass after initial cancel-known timeout; configured extension compile/unit 38/noEmit pass. Other O00 overclaims/full baseline and O01 cancellation investigation remain P. |
| [2026-09-05 Task 4 Sub2 navigation-alias withdrawal](2026-09-05-plan00-task04-sub02-navigation-aliases.md) | P/in-progress at this inventory snapshot. Declaration overclaim RED and a reaching-write definition expectation are recorded; production, three-toolchain tests, review and commit remain owned by the root's [O00 Task 4][o00] leaf. |

## Required Semantic Coverage

| Syntax contract | Required canonical facts and LSP consumption |
| --- | --- |
| [01](../../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | Type/Symbol/Place/CFG identity, artifacts, scoped query lifetime. |
| [02](../../syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md) | ref/in/out/scoped/readonly, definite assignment, borrow and escape diagnostics. |
| [03](../../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | struct/ref struct/Span layout and bounds facts. |
| [04](../../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | move/drop/owner/GC bridge effects and diagnostics. |
| [05](../../syntax/2026-07-18-05-property-unified-ast-design.md) | PropertySymbol, accessor and ref-return relations. |
| [06](../../syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md)-[07](../../syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md) | Migration diagnostics/actions and current-reference semantic goldens. |
| [08](../../syntax/2026-07-19-08-reflection-library-type-system-design.md)-[09](../../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | Reflection descriptor/capability and pooling guard/ref-like contracts. |
| [10](../../syntax/2026-07-19-10-native-ffi-module-package-design.md) | ModuleIdentity, provider generation, package/native/FFI and virtual declarations. |
| [11](../../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | Typed declaration generation, compiler phase and generated symbol origins. |
| [12](../../syntax/2026-07-20-12-async-task-job-scheduler-design.md)-[13](../../syntax/2026-07-20-13-iterator-enumerator-yield-design.md)-[14](../../syntax/2026-07-20-14-test-function-harness-design.md) | Explicit Task/Job/Iterator carriers, effects and testing provider phase. |

## Verification And Promotion

Each repair first records its failing case, then lower-layer, consumer and
protocol evidence. Use separate GCC/Clang/MSVC build directories and identify
the exact committed source plus any owned overlay. Full plan03 requires its
original 16-target matrix and three-toolchain stdio/CLI tests. Final acceptance
also requires native/WASM protocol parity, extension unit/noEmit, packaged
desktop/browser/desktop-Web smoke, memory tools and fuzz/fault injection.

Keep the frozen p95 limits: hover 50 ms, completion/signature 100 ms,
single-document diagnostics 250 ms, 100-file edit diagnostics 500 ms and
cancellation observation 50 ms. Preserve the 256 MiB semantic cache budget,
512 MiB native process peak and current plus two historical snapshots.

Historical counts and source inspection are not current pass evidence. The
baseline failure ledger will name each observed failure with its command,
source revision, expected/actual behavior and owning support layer.

## Current Protocol Baseline

Source: committed `c95e5387` with exact gitlink exports. GCC Debug shared at
`/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc`, WSL Node 12.22.9.
No other session's production overlay is included. The 2026-09-05 replay ran
the following CTest expression with `--output-on-failure`:

```text
^language_server_stdio_(smoke|server_lifecycle|protocol_conformance|diagnostic_fix_smoke|workspace_folders_smoke)$
```

| Test | Observed result | Owner and promotion consequence |
| --- | --- | --- |
| server_lifecycle | PASS, 100-cycle lifecycle executable | Historical implementation confirmed for this uninstrumented build only. |
| protocol_conformance | PASS | Continue sanitizer/MSVC and subsequent-current-source gates. |
| workspace_folders_smoke | PASS | Native notifications are implemented; the old ignored-handler finding is superseded. |
| smoke | FAIL: generic completion detail should include the normalized closed instantiation | Plan03 compiler/completion consumer; fails at baseline line 2024, before the later resolver checks. |
| diagnostic_fix_smoke | FAIL: Expected possibly_uninitialized_read publication | Plan03 structured diagnostic producer/projection; missing expected publication at test line 731. |

Both failures predate the resolve production edit. They remain open for the
integrated semantic baseline and do not permit declaring Plan00 globally green.
The identity-resolve leaf uses a focused protocol fixture in addition to the
unchanged broad failure evidence.

After ce04018c, GCC broad smoke and diagnostic-fix smoke still fail at the same
two assertions. The [resolve leaf](2026-09-05-plan00-task04-sub01-identity-resolve.md)
records G/M focused 7/7 and the Clang focused results. Its first Clang protocol
run timed out on cancel-known; the unchanged binary passed that protocol test
when rerun alone. A timing probe supports a load-sensitive fixture hypothesis,
but does not establish the first timeout's cause. This remains an O01 Task 4/6
revalidation item, not a removed failure or a relaxed cancellation threshold.

The same leaf records configured extension compile, 38 unit tests and noEmit
passes. An additional explicit strict worker check has 17 errors on both the
baseline and modified source, delta zero. That separate O05 gate remains open;
configured noEmit is not evidence that the stricter worker check passed.

## Superseded Review Clauses

- Native implementation is no longer a definition alias: it consumes local
  canonical `ImplementationsOf` relations. The 2026-08-28 local implementation
  record remains evidence; external/provider and protocol coverage remains open.
- Native workspace-folder notifications are handled and the dedicated current
  GCC protocol test passes. Keep them enabled; Web synchronization is separate.
- Declaration and typeDefinition remain definition aliases and require Plan00
  withdrawal until Plan04 supplies their real contracts. Null-only will-create/
  will-delete, unconditional proposed capabilities and untyped string colors
  remain independent capability defects.
- The original semantic-inference ledger's successive nine-through-fifteen L8
  counts are historical updates; its latest completed external-callable leaf is
  the sixteenth contract. The original external-callable overlay wait ended
  with ceadabbf, already an ancestor of the 2026-08-22 baseline. Today's wait
  concerns the separately owned symbol-projection and Task 7.63 changes.
- O01/O02 original unchecked implementation wording is not a reason to re-add
  the old global inputGeneration fence, detached reader, raw URI conversion,
  full-reparse-only telemetry or worker diagnostic hash/open-document-only
  enumeration. Their named later records establish the replacement contracts;
  current runtime and complete browser acceptance still require actual tests.
- The broad source reconstruction findings in O03/optimize index must be read
  with the later query/display/diagnostic/consumer leaves. Source-contract
  removal or a narrowed fixture fix does not prove all consumers or all
  providers are complete, and existing historical failures are not silently
  waived by this crosswalk.

[o00]: ./00-baseline-and-contract.md
[o01]: ./01-protocol-lifecycle-and-transport.md
[o02]: ./02-snapshots-workspaces-and-diagnostics.md
[o03]: ./03-canonical-semantic-query.md
[o04]: ./04-editor-feature-correctness.md
[o05]: ./05-native-web-capability-parity.md
[o06]: ./06-modularization-performance-and-acceptance.md
