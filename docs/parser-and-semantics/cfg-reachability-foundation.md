---
related_code:
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_constants.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_catch.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_switch_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_profile.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - tests/parser/test_cfg_reachability.c
  - tests/parser/test_cfg_constant_conditions.c
  - tests/parser/test_cfg_integer_bitwise_conditions.c
  - tests/parser/test_cfg_float_arithmetic_conditions.c
  - tests/parser/test_cfg_switch_constants.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_cfg_try_catch_edges.c
  - tests/parser/test_cfg_throw_effects.c
  - tests/parser/test_cfg_typed_catch_flow.c
  - tests/parser/test_cfg_typed_catch_branch_flow.c
  - tests/parser/test_cfg_typed_catch_loop_flow.c
  - tests/parser/test_cfg_typed_catch_switch_flow.c
  - tests/parser/test_semantic_query.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_constants.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_catch.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_switch_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_profile.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
plan_sources:
  - user: 2026-06-20 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_cfg_reachability.c
  - tests/parser/test_cfg_constant_conditions.c
  - tests/parser/test_cfg_integer_bitwise_conditions.c
  - tests/parser/test_cfg_float_arithmetic_conditions.c
  - tests/parser/test_cfg_switch_constants.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_cfg_try_catch_edges.c
  - tests/parser/test_cfg_throw_effects.c
  - tests/parser/test_cfg_typed_catch_flow.c
  - tests/parser/test_cfg_typed_catch_branch_flow.c
  - tests/parser/test_cfg_typed_catch_loop_flow.c
  - tests/parser/test_cfg_typed_catch_switch_flow.c
  - tests/parser/test_semantic_query.c
  - tests/acceptance/2026-06-20-semantic-stage1-cfg.md
doc_type: module-detail
---

# CFG Reachability Foundation

## Purpose

The parser now has the first Stage 1 CFG producer for semantic reachability facts. This is the bottom layer for later dataflow, definite assignment, ownership flow, numeric ranges, and the shared semantic query API.

## Current Scope

`SZrParserCfg` builds an entry block, statement blocks for analyzed statements, join blocks for supported branch and loop merge points, and an exit block. The current slice supports `ZR_AST_SCRIPT`, `ZR_AST_BLOCK`, function bodies, test bodies, straight-line statement lists, `ZR_AST_IF_EXPRESSION` nodes used as statements, `ZR_AST_SWITCH_EXPRESSION` nodes used as statements, `ZR_AST_WHILE_LOOP` nodes used as statements, `ZR_AST_FOR_LOOP` statement graphs, the first `ZR_AST_FOREACH_LOOP` statement graph, and `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` try/catch/finally body graphs with throw/call-gated catch reachability, catch-all ordering, known literal/cast/local-declaration/straight-line-assignment/full-branch/partial-branch/simple multi-variable branch, constant-if branch, constant-if selected-branch terminator sibling pruning, and simple unknown-condition `while`/traditional `for` body, simple `foreach` body, simple switch case/default body, matching or mismatched-kind known constant switch selector/case body, switch case-body fallthrough precision including folded-if selected branch terminators for simple assignments, plus traditional `for` init/step assignment throw kind sets typed catch matching, straight-line terminator-aware result-binding pruning, and separate normal, function-exit, `break`, and `continue` paths through `finally`.

The latest loop-binding slice also preserves simple conditional `continue` path assignments for post-loop typed-catch matching: a value assigned before `continue` can still reach code after the loop if the next loop condition check exits.

The newest loop-control branch-binding slice prunes unreachable folded `if` branches while collecting `break`/`continue` transfer bindings, so an unselected constant branch cannot contribute stale throw kinds to post-loop typed-catch matching.

The newest switch fallthrough slice prunes unreachable folded `if` branches while deciding whether a switch case/default body can feed post-switch typed-catch matching. If the folded selected branch terminates with `return`, `throw`, `break`, or `continue`, assignments in that selected branch do not leak into the later `throw value`; unselected branch assignments are ignored.

The latest direct lambda IIFE throw-effect slice recognizes a primary lambda expression immediately called by a single function-call member. Its catch-entry scan and known throw-kind profile come from the call arguments, generic arguments, and lambda body instead of treating that direct call as an unknown throwing function call. This makes `((value: int) -> { return value + 1; })(1)` prove the following catch body unreachable, while `(() -> { throw "boom"; })()` feeds a known `string` throw kind into typed-catch ordering.

The latest mixed-kind condition-folding slice reuses the private constant extractor and equality helper from switch matching in the shared boolean condition folder. Known constants with different kinds are deterministic non-equal for `==`/`!=`, so `if (1 == "1")` prunes the then branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH` while keeping the else branch reachable.

The integer-addition condition-folding slice extends that private constant extractor to fold known integer-literal `+` expressions into integer constants before comparison. `if ((1 + 1) == 2)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Addition is checked for `int64` overflow; overflow remains unknown rather than forced.

The integer-subtraction condition-folding slice extends the same constant extractor to fold known integer-literal `-` expressions into integer constants before comparison. `if ((3 - 1) == 2)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Subtraction is checked for `int64` overflow; overflow remains unknown rather than forced.

The integer-multiplication condition-folding slice extends the extractor again to fold known integer-literal `*` expressions into integer constants before comparison. `if ((2 * 3) == 6)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Multiplication is checked for `int64` overflow; overflow remains unknown rather than forced.

The integer-division/modulo condition-folding slice extends the extractor to fold known integer-literal `/` and `%` expressions into integer constants before comparison. `if ((9 / 3) == 3)` and `if ((10 % 4) == 2)` now keep the then branch reachable and prune the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Division and modulo by zero, plus the signed `INT64_MIN / -1` or `INT64_MIN % -1` overflow/undefined-behavior boundary, remain unknown rather than forced.

The float-addition condition-folding slice extends the extractor to fold known finite float-literal `+` expressions into float constants before comparison. `if ((1.5 + 2.25) == 3.75)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Non-finite operands or non-finite results, including `DBL_MAX + DBL_MAX`, remain unknown rather than forced.

The float-subtraction condition-folding slice extends the same finite arithmetic path to known float-literal `-` expressions. `if ((5.5 - 2.25) == 3.25)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Non-finite operands or non-finite results, including `-DBL_MAX - DBL_MAX`, remain unknown rather than forced.

The float-multiplication condition-folding slice extends the same finite arithmetic path to known float-literal `*` expressions. `if ((1.5 * 2.5) == 3.75)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Non-finite operands or non-finite results, including `DBL_MAX * 2.0`, remain unknown rather than forced.

The float-division condition-folding slice extends the same finite arithmetic path to known float-literal `/` expressions. `if ((7.5 / 2.5) == 3.0)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Zero divisors and non-finite results, including `7.5 / 0.0` and `DBL_MAX / 0.5`, remain unknown rather than forced. NaN/Infinity folding, numeric ranges, symbolic arithmetic, mixed-type arithmetic, and implicit conversions are still out of scope.

The unary-minus condition-folding slice extends the extractor to fold known integer-literal and finite float-literal unary `-` expressions before comparison. `if (-3 == -3)` and `if (-1.5 == -1.5)` now keep the then branch reachable and prune the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. The checked integer boundary `-INT64_MIN` remains unknown rather than forced.

The unary-plus condition-folding slice extends the extractor to preserve known integer-literal and finite float-literal unary `+` expressions before comparison. `if (+3 == 3)` and `if (+1.5 == 1.5)` now keep the then branch reachable and prune the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Bitwise unary `~`, implicit conversions, ranges, symbolic arithmetic, and mixed-type arithmetic remain out of scope.

The integer bitwise-not condition-folding slice extends the extractor to fold known integer-literal unary `~` expressions before comparison. `if (~0 == -1)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. The implementation models `~x` as `-1 - x` in the int64 constant domain and uses the checked integer helper instead of relying on C signed bit representation. Float bitwise operations, implicit conversions, ranges, symbolic arithmetic, and mixed-type arithmetic remain out of scope.

The nonnegative integer bitwise-AND condition-folding slice extends the extractor to fold known nonnegative integer-literal binary `&` expressions before comparison. `if ((6 & 3) == 2)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Negative operands, `^`, shifts, implicit conversions, ranges, symbolic arithmetic, and mixed-type arithmetic remain out of scope.

The nonnegative integer bitwise-OR condition-folding slice extends the same bitwise path to known nonnegative integer-literal binary `|` expressions before comparison. `if ((4 | 1) == 5)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Negative operands, shifts, implicit conversions, ranges, symbolic arithmetic, and mixed-type arithmetic remain out of scope.

The nonnegative integer bitwise-XOR condition-folding slice extends the same bitwise path to known nonnegative integer-literal binary `^` expressions before comparison. `if ((6 ^ 3) == 5)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Negative operands, shifts, implicit conversions, ranges, symbolic arithmetic, and mixed-type arithmetic remain out of scope.

The nonnegative integer left-shift condition-folding slice extends the same integer constant path to known nonnegative integer-literal binary `<<` expressions before comparison. `if ((3 << 2) == 12)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Negative operands, negative or too-large shift counts, overflowed results, implicit conversions, ranges, symbolic arithmetic, and mixed-type arithmetic remain out of scope.

The nonnegative integer right-shift condition-folding slice extends the same integer constant path to known nonnegative integer-literal binary `>>` expressions before comparison. `if ((12 >> 2) == 3)` now keeps the then branch reachable and prunes the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Negative operands, negative or too-large shift counts, implicit conversions, ranges, symbolic arithmetic, and mixed-type arithmetic remain out of scope.

The folded-constant relational slice lets `cfg.c` compare constants after extraction instead of only comparing literal AST nodes. `if ((1 + 2) > 2)` and `if ((1.5 + 2.25) > 3.0)` now reuse the existing arithmetic folders, compare the resulting same-kind integer/float constants, and prune the else branch with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. Same-kind char relational comparisons share this path; mixed-kind relational comparisons, string/bool relational operators, ranges, symbolic arithmetic, and implicit conversions remain unknown.

The CFG implementation is split across small construction files. `cfg.c` owns shared block allocation, straight-line statement construction, top-level build/reset, reachability fact emission, and folded constant boolean comparison for condition pruning. `cfg_constants.c` owns private constant extraction/comparison shared by switch CFG construction and typed-catch switch binding; known literal constants with different kinds are comparable and deterministically unequal, known integer `+`/`-`/`*`/`/`/`%` expressions plus integer unary `+`/`-`/`~` and nonnegative integer binary `&`/`|`/`^`/`<<`/`>>` can produce integer constants when arithmetic stays in int64 range and avoids zero-divisor, signed division/modulo/unary-minus overflow boundaries, invalid shift counts, or overflowed left-shift results, with unary `~` modeled as `-1 - x`, and known finite float `+`/`-`/`*`/`/` expressions plus float unary `+`/`-` can produce float constants when the result remains finite and division avoids a zero divisor. `cfg_loops.c` owns `while`/`for`/`foreach` graph construction and loop-control target wiring. `cfg_control_flow.c` owns larger switch and try/catch/finally graph construction plus catch dispatch chaining. `cfg_throw_profile.c` owns catch throwability scanning, throw kind profile collection, and literal/cast/identifier throw-kind extraction. `cfg_throw_bindings.c` owns local declaration, straight-line assignment, full-branch assignment, partial-branch assignment, simple multi-variable branch-result, constant-if branch pruning, compound selected-branch terminator pruning for result flow, simple unknown-condition `while`/traditional `for` body result type bindings, simple `foreach` body result type bindings, traditional `for` init/step result type bindings, and terminator-aware sequential result collection used by throw expressions. `cfg_throw_switch_bindings.c` owns switch case/default result type binding merges and fallthrough pruning for terminating case/default bodies, including folded-if selected branch terminators. `cfg_throw_types.c` owns builtin throw kind masks, simple type-name mapping, and private string comparison helpers. `cfg_catch.c` owns catch-all recognition and catch clause matching against known throw kind sets. `cfg_internal.h` is private to the type-inference implementation and exposes only the helpers needed across that split.

Within `cfg_throw_bindings.c`, loop-control result collection now records both `break` exits and `continue` transfers. `break` transfers contribute directly to the post-loop binding set; `continue` transfers contribute as completed-iteration state, and traditional `for` loops apply the step binding to those transfers before merging with the incoming zero-iteration state.

Function-exiting terminators (`return` and `throw`) are connected to the exit block. They still do not fall through to the next source statement, so following statements remain entry-unreachable, but backward dataflow can still start from exit and reach real function exits.

Terminator statements stop fallthrough:
- `ZR_AST_RETURN_STATEMENT` emits `ZR_SEMANTIC_REACHABILITY_AFTER_RETURN`.
- `ZR_AST_THROW_STATEMENT` emits `ZR_SEMANTIC_REACHABILITY_AFTER_THROW`.
- `ZR_AST_BREAK_CONTINUE_STATEMENT` emits `AFTER_BREAK` or `AFTER_CONTINUE` from the AST payload.

Loop-control terminators now also carry basic target edges when they appear inside supported loop bodies. `break` connects to the active loop join block. `continue` connects to the active loop header for `while` and `foreach`; for traditional `for`, `continue` connects to the step-entry join when a step exists, otherwise to the loop condition block.

Boolean condition folding is shared between `if`, `while`, and traditional `for` graph construction. It currently recognizes boolean literals, unary `!` over a folded boolean expression, logical `&&` / `||` when both operands fold to boolean constants, short-circuit decisive logical operands such as `false && unknown` or `true || unknown`, integer/char/float literal binary comparisons for `==`, `!=`, `<`, `>`, `<=`, and `>=`, string literal equality/inequality, and known constant `==`/`!=` comparisons across different literal kinds as deterministically unequal/equal-negated. The folded expression node remains the `causeNode`, so diagnostics and hovers can point at the source condition rather than only the inner literal.

Known integer addition, subtraction, multiplication, division, modulo, unary plus, unary minus, bitwise-not, and nonnegative bitwise/shift expressions can now feed those comparisons when operands are already known integer constants. Known finite float addition, subtraction, multiplication, division, unary plus, and unary minus expressions can also feed comparisons when operands are already known finite float constants, the result stays finite, and division avoids a zero divisor.

For `if` statements, the CFG uses the `ZR_AST_IF_EXPRESSION` node as the condition statement block. The then and else bodies are built as separate subgraphs and reconnect through a join block when their final block can fall through. A known boolean condition prunes the impossible branch; unreachable statements in that pruned branch use `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH` with `causeNode` set to the condition expression. Typed-catch result binding uses the same folded condition helper: when the condition is known, only the selected branch contributes assignment bindings to a later `throw value`. Sequential typed-catch result flow also treats a folded selected branch that terminates with `return`, `throw`, `break`, or `continue` as a terminator for the containing statement list, so sibling assignments after `if (true) { ... break; }` do not pollute later post-loop `throw value` matching.

For `switch` statements, the CFG uses the `ZR_AST_SWITCH_EXPRESSION` node as the selector statement block and builds switch case bodies as subgraphs. This first slice treats cases as reachable candidate bodies so terminators inside a case can mark following case statements unreachable. When the selector and case values are known boolean, integer, string, char, or float constants, non-matching cases are pruned and receive `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH` with the selector expression as the cause. Known constants with different literal kinds are deterministic nonmatches, so a mixed-kind case is pruned while a following default remains reachable unless another matching case consumes the selector. Boolean switch constants reuse the shared condition folder, so selectors and case values can be boolean literals or folded unary/logical boolean expressions. A matching constant case consumes the constant selector, so a following default branch is pruned with the same cause. If that matching case terminates and there is no default, the switch no longer keeps a synthetic no-match fallthrough edge to later statements. String literals compare with `ZrCore_String_Equal`; float literals compare the parsed `TZrDouble` value exactly; string or char literals marked with parse errors are not treated as constants. Simple switch case/default body assignments can also feed post-switch typed-catch matching for unknown selectors with a default branch; when every modeled switch alternative assigns the same thrown identifier, the merged switch result replaces the stale incoming binding before a later `throw value`. For a known constant selector and matching known constant case, typed-catch switch binding uses only the matched case body and ignores unreachable mixed-kind cases/default bindings. Case/default body assignments only contribute to post-switch typed-catch matching when that body can fall through; assignments before `return`/`throw`/`break`/`continue` in a terminating case do not affect a later `throw value`. This fallthrough proof also follows folded `if` conditions inside a case/default body: only the selected branch decides fallthrough, and unselected branch assignments are ignored. The CFG does not yet do implicit conversion or union exhaustiveness.

For `while` statements, the CFG uses the `ZR_AST_WHILE_LOOP` node as the condition statement block. The loop body reconnects to the condition block through a back edge when it can fall through, `continue` targets the condition block, and `break` targets the loop join. A known false boolean condition prunes the body; unreachable statements in that body use `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE` with `causeNode` set to the condition expression. A known true condition does not get a synthetic zero-iteration edge from the loop header to the join; later statements after `while(true)` become reachable only through real loop-exit edges such as `break`.

Typed-catch loop binding mirrors the CFG loop-control model for simple value flows. For an unknown-condition `while`, a conditional `continue` branch such as `if (done) { value = "x"; continue; }` preserves the `string` binding as a possible post-loop value because the next condition check can exit the loop before another write occurs.

For traditional `for` statements, the CFG builds the optional initializer before the loop condition block, uses the `ZR_AST_FOR_LOOP` node as the loop condition block, builds the loop body, builds the optional step expression after a fallthrough body, and reconnects the step or body back to the condition block. `break` targets the loop join. `continue` targets the step-entry join when a step expression exists, so the step still executes before returning to the condition block; otherwise it targets the condition block directly. A known false boolean condition prunes the body just like `while(false)`; unreachable statements in that body use `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE` with `causeNode` set to the `for` condition expression. Unknown conditions still get a loop-header-to-join edge for the zero-iteration exit. Known true conditions and omitted conditions, such as `for(;;)`, do not get that synthetic exit and must leave through real loop-control edges.

For `foreach` statements, the CFG uses the `ZR_AST_FOREACH_LOOP` node as the loop entry/iteration statement block, builds the loop body, reconnects a fallthrough body back to the foreach block, and adds a join block for zero iterations or loop exit. `break` targets the join block and `continue` targets the foreach iteration block. Simple body assignments now contribute to post-loop typed-catch matching while still preserving the incoming binding for the zero-iteration path. This slice still does not model iterator failure or item binding state.

For `try` statements, the CFG uses the `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` node as the try statement block and builds the try body, each catch body, and the finally body as subgraphs. Catch body reachability is gated by whether the protected try body may enter catch: explicit `throw`, function calls, construct expressions, and import expressions keep the catch path reachable, while a try body with none of those still builds catch blocks for fact emission but gives them no entry-reachable predecessor. The scanner also recursively checks common expression and statement children so a potentially throwing expression nested under an expression statement, condition, switch case, loop body, object/array literal, or similar structure keeps the catch path conservative.

A reachable catch-all clause, represented by no pattern, an empty pattern list, or a single untyped `ZR_AST_PARAMETER` such as `catch (e)`, consumes the exception path for later catches; later catch bodies are still built for fact emission but are marked unreachable with `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`. When the protected try body contains only known throw kinds and no unknown potentially throwing source, simple typed catch parameters are matched against that known kind set in source order. Literal throw expressions contribute their literal kind; explicit `ZR_AST_TYPE_CAST_EXPRESSION` throw expressions contribute the target type's simple name when it maps to a builtin catch kind; local declarations, straight-line assignments, full-branch assignments, partial-branch assignments, multi-variable branch-result bindings, constant-if selected branch assignments, constant-if selected-branch terminator sibling pruning, and switch case/default body result bindings can feed later `throw value` expressions. Unknown-condition `while`, traditional `for`, and `foreach` bodies can merge a simple final body assignment with the incoming known binding so the zero-iteration path remains represented. For traditional `for` loops, a simple init assignment is collected before the condition and becomes the incoming binding for condition/body/step handling; a simple step assignment is collected after the body assignment for a completed iteration, so the step binding can replace the body binding before the result is merged with the incoming zero-iteration path. Sequential result-binding collection stops after straight-line `return`, `throw`, `break`, or `continue`, and after compound folded-if statements whose selected branch terminates, so assignments after those proven terminators do not feed later typed-catch matching. Non-simple assignments or unknown right-hand sides shadow older bindings with unknown instead of preserving a stale declaration type.

Loop-control path collection additionally feeds typed-catch matching from simple conditional `break` and `continue` transfers. `break` assignments before the transfer can reach the post-loop `throw` directly. `continue` assignments before the transfer can reach post-loop code after a later condition check exits; for traditional `for`, the step expression is applied to that continue-transfer binding before it is merged.

When loop-control transfer collection sees a folded `if`, it follows only the selected branch before collecting those `break`/`continue` transfer bindings. For example, `if (true) { value = "x"; break; } else { value = 'c'; break; }` contributes the string break path but not the unreachable char break path.

Casts, initializers, or assignment right-hand sides that wrap a potentially throwing inner expression remain conservative because the inner expression is scanned before the known kind is recorded. Catch dispatch join blocks chain these alternatives so the try block stays within the fixed two-successor CFG block layout even when multiple typed catches are simultaneously reachable. `return`, `throw`, `break`, and `continue` terminators inside the protected try/catch range now enter `finally`, so reachability propagation sees the `finally` body even when the try/catch path exits abruptly. Normal completion, function-exit completion, `break`, and `continue` use separate finally entry blocks and separate finally body subgraphs. That keeps `try { return; } finally { A } B` from making `B` reachable through return unwinding, keeps `break` and `continue` from bypassing `finally`, and avoids conflating a `break` finally path with a `continue` finally path when both share the same source `finally` statement. The model still does not use function-effect metadata to prove calls cannot throw, and broader typed catch matching beyond known literal/cast/local-declaration/straight-line-assignment/simple full-branch/partial-branch/multi-variable branch/simple unknown-loop init/body/step assignment kind sets remains pending.

The same simple result-binding merge now covers explicit unknown-condition traditional `for` loops, `foreach` loops, and switch case/default bodies. `for (value = "x"; flag; ) { flag; }` applies the init binding before the condition, so later `throw value` sees the init's string binding on every post-loop path that this slice models. `for (; flag; ) { value = "x"; }` can still contribute the body assignment while preserving the incoming binding for the zero-iteration path. For normal completed iterations with a simple step assignment, `for (; flag; value = 'c') { value = "x"; }` contributes the step's `char` binding after the body while still preserving the incoming `int` binding for zero iterations. `foreach (...) { value = "x"; }` likewise contributes the body assignment while preserving the incoming binding for zero iterations. `switch (tag) { case 1 { value = "x"; } default { value = 'c'; } }` contributes string/char case/default bindings and replaces the stale pre-switch binding for later `throw value`; `switch (1) { case 1 { value = "x"; } default { value = 'c'; } }` contributes only the matching case's string binding. `switch (tag) { case 1 { value = "x"; return; } default { value = 'c'; } }` contributes only the fallthrough default's char binding to a later `throw value`. `while (flag) { break; value = "x"; }` now ignores the assignment after `break` when computing post-loop typed-catch bindings. `while (flag) { if (true) { value = "x"; break; } value = 'c'; }` now treats the folded true branch's `break` as stopping the containing loop-body sequence, so the sibling char assignment is ignored while the incoming int and break-exit string bindings remain represented. Omitted-condition `for(;;)`, constant-true `for`, complex init/step expressions, iterator/item binding state, complex `foreach` bodies, complex switch selector edge cases, symbolic conditional break/continue-sensitive exits, and broader loop fixed points remain pending.

`while (flag) { if (done) { value = "x"; continue; } value = 'c'; }` now preserves the incoming `int`, continue-transfer `string`, and normal body `char` possibilities for a later `throw value`. This is still a bounded simple-binding model rather than a complete symbolic loop fixed point.

## Semantic Query Diagnostics

`ZrParser_SemanticQuery_Diagnostics` now consumes the reachability facts produced by CFG and semantic analyzers. For every scope-allowed `ZR_SEMANTIC_REACHABILITY_UNREACHABLE` fact, it builds a structured warning diagnostic with code `unreachable_code`. The diagnostic location comes from the unreachable statement fact range; the cause and suggestion text are derived from the reachability cause such as `AFTER_RETURN`, `AFTER_THROW`, `AFTER_BREAK`, `AFTER_CONTINUE`, `CONDITION_FALSE`, or `CONSTANT_BRANCH`.

The diagnostics returned by the query are stored in the semantic context's `queryDiagnostics` cache. Callers receive a borrowed slice that remains valid until the same context is queried, reset, or freed again. This is a parser semantic-query bridge; LSP and compiler frontend serialization/publishing of these diagnostics remains a later integration step.

## Data Flow

`ZrParser_Cfg_Build` creates linear statement blocks and only links a statement block to its successor when the previous statement is not a terminator. It records the upstream terminator as `unreachableCauseNode` for later unreachable blocks.

CFG construction treats block ids as stable references and treats `SZrParserCfgBlock *` values as short-lived borrows only. `cfg_add_block` can grow the backing `cfg->blocks` array, so builders must not keep a block pointer across another allocation and then inspect fields such as `isTerminator`. Fallthrough wiring should pass the previous and new block ids through `cfg_connect_fallthrough`, which re-fetches the previous block after any possible reallocation. This applies to straight-line statements as well as loop, switch, and try/finally builders.

When building an `if` statement, the condition block gets one or two outgoing branch edges depending on whether the condition is known. Unknown conditions connect to both branch bodies. A known true condition omits the else edge; a known false condition omits the then edge. The omitted branch still gets blocks so facts can be emitted, but those blocks have no path from entry.

When building a `switch` statement, the selector block starts a small case-chain graph. Each `ZR_AST_SWITCH_CASE` gets its own statement block, its block body is built as a subgraph, and a fallthrough case body connects to the switch join block. The selector/case chain also falls through to the next case candidate, which keeps fanout within the current fixed successor limit while still visiting case bodies for reachability facts. A comparable boolean, integer, string, char, or float selector omits the edge into a non-matching comparable case while still building the case block and body for fact emission. A matching comparable case consumes the selector; after that point, later cases and default are not connected as possible no-match paths, but are still built for fact emission.

When building a `while` statement, the condition block connects to the body unless the condition folds to a known false boolean value. The condition block connects to the join block only when the condition is unknown or folds false; known true loops omit this zero-iteration edge. A body that falls through connects back to the condition block, `break` connects to the join block, and `continue` connects to the condition block.

When building a traditional `for` statement, the optional initializer is part of the predecessor flow, then the condition block decides whether the loop body is entry-reachable. Unknown or known true conditions connect to the body; a known false condition omits the body edge while still building body blocks for fact emission. The loop condition block connects to the join block for unknown or false conditions, but not for known true conditions or omitted conditions. A body that can fall through goes through the optional step expression when present, then back to the loop condition block. `break` connects to the join block. `continue` enters the step subgraph when present, or the condition block when no step exists.

When building a `foreach` statement, the foreach block connects to both the body and the join block. The body reconnects to the foreach block only when its final block can fall through. `break` connects to the join block, and `continue` connects to the foreach block. A `break` or `continue` terminator inside the body also blocks fallthrough to following body statements and records the terminator as the pending unreachable cause for those following statements.

When building a `try` statement, the try statement block connects to the try body and, when the protected try body may enter catch, to a catch dispatch join. The dispatch chain then connects to the current catch body and to the next dispatch join when unconsumed exception kinds remain. A fallthrough try body or reachable catch body connects to a normal-completion join block. A catch body that is filtered out by the no-throwing-path check, an earlier consuming catch, or a known throw kind mismatch is still built with no valid predecessor so `ZrParser_Cfg_EmitReachabilityFacts` can report its statements as unreachable; catch ordering and typed mismatch pruning use `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH` as the cause. Catch-all detection includes no pattern, an empty pattern list, and a single untyped catch parameter. Typed parameter matching is only applied when the protected body has a known throw kind set from literals, explicit cast target types, sequential local declaration type bindings, straight-line assignments, or simple same-name branch assignment merges and no unknown catch-entry source; otherwise typed catches remain conservative and non-consuming. If a finally body exists, the builder records whether that normal join had predecessors, allocates distinct finally entries for function-exit, `break`, and `continue` completions, then scans the protected try/catch block range for terminators and connects each terminator to the entry for its completion kind. The finally body is built separately for every present completion kind. The normal finally clone falls through to the final join, the function-exit clone has no source fallthrough target, the `break` clone falls through to the original loop join, and the `continue` clone falls through to the original loop header or step target. Because those paths are separate, one source `finally` statement can correspond to multiple CFG statement blocks; this is intentional when a shared `finally` body is reached by multiple completion kinds.

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
- `for (;;) { break; }` connects the `break` terminator block to the for join block without adding a synthetic zero-iteration edge from the for header to the join.
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
- `while (true) { try { break; } finally { A } }` routes the break edge through the finally body before reaching the loop join.
- `while (true) { try { continue; } finally { A } }` routes the continue edge through the finally body before reaching the loop header.
- `while (true) { try { if (x) break; if (y) continue; A } finally { F } B }` keeps both `F` and `B` reachable and verifies that the shared source `finally` statement is represented by separate normal, break, and continue CFG blocks rather than one block with both loop-exit and loop-header successors.

`tests/parser/test_cfg_try_catch_edges.c` covers:
- `try { A } catch { B }` marks the catch-body statement unreachable when the protected try body has no explicit `throw`.
- `try { throw; } catch { B }` keeps the catch-body statement reachable when the protected try body has an explicit `throw`.
- `try { call(); } catch { B }` keeps the catch-body statement reachable because calls are conservatively treated as potentially throwing.
- `try { throw; } catch { A } catch { B }` marks the second catch body unreachable because the first catch-all consumes the exception path.
- `try { throw; } catch (e) { A } catch { B }` treats the untyped catch parameter as catch-all and marks the second catch body unreachable.
- `try { throw 1; } catch (e: string) { A } catch { B }` marks the typed catch body unreachable and keeps the fallback catch reachable.
- `try { throw 1; } catch (e: int) { A } catch { B }` keeps the matching typed catch reachable and marks the fallback catch unreachable.
- `try { if (flag) { throw 1; } else { throw "x"; } } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch { D }` marks the bool catch and final catch-all unreachable while keeping the int and string typed catches reachable.
- `try { throw (value as string); } catch (e: int) { A } catch (e: string) { B } catch { C }` uses the explicit cast target type to prune the int catch, keep the string catch reachable, and prune the catch-all after the string kind is consumed.
- `try { var value: string = "x"; throw value; } catch (e: int) { A } catch (e: string) { B } catch { C }` uses the sequential local declaration type binding to prune the int catch, keep the string catch reachable, and prune the catch-all after the string kind is consumed.
- `try { var value: int = 1; value = "x"; throw value; } catch (e: int) { A } catch (e: string) { B } catch { C }` uses the latest straight-line assignment type binding to prune the stale int catch, keep the string catch reachable, and prune the catch-all after the string kind is consumed.

`tests/parser/test_cfg_throw_effects.c` covers:
- `try { ((value: int) -> { return value + 1; })(1); } catch (e) { A }` treats a directly visible nonthrowing lambda IIFE as nonthrowing and marks the catch body unreachable.
- `try { (() -> { throw "boom"; })(); } catch (e: int) { A } catch (e: string) { B } catch { C }` collects the lambda IIFE body's known string throw kind, prunes the mismatched int catch, keeps the string catch reachable, and prunes the catch-all after the string kind is consumed.

`tests/parser/test_cfg_typed_catch_flow.c` covers:
- `try { var value: int = 1; if (flag) { value = "x"; } else { value = 'c'; } throw value; } catch (e: int) { A } catch (e: string) { B } catch (e: char) { C } catch { D }` merges same-identifier branch assignments into a string/char throw kind set, prunes the stale int catch, keeps string and char catches reachable, and prunes catch-all after both known kinds are consumed.
- `try { var value: int = 1; if (flag) { value = "x"; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch { D }` merges the one-sided branch assignment with the incoming int binding, prunes the bool catch, keeps int and string catches reachable, and prunes catch-all after both known kinds are consumed.
- `try { var first: int = 1; var second: int = 2; if (flag) { first = "x"; second = "y"; } else { first = 'c'; second = 3; } throw first; } catch (e: int) { A } catch (e: string) { B } catch (e: char) { C } catch { D }` carries multiple branch-result bindings, uses the merged string/char binding for `first`, prunes the stale int catch, keeps string and char catches reachable, and prunes catch-all after both known kinds are consumed.
- `try { var value: int = 1; while (flag) { value = "x"; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch { D }` merges the unknown-condition while body assignment with the incoming int binding, prunes the bool catch, keeps int and string catches reachable, and prunes catch-all after both known kinds are consumed.
- `try { var value: int = 1; for (; flag; ) { value = "x"; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch { D }` merges the unknown-condition traditional `for` body assignment with the incoming int binding, prunes the bool catch, keeps int and string catches reachable, and prunes catch-all after both known kinds are consumed.
- `try { var value: int = 1; for (value = "x"; flag; ) { flag; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch { D }` applies the traditional `for` init assignment before the condition, prunes the stale int catch, keeps the string catch reachable, and prunes catch-all after the known kind is consumed.
- `try { var value: int = 1; for (; flag; value = 'c') { value = "x"; } throw value; } catch (e: int) { A } catch (e: string) { B } catch (e: char) { C } catch { D }` treats the step assignment as the completed-iteration final binding, keeps int/char catches reachable, prunes the body-only string catch, and prunes catch-all after both known kinds are consumed.
- `try { var value: int = 1; while (flag) { break; value = "x"; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch { D }` stops result-binding collection after the loop-body `break`, ignores the unreachable string assignment, keeps the incoming int catch reachable, and prunes catch-all after that kind is consumed.

`tests/parser/test_cfg_typed_catch_branch_flow.c` covers:
- `try { var value: int = 1; if (true) { value = "x"; } else { value = 'c'; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` selects only the folded true branch for typed-catch result binding, keeps string reachable, and prunes bool/int/char/catch-all.
- `try { var value: int = 1; if (flag) { value = "x"; return; } else { value = 'c'; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` ignores the terminating symbolic then-branch assignment for post-branch typed-catch result binding, keeps only char reachable, and prunes bool/int/string/catch-all.

`tests/parser/test_cfg_typed_catch_loop_flow.c` covers:
- `try { var value: int = 1; while (flag) { if (done) { value = "x"; break; } value = 'c'; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` preserves loop-exit assignments before a conditional `break`, keeps int/string/char catches reachable, and prunes bool/catch-all.
- `try { var value: int = 1; while (flag) { value = "x"; if (done) { break; } value = 'c'; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` preserves the already-active prefix assignment when a nested conditional `break` exits the loop, keeps int/string/char catches reachable, and prunes bool/catch-all.
- `try { var value: int = 1; while (flag) { if (done) { value = "x"; continue; } value = 'c'; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` preserves assignments before a conditional `continue` as possible post-loop values after a later condition exit, keeps int/string/char catches reachable, and prunes bool/catch-all.
- `try { var value: int = 1; foreach (items) { value = "x"; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch { D }` merges the foreach body assignment with the incoming int binding, prunes bool, keeps int/string catches reachable, and prunes catch-all after both known kinds are consumed.
- `try { var value: int = 1; while (flag) { if (true) { value = "x"; break; } value = 'c'; } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` uses the folded selected branch terminator to stop sibling result collection, keeps int/string catches reachable, and prunes bool/char/catch-all.
- `try { var value: int = 1; while (flag) { if (true) { value = "x"; break; } else { value = 'c'; break; } } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` follows only the folded selected loop-control branch while collecting transfer bindings, keeps int/string catches reachable, and prunes bool/char/catch-all.

`tests/parser/test_cfg_typed_catch_switch_flow.c` covers:
- `try { var value: int = 1; switch (tag) { case 1 { value = "x"; } default { value = 'c'; } } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` merges switch case/default assignments for an unknown selector with a default branch, replaces the stale int binding, keeps string/char catches reachable, and prunes bool/int/catch-all.
- `try { var value: int = 1; switch (1) { case 1 { value = "x"; } default { value = 'c'; } } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` uses only the matching constant case assignment for typed-catch binding, keeps string reachable, and prunes bool/int/char/catch-all.
- `try { var value: int = 1; switch (tag) { case 1 { value = "x"; return; } default { value = 'c'; } } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` ignores the terminating case assignment for post-switch typed-catch binding, keeps char reachable from the default fallthrough path, and prunes bool/int/string/catch-all.
- `try { var value: int = 1; switch (1) { case "x" { value = "stale"; } case 1 { value = "x"; } default { value = 'c'; } } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` treats the mixed-kind string case as a deterministic nonmatch, uses the later matching integer case assignment, and prunes bool/int/char/catch-all.
- `try { var value: int = 1; switch (tag) { case 1 { if (true) { value = "x"; return; } else { value = "stale"; } } default { value = 'c'; } } throw value; } catch (e: bool) { A } catch (e: int) { B } catch (e: string) { C } catch (e: char) { D } catch { E }` follows only the folded selected switch-branch fallthrough path, ignores the terminating selected branch assignment and unselected else assignment, keeps char reachable from default, and prunes bool/int/string/catch-all.

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
- `if (1 == "1") { A } else { B }` treats known mixed-kind constants as unequal, marks the then branch unreachable with `CONSTANT_BRANCH`, and keeps the else branch reachable.
- `if ((1 + 1) == 2) { A } else { B }` folds the known integer addition expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((3 - 1) == 2) { A } else { B }` folds the known integer subtraction expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((2 * 3) == 6) { A } else { B }` folds the known integer multiplication expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((9 / 3) == 3) { A } else { B }` folds the known integer division expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((10 % 4) == 2) { A } else { B }` folds the known integer modulo expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((1 + 2) > 2) { A } else { B }` folds the known integer addition expression first, then applies same-kind relational comparison, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if (+3 == 3) { A } else { B }` folds the known integer unary-plus expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if (-3 == -3) { A } else { B }` folds the known integer unary-minus expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if (~0 == -1) { A } else { B }` folds the known integer bitwise-not expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if (-INT64_MIN == INT64_MIN) { A } else { B }` leaves both branches reachable because the integer unary-minus result would overflow.

`tests/parser/test_cfg_integer_bitwise_conditions.c` covers:
- `if ((6 & 3) == 2) { A } else { B }` folds the known nonnegative integer bitwise-AND expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((4 | 1) == 5) { A } else { B }` folds the known nonnegative integer bitwise-OR expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((6 ^ 3) == 5) { A } else { B }` folds the known nonnegative integer bitwise-XOR expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((3 << 2) == 12) { A } else { B }` folds the known nonnegative integer left-shift expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((12 >> 2) == 3) { A } else { B }` folds the known nonnegative integer right-shift expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.

`tests/parser/test_cfg_float_arithmetic_conditions.c` covers:
- `if ((1.5 + 2.25) == 3.75) { A } else { B }` folds the known finite float addition expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((1.5 + 2.25) > 3.0) { A } else { B }` folds the known finite float addition expression first, then applies same-kind relational comparison, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((DBL_MAX + DBL_MAX) == DBL_MAX) { A } else { B }` leaves both branches reachable because the float addition result would be non-finite.
- `if ((5.5 - 2.25) == 3.25) { A } else { B }` folds the known finite float subtraction expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((-DBL_MAX - DBL_MAX) == -DBL_MAX) { A } else { B }` leaves both branches reachable because the float subtraction result would be non-finite.
- `if ((1.5 * 2.5) == 3.75) { A } else { B }` folds the known finite float multiplication expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((DBL_MAX * 2.0) == DBL_MAX) { A } else { B }` leaves both branches reachable because the float multiplication result would be non-finite.
- `if ((7.5 / 2.5) == 3.0) { A } else { B }` folds the known finite float division expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if ((7.5 / 0.0) == 1.0) { A } else { B }` leaves both branches reachable because the float division has a zero divisor.
- `if ((DBL_MAX / 0.5) == DBL_MAX) { A } else { B }` leaves both branches reachable because the float division result would be non-finite.
- `if (+1.5 == 1.5) { A } else { B }` folds the known finite float unary-plus expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.
- `if (-1.5 == -1.5) { A } else { B }` folds the known finite float unary-minus expression, keeps the then branch reachable, and marks the else branch unreachable with `CONSTANT_BRANCH`.

`tests/parser/test_cfg_switch_constants.c` covers:
- `switch(!false) { case true { A } default { B } }` marks default unreachable after the folded selector matches the case.
- `switch(true) { case !false { A } default { B } }` marks default unreachable after the folded case value matches the selector.
- `switch(false && flag) { case true { A } default { B } }` marks the non-matching true case unreachable while keeping default reachable.
- `switch(1) { case "x" { A } default { B } }` marks the mixed-kind string case unreachable while keeping default reachable.

`tests/parser/test_semantic_query.c` covers the query bridge from reachability facts to structured diagnostics:
- no diagnostic facts still returns a valid empty diagnostics list;
- an in-scope unreachable reachability fact becomes one `unreachable_code` warning diagnostic;
- reachable facts and out-of-scope unreachable facts do not produce diagnostics.

The targets are registered as `zr_vm_cfg_reachability_test`, `zr_vm_cfg_constant_conditions_test`, `zr_vm_cfg_integer_bitwise_conditions_test`, `zr_vm_cfg_float_arithmetic_conditions_test`, `zr_vm_cfg_switch_constants_test`, `zr_vm_cfg_finally_abrupt_test`, `zr_vm_cfg_try_catch_edges_test`, `zr_vm_cfg_throw_effects_test`, `zr_vm_cfg_typed_catch_flow_test`, `zr_vm_cfg_typed_catch_branch_flow_test`, `zr_vm_cfg_typed_catch_loop_flow_test`, and `zr_vm_cfg_typed_catch_switch_flow_test`, and all twelve are included in the `language_pipeline` executable list.

## Limits And Next Steps

Direct lambda IIFEs are the current narrow exception to general call conservatism. Named functions, member calls, import/construct effects, async lambda behavior, closure fixed points, user exception types, and inheritance-aware catch matching still need explicit metadata or deeper analysis before they can be treated as nonthrowing or precisely typed.

This scaffold is not the final CFG model. Switch-local `break` is not implemented because the current AST carries only `isBreak`/`expr`, the compiler resolves `break`/`continue` through `loopLabelStack`, and switch compilation does not push a switch break label. That can become a CFG task only after the language/compiler semantics change. The remaining CFG work includes iterator/item binding state, broader typed catch pattern/type matching beyond known literal/cast/local-declaration/straight-line-assignment/simple full-branch, partial-branch, multi-variable branch, constant-if branch pruning, symbolic terminating-branch fallthrough pruning, folded selected-branch terminator sibling pruning, simple unknown-loop init/body/step assignment, simple `foreach` body assignment, simple switch case/default assignment, matching or mismatched-kind known constant switch selector/case assignment, switch fallthrough-pruned case/default assignment including folded-if selected branch terminators, loop-control transfer bindings including nested conditional break prefix assignments, and straight-line terminator-pruned throw kind sets, function-effect/nonthrow metadata for more precise call handling, broader constant-condition folding beyond literal/unary/logical short-circuit/comparison plus known integer `+`/`-`/`*`/`/`/`%`/unary `+`/`-`/`~`/nonnegative `&`/`|`/`^`/`<<`/`>>`, finite float `+`/`-`/`*`/`/`/unary `+`/`-`, and same-kind folded integer/float/char relational comparisons, complex switch selectors, implicit conversion rules, union switch/default matching, concrete dataflow analyses, and LSP/compiler frontend publishing of semantic query diagnostics. `while(true)` and `for(;;)` no longer get a synthetic zero-iteration exit edge, but later slices still need broader nontermination, deeper recursive branch flow, more symbolic conditional break/continue-sensitive typed catch exits, and loop fixed-point precision beyond explicit loop-control exits.
