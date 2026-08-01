---
scope:
  - Syntax 06B
  - Syntax 07B
  - Syntax 08
  - Syntax 09
  - Syntax 10F
  - Syntax 10C
  - Syntax 11
  - Syntax 14
status: in-progress
last_verified: 2026-08-01
---

# Syntax upper gates requirement-to-evidence ledger

This ledger separates three claims that must not be conflated:

1. all 55 historical leaf status records are complete in their declared scope;
2. the production parser has completed the one-time breaking syntax cutover;
3. the root Syntax redesign and every upper promotion gate are complete.

The first two claims are proven below. The third is false while 07B and the
remaining owner gates are open.

## 2026-08-01 validation snapshot

- The mechanical leaf selection is unchanged: 55 records, 55 completion
  markers, 0 missing. Raw status spellings are 20 Chinese `已完成`, 16 plain
  `completed`, 12 backticked `completed`, three historical
  `completed_with_known_*` qualifiers, one backticked M4-qualified
  `completed`, and three Chinese M2/M3/M4 promotion-qualified completions.
- The production parser recognizes 24 removed percent names only to emit the
  structured `legacy_syntax_removed` diagnostic and return no AST. This is
  rejection-only recognition, not dual syntax. Ordinary remainder `%` and
  remainder assignment `%=` remain operators. One `%` separator belongs to
  the internal intermediate declaration text format, not a source keyword.
- Non-percent compatibility forms in the cutover matrix, including `func`,
  keywordless functions, old arrows, old constructors, old ownership members,
  split properties, and comptime decorator types, are rejected.
- The syntax-reference manifest has 29 features: 15 `current`, 1 `negative`,
  and 13 `design-pending`. Those 13 entries keep 07B open.
- The repository migration inventory is deterministic at 890 scanned files,
  420 structured exclusions, 645 findings, and 3 allowlisted negative
  findings. Classification is `machineApplicable=0`, `maybeIncorrect=0`,
  `blocked=0`, `targetNotPromoted=0`, `requiresReview=645`, and
  `unknown=0`. The review findings are retained source/embedded examples that
  require semantic classification; they are not accepted parser rules.
- Focused GCC evidence includes the seven registered cutover/reference/provider
  CTests, type inference 122/122, official provider convergence 4/4, artifact
  TestManifest roundtrip/corruption 1/1, SemIR 10/10, and the LSP CompileTool
  projection assertion. The final clean multi-toolchain result is recorded in
  `2026-08-01-syntax-status-and-breaking-cutover.md`.

## Gate ledger

| Gate | Current evidence | State | Remaining proof/work |
|---|---|---|---|
| 08 M1-M5 | reflection surface 18/18 and stress 3/3; canonical identity, member query and construction paths exist | indirect | artifact corruption/trimming, full VM/AOT matrix, LSP projection and published stress/perf matrix |
| 09 M1-M5 | pool base 11/11, GC stress 3/3 and artifact 3/3 | indirect | language guard cleanup, moving-slab compaction, PoolRef reflection/LSP and pause/allocation evidence |
| 10F M3 | native extern 27/27 and FFI 29/29 focused evidence | proven | preserve in final matrix |
| 10C | frozen 25-module N0-N3 inventory; phase-typed owners; distinct official provider descriptors rejected; LSP CompileTool phase/hash convergence | indirect | prove artifact/reflection/debug identity and every owner provider before global promotion |
| 11 M1-M2 | build facts, typed diagnostics/effects, deterministic limits/cache | proven | preserve |
| 11 M3 | typed AttributeUsage/AttributeData, Conditional elision, static decorator shape coverage, runtime decorator executor/helper removal | proven | preserve retained-data consumers |
| 11 M4 | typed diagnostics, interfaceAdds, attributeAdds, GeneratedField rebind/provenance and artifact/reflection retention | indirect | GeneratedType/Method/Property, generated source maps and allocation-failure transactional rollback |
| 11 M5 | runtime decorator deleted; artifact/reflection and LSP CompileTool projection present; migration updated | indirect | formatter/build-dependency and remaining consumer acceptance |
| 14 M1 | ordinary function test/case/skip roles, typed TestManifest, production typecheck-and-trim | proven | preserve |
| 14 M2 | official Test-phase `zr.testing` provider with assert/equal/throws and bounded structured failure | proven | preserve |
| 14 M3 | deterministic discovery/filter/list/run, process isolation, jobs, timeout, output and exit codes | proven | preserve sync/async reference matrix |
| 14 M4 | percent/draft migration and role-driven LSP CodeLens/projection | indirect | canonical Debug TestManifest consumer and final idempotent cross-consumer acceptance |
| 06B parser cutover | removed syntax has rejection-only recognition and no old AST/lowering/runtime decorator semantics | proven | keep migration diagnostics and operator/internal-IR allowlist distinct |
| 06B repository promotion | inventory has no machine/block/unknown/not-promoted findings, but 645 review findings and owner-gated reference slots remain | indirect | classify/migrate remaining current inputs as owner gates close |
| 07B | coverage still has 13 owner-gated `design-pending` entries | contradicted | promote only after each owner gate has independent evidence |

## Current conclusion

The strict production parser cutover is complete. The 55 historical leaf
records are confirmed in their own scope. The root Syntax redesign is not
complete: 07B is explicitly open, and 08/09/10C/11/14 still have the partial
rows listed above. No acceptance document may translate leaf completion into a
root promotion.
