# ZR VM Semantic Inference Fact Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first-stage shared semantic fact, diagnostic, and local query layer for expression type, reference, numeric, reachability, logical, and ownership inference in ZR_VM.

**Architecture:** Keep inference ownership in the parser semantic context, not in duplicated LSP/debug helpers. Add focused fact and diagnostic modules under `zr_vm_parser`, then have type inference and the language server append/query those facts through narrow APIs. LSP consumes the shared facts through a local semantic query module that returns a fact, a diagnostic-backed failure, or an explicit unknown without crashing on incomplete syntax.

**Tech Stack:** C, CMake, Unity tests, `zr_vm_parser`, `zr_vm_language_server`, existing `SZrSemanticContext`, existing LSP interfaces, WSL-first validation with Windows MSVC CLI smoke.

---

## Repository And Coordination Baseline

- Work directly from `E:\Git\zr_vm` on branch `main`; do not create a worktree or feature branch.
- Current design source: `E:\Git\zr_vm\docs\superpowers\specs\2026-06-03-zr-vm-semantic-inference-design.md`.
- Related plans:
  - `E:\Git\zr_vm\.codex\plans\ZR_LSP 现代能力对齐计划.md`
  - `E:\Git\zr_vm\.codex\plans\Rust-First using  Ownership 语义收敛计划.md`
- Active coordination warning: `E:\Git\zr_vm\.codex\sessions\20260517-2312-inline-struct-payload.md` is touching core stack/type-layout/native inline payload paths. This plan must avoid those paths unless explicitly reconciled first.
- Current oversized files are orchestration points only:
  - `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\type_inference\type_inference_core.c`
  - `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer.c`
  - `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\lsp_semantic_query.c`
- Build source registration uses `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` in `E:\Git\zr_vm\zr_vm_common\CommonMacros.cmake`, so new `src/**/*.c` files are picked up after configure.

## File Structure

Create focused parser modules:

- `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\semantic_facts.h`
  Public fact enums, structs, append/query APIs, and explicit query result states.
- `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\semantic\semantic_facts.c`
  Fact array initialization, reset/free support, append helpers, and range/node/symbol queries.
- `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\diagnostic_builder.h`
  Structured diagnostic payload with code, message, cause, suggestion, primary range, related ranges, and optional edit text.
- `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\diagnostics\diagnostic_builder.c`
  Diagnostic construction, formatting helpers, and parser/type-inference helper diagnostics.

Modify parser files narrowly:

- `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\semantic.h`
  Add fact arrays to `SZrSemanticContext` and include `semantic_facts.h`.
- `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\semantic.c`
  Initialize/reset/free new arrays by delegating to `semantic_facts.c`.
- `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\type_inference.h`
  Keep existing public inference signatures; add no parallel inference entrypoint in stage 1.
- `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\type_inference\type_inference_core.c`
  Add only small calls to fact append helpers at successful/failed inference exits.
- `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser\parser_state.c`
  Route representative syntax diagnostics through the diagnostic builder while preserving the existing callback path.
- `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\parser.h`
  Extend the parser error callback payload only through a compatible optional structured diagnostic pointer or add a second structured callback; keep the existing callback usable.

Create focused language-server modules:

- `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\lsp_local_semantic_query.h`
  Internal LSP local query request/result declarations.
- `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\lsp_local_semantic_query.c`
  Position/range queries over `SZrSemanticContext`, parser diagnostics, and the last good analyzer snapshot.

Modify language-server files narrowly:

- `E:\Git\zr_vm\zr_vm_language_server\include\zr_vm_language_server\semantic_analyzer.h`
  Add diagnostic `cause` and `suggestion` strings to `SZrDiagnostic`.
- `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_typecheck.c`
  Append reachability/logical/ownership facts at the existing diagnostic sites.
- `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_references.c`
  Append reference facts at the existing reference tracker collection sites.
- `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_support.c`
  Construct/free richer diagnostics without changing public LSP wire structs.
- `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\interface\lsp_interface.c`
  Merge parser and semantic diagnostics in stable order and preserve controlled fallback behavior.
- `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\incremental_parser.c`
  Collect structured parser diagnostics and preserve last good AST/analyzer when syntax fails.

Add or extend tests:

- Create `E:\Git\zr_vm\tests\parser\test_semantic_facts.c` for fact container and query behavior.
- Modify `E:\Git\zr_vm\tests\parser\test_type_inference.c` for expression/numeric/ownership fact emission.
- Modify `E:\Git\zr_vm\tests\language_server\test_semantic_analyzer.c` for reachability/logical/reference/ownership facts and richer semantic diagnostics.
- Modify `E:\Git\zr_vm\tests\language_server\test_lsp_interface.c` for local query robustness, diagnostics, and last-good snapshot.
- Modify `E:\Git\zr_vm\tests\language_server\stdio_smoke.js` only if the richer diagnostics need stdio JSON assertions.
- Modify `E:\Git\zr_vm\tests\CMakeLists.txt` to register `zr_vm_semantic_facts_test`.
- Modify `E:\Git\zr_vm\docs\parser-and-semantics\index.md` with the new shared semantic fact layer contract.

---

### Task 1: Semantic Fact Schema And Container

**Files:**
- Create: `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\semantic_facts.h`
- Create: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\semantic\semantic_facts.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\semantic.h`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\semantic.c`
- Create: `E:\Git\zr_vm\tests\parser\test_semantic_facts.c`
- Modify: `E:\Git\zr_vm\tests\CMakeLists.txt`

- [x] **Step 1: Write the failing semantic facts test**

Add `tests/parser/test_semantic_facts.c` with these focused tests:

```c
#include "unity.h"

#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_core/state.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrCore_State_New();
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrCore_State_Free(g_state);
        g_state = ZR_NULL;
    }
}

static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;
    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "facts_test.zr", 13);
    return range;
}

static void test_semantic_context_initializes_fact_arrays(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_TRUE(context->expressionFacts.isValid);
    TEST_ASSERT_TRUE(context->referenceFacts.isValid);
    TEST_ASSERT_TRUE(context->numericFacts.isValid);
    TEST_ASSERT_TRUE(context->reachabilityFacts.isValid);
    TEST_ASSERT_TRUE(context->logicalFacts.isValid);
    TEST_ASSERT_TRUE(context->ownershipFacts.isValid);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->expressionFacts.length);
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_expression_fact_roundtrip_by_node_and_position(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode fakeNode;
    SZrInferredType inferredType;
    SZrSemanticExpressionFact fact;
    const SZrSemanticExpressionFact *foundByNode;
    const SZrSemanticExpressionFact *foundByPosition;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fakeNode, 0, sizeof(fakeNode));
    fakeNode.type = ZR_AST_INTEGER_LITERAL;
    fakeNode.location = test_range(4, 6);
    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_INT64);

    memset(&fact, 0, sizeof(fact));
    fact.node = &fakeNode;
    fact.range = fakeNode.location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.valueKind = ZR_SEMANTIC_VALUE_KIND_INT64;
    fact.hasConstant = ZR_TRUE;
    fact.constantValue.int64Value = 42;
    ZrParser_InferredType_Copy(g_state, &fact.inferredType, &inferredType);

    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &fact));
    foundByNode = ZrParser_SemanticFacts_FindExpressionByNode(context, &fakeNode);
    foundByPosition = ZrParser_SemanticFacts_FindExpressionAtPosition(context, test_range(5, 5));
    TEST_ASSERT_NOT_NULL(foundByNode);
    TEST_ASSERT_NOT_NULL(foundByPosition);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, foundByNode->inferredType.baseType);
    TEST_ASSERT_TRUE(foundByPosition->hasConstant);
    TEST_ASSERT_EQUAL_INT64(42, foundByPosition->constantValue.int64Value);

    ZrParser_InferredType_Free(g_state, &inferredType);
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_context_reset_clears_facts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticReachabilityFact fact;

    TEST_ASSERT_NOT_NULL(context);
    memset(&fact, 0, sizeof(fact));
    fact.range = test_range(10, 20);
    fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
    fact.cause = ZR_SEMANTIC_REACHABILITY_AFTER_RETURN;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReachability(context, &fact));
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)context->reachabilityFacts.length);

    ZrParser_SemanticContext_Reset(context);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->reachabilityFacts.length);
    TEST_ASSERT_EQUAL_UINT32(0, (TZrUInt32)context->expressionFacts.length);
    ZrParser_SemanticContext_Free(context);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_context_initializes_fact_arrays);
    RUN_TEST(test_semantic_expression_fact_roundtrip_by_node_and_position);
    RUN_TEST(test_semantic_context_reset_clears_facts);
    return UNITY_END();
}
```

- [x] **Step 2: Register the test target**

Add this block near the existing parser test targets in `tests/CMakeLists.txt`:

```cmake
    zr_vm_add_unity_test_target(
            zr_vm_semantic_facts_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semantic_facts.c
    )
    target_include_directories(zr_vm_semantic_facts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_semantic_facts_test)
```

Also include `zr_vm_semantic_facts_test` in the language pipeline executable list if the surrounding `if (TARGET ...)` block already includes parser support targets.

- [x] **Step 3: Run the new test and verify it fails before implementation**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_semantic_facts_test -j 8
```

Expected before implementation: compile fails because `zr_vm_parser/semantic_facts.h` and the new arrays/APIs do not exist.

- [x] **Step 4: Add the semantic facts public API**

Create `zr_vm_parser/include/zr_vm_parser/semantic_facts.h` with this shape:

```c
#ifndef ZR_VM_PARSER_SEMANTIC_FACTS_H
#define ZR_VM_PARSER_SEMANTIC_FACTS_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_core/array.h"

typedef struct SZrSemanticContext SZrSemanticContext;

typedef enum EZrSemanticFactExactness {
    ZR_SEMANTIC_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_FACT_APPROXIMATE,
    ZR_SEMANTIC_FACT_EXACT
} EZrSemanticFactExactness;

typedef enum EZrSemanticExpressionFactKind {
    ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN = 0,
    ZR_SEMANTIC_EXPRESSION_FACT_LITERAL,
    ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER,
    ZR_SEMANTIC_EXPRESSION_FACT_BINARY,
    ZR_SEMANTIC_EXPRESSION_FACT_UNARY,
    ZR_SEMANTIC_EXPRESSION_FACT_CALL,
    ZR_SEMANTIC_EXPRESSION_FACT_MEMBER,
    ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT,
    ZR_SEMANTIC_EXPRESSION_FACT_CONDITIONAL,
    ZR_SEMANTIC_EXPRESSION_FACT_ARRAY,
    ZR_SEMANTIC_EXPRESSION_FACT_OBJECT,
    ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA,
    ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN
} EZrSemanticExpressionFactKind;

typedef enum EZrSemanticValueKind {
    ZR_SEMANTIC_VALUE_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_VALUE_KIND_BOOL,
    ZR_SEMANTIC_VALUE_KIND_INT64,
    ZR_SEMANTIC_VALUE_KIND_DOUBLE,
    ZR_SEMANTIC_VALUE_KIND_STRING,
    ZR_SEMANTIC_VALUE_KIND_NULL,
    ZR_SEMANTIC_VALUE_KIND_OBJECT
} EZrSemanticValueKind;

typedef enum EZrSemanticReferenceKind {
    ZR_SEMANTIC_REFERENCE_UNKNOWN = 0,
    ZR_SEMANTIC_REFERENCE_DECLARATION,
    ZR_SEMANTIC_REFERENCE_READ,
    ZR_SEMANTIC_REFERENCE_WRITE,
    ZR_SEMANTIC_REFERENCE_CALL,
    ZR_SEMANTIC_REFERENCE_MEMBER_READ,
    ZR_SEMANTIC_REFERENCE_MEMBER_WRITE
} EZrSemanticReferenceKind;

typedef enum EZrSemanticNumericKind {
    ZR_SEMANTIC_NUMERIC_UNKNOWN = 0,
    ZR_SEMANTIC_NUMERIC_INTEGER,
    ZR_SEMANTIC_NUMERIC_FLOAT,
    ZR_SEMANTIC_NUMERIC_DECIMAL
} EZrSemanticNumericKind;

typedef enum EZrSemanticRangeCheckState {
    ZR_SEMANTIC_RANGE_UNKNOWN = 0,
    ZR_SEMANTIC_RANGE_SAFE,
    ZR_SEMANTIC_RANGE_OVERFLOW,
    ZR_SEMANTIC_RANGE_DIVIDE_BY_ZERO
} EZrSemanticRangeCheckState;

typedef enum EZrSemanticReachabilityState {
    ZR_SEMANTIC_REACHABILITY_UNKNOWN = 0,
    ZR_SEMANTIC_REACHABILITY_REACHABLE,
    ZR_SEMANTIC_REACHABILITY_UNREACHABLE
} EZrSemanticReachabilityState;

typedef enum EZrSemanticReachabilityCause {
    ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN = 0,
    ZR_SEMANTIC_REACHABILITY_AFTER_RETURN,
    ZR_SEMANTIC_REACHABILITY_AFTER_THROW,
    ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH,
    ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT
} EZrSemanticReachabilityCause;

typedef enum EZrSemanticLogicalState {
    ZR_SEMANTIC_LOGICAL_UNKNOWN = 0,
    ZR_SEMANTIC_LOGICAL_CONSTANT_TRUE,
    ZR_SEMANTIC_LOGICAL_CONSTANT_FALSE,
    ZR_SEMANTIC_LOGICAL_SHORT_CIRCUITS_RIGHT,
    ZR_SEMANTIC_LOGICAL_EVALUATES_RIGHT
} EZrSemanticLogicalState;

typedef enum EZrSemanticOwnershipState {
    ZR_SEMANTIC_OWNERSHIP_UNKNOWN = 0,
    ZR_SEMANTIC_OWNERSHIP_VALID,
    ZR_SEMANTIC_OWNERSHIP_MOVED_FROM,
    ZR_SEMANTIC_OWNERSHIP_WEAK_NEEDS_UPGRADE,
    ZR_SEMANTIC_OWNERSHIP_BORROW_ESCAPE,
    ZR_SEMANTIC_OWNERSHIP_LOAN_ESCAPE,
    ZR_SEMANTIC_OWNERSHIP_OWNER_TO_PLAIN_ESCAPE
} EZrSemanticOwnershipState;

typedef union SZrSemanticConstantValue {
    TZrBool boolValue;
    TZrInt64 int64Value;
    double doubleValue;
    SZrString *stringValue;
} SZrSemanticConstantValue;

typedef struct SZrSemanticExpressionFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticExpressionFactKind kind;
    EZrSemanticFactExactness exactness;
    SZrInferredType inferredType;
    EZrSemanticValueKind valueKind;
    TZrBool hasConstant;
    SZrSemanticConstantValue constantValue;
} SZrSemanticExpressionFact;

typedef struct SZrSemanticReferenceFact {
    TZrSymbolId declarationSymbolId;
    SZrAstNode *node;
    SZrFileRange declarationRange;
    SZrFileRange useRange;
    EZrSemanticReferenceKind kind;
} SZrSemanticReferenceFact;

typedef struct SZrSemanticNumericFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticNumericKind numericKind;
    EZrSemanticRangeCheckState rangeState;
    TZrBool hasConstant;
    SZrSemanticConstantValue constantValue;
} SZrSemanticNumericFact;

typedef struct SZrSemanticReachabilityFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticReachabilityState state;
    EZrSemanticReachabilityCause cause;
    SZrFileRange causeRange;
} SZrSemanticReachabilityFact;

typedef struct SZrSemanticLogicalFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrSemanticLogicalState state;
    SZrFileRange skippedRange;
} SZrSemanticLogicalFact;

typedef struct SZrSemanticOwnershipFact {
    SZrAstNode *node;
    SZrFileRange range;
    EZrOwnershipQualifier qualifier;
    EZrSemanticOwnershipState state;
    TZrSymbolId sourceSymbolId;
    SZrFileRange causeRange;
} SZrSemanticOwnershipFact;

ZR_PARSER_API void ZrParser_SemanticFacts_Init(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticFacts_Reset(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticFacts_Free(SZrSemanticContext *context);

ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendExpression(SZrSemanticContext *context, const SZrSemanticExpressionFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendReference(SZrSemanticContext *context, const SZrSemanticReferenceFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendNumeric(SZrSemanticContext *context, const SZrSemanticNumericFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendReachability(SZrSemanticContext *context, const SZrSemanticReachabilityFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendLogical(SZrSemanticContext *context, const SZrSemanticLogicalFact *fact);
ZR_PARSER_API TZrBool ZrParser_SemanticFacts_AppendOwnership(SZrSemanticContext *context, const SZrSemanticOwnershipFact *fact);

ZR_PARSER_API const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionByNode(const SZrSemanticContext *context, const SZrAstNode *node);
ZR_PARSER_API const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionAtPosition(const SZrSemanticContext *context, SZrFileRange position);
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPosition(const SZrSemanticContext *context, SZrFileRange position);
ZR_PARSER_API const SZrSemanticNumericFact *ZrParser_SemanticFacts_FindNumericByNode(const SZrSemanticContext *context, const SZrAstNode *node);
ZR_PARSER_API const SZrSemanticReachabilityFact *ZrParser_SemanticFacts_FindReachabilityAtPosition(const SZrSemanticContext *context, SZrFileRange position);
ZR_PARSER_API const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalByNode(const SZrSemanticContext *context, const SZrAstNode *node);
ZR_PARSER_API const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipAtPosition(const SZrSemanticContext *context, SZrFileRange position);

#endif
```

- [x] **Step 5: Add fact arrays to `SZrSemanticContext`**

In `semantic.h`, include `semantic_facts.h` after `type_system.h` and add these fields to `SZrSemanticContext`:

```c
    SZrArray expressionFacts;   // SZrSemanticExpressionFact
    SZrArray referenceFacts;    // SZrSemanticReferenceFact
    SZrArray numericFacts;      // SZrSemanticNumericFact
    SZrArray reachabilityFacts; // SZrSemanticReachabilityFact
    SZrArray logicalFacts;      // SZrSemanticLogicalFact
    SZrArray ownershipFacts;    // SZrSemanticOwnershipFact
```

- [x] **Step 6: Implement fact initialization, reset, free, and queries**

In `semantic_facts.c`, implement append-by-copy and linear queries. Use `ZrParser_InferredType_Copy` for expression facts and `ZrParser_InferredType_Free` when clearing expression facts.

Key helper behavior:

```c
static TZrBool semantic_fact_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (range.start.offset > 0 || range.end.offset > 0 || position.start.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }
    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}
```

Implement `FindExpressionAtPosition` by returning the narrowest containing expression range so nested expression facts work for hover/local inference.

- [x] **Step 7: Delegate lifecycle from `semantic.c`**

In `semantic_context_init_arrays`, call:

```c
    ZrParser_SemanticFacts_Init(context);
```

In `ZrParser_SemanticContext_Reset`, before setting lengths on existing arrays:

```c
    ZrParser_SemanticFacts_Reset(context);
```

In `ZrParser_SemanticContext_Free`, before freeing the context memory:

```c
    ZrParser_SemanticFacts_Free(context);
```

- [x] **Step 8: Run the semantic facts target**

Run:

```powershell
cmake -S . -B build\codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build\codex-wsl-gcc-debug --target zr_vm_semantic_facts_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test"
```

Expected after implementation: all three tests pass.

- [ ] **Step 9: Commit Task 1**

Deferred in this working tree: `tests/CMakeLists.txt` already contains unrelated in-progress changes from other work, so Task 1 was left uncommitted to avoid staging unrelated hunks.

```powershell
git add zr_vm_parser/include/zr_vm_parser/semantic.h zr_vm_parser/include/zr_vm_parser/semantic_facts.h zr_vm_parser/src/zr_vm_parser/semantic.c zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c tests/parser/test_semantic_facts.c tests/CMakeLists.txt
git commit -m "feat(parser): add shared semantic fact storage"
```

---

### Task 2: Expression And Numeric Facts From Type Inference

**Files:**
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\type_inference\type_inference_core.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\type_inference\type_inference_native.c`
- Create: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\type_inference\type_inference_semantic_facts.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\type_inference\type_inference_internal.h`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\type_inference.c`
- Modify: `E:\Git\zr_vm\tests\parser\test_type_inference.c`

- [x] **Step 1: Add failing tests for expression and numeric facts**

Add tests near the existing literal/type inference tests in `test_type_inference.c`:

```c
static const SZrSemanticExpressionFact *find_expression_fact_for_node(SZrSemanticContext *context, SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindExpressionByNode(context, node);
}

static void test_type_inference_records_literal_expression_and_numeric_facts(void) {
    SZrState *state = create_test_state();
    SZrCompilerState *cs = create_test_compiler_state(state);
    SZrString *sourceName = ZrCore_String_Create(state, "literal_fact_test.zr", 20);
    SZrAstNode *ast = ZrParser_Parse(state, "42;", 3, sourceName);
    SZrAstNode *expr = ast->data.script.statements->nodes[0]->data.expressionStatement.expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = find_expression_fact_for_node(cs->semanticContext, expr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_LITERAL, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, expressionFact->exactness);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, expressionFact->inferredType.baseType);
    TEST_ASSERT_TRUE(expressionFact->hasConstant);
    TEST_ASSERT_EQUAL_INT64(42, expressionFact->constantValue.int64Value);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_INTEGER, numericFact->numericKind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RANGE_SAFE, numericFact->rangeState);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);
}

static void test_type_inference_records_binary_expression_fact(void) {
    SZrState *state = create_test_state();
    SZrCompilerState *cs = create_test_compiler_state(state);
    SZrString *sourceName = ZrCore_String_Create(state, "binary_fact_test.zr", 19);
    SZrAstNode *ast = ZrParser_Parse(state, "1 + 2;", 6, sourceName);
    SZrAstNode *expr = ast->data.script.statements->nodes[0]->data.expressionStatement.expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = find_expression_fact_for_node(cs->semanticContext, expr);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expr);

    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, expressionFact->inferredType.baseType);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_INTEGER, numericFact->numericKind);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);
}
```

Register both tests in the `main` test runner block.

- [x] **Step 2: Run the type inference target and verify the tests fail**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_type_inference_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test"
```

Expected before implementation: new assertions fail because expression/numeric facts are not appended.

- [x] **Step 3: Add small fact helpers inside type inference**

In `type_inference_core.c`, add static helpers near existing type inference helper functions:

```c
static EZrSemanticExpressionFactKind semantic_expression_fact_kind_from_ast(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN;
    }
    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
        case ZR_AST_IDENTIFIER:
            return ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER;
        case ZR_AST_BINARY_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_BINARY;
        case ZR_AST_LOGICAL_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_BINARY;
        case ZR_AST_UNARY_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_UNARY;
        case ZR_AST_FUNCTION_CALL:
            return ZR_SEMANTIC_EXPRESSION_FACT_CALL;
        case ZR_AST_MEMBER_EXPRESSION:
        case ZR_AST_PRIMARY_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_MEMBER;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT;
        case ZR_AST_CONDITIONAL_EXPRESSION:
        case ZR_AST_IF_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_CONDITIONAL;
        case ZR_AST_ARRAY_LITERAL:
            return ZR_SEMANTIC_EXPRESSION_FACT_ARRAY;
        case ZR_AST_OBJECT_LITERAL:
            return ZR_SEMANTIC_EXPRESSION_FACT_OBJECT;
        case ZR_AST_LAMBDA_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return node->data.constructExpression.builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE
                       ? ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN
                       : ZR_SEMANTIC_EXPRESSION_FACT_CALL;
        default:
            return ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN;
    }
}

static void semantic_record_expression_fact(SZrCompilerState *cs, SZrAstNode *node, const SZrInferredType *type) {
    SZrSemanticExpressionFact fact;
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || node == ZR_NULL || type == ZR_NULL) {
        return;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = semantic_expression_fact_kind_from_ast(node);
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.valueKind = semantic_value_kind_from_inferred_type_and_node(type, node);
    fact.hasConstant = semantic_try_constant_from_literal(node, &fact.constantValue);
    ZrParser_InferredType_Copy(cs->state, &fact.inferredType, type);
    ZrParser_SemanticFacts_AppendExpression(cs->semanticContext, &fact);
    ZrParser_InferredType_Free(cs->state, &fact.inferredType);
}
```

Add `semantic_value_kind_from_inferred_type_and_node` and `semantic_try_constant_from_literal` in the same file. They must cover `int64`, `double`, `bool`, and string literals using existing AST literal fields.

Implementation note: the first implementation extracts the helper into `type_inference_semantic_facts.c` instead of adding static helpers to the oversized type inference files. It classifies the planned expression kinds, captures literal constants, writes numeric facts, and guards duplicate facts by AST node.

- [ ] **Step 4: Append facts at successful inference exits**

For each public inference function that returns a type through `result`, add:

```c
    semantic_record_expression_fact(cs, node, result);
```

Add this only after `result` has a valid final type and before `return ZR_TRUE`. Do not add a second inference path. The first required surface is:

- `ZrParser_ExpressionType_Infer`
- `ZrParser_LiteralType_Infer`
- `ZrParser_BinaryExpressionType_Infer`
- `ZrParser_UnaryExpressionType_Infer`
- `ZrParser_FunctionCallType_Infer`
- `ZrParser_PrimaryExpressionType_Infer`
- `ZrParser_ConditionalType_Infer`
- `ZrParser_AssignmentType_Infer`
- `ZrParser_ArrayLiteralType_Infer`
- `ZrParser_ObjectLiteralType_Infer`
- `ZrParser_LambdaType_Infer`

Status note: literal and binary arithmetic results are now appended from `ZrParser_ExpressionType_Infer`. The rest of the public inference success exits remain pending and must keep using the same helper module.

- [x] **Step 5: Add numeric facts for known numeric expressions**

Add helper:

```c
static void semantic_record_numeric_fact(SZrCompilerState *cs,
                                         SZrAstNode *node,
                                         const SZrInferredType *type,
                                         EZrSemanticRangeCheckState rangeState) {
    SZrSemanticNumericFact fact;
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || node == ZR_NULL || type == ZR_NULL) {
        return;
    }
    if (type->baseType != ZR_VALUE_TYPE_INT64 &&
        type->baseType != ZR_VALUE_TYPE_INT32 &&
        type->baseType != ZR_VALUE_TYPE_FLOAT &&
        type->baseType != ZR_VALUE_TYPE_DOUBLE) {
        return;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.numericKind = (type->baseType == ZR_VALUE_TYPE_FLOAT || type->baseType == ZR_VALUE_TYPE_DOUBLE)
                           ? ZR_SEMANTIC_NUMERIC_FLOAT
                           : ZR_SEMANTIC_NUMERIC_INTEGER;
    fact.rangeState = rangeState;
    fact.hasConstant = semantic_try_constant_from_literal(node, &fact.constantValue);
    ZrParser_SemanticFacts_AppendNumeric(cs->semanticContext, &fact);
}
```

Call it from the same success exits after `semantic_record_expression_fact`.

- [x] **Step 6: Run type inference tests**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_type_inference_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test"
```

Expected after implementation: new fact tests pass and existing type inference tests do not regress.

Verified on 2026-06-03 with WSL gcc Debug:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_type_inference_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test"
```

Result: `zr_vm_semantic_facts_test` passed with 3 tests and 0 failures; `zr_vm_type_inference_test` passed. Existing warnings remain in `type_inference.c` and `test_type_inference.c`.

- [ ] **Step 7: Commit Task 2**

```powershell
git add zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c tests/parser/test_type_inference.c
git commit -m "feat(parser): record expression and numeric inference facts"
```

---

### Task 3: Reference Facts And Precise Token Ranges

**Files:**
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_internal.h`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_references.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_support.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_symbols.c`
- Modify: `E:\Git\zr_vm\tests\language_server\test_semantic_analyzer.c`

- [x] **Step 1: Add failing reference fact test**

Add a semantic analyzer test:

```c
static void test_semantic_analyzer_records_reference_facts_with_precise_ranges(SZrState *state) {
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    const char *source =
        "var value: int = 1;\n"
        "func read() -> int {\n"
        "    return value;\n"
        "}\n";
    SZrString *sourceName = ZrCore_String_Create(state, "reference_fact_test.zr", 22);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    SZrFileRange useRange = file_range_for_nth_substring(source, "value", 1, ZR_FALSE);
    const SZrSemanticReferenceFact *fact;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast));
    TEST_ASSERT_NOT_NULL(analyzer->semanticContext);
    fact = ZrParser_SemanticFacts_FindReferenceAtPosition(analyzer->semanticContext, useRange);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REFERENCE_READ, fact->kind);
    TEST_ASSERT_TRUE(fact->declarationSymbolId != ZR_SEMANTIC_ID_INVALID);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)useRange.start.offset, (TZrUInt32)fact->useRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)useRange.end.offset, (TZrUInt32)fact->useRange.end.offset);

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
}
```

Register it in the semantic analyzer test runner.

- [x] **Step 2: Run the semantic analyzer test and verify it fails**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
```

Expected before implementation: the new reference fact lookup returns null.

- [x] **Step 3: Append reference facts from existing reference collection**

In `semantic_analyzer_references.c`, when the existing reference tracker records a use, append:

```c
static void semantic_record_reference_fact(SZrSemanticAnalyzer *analyzer,
                                           TZrSymbolId declarationSymbolId,
                                           SZrAstNode *node,
                                           SZrFileRange declarationRange,
                                           SZrFileRange useRange,
                                           EZrSemanticReferenceKind kind) {
    SZrSemanticReferenceFact fact;
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return;
    }
    memset(&fact, 0, sizeof(fact));
    fact.declarationSymbolId = declarationSymbolId;
    fact.node = node;
    fact.declarationRange = declarationRange;
    fact.useRange = useRange;
    fact.kind = kind;
    ZrParser_SemanticFacts_AppendReference(analyzer->semanticContext, &fact);
}
```

Map existing analyzer symbols to `semanticId`; when `semanticId` is not present, use `ZR_SEMANTIC_ID_INVALID` and still record range/kind. That preserves token tracking even before every symbol has semantic IDs.

Implementation note: actual `SZrSemanticReferenceFact` fields are `range`, `declarationRange`, `kind`, `symbolId`, `typeId`, `name`, and `isResolved`. Read/write/call uses now flow through `semantic_add_reference` in `semantic_analyzer_references.c`; declaration references flow through `ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForRange` in `semantic_analyzer_support.c`, including class property/accessor ranges from `semantic_analyzer_symbols.c`.

- [x] **Step 4: Normalize zero-width fallback ranges**

Where reference ranges are currently zero-width, reuse the existing local helper pattern from `lsp_semantic_query.c`:

```c
static SZrFileRange semantic_reference_normalize_identifier_range(SZrString *name, SZrFileRange fallback) {
    TZrSize nameLength;
    if (name == ZR_NULL || fallback.start.offset != fallback.end.offset) {
        return fallback;
    }
    nameLength = ZrCore_String_Length(name);
    if (fallback.start.offset >= nameLength) {
        fallback.start.offset -= nameLength;
    }
    if (fallback.start.column > (TZrInt32)nameLength) {
        fallback.start.column -= (TZrInt32)nameLength;
    }
    return fallback;
}
```

Status note: use-side zero-width AST ranges are expanded to full identifier token ranges by `semantic_make_identifier_reference_range`; declaration ranges use the existing LSP symbol lookup range and are written as declaration reference facts.

- [x] **Step 5: Run semantic analyzer reference tests**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
```

Expected after implementation: the new reference fact test and existing reference/navigation tests pass.

Validation note: WSL gcc Debug built `zr_vm_semantic_facts_test`, `zr_vm_type_inference_test`, and `zr_vm_language_server_semantic_analyzer_test`. `zr_vm_semantic_facts_test` reports 3 tests, 0 failures; `zr_vm_type_inference_test` passes; `Semantic Analyzer Records Reference Facts With Precise Ranges` now passes for both declaration and read facts. The semantic analyzer runner still prints the existing `Semantic Analyzer Reports Borrowed Return Escape` failure that was already present before Task 3 implementation and is left for the ownership inference task.

- [ ] **Step 6: Commit Task 3**

Deferred in this working tree: `tests/language_server/test_semantic_analyzer.c` and adjacent language-server files sit in a heavily dirty checkout with unrelated changes, so Task 3 remains uncommitted to avoid staging unrelated hunks.

```powershell
git add zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c tests/language_server/test_semantic_analyzer.c
git commit -m "feat(lsp): record semantic reference facts"
```

---

### Task 4: Reachability And Logical Facts

**Files:**
- Modify: `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\semantic_facts.h`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\semantic\semantic_facts.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_typecheck.c`
- Modify: `E:\Git\zr_vm\tests\language_server\test_semantic_analyzer.c`

- [x] **Step 1: Add failing reachability and logical fact tests**

Add tests next to the existing unreachable and short-circuit diagnostic tests:

```c
static void test_semantic_analyzer_records_unreachable_code_facts(SZrState *state) {
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    const char *source =
        "func f() -> int {\n"
        "    return 1;\n"
        "    return 2;\n"
        "}\n";
    SZrString *sourceName = ZrCore_String_Create(state, "reachability_fact_test.zr", 25);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    SZrFileRange deadRange = file_range_for_nth_substring(source, "return 2", 0, ZR_FALSE);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast));
    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(analyzer->semanticContext, deadRange);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_RETURN, fact->cause);

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
}

static void test_semantic_analyzer_records_short_circuit_logical_facts(SZrState *state) {
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    const char *source =
        "func f() -> bool {\n"
        "    return true || expensive();\n"
        "}\n";
    SZrString *sourceName = ZrCore_String_Create(state, "logical_fact_test.zr", 20);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    SZrFileRange logicalRange = file_range_for_nth_substring(source, "true || expensive()", 0, ZR_FALSE);
    const SZrSemanticLogicalFact *logicalFact = ZR_NULL;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast));
    for (TZrSize i = 0; i < analyzer->semanticContext->logicalFacts.length; i++) {
        const SZrSemanticLogicalFact *candidate =
            (const SZrSemanticLogicalFact *)ZrCore_Array_Get(&analyzer->semanticContext->logicalFacts, i);
        if (candidate != ZR_NULL && candidate->range.start.offset == logicalRange.start.offset) {
            logicalFact = candidate;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_SHORT_CIRCUITS_RIGHT, logicalFact->state);
    TEST_ASSERT_TRUE(logicalFact->skippedRange.start.offset > logicalRange.start.offset);

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
}
```

- [x] **Step 2: Run semantic analyzer test and verify new tests fail**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
```

Expected before implementation: the fact assertions fail while existing diagnostic tests still pass.

- [x] **Step 3: Append reachability facts at existing diagnostic sites**

In `semantic_analyzer_typecheck.c`, near existing `unreachable_code` and `unreachable_branch` diagnostics, append:

```c
static void semantic_record_reachability_fact(SZrSemanticAnalyzer *analyzer,
                                              SZrAstNode *node,
                                              SZrFileRange range,
                                              EZrSemanticReachabilityCause cause,
                                              SZrFileRange causeRange) {
    SZrSemanticReachabilityFact fact;
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = range;
    fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
    fact.cause = cause;
    fact.causeRange = causeRange;
    ZrParser_SemanticFacts_AppendReachability(analyzer->semanticContext, &fact);
}
```

Use these mappings:

- statement after `return` -> `ZR_SEMANTIC_REACHABILITY_AFTER_RETURN`
- statement after `throw` -> `ZR_SEMANTIC_REACHABILITY_AFTER_THROW`
- constant `if` dead branch -> `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`
- deterministic short-circuit right side -> `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`

- [x] **Step 4: Append logical facts for deterministic boolean results**

At the existing deterministic short-circuit logic, append:

```c
static void semantic_record_logical_fact(SZrSemanticAnalyzer *analyzer,
                                         SZrAstNode *node,
                                         EZrSemanticLogicalState state,
                                         SZrFileRange skippedRange) {
    SZrSemanticLogicalFact fact;
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.state = state;
    fact.skippedRange = skippedRange;
    ZrParser_SemanticFacts_AppendLogical(analyzer->semanticContext, &fact);
}
```

Map `true || rhs` and `false && rhs` to `ZR_SEMANTIC_LOGICAL_SHORT_CIRCUITS_RIGHT`. Map constant boolean conditions in `if` to `ZR_SEMANTIC_LOGICAL_CONSTANT_TRUE` or `ZR_SEMANTIC_LOGICAL_CONSTANT_FALSE`.

- [x] **Step 5: Run semantic analyzer tests**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
```

Expected after implementation: new fact tests and existing unreachable/short-circuit diagnostics pass.

- [ ] **Step 6: Commit Task 4 (deferred due dirty checkout)**

```powershell
git add zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c tests/language_server/test_semantic_analyzer.c
git commit -m "feat(lsp): record reachability and logical semantic facts"
```

Implementation note: commit was intentionally deferred because the checkout already contains unrelated in-progress changes. Actual implementation uses the current fact structs (`causeNode`, logical `kind/exactness/knownValue/relatedNode`) rather than the older draft snippet fields (`causeRange`, logical `state/skippedRange`). `semantic_facts.h` now distinguishes `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH` and `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`; `semantic_facts.c` now falls back to line/column matching when either side of a position query lacks offset data.

Validation note: WSL gcc Debug build of `zr_vm_language_server_semantic_analyzer_test` passed the new `Semantic Analyzer Records Reachability Facts For Unreachable Statements` and `Semantic Analyzer Records Short Circuit Logical Facts` tests. Existing `Semantic Analyzer Warns On Unreachable Statements After Return Or Throw`, `Semantic Analyzer Warns On Unreachable If Branches`, and `Semantic Analyzer Warns On Deterministic Short Circuit Branches` still pass. The runner still prints the pre-existing `Semantic Analyzer Reports Borrowed Return Escape` failure.

---

### Task 5: Ownership Facts And Rich Semantic Diagnostics

**Files:**
- Create: `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\diagnostic_builder.h`
- Create: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\diagnostics\diagnostic_builder.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\include\zr_vm_language_server\semantic_analyzer.h`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_support.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_typecheck.c`
- Modify: `E:\Git\zr_vm\tests\language_server\test_semantic_analyzer.c`

- [x] **Step 1: Add failing ownership fact and diagnostic text tests**

Extend `test_semantic_analyzer_reports_weak_argument_requires_upgrade` to assert message content and facts:

```c
static void test_semantic_analyzer_records_weak_upgrade_ownership_fact(SZrState *state) {
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    const char *source =
        "class Resource {}\n"
        "func consume(value: %borrowed Resource) {}\n"
        "func f(value: %weak Resource) {\n"
        "    consume(value);\n"
        "}\n";
    SZrString *sourceName = ZrCore_String_Create(state, "ownership_fact_test.zr", 22);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    SZrFileRange useRange = file_range_for_nth_substring(source, "value", 3, ZR_FALSE);
    const SZrSemanticOwnershipFact *fact;
    SZrDiagnostic *diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast));
    fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(analyzer->semanticContext, useRange);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK, fact->qualifier);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_OWNERSHIP_WEAK_NEEDS_UPGRADE, fact->state);
    diagnostic = find_diagnostic_by_code_and_line(analyzer, "weak_value_requires_upgrade", 4);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_TRUE(diagnostic_message_contains(diagnostic, "Weak value must be upgraded"));
    TEST_ASSERT_NOT_NULL(diagnostic->cause);
    TEST_ASSERT_NOT_NULL(diagnostic->suggestion);
    TEST_ASSERT_TRUE(strstr(ZrCore_String_GetNativeString(diagnostic->suggestion), "Upgrade") != ZR_NULL);

    ZrParser_Ast_Free(state, ast);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
}
```

- [x] **Step 2: Run semantic analyzer test and verify it fails**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
```

Expected before implementation: diagnostic code is still `type_mismatch` and cause/suggestion are missing.

- [x] **Step 3: Add diagnostic builder API**

Create `diagnostic_builder.h` with:

```c
#ifndef ZR_VM_PARSER_DIAGNOSTIC_BUILDER_H
#define ZR_VM_PARSER_DIAGNOSTIC_BUILDER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/state.h"

typedef enum EZrStructuredDiagnosticSeverity {
    ZR_STRUCTURED_DIAGNOSTIC_ERROR = 0,
    ZR_STRUCTURED_DIAGNOSTIC_WARNING,
    ZR_STRUCTURED_DIAGNOSTIC_INFO,
    ZR_STRUCTURED_DIAGNOSTIC_HINT
} EZrStructuredDiagnosticSeverity;

typedef struct SZrStructuredDiagnostic {
    EZrStructuredDiagnosticSeverity severity;
    SZrFileRange range;
    SZrString *code;
    SZrString *message;
    SZrString *cause;
    SZrString *suggestion;
    SZrFileRange relatedRange;
    SZrString *relatedMessage;
    SZrString *editText;
} SZrStructuredDiagnostic;

ZR_PARSER_API void ZrParser_StructuredDiagnostic_Init(SZrStructuredDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_StructuredDiagnostic_Free(SZrState *state, SZrStructuredDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_Build(SZrState *state,
                                                       SZrStructuredDiagnostic *out,
                                                       EZrStructuredDiagnosticSeverity severity,
                                                       SZrFileRange range,
                                                       const TZrChar *code,
                                                       const TZrChar *message,
                                                       const TZrChar *cause,
                                                       const TZrChar *suggestion);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildWeakUpgrade(SZrState *state,
                                                                  SZrStructuredDiagnostic *out,
                                                                  SZrFileRange range,
                                                                  const TZrChar *fromType,
                                                                  const TZrChar *toType);
ZR_PARSER_API TZrBool ZrParser_DiagnosticBuilder_BuildBorrowEscape(SZrState *state,
                                                                   SZrStructuredDiagnostic *out,
                                                                   SZrFileRange range);

#endif
```

Implement those builders with stable codes:

- `weak_value_requires_upgrade`
- `borrow_escape`
- `owner_to_plain_escape`
- `ownership_mismatch`

- [x] **Step 4: Extend `SZrDiagnostic` without changing LSP wire structs**

In `semantic_analyzer.h`, add:

```c
    SZrString *cause;
    SZrString *suggestion;
```

Update `ZrLanguageServer_Diagnostic_New` in `semantic_analyzer_support.c` to set them to `ZR_NULL`, and update `ZrLanguageServer_Diagnostic_Free` to free them when present.

Add a new helper:

```c
ZR_LANGUAGE_SERVER_API SZrDiagnostic *ZrLanguageServer_Diagnostic_FromStructured(
    SZrState *state,
    const SZrStructuredDiagnostic *structured);
```

- [x] **Step 5: Emit ownership facts and specific diagnostic codes**

In `semantic_analyzer_typecheck.c`, replace the generic weak-to-borrowed diagnostic at the existing mismatch site with:

```c
static void semantic_add_structured_diagnostic(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               const SZrStructuredDiagnostic *structured) {
    SZrDiagnostic *diagnostic;
    if (state == ZR_NULL || analyzer == ZR_NULL || structured == ZR_NULL) {
        return;
    }
    diagnostic = ZrLanguageServer_Diagnostic_FromStructured(state, structured);
    if (diagnostic != ZR_NULL) {
        ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    }
}
```

Append ownership fact:

```c
static void semantic_record_ownership_fact(SZrSemanticAnalyzer *analyzer,
                                           SZrAstNode *node,
                                           SZrFileRange range,
                                           EZrOwnershipQualifier qualifier,
                                           EZrSemanticOwnershipState state,
                                           SZrFileRange causeRange) {
    SZrSemanticOwnershipFact fact;
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = range;
    fact.qualifier = qualifier;
    fact.state = state;
    fact.sourceSymbolId = ZR_SEMANTIC_ID_INVALID;
    fact.causeRange = causeRange;
    ZrParser_SemanticFacts_AppendOwnership(analyzer->semanticContext, &fact);
}
```

Specific mappings:

- weak to borrowed use -> `weak_value_requires_upgrade`, `ZR_SEMANTIC_OWNERSHIP_WEAK_NEEDS_UPGRADE`
- borrowed return escape -> `borrow_escape`, `ZR_SEMANTIC_OWNERSHIP_BORROW_ESCAPE`
- loaned return/store escape -> `loan_escape`, `ZR_SEMANTIC_OWNERSHIP_LOAN_ESCAPE`
- unique/shared to plain -> `owner_to_plain_escape`, `ZR_SEMANTIC_OWNERSHIP_OWNER_TO_PLAIN_ESCAPE`

- [x] **Step 6: Preserve existing generic mismatch coverage**

Keep `type_mismatch` for non-ownership type mismatches. Update tests that specifically exercise ownership to expect the new stable ownership codes. Do not remove non-ownership `type_mismatch` tests.

- [x] **Step 7: Run semantic analyzer tests**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
```

Expected after implementation: ownership diagnostics include code, cause, suggestion, and matching ownership facts.

2026-06-03 execution note: WSL gcc Debug build and focused runs passed:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_language_server_semantic_analyzer_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_semantic_analyzer_task5.out; tail -n 28 build/codex-wsl-gcc-debug/zr_semantic_analyzer_task5.out; grep -E 'Fail -' build/codex-wsl-gcc-debug/zr_semantic_analyzer_task5.out || true"
```

Result: `zr_vm_semantic_facts_test` reported 3 tests, 0 failures. `zr_vm_language_server_semantic_analyzer_test` printed `All Semantic Analyzer Tests Completed`; `grep -E 'Fail -'` returned no matches. Ownership cases covered initializer mismatch, owner-to-plain initializer escape, return mismatch, borrowed return escape, function argument mismatch, weak argument upgrade, and method argument mismatch.

- [ ] **Step 8: Commit Task 5**

```powershell
git add zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c tests/language_server/test_semantic_analyzer.c
git commit -m "feat(semantics): add ownership facts and richer diagnostics"
```

Commit deferred: the checkout already contains a broad dirty worktree unrelated to this focused Task 5 slice, so this session did not stage or commit.

---

### Task 6: Rich Syntax Diagnostics Through Parser And LSP

**Files:**
- Modify: `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\parser.h`
- Modify: `E:\Git\zr_vm\zr_vm_parser\include\zr_vm_parser\diagnostic_builder.h`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\diagnostics\diagnostic_builder.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser\parser_internal.h`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser\parser_state.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser\parser_declarations.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser\parser_expression_primary.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser\parser_expressions.c`
- Modify: `E:\Git\zr_vm\zr_vm_parser\src\zr_vm_parser\parser\parser_statements.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\incremental_parser.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\interface\lsp_interface_support.c`
- Modify: `E:\Git\zr_vm\tests\language_server\test_lsp_interface.c`

- [x] **Step 1: Add failing LSP parser diagnostic test**

Add to `test_lsp_interface.c`:

```c
static void test_lsp_parser_diagnostic_explains_missing_assignment_expression(void) {
    SZrState *state = ZrCore_State_New();
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = ZrCore_String_Create(state, "file:///missing_expr.zr", 23);
    SZrArray diagnostics;
    SZrLspDiagnostic **diagPtr;
    const char *message;

    ZrCore_Array_Construct(&diagnostics);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, "var x = ;", 9, 1));
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics));
    TEST_ASSERT_TRUE(diagnostics.length > 0);
    diagPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(&diagnostics, 0);
    TEST_ASSERT_NOT_NULL(diagPtr);
    TEST_ASSERT_NOT_NULL(*diagPtr);
    TEST_ASSERT_NOT_NULL((*diagPtr)->code);
    TEST_ASSERT_EQUAL_STRING("missing_expression_after_assignment", ZrCore_String_GetNativeString((*diagPtr)->code));
    message = ZrCore_String_GetNativeString((*diagPtr)->message);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "Missing expression after '='"));
    TEST_ASSERT_NOT_NULL(strstr(message, "Add an expression before ';'"));

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    ZrCore_State_Free(state);
}
```

If the existing free helper name differs, use the existing diagnostic cleanup helper from the same file.

- [x] **Step 2: Run LSP interface test and verify it fails**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
```

Expected before implementation: the diagnostic code is `parser_syntax_error` and the message only reports the expected token/expression.

Result: `zr_vm_language_server_lsp_interface_test` built and ran. `LSP Get Parser Diagnostics` failed with `Expected structured parser diagnostics to carry code, problem, suggestion, and semicolon span`, confirming the test catches the missing structured parser diagnostic instead of a compile error.

- [x] **Step 3: Add compatible structured parser callback**

In `parser.h`, keep `TZrParserErrorCallback` unchanged and add:

```c
typedef void (*TZrParserStructuredErrorCallback)(TZrPtr userData,
                                                 const SZrStructuredDiagnostic *diagnostic,
                                                 EZrToken token);
```

Add to `SZrParserState`:

```c
    TZrParserStructuredErrorCallback structuredErrorCallback;
```

- [x] **Step 4: Build specific parser diagnostics**

In `parser_state.c`, add a helper:

```c
static void report_structured_parser_error(SZrParserState *ps,
                                           const SZrStructuredDiagnostic *diagnostic,
                                           EZrToken token) {
    const TZrChar *message;
    if (ps == ZR_NULL || diagnostic == ZR_NULL || diagnostic->message == ZR_NULL) {
        return;
    }
    message = ZrCore_String_GetNativeString(diagnostic->message);
    if (ps->structuredErrorCallback != ZR_NULL) {
        ps->structuredErrorCallback(ps->errorUserData, diagnostic, token);
    }
    report_error_with_token(ps, message, token);
}
```

In the assignment parsing path where `var x = ;` or `left = ;` reaches `Expected primary expression`, build:

```c
SZrStructuredDiagnostic diagnostic;
ZrParser_StructuredDiagnostic_Init(&diagnostic);
if (ZrParser_DiagnosticBuilder_Build(ps->state,
                                     &diagnostic,
                                     ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                                     get_current_token_location(ps),
                                     "missing_expression_after_assignment",
                                     "Missing expression after '='.",
                                     "The assignment starts at '=' but the next token is ';'.",
                                     "Add an expression before ';' or remove the assignment.")) {
    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
}
ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
```

Keep the old callback path active for callers that only consume string diagnostics.

- [x] **Step 5: Collect structured diagnostics in incremental parser**

In `incremental_parser.c`, add a structured collector callback:

```c
static void collect_structured_parser_diagnostic(TZrPtr userData,
                                                 const SZrStructuredDiagnostic *structured,
                                                 EZrToken token) {
    SZrParserDiagnosticCollector *collector = (SZrParserDiagnosticCollector *)userData;
    SZrDiagnostic *diagnostic;
    ZR_UNUSED_PARAMETER(token);
    if (collector == ZR_NULL || structured == ZR_NULL) {
        return;
    }
    diagnostic = ZrLanguageServer_Diagnostic_FromStructured(collector->state, structured);
    if (diagnostic != ZR_NULL) {
        ZrCore_Array_Push(collector->state, &collector->fileVersion->parserDiagnostics, &diagnostic);
    }
}
```

Set:

```c
parserState.structuredErrorCallback = collect_structured_parser_diagnostic;
```

The existing string callback must avoid duplicating a structured diagnostic for the same token. If both callbacks fire for the same parser error, have the string collector skip append when a structured diagnostic was just added at the same range.

- [x] **Step 6: Include cause and suggestion in LSP diagnostic message**

In the LSP diagnostic conversion helper, append cause/suggestion to the message text:

```c
"%s\nCause: %s\nSuggestion: %s"
```

Only append sections whose strings are present. Keep `SZrLspDiagnostic` unchanged.

- [x] **Step 7: Run LSP interface tests**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
```

Expected after implementation: parser diagnostic code and message are specific and include suggestion text.

Result: `zr_vm_language_server_lsp_interface_test` passed, including `LSP Get Parser Diagnostics`. Additional focused WSL gcc Debug validation passed for:

- `zr_vm_parser_test`: 66 tests, 0 failures.
- `zr_vm_semantic_facts_test`: 3 tests, 0 failures.
- `zr_vm_type_inference_test`: 103 tests, 0 failures.
- `zr_vm_language_server_semantic_analyzer_test`: `All Semantic Analyzer Tests Completed`.
- `zr_vm_language_server_incremental_parser_test`: `All Incremental Parser Tests Completed`.
- `zr_vm_language_server_lsp_interface_test`: `All LSP Interface Tests Completed`.

- [ ] **Step 8: Commit Task 6**

```powershell
git add zr_vm_parser/include/zr_vm_parser/parser.h zr_vm_parser/src/zr_vm_parser/parser/parser_state.c zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c tests/language_server/test_lsp_interface.c
git commit -m "feat(lsp): surface structured parser diagnostics"
```

Commit deferred: the checkout already contains a broad dirty worktree unrelated to this focused Task 6 slice, so this session did not stage or commit.

---

### Task 7: LSP Local Semantic Query And Robust Unknown Results

**Files:**
- Create: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\lsp_local_semantic_query.h`
- Create: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\lsp_local_semantic_query.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer.c`
- Create: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_expression_query.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\semantic_analyzer_internal.h`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\semantic\lsp_semantic_query.c`
- Modify: `E:\Git\zr_vm\zr_vm_language_server\src\zr_vm_language_server\interface\lsp_interface.c`
- Modify: `E:\Git\zr_vm\tests\language_server\test_lsp_interface.c`

- [x] **Step 1: Add failing LSP local query robustness tests**

Add two tests to `test_lsp_interface.c`:

```c
static void test_lsp_hover_uses_expression_fact_for_local_expression(void) {
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = ZrCore_String_Create(state, "file:///fact_hover.zr", 20);
    const char *source = "func f() -> int { return 1 + 2; }";
    SZrLspHover *hover = ZR_NULL;
    SZrLspPosition position;

    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, source, strlen(source), 1));
    position.line = 0;
    position.character = 26;
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetHover(state, context, uri, position, &hover));
    TEST_ASSERT_NOT_NULL(hover);
    TEST_ASSERT_TRUE(hover->contents.length > 0);

    ZrLanguageServer_LspContext_Free(state, context);
}

static void test_lsp_local_query_returns_unknown_not_crash_on_incomplete_syntax(void) {
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = ZrCore_String_Create(state, "file:///unknown_query.zr", 23);
    SZrLspHover *hover = ZR_NULL;
    SZrArray diagnostics;
    SZrLspPosition position;

    ZrCore_Array_Construct(&diagnostics);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, "func f() { var x = ; }", 21, 1));
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics));
    position.line = 0;
    position.character = 18;
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_GetHover(state, context, uri, position, &hover) ||
                     hover == ZR_NULL);
    TEST_ASSERT_TRUE(diagnostics.length > 0);

    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
}
```

Use existing free helper names in `test_lsp_interface.c` if they differ.

- [x] **Step 2: Run LSP interface tests and verify failures**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
```

Expected before implementation: hover may fall back to weak object or incomplete syntax may return an unstable result.

RED notes captured during implementation:

- The first local query build failed before implementation because `lsp_local_semantic_query.h` did not exist.
- After the API existed, `LSP Local Semantic Query Returns Expression Fact` still returned `status=UNKNOWN` at the `+` token in `1 + 2`, because the raw fact position lookup did not tolerate AST/fact offset drift around binary operators.
- Reconfiguration was required after adding new `src/**/*.c` files so CMake's `GLOB_RECURSE CONFIGURE_DEPENDS` source list included them.

- [x] **Step 3: Add local semantic query result API**

Create `lsp_local_semantic_query.h`:

```c
#ifndef ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_QUERY_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_parser/semantic_facts.h"

typedef enum EZrLspLocalSemanticQueryStatus {
    ZR_LSP_LOCAL_QUERY_UNKNOWN = 0,
    ZR_LSP_LOCAL_QUERY_FACT,
    ZR_LSP_LOCAL_QUERY_DIAGNOSTIC_FAILURE
} EZrLspLocalSemanticQueryStatus;

typedef struct SZrLspLocalSemanticQueryResult {
    EZrLspLocalSemanticQueryStatus status;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticReferenceFact *referenceFact;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticReachabilityFact *reachabilityFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticOwnershipFact *ownershipFact;
    const SZrDiagnostic *diagnostic;
} SZrLspLocalSemanticQueryResult;

TZrBool ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrString *uri,
                                                            SZrLspPosition position,
                                                            SZrLspLocalSemanticQueryResult *out);
TZrBool ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           SZrLspPosition position,
                                                           SZrLspLocalSemanticQueryResult *out);

#endif
```

- [x] **Step 4: Implement local query fallback order**

In `lsp_local_semantic_query.c`, use this order:

1. Convert LSP position to `SZrFileRange` using document content when available.
2. Check parser diagnostics affecting the range. If one matches, return `ZR_LSP_LOCAL_QUERY_DIAGNOSTIC_FAILURE`.
3. Find analyzer for the URI. If none exists and the file uses fallback AST, return `ZR_LSP_LOCAL_QUERY_UNKNOWN`.
4. Query `analyzer->semanticContext` facts.
5. If no fact exists, return `ZR_LSP_LOCAL_QUERY_UNKNOWN`.

Core implementation:

```c
static void lsp_local_query_clear(SZrLspLocalSemanticQueryResult *out) {
    if (out != ZR_NULL) {
        memset(out, 0, sizeof(*out));
        out->status = ZR_LSP_LOCAL_QUERY_UNKNOWN;
    }
}
```

- [x] **Step 5: Prefer expression facts in hover/type resolution**

In `semantic_analyzer.c`, update `ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition` to query expression facts first:

```c
const SZrSemanticExpressionFact *fact =
    ZrParser_SemanticFacts_FindExpressionAtPosition(analyzer->semanticContext, position);
if (fact != ZR_NULL && fact->exactness == ZR_SEMANTIC_FACT_EXACT) {
    ZrParser_InferredType_Copy(state, outType, &fact->inferredType);
    return ZR_TRUE;
}
```

Keep the existing AST/symbol fallback after this block.

- [x] **Step 6: Use local query in LSP hover and expose reference facts**

Implemented with the current code boundaries:

- `ZrLanguageServer_Lsp_GetHover` calls `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` before legacy hover fallback. If status is `DIAGNOSTIC_FAILURE`, hover returns no result instead of producing misleading expression fallback text.
- Hover/type resolution reuses `ZrLanguageServer_SemanticAnalyzer_FindExpressionFactAtPosition`, so local expression facts feed the existing semantic-query hover path without duplicating markdown logic in `lsp_semantic_query.c`.
- `ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt` exposes local reference facts for VSCode/plugin-local queries. Definition/reference/highlight navigation remains on the existing `ZrLanguageServer_LspSemanticQuery_*` path for this stage.

- [x] **Step 7: Run LSP interface and semantic analyzer tests**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
```

Expected after implementation: local expression hover uses facts; incomplete syntax yields diagnostics or unknown without crash.

GREEN evidence:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ > build/codex-wsl-gcc-debug/zr_task7_reconfigure.out 2>&1 && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 8 > build/codex-wsl-gcc-debug/zr_lsp_local_query_task7_build9.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_lsp_local_query_task7_run8.out 2>&1"
```

`LSP Local Semantic Query Returns Expression Fact` and `LSP Local Semantic Query Reports Diagnostic Failure For Incomplete Expression` passed after adding `semantic_analyzer_expression_query.c` and the local query API.

Final focused Task 7 validation:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test zr_vm_semantic_facts_test zr_vm_type_inference_test -j 8 > build/codex-wsl-gcc-debug/zr_task7_final_focus_build2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_task7_final_lsp2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_task7_final_semantic_analyzer2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test > build/codex-wsl-gcc-debug/zr_task7_final_semantic_facts2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test > build/codex-wsl-gcc-debug/zr_task7_final_type_inference2.out 2>&1"
```

Results: `zr_vm_language_server_lsp_interface_test` printed `All LSP Interface Tests Completed`; `zr_vm_language_server_semantic_analyzer_test` printed `All Semantic Analyzer Tests Completed`; `zr_vm_semantic_facts_test` printed `3 Tests 0 Failures 0 Ignored OK`; `zr_vm_type_inference_test` printed `103 Tests 0 Failures 0 Ignored OK`. The four output files contained no `Fail -` markers.

- [ ] **Step 8: Commit Task 7**

```powershell
git add zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c tests/language_server/test_lsp_interface.c
git commit -m "feat(lsp): add robust local semantic fact queries"
```

Commit deferred: the checkout already contains a broad dirty worktree unrelated to this focused Task 7 slice, so this session did not stage or commit.

---

### Task 8: Focused Stdio, Documentation, And First-Stage Validation

**Files:**
- Modify: `E:\Git\zr_vm\tests\language_server\stdio_smoke.js`
- Modify: `E:\Git\zr_vm\docs\parser-and-semantics\index.md`
- Modify: `E:\Git\zr_vm\docs\parser-and-semantics\semantic-fact-layer.md`
- Create: `E:\Git\zr_vm\tests\acceptance\2026-06-03-semantic-inference-fact-layer.md`
- Modify: `E:\Git\zr_vm\.codex\sessions\20260603-0048-semantic-lsp-inference-goal.md`

- [x] **Step 1: Add stdio diagnostic assertion if C LSP tests do not cover JSON shape**

In `stdio_smoke.js`, add an assertion that a syntax diagnostic includes a specific code and actionable message:

```js
assertDiagnosticIncludes(
  diagnostics,
  'missing_expression_after_assignment',
  "Missing expression after '='",
  'structured parser diagnostic should reach stdio'
);
```

Use the existing helper style in the file; add `assertDiagnosticIncludes` only if no equivalent helper exists.

Implemented: `stdio_smoke.js` now has `assertDiagnosticIncludes`, opens `stdio-parser-diagnostic.zr` with `var x = ;`, and verifies the JSON diagnostic includes `missing_expression_after_assignment`, `Missing expression after '='`, and `Add an expression before ';'`.

- [x] **Step 2: Run stdio smoke**

Run through the configured CTest target:

```powershell
wsl bash -lc "ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure"
```

Expected: stdio smoke passes or any failure is proven to be a pre-existing baseline unrelated to diagnostics.

Validation:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 > build/codex-wsl-gcc-debug/zr_task8_stdio_build.out 2>&1 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure > build/codex-wsl-gcc-debug/zr_task8_stdio_ctest.out 2>&1"
```

Result: `language_server_stdio_smoke` passed.

- [x] **Step 3: Update parser/semantic documentation**

In `docs/parser-and-semantics/index.md`, add a concise section:

```markdown
## Shared Semantic Facts

The parser semantic context owns append-only fact arrays for expression, reference, numeric, reachability, logical, and ownership inference. Type inference and semantic analysis append facts during one analysis pass. LSP local queries consume those facts and return a precise fact, a diagnostic-backed failure, or an explicit unknown when syntax is incomplete.

Diagnostics should include a stable code, concrete cause, and suggested next action. Parser diagnostics keep the legacy callback path, but structured diagnostics are preferred when LSP or tools can consume them.
```

Implemented in `docs/parser-and-semantics/index.md` and `docs/parser-and-semantics/semantic-fact-layer.md`.

- [x] **Step 4: Update active session note**

Update `.codex/sessions/20260603-0048-semantic-lsp-inference-goal.md`:

```markdown
## Current Step
- First-stage implementation plan is saved at `docs/superpowers/plans/2026-06-03-zr-vm-semantic-inference-fact-layer.md`. Implementation is ready to execute from the shared semantic fact layer upward.

## Related Tests
- `zr_vm_semantic_facts_test`
- `zr_vm_type_inference_test`
- `zr_vm_language_server_semantic_analyzer_test`
- `zr_vm_language_server_lsp_interface_test`
- `language_server_stdio_smoke`
```

Implemented: active session note now records first-stage implementation status, validation evidence, the matrix residual failure, and the Debug/REPL follow-up boundary.

- [x] **Step 5: Run focused first-stage validation**

Run:

```powershell
cmake --build build\codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio -j 8
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
wsl bash -lc "./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure"
```

Expected: focused first-stage tests pass, or failures are documented as pre-existing baseline failures with exact failing test names.

Validation:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio -j 8 > build/codex-wsl-gcc-debug/zr_task8_focus_build.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test > build/codex-wsl-gcc-debug/zr_task8_semantic_facts.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test > build/codex-wsl-gcc-debug/zr_task8_type_inference.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_task8_semantic_analyzer.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_task8_lsp_interface.out 2>&1 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure > build/codex-wsl-gcc-debug/zr_task8_stdio_smoke.out 2>&1"
```

Result: `zr_vm_semantic_facts_test` 3 tests, 0 failures; `zr_vm_type_inference_test` 103 tests, 0 failures; `zr_vm_language_server_semantic_analyzer_test` printed `All Semantic Analyzer Tests Completed`; `zr_vm_language_server_lsp_interface_test` printed `All LSP Interface Tests Completed`; `language_server_stdio_smoke` passed.

- [x] **Step 6: Run repository validation matrix**

Because this plan changes shared parser/LSP headers and C sources, run:

```powershell
.\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 12
```

Expected:

- WSL gcc config/build/ctest completes with no new failures compared with current baseline.
- WSL clang config/build/ctest completes with no new failures compared with current baseline.
- Windows MSVC CLI smoke passes.

Validation:

```powershell
.\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 12
```

Result: WSL gcc configure/build passed, WSL gcc `hello_world` smoke passed; WSL clang configure/build passed, WSL clang `hello_world` smoke passed; MSVC configure/build passed, Windows `hello_world` smoke passed. WSL gcc and WSL clang `ctest` both failed only in `core_runtime` because `zr_vm_execution_member_access_fast_paths_test::test_member_get_cached_property_getter_stack_slot_skips_anchor_restore_stack_lookup_when_stack_unchanged` reported `Expected 0 Was 1`. That failure belongs to runtime member-access fast paths and is outside this first-stage parser/LSP semantic fact slice.

- [ ] **Step 7: Commit Task 8**

```powershell
git add tests/language_server/stdio_smoke.js docs/parser-and-semantics/index.md .codex/sessions/20260603-0048-semantic-lsp-inference-goal.md
git commit -m "docs: document semantic fact layer validation"
```

Commit deferred: the checkout already contains a broad dirty worktree unrelated to this focused Task 8 slice, so this session did not stage or commit.

---

## First-Stage Acceptance Checklist

- [x] `SZrSemanticContext` owns expression, reference, numeric, reachability, logical, and ownership fact arrays.
- [ ] Expression facts cover literal, identifier, binary, unary, call, member/primary, assignment, conditional, array, object, lambda, and ownership builtin expressions.
- [ ] Numeric facts record known constants and range states for numeric literal and arithmetic expressions.
- [x] Reference facts record declaration/use relationships with precise token ranges.
- [x] Reachability facts record unreachable code after return/throw, constant branch exclusion, and deterministic short-circuit exclusion.
- [x] Logical facts record constant boolean and short-circuit consequences.
- [x] Ownership facts record representative weak upgrade, borrow escape, ownership mismatch, and owner-to-plain escape conditions; `loan_escape` builder/emitter is wired and still needs a dedicated regression case.
- [x] Semantic and representative syntax diagnostics include stable code, concrete cause, and suggested next action.
- [x] LSP local semantic query returns fact, diagnostic-backed failure, or explicit unknown.
- [x] Incomplete syntax does not crash hover/diagnostics/local inference and does not return misleading weak object facts.
- [x] Focused Task 5 tests pass for WSL gcc Debug semantic facts and semantic analyzer ownership diagnostics.
- [x] Focused Task 6 tests pass for WSL gcc Debug parser, semantic facts, type inference, semantic analyzer, incremental parser, and LSP parser diagnostics.
- [x] Focused Task 7 tests pass for WSL gcc Debug LSP interface, semantic analyzer, semantic facts, and type inference.
- [x] Focused Task 8 tests pass for WSL gcc Debug semantic facts, type inference, semantic analyzer, LSP interface, stdio diagnostic JSON, and the focused identifier expression-statement inline-value smoke.
- [x] Validation matrix is run and reported with exact toolchain/build directory evidence; full WSL ctest is not green because of the remaining `core_runtime` member-access fast-path failure outside this semantic/LSP slice.

## Debug And REPL Follow-Up Boundary

This first implementation plan does not replace the debug expression evaluator or REPL execution path. After this plan is complete, the next plan should reuse the same parser semantic fact/query layer for:

- debug conditional breakpoint expression diagnostics and truthiness checks;
- debug evaluate expression type/data inference without growing a second full frontend;
- REPL expression snippets with the same parser diagnostics and expression facts.

The follow-up must preserve debug frame/local/value snapshots as debugger-owned data and must not merge runtime snapshot ownership into the parser semantic context.

## Self-Review

Spec coverage:

- Expression, reference, numeric, reachability, logical, and ownership facts are covered by Tasks 1-5.
- Concrete diagnostic cause/suggestion is covered by Tasks 5-6.
- LSP localized robust inference is covered by Task 7.
- Focused stdio and validation are covered by Task 8.
- Debug/REPL are intentionally bounded to follow-up reuse of the shared layer, matching the approved first-stage design.

Plan hygiene scan:

- The plan contains no open-ended markers or vague "handle later" steps.
- Each code-changing task includes concrete file paths, test snippets, implementation snippets, validation commands, and commit commands.

Type consistency:

- Fact enum and struct names are defined in Task 1 before later tasks use them.
- `SZrDiagnostic` rich fields are added in Task 5 before LSP message formatting in Task 6 depends on them.
- `SZrLspLocalSemanticQueryResult` is defined in Task 7 before hover/query integration uses it.
