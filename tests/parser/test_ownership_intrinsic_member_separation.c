#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/writer.h"

static SZrState *g_state;

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

    TEST_ASSERT_EQUAL_INT(2, ZR_SEMANTIC_OWNERSHIP_SHARE);
    TEST_ASSERT_EQUAL_INT(3, ZR_SEMANTIC_OWNERSHIP_DEGRADE);
    TEST_ASSERT_EQUAL_INT(4, ZR_SEMANTIC_OWNERSHIP_WAKE);
    TEST_ASSERT_EQUAL_INT(5, ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX);
}

typedef struct SZrCapturedParserDiagnostic {
    TZrBool reported;
    TZrChar message[256];
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

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
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

static void test_question_dot_is_one_token(void) {
    const TZrChar *source = "?.";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "question_dot.zr");
    SZrLexState lexer;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), sourceName);

    TEST_ASSERT_EQUAL_INT(ZR_TK_QUESTION_DOT, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);
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
    assert_parse_error(
            "share;",
            "Ownership intrinsic must be called with exactly one positional argument");
    assert_parse_error(
            "share();",
            "Ownership intrinsic requires exactly one positional argument");
    assert_parse_error(
            "share(first, second);",
            "Ownership intrinsic accepts exactly one positional argument");
    assert_parse_error(
            "share(value: owner);",
            "Ownership intrinsic accepts exactly one positional argument");
    assert_parse_error("let share = owner;", "Expected identifier");
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

        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(compiler, expression, &result));
        fact = ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
                compiler->semanticContext, expression);

        TEST_ASSERT_NOT_NULL(fact);
        TEST_ASSERT_EQUAL_INT(expectedOperations[index], fact->operation);
        TEST_ASSERT_EQUAL_INT(
                expectedInputs[index], fact->inputType.ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(
                expectedResults[index], fact->resultType.ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(expectedConsuming[index], fact->consuming);
        TEST_ASSERT_EQUAL_UINT32(expectedPlaces[index], fact->placeId);
        TEST_ASSERT_EQUAL_UINT32(0u, fact->loanId);
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
            "share() requires a Unique owner");
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
            "    var mask = 0;\n"
            "    try { weak.read(); }\n"
            "    catch (error: NullReferenceError) { mask = mask + 1; }\n"
            "    catch (error: RuntimeError) { mask = mask + 8; }\n"
            "    try { weak.read(); }\n"
            "    catch (error: RuntimeError) { mask = mask + 2; }\n"
            "    return mask;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "expired_weak_direct_call.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(3, result);

    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ownership_operation_ids_remain_stable);
    RUN_TEST(test_question_dot_is_one_token);
    RUN_TEST(test_question_dot_requires_adjacent_characters);
    RUN_TEST(test_reserved_intrinsics_have_independent_ast);
    RUN_TEST(test_optional_member_and_call_segments_record_access_mode);
    RUN_TEST(test_intrinsic_spellings_remain_legal_member_names);
    RUN_TEST(test_direct_and_optional_callable_syntax_are_distinct);
    RUN_TEST(test_intrinsic_syntax_reports_precise_errors);
    RUN_TEST(test_invalid_optional_postfix_forms_report_distinct_errors);
    RUN_TEST(test_syntax_writer_preserves_intrinsic_and_access_modes);
    RUN_TEST(test_intrinsic_type_contracts_publish_canonical_facts);
    RUN_TEST(test_receiver_guards_publish_chain_contracts);
    RUN_TEST(test_intrinsic_and_optional_receiver_errors_are_precise);
    RUN_TEST(test_intrinsic_calls_emit_dedicated_opcodes_and_execute);
    RUN_TEST(test_intrinsic_spellings_on_objects_use_normal_member_calls);
    RUN_TEST(test_expired_weak_optional_call_skips_arguments);
    RUN_TEST(test_live_weak_optional_call_runs_suffix_after_one_wake);
    RUN_TEST(test_expired_weak_direct_call_throws_named_runtime_error);
    return UNITY_END();
}
