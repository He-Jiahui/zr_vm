# Ownership Intrinsics And Object Member Separation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace member-name ownership control with five reserved intrinsic expressions, add nullable/Weak direct and optional receiver guards, and provide identical `NullReferenceError` behavior across VM and AOT execution.

**Architecture:** Keep the existing `PrimaryExpression` base-plus-postfix-segments representation, add explicit access mode to member/call segments, and add a dedicated ownership-intrinsic AST node. Type inference publishes canonical ownership and receiver-guard facts; compiler lowering consumes those facts to emit ownership operations, one Weak wake per chain, structured null branches, cleanup, and named direct-access failure. Ordinary member/call lowering never classifies ownership from source names.

**Tech Stack:** C11, CMake, Unity tests, ZR AST/type inference/SemIR/ExecBC, ZR system library exception descriptors, AOT C/LLVM backends, PowerShell, WSL GCC/Clang, Windows MSVC.

---

## Repository And File Boundaries

The repository policy requires direct development on `main`; no branch or
worktree is created. Existing dirty LSP/REPL files are excluded from every
exact-path stage unless a later clean integration makes one of them unavoidable.

New responsibilities are placed in focused files because the current parser,
type-inference, compiler-chain, and execution-dispatch files already exceed the
repository's modularization threshold:

- `zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c`: parse the
  five reserved intrinsic calls and their recovery diagnostics.
- `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ownership_intrinsic.c`:
  validate intrinsic contracts and publish canonical ownership facts.
- `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c`:
  derive nullable/Weak chain guards and result lifting.
- `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_ownership_intrinsic.c`:
  lower intrinsic facts without member-name inspection.
- `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c`:
  lower direct/optional chain guards, merge slots, and hidden wake cleanup.
- `tests/parser/test_ownership_intrinsic_member_separation.c`: focused parser,
  semantic, instruction, runtime, side-effect, exception, and collision tests.
- `tests/acceptance/2026-08-10-ownership-object-member-separation.md`: exact
  baseline, commands, tool versions, results, and acceptance decision.

Existing files receive narrow integration edits:

- lexer/AST: `zr_vm_parser/include/zr_vm_parser/lexer.h`,
  `zr_vm_parser/src/zr_vm_parser/lexer.c`,
  `zr_vm_parser/include/zr_vm_parser/ast.h`, parser free/writer/dispatch files;
- canonical facts: `zr_vm_parser/include/zr_vm_parser/semantic_facts.h`,
  `zr_vm_parser/include/zr_vm_parser/semantic.h`,
  `zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c`;
- compiler/IR: parser compiler switch and ownership/dataflow/throw-profile files,
  `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h`, SemIR mapping,
  artifact writer/reader, optimizer, typed metadata, and instruction writer;
- runtime: `zr_vm_core/include/zr_vm_core/ownership.h`, ownership implementation,
  exception API/implementation, dispatch integration, and
  `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c`;
- AOT: ownership/member/control lowering and AOT runtime helper files under
  `zr_vm_aot/`;
- tooling/docs: clean semantic-fact LSP consumers, Syntax 04/05, language spec,
  parser/core module docs, indexes, and acceptance records.

## Task 1: Establish The Focused RED Harness

**Files:**
- Create: `tests/parser/test_ownership_intrinsic_member_separation.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Add a focused Unity target and parser helpers**

Create a target linked with `zr_vm_link_parser_core_plus_library` and helpers
that parse source, compile source, execute an integer result, capture structured
diagnostics, inspect primary segments, and inspect emitted opcodes. The first
tests refer to the intended public enum/API names so the target cannot pass
against the old implementation:

```c
static void test_reserved_intrinsics_have_independent_ast(void) {
    SZrAstNode *script = parse_source(
        "share(owner); degrade(shared); wake(weak); intoGc(owner); drop(owner);");
    assert_intrinsic(statement_expression(script, 0), ZR_OWNERSHIP_INTRINSIC_SHARE);
    assert_intrinsic(statement_expression(script, 1), ZR_OWNERSHIP_INTRINSIC_DEGRADE);
    assert_intrinsic(statement_expression(script, 2), ZR_OWNERSHIP_INTRINSIC_WAKE);
    assert_intrinsic(statement_expression(script, 3), ZR_OWNERSHIP_INTRINSIC_INTO_GC);
    assert_intrinsic(statement_expression(script, 4), ZR_OWNERSHIP_INTRINSIC_DROP);
    ZrParser_Ast_Free(g_state, script);
}

static void test_optional_member_and_call_segments_record_access_mode(void) {
    SZrAstNode *expr = parse_first_expression("weak?.service.send(1)?.(2);");
    assert_member_mode(expr, 0, ZR_POSTFIX_ACCESS_OPTIONAL);
    assert_member_mode(expr, 1, ZR_POSTFIX_ACCESS_DIRECT);
    assert_call_mode(expr, 2, ZR_POSTFIX_ACCESS_DIRECT);
    assert_call_mode(expr, 3, ZR_POSTFIX_ACCESS_OPTIONAL);
}
```

- [x] **Step 2: Run the target and record the expected RED**

Run in the existing GCC cache after regenerating CMake:

```powershell
wsl cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_ownership_intrinsic_member_separation_test -j 8
```

Expected: compilation fails because `ZR_OWNERSHIP_INTRINSIC_SHARE`,
`ZR_POSTFIX_ACCESS_OPTIONAL`, and the dedicated intrinsic AST do not exist.
Save the exact failure in the acceptance record baseline.

- [x] **Step 3: Keep the RED unstaged while implementing Task 2**

Do not commit a target that cannot compile on `main`. Preserve the observed RED,
then make the smallest Task 2 production change that turns this same target
green. The first commit includes both the test and its syntax/AST implementation.

## Task 2: Add Tokens, AST Nodes, And Postfix Modes

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/lexer.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/lexer.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/ast.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c`
- Create: `zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c`
- Test: `tests/parser/test_ownership_intrinsic_member_separation.c`

- [x] **Step 1: Extend RED coverage for lexical and recovery boundaries**

Add cases proving `?.` is one token, whitespace `? .` is not optional access,
intrinsics require exactly one positional argument, intrinsic names cannot be
bare values, and the same spellings remain legal after `.` and in member
declarations. Cover `callable(args)` and `callable?.(args)` explicitly, reject
`callable.(args)` and `receiver?.[index]`, and distinguish missing member names
from missing optional-call parentheses.

```c
TEST_ASSERT_EQUAL_INT(ZR_TK_QUESTION_DOT, lex_one("?."));
assert_parse_error("share;", "Intrinsic 'share' must be called with one argument");
assert_parse_error("share();", "Intrinsic 'share' requires exactly one argument");
assert_parse_error("share(a, b);", "Intrinsic 'share' requires exactly one argument");
assert_parses("class Box { pub fn wake(): int { return 1; } } new Box().wake();");
```

- [x] **Step 2: Append stable token and AST enum values**

Append, rather than insert, these values:

```c
ZR_TK_QUESTION_DOT,
ZR_TK_SHARE,
ZR_TK_DEGRADE,
ZR_TK_WAKE,
ZR_TK_INTO_GC,
ZR_TK_DROP,

ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION,
```

Add explicit syntax data:

```c
typedef enum EZrPostfixAccessMode {
    ZR_POSTFIX_ACCESS_DIRECT = 0,
    ZR_POSTFIX_ACCESS_OPTIONAL
} EZrPostfixAccessMode;

typedef enum EZrOwnershipIntrinsicOperation {
    ZR_OWNERSHIP_INTRINSIC_SHARE = 0,
    ZR_OWNERSHIP_INTRINSIC_DEGRADE,
    ZR_OWNERSHIP_INTRINSIC_WAKE,
    ZR_OWNERSHIP_INTRINSIC_INTO_GC,
    ZR_OWNERSHIP_INTRINSIC_DROP
} EZrOwnershipIntrinsicOperation;

typedef struct SZrOwnershipIntrinsicExpression {
    EZrOwnershipIntrinsicOperation operation;
    SZrAstNode *argument;
    SZrFileRange nameRange;
    SZrFileRange callRange;
} SZrOwnershipIntrinsicExpression;
```

Add `accessMode` to `SZrMemberExpression` and `SZrFunctionCall`. Direct parsing
always initializes it; optional syntax never relies on zeroed memory by accident.

- [x] **Step 3: Parse reserved intrinsics and `?.` segments**

`parser_ownership_intrinsic.c` maps only reserved tokens to operations, consumes
one parenthesized expression, rejects commas/named arguments/missing arguments,
and returns the dedicated node. `parse_primary_expression` dispatches those
tokens before ordinary identifiers and then reuses `parse_member_access`.

`parse_member_access` handles:

```c
if (token == ZR_TK_QUESTION_DOT && peek_token(ps) == ZR_TK_LPAREN) {
    /* build OPTIONAL function-call segment */
} else if (token == ZR_TK_QUESTION_DOT) {
    /* build OPTIONAL named member segment */
}
```

`is_member_name_token` includes the five reserved tokens, preserving the member
namespace. Do not accept `?.[index]` in this milestone; report a precise
unsupported optional-computed-access diagnostic.

- [x] **Step 4: Complete AST ownership and writer coverage**

Free the intrinsic argument, print operation/access mode, and include new tokens
in expression-start and token-to-string paths. Audit every production switch on
AST kind plus existing clone/compare/serialize entry points; add the new node to
each facility that exists. The focused test performs parse-print-reparse
structural comparison and destroys both trees, so a missing writer, equality, or
ownership branch is observable even though the repository has no standalone
generic AST serializer API. Run:

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_ownership_intrinsic_member_separation_test -j 8
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_ownership_intrinsic_member_separation_test
```

Expected: lexer/parser/AST cases pass; semantic/runtime cases remain RED.

- [x] **Step 5: Commit syntax and AST**

```powershell
git add -- tests/CMakeLists.txt tests/parser/test_ownership_intrinsic_member_separation.c zr_vm_parser/include/zr_vm_parser/lexer.h zr_vm_parser/src/zr_vm_parser/lexer.c zr_vm_parser/include/zr_vm_parser/ast.h zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
git commit -m "feat(parser): add ownership intrinsics and optional postfix syntax"
```

## Task 3: Publish Canonical Intrinsic And Receiver-Guard Facts

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_facts.h`
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h`
- Create: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ownership_intrinsic.c`
- Create: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_profile.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_regions.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/type_system.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_system.c`
- Test: `tests/parser/test_ownership_intrinsic_member_separation.c`
- Test: `tests/parser/test_expression_fact_emission.c`

- [x] **Step 1: Add failing type/fact tests**

Cover exact contracts, consuming flags, nullable wake result, PlaceId/LoanId,
unsupported qualifiers, non-place consuming operands, redundant optional access,
unknown/dynamic rejection, nullable/Weak guard kind, direct/optional mode, chain
bounds, result lift, and throw profile.

```c
const SZrOwnershipIntrinsicFact *fact =
    ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(context, expr);
TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_INTRINSIC_DEGRADE, fact->operation);
TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED, fact->inputType.ownershipQualifier);
TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK, fact->resultType.ownershipQualifier);
TEST_ASSERT_FALSE(fact->consuming);

const SZrReceiverGuardFact *guard =
    ZrParser_SemanticFacts_FindReceiverGuardByNode(context, optionalMember);
TEST_ASSERT_EQUAL_INT(ZR_RECEIVER_GUARD_WEAK_WAKE, guard->kind);
TEST_ASSERT_EQUAL_INT(ZR_RECEIVER_GUARD_OPTIONAL, guard->mode);
TEST_ASSERT_EQUAL_INT(ZR_RECEIVER_GUARD_RESULT_NULLABLE, guard->resultLift);
```

Run the focused tests and confirm failure because fact APIs are absent.

- [x] **Step 2: Add fact storage and lookup APIs**

Add arrays to `SZrSemanticContext` and lifecycle them in `semantic_facts.c`.
Facts contain copied canonical inferred types, source node/ranges, PlaceId,
LoanId, operation/guard mode, consuming bit, chain segment bounds, and result
lift. Reset/free releases every owned type/string exactly once.

- [x] **Step 3: Infer intrinsic contracts from the dedicated AST**

Move non-construction ownership typing out of `infer_construct_expression_type`.
`own` and `ref` remain construction/reference AST behavior; all five source
intrinsics use `infer_ownership_intrinsic_expression_type`. Use canonical
qualifier/resource prototype checks and existing place/loan APIs. `drop` accepts
Unique/Shared/Weak and returns the canonical `void` TypeRef; a runtime stack slot
may use the normal null value representation for `void`. `wake` returns
nullable Shared.

- [x] **Step 4: Infer receiver guards over the postfix chain**

Walk the primary base and ordered segments once. Nullable receivers publish
`NULL_GUARD`; Weak receivers publish `WEAK_WAKE_GUARD` targeting the same inner
TypeRef. OPTIONAL on non-null Unique/Shared/GcBox/plain values emits
`redundant_optional_access`; OPTIONAL on unknown/dynamic emits
`unsupported_optional_receiver`. The first optional guard lifts the final
non-void chain result to nullable without making later direct segments nullable
inside the success path.

- [x] **Step 5: Remove member-name ownership inference and dataflow**

Delete `ZrParser_OwnershipMemberNameToBuiltinKind` and all callers. Dataflow,
region, and throw-profile consumers switch on the intrinsic AST/fact. Remove
the former module-prototype `.share()` ownership escape as well: a guarded
module payload is not an owner operand, and its lifetime is held only by the
compiler-hidden scoped owner. If the module type actually declares a `share`
member, `module.share()` remains an ordinary member call and must never lower
to `OWN_SHARE`.

Run focused inference, fact, owner move/loan, and resource suites under GCC.

- [x] **Step 6: Commit canonical semantics**

```powershell
git add -- zr_vm_parser/include/zr_vm_parser/semantic_facts.h zr_vm_parser/include/zr_vm_parser/semantic.h zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c zr_vm_parser/src/zr_vm_parser/type_inference.c zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ownership_intrinsic.c zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c zr_vm_parser/src/zr_vm_parser/type_inference/cfg_throw_profile.c zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_regions.c zr_vm_parser/include/zr_vm_parser/type_system.h zr_vm_parser/src/zr_vm_parser/type_system.c tests/parser/test_ownership_intrinsic_member_separation.c tests/parser/test_expression_fact_emission.c
git commit -m "feat(semantics): type ownership intrinsics and receiver guards"
```

## Task 4: Lower Intrinsics Without Member Classification

**Files:**
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_ownership_intrinsic.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c`
- Test: `tests/parser/test_ownership_intrinsic_member_separation.c`
- Test: `tests/parser/test_compiler_features.c`

- [x] **Step 1: Add failing instruction and collision tests**

Compile each intrinsic and assert exact opcodes. Add a resource class with real
methods named `share`, `degrade`, `wake`, `intoGc`, and `drop`; assert those calls
emit normal member/call instructions and no ownership opcode. Add negative old
forms on owner handles and assert a structured migration diagnostic rather than
ownership lowering.

- [x] **Step 2: Lower from `OwnershipIntrinsicFact`**

The new compiler module obtains the fact for the intrinsic node, compiles its
argument exactly once into a source slot, validates the fact's PlaceId for
consuming operations, chooses the opcode from operation, and writes the result
slot. No function accepts a member name.

```c
switch (fact->operation) {
    case ZR_OWNERSHIP_INTRINSIC_SHARE: opcode = ZR_INSTRUCTION_ENUM(OWN_SHARE); break;
    case ZR_OWNERSHIP_INTRINSIC_DEGRADE: opcode = ZR_INSTRUCTION_ENUM(OWN_DEGRADE); break;
    case ZR_OWNERSHIP_INTRINSIC_WAKE: opcode = ZR_INSTRUCTION_ENUM(OWN_WAKE); break;
    case ZR_OWNERSHIP_INTRINSIC_INTO_GC: opcode = ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX); break;
    case ZR_OWNERSHIP_INTRINSIC_DROP: opcode = ZR_INSTRUCTION_ENUM(OWN_DROP); break;
}
```

- [x] **Step 3: Delete compiler member special cases**

Remove the generic ownership branch from `compile_primary_member_chain` and all
helpers used only by that branch. Preserve actual module member resolution under
its canonical module prototype contract. Successful target access continues
through ordinary `GET_MEMBER`, property/meta, and call lowering. Reference
escape and semantic IR read the intrinsic node/fact.

- [x] **Step 4: Run compiler and runtime ownership suites**

Run the new target, expression facts, compiler features, resource unique/drop,
resource shared/weak, resource borrow receiver, and property ref-return targets.
Confirm all new and migrated cases pass before committing.

- [x] **Step 5: Commit intrinsic lowering**

```powershell
git add -- zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_ownership_intrinsic.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c tests/parser/test_ownership_intrinsic_member_separation.c tests/parser/test_compiler_features.c
git commit -m "feat(compiler): lower ownership intrinsics from canonical facts"
```

## Task 5: Implement Chain-Level Nullable And Weak Guards

**Files:**
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c`
- Modify: `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h`
- Modify: `zr_vm_core/include/zr_vm_core/function.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_optimize.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c`
- Modify: `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c`
- Modify: `zr_vm_core/include/zr_vm_core/exception.h`
- Modify: `zr_vm_core/src/zr_vm_core/exception.c`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- Test: `tests/parser/test_ownership_intrinsic_member_separation.c`
- Test: `tests/exceptions/test_exceptions.c`

- [x] **Step 1: Add failing execution-order and lifetime tests**

Cover nullable member/method/call, Weak member/method/call, direct absence,
optional absence, skipped computed indexes/arguments/getters, mixed
`weak?.a.b` versus `weak?.a?.b`, one wake per chain, nullable flattening,
void no-op, ref/ref-like escape rejection, exact `NullReferenceError` catching,
and catch-through-`RuntimeError`. Use real source side effects, not mocks.

- [x] **Step 2: Register the named exception and add the direct guard instruction**

Register `NullReferenceError extends RuntimeError` in the system descriptor and
hints. Add a core helper that materializes an error from an explicit prototype
name and message. Append `REQUIRE_NON_NULL` without renumbering existing
instructions; it consumes one source slot and uses that helper to raise the
named exception when null. Optional guards use existing `JUMP_IF_NULL`. Map both
through typed metadata, optimizer, quickening/control-flow metadata, SemIR,
writer, and artifact round trips.

The artifact boundary is the lowered executable projection: opcode and jump
identity, merge/result slots, typed-binding TypeId/PlaceId, serialized TypeRefs,
exception tables, and cleanup/reset instructions. Do not serialize borrowed AST
pointers or the currently invalid (`0`) intrinsic LoanId merely to mirror the
source fact layout.

- [x] **Step 3: Lower one guard over the dominated suffix**

`compile_expression_receiver_guard.c` consumes `ReceiverGuardFact`. For Weak it
emits one `OWN_WAKE` to a hidden Shared slot; for nullable it reuses the receiver
slot. DIRECT emits `REQUIRE_NON_NULL`. OPTIONAL records a `JUMP_IF_NULL` patch,
compiles only the live suffix, copies the live result into a stable merge slot,
jumps over the failure block, initializes the merge slot to null on failure, and
patches all offsets. Arguments occur only in the live block.

Hidden Shared slots register normal cleanup metadata and receive an explicit
normal-path `OWN_DROP`; exception/frame unwind remains the exceptional cleanup
source. Reject borrowed/ref-like final values that outlive the guard.

- [x] **Step 4: Prove guard control flow in instructions and execution**

Assert emitted order contains one wake, guard before member/call, arguments only
after the optional branch, merge initialization, and cleanup. Run repeated
expire/wake loops and forced GC inside getter/call paths.

- [x] **Step 5: Commit receiver guards**

```powershell
git add -- zr_vm_common/include/zr_vm_common/zr_instruction_conf.h zr_vm_core/include/zr_vm_core/function.h zr_vm_core/include/zr_vm_core/exception.h zr_vm_core/src/zr_vm_core/exception.c zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_optimize.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c tests/exceptions/test_exceptions.c tests/parser/test_ownership_intrinsic_member_separation.c
git commit -m "feat(runtime): guard nullable and weak receiver chains"
```

## Task 6: Rename Ownership Operations Across Runtime And Artifacts

**Files:**
- Modify: `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h`
- Modify: `zr_vm_core/include/zr_vm_core/function.h`
- Modify: `zr_vm_core/include/zr_vm_core/ownership.h`
- Modify: `zr_vm_core/src/zr_vm_core/function.c`
- Modify: `zr_vm_core/src/zr_vm_core/ownership.c`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_optimize.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c`
- Test: `tests/parser/test_ownership_intrinsic_member_separation.c`
- Test: `tests/parser/test_aot_c_ownership_contracts.c`
- Test: `tests/parser/test_aot_c_ownership_shared_library_smoke.c`
- Test: `tests/parser/test_compiler_features.c`
- Test: `tests/parser/test_resource_shared_weak.c`
- Test: `tests/parser/test_resource_unique_drop.c`
- Test: `tests/parser/test_semir_pipeline.c`

- [x] **Step 1: Add failing operation-name synchronization tests**

Extend manual opcode/SemIR sync tests and textual-writer tests to require
`OWN_DEGRADE` and `OWN_WAKE`, while explicitly rejecting old emitted names.

- [x] **Step 2: Rename semantic/runtime operation symbols while preserving IDs**

Mechanically converge:

```text
OWN_WEAK -> OWN_DEGRADE
OWN_UPGRADE -> OWN_WAKE
ZR_SEMIR_OPCODE_OWN_WEAK -> ZR_SEMIR_OPCODE_OWN_DEGRADE
ZR_SEMIR_OPCODE_OWN_UPGRADE -> ZR_SEMIR_OPCODE_OWN_WAKE
ZrCore_Ownership_WeakValue -> ZrCore_Ownership_DegradeValue
ZrCore_Ownership_UpgradeValue -> ZrCore_Ownership_WakeValue
ZR_OWNERSHIP_BUILTIN_KIND_SHARED -> ZR_OWNERSHIP_BUILTIN_KIND_SHARE
ZR_OWNERSHIP_BUILTIN_KIND_WEAK -> ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE
ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE -> ZR_OWNERSHIP_BUILTIN_KIND_WAKE
ZR_OWNERSHIP_BUILTIN_KIND_RELEASE -> ZR_OWNERSHIP_BUILTIN_KIND_DROP
```

Keep numeric enum positions explicit. Do not provide old symbol aliases or old
source compatibility branches.

- [x] **Step 3: Run ownership, artifact, writer, and focused suites**

Confirm opcode numbers remain stable, textual artifacts use only canonical
names, and runtime ownership behavior is unchanged before committing. The
binary round-trip case must execute the source and reloaded functions, compare
their complete recursive ExecBC/SemIR projections, and cover both Weak optional
short-circuiting and direct `NullReferenceError`.

- [x] **Step 4: Commit runtime convergence**

```powershell
git add -- zr_vm_common/include/zr_vm_common/zr_instruction_conf.h zr_vm_core/include/zr_vm_core/function.h zr_vm_core/include/zr_vm_core/ownership.h zr_vm_core/src/zr_vm_core/function.c zr_vm_core/src/zr_vm_core/ownership.c zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_optimize.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c tests/parser/test_ownership_intrinsic_member_separation.c tests/parser/test_aot_c_ownership_contracts.c tests/parser/test_aot_c_ownership_shared_library_smoke.c tests/parser/test_compiler_features.c tests/parser/test_resource_shared_weak.c tests/parser/test_resource_unique_drop.c tests/parser/test_semir_pipeline.c
git commit -m "refactor(runtime): converge ownership operation names"
```

## Task 7: Bring AOT C And LLVM To Parity

**Files:**
- Modify: `zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h`
- Modify: `zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_control.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_ownership.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c`
- Test: `zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c`
- Test: `zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c`
- Test: `zr_vm_aot/tests/parser/test_known_call_pipeline.c`
- Modify: `zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c`
- Modify: `zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_llvm/ir/main.ll`
- Modify: `zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/main.zri`
- Modify: `zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c`
- Modify: `zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_llvm/ir/main.ll`
- Modify: `zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/main.zri`

- [x] **Step 1: Add failing AOT guard and renamed-op tests**

Build source programs for optional success/failure and direct null failure into
ExecIR, generated C, and LLVM. Assert one wake, skipped argument side effect,
cleanup, and `NullReferenceError` prototype.

- [x] **Step 2: Lower `REQUIRE_NON_NULL` and renamed operations**

AOT C calls the shared named-error runtime helper. LLVM emits the same null
branch and helper call. Keep artifact numeric opcode IDs stable and update all
manual sync tables and textual writers to canonical names.

- [x] **Step 3: Run AOT pipeline and executable parity tests**

Run focused AOT targets under WSL GCC and Clang, execute generated binaries, and
compare results/exceptions with interpreter execution.

- [x] **Step 4: Commit AOT parity**

```powershell
git add -- zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_control.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_ownership.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c zr_vm_aot/tests/parser/test_known_call_pipeline.c zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_llvm/ir/main.ll zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/main.zri zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_llvm/ir/main.ll zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/main.zri
git commit -m "feat(aot): lower receiver guards and canonical ownership ops"
```

## Task 8: Destructively Migrate Repository Source And Tooling

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_completion_semantic_facts.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_signature_semantic_facts.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_expression_text.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c`
- Modify: `tests/language_server/test_ownership_diagnostics_owner_set_cases.h`
- Modify: `tests/language_server/test_ownership_diagnostics_region_cases.h`
- Modify: `tests/language_server/test_ownership_diagnostics_weak_receiver_cases.h`
- Modify: `tests/language_server/test_lsp_diagnostic_safe_fix_cases.h`
- Test: `tests/language_server/test_semantic_analyzer.c`
- Test: `tests/language_server/test_lsp_advanced_editor_features.c`
- Test: `tests/language_server/test_lsp_inlay_semantic_facts.c`
- Test: `tests/language_server/test_lsp_local_semantic_query.c`
- Test: `tests/language_server/test_lsp_interface.c`

- [x] **Step 1: Add structured migration-diagnostic RED cases**

For canonical owner handles where real member lookup fails, assert edits:

```text
owner.share()       -> share(owner)
shared.weak()       -> degrade(shared)
shared.degrade()    -> degrade(shared)
weak.upgrade()      -> wake(weak)
weak.wake()         -> wake(weak)
owner.intoGc()      -> intoGc(owner)
owner.drop()        -> drop(owner)
```

Also prove a real target method with the same name produces no diagnostic. LSP
must expose only the parser/compiler diagnostic's structured `fixes[]` edit and
must rebind against the current document version. Add tooling assertions that
hover/signature detail comes from `OwnershipIntrinsicFact`, completion omits the
removed member operations, intrinsic spellings are not rename/symbol targets,
semantic tokens classify reserved intrinsics consistently, and formatting
preserves `?.` plus intrinsic calls without emitting an old member form.

- [x] **Step 2: Publish fact-driven migration diagnostics**

Only after canonical target-member lookup fails, use receiver TypeRef/qualifier,
call shape, and source ranges to publish the replacement. Do not inspect display
type strings, diagnostic text, or LSP-side AST fallbacks.

- [x] **Step 3: Rewrite all repository ownership source forms**

Apply exact mechanical replacements in production sources, tests, fixtures, and
examples. Update expected opcode names in goldens. Retain `.share()` only in
tests where canonical member resolution proves it is an ordinary user-defined
member; the module guard payload is deliberately not such an exception. The migration set is fixed by
the pre-change search and includes parser SemIR/type/union/task tests and the AOT
ownership source fixtures; repeat the search after rewriting to catch additions
made by concurrent clean integrations.

- [x] **Step 4: Prove old paths are gone**

Run repository searches for member-name classifier symbols, old ownership
opcodes/API names, `.weak()`, `.upgrade()`, owner `.share()`, and owner
`.intoGc()`. Every remaining match must be either a migration input, an ordinary
same-name member collision test, or historical acceptance evidence; production
compiler/runtime code must contain none.

- [x] **Step 5: Commit migration and LSP facts**

```powershell
git add -- zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_receiver_guard.c zr_vm_language_server/src/zr_vm_language_server/interface/lsp_completion_semantic_facts.c zr_vm_language_server/src/zr_vm_language_server/interface/lsp_signature_semantic_facts.c zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_expression_text.c zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c tests/fixtures/projects/lsp_language_feature_matrix/src/async_native.zr tests/fixtures/projects/syntax_reference_v1/src/ownership.zr tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/generic_session_lifecycle_pass.zr tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/manifest.json tests/language_server/test_ownership_diagnostics_owner_set_cases.h tests/language_server/test_ownership_diagnostics_region_cases.h tests/language_server/test_ownership_diagnostics_weak_receiver_cases.h tests/language_server/test_lsp_diagnostic_safe_fix_cases.h tests/language_server/test_semantic_analyzer.c tests/language_server/test_lsp_advanced_editor_features.c tests/language_server/test_lsp_inlay_semantic_facts.c tests/language_server/test_lsp_local_semantic_query.c tests/language_server/test_lsp_interface.c tests/parser/test_compiler_features.c tests/parser/test_parser.c tests/parser/test_percent_syntax_cutover.c tests/parser/test_property_ref_return.c tests/parser/test_resource_owner_borrow_receiver.c tests/parser/test_resource_shared_weak.c tests/parser/test_resource_unique_drop.c tests/parser/test_semir_pipeline.c tests/parser/test_syntax_reference_v1.c tests/parser/test_type_inference.c tests/parser/test_union.c tests/task/test_task_runtime.c zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/src/main.zr zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/src/main.zr zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
git commit -m "refactor(syntax): remove ownership member compatibility paths"
```

## Task 9: Synchronize Language And Module Documentation

**Files:**
- Modify: `docs/zr_language_specification.md`
- Modify: `docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
- Modify: `docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md`
- Modify: `docs/parser-and-semantics/ast-and-syntax-contracts.md`
- Modify: `docs/parser-and-semantics/resource-shared-weak.md`
- Modify: `docs/parser-and-semantics/resource-owner-borrow-receiver.md`
- Modify: `docs/parser-and-semantics/resource-unique-drop.md`
- Modify: `docs/parser-and-semantics/pre-semantic-ir-flow.md`
- Modify: `docs/parser-and-semantics/owned-field-lifecycle.md`
- Modify: `docs/parser-and-semantics/union-types.md`
- Create: `docs/parser-and-semantics/ownership-intrinsics-and-receiver-guards.md`
- Modify: `docs/parser-and-semantics/index.md`
- Modify: `docs/core-runtime/exception-scope-resource-cleanup.md`
- Modify: `docs/core-runtime/gc-domain-single-mutator-bridge.md`
- Modify: `docs/core-runtime/index.md`
- Modify: `docs/module-system/typed-module-metadata.md`
- Modify: `docs/plans/aot/05-ownership-gc-and-bridge.md`
- Modify: `docs/plans/syntax/README.md`
- Modify: `docs/plans/syntax/2026-07-18-zr-syntax-and-memory-model-redesign.md`
- Modify: `docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md`
- Modify: `docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md`
- Modify: `docs/plans/using/01-ownership-as-generics.md`
- Modify: `docs/plans/using/05-migration-and-phasing.md`
- Modify: `docs/zr_language_test_requirements.md`
- Modify: `README.md`
- Modify: `zr_vm_aot/docs/parser-and-semantics/ownership-builtins-semir-aot.md`
- Create: `tests/acceptance/2026-08-10-ownership-object-member-separation.md`

- [x] **Step 1: Write code-facing module documentation with required frontmatter**

The new module document lists every implementing code file, design/plan source,
test, fact, control-flow invariant, failure mode, performance boundary, and
intentional non-goal. Existing docs replace old `.share/.weak/.upgrade/intoGc`
contracts rather than appending contradictory notes. Historical acceptance
records and the approved design spec retain quoted migration inputs as evidence;
the post-change search documents those intentional matches explicitly.

- [x] **Step 2: Update plan status only from evidence**

Syntax 04/05 and indexes may say implementation is complete only after the
focused and full validation commands in Task 10 have passed. Until then use
`validated_pending_full_acceptance` in the detailed acceptance record.

- [x] **Step 3: Commit documentation synchronization**

```powershell
git add -- README.md docs/zr_language_specification.md docs/zr_language_test_requirements.md docs/plans/syntax/README.md docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md docs/plans/syntax/2026-07-18-zr-syntax-and-memory-model-redesign.md docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md docs/plans/aot/05-ownership-gc-and-bridge.md docs/plans/using/01-ownership-as-generics.md docs/plans/using/05-migration-and-phasing.md docs/parser-and-semantics/ast-and-syntax-contracts.md docs/parser-and-semantics/resource-shared-weak.md docs/parser-and-semantics/resource-owner-borrow-receiver.md docs/parser-and-semantics/resource-unique-drop.md docs/parser-and-semantics/pre-semantic-ir-flow.md docs/parser-and-semantics/owned-field-lifecycle.md docs/parser-and-semantics/union-types.md docs/parser-and-semantics/ownership-intrinsics-and-receiver-guards.md docs/parser-and-semantics/index.md docs/core-runtime/exception-scope-resource-cleanup.md docs/core-runtime/gc-domain-single-mutator-bridge.md docs/core-runtime/index.md docs/module-system/typed-module-metadata.md zr_vm_aot/docs/parser-and-semantics/ownership-builtins-semir-aot.md tests/acceptance/2026-08-10-ownership-object-member-separation.md
git commit -m "docs(syntax): document ownership intrinsics and receiver guards"
```

## Task 10: Full Validation, Cleanup, Review, And Final Commit

**Files:**
- Modify: `tests/acceptance/2026-08-10-ownership-object-member-separation.md`
- Modify: `docs/plans/syntax/README.md`
- Modify: `docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
- Modify: `docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md`
- Create: `tests/parser/test_ownership_receiver_guard_performance.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Run fresh focused GCC and Clang tests**

Configure/build from WSL and directly run the new ownership target, expression
fact emission, resource unique/drop, resource shared/weak, resource owner/borrow
receiver, property ref-return, SemIR pipeline, exceptions, LSP semantic analyzer,
LSP local semantic query, LSP inlay facts, LSP interface, and LSP advanced editor
binaries before the full CTest run:

```powershell
wsl cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
wsl cmake --build build/codex-wsl-gcc-debug -j 8
wsl sh -lc 'for test in zr_vm_ownership_intrinsic_member_separation_test zr_vm_ownership_receiver_guard_performance_test zr_vm_expression_fact_emission_test zr_vm_resource_unique_drop_test zr_vm_resource_shared_weak_test zr_vm_resource_owner_borrow_receiver_test zr_vm_property_ref_return_test zr_vm_semir_pipeline_test zr_vm_exceptions_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_lsp_advanced_editor_features_test; do "./build/codex-wsl-gcc-debug/bin/$test" || exit 1; done'
wsl ctest --test-dir build/codex-wsl-gcc-debug --output-on-failure --parallel 8
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp

wsl cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
wsl cmake --build build/codex-wsl-clang-debug -j 8
wsl sh -lc 'for test in zr_vm_ownership_intrinsic_member_separation_test zr_vm_ownership_receiver_guard_performance_test zr_vm_expression_fact_emission_test zr_vm_resource_unique_drop_test zr_vm_resource_shared_weak_test zr_vm_resource_owner_borrow_receiver_test zr_vm_property_ref_return_test zr_vm_semir_pipeline_test zr_vm_exceptions_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_lsp_advanced_editor_features_test; do "./build/codex-wsl-clang-debug/bin/$test" || exit 1; done'
wsl ctest --test-dir build/codex-wsl-clang-debug --output-on-failure --parallel 8
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
```

Any existing baseline failure is accepted only after comparing it to a fresh
pre-change/baseline record and proving the changed targets are not responsible.

- [x] **Step 2: Run the focused receiver-guard performance comparison**

Build a Release-mode microbenchmark that compiles and repeatedly executes equal
workloads for non-null direct access, Weak direct access, optional Weak success,
optional Weak failure, and a deep guarded chain. Print iteration counts,
nanoseconds per operation, and ratios to direct access. The test asserts the
semantic checksum for every variant and records, rather than hides, the guard
cost. Investigate any ratio caused by repeated wakes, source-name comparisons,
or duplicated suffix evaluation before acceptance.

```powershell
wsl cmake --build build/benchmark-gcc-release --target zr_vm_ownership_receiver_guard_performance_test -j 8
wsl ./build/benchmark-gcc-release/bin/zr_vm_ownership_receiver_guard_performance_test
```

- [x] **Step 3: Run memory/lifetime tooling**

Use ASan+UBSan for the focused ownership target and `valgrind --leak-check=full`
for repeated Weak optional-chain success/failure. Record tool versions, commands,
exit codes, and leak/error summaries.

- [x] **Step 4: Run Windows MSVC compatibility and tests**

Use the Visual Studio developer environment and run:

```powershell
cmake -S . -B build\codex-msvc-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-debug --config Debug --parallel 8
ctest --test-dir build\codex-msvc-debug -C Debug --output-on-failure --parallel 8
```

If the repository-wide known MSVC baseline blocks full CTest, directly build and
run all touched targets plus the CLI smoke and document the exact remaining
baseline rather than claiming full green.

- [ ] **Step 5: Perform requirement-by-requirement review**

Audit every completion criterion in the design spec against current source,
tests, artifact output, and command evidence. Run `git diff --check`, inspect
every changed file, confirm no compatibility aliases/branches remain, and ensure
no unrelated dirty LSP/REPL paths are staged.

Pre-final review on 2026-08-26 removed the last named source-level DETACH
identity and three unused helper APIs, found and fixed a successful optional-
chain hidden-owner lifetime leak, then found and closed raw historical id `8`
acceptance in type inference and semantic-fact publication. GCC, Clang, and
MSVC pass the expanded 32-case ownership runner, 123 type-inference cases, and
14 semantic-fact cases. Keep this checkbox open until the final registered CTest
graph is re-enumerated and passes in full, along with the generated artifact
pair, migration inventory, and final diff review on the stable integrated
baseline.

The artifact-contract audit added a recursive `.zro` execution-projection
round trip. Current GCC directly passes 37/37: the reloaded graph preserves
ExecBC, SemIR, TypeRef, typed-binding TypeId/PlaceId, exception-table, and child-
function data, and produces the same Weak optional/direct result. This remains
focused pre-acceptance evidence until Clang/MSVC and the full graph replay on the
post-L8 stable baseline.

The checked-in `lsp_language_feature_matrix` artifact graph was also rebuilt
through the WSL GCC `zr_vm_cli --compile ... --intermediate` entry. Seven
tracked outputs changed semantically: three module `.zri` projections and all
four `.zro` files. Repeating generation at the canonical repository path
changed zero SHA-256 hashes, and the new `async_native.zro` contains `wakeView`
with no historical `upgraded` spelling. GCC, Clang, and MSVC each load that same
binary graph, print `matrix`, return `64`, report `executed_via=binary`, and
exit zero. The current execution-layout schema remains producer-ABI-sensitive:
an MSVC-produced diagnostic artifact's smaller private value slots were not
portable to the WSL loaders, while the larger WSL-produced layout was accepted
by all three. Cross-worktree `.zro` hashes also differ because the schema
retains absolute source/project mappings. Keep this step open until the same
WSL-producer/three-consumer replay is made on the final integrated baseline.

The final code review found one remaining fact-consumer gap: receiver-guard
lowering validated `chainSegmentStart` but ignored `chainSegmentEnd` and
`resultLift`, then closed every optional frame from the AST chain end. TDD first
reproduced the drift because a shortened dominated suffix fact compiled instead
of failing closed. Lowering now validates the full fact shape, carries the
fact-owned exclusive end and lift into each frame, requires the reached chain
end to match, and selects nullable versus void-no-op absence behavior from the
fact.

The mixed-chain runtime RED exposed a second, lower-level lifetime gap. A caught
inner direct guard could bypass normal finalization and leave an outer hidden
Shared wake alive. Each guard-owned `OWN_WAKE` now registers its destination
with `MARK_TO_BE_CLOSED`; normal live/absent exits close registrations in LIFO
order, while exception handling closes registrations above the saved handler
boundary. The test harness also materializes `zr.system.exception` before
asserting a named `NullReferenceError`, so it no longer enters unrelated generic
status normalization.

Follow-up review raised a possible early release when a direct guard's result
reuses the marked slot. Two runtime regressions return `Shared<Leaf>` through a
direct weak member and through an outer-optional/inner-direct mixed chain. Both
passed before any production response, proving the registered owner mirror
releases the hidden wake without clearing the copied expression result. The same
review did identify real fact-validation gaps. The injected matrix now covers a
nonzero shortened end, AST/mode drift, receiver kind drift, value/void lift
drift, and a missing member-chain guard. A second follow-up review then exposed
two remaining self-certification paths: lowering derived the expected guard from
the guard fact's own `receiverType`, and missing-fact detection excluded every
function-call segment before consulting canonical type. TDD added independent
canonical receiver drift, guarded-type drift, and missing nullable-callable fact
cases. Guard inference now publishes the receiver expression fact, lowering
compares against that canonical type before deriving kind/guarded type, and a
member or call segment with a canonical nullable/Weak receiver fails closed when
its guard fact is absent. Known non-null optional callable lowering remains
valid without a fabricated guard fact.

Isolated GCC 11.4, Clang 14, and MSVC 19.44 snapshots representing main
`075d68c` plus the exact ownership overlay each pass Shared/Weak 19/19,
ownership separation 37/37, type inference 123/123, expression facts 28/28, and
compiler integration 127/127. GCC and Clang pass SemIR 13/13; MSVC passes its
registered 12/12 set. The six focused executables were run serially per
toolchain because the ownership roundtrip case uses a fixed fixture path. The
focused receiver-guard correction is accepted. Keep the broader milestone open
for the full-graph replay on the stable integrated L8 baseline.

A fixed `2de3075` GCC 11.4 snapshot then built all 3,421 steps and enumerated
136 registered CTests. The first complete run finished with 134 passes and two
failures. `language_server_stdio_smoke` is the already-isolated L8 canonical
native-receiver contract and remains outside this write set. The independent
`projects/classes_super` failure exposed a lower semantic support regression:
class meta functions such as `@call` had no canonical SymbolId when override
relation publication ran. A focused RED reproduced the failure at 18/19
relation cases. Commit `592e5bc` registers named class meta functions through
the existing function-symbol path before override validation. GCC 11.4, Clang
14, and MSVC 19.44 now each pass the 19/19 relation runner and the complete
`projects` registered test. This removes the non-L8 failure without adding an
LSP or name-based fallback. It is not a replacement for a fresh single-command
full graph on the post-L8 stable baseline.

- [x] **Step 6: Remove generated build products and logs requested by the user**

The focused source/build roots were resolved to explicit absolute paths before
removal. Windows `E:\zrs\ownership-review-f77` and `E:\zrb\orm`, plus WSL
`/home/hejiahui/.codex-snapshots/ownership-review-f77` and the matching GCC and
Clang `.codex-builds` roots, are removed and verified absent. No persistent log
was created for the final serial matrix. Existing `.codex/logs` evidence owned
by L8, Q6, and other sessions is deliberately preserved.

The later full-graph audit also removed and verified absent the disposable
Windows worktree `E:\zrs\ownership-full-2de`, MSVC cache
`E:\zrb\ownership-full-2de-msvc`, WSL source snapshot
`/home/hejiahui/.codex-snapshots/ownership-full-2de`, and its GCC/Clang build
caches. The temporary full-snapshot tar and both generated patch files under
`E:\zrb` were deleted as well. CTest's internal logs were contained in those
removed caches; no repository or shared `.codex/logs` path was changed.

- [ ] **Step 7: Commit final acceptance status**

```powershell
git add -- tests/CMakeLists.txt tests/parser/test_ownership_receiver_guard_performance.c tests/acceptance/2026-08-10-ownership-object-member-separation.md docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
git diff --cached --check
git commit -m "test(syntax): accept ownership member separation"
```

Final evidence must show an empty index, only pre-existing unrelated worktree
changes, the exact commit series, and no unhandled design requirement.
