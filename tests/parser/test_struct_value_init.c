#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/bound_expression.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_ir.h"

static SZrState *g_state;
static SZrSemanticContext *g_context;

typedef struct SParserErrorCapture {
    TZrUInt32 count;
    char firstMessage[192];
} SParserErrorCapture;

static void capture_parser_error(TZrPtr userData,
                                 const SZrFileRange *location,
                                 const TZrChar *message,
                                 EZrToken token) {
    SParserErrorCapture *capture = (SParserErrorCapture *)userData;
    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL) {
        return;
    }
    if (capture->count == 0u && message != ZR_NULL) {
        snprintf(capture->firstMessage, sizeof(capture->firstMessage), "%s", message);
    }
    capture->count++;
}

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

static SZrFileRange empty_range(void) {
    SZrFileRange range;
    memset(&range, 0, sizeof(range));
    return range;
}

static SZrAstNode *parse_source(const char *source) {
    SZrString *sourceName = ZrCore_String_Create(g_state, "struct_init.zr", 14u);
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32((TZrUInt32)index,
                                    (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static SZrAstNode *variable_value(SZrAstNode *script, TZrSize index) {
    SZrAstNode *declaration = script_statement(script, index);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.value);
    return declaration->data.variableDeclaration.value;
}

static SZrFunction *find_named_function_recursive(SZrFunction *function,
                                                  const char *name,
                                                  TZrUInt32 depth) {
    TZrNativeString functionName;

    if (function == ZR_NULL || name == ZR_NULL || depth > 32u) {
        return ZR_NULL;
    }
    functionName = function->functionName != ZR_NULL
                           ? ZrCore_String_GetNativeString(function->functionName)
                           : ZR_NULL;
    if (functionName != ZR_NULL && strcmp(functionName, name) == 0) {
        return function;
    }
    for (TZrUInt32 childIndex = 0u; childIndex < function->childFunctionLength;
         childIndex++) {
        SZrFunction *match = find_named_function_recursive(
                &function->childFunctionList[childIndex], name, depth + 1u);
        if (match != ZR_NULL) {
            return match;
        }
    }
    for (TZrUInt32 constantIndex = 0u;
         constantIndex < function->constantValueLength;
         constantIndex++) {
        SZrTypeValue *constant = &function->constantValueList[constantIndex];
        SZrFunction *candidate;
        SZrFunction *match;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        candidate = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (candidate == function) {
            continue;
        }
        match = find_named_function_recursive(candidate, name, depth + 1u);
        if (match != ZR_NULL) {
            return match;
        }
    }
    return ZR_NULL;
}

static void test_init_parses_distinct_qualified_generic_type_and_named_arguments(void) {
    const char *source =
            "var value = init Geometry.Point<int>(x: 1, y: 2);\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *expression = variable_value(script, 0u);
    SZrStructInitExpression *init;
    SZrType *qualified;
    SZrString **firstName;
    SZrString **secondName;

    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_INIT_EXPRESSION, expression->type);
    init = &expression->data.structInitExpression;
    TEST_ASSERT_NOT_NULL(init->typeInfo);
    TEST_ASSERT_NOT_NULL(init->typeInfo->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, init->typeInfo->name->type);
    TEST_ASSERT_EQUAL_STRING(
            "Geometry",
            ZrCore_String_GetNativeString(init->typeInfo->name->data.identifier.name));
    qualified = init->typeInfo->subType;
    TEST_ASSERT_NOT_NULL(qualified);
    TEST_ASSERT_NOT_NULL(qualified->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, qualified->name->type);
    TEST_ASSERT_EQUAL_STRING(
            "Point",
            ZrCore_String_GetNativeString(qualified->name->data.genericType.name->name));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)qualified->name->data.genericType.params->count);

    TEST_ASSERT_NOT_NULL(init->args);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)init->args->count);
    TEST_ASSERT_TRUE(init->hasNamedArgs);
    TEST_ASSERT_NOT_NULL(init->argNames);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)init->argNames->length);
    firstName = (SZrString **)ZrCore_Array_Get(init->argNames, 0u);
    secondName = (SZrString **)ZrCore_Array_Get(init->argNames, 1u);
    TEST_ASSERT_NOT_NULL(firstName);
    TEST_ASSERT_NOT_NULL(secondName);
    TEST_ASSERT_EQUAL_STRING("x", ZrCore_String_GetNativeString(*firstName));
    TEST_ASSERT_EQUAL_STRING("y", ZrCore_String_GetNativeString(*secondName));
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)expression->location.start.offset,
            (TZrUInt32)expression->location.end.offset);

    ZrParser_Ast_Free(g_state, script);
}

static void test_init_identifier_call_remains_an_ordinary_call(void) {
    const char *source =
            "fn init(value: int): int { return value; }\n"
            "var result = init(1);\n";
    SZrAstNode *script = parse_source(source);
    SZrAstNode *expression = variable_value(script, 1u);
    SZrPrimaryExpression *primary;

    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    primary = &expression->data.primaryExpression;
    TEST_ASSERT_NOT_NULL(primary->property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, primary->property->type);
    TEST_ASSERT_EQUAL_STRING(
            "init",
            ZrCore_String_GetNativeString(primary->property->data.identifier.name));
    TEST_ASSERT_NOT_NULL(primary->members);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)primary->members->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, primary->members->nodes[0]->type);

    ZrParser_Ast_Free(g_state, script);
}

static void test_new_remains_the_existing_construct_expression(void) {
    SZrAstNode *script = parse_source("var value = new Box();\n");
    SZrAstNode *expression = variable_value(script, 0u);

    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    TEST_ASSERT_TRUE(expression->data.constructExpression.isNew);
    ZrParser_Ast_Free(g_state, script);
}

static void test_init_type_requires_constructor_parentheses(void) {
    const char *source = "var value = init Point;\n";
    SZrString *sourceName = ZrCore_String_Create(g_state, "invalid_struct_init.zr", 22u);
    SZrParserState parserState;
    SParserErrorCapture capture;
    SZrAstNode *script;

    memset(&capture, 0, sizeof(capture));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, capture.count);
    TEST_ASSERT_NOT_NULL(strstr(capture.firstMessage, "Expected '(' after init type"));

    ZrParser_Ast_Free(g_state, script);
    ZrParser_State_Free(&parserState);
}

static SZrCanonicalConstructorParameter constructor_parameter(
        const char *name,
        TZrTypeId typeId,
        EZrCanonicalCallSiteMarker marker,
        TZrBool hasDefaultValue) {
    SZrCanonicalConstructorParameter parameter;

    memset(&parameter, 0, sizeof(parameter));
    parameter.name = name != ZR_NULL
                             ? ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name)
                             : ZR_NULL;
    parameter.contract.typeId = typeId;
    parameter.contract.passingForm = marker == ZR_CANONICAL_CALL_SITE_OUT
                                             ? ZR_CANONICAL_PASSING_OUT
                                             : marker == ZR_CANONICAL_CALL_SITE_REF
                                                       ? ZR_CANONICAL_PASSING_REF
                                                       : ZR_CANONICAL_PASSING_VALUE;
    parameter.contract.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.contract.entryInitialization = marker == ZR_CANONICAL_CALL_SITE_OUT
                                                     ? ZR_CANONICAL_ENTRY_UNINITIALIZED
                                                     : ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.contract.exitInitialization = marker == ZR_CANONICAL_CALL_SITE_OUT
                                                    ? ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED
                                                    : ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.contract.acceptsTemporary = marker == ZR_CANONICAL_CALL_SITE_NONE;
    parameter.contract.callSiteMarker = marker;
    parameter.hasDefaultValue = hasDefaultValue;
    return parameter;
}

static SZrBoundValueConstructArgumentInput bound_argument(
        TZrTypeId typeId,
        const char *name,
        EZrCanonicalCallSiteMarker marker) {
    SZrBoundValueConstructArgumentInput argument;

    memset(&argument, 0, sizeof(argument));
    argument.typeId = typeId;
    argument.name = name != ZR_NULL
                            ? ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name)
                            : ZR_NULL;
    argument.callSiteMarker = marker;
    argument.sourceRange = empty_range();
    return argument;
}

static void test_bound_value_construct_maps_named_and_default_arguments(void) {
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(
            g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId pointType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9u),
            ZrCore_String_Create(g_state, "Point", 5u),
            0x02000031u);
    SZrCanonicalConstructorParameter parameters[2];
    SZrBoundValueConstructArgumentInput reversed[2];
    SZrBoundValueConstructArgumentInput onlyX[1];
    SZrBoundValueConstruct bound;
    const SZrBoundValueConstructArgument *first;
    const SZrBoundValueConstructArgument *second;

    parameters[0] = constructor_parameter("x", intType, ZR_CANONICAL_CALL_SITE_NONE, ZR_FALSE);
    parameters[1] = constructor_parameter("y", intType, ZR_CANONICAL_CALL_SITE_NONE, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterDefinition(
            g_context,
            pointType,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructorContract(
            g_context, pointType, 0x06000031u, parameters, 2u, ZR_TRUE));

    reversed[0] = bound_argument(intType, "y", ZR_CANONICAL_CALL_SITE_NONE);
    reversed[1] = bound_argument(intType, "x", ZR_CANONICAL_CALL_SITE_NONE);
    ZrParser_BoundValueConstruct_Init(g_state, &bound);
    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_CONSTRUCTOR_RESOLVED,
            ZrParser_BoundValueConstruct_Bind(
                    g_context, pointType, reversed, 2u, empty_range(), &bound));
    TEST_ASSERT_EQUAL_INT(ZR_BOUND_EXPRESSION_VALUE_CONSTRUCT, bound.kind);
    TEST_ASSERT_EQUAL_UINT32(pointType, bound.typeId);
    TEST_ASSERT_EQUAL_UINT32(pointType, bound.resultTypeId);
    TEST_ASSERT_EQUAL_UINT32(0x06000031u, bound.constructorId);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)bound.arguments.length);
    first = (const SZrBoundValueConstructArgument *)ZrCore_Array_Get(&bound.arguments, 0u);
    second = (const SZrBoundValueConstructArgument *)ZrCore_Array_Get(&bound.arguments, 1u);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_UINT32(1u, first->parameterIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, first->sourceIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, second->parameterIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, second->sourceIndex);
    ZrParser_BoundValueConstruct_Free(g_state, &bound);

    onlyX[0] = bound_argument(intType, "x", ZR_CANONICAL_CALL_SITE_NONE);
    ZrParser_BoundValueConstruct_Init(g_state, &bound);
    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_CONSTRUCTOR_RESOLVED,
            ZrParser_BoundValueConstruct_Bind(
                    g_context, pointType, onlyX, 1u, empty_range(), &bound));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)bound.arguments.length);
    TEST_ASSERT_EQUAL_UINT32(0x06000031u, bound.constructorId);
    ZrParser_BoundValueConstruct_Free(g_state, &bound);
}

static void test_bound_value_construct_has_no_class_private_or_ambiguous_fallback(void) {
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(
            g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId valueType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9u),
            ZrCore_String_Create(g_state, "Secret", 6u),
            0x02000032u);
    TZrTypeId classType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9u),
            ZrCore_String_Create(g_state, "Service", 7u),
            0x02000033u);
    SZrCanonicalConstructorParameter parameter =
            constructor_parameter("value", intType, ZR_CANONICAL_CALL_SITE_NONE, ZR_FALSE);
    SZrBoundValueConstructArgumentInput argument =
            bound_argument(intType, ZR_NULL, ZR_CANONICAL_CALL_SITE_NONE);
    SZrBoundValueConstruct bound;

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterDefinition(
            g_context,
            valueType,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructorContract(
            g_context, valueType, 0x06000032u, &parameter, 1u, ZR_FALSE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterDefinition(
            g_context,
            classType,
            ZR_CANONICAL_TYPE_CAPABILITY_GC_CLASS,
            ZR_CANONICAL_GC_SCAN_BARRIERED));

    ZrParser_BoundValueConstruct_Init(g_state, &bound);
    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_CONSTRUCTOR_INACCESSIBLE,
            ZrParser_BoundValueConstruct_Bind(
                    g_context, valueType, &argument, 1u, empty_range(), &bound));
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, bound.constructorId);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)bound.arguments.length);

    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_CONSTRUCTOR_NOT_CONSTRUCTIBLE,
            ZrParser_BoundValueConstruct_Bind(
                    g_context, classType, &argument, 1u, empty_range(), &bound));
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, bound.constructorId);

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructorContract(
            g_context, valueType, 0x06000033u, &parameter, 1u, ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructorContract(
            g_context, valueType, 0x06000034u, &parameter, 1u, ZR_TRUE));
    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_CONSTRUCTOR_AMBIGUOUS,
            ZrParser_BoundValueConstruct_Bind(
                    g_context, valueType, &argument, 1u, empty_range(), &bound));
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, bound.constructorId);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)bound.arguments.length);
    ZrParser_BoundValueConstruct_Free(g_state, &bound);
}

static void test_bound_value_construct_synthesizes_default_only_without_explicit_constructors(void) {
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(
            g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId emptyType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9u),
            ZrCore_String_Create(g_state, "Empty", 5u),
            0x02000034u);
    TZrTypeId explicitType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9u),
            ZrCore_String_Create(g_state, "Explicit", 8u),
            0x02000035u);
    SZrCanonicalConstructorParameter parameter =
            constructor_parameter("value", intType, ZR_CANONICAL_CALL_SITE_NONE, ZR_FALSE);
    SZrBoundValueConstruct bound;

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterDefinition(
            g_context,
            emptyType,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterDefinition(
            g_context,
            explicitType,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructorContract(
            g_context, explicitType, 0x06000035u, &parameter, 1u, ZR_TRUE));

    ZrParser_BoundValueConstruct_Init(g_state, &bound);
    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_CONSTRUCTOR_RESOLVED,
            ZrParser_BoundValueConstruct_Bind(
                    g_context, emptyType, ZR_NULL, 0u, empty_range(), &bound));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_CANONICAL_SYNTHESIZED_DEFAULT_CONSTRUCTOR_ID,
            bound.constructorId);

    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_CONSTRUCTOR_NO_MATCH,
            ZrParser_BoundValueConstruct_Bind(
                    g_context, explicitType, ZR_NULL, 0u, empty_range(), &bound));
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, bound.constructorId);
    ZrParser_BoundValueConstruct_Free(g_state, &bound);
}

static void test_compiler_lowers_default_struct_init_into_destination_without_object_wrapper(void) {
    static const char source[] =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "}\n"
            "var point: Point = init Point();\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semanticIr;
    const SZrSemanticIrInstruction *valueConstruct = ZR_NULL;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = script;
    compiler.scriptAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    for (index = 0U; index < script->data.script.statements->count; index++) {
        SZrAstNode *statement = script->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_STRUCT_DECLARATION) {
            ZrParser_Compiler_CompileStructDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        if (compiler.hasError) {
            break;
        }
    }

    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    semanticIr = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semanticIr);
    for (index = 0U; index < semanticIr->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(semanticIr, index);
        if (instruction != ZR_NULL &&
            instruction->opcode == ZR_SEMANTIC_IR_VALUE_CONSTRUCT) {
            valueConstruct = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(valueConstruct);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, valueConstruct->placeId);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_BASE_LOCAL,
            ZrParser_PlaceGraph_Get(
                    &semanticIr->places, valueConstruct->placeId)->base.kind);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_CANONICAL_SYNTHESIZED_DEFAULT_CONSTRUCTOR_ID,
            valueConstruct->constructorId);
    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(&compiler.instructions, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_NOT_EQUAL(
                ZR_INSTRUCTION_ENUM(CREATE_OBJECT),
                (EZrInstructionCode)instruction->instruction.operationCode);
        TEST_ASSERT_NOT_EQUAL(
                ZR_INSTRUCTION_ENUM(TO_STRUCT),
                (EZrInstructionCode)instruction->instruction.operationCode);
    }

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_compiler_lowers_explicit_named_struct_constructor_without_object_wrapper(void) {
    static const char source[] =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "var point: Point = init Point(y: 2, x: 1);\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semanticIr;
    const SZrSemanticIrInstruction *valueConstruct = ZR_NULL;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(script);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = script;
    compiler.scriptAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    for (index = 0U; index < script->data.script.statements->count; index++) {
        SZrAstNode *statement = script->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_STRUCT_DECLARATION) {
            ZrParser_Compiler_CompileStructDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        if (compiler.hasError) {
            break;
        }
    }

    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    semanticIr = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semanticIr);
    for (index = 0U; index < semanticIr->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(semanticIr, index);
        if (instruction != ZR_NULL &&
            instruction->opcode == ZR_SEMANTIC_IR_VALUE_CONSTRUCT) {
            valueConstruct = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(valueConstruct);
    TEST_ASSERT_NOT_EQUAL(
            ZR_CANONICAL_SYNTHESIZED_DEFAULT_CONSTRUCTOR_ID,
            valueConstruct->constructorId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, valueConstruct->constructorId);
    TEST_ASSERT_EQUAL_UINT32(2U, valueConstruct->operandCount);
    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(&compiler.instructions, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_NOT_EQUAL(
                ZR_INSTRUCTION_ENUM(CREATE_OBJECT),
                (EZrInstructionCode)instruction->instruction.operationCode);
        TEST_ASSERT_NOT_EQUAL(
                ZR_INSTRUCTION_ENUM(TO_STRUCT),
                (EZrInstructionCode)instruction->instruction.operationCode);
    }

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_vm_executes_struct_init_constructor_against_final_destination(void) {
    static const char source[] =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor(x: int, y: int) {\n"
            "        this.x = x;\n"
            "        this.y = y;\n"
            "    }\n"
            "}\n"
            "var point: Point = init Point(y: 2, x: 1);\n"
            "return point.x * 10 + point.y;\n";
    SZrAstNode *script = parse_source(source);
    SZrFunction *function;
    SZrFunction *constructorFunction;
    const SZrFunctionFrameSlotLayout *receiverLayout;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(g_state, script);
    TEST_ASSERT_NOT_NULL(function);
    constructorFunction = find_named_function_recursive(function, "constructor", 0u);
    TEST_ASSERT_NOT_NULL(constructorFunction);
    receiverLayout = ZrCore_Function_FindFrameSlotLayout(constructorFunction, 0u);
    TEST_ASSERT_NOT_NULL(receiverLayout);
    TEST_ASSERT_TRUE(receiverLayout->isParameter);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, receiverLayout->slotKind);
    TEST_ASSERT_BITS_HIGH(
            ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP,
            receiverLayout->reserved0);
    TEST_ASSERT_TRUE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(12, result);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, script);
}

static void test_vm_constructor_throw_unwinds_partial_inline_receiver_to_catch(void) {
    static const char source[] =
            "struct Packet {\n"
            "    pub var code: int;\n"
            "    pub var pending: int;\n"
            "    pub @constructor(code: int) {\n"
            "        this.code = code;\n"
            "        throw \"constructor failed\";\n"
            "    }\n"
            "}\n"
            "try {\n"
            "    var packet: Packet = init Packet(7);\n"
            "} catch (error: string) {\n"
            "    return 91;\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *script = parse_source(source);
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(g_state, script);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(91, result);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, script);
}

static void test_compiler_lowers_struct_init_directly_into_field_projection(void) {
    static const char source[] =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub @constructor(x: int) { this.x = x; }\n"
            "}\n"
            "struct Box { pub var point: Point; }\n"
            "var box: Box = init Box();\n"
            "box.point = init Point(42);\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semanticIr;
    const SZrSemanticIrInstruction *fieldConstruct = ZR_NULL;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(script);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = script;
    compiler.scriptAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    for (index = 0U; index < script->data.script.statements->count; index++) {
        SZrAstNode *statement = script->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_STRUCT_DECLARATION) {
            ZrParser_Compiler_CompileStructDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        if (compiler.hasError) {
            break;
        }
    }

    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    semanticIr = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semanticIr);
    for (index = 0U; index < semanticIr->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(semanticIr, index);
        const SZrParserPlace *place;
        if (instruction == ZR_NULL ||
            instruction->opcode != ZR_SEMANTIC_IR_VALUE_CONSTRUCT) {
            continue;
        }
        place = ZrParser_PlaceGraph_Get(&semanticIr->places, instruction->placeId);
        if (place != ZR_NULL && place->projections.length > 0U) {
            fieldConstruct = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(fieldConstruct);
    {
        const SZrParserPlace *place = ZrParser_PlaceGraph_Get(
                &semanticIr->places, fieldConstruct->placeId);
        const SZrParserPlaceProjection *projection;
        TEST_ASSERT_NOT_NULL(place);
        TEST_ASSERT_EQUAL_INT(ZR_PARSER_PLACE_BASE_LOCAL, place->base.kind);
        projection = ZrParser_Place_ProjectionAt(
                place, place->projections.length - 1U);
        TEST_ASSERT_NOT_NULL(projection);
        TEST_ASSERT_EQUAL_INT(
                ZR_PARSER_PLACE_PROJECTION_FIELD, projection->kind);
    }
    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(&compiler.instructions, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_NOT_EQUAL(
                ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT),
                (EZrInstructionCode)instruction->instruction.operationCode);
    }

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_vm_executes_struct_init_against_inline_field_alias_place(void) {
    static const char source[] =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub @constructor(x: int) { this.x = x; }\n"
            "}\n"
            "struct Box { pub var point: Point; }\n"
            "var box: Box = init Box();\n"
            "box.point = init Point(42);\n"
            "return box.point.x;\n";
    SZrAstNode *script = parse_source(source);
    SZrFunction *function;
    TZrInt64 result = 0;
    TZrBool foundAlias = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(g_state, script);
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0U; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        if ((layout->reserved0 & ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS) != 0U) {
            foundAlias = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(foundAlias);
    TEST_ASSERT_TRUE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, script);
}

static void test_compiler_lowers_struct_init_directly_into_fixed_array_element_projection(void) {
    static const char source[] =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub @constructor(x: int) { this.x = x; }\n"
            "}\n"
            "var points: Point[1];\n"
            "points[0] = init Point(42);\n";
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    const SZrSemanticIrFunction *semanticIr;
    const SZrSemanticIrInstruction *elementConstruct = ZR_NULL;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(script);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentAst = script;
    compiler.scriptAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    for (index = 0U; index < script->data.script.statements->count; index++) {
        SZrAstNode *statement = script->data.script.statements->nodes[index];
        if (statement->type == ZR_AST_STRUCT_DECLARATION) {
            ZrParser_Compiler_CompileStructDeclaration(&compiler, statement);
        } else {
            ZrParser_Statement_Compile(&compiler, statement);
        }
        if (compiler.hasError) {
            break;
        }
    }

    TEST_ASSERT_FALSE(compiler.hasError);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    semanticIr = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(semanticIr);
    for (index = 0U; index < semanticIr->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(semanticIr, index);
        const SZrParserPlace *place;
        const SZrParserPlaceProjection *projection;

        if (instruction == ZR_NULL ||
            instruction->opcode != ZR_SEMANTIC_IR_VALUE_CONSTRUCT) {
            continue;
        }
        place = ZrParser_PlaceGraph_Get(&semanticIr->places, instruction->placeId);
        if (place == ZR_NULL || place->projections.length == 0U) {
            continue;
        }
        projection = ZrParser_Place_ProjectionAt(
                place, place->projections.length - 1U);
        if (projection != ZR_NULL &&
            projection->kind == ZR_PARSER_PLACE_PROJECTION_INDEX) {
            elementConstruct = instruction;
        }
    }
    TEST_ASSERT_NOT_NULL(elementConstruct);
    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(&compiler.instructions, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_NOT_EQUAL(
                ZR_INSTRUCTION_ENUM(SET_BY_INDEX),
                (EZrInstructionCode)instruction->instruction.operationCode);
    }

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_vm_executes_struct_init_against_fixed_array_element_place(void) {
    static const char source[] =
            "struct Point {\n"
            "    pub var x: int;\n"
            "    pub @constructor(x: int) { this.x = x; }\n"
            "}\n"
            "var points: Point[1];\n"
            "points[0] = init Point(42);\n"
            "return points[0].x;\n";
    SZrAstNode *script = parse_source(source);
    SZrFunction *function;
    TZrInt64 result = 0;
    TZrBool foundIndirectAlias = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(script);
    function = ZrParser_Compiler_Compile(g_state, script);
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0U; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        if ((layout->reserved0 & ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS) != 0U) {
            foundIndirectAlias = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(foundIndirectAlias);
    TEST_ASSERT_TRUE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, script);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_parses_distinct_qualified_generic_type_and_named_arguments);
    RUN_TEST(test_init_identifier_call_remains_an_ordinary_call);
    RUN_TEST(test_new_remains_the_existing_construct_expression);
    RUN_TEST(test_init_type_requires_constructor_parentheses);
    RUN_TEST(test_bound_value_construct_maps_named_and_default_arguments);
    RUN_TEST(test_bound_value_construct_has_no_class_private_or_ambiguous_fallback);
    RUN_TEST(test_bound_value_construct_synthesizes_default_only_without_explicit_constructors);
    RUN_TEST(test_compiler_lowers_default_struct_init_into_destination_without_object_wrapper);
    RUN_TEST(test_compiler_lowers_explicit_named_struct_constructor_without_object_wrapper);
    RUN_TEST(test_vm_executes_struct_init_constructor_against_final_destination);
    RUN_TEST(test_vm_constructor_throw_unwinds_partial_inline_receiver_to_catch);
    RUN_TEST(test_compiler_lowers_struct_init_directly_into_field_projection);
    RUN_TEST(test_vm_executes_struct_init_against_inline_field_alias_place);
    RUN_TEST(test_compiler_lowers_struct_init_directly_into_fixed_array_element_projection);
    RUN_TEST(test_vm_executes_struct_init_against_fixed_array_element_place);
    return UNITY_END();
}
