---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semantic_receiver_guard_fact_cases.h
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics_region_cases.h
  - tests/language_server/test_ownership_diagnostics_weak_receiver_cases.h
  - tests/language_server/test_ownership_diagnostics_owner_set_cases.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
plan_sources:
  - "user: 2026-10-16 support-first receiver guard republish and sourced ownership fixture repair"
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semantic_receiver_guard_fact_cases.h
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_ownership_diagnostics_region_cases.h
  - tests/language_server/test_ownership_diagnostics_weak_receiver_cases.h
  - tests/language_server/test_ownership_diagnostics_owner_set_cases.h
doc_type: module-detail
---

# Receiver Guard Fact Publication

## Responsibility

`ZrParser_SemanticFacts_AppendReceiverGuard` publishes receiver-guard evidence
into one semantic context. The existing publication entry point owns replacement
and inferred-type container lifetime; callers do not need a separate deduplication
pass. This repair changes that entry point only, without introducing another
fact store or moving ownership analysis into the fact layer.

## Identity And Replacement

A non-NULL `SZrReceiverGuardFact.node` is the publication key within the current
context. Republishing that exact AST node replaces its complete stored fact.
The latest receiver, access-chain segments, range, guard kind, execution mode,
and inferred types become authoritative. Guard kind, mode, source coordinates,
and receiver identity are not additional deduplication keys.

Distinct AST nodes remain independent even when their ranges, kinds, modes, or
receiver pointers coincide. Facts with a NULL `node` retain append-only behavior;
NULL is not a shared identity for merging anonymous facts. This is a per-context,
current-AST rule, not cross-file, cross-context, or cross-snapshot interning.

Repeated inference of the same segment therefore does not increase the guard
count. `ZrParser_SemanticFacts_FindReceiverGuardByNode` continues to return the
stored fact for that node; it does not rerun inference or synthesize evidence.

## Payload Ownership

Publication first copies the record and independently copies `receiverType` and
`guardedType` with the existing inferred-type copy operation. When a matching
node already exists, it frees both old inferred-type payloads and assigns the
prepared copy to that slot. The ordering is significant: the input may be the
same record returned by `FindReceiverGuardByNode`.

The semantic context owns the resulting native inferred-type containers.
Replacing a record, resetting the context, or freeing the context releases those
owned payloads through the existing inferred-type lifetime operations. Input
containers remain the caller's responsibility. Copying these containers does not
transfer ownership of borrowed AST nodes, source strings, type names, or other
snapshot metadata to the fact store.

The append API still returns FALSE for a NULL context, NULL fact, or invalid fact
array. Inferred-type Copy has no failure result, so this repair does not establish
an allocation-failure transaction or promise that every allocation failure is
detected. The API's existing allocation contract is unchanged.

## Borrowed Snapshot Queries

Query results are borrowed records in the semantic context, not independently
owned copies. Array growth may invalidate their addresses. Same-node replacement
invalidates the old inferred-type payloads even when the record slot is reused.
Reset and Free invalidate the facts. Callers must not retain those records,
payload pointers, AST identities, or context-local IDs across snapshot teardown.

Passing a currently stored fact back to AppendReceiverGuard is supported because
both payload copies are prepared before either old payload is freed. This narrow
alias-input guarantee does not permit using a pointer already invalidated by
array growth, reset, free, or an earlier replacement.

## Source-Aware Position Queries

Position-based fact lookup retains strict source matching before coordinate
containment. A sourced AST must not match a query whose `range.source` is NULL
merely because its line, column, or offset is equal. No production source matcher
is relaxed by this repair.

The ownership diagnostics fixture's `file_range_for_nth_substring` now takes the
`sourceName` used to parse its in-memory source. All sixteen calls in the main
fixture and region companion pass that name into the constructed query range.
Needle selection, occurrence indices, coordinates, diagnostics, expected facts,
and the twenty-five registered cases remain unchanged. This repairs fixture
source identity, not physical file CRLF handling or a runtime text-reading path.

## Regression Evidence

The dated Windows RED receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-receiver-guard-red-r1-receipt.json`,
SHA256 `EEFE3391820B6D1BFF7E1421B84C377D8A91AFA6C611258D769DD1AFDB5852DC`.
It records identical pre/post inventories of 4,790 source inputs and HEAD
`7889191053f9106ff6d6f6c5cf3403a79e40bcc7`, unchanged tested binaries, and all
owned child processes at natural terminal states.

The semantic-facts executable ran nineteen Unity cases: the seventeen existing
cases and the distinct-node control passed; same-node replacement failed. The
new regression requires the latest complete record, independently owned nested
type payloads after freeing caller inputs, and safe alias-input republication.
The distinct-node regression holds equal range/kind/mode constant while requiring
two independent records and type payloads.

The ownership executable ran twenty-five cases with twelve PASS and thirteen
FAIL results. Failure-only observations showed four guards for two AST nodes and
two guards for one AST node: each node was published twice. Those observations
motivate the shared publication repair rather than weaker guard-count assertions.
The fixture also produced NULL-source position queries against sourced facts.

A separate weak owner-set diagnostic existed with zero related-information
entries in that same RED. This patch neither changes that diagnostic's expected
related information nor claims to resolve its dataflow cause.

## Validation Boundary

Focused native GREEN for the guard publication is now observed in receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-guard-green-aot-probe-r1-receipt.json`,
SHA256 `B689AF9C4E1F43011B83F285B1CE2C0AB33F988A6D79959B41661852B5D884D5`.
The normal three-target build exited zero, facts ran nineteen PASS with no
failures or ignored cases, including the previously unreachable self-alias
branch, and both direct weak receiver cases passed. The original twenty-five
ownership cases ran with twenty PASS and five FAIL: using owner/borrow scope
exit and three weak-release evidence cases remain unresolved. This is not an
ownership-suite GREEN. Source inventory 4,791, HEAD, six support/tool inputs and
three tested binaries were unchanged; all owned children exited naturally.

No test expectations, registrations, timeouts, diagnostic policy, source-query
policy, or platform acceptance gates are relaxed. These focused guard results
do not establish complete Windows, Linux GCC/Clang, or M1 acceptance.
