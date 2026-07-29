#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *g_state;

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

static SZrAstNode *parse_source(const char *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reflection_type_surface.zr");
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
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

static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *compiler = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(compiler);
    ZrParser_CompilerState_Init(compiler, g_state);
    return compiler;
}

static void destroy_compiler_state(SZrCompilerState *compiler) {
    if (compiler == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(compiler);
    free(compiler);
}

static TZrUInt32 count_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 count = 0u;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }
    return count;
}

static TZrBool function_contains_type_id_constant(SZrFunction *function) {
    if (function == ZR_NULL || function->constantValueList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->constantValueLength; index++) {
        SZrTypeValue *constant = &function->constantValueList[index];

        if (constant->type == ZR_VALUE_TYPE_OBJECT && constant->value.object != ZR_NULL &&
            ZrCore_Reflection_IsTypeIdObject(g_state, ZR_CAST_OBJECT(g_state, constant->value.object))) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const SZrTypeValue *object_field(SZrObject *object, const char *fieldName) {
    SZrString *fieldString = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)fieldName);
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(
            g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(g_state, object, &key);
}

static void object_set_field(
        SZrObject *object,
        const char *fieldName,
        const SZrTypeValue *value) {
    SZrString *fieldString = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)fieldName);
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(fieldString);
    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_NOT_NULL(value);
    ZrCore_Value_InitAsRawObject(
            g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(g_state, object, &key, value);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, g_state->threadStatus);
}

static SZrObject *array_object_entry(SZrObject *array, TZrUInt32 index) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_ARRAY, array->internalType);
    ZrCore_Value_InitAsInt(g_state, &key, (TZrInt64)index);
    value = ZrCore_Object_GetValue(g_state, array, &key);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    return ZR_CAST_OBJECT(g_state, value->value.object);
}

static const char *reflection_object_name(SZrObject *object) {
    const SZrTypeValue *value = object_field(object, "name");

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    return ZrCore_String_GetNativeString(
            ZR_CAST_STRING(g_state, value->value.object));
}

static SZrObjectPrototype *module_prototype(
        SZrObjectModule *module,
        const char *name) {
    SZrString *exportName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)name);
    const SZrTypeValue *value = ZrCore_Module_GetPubExport(
            g_state, module, exportName);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL_MESSAGE(value, name);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    object = ZR_CAST_OBJECT(g_state, value->value.object);
    TEST_ASSERT_EQUAL_INT(
            ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE, object->internalType);
    return (SZrObjectPrototype *)object;
}

static SZrTypePrototypeInfo *find_type_prototype(
        SZrCompilerState *compiler,
        const char *typeName) {
    if (compiler == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0u; index < compiler->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype = (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                &compiler->typePrototypes, index);

        if (prototype != ZR_NULL && prototype->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(prototype->name), typeName) == 0) {
            return prototype;
        }
    }
    return ZR_NULL;
}

static TZrBool prototype_directly_inherits(
        SZrTypePrototypeInfo *prototype,
        const char *parentTypeName) {
    if (prototype == ZR_NULL || parentTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < prototype->inherits.length; index++) {
        SZrString **inheritName = (SZrString **)ZrCore_Array_Get(
                &prototype->inherits, index);

        if (inheritName != ZR_NULL && *inheritName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(*inheritName), parentTypeName) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static EZrReflectionTypeCategory reflection_category_for_name(
        SZrCompilerState *compiler,
        const char *typeName) {
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)typeName);
    SZrInferredType type;
    EZrReflectionTypeCategory category;

    ZrParser_InferredType_InitFull(
            g_state, &type, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, name);
    category = ZrParser_ReflectionTypeCategory_FromInferred(compiler, &type);
    ZrParser_InferredType_Free(g_state, &type);
    return category;
}

static void test_reflection_query_keywords_have_dedicated_tokens(void) {
    const char *source = "typeid typeof";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "tokens.zr");
    SZrLexState lexer;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), sourceName);
    TEST_ASSERT_EQUAL_INT(ZR_TK_TYPEID, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_TYPEOF, lexer.t.token);
}

static void test_typeid_parses_operand_in_type_ref_context(void) {
    SZrAstNode *script = parse_source(
            "typeid(Dictionary<string, Array<int>>);\n");
    SZrAstNode *query = statement_expression(script, 0u);

    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_QUERY_EXPRESSION, query->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_TYPE_QUERY_CANONICAL_IDENTITY,
            query->data.typeQueryExpression.kind);
    TEST_ASSERT_NOT_NULL(query->data.typeQueryExpression.typeOperand);
    TEST_ASSERT_NULL(query->data.typeQueryExpression.operand);
    ZrParser_Ast_Free(g_state, script);
}

static void test_typeof_parses_operand_as_expression(void) {
    SZrAstNode *script = parse_source("typeof(makeValue());\n");
    SZrAstNode *query = statement_expression(script, 0u);

    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_QUERY_EXPRESSION, query->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_TYPE_QUERY_RUNTIME_DESCRIPTOR,
            query->data.typeQueryExpression.kind);
    TEST_ASSERT_NOT_NULL(query->data.typeQueryExpression.operand);
    TEST_ASSERT_NULL(query->data.typeQueryExpression.typeOperand);
    ZrParser_Ast_Free(g_state, script);
}

static void test_type_queries_infer_generic_identity_and_precise_descriptor_types(void) {
    SZrAstNode *script = parse_source("typeid(int);\ntypeof(7);\n");
    SZrCompilerState *compiler = create_compiler_state();
    SZrInferredType identityType;
    SZrInferredType descriptorType;
    SZrInferredType *identityArgument;
    SZrInferredType *descriptorArgument;

    compiler->scriptAst = script;
    ZrParser_InferredType_Init(g_state, &identityType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &descriptorType, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            compiler, statement_expression(script, 0u), &identityType));
    TEST_ASSERT_NOT_NULL(identityType.typeName);
    TEST_ASSERT_EQUAL_STRING(
            "zr.reflection.TypeId", ZrCore_String_GetNativeString(identityType.typeName));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)identityType.elementTypes.length);
    identityArgument = (SZrInferredType *)ZrCore_Array_Get(&identityType.elementTypes, 0u);
    TEST_ASSERT_NOT_NULL(identityArgument);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, identityArgument->baseType);

    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            compiler, statement_expression(script, 1u), &descriptorType));
    TEST_ASSERT_NOT_NULL(descriptorType.typeName);
    TEST_ASSERT_EQUAL_STRING(
            "zr.reflection.declaration.StructTypeOf",
            ZrCore_String_GetNativeString(descriptorType.typeName));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)descriptorType.elementTypes.length);
    descriptorArgument = (SZrInferredType *)ZrCore_Array_Get(&descriptorType.elementTypes, 0u);
    TEST_ASSERT_NOT_NULL(descriptorArgument);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, descriptorArgument->baseType);

    ZrParser_InferredType_Free(g_state, &descriptorType);
    ZrParser_InferredType_Free(g_state, &identityType);
    destroy_compiler_state(compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_precise_descriptor_registers_complete_reflection_hierarchy(void) {
    SZrAstNode *script = parse_source("typeof(7);\n");
    SZrCompilerState *compiler = create_compiler_state();
    SZrInferredType descriptorType;
    SZrTypePrototypeInfo *descriptorPrototype;
    SZrTypePrototypeInfo *typeOfPrototype;
    SZrTypePrototypeInfo *typePrototype;

    compiler->scriptAst = script;
    ZrParser_InferredType_Init(g_state, &descriptorType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            compiler, statement_expression(script, 0u), &descriptorType));

    descriptorPrototype = find_type_prototype(
            compiler, "zr.reflection.declaration.StructTypeOf");
    typeOfPrototype = find_type_prototype(compiler, "zr.reflection.TypeOf");
    typePrototype = find_type_prototype(compiler, "zr.reflection.Type");
    TEST_ASSERT_NOT_NULL(descriptorPrototype);
    TEST_ASSERT_NOT_NULL(typeOfPrototype);
    TEST_ASSERT_NOT_NULL(typePrototype);
    TEST_ASSERT_TRUE(prototype_directly_inherits(
            descriptorPrototype, "zr.reflection.TypeOf"));
    TEST_ASSERT_TRUE(prototype_directly_inherits(
            typeOfPrototype, "zr.reflection.Type"));

    ZrParser_InferredType_Free(g_state, &descriptorType);
    destroy_compiler_state(compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_typeof_null_erases_to_base_reflection_type(void) {
    SZrAstNode *script = parse_source("typeof(null);\n");
    SZrCompilerState *compiler = create_compiler_state();
    SZrInferredType descriptorType;

    compiler->scriptAst = script;
    ZrParser_InferredType_Init(g_state, &descriptorType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(
            compiler, statement_expression(script, 0u), &descriptorType));
    TEST_ASSERT_NOT_NULL(descriptorType.typeName);
    TEST_ASSERT_EQUAL_STRING(
            "zr.reflection.Type",
            ZrCore_String_GetNativeString(descriptorType.typeName));
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)descriptorType.elementTypes.length);

    ZrParser_InferredType_Free(g_state, &descriptorType);
    destroy_compiler_state(compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_declaration_facts_drive_precise_reflection_categories(void) {
    SZrAstNode *script = parse_source(
            "abstract class Base {}\n"
            "class Concrete { pub @constructor(value: int) {} }\n"
            "class Instance { pub @constructor() {} }\n"
            "class Generic<T> { pub @constructor() {} }\n"
            "interface Contract {}\n"
            "resource class Owned { pub @constructor() {} }\n"
            "ref struct Window { var value: int; }\n");
    SZrCompilerState *compiler = create_compiler_state();

    compiler->scriptAst = script;
    compiler->currentAst = script;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);

    ZrParser_Compiler_CompileClassDeclaration(compiler, script_statement(script, 0u));
    ZrParser_Compiler_CompileClassDeclaration(compiler, script_statement(script, 1u));
    ZrParser_Compiler_CompileClassDeclaration(compiler, script_statement(script, 2u));
    ZrParser_Compiler_CompileClassDeclaration(compiler, script_statement(script, 3u));
    ZrParser_Compiler_CompileInterfaceDeclaration(compiler, script_statement(script, 4u));
    ZrParser_Compiler_CompileClassDeclaration(compiler, script_statement(script, 5u));
    ZrParser_Compiler_CompileStructDeclaration(compiler, script_statement(script, 6u));
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);

    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_CLASS,
            reflection_category_for_name(compiler, "Base"));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS,
            reflection_category_for_name(compiler, "Concrete"));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS,
            reflection_category_for_name(compiler, "Instance"));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_CLASS,
            reflection_category_for_name(compiler, "Generic"));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_INTERFACE,
            reflection_category_for_name(compiler, "Contract"));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS,
            reflection_category_for_name(compiler, "Owned"));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT,
            reflection_category_for_name(compiler, "Window"));

    ZrCore_Function_Free(g_state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    destroy_compiler_state(compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_typeid_is_available_to_compile_time_evaluation(void) {
    SZrAstNode *script = parse_source("typeid(int);\n");
    SZrCompilerState *compiler = create_compiler_state();
    SZrTypeValue result;

    compiler->scriptAst = script;
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrParser_Compiler_EvaluateCompileTimeExpression(
            compiler, statement_expression(script, 0u), &result));
    TEST_ASSERT_FALSE(compiler->hasError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
    TEST_ASSERT_NOT_NULL(result.value.object);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsTypeIdObject(
            g_state, ZR_CAST_OBJECT(g_state, result.value.object)));

    destroy_compiler_state(compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_typeid_is_a_constant_and_typeof_lowers_once(void) {
    SZrAstNode *script = parse_source("typeid(int);\ntypeof(7);\n");
    SZrFunction *function = ZrParser_Compiler_Compile(g_state, script);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_type_id_constant(function));
    TEST_ASSERT_EQUAL_UINT32(1u, count_opcode(function, ZR_INSTRUCTION_ENUM(TYPEOF)));

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, script);
}

static void test_typeof_evaluates_runtime_operand_exactly_once(void) {
    const char *source =
            "var count = 0;\n"
            "typeof(count = count + 1);\n"
            "return count;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reflection_typeof_once.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(g_state, function);
}

static void test_member_query_filters_orders_and_selects_overloads(void) {
    const char *source =
            "%module \"reflection_query\";\n"
            "pub class Base {\n"
            "  pub var inheritedField: int;\n"
            "  pub fn same(value: int): int { return value; }\n"
            "  pub fn convert(value: int): int { return value; }\n"
            "}\n"
            "pub class Child: Base {\n"
            "  pub var directField: int;\n"
            "  pri var hiddenField: int;\n"
            "  pub static var staticField: int = 1;\n"
            "  pub shadow fn same(value: int): int { return value + 1; }\n"
            "  pub fn convert(value: string): int { return 1; }\n"
            "  pub @constructor() {}\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reflection_query.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    SZrObjectModule *module;
    SZrObjectPrototype *childPrototype;
    SZrTypeValue prototypeValue;
    SZrTypeValue descriptorValue;
    SZrObject *descriptor;
    SZrReflectionMemberQuery query;
    EZrReflectionQueryStatus status;
    SZrObject *members = ZR_NULL;
    SZrObject *member = ZR_NULL;
    SZrString *name;
    SZrReflectionTypeIdentity identity = {0};
    SZrObject *intTypeId;
    SZrObject *stringTypeId;
    const SZrObject *parameterTypeIds[1];

    TEST_ASSERT_NOT_NULL(function);
    module = ZrCore_Module_Create(g_state);
    TEST_ASSERT_NOT_NULL(module);
    ZrCore_Module_SetInfo(
            g_state,
            module,
            ZrCore_String_CreateFromNative(g_state, "reflection_query"),
            ZrCore_Module_CalculatePathHash(g_state, sourceName),
            sourceName);
    TEST_ASSERT_EQUAL_UINT64(
            2u,
            ZrCore_Module_CreatePrototypesFromData(g_state, module, function));
    childPrototype = module_prototype(module, "Child");

    ZrCore_Value_InitAsRawObject(
            g_state,
            &prototypeValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(childPrototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&descriptorValue);
    TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
            g_state, &prototypeValue, &descriptorValue));
    descriptor = ZR_CAST_OBJECT(g_state, descriptorValue.value.object);
    TEST_ASSERT_NOT_NULL(descriptor);

    ZrCore_Reflection_MemberQueryInitDefault(&query);
    TEST_ASSERT_TRUE(ZrCore_Reflection_QueryMembers(
            g_state,
            descriptor,
            ZR_REFLECTION_MEMBER_KIND_FIELD,
            &query,
            &members,
            &status));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_QUERY_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(3u, (TZrUInt32)members->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_STRING(
            "directField", reflection_object_name(array_object_entry(members, 0u)));
    TEST_ASSERT_EQUAL_STRING(
            "staticField", reflection_object_name(array_object_entry(members, 1u)));
    TEST_ASSERT_EQUAL_STRING(
            "inheritedField", reflection_object_name(array_object_entry(members, 2u)));

    query.storage = ZR_REFLECTION_MEMBER_STORAGE_STATIC;
    TEST_ASSERT_TRUE(ZrCore_Reflection_QueryMembers(
            g_state,
            descriptor,
            ZR_REFLECTION_MEMBER_KIND_FIELD,
            &query,
            &members,
            &status));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)members->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_STRING(
            "staticField", reflection_object_name(array_object_entry(members, 0u)));

    ZrCore_Reflection_MemberQueryInitDefault(&query);
    query.access = ZR_REFLECTION_MEMBER_ACCESS_PRIVATE;
    TEST_ASSERT_FALSE(ZrCore_Reflection_QueryMembers(
            g_state,
            descriptor,
            ZR_REFLECTION_MEMBER_KIND_FIELD,
            &query,
            &members,
            &status));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_QUERY_STATUS_ACCESS_DENIED, status);
    query.hasNonPublicAccessCapability = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_Reflection_QueryMembers(
            g_state,
            descriptor,
            ZR_REFLECTION_MEMBER_KIND_FIELD,
            &query,
            &members,
            &status));
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)members->nodeMap.elementCount);
    TEST_ASSERT_EQUAL_STRING(
            "hiddenField", reflection_object_name(array_object_entry(members, 0u)));

    intTypeId = ZrCore_Reflection_BuildTypeIdObject(
            g_state,
            ZrCore_String_CreateFromNative(g_state, "int"),
            &identity);
    stringTypeId = ZrCore_Reflection_BuildTypeIdObject(
            g_state,
            ZrCore_String_CreateFromNative(g_state, "string"),
            &identity);
    ZrCore_Reflection_MemberQueryInitDefault(&query);
    name = ZrCore_String_CreateFromNative(g_state, "same");
    parameterTypeIds[0] = intTypeId;
    TEST_ASSERT_FALSE(ZrCore_Reflection_GetMember(
            g_state,
            descriptor,
            name,
            ZR_REFLECTION_MEMBER_KIND_METHOD,
            parameterTypeIds,
            1u,
            &query,
            &member,
            &status));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_QUERY_STATUS_AMBIGUOUS, status);

    name = ZrCore_String_CreateFromNative(g_state, "convert");
    parameterTypeIds[0] = stringTypeId;
    TEST_ASSERT_TRUE(ZrCore_Reflection_GetMember(
            g_state,
            descriptor,
            name,
            ZR_REFLECTION_MEMBER_KIND_METHOD,
            parameterTypeIds,
            1u,
            &query,
            &member,
            &status));
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_STRING("Child", reflection_object_name(
            object_field(member, "owner") != ZR_NULL
                    ? ZR_CAST_OBJECT(g_state, object_field(member, "owner")->value.object)
                    : member));

    parameterTypeIds[0] = intTypeId;
    TEST_ASSERT_TRUE(ZrCore_Reflection_GetMember(
            g_state,
            descriptor,
            name,
            ZR_REFLECTION_MEMBER_KIND_METHOD,
            parameterTypeIds,
            1u,
            &query,
            &member,
            &status));
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_STRING("Base", reflection_object_name(
            object_field(member, "owner") != ZR_NULL
                    ? ZR_CAST_OBJECT(g_state, object_field(member, "owner")->value.object)
                    : member));

    ZrCore_Function_Free(g_state, function);
}

static void test_reflection_construction_binds_public_constructor_and_rejects_invalid_categories(void) {
    const char *source =
            "%module \"reflection_construction\";\n"
            "pub class Box {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "pub struct Point {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "pub abstract class AbstractValue {}\n"
            "pub interface Contract {}\n"
            "pub resource class ResourceValue { pub @constructor() {} }\n"
            "pub ref struct Window { pub var value: int; }\n";
    const char *constructibleNames[] = {"Box", "Point"};
    const char *rejectedNames[] = {
            "AbstractValue", "Contract", "ResourceValue", "Window"};
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reflection_construction.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    SZrObjectModule *module;
    SZrTypeValue argument;
    EZrReflectionConstructionStatus status;

    TEST_ASSERT_NOT_NULL(function);
    module = ZrCore_Module_Create(g_state);
    TEST_ASSERT_NOT_NULL(module);
    ZrCore_Module_SetInfo(
            g_state,
            module,
            ZrCore_String_CreateFromNative(g_state, "reflection_construction"),
            ZrCore_Module_CalculatePathHash(g_state, sourceName),
            sourceName);
    TEST_ASSERT_TRUE(
            ZrCore_Module_CreatePrototypesFromData(g_state, module, function) >= 5u);
    ZrCore_Value_InitAsInt(g_state, &argument, 42);

    for (TZrUInt32 index = 0u; index < 2u; index++) {
        SZrObjectPrototype *prototype = module_prototype(
                module, constructibleNames[index]);
        SZrTypeValue prototypeValue;
        SZrTypeValue descriptorValue;
        SZrTypeValue result;
        SZrObject *descriptor;
        SZrObject *instance;
        const SZrTypeValue *fieldValue;

        ZrCore_Value_InitAsRawObject(
                g_state,
                &prototypeValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
        prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_ResetAsNull(&descriptorValue);
        TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
                g_state, &prototypeValue, &descriptorValue));
        descriptor = ZR_CAST_OBJECT(g_state, descriptorValue.value.object);
        TEST_ASSERT_TRUE(ZrCore_Reflection_RequireConstructible(
                g_state, descriptor, &status));
        TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_CONSTRUCTION_STATUS_OK, status);
        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_TRUE(ZrCore_Reflection_CreateInstance(
                g_state, descriptor, &argument, 1u, &result, &status));
        TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_CONSTRUCTION_STATUS_OK, status);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
        instance = ZR_CAST_OBJECT(g_state, result.value.object);
        TEST_ASSERT_EQUAL_PTR(prototype, instance->prototype);
        TEST_ASSERT_EQUAL_INT(
                index == 0u ? ZR_OBJECT_INTERNAL_TYPE_OBJECT
                            : ZR_OBJECT_INTERNAL_TYPE_STRUCT,
                instance->internalType);
        fieldValue = object_field(instance, "value");
        TEST_ASSERT_NOT_NULL(fieldValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(fieldValue->type));
        TEST_ASSERT_EQUAL_INT64(42, fieldValue->value.nativeObject.nativeInt64);

        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_FALSE(ZrCore_Reflection_CreateInstance(
                g_state, descriptor, ZR_NULL, 0u, &result, &status));
        TEST_ASSERT_EQUAL_INT(
                ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_NOT_FOUND,
                status);
    }

    for (TZrUInt32 index = 0u; index < 4u; index++) {
        SZrObjectPrototype *prototype = ZR_NULL;
        SZrTypeValue prototypeValue;
        SZrTypeValue descriptorValue;
        SZrObject *descriptor;

        if (strcmp(rejectedNames[index], "Contract") == 0) {
            SZrReflectionTypeIdentity identity = {0};
            SZrObject *typeId;

            identity.canonicalTypeId = 900u + index;
            identity.category = ZR_REFLECTION_TYPE_CATEGORY_INTERFACE;
            typeId = ZrCore_Reflection_BuildTypeIdObject(
                    g_state,
                    ZrCore_String_CreateFromNative(
                            g_state, (TZrNativeString)rejectedNames[index]),
                    &identity);
            TEST_ASSERT_NOT_NULL(typeId);
            descriptor = ZrCore_Reflection_ResolveTypeIdObject(g_state, typeId);
        } else {
            prototype = module_prototype(module, rejectedNames[index]);
            ZrCore_Value_InitAsRawObject(
                    g_state,
                    &prototypeValue,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
            prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
            ZrCore_Value_ResetAsNull(&descriptorValue);
            TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
                    g_state, &prototypeValue, &descriptorValue));
            descriptor = ZR_CAST_OBJECT(g_state, descriptorValue.value.object);
        }
        TEST_ASSERT_NOT_NULL(descriptor);
        TEST_ASSERT_FALSE(ZrCore_Reflection_RequireConstructible(
                g_state, descriptor, &status));
        TEST_ASSERT_EQUAL_INT(
                ZR_REFLECTION_CONSTRUCTION_STATUS_TYPE_NOT_CONSTRUCTIBLE,
                status);
    }

    ZrCore_Function_Free(g_state, function);
}

static void test_reflection_constructor_binder_caches_success_and_negative_plans(void) {
    const char *source =
            "%module \"reflection_constructor_cache\";\n"
            "pub class TwoArgs {\n"
            "  pub var sum: int;\n"
            "  pub @constructor(first: int, second: int) {\n"
            "    this.sum = first + second;\n"
            "  }\n"
            "}\n"
            "pub class Ambiguous {\n"
            "  pub @constructor(value: int) {}\n"
            "  pub @constructor(value: string) {}\n"
            "}\n"
            "pub class TrulyAmbiguous {\n"
            "  pub @constructor(first: object) {}\n"
            "  pub @constructor(second: object) {}\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reflection_constructor_cache.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    SZrObjectModule *module;
    SZrObject *twoArgsDescriptor;
    SZrObject *overloadedDescriptor;
    SZrObject *ambiguousDescriptor;
    SZrTypeValue arguments[2];
    SZrTypeValue result;
    EZrReflectionConstructionStatus status;
    SZrReflectionConstructionCacheStats stats;

    TEST_ASSERT_NOT_NULL(function);
    module = ZrCore_Module_Create(g_state);
    TEST_ASSERT_NOT_NULL(module);
    ZrCore_Module_SetInfo(
            g_state,
            module,
            ZrCore_String_CreateFromNative(
                    g_state, "reflection_constructor_cache"),
            ZrCore_Module_CalculatePathHash(g_state, sourceName),
            sourceName);
    TEST_ASSERT_TRUE(
            ZrCore_Module_CreatePrototypesFromData(g_state, module, function) >=
            3u);

    {
        SZrTypeValue prototypeValue;
        SZrTypeValue descriptorValue;

        ZrCore_Value_InitAsRawObject(
                g_state,
                &prototypeValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(module_prototype(module, "TwoArgs")));
        prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
        TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
                g_state, &prototypeValue, &descriptorValue));
        twoArgsDescriptor = ZR_CAST_OBJECT(
                g_state, descriptorValue.value.object);

        ZrCore_Value_InitAsRawObject(
                g_state,
                &prototypeValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(module_prototype(module, "Ambiguous")));
        prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
        TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
                g_state, &prototypeValue, &descriptorValue));
        overloadedDescriptor = ZR_CAST_OBJECT(
                g_state, descriptorValue.value.object);

        ZrCore_Value_InitAsRawObject(
                g_state,
                &prototypeValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(
                        module_prototype(module, "TrulyAmbiguous")));
        prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
        TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(
                g_state, &prototypeValue, &descriptorValue));
        ambiguousDescriptor = ZR_CAST_OBJECT(
                g_state, descriptorValue.value.object);
    }

    ZrCore_Value_InitAsInt(g_state, &arguments[0], 19);
    ZrCore_Value_InitAsInt(g_state, &arguments[1], 23);
    ZrCore_Reflection_DebugResetConstructionCacheStats();
    for (TZrUInt32 iteration = 0u; iteration < 2u; iteration++) {
        const SZrTypeValue *sum;

        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_TRUE(ZrCore_Reflection_CreateInstance(
                g_state,
                twoArgsDescriptor,
                arguments,
                2u,
                &result,
                &status));
        TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_CONSTRUCTION_STATUS_OK, status);
        sum = object_field(
                ZR_CAST_OBJECT(g_state, result.value.object), "sum");
        TEST_ASSERT_NOT_NULL(sum);
        TEST_ASSERT_EQUAL_INT64(42, sum->value.nativeObject.nativeInt64);

        TEST_ASSERT_FALSE(ZrCore_Reflection_CreateInstance(
                g_state,
                twoArgsDescriptor,
                ZR_NULL,
                0u,
                &result,
                &status));
        TEST_ASSERT_EQUAL_INT(
                ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_NOT_FOUND,
                status);
        TEST_ASSERT_TRUE(ZrCore_Reflection_CreateInstance(
                g_state,
                overloadedDescriptor,
                arguments,
                1u,
                &result,
                &status));
        TEST_ASSERT_EQUAL_INT(
                ZR_REFLECTION_CONSTRUCTION_STATUS_OK,
                status);

        ZrCore_Value_InitAsRawObject(
                g_state,
                &arguments[0],
                ZR_CAST_RAW_OBJECT_AS_SUPER(
                        ZrCore_String_CreateFromNative(g_state, "value")));
        arguments[0].type = ZR_VALUE_TYPE_STRING;
        TEST_ASSERT_TRUE(ZrCore_Reflection_CreateInstance(
                g_state,
                overloadedDescriptor,
                arguments,
                1u,
                &result,
                &status));
        TEST_ASSERT_EQUAL_INT(
                ZR_REFLECTION_CONSTRUCTION_STATUS_OK,
                status);

        ZrCore_Value_InitAsInt(g_state, &arguments[0], 19);
        TEST_ASSERT_FALSE(ZrCore_Reflection_CreateInstance(
                g_state,
                ambiguousDescriptor,
                arguments,
                1u,
                &result,
                &status));
        TEST_ASSERT_EQUAL_INT(
                ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_AMBIGUOUS,
                status);
    }
    stats = ZrCore_Reflection_DebugGetConstructionCacheStats();
    TEST_ASSERT_EQUAL_UINT64(5u, stats.missCount);
    TEST_ASSERT_EQUAL_UINT64(5u, stats.hitCount);

    ZrCore_Function_Free(g_state, function);
}

static void test_type_identity_and_resolved_descriptor_are_canonical_per_generation(void) {
    SZrReflectionTypeIdentity identity = {0};
    SZrString *typeName = ZrCore_String_CreateFromNative(g_state, "int");
    SZrObject *firstTypeId;
    SZrObject *secondTypeId;
    SZrObject *firstDescriptor;
    SZrObject *secondDescriptor;
    SZrReflectionTypeIdentity decoded = {0};
    SZrString *decodedName = ZR_NULL;

    identity.canonicalTypeId = 17u;
    identity.metadataGeneration = 3u;
    identity.category = ZR_REFLECTION_TYPE_CATEGORY_STRUCT;
    firstTypeId = ZrCore_Reflection_BuildTypeIdObject(g_state, typeName, &identity);
    secondTypeId = ZrCore_Reflection_BuildTypeIdObject(g_state, typeName, &identity);

    TEST_ASSERT_NOT_NULL(firstTypeId);
    TEST_ASSERT_EQUAL_PTR(firstTypeId, secondTypeId);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadTypeIdObject(
            g_state, firstTypeId, &decoded, &decodedName));
    TEST_ASSERT_EQUAL_UINT32(identity.canonicalTypeId, decoded.canonicalTypeId);
    TEST_ASSERT_EQUAL_UINT32(identity.metadataGeneration, decoded.metadataGeneration);
    TEST_ASSERT_EQUAL_INT(identity.category, decoded.category);
    TEST_ASSERT_NOT_EQUAL(0u, decoded.signatureHash);
    TEST_ASSERT_NOT_NULL(decodedName);
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(decodedName));

    firstDescriptor = ZrCore_Reflection_ResolveTypeIdObject(g_state, firstTypeId);
    secondDescriptor = ZrCore_Reflection_ResolveTypeIdObject(g_state, secondTypeId);
    TEST_ASSERT_NOT_NULL(firstDescriptor);
    TEST_ASSERT_EQUAL_PTR(firstDescriptor, secondDescriptor);

    identity.metadataGeneration++;
    TEST_ASSERT_TRUE(
            firstTypeId != ZrCore_Reflection_BuildTypeIdObject(g_state, typeName, &identity));
}

static void test_type_identity_rejects_forged_category(void) {
    SZrReflectionTypeIdentity identity = {0};
    SZrObject *typeId;
    SZrTypeValue forgedCategory;
    SZrReflectionTypeIdentity decoded = {0};

    identity.canonicalTypeId = 31u;
    identity.metadataGeneration = 2u;
    identity.category = ZR_REFLECTION_TYPE_CATEGORY_STRUCT;
    typeId = ZrCore_Reflection_BuildTypeIdObject(
            g_state,
            ZrCore_String_CreateFromNative(g_state, "Forged"),
            &identity);
    TEST_ASSERT_NOT_NULL(typeId);

    ZrCore_Value_InitAsUInt(g_state, &forgedCategory, UINT32_MAX);
    object_set_field(typeId, "__zr_typeCategory", &forgedCategory);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadTypeIdObject(
            g_state, typeId, &decoded, ZR_NULL));
    TEST_ASSERT_NULL(ZrCore_Reflection_ResolveTypeIdObject(g_state, typeId));

    identity.category = (EZrReflectionTypeCategory)UINT32_MAX;
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildTypeIdObject(
            g_state,
            ZrCore_String_CreateFromNative(g_state, "Invalid"),
            &identity));
}

static void test_member_query_reports_stripped_metadata(void) {
    SZrObject *descriptor = ZrCore_Reflection_BuildTypeLiteralObject(
            g_state, ZrCore_String_CreateFromNative(g_state, "Stripped"));
    SZrTypeValue strippedMembers;
    SZrReflectionMemberQuery query;
    SZrObject *members = ZR_NULL;
    EZrReflectionQueryStatus status = ZR_REFLECTION_QUERY_STATUS_OK;

    TEST_ASSERT_NOT_NULL(descriptor);
    ZrCore_Value_ResetAsNull(&strippedMembers);
    object_set_field(descriptor, "members", &strippedMembers);
    ZrCore_Reflection_MemberQueryInitDefault(&query);
    TEST_ASSERT_FALSE(ZrCore_Reflection_QueryMembers(
            g_state,
            descriptor,
            ZR_REFLECTION_MEMBER_KIND_ANY,
            &query,
            &members,
            &status));
    TEST_ASSERT_NULL(members);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_QUERY_STATUS_METADATA_NOT_PRESERVED,
            status);
}

static void test_runtime_typeof_and_static_typeid_share_identity_and_descriptor(void) {
    SZrReflectionTypeIdentity identity = {0};
    SZrString *typeName = ZrCore_String_CreateFromNative(g_state, "int");
    SZrObject *typeId;
    SZrObject *descriptor;
    const SZrTypeValue *descriptorId;
    SZrTypeValue value;
    SZrTypeValue descriptorValue;

    identity.canonicalTypeId = 23u;
    identity.category = ZR_REFLECTION_TYPE_CATEGORY_STRUCT;
    typeId = ZrCore_Reflection_BuildTypeIdObject(g_state, typeName, &identity);
    TEST_ASSERT_NOT_NULL(typeId);

    ZrCore_Value_InitAsInt(g_state, &value, 7);
    ZrCore_Value_ResetAsNull(&descriptorValue);
    TEST_ASSERT_TRUE(ZrCore_Reflection_TypeOfValue(g_state, &value, &descriptorValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, descriptorValue.type);
    descriptor = ZR_CAST_OBJECT(g_state, descriptorValue.value.object);
    TEST_ASSERT_NOT_NULL(descriptor);

    descriptorId = object_field(descriptor, "id");
    TEST_ASSERT_NOT_NULL(descriptorId);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, descriptorId->type);
    TEST_ASSERT_EQUAL_PTR(typeId, descriptorId->value.object);
    TEST_ASSERT_EQUAL_PTR(
            descriptor, ZrCore_Reflection_ResolveTypeIdObject(g_state, typeId));
}

static void test_runtime_descriptor_members_are_callable_from_source(void) {
    const char *source =
            "struct Point {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "let point = init Point(7);\n"
            "let descriptor = typeof(point);\n"
            "descriptor.getField(\"value\");\n"
            "descriptor.createInstance(41);\n"
            "let constructionArgs = [42];\n"
            "descriptor.createInstance(...constructionArgs);\n"
            "return 1;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reflection_descriptor_source_call.zr");
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);
    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reflection_query_keywords_have_dedicated_tokens);
    RUN_TEST(test_typeid_parses_operand_in_type_ref_context);
    RUN_TEST(test_typeof_parses_operand_as_expression);
    RUN_TEST(test_type_queries_infer_generic_identity_and_precise_descriptor_types);
    RUN_TEST(test_precise_descriptor_registers_complete_reflection_hierarchy);
    RUN_TEST(test_typeof_null_erases_to_base_reflection_type);
    RUN_TEST(test_declaration_facts_drive_precise_reflection_categories);
    RUN_TEST(test_typeid_is_available_to_compile_time_evaluation);
    RUN_TEST(test_typeid_is_a_constant_and_typeof_lowers_once);
    RUN_TEST(test_typeof_evaluates_runtime_operand_exactly_once);
    RUN_TEST(test_member_query_filters_orders_and_selects_overloads);
    RUN_TEST(test_reflection_construction_binds_public_constructor_and_rejects_invalid_categories);
    RUN_TEST(test_reflection_constructor_binder_caches_success_and_negative_plans);
    RUN_TEST(test_type_identity_and_resolved_descriptor_are_canonical_per_generation);
    RUN_TEST(test_type_identity_rejects_forged_category);
    RUN_TEST(test_member_query_reports_stripped_metadata);
    RUN_TEST(test_runtime_typeof_and_static_typeid_share_identity_and_descriptor);
    RUN_TEST(test_runtime_descriptor_members_are_callable_from_source);
    return UNITY_END();
}
