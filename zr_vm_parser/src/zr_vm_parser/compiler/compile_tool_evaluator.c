#include "compile_tool_evaluator.h"

#include "compile_tool_binding.h"
#include "comptime_runtime_contract.h"
#include "zr_vm_parser/declaration_transform_contract.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/project.h"

#include <string.h>

static SZrObject *compile_tool_new_object(
        SZrCompilerState *cs,
        EZrObjectInternalType internalType,
        SZrFileRange location) {
    SZrObject *object;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !ZrParser_ComptimeRuntime_Consume(
                cs, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                sizeof(SZrObject), location) ||
        !ZrParser_ComptimeRuntime_Consume(
                cs, ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
                1U, location)) {
        return ZR_NULL;
    }
    object = internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT
                     ? ZrCore_Object_New(cs->state, ZR_NULL)
                     : ZrCore_Object_NewCustomized(
                               cs->state, sizeof(SZrObject), internalType);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(cs->state, object);
    }
    return object;
}

static TZrBool compile_tool_set_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name,
        const SZrTypeValue *value) {
    SZrString *keyString;
    SZrTypeValue key;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(cs->state, object, &key, value);
    return ZR_TRUE;
}

static const SZrParserCompileToolTypeDescriptor *compile_tool_struct_init_type(
        SZrCompilerState *cs,
        const SZrType *typeInfo) {
    const SZrCompileToolBinding *binding;

    if (cs == ZR_NULL || typeInfo == ZR_NULL || typeInfo->name == ZR_NULL ||
        typeInfo->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        typeInfo->name->data.identifier.name == ZR_NULL ||
        typeInfo->subType == ZR_NULL || typeInfo->subType->subType != ZR_NULL ||
        typeInfo->subType->name == ZR_NULL ||
        typeInfo->subType->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        typeInfo->subType->name->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }
    binding = ZrParser_CompileToolBinding_Resolve(
            cs, typeInfo->name->data.identifier.name);
    return binding != ZR_NULL &&
                           binding->kind == ZR_COMPILE_TOOL_BINDING_PROVIDER
                   ? ZrParser_CompileTool_FindType(
                             binding->provider,
                             ZrCore_String_GetNativeString(
                                     typeInfo->subType->name->data.identifier.name))
                   : ZR_NULL;
}

static TZrBool compile_tool_field_name_allowed(
        EZrParserCompileToolTypeRole role,
        SZrString *fieldName) {
    if (fieldName == ZR_NULL) {
        return ZR_FALSE;
    }
    if (role == ZR_PARSER_COMPILE_TOOL_TYPE_PATCH) {
        return (TZrBool)(ct_string_equals(fieldName, "target") ||
                         ct_string_equals(fieldName, "additions") ||
                         ct_string_equals(fieldName, "interfaceAdds") ||
                         ct_string_equals(fieldName, "attributeAdds") ||
                         ct_string_equals(fieldName, "diagnostics"));
    }
    if (role == ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_FIELD) {
        return (TZrBool)(ct_string_equals(fieldName, "name") ||
                         ct_string_equals(fieldName, "type") ||
                         ct_string_equals(fieldName, "visibility") ||
                         ct_string_equals(fieldName, "mutability") ||
                         ct_string_equals(fieldName, "initializer"));
    }
    return ZR_FALSE;
}

static TZrBool compile_tool_has_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name) {
    SZrString *keyString;
    SZrTypeValue key;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(cs->state, object, &key) != ZR_NULL;
}

static TZrBool compile_tool_add_empty_array_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name,
        SZrFileRange location) {
    SZrObject *array = compile_tool_new_object(
            cs, ZR_OBJECT_INTERNAL_TYPE_ARRAY, location);
    SZrTypeValue value;

    if (array == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
    value.type = ZR_VALUE_TYPE_ARRAY;
    return compile_tool_set_field(cs, object, name, &value);
}

static TZrBool compile_tool_try_evaluate_struct_init(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrTypeValue *result,
        TZrBool *handled) {
    static const TZrChar *const patchArrayFields[] = {
            "additions", "interfaceAdds", "attributeAdds", "diagnostics"};
    const SZrParserCompileToolTypeDescriptor *typeDescriptor;
    SZrStructInitExpression *init;
    SZrObject *object;
    SZrTypeValue roleValue;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || handled == ZR_NULL ||
        node->type != ZR_AST_STRUCT_INIT_EXPRESSION) {
        return ZR_TRUE;
    }
    init = &node->data.structInitExpression;
    typeDescriptor = compile_tool_struct_init_type(cs, init->typeInfo);
    if (typeDescriptor == ZR_NULL) {
        return ZR_TRUE;
    }
    if (typeDescriptor->role != ZR_PARSER_COMPILE_TOOL_TYPE_PATCH &&
        typeDescriptor->role != ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_FIELD) {
        *handled = ZR_TRUE;
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "compiletool.constructor: typed declaration value is not constructible in this phase",
                node->location);
        return ZR_FALSE;
    }
    *handled = ZR_TRUE;
    if (!ZrParser_ComptimeRuntime_RequireEffect(
                cs, ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD,
                node->location)) {
        return ZR_FALSE;
    }
    if (init->args == ZR_NULL || init->argNames == ZR_NULL ||
        init->args->count != init->argNames->length) {
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "compiletool.constructor: typed declaration values require named arguments",
                node->location);
        return ZR_FALSE;
    }

    object = compile_tool_new_object(
            cs, ZR_OBJECT_INTERNAL_TYPE_OBJECT, node->location);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(cs->state, &roleValue, typeDescriptor->role);
    if (!compile_tool_set_field(
                cs, object, "__zrCompileToolTypeRole", &roleValue)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < init->args->count; index++) {
        SZrString **fieldNamePtr =
                (SZrString **)ZrCore_Array_Get(init->argNames, index);
        SZrString *fieldName = fieldNamePtr != ZR_NULL ? *fieldNamePtr : ZR_NULL;
        SZrTypeValue fieldValue;

        if (!compile_tool_field_name_allowed(typeDescriptor->role, fieldName) ||
            compile_tool_has_field(
                    cs, object,
                    fieldName != ZR_NULL
                            ? ZrCore_String_GetNativeString(fieldName)
                            : ZR_NULL)) {
            ZrParser_CompileTime_Error(
                    cs, ZR_COMPILE_TIME_ERROR_ERROR,
                    "compiletool.constructor: unknown, unnamed, or duplicate field",
                    init->args->nodes[index] != ZR_NULL
                            ? init->args->nodes[index]->location
                            : node->location);
            return ZR_FALSE;
        }
        if (!evaluate_compile_time_expression_internal(
                    cs, init->args->nodes[index], frame, &fieldValue) ||
            !compile_tool_set_field(
                    cs, object,
                    ZrCore_String_GetNativeString(fieldName), &fieldValue)) {
            return ZR_FALSE;
        }
    }

    if (typeDescriptor->role == ZR_PARSER_COMPILE_TOOL_TYPE_PATCH) {
        if (!compile_tool_has_field(cs, object, "target")) {
            ZrParser_CompileTime_Error(
                    cs, ZR_COMPILE_TIME_ERROR_ERROR,
                    "declaration_transform.patch_shape: Patch.target is required",
                    node->location);
            return ZR_FALSE;
        }
        for (TZrSize index = 0;
             index < sizeof(patchArrayFields) / sizeof(patchArrayFields[0]);
             index++) {
            if (!compile_tool_has_field(cs, object, patchArrayFields[index]) &&
                !compile_tool_add_empty_array_field(
                        cs, object, patchArrayFields[index], node->location)) {
                return ZR_FALSE;
            }
        }
    } else if (!compile_tool_has_field(cs, object, "name") ||
               !compile_tool_has_field(cs, object, "type") ||
               !compile_tool_has_field(cs, object, "visibility") ||
               !compile_tool_has_field(cs, object, "mutability")) {
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.generated_field: name, type, visibility, and mutability are required",
                node->location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(
            cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool compile_tool_identifier_equals(
        const SZrAstNode *node,
        const TZrChar *literal) {
    return node != ZR_NULL &&
           node->type == ZR_AST_IDENTIFIER_LITERAL &&
           node->data.identifier.name != ZR_NULL &&
           ct_string_equals(node->data.identifier.name, literal);
}

static const SZrParserCompileToolModuleDescriptor *compile_tool_alias_descriptor(
        SZrCompilerState *cs,
        SZrString *aliasName) {
    const SZrCompileToolBinding *binding;

    if (cs == ZR_NULL || aliasName == ZR_NULL) {
        return ZR_NULL;
    }
    binding = ZrParser_CompileToolBinding_Resolve(cs, aliasName);
    return binding != ZR_NULL &&
                   binding->kind == ZR_COMPILE_TOOL_BINDING_PROVIDER
               ? binding->provider
               : ZR_NULL;
}

static TZrBool compile_tool_try_evaluate_feature(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrTypeValue *result,
        TZrBool *handled) {
    SZrPrimaryExpression *primary;
    SZrAstNode *buildMember;
    SZrAstNode *featureMember;
    SZrAstNode *callNode;
    SZrAstNode *featureNameNode;
    const SZrLibrary_Project *project;
    const SZrParserCompileToolModuleDescriptor *compileTool;
    const SZrParserCompileToolCallableDescriptor *callable;
    SZrString *featureName;
    SZrTypeValue featureNameValue;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        handled == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->property->data.identifier.name == ZR_NULL ||
        primary->members == ZR_NULL || primary->members->count != 3) {
        return ZR_TRUE;
    }

    compileTool = compile_tool_alias_descriptor(
            cs, primary->property->data.identifier.name);
    callable = ZrParser_CompileTool_FindCallable(
            compileTool, ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE);
    if (callable == ZR_NULL) {
        return ZR_TRUE;
    }

    buildMember = primary->members->nodes[0];
    featureMember = primary->members->nodes[1];
    callNode = primary->members->nodes[2];
    if (buildMember == ZR_NULL ||
        buildMember->type != ZR_AST_MEMBER_EXPRESSION ||
        buildMember->data.memberExpression.computed ||
        !compile_tool_identifier_equals(
                buildMember->data.memberExpression.property, "build") ||
        featureMember == ZR_NULL ||
        featureMember->type != ZR_AST_MEMBER_EXPRESSION ||
        featureMember->data.memberExpression.computed ||
        !compile_tool_identifier_equals(
                featureMember->data.memberExpression.property, "feature") ||
        callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_TRUE;
    }

    *handled = ZR_TRUE;
    if (!ZrParser_ComptimeRuntime_RequireEffect(
                cs, callable->effect, callNode->location)) {
        return ZR_FALSE;
    }
    if (callNode->data.functionCall.args == ZR_NULL ||
        callNode->data.functionCall.args->count != 1 ||
        callNode->data.functionCall.args->nodes[0] == ZR_NULL) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "compile.build.feature requires one string argument",
                callNode->location);
        return ZR_FALSE;
    }

    featureNameNode = callNode->data.functionCall.args->nodes[0];
    if (!evaluate_compile_time_expression_internal(
                cs, featureNameNode, frame, &featureNameValue) ||
        featureNameValue.type != ZR_VALUE_TYPE_STRING ||
        featureNameValue.value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "compile.build.feature requires one string argument",
                featureNameNode->location);
        return ZR_FALSE;
    }
    featureName = ZR_CAST_STRING(cs->state, featureNameValue.value.object);
    project = ZrLibrary_Project_GetFromGlobal(cs->state->global);
    for (TZrSize index = 0;
         project != ZR_NULL && index < project->featureSwitchCount;
         index++) {
        const SZrLibrary_ProjectFeatureSwitch *feature =
                &project->featureSwitches[index];
        if (feature->name != ZR_NULL &&
            ZrCore_String_Equal(feature->name, featureName)) {
            ZrCore_Value_InitAsUInt(
                    cs->state, result, feature->value ? 1U : 0U);
            result->type = ZR_VALUE_TYPE_BOOL;
            return ZR_TRUE;
        }
    }

    ZrParser_CompileTime_Error(
            cs,
            ZR_COMPILE_TIME_ERROR_ERROR,
            "Unknown project feature in compile.build.feature",
            featureNameNode->location);
    return ZR_FALSE;
}

static const SZrParserCompileToolCallableDescriptor *compile_tool_diagnostic_callable(
        const SZrParserCompileToolModuleDescriptor *module,
        SZrString *memberName) {
    if (memberName == ZR_NULL) {
        return ZR_NULL;
    }
    if (ct_string_equals(memberName, "assert")) {
        return ZrParser_CompileTool_FindCallable(
                module, ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT);
    }
    if (ct_string_equals(memberName, "error")) {
        return ZrParser_CompileTool_FindCallable(
                module, ZR_PARSER_COMPILE_TOOL_ROLE_ERROR);
    }
    if (ct_string_equals(memberName, "warning")) {
        return ZrParser_CompileTool_FindCallable(
                module, ZR_PARSER_COMPILE_TOOL_ROLE_WARNING);
    }
    return ZR_NULL;
}

static TZrBool compile_tool_evaluate_string_argument(
        SZrCompilerState *cs,
        SZrAstNode *argument,
        SZrCompileTimeFrame *frame,
        const TZrChar **text) {
    SZrTypeValue value;

    if (text != ZR_NULL) {
        *text = ZR_NULL;
    }
    if (argument == ZR_NULL || text == ZR_NULL ||
        !evaluate_compile_time_expression_internal(
                cs, argument, frame, &value) ||
        value.type != ZR_VALUE_TYPE_STRING ||
        value.value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    *text = ZrCore_String_GetNativeString(
            ZR_CAST_STRING(cs->state, value.value.object));
    return *text != ZR_NULL;
}

static TZrBool compile_tool_try_evaluate_diagnostic(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrTypeValue *result,
        TZrBool *handled) {
    SZrPrimaryExpression *primary;
    SZrAstNode *memberNode;
    SZrAstNode *callNode;
    SZrString *memberName;
    SZrFunctionCall *call;
    const SZrParserCompileToolModuleDescriptor *module;
    const SZrParserCompileToolCallableDescriptor *callable;
    const TZrChar *message = ZR_NULL;
    TZrBool shouldEmit = ZR_TRUE;
    EZrCompileTimeErrorLevel level = ZR_COMPILE_TIME_ERROR_ERROR;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        handled == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->members == ZR_NULL || primary->members->count != 2) {
        return ZR_TRUE;
    }
    module = compile_tool_alias_descriptor(
            cs, primary->property->data.identifier.name);
    if (module == ZR_NULL) {
        return ZR_TRUE;
    }

    memberNode = primary->members->nodes[0];
    callNode = primary->members->nodes[1];
    if (memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type !=
                ZR_AST_IDENTIFIER_LITERAL ||
        callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_TRUE;
    }
    memberName =
            memberNode->data.memberExpression.property->data.identifier.name;
    callable = compile_tool_diagnostic_callable(module, memberName);
    if (callable == ZR_NULL) {
        return ZR_TRUE;
    }

    *handled = ZR_TRUE;
    if (!ZrParser_ComptimeRuntime_RequireEffect(
                cs, callable->effect, callNode->location)) {
        return ZR_FALSE;
    }
    call = &callNode->data.functionCall;
    if (call->args == ZR_NULL) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "CompileTool diagnostic call has invalid arguments",
                callNode->location);
        return ZR_FALSE;
    }

    if (callable->role == ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT) {
        SZrTypeValue condition;
        if (call->args->count < 2 || call->args->count > 3 ||
            !evaluate_compile_time_expression_internal(
                    cs, call->args->nodes[0], frame, &condition) ||
            condition.type != ZR_VALUE_TYPE_BOOL ||
            !compile_tool_evaluate_string_argument(
                    cs, call->args->nodes[1], frame, &message)) {
            ZrParser_CompileTime_Error(
                    cs,
                    ZR_COMPILE_TIME_ERROR_ERROR,
                    "compile.assert requires (bool, string, SymbolId?)",
                    callNode->location);
            return ZR_FALSE;
        }
        shouldEmit = (TZrBool)!ct_truthy(&condition);
    } else {
        if (call->args->count < 1 || call->args->count > 2 ||
            !compile_tool_evaluate_string_argument(
                    cs, call->args->nodes[0], frame, &message)) {
            ZrParser_CompileTime_Error(
                    cs,
                    ZR_COMPILE_TIME_ERROR_ERROR,
                    "compile.error/warning requires (string, SymbolId?)",
                    callNode->location);
            return ZR_FALSE;
        }
        level = callable->role == ZR_PARSER_COMPILE_TOOL_ROLE_WARNING
                        ? ZR_COMPILE_TIME_ERROR_WARNING
                        : ZR_COMPILE_TIME_ERROR_ERROR;
    }

    if (shouldEmit) {
        if (!ZrParser_ComptimeRuntime_Consume(
                    cs,
                    ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT,
                    1U,
                    callNode->location)) {
            return ZR_FALSE;
        }
        ZrParser_CompileTime_Error(cs, level, message, callNode->location);
        if (level == ZR_COMPILE_TIME_ERROR_ERROR) {
            return ZR_FALSE;
        }
    }
    ZrCore_Value_ResetAsNull(result);
    return ZR_TRUE;
}

static TZrBool compile_tool_try_evaluate_declaration_enum(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrTypeValue *result,
        TZrBool *handled) {
    SZrPrimaryExpression *primary;
    SZrAstNode *enumMember;
    SZrAstNode *valueMember;
    const SZrParserCompileToolModuleDescriptor *module;
    SZrString *enumName;
    SZrString *valueName;
    TZrInt64 value = -1;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || handled == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_TRUE;
    }
    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->members == ZR_NULL || primary->members->count != 2) {
        return ZR_TRUE;
    }
    module = compile_tool_alias_descriptor(
            cs, primary->property->data.identifier.name);
    if (module == ZR_NULL || module->moduleName == ZR_NULL ||
        strcmp(module->moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION) != 0) {
        return ZR_TRUE;
    }
    enumMember = primary->members->nodes[0];
    valueMember = primary->members->nodes[1];
    if (enumMember == ZR_NULL || enumMember->type != ZR_AST_MEMBER_EXPRESSION ||
        enumMember->data.memberExpression.computed ||
        enumMember->data.memberExpression.property == ZR_NULL ||
        enumMember->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        valueMember == ZR_NULL || valueMember->type != ZR_AST_MEMBER_EXPRESSION ||
        valueMember->data.memberExpression.computed ||
        valueMember->data.memberExpression.property == ZR_NULL ||
        valueMember->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }
    enumName = enumMember->data.memberExpression.property->data.identifier.name;
    valueName = valueMember->data.memberExpression.property->data.identifier.name;
    if (ct_string_equals(enumName, "Visibility")) {
        value = ct_string_equals(valueName, "private")
                        ? ZR_PARSER_GENERATED_VISIBILITY_PRIVATE
                        : ct_string_equals(valueName, "protected")
                                  ? ZR_PARSER_GENERATED_VISIBILITY_PROTECTED
                                  : ct_string_equals(valueName, "public")
                                            ? ZR_PARSER_GENERATED_VISIBILITY_PUBLIC
                                            : -1;
    } else if (ct_string_equals(enumName, "Mutability")) {
        value = ct_string_equals(valueName, "let")
                        ? ZR_PARSER_GENERATED_MUTABILITY_LET
                        : ct_string_equals(valueName, "var")
                                  ? ZR_PARSER_GENERATED_MUTABILITY_VAR
                                  : -1;
    } else {
        return ZR_TRUE;
    }
    *handled = ZR_TRUE;
    if (value < 0) {
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "compiletool.enum_member: unknown declaration enum value",
                valueMember->location);
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(cs->state, result, value);
    return ZR_TRUE;
}

TZrBool ZrParser_CompileToolEvaluator_TryEvaluate(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrTypeValue *result,
        TZrBool *handled) {
    if (handled == ZR_NULL) {
        return ZR_FALSE;
    }
    *handled = ZR_FALSE;
    if (!compile_tool_try_evaluate_struct_init(
                cs, node, frame, result, handled) || *handled) {
        return !cs->hasCompileTimeError && !cs->hasError &&
               !cs->hasFatalError;
    }
    if (!compile_tool_try_evaluate_declaration_enum(
                cs, node, result, handled) || *handled) {
        return !cs->hasCompileTimeError && !cs->hasError &&
               !cs->hasFatalError;
    }
    if (!compile_tool_try_evaluate_feature(
                cs, node, frame, result, handled) || *handled) {
        return !cs->hasCompileTimeError && !cs->hasError &&
               !cs->hasFatalError;
    }
    return compile_tool_try_evaluate_diagnostic(
            cs, node, frame, result, handled);
}
