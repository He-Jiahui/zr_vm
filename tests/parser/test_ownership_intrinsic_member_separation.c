#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/diagnostic_registry.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_parser/syntax_contract.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/writer.h"

static SZrState *g_state;
static TZrChar g_ownership_artifact_path[ZR_TESTS_PATH_MAX];

static unsigned int ownership_test_process_id(void) {
#if defined(_WIN32)
    return (unsigned int)_getpid();
#else
    return (unsigned int)getpid();
#endif
}

static void test_ownership_operation_ids_remain_stable(void) {
    TEST_ASSERT_EQUAL_INT(125, ZR_INSTRUCTION_ENUM(OWN_UNIQUE));
    TEST_ASSERT_EQUAL_INT(129, ZR_INSTRUCTION_ENUM(OWN_DEGRADE));
    TEST_ASSERT_EQUAL_INT(162, ZR_INSTRUCTION_ENUM(OWN_WAKE));
    TEST_ASSERT_EQUAL_INT(163, ZR_INSTRUCTION_ENUM(OWN_DROP));
    TEST_ASSERT_EQUAL_INT(236, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX));

    TEST_ASSERT_EQUAL_INT(1, ZR_SEMIR_OPCODE_OWN_UNIQUE);
    TEST_ASSERT_EQUAL_INT(5, ZR_SEMIR_OPCODE_OWN_DEGRADE);
    TEST_ASSERT_EQUAL_INT(16, ZR_SEMIR_OPCODE_OWN_WAKE);
    TEST_ASSERT_EQUAL_INT(17, ZR_SEMIR_OPCODE_OWN_DROP);

    TEST_ASSERT_EQUAL_INT(2, ZR_OWNERSHIP_BUILTIN_KIND_SHARE);
    TEST_ASSERT_EQUAL_INT(3, ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE);
    TEST_ASSERT_EQUAL_INT(6, ZR_OWNERSHIP_BUILTIN_KIND_WAKE);
    TEST_ASSERT_EQUAL_INT(7, ZR_OWNERSHIP_BUILTIN_KIND_DROP);
    TEST_ASSERT_EQUAL_INT(9, ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC);
    TEST_ASSERT_EQUAL_INT(10, ZR_OWNERSHIP_BUILTIN_KIND_RETURN_LOAN);

    TEST_ASSERT_EQUAL_INT(2, ZR_SEMANTIC_OWNERSHIP_SHARE);
    TEST_ASSERT_EQUAL_INT(3, ZR_SEMANTIC_OWNERSHIP_DEGRADE);
    TEST_ASSERT_EQUAL_INT(4, ZR_SEMANTIC_OWNERSHIP_WAKE);
    TEST_ASSERT_EQUAL_INT(5, ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX);
}

typedef struct SZrCapturedParserDiagnostic {
    TZrBool reported;
    TZrChar message[256];
    TZrBool structuredReported;
    TZrChar structuredCode[96];
    SZrFileRange structuredLocation;
    EZrToken structuredToken;
    TZrUInt32 structuredDescriptorId;
    EZrDiagnosticNoFixReason noFixReason;
} SZrCapturedParserDiagnostic;

static void capture_parser_error(TZrPtr userData,
                                 const SZrFileRange *location,
                                 const TZrChar *message,
                                 EZrToken token) {
    SZrCapturedParserDiagnostic *diagnostic =
            (SZrCapturedParserDiagnostic *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (diagnostic == ZR_NULL || diagnostic->reported) {
        return;
    }
    diagnostic->reported = ZR_TRUE;
    diagnostic->message[0] = '\0';
    if (message != ZR_NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
}

static void capture_structured_parser_error(
        TZrPtr userData,
        const SZrStructuredDiagnostic *diagnostic,
        EZrToken token) {
    SZrCapturedParserDiagnostic *capture =
            (SZrCapturedParserDiagnostic *)userData;
    const TZrChar *code;

    if (capture == ZR_NULL || diagnostic == ZR_NULL ||
        capture->structuredReported) {
        return;
    }
    code = diagnostic->code != ZR_NULL
                   ? ZrCore_String_GetNativeString(diagnostic->code)
                   : ZR_NULL;
    capture->structuredReported = ZR_TRUE;
    capture->structuredCode[0] = '\0';
    if (code != ZR_NULL) {
        snprintf(
                capture->structuredCode,
                sizeof(capture->structuredCode),
                "%s",
                code);
    }
    capture->structuredLocation = diagnostic->location;
    capture->structuredToken = token;
    capture->structuredDescriptorId = diagnostic->descriptorId;
    capture->noFixReason = diagnostic->noFixReason;
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_ownership_artifact_path[0] != '\0') {
        (void)remove(g_ownership_artifact_path);
        g_ownership_artifact_path[0] = '\0';
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_intrinsic_member_separation.zr");

    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *compiler = (SZrCompilerState *)malloc(sizeof(*compiler));

    TEST_ASSERT_NOT_NULL(compiler);
    memset(compiler, 0, sizeof(*compiler));
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_NOT_NULL(compiler->semanticContext);
    TEST_ASSERT_NOT_NULL(compiler->typeEnv);
    return compiler;
}

static void destroy_compiler_state(SZrCompilerState *compiler) {
    if (compiler == ZR_NULL) {
        return;
    }
    ZrParser_CompilerState_Free(compiler);
    free(compiler);
}

static void register_resource_prototype(SZrCompilerState *compiler) {
    SZrTypePrototypeInfo prototype;
    SZrTypeMemberInfo valueField;
    SZrTypeMemberInfo ownedValueField;
    SZrTypeMemberInfo weakValueField;
    SZrTypeMemberInfo resetMethod;

    memset(&prototype, 0, sizeof(prototype));
    prototype.name = ZrCore_String_CreateFromNative(g_state, "Resource");
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototype.accessModifier = ZR_ACCESS_PUBLIC;
    prototype.modifierFlags = ZR_DECLARATION_MODIFIER_RESOURCE;
    prototype.allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(g_state, &prototype.inherits, sizeof(SZrString *), 1u);
    ZrCore_Array_Init(g_state, &prototype.implements, sizeof(SZrString *), 1u);
    ZrCore_Array_Init(g_state, &prototype.genericParameters, sizeof(SZrTypeGenericParameterInfo), 1u);
    ZrCore_Array_Init(g_state, &prototype.members, sizeof(SZrTypeMemberInfo), 1u);
    ZrCore_Array_Init(g_state, &prototype.decorators, sizeof(SZrTypeDecoratorInfo), 1u);

    memset(&valueField, 0, sizeof(valueField));
    valueField.memberType = ZR_AST_CLASS_FIELD;
    valueField.name = ZrCore_String_CreateFromNative(g_state, "value");
    valueField.fieldTypeName = ZrCore_String_CreateFromNative(g_state, "Resource");
    valueField.accessModifier = ZR_ACCESS_PUBLIC;
    valueField.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
    valueField.fieldSize = sizeof(TZrUInt64);
    ZrCore_Array_Push(g_state, &prototype.members, &valueField);

    memset(&ownedValueField, 0, sizeof(ownedValueField));
    ownedValueField.memberType = ZR_AST_CLASS_FIELD;
    ownedValueField.name = ZrCore_String_CreateFromNative(g_state, "ownedValue");
    ownedValueField.fieldTypeName = ZrCore_String_CreateFromNative(g_state, "Resource");
    ownedValueField.accessModifier = ZR_ACCESS_PUBLIC;
    ownedValueField.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
    ownedValueField.fieldSize = sizeof(TZrUInt64);
    ZrCore_Array_Push(g_state, &prototype.members, &ownedValueField);

    memset(&weakValueField, 0, sizeof(weakValueField));
    weakValueField.memberType = ZR_AST_CLASS_FIELD;
    weakValueField.name = ZrCore_String_CreateFromNative(g_state, "weakValue");
    weakValueField.fieldTypeName = ZrCore_String_CreateFromNative(g_state, "Resource");
    weakValueField.accessModifier = ZR_ACCESS_PUBLIC;
    weakValueField.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
    weakValueField.fieldSize = sizeof(TZrUInt64);
    ZrCore_Array_Push(g_state, &prototype.members, &weakValueField);

    memset(&resetMethod, 0, sizeof(resetMethod));
    resetMethod.memberType = ZR_AST_CLASS_METHOD;
    resetMethod.name = ZrCore_String_CreateFromNative(g_state, "reset");
    resetMethod.returnTypeName = ZrCore_String_CreateFromNative(g_state, "void");
    resetMethod.accessModifier = ZR_ACCESS_PUBLIC;
    resetMethod.receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    ZrCore_Array_Push(g_state, &prototype.members, &resetMethod);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
            g_state, compiler->typeEnv, prototype.name));
    ZrCore_Array_Push(g_state, &compiler->typePrototypes, &prototype);
}

static void register_plain_boxed_prototype(SZrCompilerState *compiler,
                                           const TZrChar *name) {
    SZrTypePrototypeInfo prototype;

    memset(&prototype, 0, sizeof(prototype));
    prototype.name = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)name);
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototype.accessModifier = ZR_ACCESS_PUBLIC;
    prototype.allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(g_state, &prototype.inherits, sizeof(SZrString *), 1u);
    ZrCore_Array_Init(g_state, &prototype.implements, sizeof(SZrString *), 1u);
    ZrCore_Array_Init(g_state, &prototype.genericParameters, sizeof(SZrTypeGenericParameterInfo), 1u);
    ZrCore_Array_Init(g_state, &prototype.members, sizeof(SZrTypeMemberInfo), 1u);
    ZrCore_Array_Init(g_state, &prototype.decorators, sizeof(SZrTypeDecoratorInfo), 1u);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
            g_state, compiler->typeEnv, prototype.name));
    ZrCore_Array_Push(g_state, &compiler->typePrototypes, &prototype);
}

static void register_owner_binding(SZrCompilerState *compiler,
                                   const TZrChar *name,
                                   EZrOwnershipQualifier qualifier,
                                   TZrBool isNullable,
                                   const TZrChar *typeName,
                                   TZrUInt32 identity,
                                   TZrUInt32 placeId) {
    SZrInferredType type;
    SZrFileRange declarationRange;

    memset(&declarationRange, 0, sizeof(declarationRange));
    declarationRange.source = ZrCore_String_CreateFromNative(
            g_state, "ownership_intrinsic_binding.zr");
    declarationRange.start.offset = identity;
    declarationRange.start.line = 1;
    declarationRange.start.column = (TZrInt32)identity;
    declarationRange.end = declarationRange.start;
    declarationRange.end.offset = identity + 1u;
    declarationRange.end.column++;
    ZrParser_InferredType_InitFull(
            g_state,
            &type,
            ZR_VALUE_TYPE_OBJECT,
            isNullable,
            typeName != ZR_NULL
                    ? ZrCore_String_Create(
                              g_state,
                              (TZrNativeString)typeName,
                              strlen(typeName))
                    : ZR_NULL);
    type.ownershipQualifier = qualifier;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(
            g_state,
            compiler->typeEnv,
            ZrCore_String_Create(
                    g_state, (TZrNativeString)name, strlen(name)),
            &type,
            identity,
            identity,
            placeId,
            declarationRange));
    ZrParser_InferredType_Free(g_state, &type);
}

static SZrAstNode *statement_expression(SZrAstNode *script, TZrSize index) {
    SZrAstNode *statement;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)script->data.script.statements->count);
    statement = script->data.script.statements->nodes[index];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    return statement->data.expressionStatement.expr;
}

static SZrAstNode *postfix_segment(SZrAstNode *expression, TZrSize index) {
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.primaryExpression.members);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)expression->data.primaryExpression.members->count);
    return expression->data.primaryExpression.members->nodes[index];
}

static TZrBool function_contains_opcode_recursive(
        const SZrFunction *function,
        EZrInstructionCode opcode,
        TZrUInt32 depth) {
    TZrUInt32 index;

    if (function == ZR_NULL || depth > 8u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index]
                    .instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }
    for (index = 0u; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        SZrFunction *nested;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION || constant->isNative ||
            constant->value.object == ZR_NULL) {
            continue;
        }
        nested = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (function_contains_opcode_recursive(nested, opcode, depth + 1u)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrUInt32 function_count_opcode_recursive(
        const SZrFunction *function,
        EZrInstructionCode opcode,
        TZrUInt32 depth) {
    TZrUInt32 count = 0u;

    if (function == ZR_NULL || depth > 8u) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if (function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }
    for (TZrUInt32 index = 0u; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        SZrFunction *nested;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        nested = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (nested != function) {
            count += function_count_opcode_recursive(
                    nested, opcode, depth + 1u);
        }
    }
    return count;
}

static void assert_parse_error(const TZrChar *source, const TZrChar *expectedFragment) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_intrinsic_error.zr");
    SZrCapturedParserDiagnostic diagnostic;
    SZrParserState parserState;
    SZrAstNode *script;

    memset(&diagnostic, 0, sizeof(diagnostic));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_TRUE(diagnostic.reported || parserState.hasError || script == ZR_NULL);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, expectedFragment));

    if (script != ZR_NULL) {
        ZrParser_Ast_Free(g_state, script);
    }
    ZrParser_State_Free(&parserState);
}

static void assert_intrinsic_syntax_error(
        const TZrChar *source,
        const TZrChar *expectedFragment,
        const TZrChar *expectedCode,
        TZrUInt32 expectedDescriptorId,
        EZrToken expectedToken,
        TZrUInt32 expectedStart,
        TZrUInt32 expectedEnd) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_intrinsic_error.zr");
    SZrCapturedParserDiagnostic diagnostic;
    SZrParserState parserState;
    SZrAstNode *script;

    memset(&diagnostic, 0, sizeof(diagnostic));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_parser_error;
    parserState.structuredErrorCallback = capture_structured_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_TRUE(diagnostic.reported || parserState.hasError || script == ZR_NULL);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, expectedFragment));
    TEST_ASSERT_TRUE_MESSAGE(diagnostic.structuredReported, source);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
            expectedCode, diagnostic.structuredCode, source);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedDescriptorId, diagnostic.structuredDescriptorId, source);
    TEST_ASSERT_EQUAL_INT_MESSAGE(
            expectedToken, diagnostic.structuredToken, source);
    TEST_ASSERT_EQUAL_INT_MESSAGE(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic.noFixReason,
            source);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedStart, diagnostic.structuredLocation.start.offset, source);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedEnd, diagnostic.structuredLocation.end.offset, source);

    if (script != ZR_NULL) {
        ZrParser_Ast_Free(g_state, script);
    }
    ZrParser_State_Free(&parserState);
}

static void assert_reserved_intrinsic_binding_error(
        const TZrChar *source,
        const TZrChar *name,
        EZrToken expectedToken,
        TZrUInt32 expectedStart) {
    SZrString *sourceName;
    SZrParserState parserState;
    SZrCapturedParserDiagnostic diagnostic;
    const SZrDiagnosticDescriptor *descriptor;
    SZrAstNode *script;

    sourceName = ZrCore_String_CreateFromNative(
            g_state, "reserved_ownership_intrinsic_binding.zr");
    memset(&diagnostic, 0, sizeof(diagnostic));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_parser_error;
    parserState.structuredErrorCallback = capture_structured_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    script = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_TRUE_MESSAGE(diagnostic.reported, source);
    TEST_ASSERT_TRUE_MESSAGE(diagnostic.structuredReported, source);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
            "reserved_ownership_intrinsic_name",
            diagnostic.structuredCode,
            source);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            4008u, diagnostic.structuredDescriptorId, source);
    descriptor = ZrParser_DiagnosticRegistry_FindByCode(
            "reserved_ownership_intrinsic_name");
    TEST_ASSERT_NOT_NULL_MESSAGE(descriptor, source);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(4008u, descriptor->id, source);
    TEST_ASSERT_EQUAL_INT_MESSAGE(
            ZR_LINT_CATEGORY_OWNERSHIP, descriptor->category, source);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expectedToken, diagnostic.structuredToken, source);
    TEST_ASSERT_EQUAL_INT_MESSAGE(
            ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
            diagnostic.noFixReason,
            source);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedStart,
            diagnostic.structuredLocation.start.offset,
            source);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            expectedStart + (TZrUInt32)strlen(name),
            diagnostic.structuredLocation.end.offset,
            source);
    if (script != ZR_NULL) {
        ZrParser_Ast_Free(g_state, script);
    }
    ZrParser_State_Free(&parserState);

    sourceName = ZrCore_String_CreateFromNative(
            g_state, "reserved_ownership_intrinsic_compile.zr");
    TEST_ASSERT_NULL_MESSAGE(
            ZrParser_Source_Compile(
                    g_state, source, strlen(source), sourceName),
            source);
}

static void test_question_dot_is_one_token(void) {
    const TZrChar *source = "?.";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "question_dot.zr");
    SZrLexState lexer;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), sourceName);

    TEST_ASSERT_EQUAL_INT(ZR_TK_QUESTION_DOT, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);
    ZrParser_Lexer_Free(&lexer);
}

static void test_question_dot_requires_adjacent_characters(void) {
    const TZrChar *source = "? .";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "question_space_dot.zr");
    SZrLexState lexer;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), sourceName);

    TEST_ASSERT_EQUAL_INT(ZR_TK_QUESTIONMARK, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_DOT, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);
    ZrParser_Lexer_Free(&lexer);
}

static void test_reserved_intrinsics_have_independent_ast(void) {
    static const EZrOwnershipIntrinsicOperation expectedOperations[] = {
            ZR_OWNERSHIP_INTRINSIC_SHARE,
            ZR_OWNERSHIP_INTRINSIC_DEGRADE,
            ZR_OWNERSHIP_INTRINSIC_WAKE,
            ZR_OWNERSHIP_INTRINSIC_INTO_GC,
            ZR_OWNERSHIP_INTRINSIC_DROP,
    };
    SZrAstNode *script = parse_source(
            "share(owner); degrade(shared); wake(weak); intoGc(owner); drop(owner);");

    for (TZrSize index = 0u;
         index < sizeof(expectedOperations) / sizeof(expectedOperations[0]);
         index++) {
        SZrAstNode *expression = statement_expression(script, index);

        TEST_ASSERT_EQUAL_INT(ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION, expression->type);
        TEST_ASSERT_EQUAL_INT(
                expectedOperations[index],
                expression->data.ownershipIntrinsicExpression.operation);
        TEST_ASSERT_NOT_NULL(expression->data.ownershipIntrinsicExpression.argument);
    }

    ZrParser_Ast_Free(g_state, script);
}

static void test_optional_member_and_call_segments_record_access_mode(void) {
    SZrAstNode *script = parse_source("weak?.service.send(1)?.(2);");
    SZrAstNode *expression = statement_expression(script, 0u);
    SZrAstNodeArray *segments;

    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    segments = expression->data.primaryExpression.members;
    TEST_ASSERT_NOT_NULL(segments);
    TEST_ASSERT_EQUAL_UINT32(4u, (TZrUInt32)segments->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, segments->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_OPTIONAL,
            segments->nodes[0]->data.memberExpression.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, segments->nodes[1]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_DIRECT,
            segments->nodes[1]->data.memberExpression.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, segments->nodes[2]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_DIRECT,
            segments->nodes[2]->data.functionCall.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, segments->nodes[3]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_OPTIONAL,
            segments->nodes[3]->data.functionCall.accessMode);
    TEST_ASSERT_EQUAL_UINT32(4u, segments->nodes[0]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(13u, segments->nodes[1]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(18u, segments->nodes[2]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(21u, segments->nodes[3]->location.start.offset);

    ZrParser_Ast_Free(g_state, script);
}

static void test_intrinsic_spellings_remain_legal_member_names(void) {
    static const TZrChar *expectedNames[] = {
            "share", "degrade", "wake", "intoGc", "drop",
    };
    SZrAstNode *script = parse_source(
            "class Box {\n"
            "  pub fn share(): int { return 1; }\n"
            "  pub fn degrade(): int { return 2; }\n"
            "  pub fn wake(): int { return 3; }\n"
            "  pub fn intoGc(): int { return 4; }\n"
            "  pub fn drop(): int { return 5; }\n"
            "}\n"
            "new Box().share();\n"
            "new Box().degrade();\n"
            "new Box().wake();\n"
            "new Box().intoGc();\n"
            "new Box().drop();\n");
    SZrAstNode *declaration;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(6u, (TZrUInt32)script->data.script.statements->count);
    declaration = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(
            5u,
            (TZrUInt32)declaration->data.classDeclaration.members->count);
    for (TZrSize index = 0u; index < 5u; index++) {
        SZrAstNode *member = declaration->data.classDeclaration.members->nodes[index];

        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, member->type);
        TEST_ASSERT_NOT_NULL(member->data.classMethod.name);
        TEST_ASSERT_NOT_NULL(member->data.classMethod.name->name);
        TEST_ASSERT_EQUAL_STRING(
                expectedNames[index],
                ZrCore_String_GetNativeString(member->data.classMethod.name->name));
    }

    for (TZrSize index = 1u; index < 6u; index++) {
        SZrAstNode *expression = statement_expression(script, index);
        SZrAstNodeArray *segments;

        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
        segments = expression->data.primaryExpression.members;
        TEST_ASSERT_NOT_NULL(segments);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, (TZrUInt32)segments->count);
        TEST_ASSERT_EQUAL_INT(
                ZR_POSTFIX_ACCESS_DIRECT,
                segments->nodes[segments->count - 2u]->data.memberExpression.accessMode);
        TEST_ASSERT_EQUAL_INT(
                ZR_POSTFIX_ACCESS_DIRECT,
                segments->nodes[segments->count - 1u]->data.functionCall.accessMode);
    }

    ZrParser_Ast_Free(g_state, script);
}

static void test_intrinsic_spellings_remain_legal_interface_member_names(void) {
    static const TZrChar *const names[] = {
            "share",
            "degrade",
            "wake",
            "intoGc",
            "drop",
    };
    SZrAstNode *script = parse_source(
            "interface IntrinsicMethods {\n"
            "  fn share(): int;\n"
            "  fn degrade(): int;\n"
            "  fn wake(): int;\n"
            "  fn intoGc(): int;\n"
            "  fn drop(): int;\n"
            "}\n");
    SZrAstNode *declaration;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)script->data.script.statements->count);

    declaration = script->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.interfaceDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(sizeof(names) / sizeof(names[0])),
            (TZrUInt32)declaration->data.interfaceDeclaration.members->count);

    for (index = 0u; index < sizeof(names) / sizeof(names[0]); index++) {
        SZrAstNode *member = declaration->data.interfaceDeclaration.members->nodes[index];

        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_METHOD_SIGNATURE, member->type);
        TEST_ASSERT_NOT_NULL(member->data.interfaceMethodSignature.name);
        TEST_ASSERT_NOT_NULL(member->data.interfaceMethodSignature.name->name);
        TEST_ASSERT_EQUAL_STRING(
                names[index],
                ZrCore_String_GetNativeString(
                        member->data.interfaceMethodSignature.name->name));
    }

    ZrParser_Ast_Free(g_state, script);
}

static void test_intrinsic_spellings_remain_ordinary_fields_and_properties(void) {
    const TZrChar *source =
            "class Box {\n"
            "    pub var share: int;\n"
            "    pub var degrade: int;\n"
            "    pub property wake: int { get { return this.share; } }\n"
            "    pub property intoGc: int { get { return this.degrade; } }\n"
            "    pub property drop: int { get { return this.wake + this.intoGc; } }\n"
            "    pub @constructor() { this.share = 1; this.degrade = 2; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var box = new Box();\n"
            "    if (box.share == 1 && box.degrade == 2 && box.wake == 1 &&\n"
            "        box.intoGc == 2 && box.drop == 3) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "intrinsic_named_fields_and_properties.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_SHARE), 0u));
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_DEGRADE), 0u));
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX), 0u));
    TEST_ASSERT_EQUAL_UINT32(
            0u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_DROP), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_intrinsic_spellings_are_object_literal_member_names(void) {
    const TZrChar *source =
            "fn run(): int {\n"
            "    var box = { share: 1, degrade: 2, wake: 4, intoGc: 8, drop: 16 };\n"
            "    return box.share + box.degrade + box.wake + box.intoGc + box.drop;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "intrinsic_named_object_properties.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(31, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_direct_and_optional_callable_syntax_are_distinct(void) {
    SZrAstNode *script = parse_source("callback(1); callback?.(2);");
    SZrAstNode *direct = statement_expression(script, 0u);
    SZrAstNode *optional = statement_expression(script, 1u);

    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, direct->type);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)direct->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_DIRECT,
            direct->data.primaryExpression.members->nodes[0]->data.functionCall.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, optional->type);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)optional->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_OPTIONAL,
            optional->data.primaryExpression.members->nodes[0]->data.functionCall.accessMode);

    ZrParser_Ast_Free(g_state, script);
}

static void test_intrinsic_syntax_reports_precise_errors(void) {
    static const struct {
        const TZrChar *source;
        EZrToken token;
        TZrUInt32 nameLength;
    } valueCases[] = {
            {"share;", ZR_TK_SHARE, 5u},
            {"degrade;", ZR_TK_DEGRADE, 7u},
            {"wake;", ZR_TK_WAKE, 4u},
            {"intoGc;", ZR_TK_INTO_GC, 6u},
            {"drop;", ZR_TK_DROP, 4u},
    };

    for (TZrSize index = 0u;
         index < sizeof(valueCases) / sizeof(valueCases[0]);
         index++) {
        assert_intrinsic_syntax_error(
                valueCases[index].source,
                "Ownership intrinsic must be called with exactly one positional argument",
                "ownership_intrinsic_call_required",
                4009u,
                valueCases[index].token,
                0u,
                valueCases[index].nameLength);
    }
    assert_intrinsic_syntax_error(
            "share();",
            "Ownership intrinsic requires exactly one positional argument",
            "ownership_intrinsic_arity_mismatch",
            4010u,
            ZR_TK_RPAREN,
            6u,
            7u);
    assert_intrinsic_syntax_error(
            "share(first, second);",
            "Ownership intrinsic accepts exactly one positional argument",
            "ownership_intrinsic_arity_mismatch",
            4010u,
            ZR_TK_COMMA,
            11u,
            12u);
    assert_intrinsic_syntax_error(
            "share(value: owner);",
            "Ownership intrinsic accepts exactly one positional argument",
            "ownership_intrinsic_arity_mismatch",
            4010u,
            ZR_TK_IDENTIFIER,
            6u,
            11u);
}

static void test_reserved_intrinsic_lexical_bindings_report_structured_errors(void) {
    static const struct {
        const TZrChar *name;
        EZrToken token;
    } cases[] = {
            {"share", ZR_TK_SHARE},
            {"degrade", ZR_TK_DEGRADE},
            {"wake", ZR_TK_WAKE},
            {"intoGc", ZR_TK_INTO_GC},
            {"drop", ZR_TK_DROP},
    };
    static const struct {
        const TZrChar *source;
        const TZrChar *name;
        EZrToken token;
        TZrUInt32 start;
    } lexicalCases[] = {
            {"fn share(): int { return 0; }", "share", ZR_TK_SHARE, 3u},
            {"fn consume(drop: int): int { return 0; }", "drop", ZR_TK_DROP, 11u},
            {"class wake {}", "wake", ZR_TK_WAKE, 6u},
            {"for (var intoGc in items) {}", "intoGc", ZR_TK_INTO_GC, 9u},
            {"let [degrade] = values;", "degrade", ZR_TK_DEGRADE, 5u},
    };
    TZrChar source[96];

    for (TZrSize index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        snprintf(source, sizeof(source), "let %s = owner;", cases[index].name);
        assert_reserved_intrinsic_binding_error(
                source, cases[index].name, cases[index].token, 4u);
    }
    for (TZrSize index = 0u;
         index < sizeof(lexicalCases) / sizeof(lexicalCases[0]);
         index++) {
        assert_reserved_intrinsic_binding_error(
                lexicalCases[index].source,
                lexicalCases[index].name,
                lexicalCases[index].token,
                lexicalCases[index].start);
    }
}

static void test_invalid_optional_postfix_forms_report_distinct_errors(void) {
    assert_parse_error("callback.(1);", "Missing member name after '.'");
    assert_parse_error("receiver?.[index];", "Optional computed access '?.[' is not supported");
    assert_parse_error("receiver?.;", "Missing member name after '.'");
    assert_parse_error("receiver?.(1;", "Missing closing ')' in function call");
}

static void test_syntax_writer_preserves_intrinsic_and_access_modes(void) {
    SZrAstNode *script = parse_source("share(owner); weak?.service.send(1)?.(2);");
    TZrChar outputPath[1024];
    TZrChar *output;
    TZrSize outputLength = 0u;

    TEST_ASSERT_GREATER_THAN_INT(
            0,
            snprintf(
                    outputPath,
                    sizeof(outputPath),
                    "%s/%s",
                    ZR_VM_TESTS_BINARY_DIR,
                    "ownership_intrinsic_member_separation.zrs"));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteSyntaxTreeFile(g_state, script, outputPath));
    output = ZrTests_ReadTextFile(outputPath, &outputLength);
    remove(outputPath);

    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)outputLength);
    TEST_ASSERT_NOT_NULL(strstr(output, "OWNERSHIP_INTRINSIC_EXPRESSION"));
    TEST_ASSERT_NOT_NULL(strstr(output, "operation: share"));
    TEST_ASSERT_NOT_NULL(strstr(output, "access: optional"));
    TEST_ASSERT_NOT_NULL(strstr(output, "access: direct"));

    free(output);
    ZrParser_Ast_Free(g_state, script);
}

static void test_intrinsic_type_contracts_publish_canonical_facts(void) {
    static const EZrOwnershipIntrinsicOperation expectedOperations[] = {
            ZR_OWNERSHIP_INTRINSIC_SHARE,
            ZR_OWNERSHIP_INTRINSIC_DEGRADE,
            ZR_OWNERSHIP_INTRINSIC_WAKE,
            ZR_OWNERSHIP_INTRINSIC_INTO_GC,
            ZR_OWNERSHIP_INTRINSIC_DROP,
    };
    static const EZrOwnershipQualifier expectedInputs[] = {
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            ZR_OWNERSHIP_QUALIFIER_WEAK,
    };
    static const EZrOwnershipQualifier expectedResults[] = {
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_OWNERSHIP_QUALIFIER_NONE,
    };
    static const EZrSemanticOwnershipFactKind expectedOwnershipKinds[] = {
            ZR_SEMANTIC_OWNERSHIP_FACT_MOVE,
            ZR_SEMANTIC_OWNERSHIP_FACT_COPY,
            ZR_SEMANTIC_OWNERSHIP_FACT_BORROW,
            ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE,
            ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE,
    };
    static const TZrBool expectedConsuming[] = {
            ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_TRUE, ZR_TRUE,
    };
    static const TZrUInt32 expectedPlaces[] = {301u, 302u, 303u, 301u, 303u};
    SZrCompilerState *compiler = create_compiler_state();
    SZrAstNode *script = parse_source(
            "share(owner); degrade(shared); wake(weak); intoGc(owner); drop(weak);");

    register_resource_prototype(compiler);
    register_owner_binding(
            compiler,
            "owner",
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            ZR_FALSE,
            "Resource",
            101u,
            301u);
    register_owner_binding(
            compiler,
            "shared",
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_FALSE,
            "Resource",
            102u,
            302u);
    register_owner_binding(
            compiler,
            "weak",
            ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_FALSE,
            "Resource",
            103u,
            303u);

    for (TZrSize index = 0u; index < 5u; index++) {
        SZrAstNode *expression = statement_expression(script, index);
        SZrInferredType result;
        const SZrOwnershipIntrinsicFact *fact;
        const SZrSemanticOwnershipFact *ownershipFact;

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(compiler, expression, &result));
        fact = ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
                compiler->semanticContext, expression);
        ownershipFact = ZrParser_SemanticFacts_FindOwnershipByNode(
                compiler->semanticContext, expression);

        TEST_ASSERT_NOT_NULL(fact);
        TEST_ASSERT_NOT_NULL(ownershipFact);
        TEST_ASSERT_EQUAL_INT(expectedOperations[index], fact->operation);
        TEST_ASSERT_EQUAL_INT(
                expectedInputs[index], fact->inputType.ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(
                expectedResults[index], fact->resultType.ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(expectedConsuming[index], fact->consuming);
        TEST_ASSERT_EQUAL_UINT32(expectedPlaces[index], fact->placeId);
        TEST_ASSERT_EQUAL_UINT32(0u, fact->loanId);
        TEST_ASSERT_EQUAL_INT(expectedOwnershipKinds[index], ownershipFact->kind);
        TEST_ASSERT_EQUAL_INT(expectedResults[index], result.ownershipQualifier);
        if (expectedOperations[index] == ZR_OWNERSHIP_INTRINSIC_WAKE) {
            TEST_ASSERT_TRUE(result.isNullable);
        }
        if (expectedOperations[index] == ZR_OWNERSHIP_INTRINSIC_INTO_GC) {
            TEST_ASSERT_EQUAL_INT(ZR_GC_BRIDGE_BOX, result.gcBridgeKind);
        }
        if (expectedOperations[index] == ZR_OWNERSHIP_INTRINSIC_DROP) {
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.baseType);
        }
        ZrParser_InferredType_Free(g_state, &result);
    }

    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void test_intrinsics_reject_nullable_owner_operands(void) {
    static const struct {
        const TZrChar *source;
        const TZrChar *bindingName;
        EZrOwnershipQualifier qualifier;
    } cases[] = {
            {"share(maybeOwner);", "maybeOwner", ZR_OWNERSHIP_QUALIFIER_UNIQUE},
            {"degrade(maybeShared);", "maybeShared", ZR_OWNERSHIP_QUALIFIER_SHARED},
            {"wake(maybeWeak);", "maybeWeak", ZR_OWNERSHIP_QUALIFIER_WEAK},
            {"intoGc(maybeOwner);", "maybeOwner", ZR_OWNERSHIP_QUALIFIER_UNIQUE},
    };

    for (TZrSize index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        SZrAstNode *script = parse_source(cases[index].source);
        SZrAstNode *expression = statement_expression(script, 0u);
        SZrCompilerState *compiler = create_compiler_state();
        SZrInferredType result;
        const SZrDiagnosticDescriptor *descriptor;
        const TZrChar *diagnosticCode;
        const TZrChar *argumentStart;

        register_resource_prototype(compiler);
        register_owner_binding(
                compiler,
                cases[index].bindingName,
                cases[index].qualifier,
                ZR_TRUE,
                "Resource",
                (TZrUInt32)(900u + index),
                (TZrUInt32)(1000u + index));

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE_MESSAGE(
                ZrParser_ExpressionType_Infer(compiler, expression, &result),
                cases[index].source);
        TEST_ASSERT_TRUE_MESSAGE(compiler->hasError, cases[index].source);
        TEST_ASSERT_NOT_NULL_MESSAGE(compiler->errorMessage, cases[index].source);
        TEST_ASSERT_NOT_NULL_MESSAGE(
                strstr(
                        compiler->errorMessage,
                        "Ownership transition requires a live owner"),
                cases[index].source);
        TEST_ASSERT_TRUE_MESSAGE(compiler->hasStructuredError, cases[index].source);
        diagnosticCode = ZrCore_String_GetNativeString(
                compiler->structuredError.code);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
                "nullable_ownership_intrinsic_operand",
                diagnosticCode,
                cases[index].source);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                4011u,
                compiler->structuredError.descriptorId,
                cases[index].source);
        descriptor = ZrParser_DiagnosticRegistry_FindByCode(diagnosticCode);
        TEST_ASSERT_NOT_NULL_MESSAGE(descriptor, cases[index].source);
        TEST_ASSERT_EQUAL_INT_MESSAGE(
                ZR_LINT_CATEGORY_OWNERSHIP,
                descriptor->category,
                cases[index].source);
        TEST_ASSERT_EQUAL_INT_MESSAGE(
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION,
                compiler->structuredError.noFixReason,
                cases[index].source);
        TEST_ASSERT_FALSE_MESSAGE(
                compiler->structuredError.fixes.isValid,
                cases[index].source);
        argumentStart = strstr(cases[index].source, cases[index].bindingName);
        TEST_ASSERT_NOT_NULL_MESSAGE(argumentStart, cases[index].source);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                (TZrUInt32)(argumentStart - cases[index].source),
                compiler->structuredError.location.start.offset,
                cases[index].source);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                (TZrUInt32)(argumentStart - cases[index].source +
                            strlen(cases[index].bindingName)),
                compiler->structuredError.location.end.offset,
                cases[index].source);
        TEST_ASSERT_NULL_MESSAGE(
                ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
                        compiler->semanticContext, expression),
                cases[index].source);
        TEST_ASSERT_NULL_MESSAGE(
                ZrParser_SemanticFacts_FindOwnershipByNode(
                        compiler->semanticContext, expression),
                cases[index].source);

        ZrParser_InferredType_Free(g_state, &result);
        ZrParser_Ast_Free(g_state, script);
        destroy_compiler_state(compiler);
    }
}

static void test_drop_accepts_nullable_owner_cleanup(void) {
    SZrAstNode *script = parse_source("drop(maybeWeak);");
    SZrAstNode *expression = statement_expression(script, 0u);
    SZrCompilerState *compiler = create_compiler_state();
    SZrInferredType result;
    const SZrOwnershipIntrinsicFact *fact;

    register_owner_binding(
            compiler,
            "maybeWeak",
            ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_TRUE,
            "Resource",
            910u,
            1010u);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            compiler, expression, &result));
    TEST_ASSERT_FALSE(compiler->hasError);
    fact = ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
            compiler->semanticContext, expression);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_INTRINSIC_DROP, fact->operation);
    TEST_ASSERT_TRUE(fact->inputType.isNullable);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.baseType);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void test_receiver_guards_publish_chain_contracts(void) {
    static const EZrReceiverGuardKind expectedKinds[] = {
            ZR_RECEIVER_GUARD_WEAK_WAKE,
            ZR_RECEIVER_GUARD_WEAK_WAKE,
            ZR_RECEIVER_GUARD_NULL,
            ZR_RECEIVER_GUARD_NULL,
    };
    static const EZrReceiverGuardMode expectedModes[] = {
            ZR_RECEIVER_GUARD_DIRECT,
            ZR_RECEIVER_GUARD_OPTIONAL,
            ZR_RECEIVER_GUARD_DIRECT,
            ZR_RECEIVER_GUARD_OPTIONAL,
    };
    SZrCompilerState *compiler = create_compiler_state();
    SZrAstNode *script = parse_source(
            "weakDirect.value; weakOptional?.value; "
            "nullableDirect.value; nullableOptional?.value;");

    register_resource_prototype(compiler);
    register_owner_binding(
            compiler, "weakDirect", ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_FALSE, "Resource", 201u, 401u);
    register_owner_binding(
            compiler, "weakOptional", ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_FALSE, "Resource", 202u, 402u);
    register_owner_binding(
            compiler, "nullableDirect", ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE, "Resource", 203u, 403u);
    register_owner_binding(
            compiler, "nullableOptional", ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE, "Resource", 204u, 404u);

    for (TZrSize index = 0u; index < 4u; index++) {
        SZrAstNode *expression = statement_expression(script, index);
        SZrAstNode *segment = postfix_segment(expression, 0u);
        SZrInferredType result;
        const SZrReceiverGuardFact *guard;

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
                compiler, expression, &result));
        guard = ZrParser_SemanticFacts_FindReceiverGuardByNode(
                compiler->semanticContext, segment);
        TEST_ASSERT_NOT_NULL(guard);
        TEST_ASSERT_EQUAL_INT(expectedKinds[index], guard->kind);
        TEST_ASSERT_EQUAL_INT(expectedModes[index], guard->mode);
        TEST_ASSERT_EQUAL_INT(
                index == 1u || index == 3u
                        ? ZR_RECEIVER_GUARD_RESULT_NULLABLE
                        : ZR_RECEIVER_GUARD_RESULT_UNCHANGED,
                guard->resultLift);
        TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)guard->chainSegmentStart);
        TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)guard->chainSegmentEnd);
        TEST_ASSERT_FALSE(guard->guardedType.isNullable);
        if (expectedKinds[index] == ZR_RECEIVER_GUARD_WEAK_WAKE) {
            TEST_ASSERT_EQUAL_INT(
                    ZR_OWNERSHIP_QUALIFIER_SHARED,
                    guard->guardedType.ownershipQualifier);
        }
        TEST_ASSERT_EQUAL_INT(
                index == 1u || index == 3u, result.isNullable);
        ZrParser_InferredType_Free(g_state, &result);
    }

    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void test_optional_void_calls_publish_void_noop_contracts(void) {
    SZrCompilerState *compiler = create_compiler_state();
    SZrAstNode *script = parse_source(
            "weak?.reset(); nullable?.reset();");

    register_resource_prototype(compiler);
    register_owner_binding(
            compiler, "weak", ZR_OWNERSHIP_QUALIFIER_WEAK,
            ZR_FALSE, "Resource", 211u, 411u);
    register_owner_binding(
            compiler, "nullable", ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_TRUE, "Resource", 212u, 412u);

    for (TZrSize index = 0u; index < 2u; index++) {
        SZrAstNode *expression = statement_expression(script, index);
        SZrAstNode *optionalMember = postfix_segment(expression, 0u);
        SZrInferredType result;
        const SZrReceiverGuardFact *guard;

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
                compiler, expression, &result));
        guard = ZrParser_SemanticFacts_FindReceiverGuardByNode(
                compiler->semanticContext, optionalMember);
        TEST_ASSERT_NOT_NULL(guard);
        TEST_ASSERT_EQUAL_INT(
                ZR_RECEIVER_GUARD_RESULT_VOID_NOOP, guard->resultLift);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.baseType);
        TEST_ASSERT_FALSE(result.isNullable);
        ZrParser_InferredType_Free(g_state, &result);
    }

    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void assert_inference_error(
        const TZrChar *source,
        const TZrChar *bindingName,
        EZrOwnershipQualifier qualifier,
        TZrBool isNullable,
        const TZrChar *typeName,
        const TZrChar *expectedMessage) {
    SZrCompilerState *compiler = create_compiler_state();
    SZrAstNode *script = parse_source(source);
    SZrAstNode *expression = statement_expression(script, 0u);
    SZrInferredType result;

    register_resource_prototype(compiler);
    register_owner_binding(
            compiler,
            bindingName,
            qualifier,
            isNullable,
            typeName,
            501u,
            601u);
    if (strstr(source, "weak") != ZR_NULL &&
        strcmp(bindingName, "weak") != 0) {
        register_owner_binding(
                compiler,
                "weak",
                ZR_OWNERSHIP_QUALIFIER_WEAK,
                ZR_FALSE,
                "Resource",
                502u,
                602u);
    }

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(
            compiler, expression, &result));
    TEST_ASSERT_TRUE(compiler->hasError);
    TEST_ASSERT_NOT_NULL(compiler->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler->errorMessage, expectedMessage));

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void test_intrinsic_and_optional_receiver_errors_are_precise(void) {
    assert_inference_error(
            "share(shared);",
            "shared",
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_FALSE,
            "Resource",
            "share(owner) requires a Unique owner");
    assert_inference_error(
            "drop(wake(weak));",
            "owner",
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            ZR_FALSE,
            "Resource",
            "Consuming ownership intrinsic requires a place expression");
    assert_inference_error(
            "shared?.value;",
            "shared",
            ZR_OWNERSHIP_QUALIFIER_SHARED,
            ZR_FALSE,
            "Resource",
            "redundant_optional_access");
    assert_inference_error(
            "dynamic?.value;",
            "dynamic",
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            ZR_NULL,
            "unsupported_optional_receiver");
}

static void test_consuming_intrinsics_reject_unlowerable_projections(void) {
    static const TZrChar *errorMessage =
            "Consuming ownership intrinsic currently requires a local owner binding";
    SZrCompilerState *compiler;
    SZrAstNode *script;

    assert_inference_error(
            "share(holder.ownedValue);",
            "holder",
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            "Resource",
            errorMessage);
    assert_inference_error(
            "intoGc(holder.ownedValue);",
            "holder",
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            "Resource",
            errorMessage);
    assert_inference_error(
            "drop(holder.value);",
            "holder",
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            "Resource",
            errorMessage);

    compiler = create_compiler_state();
    script = parse_source("degrade(holder.value); wake(holder.weakValue);");
    register_resource_prototype(compiler);
    register_owner_binding(
            compiler,
            "holder",
            ZR_OWNERSHIP_QUALIFIER_NONE,
            ZR_FALSE,
            "Resource",
            503u,
            603u);
    for (TZrSize index = 0u; index < 2u; index++) {
        SZrInferredType result;

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
                compiler, statement_expression(script, index), &result));
        TEST_ASSERT_EQUAL_INT(
                index == 0u
                        ? ZR_OWNERSHIP_QUALIFIER_WEAK
                        : ZR_OWNERSHIP_QUALIFIER_SHARED,
                result.ownershipQualifier);
        ZrParser_InferredType_Free(g_state, &result);
    }

    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void test_intrinsic_calls_emit_dedicated_opcodes_and_execute(void) {
    const TZrChar *source =
            "resource class Session {}\n"
            "fn runLifecycle(): int {\n"
            "    var seed = own Session();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    var revived = wake(weak);\n"
            "    var bridgeSeed = own Session();\n"
            "    var boxed = intoGc(bridgeSeed);\n"
            "    var mask = 0;\n"
            "    if (revived != null) { mask = mask + 1; }\n"
            "    if (boxed != null) { mask = mask + 2; }\n"
            "    var releasedRevived = drop(revived);\n"
            "    var releasedShared = drop(shared);\n"
            "    var releasedWeak = drop(weak);\n"
            "    if (releasedRevived == null && releasedShared == null && releasedWeak == null) {\n"
            "        mask = mask + 4;\n"
            "    }\n"
            "    return mask;\n"
            "}\n"
            "return runLifecycle();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_intrinsic_lowering.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_SHARE), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_DEGRADE), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX), 0u));
    TEST_ASSERT_TRUE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_DROP), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_construct_qualifier_does_not_publish_ownership_semantics(void) {
    SZrCompilerState *compiler = create_compiler_state();
    SZrAstNode *script = parse_source("new Plain();");
    SZrAstNode *expression = statement_expression(script, 0u);
    SZrInferredType result;
    const SZrSemanticExpressionFact *fact;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_BUILTIN_KIND_NONE,
            expression->data.constructExpression.builtinKind);
    expression->data.constructExpression.ownershipQualifier =
            ZR_OWNERSHIP_QUALIFIER_SHARED;
    register_plain_boxed_prototype(compiler, "Plain");

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(compiler, expression, &result));
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_QUALIFIER_NONE, result.ownershipQualifier);
    fact = ZrParser_SemanticFacts_FindExpressionByNode(
            compiler->semanticContext, expression);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_CALL, fact->kind);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void test_construct_qualifier_does_not_select_ownership_lowering(void) {
    SZrAstNode *script = parse_source(
            "class Plain {}\n"
            "new Plain();\n");
    SZrAstNode *expression = statement_expression(script, 1u);
    SZrFunction *function;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_BUILTIN_KIND_NONE,
            expression->data.constructExpression.builtinKind);
    expression->data.constructExpression.ownershipQualifier =
            ZR_OWNERSHIP_QUALIFIER_SHARED;

    function = ZrParser_Compiler_Compile(g_state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_SHARE), 0u));

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, script);
}

static void test_removed_detach_builtin_id_cannot_lower(void) {
    SZrAstNode *script = parse_source(
            "resource class Session {}\n"
            "var owner = own Session();\n"
            "new owner();\n");
    SZrAstNode *expression = statement_expression(script, 2u);
    SZrFunction *function;

    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    expression->data.constructExpression.isNew = ZR_FALSE;
    expression->data.constructExpression.builtinKind =
            (EZrOwnershipBuiltinKind)8;

    function = ZrParser_Compiler_Compile(g_state, script);
    TEST_ASSERT_NULL(function);

    ZrParser_Ast_Free(g_state, script);
}

static void test_removed_detach_builtin_id_is_rejected_by_type_inference(void) {
    SZrCompilerState *compiler = create_compiler_state();
    SZrAstNode *script = parse_source("new owner();\n");
    SZrAstNode *expression = statement_expression(script, 0u);
    SZrInferredType result;

    register_resource_prototype(compiler);
    register_owner_binding(
            compiler,
            "owner",
            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
            ZR_FALSE,
            "Resource",
            701u,
            801u);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    expression->data.constructExpression.isNew = ZR_FALSE;
    expression->data.constructExpression.builtinKind =
            (EZrOwnershipBuiltinKind)8;

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(
            compiler, expression, &result));
    TEST_ASSERT_TRUE(compiler->hasError);
    TEST_ASSERT_NOT_NULL(compiler->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler->errorMessage, "Unknown ownership builtin kind"));
    TEST_ASSERT_NULL(ZrParser_SemanticFacts_FindOwnershipByNode(
            compiler->semanticContext, expression));

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, script);
    destroy_compiler_state(compiler);
}

static void test_intrinsic_spellings_on_objects_use_normal_member_calls(void) {
    const TZrChar *source =
            "class Service {\n"
            "    fn share(): int { return 1; }\n"
            "    fn degrade(): int { return 2; }\n"
            "    fn wake(): int { return 4; }\n"
            "    fn intoGc(): int { return 8; }\n"
            "    fn drop(): int { return 16; }\n"
            "}\n"
            "var service = new Service();\n"
            "return service.share() + service.degrade() + service.wake() +\n"
            "       service.intoGc() + service.drop();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_member_name_collision.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_SHARE), 0u));
    TEST_ASSERT_FALSE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_DEGRADE), 0u));
    TEST_ASSERT_FALSE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_FALSE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX), 0u));
    TEST_ASSERT_FALSE(function_contains_opcode_recursive(
            function, ZR_INSTRUCTION_ENUM(OWN_DROP), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(31, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_removed_ownership_member_calls_publish_structured_fixes(void) {
    static const struct {
        const TZrChar *source;
        const TZrChar *bindingName;
        EZrOwnershipQualifier qualifier;
        const TZrChar *replacement;
    } cases[] = {
            {"owner.share();", "owner", ZR_OWNERSHIP_QUALIFIER_UNIQUE, "share(owner)"},
            {"shared.weak();", "shared", ZR_OWNERSHIP_QUALIFIER_SHARED, "degrade(shared)"},
            {"shared.degrade();", "shared", ZR_OWNERSHIP_QUALIFIER_SHARED, "degrade(shared)"},
            {"weak.upgrade();", "weak", ZR_OWNERSHIP_QUALIFIER_WEAK, "wake(weak)"},
            {"weak.wake();", "weak", ZR_OWNERSHIP_QUALIFIER_WEAK, "wake(weak)"},
            {"owner.intoGc();", "owner", ZR_OWNERSHIP_QUALIFIER_UNIQUE, "intoGc(owner)"},
            {"owner.drop();", "owner", ZR_OWNERSHIP_QUALIFIER_UNIQUE, "drop(owner)"},
    };

    for (TZrSize index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        SZrAstNode *script = parse_source(cases[index].source);
        SZrAstNode *expression = statement_expression(script, 0u);
        SZrCompilerState *compiler = create_compiler_state();
        SZrInferredType result;
        const SZrStructuredDiagnosticFix *fix;

        register_resource_prototype(compiler);
        register_owner_binding(
                compiler,
                cases[index].bindingName,
                cases[index].qualifier,
                ZR_FALSE,
                "Resource",
                (TZrUInt32)(index + 1u),
                (TZrUInt32)(index + 100u));

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE_MESSAGE(
                ZrParser_ExpressionType_Infer(compiler, expression, &result),
                cases[index].source);
        TEST_ASSERT_TRUE(compiler->hasError);
        TEST_ASSERT_TRUE(compiler->hasStructuredError);
        TEST_ASSERT_NOT_NULL(compiler->structuredError.code);
        TEST_ASSERT_EQUAL_STRING(
                "removed_ownership_member_syntax",
                ZrCore_String_GetNativeString(compiler->structuredError.code));
        TEST_ASSERT_TRUE(compiler->structuredError.fixes.isValid);
        TEST_ASSERT_EQUAL_UINT32(
                1u, (TZrUInt32)compiler->structuredError.fixes.length);
        fix = (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
                &compiler->structuredError.fixes, 0u);
        TEST_ASSERT_NOT_NULL(fix);
        TEST_ASSERT_EQUAL_INT(
                ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE, fix->applicability);
        TEST_ASSERT_EQUAL_STRING(
                cases[index].replacement,
                ZrCore_String_GetNativeString(fix->editText));
        TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)fix->editRange.start.offset);
        TEST_ASSERT_EQUAL_UINT32(
                (TZrUInt32)(strlen(cases[index].source) - 1u),
                (TZrUInt32)fix->editRange.end.offset);

        ZrParser_InferredType_Free(g_state, &result);
        ZrParser_Ast_Free(g_state, script);
        destroy_compiler_state(compiler);
    }
}

static void test_expired_weak_optional_call_skips_arguments(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn add(value: int): int { return value + 10; }\n"
            "}\n"
            "var sideEffects = 0;\n"
            "fn bump(): int { sideEffects = sideEffects + 1; return sideEffects; }\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    drop(shared);\n"
            "    var result = weak?.add(bump());\n"
            "    var mask = 0;\n"
            "    if (result == null) { mask = mask + 1; }\n"
            "    if (sideEffects == 0) { mask = mask + 2; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "expired_weak_optional_call.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(3, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_absent_nullable_optional_void_call_skips_arguments(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn consume(value: int): void { throw \"unexpected suffix\"; }\n"
            "}\n"
            "var sideEffects = 0;\n"
            "fn bump(): int { sideEffects = sideEffects + 1; return sideEffects; }\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    drop(shared);\n"
            "    var nullable = wake(weak);\n"
            "    nullable?.consume(bump());\n"
            "    if (nullable == null && sideEffects == 0) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "absent_nullable_optional_void_call.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_live_weak_optional_call_runs_suffix_after_one_wake(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn add(value: int): int { return value + 10; }\n"
            "}\n"
            "var sideEffects = 0;\n"
            "fn bump(): int { sideEffects = sideEffects + 1; return sideEffects; }\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    var result = weak?.add(bump());\n"
            "    if (result == 11 && sideEffects == 1) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "live_weak_optional_call.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            1u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_expired_weak_direct_call_throws_named_runtime_error(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn read(): int { return 7; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    drop(shared);\n"
            "    var nullable = wake(weak);\n"
            "    var mask = 0;\n"
            "    try { weak.read(); }\n"
            "    catch (error: NullReferenceError) { mask = mask + 1; }\n"
            "    catch (error: RuntimeError) { mask = mask + 8; }\n"
            "    try { weak.read(); }\n"
            "    catch (error: RuntimeError) { mask = mask + 2; }\n"
            "    try { nullable.read(); }\n"
            "    catch (error: NullReferenceError) { mask = mask + 4; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "expired_weak_direct_call.zr");
    function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_expired_weak_direct_member_access_throws_named_runtime_error(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var seed = own Service(7);\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    drop(shared);\n"
            "    var nullable = wake(weak);\n"
            "    var mask = 0;\n"
            "    try { var ignored = weak.value; }\n"
            "    catch (error: NullReferenceError) { mask = mask + 1; }\n"
            "    catch (error: RuntimeError) { mask = mask + 8; }\n"
            "    try { var ignored = nullable.value; }\n"
            "    catch (error: NullReferenceError) { mask = mask + 4; }\n"
            "    catch (error: RuntimeError) { mask = mask + 16; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "expired_weak_direct_member_access.zr");
    function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(5, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_direct_receiver_guard_skips_computed_index_before_throw(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub var values: int[1];\n"
            "    pub @constructor() { this.values = [7]; }\n"
            "}\n"
            "var sideEffects = 0;\n"
            "fn bump(): int { sideEffects = sideEffects + 1; return 0; }\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    drop(shared);\n"
            "    var nullable = wake(weak);\n"
            "    var mask = 0;\n"
            "    try { var ignored = weak.values[bump()]; }\n"
            "    catch (error: NullReferenceError) { mask = mask + 1; }\n"
            "    catch (error: RuntimeError) { mask = mask + 8; }\n"
            "    try { var ignored = nullable.values[bump()]; }\n"
            "    catch (error: NullReferenceError) { mask = mask + 2; }\n"
            "    catch (error: RuntimeError) { mask = mask + 16; }\n"
            "    if (sideEffects != 0) { return 32; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "direct_guard_computed_index_order.zr");
    function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(3, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_weak_receiver_guard_releases_wake_on_suffix_throw(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn explode(): int { throw \"boom\"; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    try { weak.explode(); } catch (error) {}\n"
            "    drop(shared);\n"
            "    var afterDirect = wake(weak);\n"
            "    var directReleased = afterDirect == null;\n"
            "    drop(afterDirect);\n"
            "    var seed2 = own Service();\n"
            "    var shared2 = share(seed2);\n"
            "    var weak2 = degrade(shared2);\n"
            "    try { weak2?.explode(); } catch (error) {}\n"
            "    drop(shared2);\n"
            "    var afterOptional = wake(weak2);\n"
            "    var optionalReleased = afterOptional == null;\n"
            "    drop(afterOptional);\n"
            "    if (directReleased && optionalReleased) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "weak_receiver_guard_suffix_throw.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_live_nullable_shared_receiver_projects_owned_fields(void) {
    const TZrChar *source =
            "resource class Child {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "resource class Root {\n"
            "    pub var child: Unique<Child>;\n"
            "    pub @constructor() { this.child = own Child(7); }\n"
            "}\n"
            "var seed = own Root();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var guarded = wake(weak);\n"
            "var first = guarded?.child.value;\n"
            "var second = guarded?.child.value;\n"
            "if (first == 7 && second == 7) { return 1; }\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "nullable_shared_owned_field_projection.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_weak_optional_field_chain_releases_hidden_owner_after_success(void) {
    const TZrChar *source =
            "resource class Child {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "resource class Parent {\n"
            "    pub var child: Unique<Child>;\n"
            "    pub @constructor() { this.child = own Child(7); }\n"
            "}\n"
            "var seed = own Parent();\n"
            "var shared = share(seed);\n"
            "var weak = degrade(shared);\n"
            "var value = weak?.child.value;\n"
            "drop(shared);\n"
            "var after = wake(weak);\n"
            "if (value == 7 && after == null) { return 1; }\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "weak_optional_field_chain_releases_owner.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_weak_optional_guard_skips_computed_index_suffix(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub var values: int[1];\n"
            "    pub @constructor() { this.values = [7]; }\n"
            "}\n"
            "var sideEffects = 0;\n"
            "fn bump(): int { sideEffects = sideEffects + 1; return 0; }\n"
            "fn run(): int {\n"
            "    var liveSeed = own Service();\n"
            "    var liveShared = share(liveSeed);\n"
            "    var liveWeak = degrade(liveShared);\n"
            "    var liveValue = liveWeak?.values[bump()];\n"
            "    var expiredSeed = own Service();\n"
            "    var expiredShared = share(expiredSeed);\n"
            "    var expiredWeak = degrade(expiredShared);\n"
            "    drop(expiredShared);\n"
            "    var expiredValue = expiredWeak?.values[bump()];\n"
            "    if (liveValue == 7 && expiredValue == null && sideEffects == 1) {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "weak_optional_computed_index_suffix.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_weak_optional_receiver_expression_runs_once(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn read(): int { return 7; }\n"
            "}\n"
            "var receiverEffects = 0;\n"
            "fn countReceiver(value: Weak<Service>): Weak<Service> {\n"
            "    receiverEffects = receiverEffects + 1;\n"
            "    return value;\n"
            "}\n"
            "fn run(): int {\n"
            "    var liveSeed = own Service();\n"
            "    var liveShared = share(liveSeed);\n"
            "    var liveWeak = degrade(liveShared);\n"
            "    var liveValue = countReceiver(liveWeak)?.read();\n"
            "    var expiredSeed = own Service();\n"
            "    var expiredShared = share(expiredSeed);\n"
            "    var expiredWeak = degrade(expiredShared);\n"
            "    drop(expiredShared);\n"
            "    var expiredValue = countReceiver(expiredWeak)?.read();\n"
            "    if (liveValue == 7 && expiredValue == null && receiverEffects == 2) {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "weak_optional_receiver_expression_once.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_weak_optional_guard_skips_property_getter(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub property value: int { get { throw \"getter\"; } }\n"
            "}\n"
            "fn run(): int {\n"
            "    var liveSeed = own Service();\n"
            "    var liveShared = share(liveSeed);\n"
            "    var liveWeak = degrade(liveShared);\n"
            "    var mask = 0;\n"
            "    try {\n"
            "        var liveValue = liveWeak?.value;\n"
            "    } catch (error) { mask = mask + 1; }\n"
            "    var expiredSeed = own Service();\n"
            "    var expiredShared = share(expiredSeed);\n"
            "    var expiredWeak = degrade(expiredShared);\n"
            "    drop(expiredShared);\n"
            "    try {\n"
            "        var expiredValue = expiredWeak?.value;\n"
            "        if (expiredValue == null) { mask = mask + 2; }\n"
            "    } catch (error) { mask = mask + 8; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "weak_optional_property_getter_skip.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(3, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_live_weak_optional_chain_survives_native_gc_pressure(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "    pub const fn readAfterGc(): int {\n"
            "        let system = import(\"zr.system\");\n"
            "        system.gc.collect(\"full\");\n"
            "        return this.value;\n"
            "    }\n"
            "}\n"
            "fn run(): int {\n"
            "    var seed = own Service(7);\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    var observed = weak?.readAfterGc();\n"
            "    drop(shared);\n"
            "    if (observed == 7) { return 1; }\n"
            "    return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "live_weak_optional_gc_pressure.zr");
    function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            1u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_live_weak_missing_member_is_not_null_reference_error(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var seed = own Service(7);\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    var outcome = 0;\n"
            "    if (weak.value != 7) { return 3; }\n"
            "    try { var ignored = weak.missing; }\n"
            "    catch (error: NullReferenceError) { outcome = 1; }\n"
            "    catch (error) { outcome = 2; }\n"
            "    drop(shared);\n"
            "    return outcome;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    ZrParser_ToGlobalState_Register(g_state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(g_state->global));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "live_weak_missing_member_error.zr");
    function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            2u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(2, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_live_weak_supports_repeated_direct_method_calls(void) {
    const TZrChar *source =
            "resource class Service {\n"
            "    pub const fn read(): int { return 7; }\n"
            "}\n"
            "fn run(): int {\n"
            "    var seed = own Service();\n"
            "    var shared = share(seed);\n"
            "    var weak = degrade(shared);\n"
            "    var first = weak.read();\n"
            "    var second = weak.read();\n"
            "    drop(shared);\n"
            "    return first * 10 + second;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "live_weak_repeated_method_calls.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            2u,
            function_count_opcode_recursive(
                    function, ZR_INSTRUCTION_ENUM(OWN_WAKE), 0u));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(g_state, function);
}

#include "test_ownership_optional_callable_cases.h"
#include "test_ownership_artifact_roundtrip_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ownership_operation_ids_remain_stable);
    RUN_TEST(test_question_dot_is_one_token);
    RUN_TEST(test_question_dot_requires_adjacent_characters);
    RUN_TEST(test_reserved_intrinsics_have_independent_ast);
    RUN_TEST(test_optional_member_and_call_segments_record_access_mode);
    RUN_TEST(test_intrinsic_spellings_remain_legal_member_names);
    RUN_TEST(test_intrinsic_spellings_remain_legal_interface_member_names);
    RUN_TEST(test_intrinsic_spellings_remain_ordinary_fields_and_properties);
    RUN_TEST(test_intrinsic_spellings_are_object_literal_member_names);
    RUN_TEST(test_direct_and_optional_callable_syntax_are_distinct);
    RUN_TEST(test_intrinsic_syntax_reports_precise_errors);
    RUN_TEST(test_reserved_intrinsic_lexical_bindings_report_structured_errors);
    RUN_TEST(test_invalid_optional_postfix_forms_report_distinct_errors);
    RUN_TEST(test_syntax_writer_preserves_intrinsic_and_access_modes);
    RUN_TEST(test_intrinsic_type_contracts_publish_canonical_facts);
    RUN_TEST(test_intrinsics_reject_nullable_owner_operands);
    RUN_TEST(test_drop_accepts_nullable_owner_cleanup);
    RUN_TEST(test_receiver_guards_publish_chain_contracts);
    RUN_TEST(test_optional_void_calls_publish_void_noop_contracts);
    RUN_TEST(test_intrinsic_and_optional_receiver_errors_are_precise);
    RUN_TEST(test_consuming_intrinsics_reject_unlowerable_projections);
    RUN_TEST(test_intrinsic_calls_emit_dedicated_opcodes_and_execute);
    RUN_TEST(test_construct_qualifier_does_not_publish_ownership_semantics);
    RUN_TEST(test_construct_qualifier_does_not_select_ownership_lowering);
    RUN_TEST(test_removed_detach_builtin_id_cannot_lower);
    RUN_TEST(test_removed_detach_builtin_id_is_rejected_by_type_inference);
    RUN_TEST(test_intrinsic_spellings_on_objects_use_normal_member_calls);
    RUN_TEST(test_removed_ownership_member_calls_publish_structured_fixes);
    RUN_TEST(test_expired_weak_optional_call_skips_arguments);
    RUN_TEST(test_absent_nullable_optional_void_call_skips_arguments);
    RUN_TEST(test_live_weak_optional_call_runs_suffix_after_one_wake);
    RUN_TEST(test_expired_weak_direct_call_throws_named_runtime_error);
    RUN_TEST(test_expired_weak_direct_member_access_throws_named_runtime_error);
    RUN_TEST(test_direct_receiver_guard_skips_computed_index_before_throw);
    RUN_TEST(test_weak_receiver_guard_releases_wake_on_suffix_throw);
    RUN_TEST(test_live_nullable_shared_receiver_projects_owned_fields);
    RUN_TEST(test_weak_optional_field_chain_releases_hidden_owner_after_success);
    RUN_TEST(test_weak_optional_guard_skips_computed_index_suffix);
    RUN_TEST(test_weak_optional_receiver_expression_runs_once);
    RUN_TEST(test_weak_optional_guard_skips_property_getter);
    RUN_TEST(test_live_weak_optional_chain_survives_native_gc_pressure);
    RUN_TEST(test_live_weak_missing_member_is_not_null_reference_error);
    RUN_TEST(test_live_weak_supports_repeated_direct_method_calls);
    RUN_TEST(test_const_meta_call_publishes_readonly_receiver_effect);
    RUN_TEST(test_static_const_meta_call_is_rejected);
    RUN_TEST(test_const_meta_call_rejects_receiver_mutation);
    RUN_TEST(test_weak_callable_optional_and_direct_call_contracts);
    RUN_TEST(test_absent_nullable_callable_guards_before_arguments);
    RUN_TEST(test_named_function_optional_call_is_rejected);
    RUN_TEST(test_nullable_callable_variable_shadows_named_function);
    RUN_TEST(test_weak_optional_intrinsic_named_members_use_normal_dispatch);
    RUN_TEST(test_weak_direct_wake_named_member_uses_normal_dispatch);
    RUN_TEST(test_ownership_binary_roundtrip_preserves_guard_and_bridge_projection);
    return UNITY_END();
}
