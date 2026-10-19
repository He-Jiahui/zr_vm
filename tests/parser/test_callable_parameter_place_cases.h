#ifndef ZR_TEST_CALLABLE_PARAMETER_PLACE_CASES_H
#define ZR_TEST_CALLABLE_PARAMETER_PLACE_CASES_H

#include "backend_aot/backend_aot_function_table.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/place.h"

typedef struct SZrTestCallableParameterPlaceFixture {
    SZrState *state;
    SZrFunction *root;
    SZrAstNode *expectedAst;
    SZrAotFunctionTable functions;
    TZrBool emitFlagSaved;
    TZrBool oldEmitCompileTimeRuntimeSupport;
} SZrTestCallableParameterPlaceFixture;

/* Unity can leave any assertion through longjmp; ownership stays outside the case. */
static SZrTestCallableParameterPlaceFixture g_callableParameterPlace;

static void callable_parameter_place_cleanup(void) {
    if (g_callableParameterPlace.state != ZR_NULL) {
        if (g_callableParameterPlace.emitFlagSaved) {
            g_callableParameterPlace.state->global->emitCompileTimeRuntimeSupport =
                    g_callableParameterPlace.oldEmitCompileTimeRuntimeSupport;
        }
        backend_aot_release_function_table(
                g_callableParameterPlace.state, &g_callableParameterPlace.functions);
        if (g_callableParameterPlace.root != ZR_NULL) {
            ZrCore_Function_Free(
                    g_callableParameterPlace.state, g_callableParameterPlace.root);
        }
        if (g_callableParameterPlace.expectedAst != ZR_NULL) {
            ZrParser_Ast_Free(
                    g_callableParameterPlace.state, g_callableParameterPlace.expectedAst);
        }
        ZrTests_Runtime_State_Destroy(g_callableParameterPlace.state);
    }
    memset(&g_callableParameterPlace, 0, sizeof(g_callableParameterPlace));
}

static void callable_parameter_place_compile(const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    callable_parameter_place_cleanup();
    g_callableParameterPlace.state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_callableParameterPlace.state,
                                "fixture prerequisite: could not create the compiler state");
    sourceName = ZrCore_String_CreateFromNative(
            g_callableParameterPlace.state, (TZrNativeString)sourceNameText);
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName,
                                "fixture prerequisite: could not create the source name");
    g_callableParameterPlace.oldEmitCompileTimeRuntimeSupport =
            g_callableParameterPlace.state->global->emitCompileTimeRuntimeSupport;
    g_callableParameterPlace.emitFlagSaved = ZR_TRUE;
    g_callableParameterPlace.state->global->emitCompileTimeRuntimeSupport = ZR_TRUE;
    g_callableParameterPlace.root = ZrParser_Source_Compile(
            g_callableParameterPlace.state, source, strlen(source), sourceName);
    g_callableParameterPlace.state->global->emitCompileTimeRuntimeSupport =
            g_callableParameterPlace.oldEmitCompileTimeRuntimeSupport;
    g_callableParameterPlace.emitFlagSaved = ZR_FALSE;
    TEST_ASSERT_NOT_NULL_MESSAGE(g_callableParameterPlace.root,
                                "fixture prerequisite: callable parameter source did not compile");

    /* A separate parse supplies actual source ranges, never synthetic identity metadata. */
    g_callableParameterPlace.expectedAst = ZrParser_Parse(
            g_callableParameterPlace.state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_callableParameterPlace.expectedAst,
                                "fixture prerequisite: compiled source did not parse again");
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_callableParameterPlace.expectedAst->type);
    TEST_ASSERT_NOT_NULL(g_callableParameterPlace.expectedAst->data.script.statements);
    TEST_ASSERT_TRUE_MESSAGE(backend_aot_build_function_table(
                                    g_callableParameterPlace.state,
                                    g_callableParameterPlace.root,
                                    &g_callableParameterPlace.functions),
                             "fixture prerequisite: production function table is unavailable");
}

static SZrAstNode *callable_parameter_place_statement(TZrSize index, EZrAstNodeType type) {
    SZrAstNodeArray *statements =
            g_callableParameterPlace.expectedAst->data.script.statements;
    SZrAstNode *node;

    TEST_ASSERT_TRUE(index < statements->count);
    node = statements->nodes[index];
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(type, node->type);
    return node;
}

static SZrAstNode *callable_parameter_place_parameter(SZrAstNodeArray *parameters) {
    SZrAstNode *parameter;

    TEST_ASSERT_NOT_NULL(parameters);
    TEST_ASSERT_EQUAL_UINT64(1u, parameters->count);
    parameter = parameters->nodes[0];
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, parameter->type);
    TEST_ASSERT_NOT_NULL(parameter->data.parameter.name);
    TEST_ASSERT_NOT_NULL(parameter->data.parameter.name->name);
    return parameter;
}

static const SZrFunction *callable_parameter_place_function(
        const SZrAstNode *node, const char *name) {
    const SZrFunction *found = ZR_NULL;
    TZrUInt32 matches = 0u;

    for (TZrUInt32 index = 0u; index < g_callableParameterPlace.functions.count; index++) {
        const SZrFunction *function = g_callableParameterPlace.functions.entries[index].function;
        if (function == ZR_NULL || function == g_callableParameterPlace.root ||
            function->lineInSourceStart != (TZrUInt32)node->location.start.line ||
            function->lineInSourceEnd != (TZrUInt32)node->location.end.line) {
            continue;
        }
        if ((name == ZR_NULL && function->functionName != ZR_NULL) ||
            (name != ZR_NULL &&
             (function->functionName == ZR_NULL ||
              strcmp(name, ZrCore_String_GetNativeString(function->functionName)) != 0))) {
            continue;
        }
        found = function;
        matches++;
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            1u, matches, "fixture prerequisite: compiled callable must have a unique source identity");
    return found;
}

static const SZrFunctionTypedLocalBinding *callable_parameter_place_binding(
        const SZrFunction *function, const char *name) {
    const SZrFunctionTypedLocalBinding *found = ZR_NULL;
    TZrUInt32 matches = 0u;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->typedLocalBindings);
    for (TZrUInt32 index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->name != ZR_NULL &&
            strcmp(name, ZrCore_String_GetNativeString(binding->name)) == 0) {
            found = binding;
            matches++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, matches, "compiled callable must retain one named binding");
    return found;
}

static void callable_parameter_place_assert_identity(
        const SZrFunctionTypedLocalBinding *binding, TZrUInt32 slot,
        TZrUInt32 role, SZrFileRange range, const char *missingPlaceMessage) {
    TEST_ASSERT_EQUAL_UINT32(slot, binding->stackSlot);
    TEST_ASSERT_EQUAL_UINT32(role, binding->roleFlags);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(ZR_SEMANTIC_ID_INVALID, binding->symbolId,
                                 "compiled parameter must retain its declaration symbol");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(ZR_SEMANTIC_ID_INVALID, binding->typeId,
                                 "compiled parameter must retain its canonical source type");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(ZR_PLACE_ID_INVALID, binding->placeId, missingPlaceMessage);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)range.start.line, binding->declarationStartLine);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)range.start.column, binding->declarationStartColumn);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)range.end.line, binding->declarationEndLine);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)range.end.column, binding->declarationEndColumn);
}

static void callable_parameter_place_assert_distinct_outer(
        const SZrFunctionTypedLocalBinding *parameter,
        const SZrFunctionTypedLocalBinding *outer, EZrValueType outerBaseType) {
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, outer->symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, outer->typeId);
    TEST_ASSERT_EQUAL_INT(outerBaseType, outer->type.baseType);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(outer->symbolId, parameter->symbolId,
                                 "parameter identity must not borrow the same-name outer declaration");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(outer->typeId, parameter->typeId,
                                 "different source types must not borrow the outer type identity");
}

static void test_callable_parameter_place_preserves_unused_function_parameter(void) {
    static const char source[] =
            "var value: bool = true;\n"
            "fn unused_function(value: int): int { return 7; }\n"
            "return 0;\n";
    SZrAstNode *node;
    SZrAstNode *parameter;
    const SZrFunction *function;
    const SZrFunctionTypedLocalBinding *binding;

    callable_parameter_place_compile(source, "unused_function_parameter.zr");
    node = callable_parameter_place_statement(1u, ZR_AST_FUNCTION_DECLARATION);
    parameter = callable_parameter_place_parameter(node->data.functionDeclaration.params);
    function = callable_parameter_place_function(node, "unused_function");
    TEST_ASSERT_EQUAL_UINT32(1u, function->parameterCount);
    binding = callable_parameter_place_binding(function, "value");
    callable_parameter_place_assert_identity(
            binding, 0u, ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
            parameter->location, "unused function parameter must have a canonical SemanticIR place");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, binding->type.baseType);
    callable_parameter_place_assert_distinct_outer(
            binding, callable_parameter_place_binding(g_callableParameterPlace.root, "value"),
            ZR_VALUE_TYPE_BOOL);
}

static void test_callable_parameter_place_preserves_unused_generic_parameter(void) {
    static const char source[] =
            "class RefA { }\n"
            "var value: RefA;\n"
            "fn unused_generic<T>(value: T): int where T: class { return 7; }\n"
            "return 0;\n";
    SZrAstNode *node;
    SZrAstNode *parameter;
    const SZrFunction *function;
    const SZrFunctionTypedLocalBinding *binding;
    const SZrFunctionTypedLocalBinding *outer;

    callable_parameter_place_compile(source, "unused_generic_parameter.zr");
    node = callable_parameter_place_statement(2u, ZR_AST_FUNCTION_DECLARATION);
    parameter = callable_parameter_place_parameter(node->data.functionDeclaration.params);
    function = callable_parameter_place_function(node, "unused_generic");
    TEST_ASSERT_EQUAL_UINT32(1u, function->parameterCount);
    binding = callable_parameter_place_binding(function, "value");
    callable_parameter_place_assert_identity(
            binding, 0u, ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
            parameter->location, "unused generic parameter must have a canonical SemanticIR place");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, binding->type.baseType);
    TEST_ASSERT_NOT_NULL(binding->type.typeName);
    TEST_ASSERT_EQUAL_STRING("T", ZrCore_String_GetNativeString(binding->type.typeName));
    outer = callable_parameter_place_binding(g_callableParameterPlace.root, "value");
    callable_parameter_place_assert_distinct_outer(binding, outer, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(outer->type.typeName);
    TEST_ASSERT_EQUAL_STRING("RefA", ZrCore_String_GetNativeString(outer->type.typeName));
}

static void test_callable_parameter_place_preserves_unused_member_and_receiver(void) {
    static const char source[] =
            "var value: bool = true;\n"
            "resource class Service {\n"
            "    pub const fn unused_member(value: int): int { return 7; }\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *declaration;
    SZrAstNode *member;
    SZrAstNode *parameter;
    const SZrFunction *function;
    const SZrFunctionTypedLocalBinding *binding;
    const SZrFunctionTypedLocalBinding *receiver;

    callable_parameter_place_compile(source, "unused_member_parameter.zr");
    declaration = callable_parameter_place_statement(1u, ZR_AST_CLASS_DECLARATION);
    TEST_ASSERT_NOT_NULL(declaration->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT64(1u, declaration->data.classDeclaration.members->count);
    member = declaration->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, member->type);
    parameter = callable_parameter_place_parameter(member->data.classMethod.params);
    function = callable_parameter_place_function(member, "unused_member");
    TEST_ASSERT_EQUAL_UINT32(2u, function->parameterCount);
    binding = callable_parameter_place_binding(function, "value");
    callable_parameter_place_assert_identity(
            binding, 1u, ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
            parameter->location, "unused member parameter must have a canonical SemanticIR place");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, binding->type.baseType);
    receiver = callable_parameter_place_binding(function, "this");
    callable_parameter_place_assert_identity(
            receiver, 0u, ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER, member->location,
            "unused receiver must have a canonical SemanticIR place");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, receiver->type.baseType);
    TEST_ASSERT_NOT_NULL(receiver->type.typeName);
    TEST_ASSERT_EQUAL_STRING("Service", ZrCore_String_GetNativeString(receiver->type.typeName));
    TEST_ASSERT_NOT_EQUAL(receiver->symbolId, binding->symbolId);
    TEST_ASSERT_NOT_EQUAL(receiver->placeId, binding->placeId);
    callable_parameter_place_assert_distinct_outer(
            binding, callable_parameter_place_binding(g_callableParameterPlace.root, "value"),
            ZR_VALUE_TYPE_BOOL);
}

static void test_callable_parameter_place_preserves_unused_lambda_parameter(void) {
    static const char source[] =
            "fn outer(value: bool): int {\n"
            "    var child = fn(value: int) => { return 7; };\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *outerNode;
    SZrAstNode *outerParameter;
    SZrAstNode *body;
    SZrAstNode *declaration;
    SZrAstNode *lambda;
    SZrAstNode *parameter;
    const SZrFunction *function;
    const SZrFunction *outerFunction;
    const SZrFunctionTypedLocalBinding *binding;
    const SZrFunctionTypedLocalBinding *outer;

    callable_parameter_place_compile(source, "unused_lambda_parameter.zr");
    outerNode = callable_parameter_place_statement(0u, ZR_AST_FUNCTION_DECLARATION);
    outerParameter = callable_parameter_place_parameter(outerNode->data.functionDeclaration.params);
    body = outerNode->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(2u, body->data.block.body->count);
    declaration = body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    lambda = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    parameter = callable_parameter_place_parameter(lambda->data.lambdaExpression.params);
    function = callable_parameter_place_function(lambda, ZR_NULL);
    TEST_ASSERT_EQUAL_UINT32(1u, function->parameterCount);
    binding = callable_parameter_place_binding(function, "value");
    callable_parameter_place_assert_identity(
            binding, 0u, ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
            parameter->location, "unused lambda parameter must have a canonical SemanticIR place");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, binding->type.baseType);
    outerFunction = callable_parameter_place_function(outerNode, "outer");
    TEST_ASSERT_EQUAL_UINT32(1u, outerFunction->parameterCount);
    outer = callable_parameter_place_binding(outerFunction, "value");
    callable_parameter_place_assert_identity(
            outer, 0u, ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
            outerParameter->location, "outer parameter must retain its own canonical SemanticIR place");
    callable_parameter_place_assert_distinct_outer(binding, outer, ZR_VALUE_TYPE_BOOL);
}

#endif
