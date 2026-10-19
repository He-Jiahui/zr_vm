---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_reaching_definitions.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/types/type_system.c
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg/cfg.c
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semantic_da_ast_fact_cases.h
  - tests/parser/test_semantic_query.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_cfg_reachability.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_reaching_definitions.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
plan_sources:
  - "user: 2026-06-20 参照 docs/plans/lsp 优化语义推断能力"
  - "user: 2026-10-16 document DA callable AST identity, lambda CFG isolation and pending source-backed regressions"
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_definite_assignment_callable_identity_cases.h
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semantic_da_ast_fact_cases.h
  - tests/parser/test_semantic_query.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_cfg_reachability.c
  - tests/acceptance/2026-06-20-semantic-stage1-dataflow.md
doc_type: module-detail
---

# Dataflow Engine Foundation

## Purpose

The dataflow engine is the Stage 1 execution layer that runs analyses over `SZrParserCfg`. It is deliberately generic: concrete analyses such as definite assignment, reaching definitions, ownership flow, numeric intervals, and logical facts provide their own state shape and transfer rules.

The first reaching-definitions slice started with a straight-line resolver. `SZrSemanticReferenceFact` carries `definitionRange` / `hasDefinitionRange`, and `ZrParser_SemanticFacts_ResolveLinearReachingDefinitions` can annotate straight-line reference facts so a read points to the latest preceding declaration/write for the same resolved symbol. `ZrParser_SemanticQuery_DefinitionOf` consumes this payload for reads before falling back to declarations.

`semantic_reaching_definitions.c` now adds the first CFG-backed producer for that same payload. It maps resolved declaration/read/write facts to symbol ordinals, runs forward dataflow over script/function/test body CFGs, and tracks whether each symbol has one reaching declaration/write, no known definition, or multiple divergent definitions at a join. Reads keep a `definitionRange` only when the incoming state is a single known definition. Ambiguous states also preserve a bounded `definitionRanges` list, so query consumers that support multiple locations can show all known branch writes instead of incorrectly picking the last source-order write or falling back to the declaration.

Repeated source reads cloned into multiple `finally` CFG paths are accumulated by reference-fact index and applied after dataflow convergence. This prevents a normal path and a function-exit path from racing to write the same source read fact; if those paths see different declaration/write definitions, the read's `hasDefinitionRange` is cleared while `definitionRanges` retains the enumerable alternatives when they fit the current bounded payload. `semantic_reaching_definitions.c` also excludes structural parent statements such as `try`, `catch`, blocks, and `switch default` from whole-range fact matching, matching the definite-assignment producer so child-body facts only transfer in their real CFG blocks.

The first definite-assignment slice provides a reusable state lattice in `dataflow_definite_assignment.h/.c`. A state array is indexed by symbol ordinal and stores `UNINIT`, `INIT`, or `MAYBE_INIT`; joins keep equal values unchanged and collapse mismatches to `MAYBE_INIT`. `semantic_definite_assignment.c` now provides the first concrete CFG-backed producer: it maps resolved declaration/read/write reference facts to symbol ordinals, runs forward dataflow over each script/function/test body CFG, and annotates source read facts with the joined state seen at that read. Variable declarations with initializers keep the symbol `UNINIT` while the initializer expression is transferred, then mark the symbol `INIT` at statement end; this keeps `var seed = seed` from treating the initializer read as already assigned. Reads from a source `finally` body that is cloned into multiple CFG paths are accumulated by reference-fact index and written back after dataflow convergence, so normal and function-exit paths can join to `MAYBE_INIT` instead of whichever cloned block runs last. Structural parent statements such as `try`, `catch`, blocks, and `switch default` are excluded from whole-range fact matching so child-body reads and writes transfer only in their real CFG blocks. Constant-true `while`/traditional `for` loops and omitted-condition `for(;;)` loops no longer contribute a synthetic zero-iteration path to the loop join, so a write before an explicit `break` can stay definitely assigned at the following read.

The semantic fact layer also has a straight-line supplement: `ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments` walks reference facts in order, treats explicitly marked declarations as initial symbol state, treats writes as `INIT`, and annotates reads with the latest known state for diagnostics. Type inference and the compiler assignment lowering both pre-fill identifier write reference facts as `INIT`, so write facts carry a usable assignment-state payload before or during resolver passes. This mirrors the reaching-definitions bridge shape while the CFG-backed resolver starts covering branch joins.

## Behavior Model

`SZrParserDataflowAnalysis` describes one analysis:
- `direction` selects forward or backward propagation.
- `stateSize` defines the byte size of each per-block state value.
- `initEntry` initializes the boundary state: entry `inState` for forward, exit `outState` for backward.
- `join` merges a predecessor or successor state into the destination state and reports whether the destination changed.
- `transferStatement` applies one statement to the current block state.

`SZrParserDataflowResult` owns one `SZrParserDataflowBlockState` per CFG block. Each block state has `inState`, `outState`, and `isReachable`.

## Definite-Assignment AST Ownership

The private [semantic_da_ast.h](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.h)
and [semantic_da_ast.c](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.c)
provide the AST identity index used by
[semantic_definite_assignment.c](../../zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_definite_assignment.c).
This separates lexical containment from execution ownership: creating a lambda
does not execute its parameter or body facts in the enclosing initializer's DA
transfer. The helper does not change the generic dataflow callbacks or implicitly
give reaching definitions the same ownership policy.

### Index And Match API

`SZrSemanticDaAstIndex` stores a preorder entry for each owning AST occurrence.
Each entry records the node, its half-open subtree interval `[begin, end)` and
the nearest enclosing callable (`executionOwner`). A second, address-sorted
array points into those entries for binary-search lookup. A callable node itself
retains its enclosing owner; its children receive that callable as their owner.
Thus the outer statement can own the lambda expression without owning the
lambda's parameters and block. Function, method, accessor, generator and
callable-signature/type nodes also create ownership boundaries.

`ZrParser_SemanticDaAst_Match(index, subtree, factNode)` returns one of three
results:

- `INCLUDED`: the fact node is in the indexed subtree interval and has the same
  execution owner as that subtree.
- `EXCLUDED`: the real node is outside the subtree or belongs to another
  callable, the subtree is NULL, or a non-indexed fact node has parser
  ownership. A source range cannot rescue an excluded real node.
- `LEGACY`: the fact node is not indexed and is either absent or has no
  ownership object. This preserves the synthetic fact fixtures, not a fallback
  for parsed nodes that disagree with the index.

The DA adapter consumes ranges only after `LEGACY`: containment still requires
known ranges and the existing same-source check, preferring offsets when both
ranges provide them and otherwise comparing line/column endpoints. Declaration
slot lookup likewise permits its exact-pattern-range pass only for a legacy
fact. Real declaration or identifier-pattern identity remains the first pass,
and conflicting resolved symbol IDs still fail instead of selecting a name.
Structural parent statements remain excluded from direct fact transfer.

### Owning Edges And Lifetime

The walker follows the owning child layout of
[parser_ast_free.c](../../zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c),
including statement/expression children, arrays, parameters, generic
declarations and the AST children held by type descriptors. It does not follow
borrowed back-references. In particular, a call's callee is owned through its
primary expression; the function-call node contributes arguments and generic
arguments, not a second callee traversal. Future AST child-layout changes must
keep this private traversal aligned with the ownership/freeing layout.

`ZrParser_SemanticDaAst_Init` creates an empty index. `Build` requires a valid
state/global, a root and an empty initialized index. It counts owning nodes,
checks allocation-size overflow, allocates the preorder and address-lookup
buffers, then repeats the walk to fill them. Invalid array storage, allocation
failure, an inconsistent count or duplicate node addresses reject the build;
allocated buffers are released on failure. This expects the parser's acyclic
owning tree, not arbitrary cyclic or multiply-owned hand-built graphs.

The index borrows every AST node. It does not retain or free the root, its
ownership objects, source strings, semantic context or reference facts. The AST
node addresses and owning child layout must remain valid and unchanged while
the index is built and used. The resolver also borrows the context and stable
reference-fact array, while updating matched facts' DA state fields.
`ZrParser_SemanticDaAst_Free`
releases only the two index buffers using the recorded state and resets the
index; call it before releasing that state. Rebuild requires a freed/empty
index. This local allocation cleanup is not a transactional rollback of already
published semantic facts or a global OOM guarantee.

### Independent Lambda CFGs

`ZrParser_SemanticDaAst_VisitCfgRoots` visits indexed nodes in preorder and
dispatches script roots, function declarations and each actual lambda block.
It finds lambdas through owning descendants, including an `if` body, rather
than treating every nested block as a separate execution root. A false visitor
result stops traversal and propagates failure.

The public DA resolver also accepts an actual standalone BLOCK supplied as its
root, for example a function body extracted from a parsed script. The current
visitor includes a BLOCK only when it is the first indexed node (`position == 0`);
ordinary nested blocks are still part of their enclosing CFG, not independent
execution roots. R6 exposed that this entry contract had been lost: the resolver
returned TRUE without visiting that body or assigning its READ state. The narrow
top-level BLOCK dispatch repair passed the facts20 suite in R7; that run's
overall receipt nevertheless remains FAIL for other support results below.

`ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments` builds one index
and the symbol map, then synchronously runs a CFG/dataflow pass for each visited
root. Each pass owns and releases its CFG, dataflow result and fact-indexed
read-state/seen buffers. Successful passes apply their accumulated read states;
the shared map and AST index are released after traversal, including a failed
visitor. Lambda parameters and body reads are no longer transferred by the
outer initializer; the actual lambda block is scheduled for its own analysis.
Member bodies are excluded by callable identity, but independent method,
accessor and generator CFG scheduling is not supplied by this visitor.

## Control Flow

Forward analysis starts at CFG entry. It copies `inState` to `outState`, applies the statement transfer for statement blocks, then propagates `outState` into each successor `inState`.

Backward analysis starts at CFG exit. It copies `outState` to `inState`, applies the statement transfer for statement blocks, then propagates `inState` into each predecessor `outState`.

Both directions use a work queue and re-enqueue blocks when propagation changes downstream state. The first reachable predecessor/successor copies its state into the destination; later reachable paths call the analysis `join` callback. The queue stores each block at most once at a time.

## Reachability Filter

Before running the work queue, the engine computes CFG reachability from entry. Propagation ignores blocks that are not entry-reachable. This is important for backward analyses: a syntactically unreachable statement may still have an edge to exit, but it must not be treated as a real path from program entry.

CFG therefore connects `return` and `throw` terminators to exit for backward analysis while keeping post-terminator statements entry-unreachable.

## Test Coverage

`tests/parser/test_dataflow_engine.c` covers:
- forward propagation visits a reachable `return` statement and skips the statement after it;
- backward propagation reaches `return` through the exit edge and still skips the entry-unreachable statement after it;
- definite-assignment state propagation keeps a straight-line assignment as `INIT` at CFG exit;
- definite-assignment branch join marks `if (condition) { assignment }` with no `else` as `MAYBE_INIT` at CFG exit.

`tests/parser/test_reference_fact_emission.c` now also covers the first concrete reaching-definitions payload:
- `var seed = 1; seed = 3; seed;` keeps the read fact's `declarationRange` on the declaration token while `definitionRange` resolves to the latest assignment target after `ZrParser_SemanticFacts_ResolveLinearReachingDefinitions`.

`tests/parser/test_compiler_semantic_query_diagnostics.c` covers the first CFG-backed reaching-definitions branch join:
- `var seed; if (flag) { seed = 1; } else { seed = 2; } return seed;` first shows the linear resolver would resolve the final read to the last source-order write, then verifies `ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions` clears that read's single-definition payload because both branch writes reach the join.
- `var seed = 0; while (flag) { seed; seed = 1; } return seed;` verifies loop-carried paths clear single-definition payloads for reads that can see both the initializer and the prior iteration write.
- `var seed = 0; while (true) { seed = 1; break; } return seed;` verifies a constant-true loop with a write-before-break preserves the single reaching write for the final read.

`tests/parser/test_semantic_query.c` covers the first consumer:
- `DefinitionOf` on the read after `seed = 3` returns the matching write fact through `definitionRange`, while the returned fact still exposes the original declaration range.
- `DefinitionsOf` returns both write facts in source order when a read carries two reaching definition ranges, even if the payload was stored in reverse order.
- `Diagnostics` consumes linear definite-assignment resolver output, reporting the read before a write while ignoring the read after the write.

`tests/parser/test_semantic_facts.c` covers the straight-line definite-assignment fact producer:
- a declaration marked `UNINIT` makes the first read `UNINIT`, a later write becomes `INIT`, and the later read stays `INIT`;
- a declaration with no known state leaves the read unannotated as `UNKNOWN` / no-state.
- the CFG-backed resolver marks the read inside `var seed = seed` as `UNINIT` because the declaration initializer has not completed before that read.
- the CFG-backed resolver joins normal and function-exit paths through cloned `finally` blocks, so `try { if (flag) { seed = 1; return seed; } } finally { return seed; }` marks the final read as `MAYBE_INIT`.
- the CFG-backed resolver marks the final read in `var seed; while (true) { seed = 1; break; } return seed;` as `INIT`, proving the constant-true loop no longer contributes a fake uninitialized zero-iteration path.
- the CFG-backed reaching-definitions resolver joins normal and function-exit paths through cloned `finally` blocks, so the same final read clears `hasDefinitionRange` instead of falsely pointing at the guarded `seed = 1` write.
- LSP reaching-definition navigation now requires divergent `if/else` branch writes to return both branch write locations through `DefinitionsOf`.

`tests/parser/test_reference_fact_emission.c` covers the first production write-state payload:
- identifier assignment write facts are emitted with `hasDefiniteAssignmentState` and `INIT`.

`tests/parser/test_compiler_semantic_query_diagnostics.c` also covers the first CFG-backed compiler diagnostic consumer:
- `var seed; if (flag) { seed = 1; } return seed;` publishes `possibly_uninitialized_read` after CFG branch join state is written back to the return read fact.
- `var seed; while (true) { seed = 1; break; } return seed;` publishes no `uninitialized_read` or `possibly_uninitialized_read` diagnostic after the CFG removes the fake zero-iteration path.

`tests/parser/test_cfg_reachability.c` also locks the CFG invariant that `return` terminator blocks connect to exit.

### Lambda And Standalone-Body Regressions

The existing source-backed callable-identity fixture in
[test_definite_assignment_callable_identity_cases.h](../../tests/parser/test_definite_assignment_callable_identity_cases.h)
now adds two cases to the normal
[test_compiler_semantic_query_diagnostics.c](../../tests/parser/test_compiler_semantic_query_diagnostics.c)
registration, taking the default inventory from query75 to query77 without
removing the prior cases:

- `test_definite_assignment_lambda_body_reports_uninitialized_local` compiles
  a lambda containing `var missing: int; return missing;`. Every READ on the
  actual return identifier must resolve to that declaration and remain UNINIT;
  descriptor3002 must identify the real source range, with no
  possibly-uninitialized diagnostic.
- `test_definite_assignment_finds_lambda_inside_if_body` compiles a lambda in
  an `if (true)` body. Its actual parameter READ must be resolved and INIT, with
  no uninitialized or possibly-uninitialized diagnostic, requiring independent
  discovery and analysis of the nested lambda block.

The fixture parses and compiles real source before inspecting AST nodes,
resolved symbols and semantic facts. Compilation prerequisites are separate
from the DA assertions; it does not manufacture READ facts or symbol IDs. Its
global cleanup releases the compiler function/state and AST before the main
test teardown releases the VM state, including after a Unity assertion jump.

R11-support R6 executed all query77 cases, including these two additions, and
passed them. The earlier R4 receipt remains evidence for its own query75 inputs,
not retroactive execution credit for the new helper or cases.

The twentieth facts case is
[`test_cfg_definite_assignment_accepts_standalone_body_root`](../../tests/parser/test_semantic_da_ast_fact_cases.h).
It parses a real function with an uninitialized local, extracts its actual BLOCK,
then uses controlled resolved DECLARATION/READ facts sharing symbol 117 and the
real AST nodes. This is a public-resolver entry test, not a complete source
compilation fixture. It requires a successful resolver call to annotate that
READ as UNINIT; TRUE without visiting the root is not success for this contract.
The fixture owns its semantic context and script AST through static cleanup
before the VM state is destroyed. In R6 it reached the exact missing-state
behavior failure while all original nineteen facts cases passed.

## Limits And Next Steps

### Declaration Identity Regression

DA declaration-slot selection now first matches a resolved DECLARATION fact's
statement or identifier-pattern node. It rejects distinct symbol IDs matching
that identity instead of selecting whichever same-name symbol was registered
first. Only a known, same-source, exact pattern range can support the synthetic
fact fixtures; a statement's whole initializer range is not a declaration key.
The existing UNINIT-before-initializer and INIT-after-initializer order remains.

The real-source same-name regression failed before this repair. Windows focused
R4 receipt `8BA8F1F51B8DFC3ED5712305476948EF0FBDAE2AE578929852C5D39B99D0ADE2`
then verified the same-name positive, actual uninitialized-local negative and
original owner/member generic signature through the normal compiler/LSP path.
All original 19 semantic-facts tests passed, including self-initializer,
synthetic-range, cloned-finally and sourceless isolation controls. Default
query75 had one remaining lambda failure; LSP84 had one corresponding nested
metadata failure. AOT18 and guardrail6 retained their pending BOOL-result failures.

The negative fixture deliberately does not constrain how many READ facts the
compiler publishes for the same return identifier. Ownership validation and
slot-type inference currently both publish a READ. Instead, it requires the
actual AST identity, matching resolved declaration, UNINIT on every corresponding
READ and descriptor3002 diagnostics at that exact source range, with no
possibly-uninitialized diagnostic.

Callable-body ownership remained a separate pending repair at R4. The private
AST index and actual lambda-block CFG dispatch subsequently passed query77 and
the original nested-metadata LSP case in R6. That does not waive the newly
exposed standalone BLOCK entry regression or accept full Windows, Linux
GCC/Clang or M1.

### R11-Support R6 And R7 Boundary

The dated receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-lambda-and-origin-red-r6-receipt.json`,
SHA256 `5C66CA772D337D99291669926CBADE3ACC072E94E61C2ADD1869EF04E1496637`.
Unified handle `24087` naturally exited 0 and was drained; the normal build
exited 0 and all thirteen children terminated naturally. All 4810 source inputs,
HEAD `83e88fad7c70876c63b1fca86457259ba300d647`, support6 and binary8 were stable.

Query77, original LSP84 and ownership25 all passed. Facts20 completed with
19 PASS / 1 FAIL / 0 IGNORE: the standalone BLOCK case failed with
`Standalone BLOCK root must run its CFG rather than succeed without visiting it`.
SemIR4 retained its original two passes, while both new UNKNOWN/explicit-OBJECT
identity cases reached behavior FAIL. Token7 retained its original six passes;
the mixed-postfix case passed original-Span prerequisites but failed all five
AST anchor comparisons, with parent range `0..70`. AOT18 and guardrail6 each
retained their single mandatory BOOL behavior failure.

The receipt state is
`LAMBDA_IDENTITY_VERIFIED_AOT_AND_PROVENANCE_RED_BLOCK_REGRESSION_NO_PLATFORM_CREDIT`.
Controller exit 0 records this bounded observation contract, not all-suite
behavior GREEN. Root then made narrow standalone-BLOCK and default-UNKNOWN
repairs without changing this R6 result.

The subsequent R7 receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-da-aot-repair-r7-receipt.json`,
SHA256 `985AF370C14738D6AD637272505A4DC3348F8A7E9A1898E1063C3B1CE9229B80`.
It naturally exited 1 and was drained, with state FAIL. Normal build exited 0;
query77, ownership25, AOT18, original LSP84, guardrail6 and facts20 all passed.
The standalone BLOCK test now received its required UNINIT state. SemIR's two
original cases and missing-UNKNOWN case passed, but the explicit-OBJECT test
failed its incorrect plain-DYNAMIC expectation (`Expected 0 Was 12`): the
existing producer annotates OBJECT as GC_REF. That is not evidence of a dangling
type pointer or a completed downstream OBJECT control. Token7 kept six passes
and the same mixed-postfix behavior failure. All 4810 source inputs, the same
HEAD, support6 and binary8 were stable; all thirteen children terminated naturally.
The planned OBJECT fixture/identity correction has no R8 result here.
Mixed-source projection remains open; neither run accepts full Windows/Linux/M1
or supplies commit credit.

The engine does not yet enforce iteration caps or expose diagnostics for analysis bailout. CFG-backed definite assignment now has a first production integration for function/test/script body branch joins, declaration-initializer read ordering, cloned-finally read joins, and constant-true write-before-break loop precision, but broader loop fixed points, remaining finally edge cases, local reanalysis, and richer source-to-statement matching remain open. CFG-backed reaching definitions now handle conservative branch joins, representative loop regressions, cloned-finally read aggregation, a first multi-definition result surface, and deterministic same-source source-order query output, but broader loop fixed points, overload/member-aware ranking, and local recomputation remain pending. Ownership, numeric intervals, and logical facts remain future analyses.
