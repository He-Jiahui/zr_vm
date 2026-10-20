---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/token_span.h
  - zr_vm_parser/include/zr_vm_parser/token_stream.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_source.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/syntax/location.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_statements.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_core/src/zr_vm_core/gc/gc_roots.c
  - tests/parser/test_primary_expression_source_range_cases.h
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_token_source_edges.c
  - tests/parser/test_token_source.c
  - tests/parser/test_token_source_mixed_postfix_cases.h
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_weak_owner_release_evidence_cases.h
  - tests/language_server/test_ownership_diagnostics.c
  - tests/parser/test_aot_c_method_info_signature.c
  - lua/rust/compiler/rustc_parse/src/parser/expr.rs
  - lua/cpython/Lib/test/test_ast/test_ast.py
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_source.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/syntax/location.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_statements.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
plan_sources:
  - "user: 2026-10-16 document postfix PRIMARY parent source ranges, unchanged child identity, source-backed parse-only regression, and bounded R7 RED to R8 focused GREEN evidence"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
tests:
  - tests/parser/test_primary_expression_source_range_cases.h
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_token_source_edges.c
  - tests/parser/test_token_source.c
  - tests/parser/test_token_source_mixed_postfix_cases.h
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_weak_owner_release_evidence_cases.h
  - tests/language_server/test_ownership_diagnostics.c
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/acceptance/2026-10-16-macro-m1-token-foundation.md
doc_type: module-detail
---

# Primary Expression Source Ranges

## Purpose and Current Status

A postfix expression needs two distinct source views: the PRIMARY parent covers
the complete adopted chain, while its callee, argument, and member children keep
their own narrow ranges and AST identities. A parent left at the first identifier
can exclude an argument that is genuinely in its subtree. Expanding every child
to the parent's range would instead destroy precise semantic queries.

The bounded producer is
[append_primary_member](../../zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c).
Its frozen SHA256 is
`B8C541143C7D0345305E933738954C4FD21742C02A59F84515AB9A7CAAB25C35`
(52 lines, 2107 bytes). It now merges the parent and newly adopted member ranges
on both successful paths. No syntax, macro surface, child range, or ownership
protocol was added.

The source-backed regression first reached genuine parent-range RED in r7.
R8 then passed the default query72, ownership25, and Windows AOT17 suites under
stable complete inputs. Its state is
`FOCUSED_DEFAULT_SUITES_ALL_PASS_NOT_PLATFORM_ACCEPTED`, not Windows, Linux, or
M1 acceptance. The subsequent complete Windows r11 finished FAIL: foundation52,
lower323, and original127 passed, but full55 had 53 PASS / 2 FAIL and HEAD changed.
The same-physical-coordinate repair did not decide mixed-source projection policy.
The user has since approved retaining original TokenSpan provenance, AST-identity
attribution, and cross-source diagnostic anchors with related origins. That is a
chosen direction, not a completed mixed-source repair. R11-support R6 passed
query77, original LSP84 and ownership25, but its new mixed-postfix case reached
real anchor-projection behavior RED. R7 retained that failure despite passing
the lambda, standalone BLOCK, AOT18 and guardrail6 suites. Its strict receipt
remains FAIL, with no full Windows/Linux/M1 credit.

## Range Contract

The public [location types](../../zr_vm_parser/include/zr_vm_parser/location.h)
define source byte offsets and parser-produced one-based lines and columns.
Normal parser ranges are half-open: start included, end excluded. The source
field borrows a VM string; assigning a range does not acquire ownership of it.
Placeholder ranges can be empty and are not made valid merely by their type.

[ZrParser_FileRange_Merge](../../zr_vm_parser/src/zr_vm_parser/syntax/location.c)
has a narrower contract than validation or source mapping:

| Field | Merge behavior | Caller responsibility |
| --- | --- | --- |
| Start | Select the earlier byte offset and copy that complete position | Supply an accurate start offset and matching line/column |
| End | Select the later byte offset and copy that complete position | Supply an accurate exclusive end and matching line/column |
| Source | Retain the first operand's source pointer | Ensure both ranges belong to the same source and keep the borrowed string alive |

The helper does not compare source names, recompute line/column from offsets, or
reject malformed ranges. Equal offsets follow the implementation's existing
comparison branches; there is no new coordinate normalization. A merge can
preserve an already valid envelope, but cannot repair arbitrary inconsistent
inputs or combine ranges from different files meaningfully.

For valid same-source postfix construction, the resulting parent includes the
adopted segment's closing delimiter or member token. It does not absorb the
following semicolon, whitespace, or next statement merely because the parser
has advanced beyond them.

### Mixed-Source Direction Approved, Validation Open

Equal `FileRange.source` pointers do not establish a common physical coordinate
system for generated token input. The existing
[TokenSource](../../zr_vm_parser/src/zr_vm_parser/syntax/token_source.c) retains
each tree's original `TokenSpan`, including its `sourceId` and coordinates.
The legacy [parser location projection](../../zr_vm_parser/src/zr_vm_parser/parser/parser_state.c)
copies those coordinates while assigning `lexer->sourceName` to the range; it
does not logically renumber tokens or retain `sourceId` in `FileRange`.

The existing [cross-source cursor case](../../tests/parser/test_token_source_edges.c)
allows one token at source A's `6..7` followed by another at source B's `0..1`.
It verifies lexer/split provenance, not a mixed-source postfix AST envelope.
Public builder validation and sealing do not impose cross-token same-source or
monotonic-coordinate restrictions, so the parser boundary is publicly reachable.

Consequently the two parent merges and five single-source shapes do not prove
mixed-source PRIMARY attribution or diagnostics. Unrelated origin coordinates
cannot meaningfully become one physical range by taking their minimum/maximum.
The `TokenSpan`-to-legacy-range projection policy needs a source-backed parser
regression before the M1 provenance gate is accepted. At the earlier R4 boundary
there was no executed mixed-postfix behavior RED; R6 subsequently supplied the
counterexample below, not a production repair. Canonical token origins must not
be reordered to force a range assertion, and an ownership-only fallback is not a
substitute for a shared projection/attribution contract.

At the r8/r11 documentation boundary the product policy was still undecided.
The user has now approved this direction: retain original TokenSpan provenance,
use AST identity to determine semantic ownership, and present a diagnostic anchor
plus related origins when evidence crosses sources. Do not rewrite origin spans
to manufacture a single physical envelope. The source-backed mixed-postfix
regression has now executed and remains RED through R7; anchor/related-origin
repair is still open. An unanswered product decision is no longer the current
blocker. Single-source focused GREEN supplies no such credit.

### Executed Mixed-Source Counterexample

The new [mixed-postfix header](../../tests/parser/test_token_source_mixed_postfix_cases.h)
adds one case to [the token-source main](../../tests/parser/test_token_source.c),
giving token7 with the original six-case ordered prefix unchanged. It tokenizes
`target` after 64 spaces in `left.zr` and `(watcher); return 9;` in `right.zr`,
concatenates the sealed streams and parses the result using `expansion.zr` as
the parser's expansion name. Original TokenSpan checks require `left.zr:64..70`,
the `right.zr:0..9` group and `right.zr:1..8` argument before and after parsing.
Actual parse, EOS, AST kinds, argument/member counts and following RETURN are
prerequisites, not the behavior comparison.

The expected AST anchors are target/parent at `left.zr:64..70`, CALL at
`right.zr:0..9`, argument at `right.zr:1..8`, and following RETURN at
`right.zr:11..20`, including line/column endpoints. R6 passed the original-Span
and parse prerequisites, then printed all five comparisons false and parent
range `0..70`; the final anchor assertion failed. R7 retained the same raw
failure. Original spans were not rewritten to obtain this result. This case
tests AST anchor projection, not materialized structured-diagnostic related
information, macro expansion completeness or cross-source semantic correctness.

Static fixture storage keeps the AST, parser, three retained streams, context
table and expansion-name root lease available to teardown after a Unity jump.
Cleanup releases them while the VM state is alive. The case does not compile
source, run the VM, emit C or create fixture files.

## Successful Adoption Paths

The existing helper owns valid nonnull base/member inputs on entry. The change
adds only two assignments after successful member-array insertion:

| Existing shape | Ownership and identity | Parent range after successful adoption |
| --- | --- | --- |
| Base already is PRIMARY | Reuse that parent and its existing member array; append the real member node | Merge `base->location` with `memberNode->location` |
| Base is not PRIMARY | Create the existing PRIMARY wrapper, retain base as `property`, and append the real member node | Merge the wrapper's initial `startLoc` range with `memberNode->location` |

The first operand is consequently the existing parent's range or the newly
created wrapper's range. Its source identity is preserved. In the wrapping path,
the caller still must provide a valid `startLoc` covering the base: this helper
does not independently reconstruct a missing base envelope.

[Postfix call parsing](../../zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c)
builds the FUNCTION_CALL segment from its segment start through the closing
parenthesis, then transfers it through this helper. The normal
[primary parser](../../zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c)
also routes member and index segments through it. Repeated successful append
therefore grows a chain incrementally. Nested calls grow their own PRIMARY
before their enclosing call adopts them as arguments.

The assignments do not clone or replace a child, widen the identifier/callee,
rewrite CALL arguments, change member access modes, or allocate another array.
The existing wrapper allocation is unchanged; the range correction itself is
plain struct assignment. AST pointer identity and source ownership remain with
the same nodes and owners as before.

## Failure and Synthetic-AST Boundaries

A failed allocation or array append still returns NULL and destroys the owned
trees through the existing [AST release path](../../zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c).
In the reused-parent path, base and the unadopted member are released. In the
wrapper path, releasing the wrapper releases its adopted base, while the member
is separately released if it was not adopted. Neither new merge runs before a
successful append, and no new cleanup resource or failure mode was introduced.

The ownership-transfer contract applies to valid inputs. The existing invalid
argument guard returns base; it is not a general guarantee to dispose of an
arbitrary member when parser/base/member is NULL.

Only construction through this helper receives this incremental parent update.
Hand-built ASTs, temporary PRIMARY views, direct array edits, and other node
constructors do not automatically gain valid enclosing ranges. Their callers
must continue supplying consistent source, offsets, and line/column tuples.
This change is not a global AST repair pass, cross-source range validator, OOM
transaction, or promise that every synthetic expression has an exact envelope.

## Semantic Consumers

The parent/child distinction supports several concrete consumers:

- [Ownership statement attribution](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_statements.c), `ZrParser_DataflowOwnership_FactInStatement`, accepts direct node identity or same-source containment in the statement's relevant expression. An argument fact can be lost when the PRIMARY envelope stops at the callee.
- [Rejected weak-call classification](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_ownership_moves.c) requires the statement expression to contain a resolved WEAK READ before using its matching ERROR's explicit `requiresWake` evidence. It does not invent a resolved semantic CALL for a rejected call or classify errors from English diagnostic text.
- [Definite-assignment declaration selection](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c) now prioritizes the actual declaration/pattern AST identity, with a known exact same-source pattern-range fallback for synthetic facts. Its R11-support R4 evidence does not validate mixed-source projection or all nested callable ownership.
- [Call fact publication](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c) retains the target's narrow range when available. [Reference queries](../../zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c) compare source positions and prefer narrow matching facts. The full PRIMARY envelope is not permission to widen those callee facts.
- [Receiver guard publication](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c) keeps the actual segment/receiver identities while recording the PRIMARY range. Existing node-based guard lookup remains node-based; a range correction does not synthesize a guard or change its ownership policy.
- [LSP receiver-prefix construction](../../zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c), `receiver_build_primary_prefix`, borrows the original property and a member-array prefix and copies the original parent location. It does not reparse or calculate a tighter prefix envelope. This temporary-view contract is unchanged and is not an automatic range guarantee for hand-built ASTs.

R6's failure-only weak diagnostic had a resolved READ at `296..303` and an ERROR
with `requiresWake=1`, but the statement expression's PRIMARY covered only the
function name at `284..291`; the read classifier returned false. The independent
parse-only regression below isolated the producer-level parent-range defect
without compiling that ownership fixture. See
[ownership release evidence](ownership-flow-release-evidence.md) for the shared
CFG/query contract, not for a separate range repair in a consumer.

## Source-Backed Regression

[The new header](../../tests/parser/test_primary_expression_source_range_cases.h)
defines one Unity case,
`test_primary_expression_ranges_include_postfix_members`, looping over five
real sources. These are five shapes, not five extra Unity registrations.

| First expression before `; return 9;` | Members | Required parent byte range | Additional check |
| --- | --- | --- | --- |
| `target(watcher)` | 1 | `0..15` | Callee, CALL segment, and argument keep their local ranges |
| `owner.field` | 1 | `0..11` | Dot member and field identifier remain distinct |
| `owner[0]` | 1 | `0..8` | Computed member and integer index retain their ranges/value |
| `owner.call(watcher).field[0]` | 4 | `0..28` | Every intermediate member/CALL segment remains correctly bounded |
| `target(other(watcher)).field` | 2 | `0..28` | Nested `other(watcher)` PRIMARY must cover `7..21` |

Each iteration creates an actual parser state and uses public
`ZrParser_ParseWithState`. Prerequisites require no parser/fatal/lexical error,
EOS, a SCRIPT with exactly two statements, the expected local node kinds and
member counts, exact identifier text, and the following RETURN's actual range
and value 9. The postfix segment and leaf checks are prerequisites, not the
parent-envelope assertion under test.

The behavior comparison checks source pointer identity, both byte offsets, and
both line/column endpoints. The fixtures are single-line ASCII; they do not by
themselves cover multiline/Unicode coordinates, malformed recovery, cross-source
input, allocation fault injection, or every optional/construction postfix form.
No compiler, semantic fact publisher, C writer, VM execution, or fixture file is
used by this parse-only case. An AST FUNCTION_CALL is not a fabricated resolved
semantic CALL fact.

The case records all five parent comparisons, including the nested comparison,
then asserts the executed count and aggregate result. A prerequisite failure
longjmps immediately and cannot count as a parent-range behavior RED. A behavior
mismatch is retained while subsequent valid sources are still inspected.

## Fixture Lifetime and Registration

The fixture stores parser, AST, source-name string, and GC root lease statically
so Unity longjmp does not lose cleanup handles. The source name is rooted through
the [existing lease API](../../zr_vm_core/src/zr_vm_core/gc/gc_roots.c).
Cleanup frees the AST, frees an initialized parser state, releases the source-name
lease, and zeros the fixture while the runtime state is still alive. It runs
between inputs and from the main's `tearDown` before runtime-state destruction.
No borrowed AST/range/source pointer may be used after that cleanup.

[The existing main](../../tests/parser/test_compiler_semantic_query_diagnostics.c)
adds the cleanup declaration/call, one include, and one final RUN_TEST. It retains
the original64, weak4, using2, and direct-degrade ordered prefix, producing default
72 registrations in r7/r8 with no new selector. The later R11-support R3/R4
appended three DA identity cases and executed 75; that does not change these
dated parse-only inputs. R11-support R6 appended two more lambda controls and
passed query77, while the separate token-source target executed token7 above.
Frozen inputs for the earlier r7/r8 single-source regression were:

| Input | SHA256 |
| --- | --- |
| New header | `9891DBCBE04246D8A5015829D33E16960742DF8A917866EF69DE09E1D05A0849` |
| Main | `BB4A56ABE41D66F6C65F391E1B096CF8D3DB58AEC72F667A433F7969540E9B5F` |

These identities describe the dated regression inputs, not an unrestricted
claim about future checkout contents or complete parser coverage.

## Reference-Language Evidence

The local [Rust call parser](../../lua/rust/compiler/rustc_parse/src/parser/expr.rs)
at line 1270 constructs the call with `lo.to(self.prev_token.span)` after parsing
the argument sequence, retaining the function expression inside the call.
The local [CPython AST tests](../../lua/cpython/Lib/test/test_ast/test_ast.py)
at lines 2762-2766 parse `x[0]()` and independently check a narrow `call.func`
containing `x[0]` and the complete call's end position.

The common structural precedent is a complete call parent with precise child
locations, not identical AST node kinds or coordinate conventions. These files
were read as local primary evidence; no Rust/CPython test execution or imported
validation credit is claimed.

## Actual R7 RED and R8 Focused GREEN

The following runs are dated 2026-10-17 UTC. R7 receipt:
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r7-receipt.json`,
SHA256 `4EBF6E3F8CD04D58B80894E24C025206AD7786DB38EB27D2C0F299D7B4B5E7C3`.
Its source pin used the old producer
`BF532C6FB153B2E5A097C200F65DE506DF63675CB1D7D5ACF8C45D0D965A3403`
(1919 bytes), not the current two-merge implementation. Removing just those two
assignments in RAM restores that exact old identity.

All five sources passed leaf, CALL/member, and following-RETURN prerequisites.
The root PRIMARYs remained at `0..6`, `0..5`, `0..5`, `0..5`, and `0..6`;
the nested PRIMARY remained at `7..12` instead of `7..21`. These were actual
parent-range behavior failures, not malformed fixtures. Query completed
`72 Tests 2 Failures 0 Ignored`: this new case and the main weak case failed.
Ownership had 22 PASS / 3 weak FAIL; AOT17 all passed. Controller handle `23976`
exited 0 only as a controlled observer, with state
`CONTROLLED_LOWER_SUPPORT_OBSERVATIONS_NOT_PLATFORM_ACCEPTED`.

R8 receipt:
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-lower-support-observation-r8-receipt.json`,
SHA256 `FFC86B539C59CB9FD4FE1CFA9F3E19741793DA1AEC8BEE319B17A2E99E9E907D`.
With the frozen two-merge producer, controller handle `37119` naturally exited 0
and was drained. Normal build and all three default native suites exited 0:

| Actual R8 default suite | Result |
| --- | --- |
| Query | 72 PASS / 0 FAIL / 0 IGNORE, including all five PRIMARY shapes/nested range, main weak evidence, controls, using2, and direct-degrade |
| Ownership | 25 ordered custom-protocol results, all PASS; this is not a Unity footer |
| Windows AOT | 17 PASS / 0 FAIL / 0 IGNORE, retaining scalar, parameter4, Stamp, and owned-copy cases |

Each run independently held its 4801 whole-source inputs, HEAD
`2e2c95d004317014334c9a09de599734d8da9746`, support6, and binary3 stable before
and after; all eight children terminated naturally in each run. The source pins
between R7 and R8 intentionally differ by the producer repair even though HEAD
is the same. Stable HEAD alone is not a substitute for complete source pins.
There was no case/order reduction, IGNORE, or fixture failure used to obtain R8.

R8's exact state remains `FOCUSED_DEFAULT_SUITES_ALL_PASS_NOT_PLATFORM_ACCEPTED`.
It supplies a dated producer RED-to-GREEN sequence and focused integration
evidence, not full Windows/Linux acceptance, a W0 claim, complete macro support,
M1 acceptance, or commit credit. Only the two producer assignments changed between
these R7/R8 source pins; parameter entry and using producers did not change.

## Actual Full R11 Remains FAIL

The inner receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-receipt.json`, SHA256
`7C7D2DDCDCCE2BC12135D53B927DE86D3D5783A9621728E118626C47DB0CFF78`;
the outer receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-current-r11-environment-launch-receipt.json`,
SHA256 `3AE3BD1E7E4317BD7FEF8D9BB256F3C0ACBD5E9485AEF89D3CBAA98C11D17995`.
Foundation52, lower323, and original127 all passed. Full55 completed with 53 PASS /
2 FAIL: `language_pipeline` and `language_server`.

The pipeline guardrail run had 6 tests, 5 PASS / 1 FAIL; its bool typed-call
value-free assertion failed. Within the original semantic analyzer84,
`Semantic Analyzer Nested Source Metadata Is Complete` failed with the case7
diagnostic, and `Semantic Analyzer Preserves Owner Generic Context In Member
Signatures` also failed. These observed failures do not establish their causes
or validate a mixed-source implementation. The later user-approved direction
does not retroactively change this r11 result.

The 4801 source inputs were unchanged, but HEAD advanced from
`2e2c95d004317014334c9a09de599734d8da9746` to
`d7d4c8f28a4dd78920ce5ab1bb9e882f68e51dc0`; the strict receipts remain FAIL.
The later CLI gate, outer auxiliary native3+4, and source-final were not run.
Linux remains unpassed, M1 remains unaccepted, and neither commit nor M2
advancement has occurred. R8's bounded focused GREEN is not upgraded by R11.

## Later R11-Support Identity Evidence

The later R3 receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lower-support-red-r3-receipt.json`,
SHA256 `8D63E96790549D636DAC7B901EFF6ABEDB8E9C8AAB5BC6690F98B0BD443F06B5`,
remains FAIL. Query75 had two genuine positive behavior failures and a third
negative-control prerequisite failure: `count==1` was false because the same
return AST was inferred twice and produced two READs. That is not a third
behavior RED or a reason to suppress real duplicate evidence.

R4 receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-declaration-identity-r4-receipt.json`,
SHA256 `8BA8F1F51B8DFC3ED5712305476948EF0FBDAE2AE578929852C5D39B99D0ADE2`,
has state `DECLARATION_IDENTITY_VERIFIED_OTHER_SUPPORT_RED_NO_PLATFORM_CREDIT`.
Build exited 0; query75 had 74 PASS / 1 lambda FAIL, ownership25 and facts19 all
passed, AOT18 had 17 PASS / 1 BOOL FAIL, LSP84 had 83 PASS / 1 nested-lambda FAIL,
and guardrail6 had 5 PASS / 1 BOOL FAIL. The original generic signature case passed.
All 4805 source inputs, HEAD `676c3c88bc39d155c16cae873ee565207b3c166b`,
support6, and binary6 were stable; all eleven children terminated naturally.

The executed DA repair (`DD206F9B` full identity in the
[parameter module](callable-parameter-semantic-identity.md)) preserves precise
same-source synthetic pattern ranges while preferring real declaration identity.
The corrected negative requires actual AST/resolved-symbol evidence, every
matching READ to remain UNINIT, and descriptor 3002's exact range, without fixing
duplicate counts. It is not a mixed-source regression. The private lambda module
and unknown/explicit-OBJECT regressions had no native result at that R4 boundary;
the later R6/R7 observations below do not rewrite it. Full Windows/Linux/M1
remain unaccepted; no commit was made and released space is 0 B.

## R11-Support R6/R7 Provenance Boundary

R6 receipt:
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lambda-and-origin-red-r6-receipt.json`,
SHA256 `5C66CA772D337D99291669926CBADE3ACC072E94E61C2ADD1869EF04E1496637`.
Unified `24087` naturally exited 0 and was drained, normal build exited 0,
and all thirteen children terminated naturally. All 4810 source inputs, HEAD
`83e88fad7c70876c63b1fca86457259ba300d647`, support6 and binary8 were stable.
Query77, original LSP84 and ownership25 all passed, including the independent
lambda controls. Facts20 retained nineteen passes and a standalone-BLOCK
missing-state failure; SemIR4 retained two passes and two UNKNOWN/OBJECT behavior
failures. Token7 had six passes and the mixed-anchor behavior failure above.
AOT18 and guardrail6 each retained their original single BOOL failure. Its state
was `LAMBDA_IDENTITY_VERIFIED_AOT_AND_PROVENANCE_RED_BLOCK_REGRESSION_NO_PLATFORM_CREDIT`;
the zero observer exit is not overall behavior GREEN.

R7 receipt:
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-da-aot-repair-r7-receipt.json`,
SHA256 `985AF370C14738D6AD637272505A4DC3348F8A7E9A1898E1063C3B1CE9229B80`.
It naturally exited 1 and was drained with state FAIL. Build exited 0;
query77/ownership25/AOT18/LSP84/guardrail6/facts20 all passed after the narrow
BLOCK/default-UNKNOWN repairs. SemIR's original two and UNKNOWN case passed;
the new OBJECT case failed at its incorrect DYNAMIC expectation (`Expected 0 Was 12`,
existing OBJECT-to-GC_REF annotation), not an established dangling
identity defect or completed downstream control. Token7 kept six passes and
the same mixed-anchor RED. All 4810 source inputs, the same HEAD, support6 and
binary8 were stable; all thirteen children terminated naturally. The planned
OBJECT fixture/identity correction has no R8 credit. Cross-source projection
and related-origin diagnostic validation remain open; no platform, M1 or commit
credit follows from either run.

The complete M1-M6 scope and platform gates remain governed by the
[design plan](../plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md)
and [M1 acceptance document](../../tests/acceptance/2026-10-16-macro-m1-token-foundation.md).
