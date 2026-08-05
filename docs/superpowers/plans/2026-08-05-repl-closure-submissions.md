# REPL Closure Submissions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement LSP 04 E5 so a CLI REPL cell executes against a generation-checked, rooted closure environment rather than replaying prior source text.

**Architecture:** Add a narrow parser submission API that accepts verified prior bindings and publishes new capture bindings from the normal parser, binder, canonical facts, and compiler. The CLI keeps one global state and replaces a rooted closure environment only after a cell completes successfully; existing `GET_CLOSURE` and `SET_CLOSURE` instructions carry cross-cell reads and writes. The normal source compiler, grammar, and runtime instruction set remain unchanged unless an explicit submission context is present.

**Tech Stack:** C17, CMake, Unity C tests, `zr_vm_parser`, `zr_vm_core` closure/GC-root APIs, `zr_vm_cli` REPL end-to-end process tests.

---

## File Structure

- Modify: `zr_vm_parser/include/zr_vm_parser/compiler.h`
  - Publish the immutable submission input row, owned compile result, and `ZrParser_Source_CompileSubmission` API.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler.c`
  - Thread an optional submission context through the existing parse/import/build-facts/compiler flow without creating a second parser mode.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c`
  - Initialize the borrowed submission context and output binding collector in `SZrCompilerState`.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h`
  - Declare submission-only compiler helpers used by the existing compiler pipeline.
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c`
  - Validate all input rows, seed the type environment and closure variables, preflight non-escaping values, reserve declaration slots, and copy output facts.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c`
  - Seed `closureVars` from verified rows using their existing capture index and canonical identity.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c`
  - Publish an accepted top-level callable as a closure slot using its formal signature rather than a source replay registration.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c`
  - Resolve an injected callable through its verified closure row and formal signature before normal call lowering.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c`
  - Lower an accepted top-level submission declaration directly to its reserved `SET_CLOSURE` slot instead of a short-lived local.
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c`
  - Keep existing identifier assignment lowering, but route submission bindings through the pre-seeded closure row and reject a stale identity before emitting a store.
- Create: `tests/parser/test_repl_submission_bindings.c`
  - Exercise the public compiler API, capture metadata, stale generation rejection, and non-escaping type preflight without spawning the CLI.
- Modify: `tests/CMakeLists.txt`
  - Register the focused parser submission test and its existing parser/core link dependencies.
- Create: `zr_vm_cli/src/zr_vm_cli/repl/repl_session.h`
  - Define the CLI-private session, binding table, and submit/query/reset API.
- Create: `zr_vm_cli/src/zr_vm_cli/repl/repl_session.c`
  - Own the long-lived global, `SZrGcRootHandle`, active closure, execution helper, binding table, successor construction, and cleanup.
- Modify: `zr_vm_cli/src/zr_vm_cli/repl/repl.c`
  - Keep input buffering and commands only; delegate submission, `:type`, and `:reset` to `repl_session` and remove accumulated-source replay.
- Modify: `tests/cli/test_cli_repl_e2e.c`
  - Add process-level generation, assignment, owner/ref-like, reset, `:type`, and semicolon cases.
- Create: `docs/parser-and-semantics/repl-closure-submissions.md`
  - Describe the canonical submission contract and reject/fail-closed boundaries.
- Modify: `docs/parser-and-semantics/index.md`
  - Link the new REPL submission contract.
- Create: `docs/plans/lsp/04-debug-and-repl/2026-08-05-e5-repl-closure-generations.md`
  - Write the required E5 status, outputs, exact validation evidence, and commit ID.
- Modify: `docs/plans/lsp/semantic-inference/status-and-output.md`
  - Add the E5 row under `## 状态与产出记录` only after all acceptance gates pass.

## Task 1: Public Submission Contract And Parser RED Test

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/compiler.h`
- Create: `tests/parser/test_repl_submission_bindings.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing parser test for a canonical prior capture**

Add a focused Unity test that constructs one `SZrParserSubmissionBinding` with a nonzero SymbolId, TypeId, PlaceId, whole declaration range, capture index `0`, and matching module/environment/cell generations. Compile `return seed + 3;` through the new API and assert a non-null function with one typed closure binding whose identity is exact.

```c
TEST_ASSERT_NOT_NULL(ZrParser_Source_CompileSubmission(
        fixture.state, "return seed + 3;", strlen("return seed + 3;"),
        fixture.sourceName, &context, &result));
TEST_ASSERT_EQUAL_UINT32(0u, result.bindingCount);
TEST_ASSERT_TRUE(repl_submission_has_capture_identity(
        function, 0u, seed.symbolId, seed.typeId, &seed.declarationRange));
```

- [ ] **Step 2: Run the focused target to verify the API is unavailable**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test
ctest --test-dir .codex/build-e5-gcc -R '^repl_submission_bindings$' --output-on-failure
```

Expected: compilation fails because `SZrParserSubmissionBinding`, `SZrParserSubmissionContext`, and `ZrParser_Source_CompileSubmission` do not exist.

- [ ] **Step 3: Add the immutable input and owned output API**

In `compiler.h`, add the following contract near the existing source-compile APIs. `bindings` is borrowed for the call; `result` owns deep-copied names and inferred types until `Free`.

```c
typedef struct SZrParserSubmissionBinding {
    SZrString *name;
    EZrParserSubmissionBindingKind kind;
    SZrInferredType inferredType;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrUInt32 placeId;
    SZrFileRange declarationRange;
    TZrUInt32 captureIndex;
    TZrUInt64 moduleGeneration;
    TZrUInt64 environmentGeneration;
    TZrUInt64 cellGeneration;
} SZrParserSubmissionBinding;

typedef struct SZrParserSubmissionCallableSignature {
    SZrInferredType returnType;
    SZrArray parameterTypes;
    SZrArray parameterPassingModes;
} SZrParserSubmissionCallableSignature;

typedef struct SZrParserSubmissionContext {
    const SZrParserSubmissionBinding *bindings;
    const SZrParserSubmissionCallableSignature *callableSignatures;
    TZrSize bindingCount;
    TZrUInt64 moduleGeneration;
    TZrUInt64 environmentGeneration;
    TZrUInt64 cellGeneration;
} SZrParserSubmissionContext;

typedef struct SZrParserSubmissionResult {
    SZrParserSubmissionBinding *bindings;
    TZrSize bindingCount;
} SZrParserSubmissionResult;

ZR_PARSER_API SZrFunction *ZrParser_Source_CompileSubmission(
        SZrState *state, const TZrChar *source, TZrSize sourceLength,
        SZrString *sourceName, const SZrParserSubmissionContext *context,
        SZrParserSubmissionResult *result);
ZR_PARSER_API void ZrParser_SubmissionResult_Free(
        SZrState *state, SZrParserSubmissionResult *result);
```

Define `EZrParserSubmissionBindingKind` immediately before these structs with `VALUE` and `CALLABLE` members. A value row has an empty signature. A callable row has one same-index signature; `ZrParser_SubmissionResult_Free` frees every copied return/parameter inferred type and both arrays.

- [ ] **Step 4: Register the test target**

Add `zr_vm_repl_submission_bindings_test` in the existing parser test section of `tests/CMakeLists.txt`, using `tests/parser/test_repl_submission_bindings.c`, the normal parser/core link helper, and a `repl_submission_bindings` CTest entry.

- [ ] **Step 5: Re-run the test and commit the RED/API slice**

Run the command from Step 2. Expected: target builds and test fails only because submission contexts are not yet seeded into the compiler.

```powershell
git add zr_vm_parser/include/zr_vm_parser/compiler.h tests/parser/test_repl_submission_bindings.c tests/CMakeLists.txt
git commit -m "test(parser): define repl submission contract"
```

## Task 2: Seed Canonical Bindings And Preflight Escape Policy

**Files:**
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c`
- Modify: `tests/parser/test_repl_submission_bindings.c`

- [ ] **Step 1: Extend the parser test with stale and non-escaping negative cases**

Add tests that pass a mismatched row generation, a zero SymbolId, a `ref`/`readonly ref` inferred type, and a ref-like inferred type. Each must return `NULL`, leave `result.bindingCount == 0`, and compile no executable function.

```c
TEST_ASSERT_NULL(ZrParser_Source_CompileSubmission(
        fixture.state, "return seed;", strlen("return seed;"), fixture.sourceName,
        &staleContext, &result));
TEST_ASSERT_EQUAL_UINT32(0u, (unsigned)result.bindingCount);
```

- [ ] **Step 2: Run the focused test to capture the failing behavior**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test
ctest --test-dir .codex/build-e5-gcc -R '^repl_submission_bindings$' --output-on-failure
```

Expected: positive and negative submission cases fail because the compiler still starts with an empty type environment and closure list.

- [ ] **Step 3: Add one submission-only compiler bridge**

In `compiler_internal.h`, declare helpers implemented only by `compiler_submission.c`:

```c
TZrBool ZrParser_CompilerSubmission_Seed(SZrCompilerState *cs);
TZrBool ZrParser_CompilerSubmission_Preflight(SZrCompilerState *cs, SZrAstNode *script);
TZrBool ZrParser_CompilerSubmission_IsReservedDeclaration(
        const SZrCompilerState *cs, const SZrAstNode *declaration,
        TZrUInt32 *outCaptureIndex);
TZrBool ZrParser_CompilerSubmission_CollectResult(
        SZrCompilerState *cs, SZrParserSubmissionResult *result);
```

Define `EZrParserSubmissionValuePolicy` in the same internal header with `VALUE`, `UNIQUE_MOVE`, `SHARED_COPY`, `WEAK_COPY`, and `REJECT` members. The classifier is the sole producer of this enum for preflight and cleanup.

`Seed` must reject an invalid row before registration; call `ZrParser_TypeEnvironment_RegisterClosureCapture` with the supplied name, inferred type, SymbolId, TypeId, declaration range, capture index, and generation token. It must append the matching `SZrFunctionClosureVariable` with `inStack = ZR_FALSE`, not manufacture a symbol or slot from the name.

For `CALLABLE`, `Seed` must instead deep-copy the formal return type, parameter types, and passing modes into `ZrParser_TypeEnvironment_RegisterFunctionEx`, then seed the same capture index as a closure value. A same-name callable is usable only when the signature and canonical identity match its row. Type-like declarations without a recoverable canonical module/prototype identity must fail preflight; E5 must not replay their source to make them appear persistent.

`Preflight` must read structured inferred-type/reference/capability information and reject values that carry reference access or ref-like capability. It must not inspect source spelling, display text, or error messages.

- [ ] **Step 4: Thread the optional context through the existing compiler pipeline**

Change the private compile-mode functions in `compiler.c` to accept `const SZrParserSubmissionContext *submission` and `SZrParserSubmissionResult *result`. After `ZrParser_CompilerState_Init`, attach the borrowed pointers, run `Seed` before source statement binding, and run `Preflight` after normal type binding but before function allocation/execution setup.

```c
if (submission != ZR_NULL &&
        (!ZrParser_CompilerSubmission_Seed(&cs) ||
         !ZrParser_CompilerSubmission_Preflight(&cs, ast))) {
    ZrParser_CompilerState_Free(&cs);
    return ZR_NULL;
}
```

Keep `ZrParser_Source_Compile` passing `ZR_NULL`; only `ZrParser_Source_CompileSubmission` passes a non-null context. On every error path zero and free `result` exactly once.

- [ ] **Step 5: Run parser tests and commit the seeded-binding slice**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test zr_vm_semantic_query_test
ctest --test-dir .codex/build-e5-gcc -R '^(repl_submission_bindings|semantic_query)$' --output-on-failure
```

Expected: canonical capture test passes; stale/ref/ref-like cases fail closed.

```powershell
git add zr_vm_parser/src/zr_vm_parser/compiler.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c tests/parser/test_repl_submission_bindings.c
git commit -m "feat(parser): seed canonical repl submissions"
```

## Task 3: Persist Top-Level Declarations And Assignments In Closure Slots

**Files:**
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c`
- Modify: `tests/parser/test_repl_submission_bindings.c`

- [ ] **Step 1: Add a failing direct-execution test for two cells**

The test must compile the first cell `var seed = 2;`, build a closure from its output slot, then compile a second cell `seed = seed + 3; return seed;` using the first result as input. Execute both closures and assert the second result string is `5`; inspect the second function instructions/typed closure metadata to prove both read and write use capture index `0`. Add a second pair of cells, `fn add(value: int): int { return value + 1; }` and `return add(2);`, and assert the later call consumes the exact injected callable signature and capture identity.

```c
TEST_ASSERT_TRUE(repl_submission_execute(&fixture, firstFunction, firstClosure, &value));
TEST_ASSERT_TRUE(repl_submission_execute(&fixture, secondFunction, secondClosure, &value));
TEST_ASSERT_EQUAL_STRING("5", repl_submission_value_text(&fixture, &value));
TEST_ASSERT_TRUE(repl_submission_has_closure_access(secondFunction, 0u));
```

- [ ] **Step 2: Run the test to verify declaration storage is still local**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test
ctest --test-dir .codex/build-e5-gcc -R '^repl_submission_bindings$' --output-on-failure
```

Expected: second cell cannot read the first declaration or the first declaration is absent from typed closure metadata.

- [ ] **Step 3: Reserve declarations before lowering and emit `SET_CLOSURE`**

`compiler_submission.c` must scan only top-level variable declarations after normal binder registration. For each accepted declaration, reserve the next capture index, copy the binder-produced SymbolId/TypeId/PlaceId/range/inferred type, and seed a non-stack closure variable before expansion.

At the start of `compile_variable_declaration` in `compile_statement.c`, select the reserved row structurally. Compile the initializer through existing expression lowering and write it to the reserved capture, leaving it visible to later statements in the same cell.

```c
if (ZrParser_CompilerSubmission_IsReservedDeclaration(cs, node, &captureIndex)) {
    ZrParser_Expression_Compile(cs, declaration->value);
    emit_instruction(cs, create_instruction_2(
            ZR_INSTRUCTION_ENUM(SET_CLOSURE),
            ZR_COMPILE_SLOT_U16(cs->lastExpressionSlot),
            ZR_COMPILE_SLOT_U16(captureIndex), 0));
    return;
}
```

In `compile_expression.c`, existing closure assignment lowering remains the only store implementation. Add a submission row identity check before the `SET_CLOSURE` emission, so a same-name row with a mismatched SymbolId/TypeId/range is a compiler error rather than a fallback.

After normal `compile_function_declaration` creates its child function, `compiler_function.c` must emit the existing `GET_SUB_FUNCTION` for the generated child and a `SET_CLOSURE` for the reserved callable capture. `compile_expression_call.c` must use the seeded formal signature to type-check the call and lower the captured function value through the existing direct-call/value-call path. No call is selected by function spelling alone.

- [ ] **Step 4: Collect final output rows from canonical compiler state**

After normal function assembly, `CollectResult` deep-copies only reserved declaration rows from the compiler state. It must copy the canonical values produced by the normal binder and assign their reserved capture index. It must not reconstruct a TypeId from a type string or reuse an unverified input row.

- [ ] **Step 5: Run the focused parser suite and commit**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test zr_vm_semantic_facts_test
ctest --test-dir .codex/build-e5-gcc -R '^(repl_submission_bindings|semantic_facts)$' --output-on-failure
```

Expected: declaration and assignment use the same capture index, and all identity mismatches fail closed.

```powershell
git add zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c tests/parser/test_repl_submission_bindings.c
git commit -m "feat(parser): lower repl declarations through closure slots"
```

## Task 4: Build A Rooted CLI Submission Session

**Files:**
- Create: `zr_vm_cli/src/zr_vm_cli/repl/repl_session.h`
- Create: `zr_vm_cli/src/zr_vm_cli/repl/repl_session.c`
- Modify: `zr_vm_cli/src/zr_vm_cli/repl/repl.c`
- Modify: `tests/cli/test_cli_repl_e2e.c`

- [ ] **Step 1: Add failing CLI cases for generation-backed values**

Extend `test_cli_repl_e2e.c` with a single process sequence:

```text
var seed = 2;

seed = seed + 3;

seed

:type seed
:reset
:type seed
:quit
```

Assert output contains `5`, `Type: int`, and an unavailable/stale diagnostic after reset. Assert the old `sessionSource` text is never required by checking the new session helper exposes only binding count and generations, not a source accumulator.

- [ ] **Step 2: Run the existing CLI test to establish the failing behavior**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_cli_executable zr_vm_cli_repl_e2e_test
ctest --test-dir .codex/build-e5-gcc -R '^cli_repl_e2e$' --output-on-failure
```

Expected: the reset/type generation assertion fails because the old REPL only frees accumulated source and creates a new global for every submit.

- [ ] **Step 3: Implement the session API in a dedicated module**

Define a private session with one long-lived global and main state, one `SZrGcRootHandle` for the active closure, and an owned table of `SZrParserSubmissionBinding` rows:

```c
typedef struct ZrCliReplSession {
    SZrGlobalState *global;
    SZrState *state;
    SZrGcRootHandle environmentRoot;
    SZrClosure *activeClosure;
    SZrParserSubmissionBinding *bindings;
    TZrSize bindingCount;
    TZrUInt64 moduleGeneration;
    TZrUInt64 environmentGeneration;
    TZrUInt64 nextCellGeneration;
} ZrCliReplSession;
```

`ZrCli_ReplSession_Init` creates the bare global, registers standard modules, injects process arguments once, and creates an empty closure/root. `ZrCli_ReplSession_Submit` uses `ZrParser_Source_CompileSubmission`, creates a successor closure with the exact output capture count, copies old capture values through `ZrCore_Value_Copy`, executes it, and publishes its root/bindings only after success.

```c
if (success) {
    ZrCore_GcRootHandle_Update(session->state, &session->environmentRoot,
                               ZR_CAST_RAW_OBJECT_AS_SUPER(successor));
    session->environmentGeneration++;
    session->nextCellGeneration++;
    zr_cli_repl_session_restamp_and_replace_bindings(
            session, &compileResult, session->moduleGeneration,
            session->environmentGeneration, session->nextCellGeneration);
} else {
    zr_cli_repl_session_discard_successor(session, successor, &compileResult);
}
```

The session must use `ZrCore_GcRootHandle_Resolve` before each closure read/write and reject a root that no longer resolves. It must not use `ZrCore_GarbageCollector_IgnoreObject` as a session-lifetime root.

`repl_session.h` must declare `ZrCli_ReplSession_Init`, `ZrCli_ReplSession_Free`, `ZrCli_ReplSession_Submit`, `ZrCli_ReplSession_TypeQuery`, and `ZrCli_ReplSession_Reset`. Successful publication stamps every retained and new row with the new environment/cell generations; the module generation remains stable until reset. A failed cell leaves the root, all rows, and all three visible generations unchanged.

- [ ] **Step 4: Reduce `repl.c` to input and command orchestration**

Replace `ZrCliReplSessionContext`, `zr_cli_repl_session_append`, `zr_cli_repl_build_prefixed_source`, and per-submit global setup with calls to `ZrCli_ReplSession_Submit`, `ZrCli_ReplSession_TypeQuery`, and `ZrCli_ReplSession_Reset`. Preserve only the controlled bare-expression wrapper from `repl_input_scan.c`.

- [ ] **Step 5: Run the CLI and parser tests, then commit**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_cli_executable zr_vm_cli_repl_e2e_test zr_vm_repl_submission_bindings_test
ctest --test-dir .codex/build-e5-gcc -R '^(cli_repl_e2e|repl_submission_bindings)$' --output-on-failure
```

Expected: cross-cell assignment and `:type` consume the rooted canonical row; reset makes the previous row unavailable.

```powershell
git add zr_vm_cli/src/zr_vm_cli/repl/repl_session.h zr_vm_cli/src/zr_vm_cli/repl/repl_session.c zr_vm_cli/src/zr_vm_cli/repl/repl.c tests/cli/test_cli_repl_e2e.c
git commit -m "feat(cli): persist repl cells in rooted closures"
```

## Task 5: Enforce Owner, Reference, Ref-Like, Failure, And Reset Rules

**Files:**
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c`
- Modify: `zr_vm_cli/src/zr_vm_cli/repl/repl_session.c`
- Modify: `tests/parser/test_repl_submission_bindings.c`
- Modify: `tests/cli/test_cli_repl_e2e.c`

- [ ] **Step 1: Add failing owner-policy tests**

Add direct parser and CLI cases that prove the policy is driven by inferred type facts:

```text
var owned: Unique<Resource> = init Resource();

owned.share();

var borrowed = readonly ref owned.member;

```

Assert that an accepted `Unique<T>` declaration produces a capture row, `Shared<T>`/`Weak<T>` values follow normal copy/release, and `ref`, `readonly ref`, `ref struct`, and `PoolRef` submissions fail before execution. Use existing resource/ref-like fixtures for the exact syntax and assert no new binding is published after a rejected cell.

- [ ] **Step 2: Run targeted tests to verify the gaps**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test zr_vm_cli_repl_e2e_test
ctest --test-dir .codex/build-e5-gcc -R '^(repl_submission_bindings|cli_repl_e2e)$' --output-on-failure
```

Expected: tests expose any owner copy, ref escape, or failure-publication path not yet handled by the submission preflight/session cleanup.

- [ ] **Step 3: Make policy and cleanup structural**

Implement a single classifier in `compiler_submission.c` that returns one of the allowed categories from `SZrInferredType`, canonical capability fields, and active-loan facts. It must return rejection for any reference access, active loan, invalid owner state, or ref-like capability and make `Unique`, `Shared`, and `Weak` follow their existing runtime ownership operation. `repl_session.c` must release every unpublished successor capture and `ZrParser_SubmissionResult_Free` on compile/setup/runtime failure without rolling back already-observable program effects.

```c
if (classification == ZR_PARSER_SUBMISSION_VALUE_POLICY_REJECT) {
    ZrParser_Compiler_Error(cs, "REPL values cannot escape this cell", node->location);
    return ZR_FALSE;
}
```

The rejection message is diagnostic text only; classification must not branch on that text, a type name, or member spelling.

- [ ] **Step 4: Implement reset and read-only type query rules**

`ZrCli_ReplSession_Reset` must release the current root, dispose captured values through normal closure/ownership cleanup, clear binding rows, increment module and environment generations, and install a new empty rooted closure. `ZrCli_ReplSession_TypeQuery` must seed only generation-valid rows, use the formal parser/binder facts, and neither execute code nor mutate the root/bindings.

- [ ] **Step 5: Re-run policy tests and commit**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test zr_vm_cli_repl_e2e_test zr_vm_reference_escape_closure_suspension_test
ctest --test-dir .codex/build-e5-gcc -R '^(repl_submission_bindings|cli_repl_e2e|reference_escape_closure_suspension)$' --output-on-failure
```

Expected: forbidden values fail before execution, accepted owners use normal lifetime paths, failed cells publish nothing, and reset/type-query generations fail closed.

```powershell
git add zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c zr_vm_cli/src/zr_vm_cli/repl/repl_session.c tests/parser/test_repl_submission_bindings.c tests/cli/test_cli_repl_e2e.c
git commit -m "fix(repl): enforce cross-cell owner and reference policy"
```

## Task 6: Preserve The Parser Boundary And Complete E5 Documentation

**Files:**
- Modify: `zr_vm_cli/src/zr_vm_cli/repl/repl.c`
- Modify: `tests/cli/test_cli_repl_e2e.c`
- Create: `docs/parser-and-semantics/repl-closure-submissions.md`
- Modify: `docs/parser-and-semantics/index.md`
- Create: `docs/plans/lsp/04-debug-and-repl/2026-08-05-e5-repl-closure-generations.md`
- Modify: `docs/plans/lsp/semantic-inference/status-and-output.md`

- [ ] **Step 1: Add failing CLI semicolon and bare-expression boundary cases**

Add these submissions to the E2E test:

```text
1 + 2

var missing = 3

```

Assert the first is accepted only through the existing controlled `return <expr>;` wrapper and prints `3`. Assert the second reports the normal parser semicolon diagnostic and leaves the environment generation and binding count unchanged.

- [ ] **Step 2: Run the CLI test to establish the boundary**

Run:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_cli_executable zr_vm_cli_repl_e2e_test
ctest --test-dir .codex/build-e5-gcc -R '^cli_repl_e2e$' --output-on-failure
```

Expected: tests fail if any code path inserts a terminator into a simple statement or publishes a failed cell.

- [ ] **Step 3: Keep wrapping controlled and document the exact contract**

Only call `ZrCli_ReplInput_ShouldWrapExpression` before `ZrCli_ReplSession_Submit`; never modify parser tokenization or statement parsing. Write the module document with the submission row fields, generation validation, closure root, owner table, failure behavior, `:reset`, `:type`, and the no-ASI rule.

- [ ] **Step 4: Run full focused E5 validation on all toolchains**

Run each command with the test executable as the final command, preserving its true process exit code:

```powershell
cmake --build .codex/build-e5-gcc --target zr_vm_repl_submission_bindings_test zr_vm_cli_repl_e2e_test zr_vm_semantic_query_test
ctest --test-dir .codex/build-e5-gcc -R '^(repl_submission_bindings|cli_repl_e2e|semantic_query)$' --output-on-failure
cmake --build .codex/build-e5-clang --target zr_vm_repl_submission_bindings_test zr_vm_cli_repl_e2e_test zr_vm_semantic_query_test
ctest --test-dir .codex/build-e5-clang -R '^(repl_submission_bindings|cli_repl_e2e|semantic_query)$' --output-on-failure
cmake --build .codex/build-e5-msvc --target zr_vm_repl_submission_bindings_test zr_vm_cli_repl_e2e_test zr_vm_semantic_query_test
ctest --test-dir .codex/build-e5-msvc -R '^(repl_submission_bindings|cli_repl_e2e|semantic_query)$' --output-on-failure
```

Expected: all selected suites pass with real exit `0`; the existing LSP/CLI smoke matrix remains runnable on the same committed baseline.

- [ ] **Step 5: Record E5 only after validation is green**

Write `## 状态与产出记录` in the E5 record with the real completion time, status `已完成`, exact implemented items, commit IDs, per-toolchain tests, and known existing ignored tests. Add one matching E5 row to `semantic-inference/status-and-output.md`; do not claim the overall semantic/LSP objective complete.

- [ ] **Step 6: Exact-path audit and final E5 commit**

Before staging, verify only the E5 parser/compiler/CLI/tests/docs paths listed above are changed. Exclude all `tests/language_server/**`, `zr_vm_language_server/**`, unrelated Syntax plan drafts, build directories, and `.codex` artifacts.

```powershell
git diff --check
git diff --name-only
git add zr_vm_cli/src/zr_vm_cli/repl/repl.c zr_vm_cli/src/zr_vm_cli/repl/repl_session.h zr_vm_cli/src/zr_vm_cli/repl/repl_session.c zr_vm_parser/include/zr_vm_parser/compiler.h zr_vm_parser/src/zr_vm_parser/compiler.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c tests/parser/test_repl_submission_bindings.c tests/cli/test_cli_repl_e2e.c tests/CMakeLists.txt docs/parser-and-semantics/repl-closure-submissions.md docs/parser-and-semantics/index.md docs/plans/lsp/04-debug-and-repl/2026-08-05-e5-repl-closure-generations.md docs/plans/lsp/semantic-inference/status-and-output.md
git diff --cached --check
git commit -m "feat(repl): persist canonical closure submissions"
```
