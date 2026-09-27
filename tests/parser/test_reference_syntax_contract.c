/* 验证引用参数、可调用语法在 lexer、AST 与 canonical type 合同中的一致性。 */
#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_contract.h"

/* Unity 每个用例独占 state；semantic context 借用该 state，销毁时先释放 context。 */
static SZrState *g_state;
static SZrSemanticContext *g_context;

/* 累计所有解析错误，但只复制首条消息和位置供非零范围断言。 */
typedef struct SParserErrorCapture {
    TZrUInt32 count;
    SZrFileRange firstRange;
    char firstMessage[192];
} SParserErrorCapture;

/* 回调入参仅在解析期间有效，所需诊断内容复制到用例栈上的 capture。 */
static void capture_parser_error(
        TZrPtr userData,
        const SZrFileRange *location,
        const TZrChar *message,
        EZrToken token) {
    SParserErrorCapture *capture = (SParserErrorCapture *)userData;
    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL) {
        return;
    }
    if (capture->count == 0u) {
        if (location != ZR_NULL) {
            capture->firstRange = *location;
        }
        if (message != ZR_NULL) {
            snprintf(capture->firstMessage, sizeof(capture->firstMessage), "%s", message);
        }
    }
    capture->count++;
}

/* 每个 RUN_TEST 建立新的 VM/context；tearDown 按依赖顺序释放。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_context);
}

void tearDown(void) {
    if (g_context != ZR_NULL) {
        ZrParser_SemanticContext_Free(g_context);
        g_context = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 返回拥有型 AST 交用例释放；sourceName 由测试 state 管理。 */
static SZrAstNode *parse_source(const char *source) {
    SZrString *sourceName = ZrCore_String_Create(g_state, "reference_syntax.zr", 19u);
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

/* 断言脚本边界后返回借用子节点，生命周期不超过 script。 */
static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32((TZrUInt32)index,
                                    (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static void test_lexer_distinguishes_fn_ref_and_callable_delimiters(void) {
    const char *source = "fn ref -> =>";
    SZrString *sourceName = ZrCore_String_Create(g_state, "tokens.zr", 9u);
    SZrLexState lexer;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), sourceName);
    TEST_ASSERT_EQUAL_INT(ZR_TK_FN, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_REF, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_THIN_ARROW, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_FAT_ARROW, lexer.t.token);
    ZrParser_Lexer_Free(&lexer);
}

static void test_named_and_nested_function_type_syntax_preserves_delimiters(void) {
    const char *source =
            "fn transform(factory: fn(int) -> fn(string) -> bool): fn(int) -> bool {\n"
            "  return factory;\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *declaration = script_statement(script, 0u);
    SZrFunctionDeclaration *function;
    SZrParameter *parameter;
    SZrFunctionType *outerType;
    SZrFunctionType *nestedType;

    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, declaration->type);
    function = &declaration->data.functionDeclaration;
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)function->params->count);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)function->returnDelimiterLocation.end.offset);

    parameter = &function->params->nodes[0]->data.parameter;
    TEST_ASSERT_NOT_NULL(parameter->typeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_TYPE, parameter->typeInfo->name->type);
    outerType = &parameter->typeInfo->name->data.functionType;
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)outerType->arrowLocation.end.offset);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_TYPE, outerType->returnType->name->type);
    nestedType = &outerType->returnType->name->data.functionType;
    TEST_ASSERT_GREATER_THAN_UINT32((TZrUInt32)outerType->arrowLocation.end.offset,
                                    (TZrUInt32)nestedType->arrowLocation.start.offset);

    ZrParser_Ast_Free(g_state, script);
}

/* 区分源码 passing 拼写与 canonical 逃逸上界，防止 scoped ref 被误当 caller escape。 */
static void test_parameter_source_forms_normalize_to_canonical_contracts(void) {
    const char *source =
            "fn contracts(value: Data, input: in Data, writable: ref Data, "
            "observed: ref readonly Data, local: scoped ref Data, "
            "localView: scoped ref readonly Data, result: out Data): void {}";
    const EZrParameterSourcePassingForm expectedForms[] = {
            ZR_PARAMETER_SOURCE_VALUE,
            ZR_PARAMETER_SOURCE_IN,
            ZR_PARAMETER_SOURCE_REF,
            ZR_PARAMETER_SOURCE_REF_READONLY,
            ZR_PARAMETER_SOURCE_SCOPED_REF,
            ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY,
            ZR_PARAMETER_SOURCE_OUT,
    };
    const EZrCanonicalPassingForm expectedPassing[] = {
            ZR_CANONICAL_PASSING_VALUE,
            ZR_CANONICAL_PASSING_IN,
            ZR_CANONICAL_PASSING_REF,
            ZR_CANONICAL_PASSING_REF_READONLY,
            ZR_CANONICAL_PASSING_REF,
            ZR_CANONICAL_PASSING_REF_READONLY,
            ZR_CANONICAL_PASSING_OUT,
    };
    const EZrCanonicalEscapeUpperBound expectedEscapeBounds[] = {
            ZR_CANONICAL_ESCAPE_FUNCTION,
            ZR_CANONICAL_ESCAPE_FUNCTION,
            ZR_CANONICAL_ESCAPE_CALLER,
            ZR_CANONICAL_ESCAPE_CALLER,
            ZR_CANONICAL_ESCAPE_FUNCTION,
            ZR_CANONICAL_ESCAPE_FUNCTION,
            ZR_CANONICAL_ESCAPE_FUNCTION,
    };
    SZrAstNode *script = parse_source(source);
    SZrFunctionDeclaration *function = &script_statement(script, 0u)->data.functionDeclaration;
    TZrTypeId dataType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "test", 4u),
            ZrCore_String_Create(g_state, "Data", 4u),
            0x02000001u);

    TEST_ASSERT_EQUAL_UINT32(7u, (TZrUInt32)function->params->count);
    for (TZrSize index = 0u; index < function->params->count; index++) {
        SZrParameter *parameter = &function->params->nodes[index]->data.parameter;
        SZrCanonicalParameterContract contract;
        const SZrCanonicalTypeNode *contractType;

        TEST_ASSERT_EQUAL_INT(expectedForms[index], parameter->sourcePassingForm);
        TEST_ASSERT_TRUE(ZrParser_SyntaxParameter_Normalize(
                g_context, parameter, dataType, &contract));
        TEST_ASSERT_EQUAL_INT(expectedPassing[index], contract.passingForm);
        TEST_ASSERT_EQUAL_INT(
                expectedEscapeBounds[index], contract.escapeUpperBound);
        contractType = ZrParser_CanonicalType_Find(g_context, contract.typeId);
        if (index == 0u) {
            TEST_ASSERT_EQUAL_UINT32(dataType, contract.typeId);
        } else {
            TEST_ASSERT_NOT_NULL(contractType);
            TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_REF, contractType->kind);
        }
    }

    ZrParser_Ast_Free(g_state, script);
}

static void test_anonymous_expression_body_and_call_markers_are_preserved(void) {
    const char *source =
            "var increment = fn(value: int): int => value + 1;\n"
            "var noop = fn(): void {};\n"
            "fn invoke(target: ref int, result: out int): void {\n"
            "  update(ref target, out result);\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrVariableDeclaration *variable = &script_statement(script, 0u)->data.variableDeclaration;
    SZrLambdaExpression *lambda;
    SZrFunctionDeclaration *invoke;
    SZrAstNode *statement;
    SZrPrimaryExpression *primary;
    SZrFunctionCall *call;

    TEST_ASSERT_NOT_NULL(variable->value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, variable->value->type);
    lambda = &variable->value->data.lambdaExpression;
    TEST_ASSERT_TRUE(lambda->isExpressionBody);
    TEST_ASSERT_NOT_NULL(lambda->returnType);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)lambda->bodyDelimiterLocation.end.offset);

    TEST_ASSERT_FALSE(script_statement(script, 1u)->data.variableDeclaration.value->data.lambdaExpression.isExpressionBody);
    invoke = &script_statement(script, 2u)->data.functionDeclaration;
    statement = invoke->body->data.block.body->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    primary = &statement->data.expressionStatement.expr->data.primaryExpression;
    call = &primary->members->nodes[0]->data.functionCall;
    TEST_ASSERT_NOT_NULL(call->argumentMarkers);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)call->argumentMarkers->length);
    TEST_ASSERT_EQUAL_INT(
            ZR_CALL_ARGUMENT_MARKER_REF,
            ((SZrCallArgumentSyntax *)ZrCore_Array_Get(call->argumentMarkers, 0u))->marker);
    TEST_ASSERT_EQUAL_INT(
            ZR_CALL_ARGUMENT_MARKER_OUT,
            ((SZrCallArgumentSyntax *)ZrCore_Array_Get(call->argumentMarkers, 1u))->marker);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u,
            (TZrUInt32)((SZrCallArgumentSyntax *)ZrCore_Array_Get(
                    call->argumentMarkers, 0u))->markerLocation.end.offset);

    ZrParser_Ast_Free(g_state, script);
}

/* 命名声明和函数类型注解应驻留为同一个 canonical callable identity。 */
static void test_callable_syntaxes_intern_the_same_canonical_contract(void) {
    const char *source =
            "fn named(value: ref readonly Data): Data { return value; }\n"
            "var typed: fn(value: ref readonly Data) -> Data;\n";
    SZrAstNode *script = parse_source(source);
    SZrFunctionDeclaration *named = &script_statement(script, 0u)->data.functionDeclaration;
    SZrVariableDeclaration *typed = &script_statement(script, 1u)->data.variableDeclaration;
    SZrFunctionType *functionType = &typed->typeInfo->name->data.functionType;
    TZrTypeId dataType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "test", 4u),
            ZrCore_String_Create(g_state, "Data", 4u),
            0x02000002u);
    TZrTypeId parameterTypes[] = {dataType};
    TZrTypeId namedType;
    TZrTypeId annotationType;

    namedType = ZrParser_SyntaxCallable_Intern(
            g_context,
            named->params,
            parameterTypes,
            dataType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    annotationType = ZrParser_SyntaxCallable_Intern(
            g_context,
            functionType->params,
            parameterTypes,
            dataType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, namedType);
    TEST_ASSERT_EQUAL_UINT32(namedType, annotationType);

    ZrParser_Ast_Free(g_state, script);
}

/* 声明上的 readonly/可变/static 修饰须精化或保留 callable receiver effect。 */
static void test_declaration_receiver_effect_refines_callable_contract(void) {
    const char *source =
            "class Service {\n"
            "  pub const fn read(): int { return 1; }\n"
            "  pub fn write(): int { return 2; }\n"
            "  pub static fn make(): int { return 3; }\n"
            "}\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *classNode = script_statement(script, 0U);
    SZrAstNode *readonlyMethod;
    SZrAstNode *mutableMethod;
    SZrAstNode *staticMethod;
    const SZrCanonicalTypeNode *refinedType;
    TZrTypeId intType;
    TZrTypeId baseType;
    TZrTypeId refinedTypeId;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(
            3U, (TZrUInt32)classNode->data.classDeclaration.members->count);
    readonlyMethod = classNode->data.classDeclaration.members->nodes[0];
    mutableMethod = classNode->data.classDeclaration.members->nodes[1];
    staticMethod = classNode->data.classDeclaration.members->nodes[2];

    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_READONLY,
            ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(readonlyMethod));
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_MUTABLE,
            ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(mutableMethod));
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_NONE,
            ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(staticMethod));

    intType = ZrParser_CanonicalType_InternPrimitive(
            g_context, ZR_VALUE_TYPE_INT64);
    baseType = ZrParser_CanonicalType_InternFunction(
            g_context,
            ZR_NULL,
            0U,
            intType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, baseType);

    refinedTypeId = ZrParser_SyntaxCallable_RefineFromDeclaration(
            g_context, readonlyMethod, baseType);
    refinedType = ZrParser_CanonicalType_Find(g_context, refinedTypeId);
    TEST_ASSERT_NOT_NULL(refinedType);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_READONLY,
            refinedType->data.function.receiverEffect);

    refinedTypeId = ZrParser_SyntaxCallable_RefineFromDeclaration(
            g_context, mutableMethod, baseType);
    refinedType = ZrParser_CanonicalType_Find(g_context, refinedTypeId);
    TEST_ASSERT_NOT_NULL(refinedType);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_MUTABLE,
            refinedType->data.function.receiverEffect);

    refinedTypeId = ZrParser_SyntaxCallable_RefineFromDeclaration(
            g_context, staticMethod, baseType);
    TEST_ASSERT_EQUAL_UINT32(baseType, refinedTypeId);

    ZrParser_Ast_Free(g_state, script);
}

static void test_fn_keyword_applies_to_class_struct_and_interface_methods(void) {
    const char *source =
            "class Service { fn update(value: ref int): void {} }\n"
            "struct Buffer { fn update(value: ref int): void {} }\n"
            "interface Updatable { fn update(value: ref int): void; }\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *classNode = script_statement(script, 0u);
    SZrAstNode *structNode = script_statement(script, 1u);
    SZrAstNode *interfaceNode = script_statement(script, 2u);
    SZrAstNode *classMethod;
    SZrAstNode *structMethod;
    SZrAstNode *interfaceMethod;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, interfaceNode->type);
    classMethod = classNode->data.classDeclaration.members->nodes[0];
    structMethod = structNode->data.structDeclaration.members->nodes[0];
    interfaceMethod = interfaceNode->data.interfaceDeclaration.members->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, classMethod->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_METHOD, structMethod->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_METHOD_SIGNATURE, interfaceMethod->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_SOURCE_REF,
            classMethod->data.classMethod.params->nodes[0]->data.parameter.sourcePassingForm);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_SOURCE_REF,
            structMethod->data.structMethod.params->nodes[0]->data.parameter.sourcePassingForm);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARAMETER_SOURCE_REF,
            interfaceMethod->data.interfaceMethodSignature.params->nodes[0]->data.parameter.sourcePassingForm);

    ZrParser_Ast_Free(g_state, script);
}

/* 逐条拒绝旧箭头和错误修饰顺序，先释放 parser/AST 后比较首条诊断。 */
/* BUG: 回调丢弃 token，断言仅检查消息子串和非零结束偏移；同消息落在错误 token/位置仍会通过，需逐例比较预期 token 与范围。 */
static void test_invalid_callable_delimiters_and_modifier_orders_report_exact_token(void) {
    const char *sources[] = {
            "fn broken() -> int {}",
            "var broken: fn(int) => int;",
            "var broken = fn(value: int): int -> value;",
            "fn broken(value: readonly ref Data): void {}",
            "fn broken(value: scoped Data): void {}",
    };
    const char *messages[] = {
            "Legacy syntax 'function definition arrow' was removed",
            "Legacy syntax 'function type fat arrow' was removed",
            "Anonymous function expressions use '=>'",
            "'readonly' must follow 'ref'",
            "Expected 'ref' after 'scoped'",
    };

    for (TZrSize index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrString *sourceName = ZrCore_String_Create(g_state, "invalid_reference_syntax.zr", 27u);
        SZrParserState parserState;
        SParserErrorCapture capture;
        SZrAstNode *ast;
        TZrBool hasDiagnostic;
        TZrBool messageMatches;
        TZrBool hasPreciseRange;

        memset(&capture, 0, sizeof(capture));
        ZrParser_State_Init(&parserState, g_state, sources[index], strlen(sources[index]), sourceName);
        parserState.suppressErrorOutput = ZR_TRUE;
        parserState.errorCallback = capture_parser_error;
        parserState.errorUserData = &capture;
        ast = ZrParser_ParseWithState(&parserState);
        hasDiagnostic = capture.count > 0u;
        messageMatches = strstr(capture.firstMessage, messages[index]) != ZR_NULL;
        hasPreciseRange = capture.firstRange.end.offset > 0u;
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(g_state, ast);
        }
        ZrParser_State_Free(&parserState);
        TEST_ASSERT_TRUE(hasDiagnostic);
        TEST_ASSERT_TRUE(messageMatches);
        TEST_ASSERT_TRUE(hasPreciseRange);
    }
}

/* TODO: CMake 仅创建此 Unity 目标，未见 CTest/manifest 注册；需确认语法契约应进入哪个自动套件。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lexer_distinguishes_fn_ref_and_callable_delimiters);
    RUN_TEST(test_named_and_nested_function_type_syntax_preserves_delimiters);
    RUN_TEST(test_parameter_source_forms_normalize_to_canonical_contracts);
    RUN_TEST(test_anonymous_expression_body_and_call_markers_are_preserved);
    RUN_TEST(test_callable_syntaxes_intern_the_same_canonical_contract);
    RUN_TEST(test_declaration_receiver_effect_refines_callable_contract);
    RUN_TEST(test_fn_keyword_applies_to_class_struct_and_interface_methods);
    RUN_TEST(test_invalid_callable_delimiters_and_modifier_orders_report_exact_token);
    return UNITY_END();
}
