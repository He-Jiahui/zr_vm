---
related_code:
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/src/zr_vm_core/optimization_remark.c
  - zr_vm_parser/include/zr_vm_parser/optimization_remarks.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/optimization_remarks.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.h
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/src/zr_vm_core/optimization_remark.c
  - zr_vm_parser/include/zr_vm_parser/optimization_remarks.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/optimization_remarks.c
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.h
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
plan_sources:
  - docs/plans/ssa/11-tooling-acceptance/02-optimization-remarks.md
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - docs/plans/ssa/guides/B-passes-analysis.md
  - "user: 2026-09-14 完成 11.02 optimization remarks，保留 CMake/umbrella/index 由父集成"
tests:
  - tests/parser/test_ssa_optimization_remarks.c
  - tests/parser/test_ssa_optimization_remarks_projection.c
  - tests/acceptance/ssa-optimization-remarks.md
doc_type: module-detail
status: projection-focused-passed
---

# SSA optimization remarks

## Purpose

Optimization remarks explain a pass decision at a canonical source location.
They are tooling facts, not compiler errors: a missed or blocked transform is
reported with its reason and evidence, while malformed producer data is
reported through a separate structured diagnostic.  The CLI and language
server consume the same records and never re-run AST/IR analysis.

## Stable core contract

`SZrOptimizationRemark` in the core header is a C11/POD record.  It contains no
pointer: module/IR/source versions and hashes, an optional address-free
`siteKey` for profile/runtime correlation, source id and byte range, fixed
pass/module strings, status (`success`, `missed`, `blocked`), reason, before and
after representation ids, backend mask, evidence kind, proof/profile/cost
facts, and boxing/allocation/cache/deopt flags.  Optional counters distinguish
software inline-cache misses from hardware cache misses (and branch,
allocation, and deopt counters).

`ZrCore_OptimizationRemark_Validate` rejects incompatible schema versions,
unterminated strings, reversed ranges, unknown enum/mask bits, and measured
rows without a declared counter mask.  `proven`, `estimated`, `measured`, and
`unavailable` remain distinct.  Absent counters are serialized as JSON `null`,
never as an invented measured zero.

`SZrOptimizationRemarkStore` owns a bounded append-only array.  Once its
configured limit is reached, valid rows are counted in `droppedCount` and
`truncated` is set.  Query results are copied into an owned page, sorted by
source range/source id/pass/status/reason/proof id, and paged by offset/limit.
Each page carries `droppedCount` as well as `truncated`, allowing consumers to
distinguish an empty result from a bounded collection that lost records.
Queries support module and IR hashes, source version/id/range, pass, reason,
status, backend, and evidence filters.  `InvalidateSourceVersion` removes
stale rows after an edit or incremental rebuild.

## Producer and parser bridge

ExecIR passes continue to write their existing `SZrExecIrOptimizationRemark`
sink.  `ZrParser_OptimizationRemark_FromExecIr` copies the bounded pass name,
maps common pass-manager and LICM loop reason codes, resolves the source id
through the function's canonical source map, and copies context identity,
profile, proof, representation, and counter facts.  Unknown producer reason
codes are rejected instead of being relabeled `none`.  The context's optional
module name is borrowed only during conversion and copied into the fixed core
array; no pointer crosses the boundary.

`ZrParser_OptimizationRemarks_ImportExecIr` checks sink bounds and imports as a
transaction.  If a later row fails validation or append, count, truncation,
and dropped counters are restored.  A successful pass with no profile/counter
evidence is marked `unavailable`; a missed/blocked pass without evidence is
`estimated`; neither path claims a measured result.

## CLI projection

`ZrCli_ExplainOptimizeOptions_Parse` is the formal parser for the arguments
following `explain optimize`.  It accepts a positional or `--module` name,
`--module-hash`, `--ir-hash`, `--source-version`, `--source-id`, comma-separated
`--reason`, `--status`, `--backend`, and `--evidence` filters, `--pass`,
`--range start:end` (or `start..end`), `--offset/--limit`, and `--json`.
Names are normalized so `code_budget` and `code-budget` are equivalent; unknown
names, duplicate identity options, malformed numbers/ranges, and overlong
strings fail with a caller-provided error.

`ZrCli_ExplainOptimize_RunStore` receives the already-produced core store and
projects one stable query.  Text output includes count, pass/status/reason and
canonical source offsets; JSON output is generated by the core page writer and
therefore has a fixed `schemaVersion`.  The API intentionally does not invent
an artifact loader: the owning compile/project command supplies the store and
then invokes this formatter.  Parent integration should route the top-level
`explain optimize` tokens to this parser and pass the compiler's remark store.

## LSP projection and stale-result handling

`ZrLanguageServer_LspOptimizationRemark_ProjectVersioned` converts a core row
to an owned fixed-string LSP value (including the optional `siteKey`) and maps
canonical UTF-8 byte offsets to LSP UTF-16 line/character positions (including
CRLF).  It copies identity,
representation, evidence, counters, and all explanatory flags.  A request may
explicitly select document version zero via `hasDocumentVersion`; mismatches
return `ZR_LSP_OPTIMIZATION_REMARK_STALE` and never publish a result.

`ZrLanguageServer_LspOptimizationRemarks_Query` applies module/IR/source,
reason/status/backend/evidence, pass/range, and pagination filters through the
core query (including `droppedCount`/`truncated` metadata).  It checks cancellation before and during projection, reports
`CANCELLED`, and performs a stale-version probe only for the same filtered
identity.  Thus an unrelated module cannot make a request appear stale.

`SZrLspOptimizationRemarkCache` tracks document version and a monotonic
generation.  `Begin` invalidates old results; `PublishGeneration` and
`AcceptGeneration` require both version and generation to match.  This is the
minimal cache boundary needed by an incremental LSP handler; transport JSON-RPC
and artifact loading remain in their owning modules.

## Data flow and ownership

```text
ExecIR pass sink -> parser conversion/source map -> core validation/store
       -> core query/page -> CLI text or JSON
                              \-> LSP UTF-16 projection + version/generation gate
```

Producer sinks and stores own their arrays.  Core query pages and LSP pages own
their copied arrays and must be released with their corresponding `PageFree`
function.  The fixed remark itself can be copied or retained after parser and
runtime snapshots are released.

## Test coverage and acceptance

`test_ssa_optimization_remarks.c` covers schema/diagnostics, all evidence
states, bounded truncation, source-version invalidation, parser source-map
identity and `siteKey`, unknown reason/pass rejection, invalid mask/counter
combinations, and transactional import rollback.  The projection test covers
CLI text/JSON filter parsing and identity, malformed-filter rejection, LSP
UTF-16 range mapping, explicit version-zero acceptance, matching-identity
stale rejection, unrelated-candidate isolation, cancellation, and cache
generation rejection.

The exact standalone GCC/Clang commands and their exit-0 evidence are recorded
in `tests/acceptance/ssa-optimization-remarks.md`.  CMake target registration,
library source lists, top-level command routing, and umbrella/index edits are
deliberately left to the parent integration phase so this slice does not
change shared build ownership.

## Out of scope

This slice does not infer dynamic hardware causes, synthesize profile values,
or decide optimization legality.  It also does not add a second AST analysis
path, persist raw pointers in artifacts, or claim a repository-wide CTest pass.
