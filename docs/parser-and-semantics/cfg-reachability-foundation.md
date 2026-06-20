---
related_code:
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - tests/parser/test_cfg_reachability.c
  - tests/parser/test_cfg_constant_conditions.c
  - tests/parser/test_cfg_switch_constants.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
plan_sources:
  - user: 2026-06-20 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_cfg_reachability.c
  - tests/parser/test_cfg_constant_conditions.c
  - tests/parser/test_cfg_switch_constants.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/acceptance/2026-06-20-semantic-stage1-cfg.md
doc_type: module-detail
---

# CFG Reachability Foundation

## Purpose

The parser now has the first Stage 1 CFG producer for semantic reachability facts. This is the bottom layer for later dataflow, definite assignment, ownership flow, numeric ranges, and the shared semantic query API.

## Current Scope

`SZrParserCfg` builds an entry block, statement blocks for analyzed statements, join blocks for supported branch and loop merge points, and an exit block. The current slice supports `ZR_AST_SCRIPT`, `ZR_AST_BLOCK`, function bodies, test bodies, straight-line statement lists, `ZR_AST_IF_EXPRESSION` nodes used as statements, `ZR_AST_SWITCH_EXPRESSION` nodes used as statements, `ZR_AST_WHILE_LOOP` nodes used as statements, `ZR_AST_FOR_LOOP` statement graphs, the first `ZR_AST_FOREACH_LOOP` statement graph, and `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` try/catch/finally body graphs with `return`/`throw`/`break`/`continue` abrupt paths into `finally`.

The CFG implementation is split across small construction files. `cfg.c` owns shared block allocation, straight-line statement construction, top-level build/reset, and reachability fact emission. `cfg_loops.c` owns `while`/`for`/`foreach` graph construction and loop-control target wiring. `cfg_control_flow.c` owns larger switch and try/catch/finally graph construction. `cfg_internal.h` is private to the type-inference implementation and exposes only the helpers needed across that split.

Function-exiting terminators (`return` and `throw`) are connected to the exit block. They still do not fall through to the next source statement, so following statements remain entry-unreachable, but backward dataflow can still start from exit and reach real function exits.

Terminator statements stop fallthrough:
- `ZR_AST_RETURN_STATEMENT` emits `ZR_SEMANTIC_REACHABILITY_AFTER_RETURN`.
- `ZR_AST_THROW_STATEMENT` emits `ZR_SEMANTIC_REACHABILITY_AFTER_THROW`.
- `ZR_AST_BREAK_CONTINUE_STATEMENT` emits `AFTER_BREAK` or `AFTER_CONTINUE` from the AST payload.

Loop-control terminators now also carry basic target edges when they appear inside supported loop bodies. `break` connects to the active loop join block. `continue` connects to the active loop header for `while` and `foreach`; for traditional `for`, `continue` connects to the step-entry join when a step exists, otherwise to the loop condition block.

Boolean condition folding is shared between `if`, `while`, and traditional `for` graph construction. It currently recognizes boolean literals, unary `!` over a folded boolean expression, logical `&&` / `||` when both operands fold to boolean constants, short-circuit decisive logical operands such as `false && unknown` or `true || unknown`, integer/char/float literal binary comparisons for `==`, `!=`, `<`, `>`, `<=`, and `>=`, and string literal equality/inequality. The folded expression node remains the `causeNode`, so diagnostics and hovers can point at the source condition rather than only the inner literal.

For `if` statements, the CFG uses the `ZR_AST_IF_EXPRESSION` node as the condition statement block. The then and else bodies are built as separate subgraphs and reconnect through a join block when their final block can fall through. A known boolean condition prunes the impossible branch; unreachable statements in that pruned branch use `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH` with `causeNode` set to the condition expression.

For `switch` statements, the CFG uses the `ZR_AST_SWITCH_EXPRESSION` node as the selector statement block and builds switch case bodies as subgraphs. This first slice treats cases as reachable candidate bodies so terminators inside a case can mark following case statements unreachable. When the selector and case values are comparable boolean, integer, string, char, or float constants, non-matching cases are pruned and receive `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH` with the selector expression as the cause. Boolean switch constants reuse the shared condition folder, so selectors and case values can be boolean literals or folded unary/logical boolean expressions. A matching constant case consumes the constant selector, so a following default branch is pruned with the same cause. If that matching case terminates and there is no default, the switch no longer keeps a synthetic no-match fallthrough edge to later statements. String literals compare with `ZrCore_String_Equal`; float literals compare the parsed `TZrDouble` value exactly; string or char literals marked with parse errors are not treated as constants. The CFG does not yet do union exhaustiveness.

For `while` statements, the CFG uses the `ZR_AST_WHILE_LOOP` node as the condition statement block. The loop body reconnects to the condition block through a back edge when it can fall through, `continue` targets the condition block, and `break` targets the loop join. A known false boolean condition prunes the body; unreachable statements in that body use `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE` with `causeNode` set to the condition expression.

For traditional `for` statements, the CFG builds the optional initializer before the loop condition block, uses the `ZR_AST_FOR_LOOP` node as the loop condition block, builds the loop body, builds the optional step expression after a fallthrough body, and reconnects the step or body back to the condition block. `break` targets the loop join. `continue` targets the step-entry join when a step expression exists, so the step still executes before returning to the condition block; otherwise it targets the condition block directly. A known false boolean condition prunes the body just like `while(false)`; unreachable statements in that body use `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE` with `causeNode` set to the `for` condition expression.

For `foreach` statements, the CFG uses the `ZR_AST_FOREACH_LOOP` node as the loop entry/iteration statement block, builds the loop body, reconnects a fallthrough body back to the foreach block, and adds a join block for zero iterations or loop exit. `break` targets the join block and `continue` targets the foreach iteration block. This first slice does not yet model iterator failure or item binding state.

For `try` statements, the CFG uses the `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` node as the try statement block and builds the try body, each catch body, and the finally body as subgraphs. `return`, `throw`, `break`, and `continue` terminators inside the protected try/catch range now also connect to the finally entry join, so reachability propagation sees the `finally` body even when the try/catch path exits abruptly. If there was no normal try/catch completion before those abrupt edges were added, the finally body's fallthrough is not connected to the final join, preserving later source statements as unreachable. The model still does not perform precise exception filtering, catch matching, or `break`/`continue` target rewriting after `finally`; loop-control terminators still retain their existing loop target edge as well as the finally reachability edge.

## Data Flow

`ZrParser_Cfg_Build` creates linear statement blocks and only links a statement block to its successor when the previous statement is not a terminator. It records the upstream terminator as `unreachableCauseNode` for later unreachable blocks.

When building an `if` statement, the condition block gets one or two outgoing branch edges depending on whether the condition is known. Unknown conditions connect to both branch bodies. A known true condition omits the else edge; a known false condition omits the then edge. The omitted branch still gets blocks so facts can be emitted, but those blocks have no path from entry.

When building a `switch` statement, the selector block starts a small case-chain graph. Each `ZR_AST_SWITCH_CASE` gets its own statement block, its block body is built as a subgraph, and a fallthrough case body connects to the switch join block. The selector/case chain also falls through to the next case candidate, which keeps fanout within the current fixed successor limit while still visiting case bodies for reachability facts. A comparable boolean, integer, string, char, or float selector omits the edge into a non-matching comparable case while still building the case block and body for fact emission. A matching comparable case consumes the selector; after that point, later cases and default are not connected as possible no-match paths, but are still built for fact emission.

When building a `while` statement, the condition block connects to the body unless the condition folds to a known false boolean value. The condition block also connects to the join block so later statements remain reachable after zero iterations. A body that falls through connects back to the condition block, `break` connects to the join block, and `continue` connects to the condition block.

When building a traditional `for` statement, the optional initializer is part of the predecessor flow, then the condition block decides whether the loop body is entry-reachable. Unknown or known true conditions connect to the body; a known false condition omits the body edge while still building body blocks for fact emission. The loop condition block always connects to the join block so later statements remain reachable after zero iterations. A body that can fall through goes through the optional step expression when present, then back to the loop condition block. `break` connects to the join block. `continue` enters the step subgraph when present, or the condition block when no step exists.

When building a `foreach` statement, the foreach block connects to both the body and the join block. The body reconnects to the foreach block only when its final block can fall through. `break` connects to the join block, and `continue` connects to the foreach block. A `break` or `continue` terminator inside the body also blocks fallthrough to following body statements and records the terminator as the pending unreachable cause for those following statements.

When building a `try` statement, the try statement block connects to the try body and to each catch body. A fallthrough try or catch body connects to a join block. If a finally body exists, the builder records whether that join block had normal predecessors before abrupt edges are added, then scans the try/catch block range for `return`, `throw`, `break`, and `continue` terminators and connects those blocks to the finally entry join. The finally body is then built from that join. Its fallthrough connects to a final join only when normal completion existed before the abrupt edges, which keeps `try { return; } finally { A } B` from making `B` reachable solely because `A` can run during return unwinding. For `break` and `continue`, this is currently a reachability edge rather than a full target rewrite through the finally body.

`ZrParser_Cfg_EmitReachabilityFacts` marks blocks reachable from entry, then appends a `SZrSemanticReachabilityFact` for every unvisited statement block. The fact range and node come from the unreachable statement; the cause points back to the terminator.

## Validation

`tests/parser/test_cfg_reachability.c` covers:
- statement after `return` becomes unreachable with `AFTER_RETURN` and causeNode set to the return statement;
- `return` terminator blocks connect to the CFG exit block while preserving the following statement as entry-unreachable;
- expression after expression remains reachable and emits no reachability fact.
- `if (true) { A } else { B }` marks the else statement unreachable with `CONSTANT_BRANCH` and causeNode set to the boolean condition.
- `while (false) { A }` marks the body statement unreachable with `CONDITION_FALSE` and causeNode set to the boolean condition.
- `for (; false; step) { A }` marks the body statement unreachable with `CONDITION_FALSE` and causeNode set to the boolean condition.
- `foreach (...) { break; A }` marks the body statement after `break` unreachable with `AFTER_BREAK` and causeNode set to the break statement.
- `while (true) { break; }` connects the `break` terminator block to the while join block.
- `while (true) { continue; }` connects the `continue` terminator block back to the while condition block.
- `for (; true; step) { break; }` connects the `break` terminator block to the for join block.
- `for (; true; step) { continue; }` connects the `continue` terminator block to the for step-entry join, and the step expression reconnects to the for condition block.
- `foreach (...) { break; }` connects the `break` terminator block to the foreach join block.
- `foreach (...) { continue; }` connects the `continue` terminator block back to the foreach iteration block.
- `try { return; A }` marks the try-body statement after `return` unreachable with `AFTER_RETURN` and causeNode set to the return statement.
- `try { A } catch { return; B }` marks the catch-body statement after `return` unreachable with `AFTER_RETURN` and causeNode set to the return statement.
- `try { A } finally { return; B }` marks the finally-body statement after `return` unreachable with `AFTER_RETURN` and causeNode set to the return statement.
- `switch (x) { case y { return; A } }` marks the case-body statement after `return` unreachable with `AFTER_RETURN` and causeNode set to the return statement.
- `switch (true) { case false { A } }` marks the non-matching boolean case unreachable with `CONSTANT_BRANCH` and causeNode set to the switch selector.
- `switch (true) { case true { A } case false { B } default { C } }` marks the default branch unreachable with `CONSTANT_BRANCH` and causeNode set to the switch selector.
- `switch (1) { case 2 { A } }` marks the non-matching integer case unreachable with `CONSTANT_BRANCH` and causeNode set to the switch selector.
- `switch (1) { case 1 { A } default { B } }` marks the default branch unreachable because the constant selector is consumed by the matching integer case.
- `switch (1) { case 1 { return; } } A` marks the statement after switch unreachable because the known matching case terminates and there is no default/no-match path.
- `switch ("red") { case "blue" { A } }` marks the non-matching string case unreachable with `CONSTANT_BRANCH` and causeNode set to the switch selector.
- `switch ("red") { case "red" { A } default { B } }` marks the default branch unreachable because the constant selector is consumed by the matching string case.
- `switch ('a') { case 'b' { A } }` marks the non-matching char case unreachable with `CONSTANT_BRANCH` and causeNode set to the switch selector.
- `switch ('a') { case 'a' { A } default { B } }` marks the default branch unreachable because the constant selector is consumed by the matching char case.
- `switch (1.5) { case 2.5 { A } }` marks the non-matching float case unreachable with `CONSTANT_BRANCH` and causeNode set to the switch selector.
- `switch (1.5) { case 1.5 { A } default { B } }` marks the default branch unreachable because the constant selector is consumed by the matching float case.

`tests/parser/test_cfg_finally_abrupt.c` covers:
- `try { return; } finally { A }` keeps the finally-body statement reachable and emits no reachability fact for `A`.
- `try { return; } finally { A } B` keeps `B` unreachable when the only path into `finally` is abrupt return completion.
- `while (true) { try { break; } finally { A } }` keeps the finally-body statement reachable from loop-control abrupt completion.
- `while (true) { try { continue; } finally { A } }` keeps the finally-body statement reachable from loop-control abrupt completion.

`tests/parser/test_cfg_constant_conditions.c` covers:
- `if (!false) { A } else { B }` keeps the then branch reachable and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `while (!true) { A }` marks the body unreachable with `CONDITION_FALSE`.
- `if (true && false) { A } else { B }` marks the then branch unreachable with `CONSTANT_BRANCH`.
- `while (false || false) { A }` marks the body unreachable with `CONDITION_FALSE`.
- `if (false && flag) { A } else { B }` marks the then branch unreachable without requiring the unknown right operand to fold.
- `if (true || flag) { A } else { B }` marks the else branch unreachable without requiring the unknown right operand to fold.
- `if (1 == 2) { A } else { B }` marks the then branch unreachable with `CONSTANT_BRANCH`.
- `while (1 < 0) { A }` marks the body unreachable with `CONDITION_FALSE`.
- `if ("red" == "blue") { A } else { B }` marks the then branch unreachable with `CONSTANT_BRANCH`.
- `while ('a' != 'a') { A }` marks the body unreachable with `CONDITION_FALSE`.
- `if (1.5 >= 2.5) { A } else { B }` marks the then branch unreachable with `CONSTANT_BRANCH`.

`tests/parser/test_cfg_switch_constants.c` covers:
- `switch(!false) { case true { A } default { B } }` marks default unreachable after the folded selector matches the case.
- `switch(true) { case !false { A } default { B } }` marks default unreachable after the folded case value matches the selector.
- `switch(false && flag) { case true { A } default { B } }` marks the non-matching true case unreachable while keeping default reachable.

The targets are registered as `zr_vm_cfg_reachability_test`, `zr_vm_cfg_constant_conditions_test`, `zr_vm_cfg_switch_constants_test`, and `zr_vm_cfg_finally_abrupt_test`, and all four are included in the `language_pipeline` executable list.

## Limits And Next Steps

This scaffold is not the final CFG model. Switch-local `break` is not implemented because the current AST carries only `isBreak`/`expr`, the compiler resolves `break`/`continue` through `loopLabelStack`, and switch compilation does not push a switch break label. That can become a CFG task only after the language/compiler semantics change. The remaining CFG work includes iterator/item binding state, precise catch exception matching/filter edges, `break`/`continue` target rewriting after `finally`, broader constant-condition folding beyond literal/unary/logical short-circuit/comparison expressions, union switch/default matching, dynamic successor storage for high-fanout control flow, concrete dataflow analyses, and compiler/LSP diagnostic consumption. `while(true)` and `for(;;)` are not yet treated as non-fallthrough loops unless later slices prove no break path exists.
