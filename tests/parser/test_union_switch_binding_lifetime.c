/* union switch 覆盖校验对 resolver 输出 bindings 的所有权回归：struct payload pattern 的独立容器
 * 必须配对释放，tuple payload pattern 借用的 case AST 实参数组不得被释放。 */
#include "unity.h"

#include <string.h>

#include "compiler/statement/compile_statement_union_switch_validation.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

/* 只计量 ARRAY 类别的原生块：AstNodeArray 的外壳与 nodes 缓冲区都用该类别申请和释放。 */
typedef struct SArrayAllocationProbe {
    SZrGlobalState *global;
    FZrAllocator allocator;
    TZrPtr userData;
    TZrSize allocated;
    TZrSize freed;
    TZrSize allocatedBytes;
    TZrSize freedBytes;
} SArrayAllocationProbe;

/* 校验入口需要的借用视图：均指向 g_ast 内的节点，随 g_ast 一起失效。 */
typedef struct SUnionSwitchFixture {
    SZrAstNodeArray *statements;
    SZrAstNode *unionDeclaration;
    SZrAstNode *switchNode;
    SZrSwitchExpression *switchExpression;
    SZrString *unionTypeName;
} SUnionSwitchFixture;

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SArrayAllocationProbe g_probe;

static TZrPtr counting_allocator(TZrPtr userData, TZrPtr pointer,
        TZrSize oldSize, TZrSize newSize, TZrInt64 type) {
    SArrayAllocationProbe *probe = (SArrayAllocationProbe *)userData;
    TZrBool hadAllocation = pointer != ZR_NULL;
    TZrPtr result = probe->allocator(
            probe->userData, pointer, oldSize, newSize, type);

    if (type == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        if (hadAllocation && (newSize == 0U || result != ZR_NULL)) {
            probe->freed++;
            probe->freedBytes += oldSize;
        }
        if (result != ZR_NULL && newSize != 0U) {
            probe->allocated++;
            probe->allocatedBytes += newSize;
        }
    }
    return result;
}

static void end_probe(void) {
    if (g_probe.global != ZR_NULL) {
        g_probe.global->allocator = g_probe.allocator;
        g_probe.global->userAllocationArguments = g_probe.userData;
        g_probe.global = ZR_NULL;
    }
}

static void begin_probe(void) {
    memset(&g_probe, 0, sizeof(g_probe));
    g_probe.global = g_state->global;
    g_probe.allocator = g_state->global->allocator;
    g_probe.userData = g_state->global->userAllocationArguments;
    g_state->global->allocator = counting_allocator;
    g_state->global->userAllocationArguments = &g_probe;
}

static void clear_error_flags(void) {
    g_compiler.hasError = ZR_FALSE;
    g_compiler.hadRecoverableError = ZR_FALSE;
    g_compiler.errorMessage = ZR_NULL;
}

void setUp(void) {
    g_state = ZR_NULL;
    g_ast = ZR_NULL;
    g_compilerInitialized = ZR_FALSE;
    memset(&g_probe, 0, sizeof(g_probe));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    memset(&g_compiler, 0, sizeof(g_compiler));
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
}

/* 断言失败经 Unity longjmp 到达这里；探针、函数、编译状态、AST 与运行时按依赖顺序回收。 */
void tearDown(void) {
    end_probe();
    if (g_compilerInitialized) {
        if (g_compiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, g_compiler.currentFunction);
            g_compiler.currentFunction = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 源码固定为 union 声明、subject 变量、switch 三条语句；校验借 scriptAst 查找 union 声明。 */
static void load_union_switch(const TZrChar *source, SUnionSwitchFixture *fixture) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "union_switch_binding_lifetime.zr");

    TEST_ASSERT_NOT_NULL(sourceName);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    fixture->statements = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(fixture->statements);
    TEST_ASSERT_EQUAL_UINT32(3U, (TZrUInt32)fixture->statements->count);

    fixture->unionDeclaration = fixture->statements->nodes[0];
    fixture->switchNode = fixture->statements->nodes[2];
    TEST_ASSERT_NOT_NULL(fixture->unionDeclaration);
    TEST_ASSERT_NOT_NULL(fixture->switchNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNION_DECLARATION, fixture->unionDeclaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SWITCH_EXPRESSION, fixture->switchNode->type);
    fixture->switchExpression = &fixture->switchNode->data.switchExpression;
    TEST_ASSERT_NOT_NULL(fixture->switchExpression->cases);
    TEST_ASSERT_NOT_NULL(fixture->unionDeclaration->data.unionDeclaration.name);
    fixture->unionTypeName = fixture->unionDeclaration->data.unionDeclaration.name->name;
    TEST_ASSERT_NOT_NULL(fixture->unionTypeName);
    g_compiler.scriptAst = g_ast;
}

/* 与 compile_switch_statement 相同的顺序：duplicate 校验通过后才计算穷尽性。 */
static TZrBool run_validation(const SUnionSwitchFixture *fixture) {
    compile_switch_validate_union_duplicate_cases(&g_compiler,
                                                  fixture->switchExpression,
                                                  fixture->unionDeclaration,
                                                  fixture->unionTypeName);
    if (g_compiler.hasError) {
        return ZR_FALSE;
    }
    return compile_switch_validate_union_exhaustiveness(&g_compiler,
                                                        fixture->switchExpression,
                                                        fixture->unionDeclaration,
                                                        fixture->unionTypeName,
                                                        fixture->switchNode->location);
}

/* 先预热一轮再计量，排除与 bindings 无关的首次分配；返回被计量那一轮的穷尽性结果。 */
static TZrBool run_probed_validation(const SUnionSwitchFixture *fixture) {
    TZrBool exhaustive;

    (void)run_validation(fixture);
    clear_error_flags();
    begin_probe();
    exhaustive = run_validation(fixture);
    end_probe();
    return exhaustive;
}

/* 取第 caseIndex 个 case 的 tuple payload 调用节点；qualified 与 unqualified 写法的成员位置不同。 */
static SZrFunctionCall *case_payload_call(const SUnionSwitchFixture *fixture, TZrSize caseIndex) {
    SZrAstNode *caseNode;
    SZrAstNode *caseValue;
    SZrAstNodeArray *members;

    TEST_ASSERT_TRUE(caseIndex < fixture->switchExpression->cases->count);
    caseNode = fixture->switchExpression->cases->nodes[caseIndex];
    TEST_ASSERT_NOT_NULL(caseNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SWITCH_CASE, caseNode->type);
    caseValue = caseNode->data.switchCase.value;
    TEST_ASSERT_NOT_NULL(caseValue);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, caseValue->type);
    members = caseValue->data.primaryExpression.members;
    TEST_ASSERT_NOT_NULL(members);
    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];

        if (member != ZR_NULL && member->type == ZR_AST_FUNCTION_CALL) {
            return &member->data.functionCall;
        }
    }
    TEST_FAIL_MESSAGE("switch case has no tuple payload call");
    return ZR_NULL;
}

static void assert_struct_containers_released(void) {
    TEST_ASSERT_TRUE_MESSAGE(g_probe.allocated > 0U,
            "struct payload pattern must build an independent bindings container");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(g_probe.allocated, g_probe.freed,
            "coverage validation must release every bindings container it resolved");
    TEST_ASSERT_EQUAL_UINT64(g_probe.allocatedBytes, g_probe.freedBytes);
}

static void assert_struct_payload_switch_releases_bindings(const TZrChar *source) {
    SUnionSwitchFixture fixture;
    TZrBool exhaustive;

    load_union_switch(source, &fixture);
    exhaustive = run_probed_validation(&fixture);

    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(exhaustive);
    assert_struct_containers_released();
}

static void assert_tuple_payload_switch_keeps_case_arguments(const TZrChar *source) {
    SUnionSwitchFixture fixture;
    SZrFunctionCall *call;
    SZrAstNodeArray *arguments;
    SZrAstNode *binding;
    TZrBool exhaustive;

    load_union_switch(source, &fixture);
    call = case_payload_call(&fixture, 0U);
    arguments = call->args;
    TEST_ASSERT_NOT_NULL(arguments);
    TEST_ASSERT_EQUAL_UINT(1U, arguments->count);
    binding = arguments->nodes[0];
    TEST_ASSERT_NOT_NULL(binding);

    exhaustive = run_probed_validation(&fixture);

    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(exhaustive);
    /* 先看释放计数再读数组：被错误释放时由断言报告，不去读已释放的 AST 内存。 */
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0U, g_probe.freed,
            "tuple payload bindings are borrowed from the case AST and must not be released");
    TEST_ASSERT_EQUAL_UINT64(0U, g_probe.allocated);
    TEST_ASSERT_EQUAL_PTR(arguments, call->args);
    TEST_ASSERT_EQUAL_UINT(1U, arguments->count);
    TEST_ASSERT_EQUAL_PTR(binding, arguments->nodes[0]);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, binding->type);
    TEST_ASSERT_EQUAL_STRING("r",
            ZrCore_String_GetNativeString(binding->data.identifier.name));
}

static void assert_struct_payload_error_releases_bindings(const TZrChar *source,
        const TZrChar *expectedMessage) {
    SUnionSwitchFixture fixture;
    TZrBool exhaustive;

    load_union_switch(source, &fixture);
    exhaustive = run_probed_validation(&fixture);

    TEST_ASSERT_FALSE(exhaustive);
    TEST_ASSERT_TRUE(g_compiler.hasError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(g_compiler.errorMessage, expectedMessage),
            g_compiler.errorMessage);
    assert_struct_containers_released();
}

static void test_unqualified_struct_payload_bindings_are_released(void) {
    assert_struct_payload_switch_releases_bindings(
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "switch (s) {\n"
            "    (Rect { width: w, height: h }) { var area = w; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n");
}

static void test_qualified_struct_payload_bindings_are_released(void) {
    assert_struct_payload_switch_releases_bindings(
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "switch (s) {\n"
            "    (Shape.Rect { width: w, height: h }) { var area = w; }\n"
            "    (Shape.Empty) { var miss = 0; }\n"
            "}\n");
}

static void test_unqualified_tuple_payload_bindings_stay_owned_by_case_ast(void) {
    assert_tuple_payload_switch_keeps_case_arguments(
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s: Shape = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Circle(r)) { var hit = r; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n");
}

static void test_qualified_tuple_payload_bindings_stay_owned_by_case_ast(void) {
    assert_tuple_payload_switch_keeps_case_arguments(
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s: Shape = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Shape.Circle(r)) { var hit = r; }\n"
            "    (Shape.Empty) { var miss = 0; }\n"
            "}\n");
}

/* 同一 switch 混用两种 payload：释放次数必须恰好等于新建容器的次数，多放少放都不相等。 */
static void test_mixed_payload_switch_releases_only_struct_containers(void) {
    const TZrChar *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s: Shape = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Circle(r)) { var hit = r; }\n"
            "    (Rect { width: w, height: h }) { var area = w; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n";
    SUnionSwitchFixture fixture;
    SZrFunctionCall *call;
    SZrAstNodeArray *arguments;
    SZrAstNode *binding;
    TZrBool exhaustive;

    load_union_switch(source, &fixture);
    call = case_payload_call(&fixture, 0U);
    arguments = call->args;
    TEST_ASSERT_NOT_NULL(arguments);
    TEST_ASSERT_EQUAL_UINT(1U, arguments->count);
    binding = arguments->nodes[0];

    exhaustive = run_probed_validation(&fixture);

    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(exhaustive);
    assert_struct_containers_released();
    TEST_ASSERT_EQUAL_PTR(arguments, call->args);
    TEST_ASSERT_EQUAL_UINT(1U, arguments->count);
    TEST_ASSERT_EQUAL_PTR(binding, arguments->nodes[0]);
}

static void test_duplicate_struct_case_error_releases_bindings(void) {
    assert_struct_payload_error_releases_bindings(
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "switch (s) {\n"
            "    (Rect { width: w, height: h }) { var first = w; }\n"
            "    (Rect { width: a, height: b }) { var second = a; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n",
            "Unreachable union switch case");
}

static void test_non_exhaustive_struct_switch_error_releases_bindings(void) {
    assert_struct_payload_error_releases_bindings(
            "union Shape {\n"
            "    Empty;\n"
            "    Rect { width: float; height: float; }\n"
            "}\n"
            "var s: Shape = Shape.Rect { width: 3.0, height: 4.0 };\n"
            "switch (s) {\n"
            "    (Rect { width: w, height: h }) { var area = w; }\n"
            "}\n",
            "Non-exhaustive union switch");
}

/* 完整语句编译：校验之后的 lowering 仍从同一 AST 实参数组读取 tuple 绑定。 */
static void test_tuple_payload_switch_lowers_after_validation(void) {
    const TZrChar *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: float);\n"
            "}\n"
            "var s: Shape = Shape.Circle(2.0);\n"
            "switch (s) {\n"
            "    (Circle(r)) { var hit = r; }\n"
            "    (Empty) { var miss = 0; }\n"
            "}\n";
    SUnionSwitchFixture fixture;
    SZrFunctionCall *call;
    SZrAstNodeArray *arguments;
    SZrAstNode *binding;

    load_union_switch(source, &fixture);
    call = case_payload_call(&fixture, 0U);
    arguments = call->args;
    TEST_ASSERT_NOT_NULL(arguments);
    TEST_ASSERT_EQUAL_UINT(1U, arguments->count);
    binding = arguments->nodes[0];
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);

    ZrParser_Statement_Compile(&g_compiler, fixture.statements->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    ZrParser_Statement_Compile(&g_compiler, fixture.switchNode);

    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_TRUE(fixture.switchExpression->isUnionExhaustive);
    TEST_ASSERT_EQUAL_PTR(arguments, call->args);
    TEST_ASSERT_EQUAL_UINT(1U, arguments->count);
    TEST_ASSERT_EQUAL_PTR(binding, arguments->nodes[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unqualified_struct_payload_bindings_are_released);
    RUN_TEST(test_qualified_struct_payload_bindings_are_released);
    RUN_TEST(test_unqualified_tuple_payload_bindings_stay_owned_by_case_ast);
    RUN_TEST(test_qualified_tuple_payload_bindings_stay_owned_by_case_ast);
    RUN_TEST(test_mixed_payload_switch_releases_only_struct_containers);
    RUN_TEST(test_duplicate_struct_case_error_releases_bindings);
    RUN_TEST(test_non_exhaustive_struct_switch_error_releases_bindings);
    RUN_TEST(test_tuple_payload_switch_lowers_after_validation);
    return UNITY_END();
}
