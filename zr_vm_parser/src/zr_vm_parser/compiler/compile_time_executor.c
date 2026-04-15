//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "compile_time_binding_metadata.h"
#include "compile_time_executor_internal.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

static TZrBool ct_eval_object_literal(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    SZrCompileTimeFrame *frame,
                                    SZrTypeValue *result);
static TZrBool ct_eval_assignment(SZrCompilerState *cs,
                                SZrAstNode *node,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result);
static TZrBool ct_eval_runtime_projected_call_arg(SZrCompilerState *cs,
                                                  SZrCompileTimeFunction *func,
                                                  SZrFunctionCall *call,
                                                  SZrString *paramName,
                                                  TZrSize paramIndex,
                                                  SZrCompileTimeFrame *frame,
                                                  SZrTypeValue *result);
static TZrBool ct_invoke_runtime_callable_with_values(SZrCompilerState *cs,
                                                      SZrAstNode *callSite,
                                                      const SZrTypeValue *callableValue,
                                                      TZrSize argCount,
                                                      const SZrTypeValue *argValues,
                                                      SZrTypeValue *result);
static TZrBool ct_call_runtime_projected_compile_time_function(SZrCompilerState *cs,
                                                               SZrAstNode *callSite,
                                                               SZrCompileTimeFunction *func,
                                                               SZrFunctionCall *call,
                                                               SZrCompileTimeFrame *frame,
                                                               SZrTypeValue *result);
static TZrBool ct_eval_member_access(SZrCompilerState *cs,
                                   SZrAstNode *callSite,
                                   const SZrTypeValue *baseValue,
                                   SZrMemberExpression *memberExpr,
                                   SZrCompileTimeFrame *frame,
                                   SZrTypeValue *result);
static TZrBool ct_call_value(SZrCompilerState *cs,
                           SZrAstNode *callSite,
                           const SZrTypeValue *callableValue,
                           SZrFunctionCall *call,
                           SZrCompileTimeFrame *frame,
                           SZrTypeValue *result);
static TZrBool ct_eval_call_arg(SZrCompilerState *cs,
                              SZrFunctionCall *call,
                              SZrParameter *param,
                              TZrSize paramIndex,
                              SZrCompileTimeFrame *frame,
                              SZrTypeValue *result);

static TZrBool ct_make_string_value(SZrState *state, const TZrChar *text, SZrTypeValue *result) {
    SZrString *stringValue;

    if (state == ZR_NULL || text == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stringValue = ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
    if (stringValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    result->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static TZrBool ct_set_object_field_value(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ct_make_string_value(state, fieldName, &key)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, &key, value);
    return ZR_TRUE;
}

static TZrBool ct_set_object_field_string(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          const TZrChar *valueText) {
    SZrTypeValue value;

    if (!ct_make_string_value(state, valueText != ZR_NULL ? valueText : "", &value)) {
        return ZR_FALSE;
    }

    return ct_set_object_field_value(state, object, fieldName, &value);
}

static TZrBool ct_set_object_field_bool(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *fieldName,
                                        TZrBool valueBool) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(state, &value, valueBool ? 1u : 0u);
    value.type = ZR_VALUE_TYPE_BOOL;
    return ct_set_object_field_value(state, object, fieldName, &value);
}

static TZrBool ct_set_object_field_object(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          SZrObject *fieldObject,
                                          EZrValueType valueType) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || fieldObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject));
    value.type = valueType;
    return ct_set_object_field_value(state, object, fieldName, &value);
}

static SZrString *ct_decorator_target_type_name(SZrType *typeInfo) {
    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeInfo->name->data.identifier.name;
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE &&
        typeInfo->name->data.genericType.name != ZR_NULL) {
        return typeInfo->name->data.genericType.name->name;
    }

    return ZR_NULL;
}

static const TZrChar *ct_expected_type_decorator_target_name(EZrObjectPrototypeType targetType) {
    switch (targetType) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "Class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "Struct";
        default:
            return "Object";
    }
}

static const TZrChar *ct_module_name_text(SZrCompilerState *cs) {
    SZrAstNode *moduleNode;
    SZrAstNode *nameNode;

    if (cs != ZR_NULL && cs->currentModuleKey != ZR_NULL) {
        return ZrCore_String_GetNativeString(cs->currentModuleKey);
    }

    if (cs == ZR_NULL ||
        cs->scriptAst == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT ||
        cs->scriptAst->data.script.moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNode = cs->scriptAst->data.script.moduleName;
    if (moduleNode->type != ZR_AST_MODULE_DECLARATION || moduleNode->data.moduleDeclaration.name == ZR_NULL) {
        return ZR_NULL;
    }

    nameNode = moduleNode->data.moduleDeclaration.name;
    if (nameNode->type != ZR_AST_STRING_LITERAL || nameNode->data.stringLiteral.value == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(nameNode->data.stringLiteral.value);
}

static TZrBool ct_validate_named_decorator_target_param(SZrCompilerState *cs,
                                                        SZrParameter *param,
                                                        const TZrChar *expectedName,
                                                        const TZrChar *decoratorKind,
                                                        SZrFileRange location) {
    SZrString *typeName;

    if (cs == ZR_NULL || param == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = ct_decorator_target_type_name(param->typeInfo);
    if (typeName != ZR_NULL &&
        ((expectedName != ZR_NULL && ct_string_equals(typeName, expectedName)) || ct_string_equals(typeName, "Object"))) {
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               decoratorKind != ZR_NULL
                                       ? decoratorKind
                                       : "Compile-time decorator target parameter must use a supported %type target",
                               location);
    return ZR_FALSE;
}

static TZrBool ct_validate_decorator_meta_method_target(SZrCompilerState *cs,
                                                        SZrAstNode *decorateMethod,
                                                        const TZrChar *expectedTargetName,
                                                        SZrFileRange location) {
    SZrAstNodeArray *params = ZR_NULL;

    if (decorateMethod == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorateMethod->type == ZR_AST_CLASS_META_FUNCTION) {
        params = decorateMethod->data.classMetaFunction.params;
    } else if (decorateMethod->type == ZR_AST_STRUCT_META_FUNCTION) {
        params = decorateMethod->data.structMetaFunction.params;
    }

    if (params == ZR_NULL || params->count == 0 || params->nodes[0] == ZR_NULL ||
        params->nodes[0]->type != ZR_AST_PARAMETER) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator @decorate must declare a target parameter",
                                   location);
        return ZR_FALSE;
    }

    return ct_validate_named_decorator_target_param(cs,
                                                    &params->nodes[0]->data.parameter,
                                                    expectedTargetName,
                                                    "Compile-time decorator @decorate target must use %type Class, %type Struct, %type Function, %type Field, %type Method, %type Property, %type Parameter, or %type Object",
                                                    location);
}

static SZrObject *ct_new_object(SZrState *state) {
    SZrObject *object;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(state, object);
    }
    return object;
}

static SZrObject *ct_new_array(SZrState *state) {
    SZrObject *array;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(state, array);
    }
    return array;
}

static SZrCompileTimeDecoratorClass *ct_find_compile_time_decorator_class(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < cs->compileTimeDecoratorClasses.length; i++) {
        SZrCompileTimeDecoratorClass **classPtr =
                (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get(&cs->compileTimeDecoratorClasses, i);
        if (classPtr != ZR_NULL && *classPtr != ZR_NULL && (*classPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*classPtr)->name, name)) {
            return *classPtr;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *ct_find_compile_time_meta_method(SZrAstNodeArray *members,
                                                    const TZrChar *metaName,
                                                    TZrBool isStructDecorator) {
    if (members == ZR_NULL || metaName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < members->count; i++) {
        SZrAstNode *member = members->nodes[i];
        SZrIdentifier *meta = ZR_NULL;

        if (member == ZR_NULL) {
            continue;
        }

        if (!isStructDecorator && member->type == ZR_AST_CLASS_META_FUNCTION) {
            meta = member->data.classMetaFunction.meta;
        } else if (isStructDecorator && member->type == ZR_AST_STRUCT_META_FUNCTION) {
            meta = member->data.structMetaFunction.meta;
        }

        if (meta != ZR_NULL && meta->name != ZR_NULL && ct_string_equals(meta->name, metaName)) {
            return member;
        }
    }

    return ZR_NULL;
}

static TZrBool ct_register_compile_time_decorator_class(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        SZrFileRange location) {
    SZrCompileTimeDecoratorClass *decoratorClass;
    SZrString *name = ZR_NULL;
    SZrAstNodeArray *members = ZR_NULL;
    TZrBool isStructDecorator = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_CLASS_DECLARATION) {
        if (node->data.classDeclaration.name != ZR_NULL) {
            name = node->data.classDeclaration.name->name;
        }
        members = node->data.classDeclaration.members;
    } else if (node->type == ZR_AST_STRUCT_DECLARATION) {
        if (node->data.structDeclaration.name != ZR_NULL) {
            name = node->data.structDeclaration.name->name;
        }
        members = node->data.structDeclaration.members;
        isStructDecorator = ZR_TRUE;
    } else {
        return ZR_FALSE;
    }

    if (name == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator class must have a name",
                                   location);
        return ZR_FALSE;
    }

    decoratorClass = ct_find_compile_time_decorator_class(cs, name);
    if (decoratorClass == ZR_NULL) {
        decoratorClass = (SZrCompileTimeDecoratorClass *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrCompileTimeDecoratorClass),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (decoratorClass == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, &cs->compileTimeDecoratorClasses, &decoratorClass);
    }

    ZrCore_Memory_RawSet(decoratorClass, 0, sizeof(*decoratorClass));
    decoratorClass->name = name;
    decoratorClass->declaration = node;
    decoratorClass->decorateMethod = ct_find_compile_time_meta_method(members, "decorate", isStructDecorator);
    decoratorClass->constructorMethod = ct_find_compile_time_meta_method(members, "constructor", isStructDecorator);
    decoratorClass->isStructDecorator = isStructDecorator;
    decoratorClass->location = location;
    return ZR_TRUE;
}

typedef struct SZrResolvedCompileTimeDecoratorBinding {
    SZrString *name;
    SZrCompileTimeDecoratorClass *decoratorClass;
    SZrCompileTimeFunction *decoratorFunction;
    SZrFunctionCall *constructorCall;
} SZrResolvedCompileTimeDecoratorBinding;

static SZrImportedCompileTimeModule *ct_find_imported_compile_time_module_alias(SZrCompilerState *cs,
                                                                                SZrString *aliasName) {
    if (cs == ZR_NULL || aliasName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->importedCompileTimeModuleAliases.length; index++) {
        SZrImportedCompileTimeModuleAlias *alias =
                (SZrImportedCompileTimeModuleAlias *)ZrCore_Array_Get(&cs->importedCompileTimeModuleAliases, index);
        if (alias != ZR_NULL && alias->aliasName != ZR_NULL && alias->module != ZR_NULL &&
            ZrCore_String_Equal(alias->aliasName, aliasName)) {
            return alias->module;
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeDecoratorClass *ct_find_imported_compile_time_decorator_class(
        const SZrImportedCompileTimeModule *module,
        SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeDecoratorClasses.length; index++) {
        SZrCompileTimeDecoratorClass **classPtr =
                (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get((SZrArray *)&module->compileTimeDecoratorClasses,
                                                                  index);
        if (classPtr != ZR_NULL && *classPtr != ZR_NULL && (*classPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*classPtr)->name, name)) {
            return *classPtr;
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeFunction *ct_find_imported_compile_time_function(const SZrImportedCompileTimeModule *module,
                                                                      SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get((SZrArray *)&module->compileTimeFunctions, index);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*funcPtr)->name, name)) {
            return *funcPtr;
        }
    }

    return ZR_NULL;
}

static SZrFunctionCompileTimeVariableInfo *ct_find_imported_compile_time_variable(
        const SZrImportedCompileTimeModule *module,
        SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeVariables.length; index++) {
        SZrFunctionCompileTimeVariableInfo **infoPtr =
                (SZrFunctionCompileTimeVariableInfo **)ZrCore_Array_Get((SZrArray *)&module->compileTimeVariables, index);
        if (infoPtr != ZR_NULL && *infoPtr != ZR_NULL && (*infoPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*infoPtr)->name, name)) {
            return *infoPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool ct_try_get_static_decorator_member_name(SZrCompilerState *cs,
                                                       SZrAstNode *decoratorNode,
                                                       SZrAstNode *memberNode,
                                                       SZrString **outName) {
    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }

    if (cs == ZR_NULL || decoratorNode == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                   decoratorNode != ZR_NULL ? decoratorNode->location
                                                            : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        memberNode->data.memberExpression.property->data.identifier.name == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                   decoratorNode->location);
        return ZR_FALSE;
    }

    if (outName != ZR_NULL) {
        *outName = memberNode->data.memberExpression.property->data.identifier.name;
    }
    return ZR_TRUE;
}

static TZrBool ct_try_resolve_compile_time_function_from_value(SZrCompilerState *cs,
                                                               const SZrTypeValue *value,
                                                               SZrCompileTimeFunction **outFunction) {
    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }

    if (cs == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outFunction != ZR_NULL && ct_value_try_get_compile_time_function(cs, value, outFunction)) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < cs->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, index);
        SZrTypeValue projectedValue;

        if (funcPtr == ZR_NULL || *funcPtr == ZR_NULL) {
            continue;
        }

        if (!ct_value_from_compile_time_function(cs, *funcPtr, &projectedValue)) {
            continue;
        }

        if (ZrCore_Value_Equal(cs->state, &projectedValue, (SZrTypeValue *)value)) {
            if (outFunction != ZR_NULL) {
                *outFunction = *funcPtr;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool ct_try_resolve_decorator_root_value(SZrCompilerState *cs,
                                                   SZrString *rootName,
                                                   SZrTypeValue *outValue) {
    SZrCompileTimeFunction *rootFunction = ZR_NULL;
    SZrImportedCompileTimeModule *importedModule;
    SZrObjectModule *moduleObject;

    if (cs == ZR_NULL || rootName == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrParser_Compiler_TryGetCompileTimeValue(cs, rootName, outValue)) {
        return ZR_TRUE;
    }

    rootFunction = find_compile_time_function(cs, rootName);
    if (rootFunction != ZR_NULL && ct_value_from_compile_time_function(cs, rootFunction, outValue)) {
        return ZR_TRUE;
    }

    importedModule = ct_find_imported_compile_time_module_alias(cs, rootName);
    if (importedModule == ZR_NULL || importedModule->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleObject = ZrCore_Module_ImportByPath(cs->state, importedModule->moduleName);
    if (moduleObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(moduleObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_resolve_compile_time_decorator_binding(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        SZrResolvedCompileTimeDecoratorBinding *binding) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primary = ZR_NULL;
    SZrString *rootName = ZR_NULL;
    SZrString *leafName = ZR_NULL;
    SZrImportedCompileTimeModule *importedModule = ZR_NULL;
    TZrSize memberCount = 0;
    TZrSize chainCount = 0;
    TZrBool hasValueChain = ZR_FALSE;
    SZrTypeValue currentValue;

    if (binding != ZR_NULL) {
        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
    }

    if (cs == ZR_NULL || decoratorNode == ZR_NULL || decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION) {
        return ZR_FALSE;
    }

    expr = decoratorNode->data.decoratorExpression.expr;
    if (expr == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorator expression is empty",
                                   decoratorNode->location);
        return ZR_FALSE;
    }

    if (expr->type == ZR_AST_IDENTIFIER_LITERAL) {
        rootName = expr->data.identifier.name;
        if (rootName == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorators require a named identifier path",
                                       decoratorNode->location);
            return ZR_FALSE;
        }

        if (binding != ZR_NULL) {
            binding->name = rootName;
            binding->decoratorClass = ct_find_compile_time_decorator_class(cs, rootName);
            binding->decoratorFunction = find_compile_time_function(cs, rootName);
            if (binding->decoratorClass != ZR_NULL && binding->decoratorFunction != ZR_NULL) {
                ct_error_name(cs, rootName, "Ambiguous compile-time decorator name: ", decoratorNode->location);
                return ZR_FALSE;
            }
            return (binding->decoratorClass != ZR_NULL || binding->decoratorFunction != ZR_NULL) ? ZR_TRUE
                                                                                                  : ZR_FALSE;
        }
        return (ct_find_compile_time_decorator_class(cs, rootName) != ZR_NULL ||
                find_compile_time_function(cs, rootName) != ZR_NULL)
                       ? ZR_TRUE
                       : ZR_FALSE;
    }

    if (expr->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                   decoratorNode->location);
        return ZR_FALSE;
    }

    primary = &expr->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->property->data.identifier.name == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators require a named identifier path",
                                   decoratorNode->location);
        return ZR_FALSE;
    }
    rootName = primary->property->data.identifier.name;

    memberCount = primary->members != ZR_NULL ? primary->members->count : 0;
    if (binding != ZR_NULL) {
        binding->constructorCall = ZR_NULL;
    }
    for (TZrSize index = 0; index < memberCount; index++) {
        SZrAstNode *memberNode = primary->members->nodes[index];

        if (memberNode == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                       decoratorNode->location);
            return ZR_FALSE;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            if (index + 1 != memberCount) {
                ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "Compile-time decorators only support a final constructor-style call",
                                           decoratorNode->location);
                return ZR_FALSE;
            }
            if (binding != ZR_NULL) {
                binding->constructorCall = &memberNode->data.functionCall;
            }
            continue;
        }

        if (!ct_try_get_static_decorator_member_name(cs, decoratorNode, memberNode, &leafName)) {
            return ZR_FALSE;
        }
        chainCount++;
    }

    leafName = chainCount > 0 ? leafName : rootName;
    importedModule = ct_find_imported_compile_time_module_alias(cs, rootName);

    if (chainCount == 0) {
        if (binding != ZR_NULL) {
            binding->name = rootName;
            binding->decoratorClass = ct_find_compile_time_decorator_class(cs, rootName);
            binding->decoratorFunction = find_compile_time_function(cs, rootName);
            if (binding->decoratorClass != ZR_NULL && binding->decoratorFunction != ZR_NULL) {
                ct_error_name(cs, rootName, "Ambiguous compile-time decorator name: ", decoratorNode->location);
                return ZR_FALSE;
            }
            return (binding->decoratorClass != ZR_NULL || binding->decoratorFunction != ZR_NULL) ? ZR_TRUE
                                                                                                  : ZR_FALSE;
        }
        return (ct_find_compile_time_decorator_class(cs, rootName) != ZR_NULL ||
                find_compile_time_function(cs, rootName) != ZR_NULL)
                       ? ZR_TRUE
                       : ZR_FALSE;
    }

    if (importedModule != ZR_NULL && chainCount == 1) {
        SZrCompileTimeDecoratorClass *importedClass =
                ct_find_imported_compile_time_decorator_class(importedModule, leafName);
        SZrCompileTimeFunction *importedFunction = ct_find_imported_compile_time_function(importedModule, leafName);

        if (importedClass != ZR_NULL && importedFunction != ZR_NULL) {
            ct_error_name(cs, leafName, "Ambiguous compile-time decorator name: ", decoratorNode->location);
            return ZR_FALSE;
        }

        if (importedClass != ZR_NULL || importedFunction != ZR_NULL) {
            if (binding != ZR_NULL) {
                binding->name = leafName;
                binding->decoratorClass = importedClass;
                binding->decoratorFunction = importedFunction;
            }
            return ZR_TRUE;
        }
    }

    if (importedModule != ZR_NULL && chainCount > 0 && primary->members != ZR_NULL) {
        SZrString *rootMemberName = ZR_NULL;
        SZrString *relativePath = ZR_NULL;
        SZrFunctionCompileTimeVariableInfo *importedVariable;
        const SZrFunctionCompileTimePathBinding *pathBinding;

        if (!ct_try_get_static_decorator_member_name(cs, decoratorNode, primary->members->nodes[0], &rootMemberName)) {
            return ZR_FALSE;
        }

        importedVariable = ct_find_imported_compile_time_variable(importedModule, rootMemberName);
        if (importedVariable != ZR_NULL &&
            ZrParser_CompileTimeBinding_BuildStaticMemberPath(cs->state,
                                                              primary->members,
                                                              1,
                                                              chainCount,
                                                              &relativePath)) {
            pathBinding = ZrParser_CompileTimeBinding_FindPath(importedVariable, relativePath);
            if (pathBinding != ZR_NULL && pathBinding->targetName != ZR_NULL) {
                SZrCompileTimeDecoratorClass *importedClass = ZR_NULL;
                SZrCompileTimeFunction *importedFunction = ZR_NULL;

                if (pathBinding->targetKind == ZR_COMPILE_TIME_BINDING_TARGET_DECORATOR_CLASS) {
                    importedClass =
                            ct_find_imported_compile_time_decorator_class(importedModule, pathBinding->targetName);
                } else if (pathBinding->targetKind == ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION) {
                    importedFunction = ct_find_imported_compile_time_function(importedModule, pathBinding->targetName);
                }

                if (importedClass != ZR_NULL && importedFunction != ZR_NULL) {
                    ct_error_name(cs, pathBinding->targetName, "Ambiguous compile-time decorator name: ", decoratorNode->location);
                    return ZR_FALSE;
                }

                if (importedClass != ZR_NULL || importedFunction != ZR_NULL) {
                    if (binding != ZR_NULL) {
                        binding->name = pathBinding->targetName;
                        binding->decoratorClass = importedClass;
                        binding->decoratorFunction = importedFunction;
                    }
                    return ZR_TRUE;
                }
            }
        }
    }

    if (!ct_try_resolve_decorator_root_value(cs, rootName, &currentValue)) {
        return ZR_FALSE;
    }
    hasValueChain = ZR_TRUE;

    for (TZrSize index = 0; index < memberCount; index++) {
        SZrAstNode *memberNode = primary->members->nodes[index];
        const SZrTypeValue *memberValue;
        SZrTypeValue keyValue;
        SZrString *memberName = ZR_NULL;

        if (memberNode == ZR_NULL || memberNode->type == ZR_AST_FUNCTION_CALL) {
            continue;
        }

        if (!ct_try_get_static_decorator_member_name(cs, decoratorNode, memberNode, &memberName)) {
            return ZR_FALSE;
        }

        if (currentValue.type != ZR_VALUE_TYPE_OBJECT && currentValue.type != ZR_VALUE_TYPE_ARRAY) {
            hasValueChain = ZR_FALSE;
            break;
        }

        ZrCore_Value_InitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        keyValue.type = ZR_VALUE_TYPE_STRING;
        memberValue = ZrCore_Object_GetValue(cs->state, ZR_CAST_OBJECT(cs->state, currentValue.value.object), &keyValue);
        if (memberValue == ZR_NULL) {
            hasValueChain = ZR_FALSE;
            break;
        }

        currentValue = *memberValue;
    }

    if (!hasValueChain) {
        return ZR_FALSE;
    }

    if (binding != ZR_NULL) {
        binding->name = leafName;
        if (ct_try_resolve_compile_time_function_from_value(cs, &currentValue, &binding->decoratorFunction)) {
            return ZR_TRUE;
        }
    } else {
        SZrCompileTimeFunction *resolvedFunction = ZR_NULL;
        if (ct_try_resolve_compile_time_function_from_value(cs, &currentValue, &resolvedFunction)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_IsCompileTimeDecorator(SZrCompilerState *cs,
                                                               SZrAstNode *decoratorNode) {
    SZrResolvedCompileTimeDecoratorBinding binding;

    if (cs == ZR_NULL || decoratorNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
        return ZR_FALSE;
    }

    return (binding.decoratorClass != ZR_NULL || binding.decoratorFunction != ZR_NULL) ? ZR_TRUE : ZR_FALSE;
}

static TZrBool ct_get_meta_method_signature(SZrAstNode *methodNode,
                                            SZrAstNodeArray **outParams,
                                            SZrAstNode **outBody) {
    if (outParams != ZR_NULL) {
        *outParams = ZR_NULL;
    }
    if (outBody != ZR_NULL) {
        *outBody = ZR_NULL;
    }

    if (methodNode == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (methodNode->type) {
        case ZR_AST_CLASS_META_FUNCTION:
            if (outParams != ZR_NULL) {
                *outParams = methodNode->data.classMetaFunction.params;
            }
            if (outBody != ZR_NULL) {
                *outBody = methodNode->data.classMetaFunction.body;
            }
            return ZR_TRUE;
        case ZR_AST_STRUCT_META_FUNCTION:
            if (outParams != ZR_NULL) {
                *outParams = methodNode->data.structMetaFunction.params;
            }
            if (outBody != ZR_NULL) {
                *outBody = methodNode->data.structMetaFunction.body;
            }
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool ct_invoke_compile_time_meta_method(SZrCompilerState *cs,
                                                  SZrAstNode *methodNode,
                                                  SZrFunctionCall *call,
                                                  SZrObject *instanceObject,
                                                  const SZrTypeValue *implicitTarget,
                                                  TZrBool bindOnlyFirstParamFromTarget,
                                                  SZrTypeValue *result,
                                                  TZrBool *didReturn) {
    SZrCompileTimeFrame frame;
    SZrAstNodeArray *params = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    TZrBool success;
    TZrBool localDidReturn = ZR_FALSE;

    if (didReturn != ZR_NULL) {
        *didReturn = ZR_FALSE;
    }
    if (cs == ZR_NULL || methodNode == ZR_NULL || result == ZR_NULL ||
        !ct_get_meta_method_signature(methodNode, &params, &body) || body == ZR_NULL) {
        return ZR_FALSE;
    }

    ct_frame_init(cs, &frame, ZR_NULL);
    if (instanceObject != ZR_NULL) {
        SZrTypeValue thisValue;
        ZrCore_Value_InitAsRawObject(cs->state, &thisValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject));
        thisValue.type = ZR_VALUE_TYPE_OBJECT;
        if (!ct_frame_set(cs, &frame, ZrCore_String_CreateFromNative(cs->state, "this"), &thisValue)) {
            ct_frame_free(cs, &frame);
            return ZR_FALSE;
        }
    }

    if (params != ZR_NULL) {
        for (TZrSize paramIndex = 0; paramIndex < params->count; paramIndex++) {
            SZrAstNode *paramNode = params->nodes[paramIndex];
            SZrParameter *param;
            SZrTypeValue argValue;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            param = &paramNode->data.parameter;
            if (param->name == ZR_NULL || param->name->name == ZR_NULL) {
                continue;
            }

            if (bindOnlyFirstParamFromTarget && paramIndex == 0 && implicitTarget != ZR_NULL) {
                argValue = *implicitTarget;
            } else if (!ct_eval_call_arg(cs,
                                         call,
                                         param,
                                         bindOnlyFirstParamFromTarget ? paramIndex - 1 : paramIndex,
                                         &frame,
                                         &argValue)) {
                ct_frame_free(cs, &frame);
                return ZR_FALSE;
            }

            if (!ct_frame_set(cs, &frame, param->name->name, &argValue)) {
                ct_frame_free(cs, &frame);
                return ZR_FALSE;
            }
        }
    }

    success = body->type == ZR_AST_BLOCK
                      ? execute_compile_time_block(cs, body, &frame, &localDidReturn, result)
                      : execute_compile_time_statement(cs, body, &frame, &localDidReturn, result);
    if (success && !localDidReturn) {
        ZrCore_Value_ResetAsNull(result);
    }

    if (didReturn != ZR_NULL) {
        *didReturn = localDidReturn;
    }
    ct_frame_free(cs, &frame);
    return success;
}

static TZrBool ct_build_type_decorator_snapshot(SZrCompilerState *cs,
                                                SZrTypePrototypeInfo *info,
                                                SZrTypeValue *result) {
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    const TZrChar *kindName = "type";
    const TZrChar *nameText;
    TZrChar qualifiedName[ZR_PARSER_ERROR_BUFFER_LENGTH];
    const TZrChar *moduleNameText = ZR_NULL;

    if (cs == ZR_NULL || info == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    snapshotObject = ct_new_object(cs->state);
    metadataObject = ct_new_object(cs->state);
    decoratorsArray = ct_new_array(cs->state);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    if (info->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        kindName = "class";
    } else if (info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        kindName = "struct";
    }

    nameText = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "type";
    moduleNameText = ct_module_name_text(cs);

    if (moduleNameText != ZR_NULL && moduleNameText[0] != '\0') {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", moduleNameText, nameText != ZR_NULL ? nameText : "type");
    } else {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s", nameText != ZR_NULL ? nameText : "type");
    }

    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", kindName) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", nameText != ZR_NULL ? nameText : "type") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", qualifiedName) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "compileTime")) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_build_function_decorator_snapshot(SZrCompilerState *cs,
                                                    SZrFunction *function,
                                                    SZrTypeValue *result) {
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    const TZrChar *nameText;

    if (cs == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    snapshotObject = ct_new_object(cs->state);
    metadataObject = ct_new_object(cs->state);
    decoratorsArray = ct_new_array(cs->state);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    nameText = function->functionName != ZR_NULL ? ZrCore_String_GetNativeString(function->functionName) : "function";
    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", "function") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", nameText != ZR_NULL ? nameText : "function") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", nameText != ZR_NULL ? nameText : "function") ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "compileTime")) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_build_parameter_decorator_snapshot(SZrCompilerState *cs,
                                                     SZrAstNode *parameterNode,
                                                     TZrUInt32 position,
                                                     const SZrFunctionMetadataParameter *parameterInfo,
                                                     SZrTypeValue *result) {
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    const TZrChar *nameText = "arg";
    const TZrChar *typeNameText = "object";
    TZrChar qualifiedName[ZR_PARSER_ERROR_BUFFER_LENGTH];
    SZrTypeValue positionValue;

    if (cs == ZR_NULL || parameterNode == ZR_NULL || parameterInfo == ZR_NULL || result == ZR_NULL ||
        parameterNode->type != ZR_AST_PARAMETER) {
        return ZR_FALSE;
    }

    if (parameterInfo->name != ZR_NULL) {
        const TZrChar *nativeName = ZrCore_String_GetNativeString(parameterInfo->name);
        if (nativeName != ZR_NULL && nativeName[0] != '\0') {
            nameText = nativeName;
        }
    }

    if (parameterInfo->type.typeName != ZR_NULL) {
        const TZrChar *nativeTypeName = ZrCore_String_GetNativeString(parameterInfo->type.typeName);
        if (nativeTypeName != ZR_NULL && nativeTypeName[0] != '\0') {
            typeNameText = nativeTypeName;
        }
    } else if (parameterInfo->type.isArray) {
        typeNameText = "array";
    } else if (parameterInfo->type.baseType == ZR_VALUE_TYPE_STRING) {
        typeNameText = "string";
    } else if (parameterInfo->type.baseType == ZR_VALUE_TYPE_BOOL) {
        typeNameText = "bool";
    } else if (parameterInfo->type.baseType == ZR_VALUE_TYPE_CLOSURE ||
               parameterInfo->type.baseType == ZR_VALUE_TYPE_FUNCTION) {
        typeNameText = "function";
    } else if (ZR_VALUE_IS_TYPE_INT(parameterInfo->type.baseType)) {
        typeNameText = "int";
    } else if (ZR_VALUE_IS_TYPE_FLOAT(parameterInfo->type.baseType)) {
        typeNameText = "float";
    }

    snapshotObject = ct_new_object(cs->state);
    metadataObject = ct_new_object(cs->state);
    decoratorsArray = ct_new_array(cs->state);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(qualifiedName, sizeof(qualifiedName), "%s[%u]", nameText, (unsigned int)position);
    ZrCore_Value_InitAsInt(cs->state, &positionValue, position);
    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", "parameter") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", nameText) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", qualifiedName) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "typeName", typeNameText) ||
        !ct_set_object_field_value(cs->state, snapshotObject, "position", &positionValue) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "hasDefaultValue", parameterInfo->hasDefaultValue) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "compileTime")) {
        return ZR_FALSE;
    }

    if (parameterInfo->hasDefaultValue &&
        !ct_set_object_field_value(cs->state, snapshotObject, "defaultValue", &parameterInfo->defaultValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_member_logical_name_text(SZrAstNode *memberNode,
                                           const SZrTypeMemberInfo *memberInfo,
                                           const TZrChar **outNameText) {
    const TZrChar *nameText = ZR_NULL;

    if (outNameText != ZR_NULL) {
        *outNameText = ZR_NULL;
    }

    if (memberNode == ZR_NULL || memberInfo == ZR_NULL || outNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (memberNode->type) {
        case ZR_AST_CLASS_FIELD:
            if (memberNode->data.classField.name != ZR_NULL && memberNode->data.classField.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.classField.name->name);
            }
            break;
        case ZR_AST_STRUCT_FIELD:
            if (memberNode->data.structField.name != ZR_NULL && memberNode->data.structField.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.structField.name->name);
            }
            break;
        case ZR_AST_CLASS_METHOD:
            if (memberNode->data.classMethod.name != ZR_NULL && memberNode->data.classMethod.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.classMethod.name->name);
            }
            break;
        case ZR_AST_STRUCT_METHOD:
            if (memberNode->data.structMethod.name != ZR_NULL && memberNode->data.structMethod.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.structMethod.name->name);
            }
            break;
        case ZR_AST_CLASS_PROPERTY:
            if (memberNode->data.classProperty.modifier != ZR_NULL) {
                if (memberNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET &&
                    memberNode->data.classProperty.modifier->data.propertyGet.name != ZR_NULL &&
                    memberNode->data.classProperty.modifier->data.propertyGet.name->name != ZR_NULL) {
                    nameText = ZrCore_String_GetNativeString(
                            memberNode->data.classProperty.modifier->data.propertyGet.name->name);
                } else if (memberNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET &&
                           memberNode->data.classProperty.modifier->data.propertySet.name != ZR_NULL &&
                           memberNode->data.classProperty.modifier->data.propertySet.name->name != ZR_NULL) {
                    nameText = ZrCore_String_GetNativeString(
                            memberNode->data.classProperty.modifier->data.propertySet.name->name);
                }
            }
            break;
        default:
            break;
    }

    if (nameText == ZR_NULL && memberInfo->name != ZR_NULL) {
        nameText = ZrCore_String_GetNativeString(memberInfo->name);
    }

    *outNameText = nameText;
    return nameText != ZR_NULL && nameText[0] != '\0';
}

static const TZrChar *ct_expected_member_decorator_target_name(SZrAstNode *memberNode) {
    if (memberNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (memberNode->type) {
        case ZR_AST_CLASS_FIELD:
        case ZR_AST_STRUCT_FIELD:
            return "Field";
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_METHOD:
            return "Method";
        case ZR_AST_CLASS_PROPERTY:
            return "Property";
        default:
            return ZR_NULL;
    }
}

static const TZrChar *ct_member_snapshot_kind_name(SZrAstNode *memberNode) {
    const TZrChar *targetName = ct_expected_member_decorator_target_name(memberNode);

    if (targetName == ZR_NULL) {
        return ZR_NULL;
    }
    if (strcmp(targetName, "Field") == 0) {
        return "field";
    }
    if (strcmp(targetName, "Method") == 0) {
        return "method";
    }
    if (strcmp(targetName, "Property") == 0) {
        return "property";
    }
    return ZR_NULL;
}

static TZrBool ct_build_member_decorator_snapshot(SZrCompilerState *cs,
                                                  SZrAstNode *memberNode,
                                                  const SZrTypeMemberInfo *memberInfo,
                                                  SZrTypeValue *result) {
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    const TZrChar *kindName;
    const TZrChar *memberNameText;
    const TZrChar *moduleNameText;
    const TZrChar *typeNameText;
    TZrChar qualifiedName[ZR_PARSER_TEXT_BUFFER_LENGTH];
    SZrTypeValue parameterCountValue;

    if (cs == ZR_NULL || memberNode == ZR_NULL || memberInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    kindName = ct_member_snapshot_kind_name(memberNode);
    if (kindName == ZR_NULL || !ct_member_logical_name_text(memberNode, memberInfo, &memberNameText)) {
        return ZR_FALSE;
    }

    snapshotObject = ct_new_object(cs->state);
    metadataObject = ct_new_object(cs->state);
    decoratorsArray = ct_new_array(cs->state);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleNameText = ct_module_name_text(cs);
    typeNameText = cs->currentTypeName != ZR_NULL ? ZrCore_String_GetNativeString(cs->currentTypeName) : ZR_NULL;
    if (moduleNameText != ZR_NULL && moduleNameText[0] != '\0' &&
        typeNameText != ZR_NULL && typeNameText[0] != '\0') {
        snprintf(qualifiedName,
                 sizeof(qualifiedName),
                 "%s.%s.%s",
                 moduleNameText,
                 typeNameText,
                 memberNameText);
    } else if (typeNameText != ZR_NULL && typeNameText[0] != '\0') {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", typeNameText, memberNameText);
    } else {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s", memberNameText);
    }

    ZrCore_Value_InitAsInt(cs->state, &parameterCountValue, memberInfo->parameterCount);
    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", kindName) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", memberNameText) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", qualifiedName) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "compileTime") ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "isStatic", memberInfo->isStatic) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "isConst", memberInfo->isConst) ||
        !ct_set_object_field_value(cs->state, snapshotObject, "parameterCount", &parameterCountValue)) {
        return ZR_FALSE;
    }

    if (memberInfo->fieldTypeName != ZR_NULL &&
        !ct_set_object_field_string(cs->state,
                                    snapshotObject,
                                    "typeName",
                                    ZrCore_String_GetNativeString(memberInfo->fieldTypeName))) {
        return ZR_FALSE;
    }

    if (memberInfo->returnTypeName != ZR_NULL &&
        !ct_set_object_field_string(cs->state,
                                    snapshotObject,
                                    "returnTypeName",
                                    ZrCore_String_GetNativeString(memberInfo->returnTypeName))) {
        return ZR_FALSE;
    } else if ((strcmp(kindName, "method") == 0 || strcmp(kindName, "property") == 0) &&
               !ct_set_object_field_string(cs->state, snapshotObject, "returnTypeName", "void")) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_build_metadata_patch_from_snapshot(SZrCompilerState *cs,
                                                     const SZrTypeValue *targetSnapshot,
                                                     SZrTypeValue *patchResult) {
    SZrTypeValue metadataKey;
    const SZrTypeValue *metadataValue = ZR_NULL;
    SZrObject *patchObject;

    if (cs == ZR_NULL || targetSnapshot == ZR_NULL || patchResult == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(patchResult);
    if (targetSnapshot->type != ZR_VALUE_TYPE_OBJECT || targetSnapshot->value.object == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ct_make_string_value(cs->state, "metadata", &metadataKey)) {
        return ZR_FALSE;
    }

    metadataValue = ZrCore_Object_GetValue(cs->state, ZR_CAST_OBJECT(cs->state, targetSnapshot->value.object), &metadataKey);
    if (metadataValue == ZR_NULL || metadataValue->type == ZR_VALUE_TYPE_NULL) {
        return ZR_TRUE;
    }

    patchObject = ct_new_object(cs->state);
    if (patchObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ct_set_object_field_value(cs->state, patchObject, "metadata", metadataValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, patchResult, ZR_CAST_RAW_OBJECT_AS_SUPER(patchObject));
    patchResult->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_merge_object_fields(SZrState *state, SZrObject *target, SZrObject *source) {
    if (state == ZR_NULL || target == ZR_NULL || source == ZR_NULL ||
        !source->nodeMap.isValid || source->nodeMap.buckets == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < source->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = source->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            ZrCore_Object_SetValue(state, target, &pair->key, &pair->value);
            pair = pair->next;
        }
    }

    return ZR_TRUE;
}

static TZrBool ct_apply_type_decorator_patch(SZrCompilerState *cs,
                                             SZrTypePrototypeInfo *info,
                                             SZrString *decoratorName,
                                             const SZrTypeValue *patchValue,
                                             SZrFileRange location) {
    SZrObject *patchObject;
    const SZrTypeValue *metadataValue = ZR_NULL;
    SZrTypeDecoratorInfo decoratorInfo;

    if (cs == ZR_NULL || info == ZR_NULL || decoratorName == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator must return an object patch",
                                   location);
        return ZR_FALSE;
    }

    patchObject = ZR_CAST_OBJECT(cs->state, patchValue->value.object);
    if (patchObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchObject->nodeMap.isValid && patchObject->nodeMap.buckets != ZR_NULL) {
        for (TZrSize bucketIndex = 0; bucketIndex < patchObject->nodeMap.capacity; bucketIndex++) {
            SZrHashKeyValuePair *pair = patchObject->nodeMap.buckets[bucketIndex];
            while (pair != ZR_NULL) {
                if (pair->key.type != ZR_VALUE_TYPE_STRING ||
                    pair->key.value.object == ZR_NULL ||
                    !ct_string_equals(ZR_CAST_STRING(cs->state, pair->key.value.object), "metadata")) {
                    ZrParser_CompileTime_Error(cs,
                                               ZR_COMPILE_TIME_ERROR_ERROR,
                                               "Compile-time decorator patch currently only supports the 'metadata' field",
                                               location);
                    return ZR_FALSE;
                }
                pair = pair->next;
            }
        }
    }

    {
        SZrTypeValue metadataKey;
        if (ct_make_string_value(cs->state, "metadata", &metadataKey)) {
            metadataValue = ZrCore_Object_GetValue(cs->state, patchObject, &metadataKey);
        }
    }

    if (metadataValue != ZR_NULL && metadataValue->type != ZR_VALUE_TYPE_NULL) {
        if (metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorator patch metadata must be an object",
                                       location);
            return ZR_FALSE;
        }

        if (!ZrParser_Compiler_ValidateRuntimeProjectionValue(cs, metadataValue, location)) {
            return ZR_FALSE;
        }

        if (!info->hasDecoratorMetadata || info->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
            info->decoratorMetadataValue.value.object == ZR_NULL) {
            info->decoratorMetadataValue = *metadataValue;
            info->hasDecoratorMetadata = ZR_TRUE;
        } else if (!ct_merge_object_fields(cs->state,
                                           ZR_CAST_OBJECT(cs->state, info->decoratorMetadataValue.value.object),
                                           ZR_CAST_OBJECT(cs->state, metadataValue->value.object))) {
            return ZR_FALSE;
        }
    }

    decoratorInfo.name = decoratorName;
    ZrCore_Array_Push(cs->state, &info->decorators, &decoratorInfo);
    return ZR_TRUE;
}

static TZrBool ct_apply_function_decorator_patch(SZrCompilerState *cs,
                                                 SZrFunction *function,
                                                 SZrString *decoratorName,
                                                 const SZrTypeValue *patchValue,
                                                 SZrFileRange location) {
    SZrObject *patchObject;
    const SZrTypeValue *metadataValue = ZR_NULL;

    if (cs == ZR_NULL || function == ZR_NULL || decoratorName == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator must return an object patch",
                                   location);
        return ZR_FALSE;
    }

    patchObject = ZR_CAST_OBJECT(cs->state, patchValue->value.object);
    if (patchObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchObject->nodeMap.isValid && patchObject->nodeMap.buckets != ZR_NULL) {
        for (TZrSize bucketIndex = 0; bucketIndex < patchObject->nodeMap.capacity; bucketIndex++) {
            SZrHashKeyValuePair *pair = patchObject->nodeMap.buckets[bucketIndex];
            while (pair != ZR_NULL) {
                if (pair->key.type != ZR_VALUE_TYPE_STRING ||
                    pair->key.value.object == ZR_NULL ||
                    !ct_string_equals(ZR_CAST_STRING(cs->state, pair->key.value.object), "metadata")) {
                    ZrParser_CompileTime_Error(cs,
                                               ZR_COMPILE_TIME_ERROR_ERROR,
                                               "Compile-time decorator patch currently only supports the 'metadata' field",
                                               location);
                    return ZR_FALSE;
                }
                pair = pair->next;
            }
        }
    }

    {
        SZrTypeValue metadataKey;
        if (ct_make_string_value(cs->state, "metadata", &metadataKey)) {
            metadataValue = ZrCore_Object_GetValue(cs->state, patchObject, &metadataKey);
        }
    }

    if (metadataValue != ZR_NULL && metadataValue->type != ZR_VALUE_TYPE_NULL) {
        if (metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorator patch metadata must be an object",
                                       location);
            return ZR_FALSE;
        }

        if (!ZrParser_Compiler_ValidateRuntimeProjectionValue(cs, metadataValue, location)) {
            return ZR_FALSE;
        }

        if (!function->hasDecoratorMetadata || function->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
            function->decoratorMetadataValue.value.object == ZR_NULL) {
            function->decoratorMetadataValue = *metadataValue;
            function->hasDecoratorMetadata = ZR_TRUE;
        } else if (!ct_merge_object_fields(cs->state,
                                           ZR_CAST_OBJECT(cs->state, function->decoratorMetadataValue.value.object),
                                           ZR_CAST_OBJECT(cs->state, metadataValue->value.object))) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool ct_apply_member_decorator_patch(SZrCompilerState *cs,
                                               SZrTypeMemberInfo *memberInfo,
                                               SZrString *decoratorName,
                                               const SZrTypeValue *patchValue,
                                               SZrFileRange location) {
    SZrObject *patchObject;
    const SZrTypeValue *metadataValue = ZR_NULL;
    SZrTypeDecoratorInfo decoratorInfo;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || decoratorName == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator must return an object patch",
                                   location);
        return ZR_FALSE;
    }

    patchObject = ZR_CAST_OBJECT(cs->state, patchValue->value.object);
    if (patchObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchObject->nodeMap.isValid && patchObject->nodeMap.buckets != ZR_NULL) {
        for (TZrSize bucketIndex = 0; bucketIndex < patchObject->nodeMap.capacity; bucketIndex++) {
            SZrHashKeyValuePair *pair = patchObject->nodeMap.buckets[bucketIndex];
            while (pair != ZR_NULL) {
                if (pair->key.type != ZR_VALUE_TYPE_STRING ||
                    pair->key.value.object == ZR_NULL ||
                    !ct_string_equals(ZR_CAST_STRING(cs->state, pair->key.value.object), "metadata")) {
                    ZrParser_CompileTime_Error(cs,
                                               ZR_COMPILE_TIME_ERROR_ERROR,
                                               "Compile-time decorator patch currently only supports the 'metadata' field",
                                               location);
                    return ZR_FALSE;
                }
                pair = pair->next;
            }
        }
    }

    {
        SZrTypeValue metadataKey;
        if (ct_make_string_value(cs->state, "metadata", &metadataKey)) {
            metadataValue = ZrCore_Object_GetValue(cs->state, patchObject, &metadataKey);
        }
    }

    if (metadataValue != ZR_NULL && metadataValue->type != ZR_VALUE_TYPE_NULL) {
        if (metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorator patch metadata must be an object",
                                       location);
            return ZR_FALSE;
        }

        if (!ZrParser_Compiler_ValidateRuntimeProjectionValue(cs, metadataValue, location)) {
            return ZR_FALSE;
        }

        if (!memberInfo->hasDecoratorMetadata ||
            memberInfo->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
            memberInfo->decoratorMetadataValue.value.object == ZR_NULL) {
            memberInfo->decoratorMetadataValue = *metadataValue;
            memberInfo->hasDecoratorMetadata = ZR_TRUE;
        } else if (!ct_merge_object_fields(cs->state,
                                           ZR_CAST_OBJECT(cs->state, memberInfo->decoratorMetadataValue.value.object),
                                           ZR_CAST_OBJECT(cs->state, metadataValue->value.object))) {
            return ZR_FALSE;
        }
    }

    if (!memberInfo->decorators.isValid || memberInfo->decorators.head == ZR_NULL ||
        memberInfo->decorators.capacity == 0 || memberInfo->decorators.elementSize == 0) {
        ZrCore_Array_Init(cs->state,
                          &memberInfo->decorators,
                          sizeof(SZrTypeDecoratorInfo),
                          ZR_PARSER_INITIAL_CAPACITY_TINY);
    }

    decoratorInfo.name = decoratorName;
    ZrCore_Array_Push(cs->state, &memberInfo->decorators, &decoratorInfo);
    return ZR_TRUE;
}

static TZrBool ct_apply_parameter_decorator_patch(SZrCompilerState *cs,
                                                  SZrFunctionMetadataParameter *parameterInfo,
                                                  const SZrTypeValue *patchValue,
                                                  SZrFileRange location) {
    SZrObject *patchObject;
    const SZrTypeValue *metadataValue = ZR_NULL;

    if (cs == ZR_NULL || parameterInfo == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator must return an object patch",
                                   location);
        return ZR_FALSE;
    }

    patchObject = ZR_CAST_OBJECT(cs->state, patchValue->value.object);
    if (patchObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchObject->nodeMap.isValid && patchObject->nodeMap.buckets != ZR_NULL) {
        for (TZrSize bucketIndex = 0; bucketIndex < patchObject->nodeMap.capacity; bucketIndex++) {
            SZrHashKeyValuePair *pair = patchObject->nodeMap.buckets[bucketIndex];
            while (pair != ZR_NULL) {
                if (pair->key.type != ZR_VALUE_TYPE_STRING ||
                    pair->key.value.object == ZR_NULL ||
                    !ct_string_equals(ZR_CAST_STRING(cs->state, pair->key.value.object), "metadata")) {
                    ZrParser_CompileTime_Error(cs,
                                               ZR_COMPILE_TIME_ERROR_ERROR,
                                               "Compile-time decorator patch currently only supports the 'metadata' field",
                                               location);
                    return ZR_FALSE;
                }
                pair = pair->next;
            }
        }
    }

    {
        SZrTypeValue metadataKey;
        if (ct_make_string_value(cs->state, "metadata", &metadataKey)) {
            metadataValue = ZrCore_Object_GetValue(cs->state, patchObject, &metadataKey);
        }
    }

    if (metadataValue != ZR_NULL && metadataValue->type != ZR_VALUE_TYPE_NULL) {
        if (metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorator patch metadata must be an object",
                                       location);
            return ZR_FALSE;
        }

        if (!ZrParser_Compiler_ValidateRuntimeProjectionValue(cs, metadataValue, location)) {
            return ZR_FALSE;
        }

        if (!parameterInfo->hasDecoratorMetadata ||
            parameterInfo->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
            parameterInfo->decoratorMetadataValue.value.object == ZR_NULL) {
            parameterInfo->decoratorMetadataValue = *metadataValue;
            parameterInfo->hasDecoratorMetadata = ZR_TRUE;
        } else if (!ct_merge_object_fields(cs->state,
                                           ZR_CAST_OBJECT(cs->state, parameterInfo->decoratorMetadataValue.value.object),
                                           ZR_CAST_OBJECT(cs->state, metadataValue->value.object))) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool ct_execute_compile_time_decorator_class(SZrCompilerState *cs,
                                                       SZrCompileTimeDecoratorClass *decoratorClass,
                                                       SZrFunctionCall *constructorCall,
                                                       const SZrTypeValue *targetSnapshot,
                                                       const TZrChar *expectedTargetName,
                                                       SZrTypeValue *patchResult,
                                                       SZrFileRange location) {
    SZrObject *instanceObject;
    SZrTypeValue ignoredResult;
    TZrBool didReturn = ZR_FALSE;

    if (cs == ZR_NULL || decoratorClass == ZR_NULL || targetSnapshot == ZR_NULL || patchResult == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decoratorClass->decorateMethod == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator class must declare @decorate",
                                   location);
        return ZR_FALSE;
    }

    if (!ct_validate_decorator_meta_method_target(cs, decoratorClass->decorateMethod, expectedTargetName, location)) {
        return ZR_FALSE;
    }

    instanceObject = ct_new_object(cs->state);
    if (instanceObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decoratorClass->constructorMethod != ZR_NULL) {
        if (!ct_invoke_compile_time_meta_method(cs,
                                                decoratorClass->constructorMethod,
                                                constructorCall,
                                                instanceObject,
                                                ZR_NULL,
                                                ZR_FALSE,
                                                &ignoredResult,
                                                &didReturn)) {
            return ZR_FALSE;
        }
    } else if (constructorCall != ZR_NULL && constructorCall->args != ZR_NULL && constructorCall->args->count > 0) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator constructor arguments require an @constructor meta method",
                                   location);
        return ZR_FALSE;
    }

    if (!ct_invoke_compile_time_meta_method(cs,
                                            decoratorClass->decorateMethod,
                                            ZR_NULL,
                                            instanceObject,
                                            targetSnapshot,
                                            ZR_TRUE,
                                            patchResult,
                                            &didReturn)) {
        return ZR_FALSE;
    }

    if (!didReturn) {
        return ct_build_metadata_patch_from_snapshot(cs, targetSnapshot, patchResult);
    }

    return ZR_TRUE;
}

static TZrBool ct_execute_compile_time_decorator_function(SZrCompilerState *cs,
                                                          SZrCompileTimeFunction *decoratorFunction,
                                                          SZrFunctionCall *constructorCall,
                                                          const SZrTypeValue *targetSnapshot,
                                                          const TZrChar *expectedTargetName,
                                                          SZrTypeValue *patchResult,
                                                          SZrFileRange location) {
    SZrFunctionDeclaration *decl;
    SZrCompileTimeFrame frame;
    TZrBool success = ZR_FALSE;
    TZrBool didReturn = ZR_FALSE;
    TZrSize expectedArgumentCount = 0;
    SZrTypeValue callableValue;
    SZrTypeValue *argValues = ZR_NULL;

    if (cs == ZR_NULL || decoratorFunction == ZR_NULL || targetSnapshot == ZR_NULL || patchResult == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decoratorFunction->isRuntimeProjection) {
        ZR_UNUSED_PARAMETER(expectedTargetName);

        if (decoratorFunction->paramNames.length == 0) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorator function must declare a target parameter",
                                       location);
            return ZR_FALSE;
        }

        expectedArgumentCount = decoratorFunction->paramNames.length > 0 ? decoratorFunction->paramNames.length - 1 : 0;
        if (constructorCall != ZR_NULL && constructorCall->args != ZR_NULL &&
            constructorCall->args->count > expectedArgumentCount) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Too many arguments for compile-time decorator function",
                                       location);
            return ZR_FALSE;
        }

        if (!ct_value_from_compile_time_function(cs, decoratorFunction, &callableValue)) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Failed to resolve runtime projection for compile-time decorator function",
                                       location);
            return ZR_FALSE;
        }

        argValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                    sizeof(SZrTypeValue) * decoratorFunction->paramNames.length,
                                                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (argValues == ZR_NULL) {
            return ZR_FALSE;
        }

        argValues[0] = *targetSnapshot;
        for (TZrSize paramIndex = 1; paramIndex < decoratorFunction->paramNames.length; paramIndex++) {
            SZrString **paramNamePtr =
                    (SZrString **)ZrCore_Array_Get(&decoratorFunction->paramNames, paramIndex);
            if (!ct_eval_runtime_projected_call_arg(cs,
                                                    decoratorFunction,
                                                    constructorCall,
                                                    paramNamePtr != ZR_NULL ? *paramNamePtr : ZR_NULL,
                                                    paramIndex - 1,
                                                    ZR_NULL,
                                                    &argValues[paramIndex])) {
                ZrCore_Memory_RawFreeWithType(cs->state->global,
                                              argValues,
                                              sizeof(SZrTypeValue) * decoratorFunction->paramNames.length,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_FALSE;
            }
        }

        success = ct_invoke_runtime_callable_with_values(cs,
                                                         ZR_NULL,
                                                         &callableValue,
                                                         decoratorFunction->paramNames.length,
                                                         argValues,
                                                         patchResult);
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argValues,
                                      sizeof(SZrTypeValue) * decoratorFunction->paramNames.length,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return success;
    }

    if (decoratorFunction->declaration == ZR_NULL ||
        decoratorFunction->declaration->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &decoratorFunction->declaration->data.functionDeclaration;
    if (decl->params == ZR_NULL || decl->params->count == 0 || decl->params->nodes[0] == ZR_NULL ||
        decl->params->nodes[0]->type != ZR_AST_PARAMETER) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator function must declare a target parameter",
                                   location);
        return ZR_FALSE;
    }

    if (!ct_validate_named_decorator_target_param(cs,
                                                  &decl->params->nodes[0]->data.parameter,
                                                  expectedTargetName,
                                                  "Compile-time decorator function target must use %type Class, %type Struct, %type Function, %type Field, %type Method, %type Property, or %type Object",
                                                  location)) {
        return ZR_FALSE;
    }

    expectedArgumentCount = decl->params->count > 0 ? decl->params->count - 1 : 0;
    if (constructorCall != ZR_NULL && constructorCall->args != ZR_NULL && decl->args == ZR_NULL &&
        constructorCall->args->count > expectedArgumentCount) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Too many arguments for compile-time decorator function",
                                   location);
        return ZR_FALSE;
    }

    ct_frame_init(cs, &frame, ZR_NULL);
    if (!ct_frame_set(cs,
                      &frame,
                      decl->params->nodes[0]->data.parameter.name != ZR_NULL
                              ? decl->params->nodes[0]->data.parameter.name->name
                              : ZR_NULL,
                      targetSnapshot)) {
        ct_frame_free(cs, &frame);
        return ZR_FALSE;
    }

    for (TZrSize paramIndex = 1; paramIndex < decl->params->count; paramIndex++) {
        SZrAstNode *paramNode = decl->params->nodes[paramIndex];
        SZrParameter *param;
        SZrTypeValue argValue;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        param = &paramNode->data.parameter;
        if (param->name == ZR_NULL || param->name->name == ZR_NULL ||
            !ct_eval_call_arg(cs, constructorCall, param, paramIndex - 1, &frame, &argValue) ||
            !ct_frame_set(cs, &frame, param->name->name, &argValue)) {
            ct_frame_free(cs, &frame);
            return ZR_FALSE;
        }
    }

    if (decl->body == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator function body is null",
                                   location);
        ct_frame_free(cs, &frame);
        return ZR_FALSE;
    }

    success = decl->body->type == ZR_AST_BLOCK
                      ? execute_compile_time_block(cs, decl->body, &frame, &didReturn, patchResult)
                      : execute_compile_time_statement(cs, decl->body, &frame, &didReturn, patchResult);
    if (success && !didReturn) {
        success = ct_build_metadata_patch_from_snapshot(cs, targetSnapshot, patchResult);
    }

    ct_frame_free(cs, &frame);
    return success;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_ApplyCompileTimeTypeDecorators(SZrCompilerState *cs,
                                                                       SZrAstNode *typeNode,
                                                                       SZrAstNodeArray *decorators,
                                                                       SZrTypePrototypeInfo *info) {
    SZrTypeValue targetSnapshot;
    const TZrChar *expectedTargetName;
    TZrSize compileTimeDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || info == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);

    if (!ct_build_type_decorator_snapshot(cs, info, &targetSnapshot)) {
        goto cleanup;
    }

    expectedTargetName = ct_expected_type_decorator_target_name(info->type);

    for (TZrSize index = 0, compileIndex = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[compileIndex] = binding.name;
        compileTimeDecoratorNodes[compileIndex] = decoratorNode;
        if (binding.decoratorClass != ZR_NULL &&
            !ct_execute_compile_time_decorator_class(cs,
                                                     binding.decoratorClass,
                                                     binding.constructorCall,
                                                     &targetSnapshot,
                                                     expectedTargetName,
                                                     &patchValues[compileIndex],
                                                     decoratorNode->location)) {
            goto cleanup;
        }
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        expectedTargetName,
                                                        &patchValues[compileIndex],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        compileIndex++;
    }

    for (TZrSize index = compileTimeDecoratorCount; index > 0; index--) {
        if (!ct_apply_type_decorator_patch(cs,
                                           info,
                                           decoratorNames[index - 1],
                                           &patchValues[index - 1],
                                           compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                   ? compileTimeDecoratorNodes[index - 1]->location
                                                   : typeNode->location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrBool ZrParser_CompileTime_RegisterDecoratorTypeIfAvailable(SZrCompilerState *cs,
                                                              SZrAstNode *node,
                                                              SZrFileRange location) {
    SZrAstNodeArray *members = ZR_NULL;
    TZrBool isStructDecorator = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!cs->isInCompileTimeContext) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_CLASS_DECLARATION) {
        members = node->data.classDeclaration.members;
    } else if (node->type == ZR_AST_STRUCT_DECLARATION) {
        members = node->data.structDeclaration.members;
        isStructDecorator = ZR_TRUE;
    } else {
        return ZR_TRUE;
    }

    if (ct_find_compile_time_meta_method(members, "decorate", isStructDecorator) == ZR_NULL) {
        return ZR_TRUE;
    }

    return ct_register_compile_time_decorator_class(cs, node, location);
}

TZrBool ZrParser_CompileTime_RegisterDecoratorFunctionIfAvailable(SZrCompilerState *cs,
                                                                  SZrAstNode *node,
                                                                  SZrFileRange location) {
    SZrFunctionDeclaration *decl;
    SZrString *targetTypeName;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    if (!cs->isInCompileTimeContext) {
        return ZR_TRUE;
    }

    decl = &node->data.functionDeclaration;
    if (decl->params == ZR_NULL || decl->params->count == 0 || decl->params->nodes[0] == ZR_NULL ||
        decl->params->nodes[0]->type != ZR_AST_PARAMETER) {
        return ZR_TRUE;
    }

    targetTypeName = ct_decorator_target_type_name(decl->params->nodes[0]->data.parameter.typeInfo);
    if (targetTypeName == ZR_NULL ||
        (!ct_string_equals(targetTypeName, "Class") &&
         !ct_string_equals(targetTypeName, "Struct") &&
         !ct_string_equals(targetTypeName, "Function") &&
         !ct_string_equals(targetTypeName, "Field") &&
         !ct_string_equals(targetTypeName, "Method") &&
         !ct_string_equals(targetTypeName, "Property") &&
         !ct_string_equals(targetTypeName, "Parameter") &&
         !ct_string_equals(targetTypeName, "Object"))) {
        return ZR_TRUE;
    }

    return register_compile_time_function_declaration(cs, node, location);
}

TZrBool ZrParser_CompileTime_ApplyFunctionDecorators(SZrCompilerState *cs,
                                                     SZrAstNodeArray *decorators,
                                                     SZrFunction *function,
                                                     SZrFileRange location) {
    SZrTypeValue targetSnapshot;
    TZrSize compileTimeDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);

    if (!ct_build_function_decorator_snapshot(cs, function, &targetSnapshot)) {
        goto cleanup;
    }

    for (TZrSize index = 0, compileIndex = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[compileIndex] = binding.name;
        compileTimeDecoratorNodes[compileIndex] = decoratorNode;
        if (binding.decoratorClass != ZR_NULL &&
            !ct_execute_compile_time_decorator_class(cs,
                                                     binding.decoratorClass,
                                                     binding.constructorCall,
                                                     &targetSnapshot,
                                                     "Function",
                                                     &patchValues[compileIndex],
                                                     decoratorNode->location)) {
            goto cleanup;
        }
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        "Function",
                                                        &patchValues[compileIndex],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        compileIndex++;
    }

    if (compileTimeDecoratorCount > 0) {
        function->decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                  sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->decoratorNames == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawSet(function->decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
        function->decoratorCount = (TZrUInt32)compileTimeDecoratorCount;
        for (TZrSize index = 0; index < compileTimeDecoratorCount; index++) {
            function->decoratorNames[index] = decoratorNames[index];
        }
    }

    for (TZrSize index = compileTimeDecoratorCount; index > 0; index--) {
        if (!ct_apply_function_decorator_patch(cs,
                                               function,
                                               decoratorNames[index - 1],
                                               &patchValues[index - 1],
                                               compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                       ? compileTimeDecoratorNodes[index - 1]->location
                                                       : location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (!success && function != ZR_NULL && function->decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      function->decoratorNames,
                                      sizeof(SZrString *) * function->decoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        function->decoratorNames = ZR_NULL;
        function->decoratorCount = 0;
    }
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrBool ZrParser_CompileTime_ApplyMemberDecorators(SZrCompilerState *cs,
                                                   SZrAstNode *memberNode,
                                                   SZrAstNodeArray *decorators,
                                                   SZrTypeMemberInfo *memberInfo) {
    SZrTypeValue targetSnapshot;
    const TZrChar *expectedTargetName;
    TZrSize compileTimeDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || memberNode == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    expectedTargetName = ct_expected_member_decorator_target_name(memberNode);
    if (expectedTargetName == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);

    if (!ct_build_member_decorator_snapshot(cs, memberNode, memberInfo, &targetSnapshot)) {
        goto cleanup;
    }

    for (TZrSize index = 0, compileIndex = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[compileIndex] = binding.name;
        compileTimeDecoratorNodes[compileIndex] = decoratorNode;
        if (binding.decoratorClass != ZR_NULL &&
            !ct_execute_compile_time_decorator_class(cs,
                                                     binding.decoratorClass,
                                                     binding.constructorCall,
                                                     &targetSnapshot,
                                                     expectedTargetName,
                                                     &patchValues[compileIndex],
                                                     decoratorNode->location)) {
            goto cleanup;
        }
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        expectedTargetName,
                                                        &patchValues[compileIndex],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        compileIndex++;
    }

    for (TZrSize index = compileTimeDecoratorCount; index > 0; index--) {
        if (!ct_apply_member_decorator_patch(cs,
                                             memberInfo,
                                             decoratorNames[index - 1],
                                             &patchValues[index - 1],
                                             compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                     ? compileTimeDecoratorNodes[index - 1]->location
                                                     : memberNode->location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrBool ZrParser_CompileTime_ApplyParameterDecorators(SZrCompilerState *cs,
                                                      SZrAstNode *parameterNode,
                                                      TZrUInt32 position,
                                                      SZrFunctionMetadataParameter *parameterInfo) {
    SZrParameter *parameter;
    SZrAstNodeArray *decorators;
    SZrTypeValue targetSnapshot;
    TZrSize compileTimeDecoratorCount = 0;
    TZrSize appliedDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || parameterNode == ZR_NULL || parameterInfo == ZR_NULL ||
        parameterNode->type != ZR_AST_PARAMETER) {
        return ZR_FALSE;
    }

    parameter = &parameterNode->data.parameter;
    decorators = parameter->decorators;
    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);

    if (!ct_build_parameter_decorator_snapshot(cs, parameterNode, position, parameterInfo, &targetSnapshot)) {
        goto cleanup;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[appliedDecoratorCount] = binding.name;
        compileTimeDecoratorNodes[appliedDecoratorCount] = decoratorNode;
        if (binding.decoratorClass != ZR_NULL &&
            !ct_execute_compile_time_decorator_class(cs,
                                                     binding.decoratorClass,
                                                     binding.constructorCall,
                                                     &targetSnapshot,
                                                     "Parameter",
                                                     &patchValues[appliedDecoratorCount],
                                                     decoratorNode->location)) {
            goto cleanup;
        }
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        "Parameter",
                                                        &patchValues[appliedDecoratorCount],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        appliedDecoratorCount++;
    }

    if (appliedDecoratorCount > 0) {
        parameterInfo->decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrString *) * appliedDecoratorCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (parameterInfo->decoratorNames == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawSet(parameterInfo->decoratorNames, 0, sizeof(SZrString *) * appliedDecoratorCount);
        parameterInfo->decoratorCount = (TZrUInt32)appliedDecoratorCount;
        for (TZrSize index = 0; index < appliedDecoratorCount; index++) {
            parameterInfo->decoratorNames[index] = decoratorNames[index];
        }
    }

    for (TZrSize index = appliedDecoratorCount; index > 0; index--) {
        if (!ct_apply_parameter_decorator_patch(cs,
                                                parameterInfo,
                                                &patchValues[index - 1],
                                                compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                        ? compileTimeDecoratorNodes[index - 1]->location
                                                        : parameterNode->location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (!success && parameterInfo->decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      parameterInfo->decoratorNames,
                                      sizeof(SZrString *) * parameterInfo->decoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        parameterInfo->decoratorNames = ZR_NULL;
        parameterInfo->decoratorCount = 0;
    }
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

static TZrBool ct_eval_binary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrBinaryExpression *expr = &node->data.binaryExpression;
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    const TZrChar *op = expr->op.op;

    if (!evaluate_compile_time_expression_internal(cs, expr->left, frame, &leftValue) ||
        !evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue) ||
        op == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (!ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) || !ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time arithmetic requires numeric operands", node->location);
            return ZR_FALSE;
        }
        if (strcmp(op, "/") == 0) {
            TZrBool isZero = ZR_VALUE_IS_TYPE_INT(rightValue.type)
                               ? (rightValue.value.nativeObject.nativeInt64 == 0)
                               : (rightValue.value.nativeObject.nativeDouble == 0.0);
            if (isZero) {
                ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Division by zero in compile-time expression", node->location);
                return ZR_FALSE;
            }
        }
        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
            TZrInt64 left = leftValue.value.nativeObject.nativeInt64;
            TZrInt64 right = rightValue.value.nativeObject.nativeInt64;
            if (strcmp(op, "+") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left + right);
            } else if (strcmp(op, "-") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left - right);
            } else if (strcmp(op, "*") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left * right);
            } else {
                ZrCore_Value_InitAsInt(cs->state, result, left / right);
            }
            return ZR_TRUE;
        }

        TZrDouble left = leftValue.value.nativeObject.nativeDouble;
        TZrDouble right = rightValue.value.nativeObject.nativeDouble;
        if (strcmp(op, "+") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left + right);
        } else if (strcmp(op, "-") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left - right);
        } else if (strcmp(op, "*") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left * right);
        } else {
            ZrCore_Value_InitAsFloat(cs->state, result, left / right);
        }
        return ZR_TRUE;
    }

    if (strcmp(op, "%") == 0) {
        if (!ZR_VALUE_IS_TYPE_INT(leftValue.type) || !ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time modulo requires integer operands", node->location);
            return ZR_FALSE;
        }
        if (rightValue.value.nativeObject.nativeInt64 == 0) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Modulo by zero in compile-time expression", node->location);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsInt(cs->state, result, leftValue.value.nativeObject.nativeInt64 % rightValue.value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
        strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
        TZrBool value = ZR_FALSE;
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
            if (leftValue.type == rightValue.type) {
                if (ZR_VALUE_IS_TYPE_INT(leftValue.type)) {
                    value = leftValue.value.nativeObject.nativeInt64 == rightValue.value.nativeObject.nativeInt64;
                } else if (ZR_VALUE_IS_TYPE_FLOAT(leftValue.type)) {
                    value = leftValue.value.nativeObject.nativeDouble == rightValue.value.nativeObject.nativeDouble;
                } else if (leftValue.type == ZR_VALUE_TYPE_BOOL) {
                    value = leftValue.value.nativeObject.nativeBool == rightValue.value.nativeObject.nativeBool;
                } else if (leftValue.type == ZR_VALUE_TYPE_NULL) {
                    value = ZR_TRUE;
                }
            }
            if (strcmp(op, "!=") == 0) {
                value = !value;
            }
        } else if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
            if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                TZrInt64 left = leftValue.value.nativeObject.nativeInt64;
                TZrInt64 right = rightValue.value.nativeObject.nativeInt64;
                value = strcmp(op, "<") == 0 ? left < right :
                        strcmp(op, "<=") == 0 ? left <= right :
                        strcmp(op, ">") == 0 ? left > right : left >= right;
            } else {
                TZrDouble left = leftValue.value.nativeObject.nativeDouble;
                TZrDouble right = rightValue.value.nativeObject.nativeDouble;
                value = strcmp(op, "<") == 0 ? left < right :
                        strcmp(op, "<=") == 0 ? left <= right :
                        strcmp(op, ">") == 0 ? left > right : left >= right;
            }
        } else {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time comparison requires compatible operands", node->location);
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsUInt(cs->state, result, value ? 1 : 0);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time binary expression", node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_logical(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrLogicalExpression *expr = &node->data.logicalExpression;
    SZrTypeValue leftValue;
    TZrBool value;

    if (!evaluate_compile_time_expression_internal(cs, expr->left, frame, &leftValue)) {
        return ZR_FALSE;
    }

    if (strcmp(expr->op, "&&") == 0) {
        value = ct_truthy(&leftValue);
        if (value) {
            SZrTypeValue rightValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue)) {
                return ZR_FALSE;
            }
            value = ct_truthy(&rightValue);
        }
    } else if (strcmp(expr->op, "||") == 0) {
        value = ct_truthy(&leftValue);
        if (!value) {
            SZrTypeValue rightValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue)) {
                return ZR_FALSE;
            }
            value = ct_truthy(&rightValue);
        }
    } else {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time logical expression", node->location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(cs->state, result, value ? 1 : 0);
    result->type = ZR_VALUE_TYPE_BOOL;
    return ZR_TRUE;
}

static TZrBool ct_eval_unary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrUnaryExpression *expr = &node->data.unaryExpression;
    SZrTypeValue argValue;

    if (!evaluate_compile_time_expression_internal(cs, expr->argument, frame, &argValue)) {
        return ZR_FALSE;
    }

    if (strcmp(expr->op.op, "!") == 0) {
        ZrCore_Value_InitAsUInt(cs->state, result, ct_truthy(&argValue) ? 0 : 1);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }
    if (strcmp(expr->op.op, "+") == 0) {
        *result = argValue;
        return ZR_TRUE;
    }
    if (strcmp(expr->op.op, "-") == 0) {
        if (ZR_VALUE_IS_TYPE_INT(argValue.type)) {
            ZrCore_Value_InitAsInt(cs->state, result, -argValue.value.nativeObject.nativeInt64);
            return ZR_TRUE;
        }
        if (ZR_VALUE_IS_TYPE_FLOAT(argValue.type)) {
            ZrCore_Value_InitAsFloat(cs->state, result, -argValue.value.nativeObject.nativeDouble);
            return ZR_TRUE;
        }
    }

    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time unary expression", node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_type_cast(SZrCompilerState *cs,
                                 SZrAstNode *node,
                                 SZrCompileTimeFrame *frame,
                                 SZrTypeValue *result) {
    SZrTypeCastExpression *expr;
    SZrTypeValue sourceValue;
    const TZrChar *targetName;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_TYPE_CAST_EXPRESSION) {
        return ZR_FALSE;
    }

    expr = &node->data.typeCastExpression;
    if (expr->targetType == ZR_NULL || expr->expression == ZR_NULL ||
        expr->targetType->name == ZR_NULL ||
        expr->targetType->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        expr->targetType->name->data.identifier.name == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Unsupported compile-time type cast target",
                                   node->location);
        return ZR_FALSE;
    }

    if (!evaluate_compile_time_expression_internal(cs, expr->expression, frame, &sourceValue)) {
        return ZR_FALSE;
    }

    targetName = ct_name(expr->targetType->name->data.identifier.name);
    if (strcmp(targetName, "int") == 0) {
        if (ZR_VALUE_IS_TYPE_INT(sourceValue.type)) {
            ZrCore_Value_Copy(cs->state, result, &sourceValue);
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue.type)) {
            ZrCore_Value_InitAsInt(cs->state, result, (TZrInt64)sourceValue.value.nativeObject.nativeUInt64);
        } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue.type)) {
            ZrCore_Value_InitAsInt(cs->state, result, (TZrInt64)sourceValue.value.nativeObject.nativeDouble);
        } else if (sourceValue.type == ZR_VALUE_TYPE_BOOL) {
            ZrCore_Value_InitAsInt(cs->state, result, sourceValue.value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
        } else {
            ZrCore_Value_InitAsInt(cs->state, result, 0);
        }
        return ZR_TRUE;
    }

    if (strcmp(targetName, "float") == 0) {
        if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue.type)) {
            ZrCore_Value_Copy(cs->state, result, &sourceValue);
        } else if (ZR_VALUE_IS_TYPE_INT(sourceValue.type)) {
            ZrCore_Value_InitAsFloat(cs->state, result, (TZrFloat64)sourceValue.value.nativeObject.nativeInt64);
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue.type)) {
            ZrCore_Value_InitAsFloat(cs->state, result, (TZrFloat64)sourceValue.value.nativeObject.nativeUInt64);
        } else if (sourceValue.type == ZR_VALUE_TYPE_BOOL) {
            ZrCore_Value_InitAsFloat(cs->state,
                                     result,
                                     sourceValue.value.nativeObject.nativeBool ? (TZrFloat64)ZR_TRUE : (TZrFloat64)ZR_FALSE);
        } else {
            ZrCore_Value_InitAsFloat(cs->state, result, 0.0);
        }
        return ZR_TRUE;
    }

    if (strcmp(targetName, "bool") == 0) {
        ZrCore_Value_InitAsUInt(cs->state, result, ct_truthy(&sourceValue) ? 1 : 0);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }

    if (strcmp(targetName, "string") == 0) {
        SZrString *stringValue = ZrCore_Value_ConvertToString(cs->state, &sourceValue);
        if (stringValue == ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        } else {
            ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
            result->type = ZR_VALUE_TYPE_STRING;
        }
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Unsupported compile-time type cast target",
                               node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_builtin_call(SZrCompilerState *cs,
                                  SZrAstNode *node,
                                  SZrString *funcName,
                                  SZrFunctionCall *call,
                                  SZrCompileTimeFrame *frame,
                                  SZrTypeValue *result) {
    const TZrChar *nameStr = ct_name(funcName);

    if (strcmp(nameStr, "FatalError") == 0) {
        const TZrChar *msg = "FatalError";
        if (call != ZR_NULL && call->args != ZR_NULL && call->args->count > 0) {
            SZrTypeValue msgValue;
            if (evaluate_compile_time_expression_internal(cs, call->args->nodes[0], frame, &msgValue) &&
                msgValue.type == ZR_VALUE_TYPE_STRING) {
                msg = ct_name((SZrString *)ZrCore_Value_GetRawObject(&msgValue));
            }
        }
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_FATAL, msg, node->location);
        return ZR_FALSE;
    }

    if (strcmp(nameStr, "Assert") == 0) {
        if (call == ZR_NULL || call->args == ZR_NULL || call->args->count == 0) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Assert requires at least one argument", node->location);
            return ZR_FALSE;
        }

        SZrTypeValue condValue;
        if (!evaluate_compile_time_expression_internal(cs, call->args->nodes[0], frame, &condValue)) {
            return ZR_FALSE;
        }
        if (!ct_truthy(&condValue)) {
            const TZrChar *msg = "Assertion failed";
            if (call->args->count > 1) {
                SZrTypeValue msgValue;
                if (evaluate_compile_time_expression_internal(cs, call->args->nodes[1], frame, &msgValue) &&
                    msgValue.type == ZR_VALUE_TYPE_STRING) {
                    msg = ct_name((SZrString *)ZrCore_Value_GetRawObject(&msgValue));
                }
            }
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_FATAL, msg, node->location);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsUInt(cs->state, result, 1);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool ct_eval_call_arg(SZrCompilerState *cs,
                              SZrFunctionCall *call,
                              SZrParameter *param,
                              TZrSize paramIndex,
                              SZrCompileTimeFrame *frame,
                              SZrTypeValue *result) {
    TZrSize positionalCount = 0;

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL &&
        param != ZR_NULL && param->name != ZR_NULL && param->name->name != ZR_NULL) {
        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                ZrCore_String_Equal(*argNamePtr, param->name->name)) {
                return evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, result);
            }
        }

        if (paramIndex < positionalCount) {
            return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
    }

    if (param != ZR_NULL && param->defaultValue != ZR_NULL) {
        return evaluate_compile_time_expression_internal(cs, param->defaultValue, frame, result);
    }

    ct_error_name(cs,
                  param != ZR_NULL && param->name != ZR_NULL ? param->name->name : ZR_NULL,
                  "Missing compile-time argument for parameter: ",
                  (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_eval_runtime_projected_call_arg(SZrCompilerState *cs,
                                                  SZrCompileTimeFunction *func,
                                                  SZrFunctionCall *call,
                                                  SZrString *paramName,
                                                  TZrSize paramIndex,
                                                  SZrCompileTimeFrame *frame,
                                                  SZrTypeValue *result) {
    TZrSize positionalCount = 0;

    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        for (TZrSize i = 0; i < call->argNames->length && call->args != ZR_NULL && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (paramName != ZR_NULL) {
            for (TZrSize i = 0; i < call->argNames->length && call->args != ZR_NULL && i < call->args->count; i++) {
                SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
                if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                    ZrCore_String_Equal(*argNamePtr, paramName)) {
                    return evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, result);
                }
            }
        }

        if (call->args != ZR_NULL && paramIndex < positionalCount) {
            return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
    }

    if (func != ZR_NULL &&
        paramIndex < func->paramHasDefaultValues.length &&
        paramIndex < func->paramDefaultValues.length) {
        TZrBool *hasDefaultPtr = (TZrBool *)ZrCore_Array_Get(&func->paramHasDefaultValues, paramIndex);
        SZrTypeValue *defaultValue = (SZrTypeValue *)ZrCore_Array_Get(&func->paramDefaultValues, paramIndex);
        if (hasDefaultPtr != ZR_NULL && *hasDefaultPtr && defaultValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
            ZrCore_Value_Copy(cs->state, result, defaultValue);
            return ZR_TRUE;
        }
    }

    ct_error_name(cs, paramName, "Missing compile-time argument for parameter: ", (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_invoke_runtime_callable_with_values(SZrCompilerState *cs,
                                                      SZrAstNode *callSite,
                                                      const SZrTypeValue *callableValue,
                                                      TZrSize argCount,
                                                      const SZrTypeValue *argValues,
                                                      SZrTypeValue *result) {
    SZrState *state;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;

    if (cs == ZR_NULL || callableValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    state = cs->state;
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, argCount + 1, base, base, &baseAnchor);
    state->stackTop.valuePointer = base;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), callableValue);
    state->stackTop.valuePointer = base + 1;

    for (TZrSize i = 0; i < argCount; i++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base + 1 + i));
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base + 1 + i), &argValues[i]);
        state->stackTop.valuePointer = base + 2 + i;
    }

    base = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Runtime callable failed during compile-time evaluation",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
    return ZR_TRUE;
}

static TZrBool ct_eval_runtime_callable_metadata_arg(SZrCompilerState *cs,
                                                     const SZrFunction *function,
                                                     SZrFunctionCall *call,
                                                     TZrSize paramIndex,
                                                     SZrCompileTimeFrame *frame,
                                                     SZrTypeValue *result) {
    const SZrFunctionMetadataParameter *parameter;
    TZrSize positionalCount = 0;

    if (cs == ZR_NULL || function == ZR_NULL || result == ZR_NULL || function->parameterMetadata == ZR_NULL ||
        paramIndex >= function->parameterMetadataCount) {
        return ZR_FALSE;
    }

    parameter = &function->parameterMetadata[paramIndex];
    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        for (TZrSize index = 0; index < call->argNames->length && call->args != ZR_NULL && index < call->args->count;
             index++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (parameter->name != ZR_NULL) {
            for (TZrSize index = 0; index < call->argNames->length && call->args != ZR_NULL &&
                                     index < call->args->count;
                 index++) {
                SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
                if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                    ZrCore_String_Equal(*argNamePtr, parameter->name)) {
                    return evaluate_compile_time_expression_internal(cs, call->args->nodes[index], frame, result);
                }
            }
        }

        if (call->args != ZR_NULL && paramIndex < positionalCount) {
            return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
    }

    if (parameter->hasDefaultValue) {
        ZrCore_Value_ResetAsNull(result);
        ZrCore_Value_Copy(cs->state, result, &parameter->defaultValue);
        return ZR_TRUE;
    }

    ct_error_name(cs, parameter->name, "Missing compile-time argument for parameter: ", (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_call_runtime_projected_compile_time_function(SZrCompilerState *cs,
                                                               SZrAstNode *callSite,
                                                               SZrCompileTimeFunction *func,
                                                               SZrFunctionCall *call,
                                                               SZrCompileTimeFrame *frame,
                                                               SZrTypeValue *result) {
    SZrTypeValue callableValue;
    SZrTypeValue *argValues = ZR_NULL;
    TZrSize parameterCount;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    parameterCount = func->paramNames.length;
    if (!ct_value_from_compile_time_function(cs, func, &callableValue)) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Failed to resolve runtime projection for compile-time function",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->args != ZR_NULL && call->args->count > parameterCount) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Too many arguments for compile-time function call",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (parameterCount > 0) {
        argValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                    sizeof(SZrTypeValue) * parameterCount,
                                                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (argValues == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < parameterCount; index++) {
            SZrString **paramNamePtr = (SZrString **)ZrCore_Array_Get(&func->paramNames, index);
            if (!ct_eval_runtime_projected_call_arg(cs,
                                                    func,
                                                    call,
                                                    paramNamePtr != ZR_NULL ? *paramNamePtr : ZR_NULL,
                                                    index,
                                                    frame,
                                                    &argValues[index])) {
                goto cleanup;
            }
        }
    }

    success = ct_invoke_runtime_callable_with_values(cs,
                                                     callSite,
                                                     &callableValue,
                                                     parameterCount,
                                                     argValues,
                                                     result);

cleanup:
    if (argValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argValues,
                                      sizeof(SZrTypeValue) * parameterCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

static TZrBool ct_call_function(SZrCompilerState *cs,
                              SZrAstNode *callSite,
                              SZrCompileTimeFunction *func,
                              SZrFunctionCall *call,
                              SZrCompileTimeFrame *parentFrame,
                              SZrTypeValue *result) {
    SZrFunctionDeclaration *decl;
    SZrCompileTimeFrame frame;
    TZrBool success = ZR_FALSE;
    TZrBool didReturn = ZR_FALSE;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (func->isRuntimeProjection) {
        return ct_call_runtime_projected_compile_time_function(cs, callSite, func, call, parentFrame, result);
    }

    if (func->declaration == ZR_NULL || func->declaration->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &func->declaration->data.functionDeclaration;
    ct_frame_init(cs, &frame, parentFrame);

    if (decl->params != ZR_NULL) {
        for (TZrSize i = 0; i < decl->params->count; i++) {
            SZrAstNode *paramNode = decl->params->nodes[i];
            SZrParameter *param;
            SZrTypeValue argValue;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            param = &paramNode->data.parameter;
            if (!ct_eval_call_arg(cs, call, param, i, &frame, &argValue) ||
                param->name == ZR_NULL || param->name->name == ZR_NULL ||
                !ct_frame_set(cs, &frame, param->name->name, &argValue)) {
                goto cleanup;
            }
        }
    }

    if (call != ZR_NULL && call->args != ZR_NULL && !call->hasNamedArgs) {
        TZrSize expectedArgs = decl->params != ZR_NULL ? decl->params->count : 0;
        if (call->args->count > expectedArgs) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Too many arguments for compile-time function call", callSite->location);
            goto cleanup;
        }
    }

    if (decl->body == ZR_NULL) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time function body is null", callSite->location);
        goto cleanup;
    }

    success = decl->body->type == ZR_AST_BLOCK
                  ? execute_compile_time_block(cs, decl->body, &frame, &didReturn, result)
                  : execute_compile_time_statement(cs, decl->body, &frame, &didReturn, result);
    if (success && !didReturn) {
        ZrCore_Value_ResetAsNull(result);
    }

cleanup:
    ct_frame_free(cs, &frame);
    return success;
}

static TZrBool ct_invoke_runtime_callable(SZrCompilerState *cs,
                                        SZrAstNode *callSite,
                                        const SZrTypeValue *callableValue,
                                        SZrFunctionCall *call,
                                        SZrCompileTimeFrame *frame,
                                        SZrTypeValue *result) {
    const SZrFunction *metadataFunction;
    SZrTypeValue *argValues = ZR_NULL;
    SZrState *state;
    TZrStackValuePointer base;
    TZrSize argCount;
    SZrFunctionStackAnchor baseAnchor;

    if (cs == ZR_NULL || callableValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    state = cs->state;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
    argCount = (call != ZR_NULL && call->args != ZR_NULL) ? call->args->count : 0;

    if (metadataFunction != ZR_NULL && metadataFunction->parameterMetadata != ZR_NULL &&
        metadataFunction->parameterMetadataCount > 0) {
        if (argCount > metadataFunction->parameterMetadataCount) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Too many arguments for runtime callable projection in compile-time evaluation",
                                       callSite != ZR_NULL ? callSite->location
                                                           : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
            return ZR_FALSE;
        }

        argValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                    sizeof(SZrTypeValue) *
                                                                            metadataFunction->parameterMetadataCount,
                                                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (argValues == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < metadataFunction->parameterMetadataCount; index++) {
            if (!ct_eval_runtime_callable_metadata_arg(cs,
                                                       metadataFunction,
                                                       call,
                                                       index,
                                                       frame,
                                                       &argValues[index])) {
                ZrCore_Memory_RawFreeWithType(cs->state->global,
                                              argValues,
                                              sizeof(SZrTypeValue) * metadataFunction->parameterMetadataCount,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
                return ZR_FALSE;
            }
        }

        if (!ct_invoke_runtime_callable_with_values(cs,
                                                    callSite,
                                                    callableValue,
                                                    metadataFunction->parameterMetadataCount,
                                                    argValues,
                                                    result)) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          argValues,
                                          sizeof(SZrTypeValue) * metadataFunction->parameterMetadataCount,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argValues,
                                      sizeof(SZrTypeValue) * metadataFunction->parameterMetadataCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_TRUE;
    }

    if (call != ZR_NULL && call->hasNamedArgs) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Named arguments are not supported for runtime callable projection in compile-time evaluation",
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, argCount + 1, base, base, &baseAnchor);
    state->stackTop.valuePointer = base;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), callableValue);
    state->stackTop.valuePointer = base + 1;

    if (call != ZR_NULL && call->args != ZR_NULL) {
        for (TZrSize i = 0; i < call->args->count; i++) {
            SZrTypeValue argValue;
            if (!evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, &argValue)) {
                base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
                state->stackTop.valuePointer = base;
                return ZR_FALSE;
            }
            base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base + 1 + i));
            ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base + 1 + i), &argValue);
            state->stackTop.valuePointer = base + 2 + i;
        }
    }

    base = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Runtime callable failed during compile-time evaluation",
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
    return ZR_TRUE;
}

static TZrBool ct_eval_object_key(SZrCompilerState *cs,
                                SZrAstNode *keyNode,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result) {
    if (cs == ZR_NULL || keyNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (keyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(keyNode->data.identifier.name));
        result->type = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }

    return evaluate_compile_time_expression_internal(cs, keyNode, frame, result);
}

static TZrBool ct_eval_object_literal(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    SZrCompileTimeFrame *frame,
                                    SZrTypeValue *result) {
    SZrObjectLiteral *objectLiteral;
    SZrObject *object;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_OBJECT_LITERAL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    objectLiteral = &node->data.objectLiteral;
    object = ZrCore_Object_New(cs->state, ZR_NULL);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(cs->state, object);

    if (objectLiteral->properties != ZR_NULL) {
        for (TZrSize i = 0; i < objectLiteral->properties->count; i++) {
            SZrAstNode *propertyNode = objectLiteral->properties->nodes[i];
            SZrTypeValue keyValue;
            SZrTypeValue propertyValue;

            if (propertyNode == ZR_NULL) {
                continue;
            }

            if (propertyNode->type != ZR_AST_KEY_VALUE_PAIR) {
                ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Unsupported compile-time object literal property",
                                   propertyNode->location);
                return ZR_FALSE;
            }

            if (!ct_eval_object_key(cs, propertyNode->data.keyValuePair.key, frame, &keyValue) ||
                !evaluate_compile_time_expression_internal(cs, propertyNode->data.keyValuePair.value, frame, &propertyValue)) {
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(cs->state, object, &keyValue, &propertyValue);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_eval_member_key(SZrCompilerState *cs,
                                SZrMemberExpression *memberExpr,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result) {
    if (cs == ZR_NULL || memberExpr == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!memberExpr->computed && memberExpr->property != ZR_NULL &&
        memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrCore_Value_InitAsRawObject(cs->state,
                                     result,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(memberExpr->property->data.identifier.name));
        result->type = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }

    return evaluate_compile_time_expression_internal(cs, memberExpr->property, frame, result);
}

static TZrBool ct_eval_member_access(SZrCompilerState *cs,
                                   SZrAstNode *callSite,
                                   const SZrTypeValue *baseValue,
                                   SZrMemberExpression *memberExpr,
                                   SZrCompileTimeFrame *frame,
                                   SZrTypeValue *result) {
    SZrTypeValue keyValue;
    const SZrTypeValue *memberValue;

    if (cs == ZR_NULL || baseValue == ZR_NULL || memberExpr == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (baseValue->type != ZR_VALUE_TYPE_OBJECT && baseValue->type != ZR_VALUE_TYPE_ARRAY) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Compile-time member access requires object or array value",
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (!ct_eval_member_key(cs, memberExpr, frame, &keyValue)) {
        return ZR_FALSE;
    }

    memberValue = ZrCore_Object_GetValue(cs->state, ZR_CAST_OBJECT(cs->state, baseValue->value.object), &keyValue);
    if (memberValue == ZR_NULL) {
        TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
        snprintf(message, sizeof(message), "Unknown compile-time member: %s",
                 (!memberExpr->computed && memberExpr->property != ZR_NULL &&
                  memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL)
                         ? ct_name(memberExpr->property->data.identifier.name)
                         : "<computed>");
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, message,
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    *result = *memberValue;
    return ZR_TRUE;
}

static TZrBool ct_call_value(SZrCompilerState *cs,
                           SZrAstNode *callSite,
                           const SZrTypeValue *callableValue,
                           SZrFunctionCall *call,
                           SZrCompileTimeFrame *frame,
                           SZrTypeValue *result) {
    SZrCompileTimeFunction *compileTimeFunction = ZR_NULL;

    if (ct_value_try_get_compile_time_function(cs, callableValue, &compileTimeFunction)) {
        return ct_call_function(cs, callSite, compileTimeFunction, call, frame, result);
    }

    return ct_invoke_runtime_callable(cs, callSite, callableValue, call, frame, result);
}

static TZrBool ct_eval_primary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    SZrTypeValue currentValue;
    TZrSize startIndex = 0;

    if (primary->members == ZR_NULL || primary->members->count == 0) {
        return primary->property != ZR_NULL
                   ? evaluate_compile_time_expression_internal(cs, primary->property, frame, result)
                   : ZR_FALSE;
    }

    if (primary->property != ZR_NULL &&
        primary->property->type == ZR_AST_IDENTIFIER_LITERAL &&
        primary->members->nodes[0] != ZR_NULL &&
        primary->members->nodes[0]->type == ZR_AST_FUNCTION_CALL) {
        SZrString *funcName = primary->property->data.identifier.name;
        SZrFunctionCall *call = &primary->members->nodes[0]->data.functionCall;
        SZrCompileTimeFunction *compileTimeFunction = ZR_NULL;

        if (ct_string_equals(funcName, "FatalError") ||
            ct_string_equals(funcName, "Assert")) {
            if (!ct_eval_builtin_call(cs, node, funcName, call, frame, &currentValue)) {
                return ZR_FALSE;
            }
            startIndex = 1;
        } else if ((compileTimeFunction = find_compile_time_function(cs, funcName)) != ZR_NULL) {
            if (!ct_call_function(cs, primary->members->nodes[0], compileTimeFunction, call, frame, &currentValue)) {
                return ZR_FALSE;
            }
            startIndex = 1;
        } else if (!evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
            return ZR_FALSE;
        }
    } else if (!evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
        return ZR_FALSE;
    }

    for (TZrSize i = startIndex; i < primary->members->count; i++) {
        SZrAstNode *memberNode = primary->members->nodes[i];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
            if (!ct_eval_member_access(cs, memberNode, &currentValue, &memberNode->data.memberExpression, frame,
                                       &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            if (!ct_call_value(cs, memberNode, &currentValue, &memberNode->data.functionCall, frame, &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Unsupported compile-time primary expression member",
                           memberNode->location);
        return ZR_FALSE;
    }

    *result = currentValue;
    return ZR_TRUE;
}

static TZrBool ct_assign_identifier(SZrCompilerState *cs,
                                    SZrCompileTimeFrame *frame,
                                    SZrString *name,
                                    const SZrTypeValue *value,
                                    SZrFileRange location,
                                    SZrTypeValue *result) {
    SZrCompileTimeFrame *cursor = frame;

    if (cs == ZR_NULL || name == ZR_NULL || value == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    while (cursor != ZR_NULL) {
        for (TZrSize i = cursor->bindings.length; i > 0; i--) {
            SZrCompileTimeBinding *binding =
                    (SZrCompileTimeBinding *)ZrCore_Array_Get(&cursor->bindings, i - 1);
            if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
                binding->value = *value;
                *result = *value;
                return ZR_TRUE;
            }
        }
        cursor = cursor->parent;
    }

    {
        SZrCompileTimeVariable *var = find_compile_time_variable(cs, name);
        if (var != ZR_NULL) {
            var->evaluatedValue = *value;
            var->hasEvaluatedValue = ZR_TRUE;
            var->isEvaluating = ZR_FALSE;
            *result = *value;
            return ZR_TRUE;
        }
    }

    ct_error_name(cs, name, "Unknown compile-time assignment target: ", location);
    return ZR_FALSE;
}

static TZrBool ct_resolve_primary_assignment_target(SZrCompilerState *cs,
                                                    SZrAstNode *node,
                                                    SZrCompileTimeFrame *frame,
                                                    SZrObject **targetObject,
                                                    SZrTypeValue *keyValue) {
    SZrPrimaryExpression *primary;
    SZrTypeValue currentValue;
    SZrAstNode *lastMemberNode;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION ||
        targetObject == ZR_NULL || keyValue == ZR_NULL) {
        return ZR_FALSE;
    }

    *targetObject = ZR_NULL;
    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->members == ZR_NULL || primary->members->count == 0) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment target must end in a member access",
                                   node->location);
        return ZR_FALSE;
    }

    if (!evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i + 1 < primary->members->count; i++) {
        SZrAstNode *memberNode = primary->members->nodes[i];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
            if (!ct_eval_member_access(cs,
                                       memberNode,
                                       &currentValue,
                                       &memberNode->data.memberExpression,
                                       frame,
                                       &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            if (!ct_call_value(cs, memberNode, &currentValue, &memberNode->data.functionCall, frame, &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Unsupported compile-time assignment target member",
                                   memberNode->location);
        return ZR_FALSE;
    }

    lastMemberNode = primary->members->nodes[primary->members->count - 1];
    if (lastMemberNode == ZR_NULL || lastMemberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment target must end in a member access",
                                   node->location);
        return ZR_FALSE;
    }

    if ((currentValue.type != ZR_VALUE_TYPE_OBJECT && currentValue.type != ZR_VALUE_TYPE_ARRAY) ||
        currentValue.value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment member target requires object or array value",
                                   node->location);
        return ZR_FALSE;
    }

    if (!ct_eval_member_key(cs, &lastMemberNode->data.memberExpression, frame, keyValue)) {
        return ZR_FALSE;
    }

    *targetObject = ZR_CAST_OBJECT(cs->state, currentValue.value.object);
    return *targetObject != ZR_NULL;
}

static TZrBool ct_eval_assignment(SZrCompilerState *cs,
                                SZrAstNode *node,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result) {
    SZrAssignmentExpression *expr;
    SZrTypeValue assignedValue;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION || result == ZR_NULL) {
        return ZR_FALSE;
    }

    expr = &node->data.assignmentExpression;
    if (expr->op.op == ZR_NULL || strcmp(expr->op.op, "=") != 0) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Only '=' assignments are supported in compile-time expressions",
                                   node->location);
        return ZR_FALSE;
    }

    if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &assignedValue)) {
        return ZR_FALSE;
    }

    if (expr->left == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment target is missing",
                                   node->location);
        return ZR_FALSE;
    }

    if (expr->left->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ct_assign_identifier(cs,
                                    frame,
                                    expr->left->data.identifier.name,
                                    &assignedValue,
                                    expr->left->location,
                                    result);
    }

    if (expr->left->type == ZR_AST_PRIMARY_EXPRESSION) {
        SZrPrimaryExpression *primary = &expr->left->data.primaryExpression;

        if (primary->members == ZR_NULL || primary->members->count == 0) {
            if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                return ct_assign_identifier(cs,
                                            frame,
                                            primary->property->data.identifier.name,
                                            &assignedValue,
                                            primary->property->location,
                                            result);
            }

            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time assignment target must be an identifier or member access",
                                       expr->left->location);
            return ZR_FALSE;
        }

        {
            SZrObject *targetObject = ZR_NULL;
            SZrTypeValue keyValue;

            if (!ct_resolve_primary_assignment_target(cs, expr->left, frame, &targetObject, &keyValue)) {
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(cs->state, targetObject, &keyValue, &assignedValue);
            *result = assignedValue;
            return ZR_TRUE;
        }
    }

    ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Compile-time assignment target must be an identifier or member access",
                               expr->left->location);
    return ZR_FALSE;
}

TZrBool evaluate_compile_time_expression_internal(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrCompileTimeFrame *frame,
                                                       SZrTypeValue *result) {
    TZrBool oldContext;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
            ZrCore_Value_InitAsInt(cs->state, result, node->data.integerLiteral.value);
            break;
        case ZR_AST_FLOAT_LITERAL:
            ZrCore_Value_InitAsFloat(cs->state, result, node->data.floatLiteral.value);
            break;
        case ZR_AST_BOOLEAN_LITERAL:
            ZrCore_Value_InitAsUInt(cs->state, result, node->data.booleanLiteral.value ? 1 : 0);
            result->type = ZR_VALUE_TYPE_BOOL;
            break;
        case ZR_AST_STRING_LITERAL:
            ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(node->data.stringLiteral.value));
            result->type = ZR_VALUE_TYPE_STRING;
            break;
        case ZR_AST_NULL_LITERAL:
            ZrCore_Value_ResetAsNull(result);
            break;
        case ZR_AST_IDENTIFIER_LITERAL:
            if (!ct_frame_get(frame, node->data.identifier.name, result) &&
                !ZrParser_Compiler_TryGetCompileTimeValue(cs, node->data.identifier.name, result)) {
                SZrCompileTimeFunction *func = find_compile_time_function(cs, node->data.identifier.name);
                if (func != ZR_NULL && ct_value_from_compile_time_function(cs, func, result)) {
                    break;
                }
                ct_error_name(cs, node->data.identifier.name, "Unknown compile-time identifier: ", node->location);
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_object_literal(cs, node, frame, result);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_assignment(cs, node, frame, result);
        case ZR_AST_BINARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_binary(cs, node, frame, result);
        case ZR_AST_LOGICAL_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_logical(cs, node, frame, result);
        case ZR_AST_UNARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_unary(cs, node, frame, result);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_type_cast(cs, node, frame, result);
        case ZR_AST_IMPORT_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_import_expression(cs, node, result);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "%type is not supported in compile-time expressions",
                                       node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *expr = &node->data.conditionalExpression;
            SZrTypeValue condValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->test, frame, &condValue)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return evaluate_compile_time_expression_internal(cs, ct_truthy(&condValue) ? expr->consequent : expr->alternate, frame, result);
        }
        case ZR_AST_PRIMARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_primary(cs, node, frame, result);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Prototype references are not supported in compile-time expressions",
                               node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Prototype construction is not supported in compile-time expressions",
                               node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_EXPRESSION_STATEMENT:
            cs->isInCompileTimeContext = oldContext;
            return evaluate_compile_time_expression_internal(cs, node->data.expressionStatement.expr, frame, result);
        default:
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time expression node", node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
    }

    cs->isInCompileTimeContext = oldContext;
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_EvaluateCompileTimeExpression(SZrCompilerState *cs,
                                                            SZrAstNode *node,
                                                            SZrTypeValue *result) {
    return evaluate_compile_time_expression_internal(cs, node, ZR_NULL, result);
}

TZrBool execute_compile_time_statement(SZrCompilerState *cs,
                                            SZrAstNode *node,
                                            SZrCompileTimeFrame *frame,
                                            TZrBool *didReturn,
                                            SZrTypeValue *result) {
    if (didReturn != ZR_NULL) {
        *didReturn = ZR_FALSE;
    }
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BLOCK:
            return execute_compile_time_block(cs, node, frame, didReturn, result);
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *stmt = &node->data.returnStatement;
            if (result == ZR_NULL) {
                return ZR_FALSE;
            }
            if (stmt->expr == ZR_NULL) {
                ZrCore_Value_ResetAsNull(result);
            } else if (!evaluate_compile_time_expression_internal(cs, stmt->expr, frame, result)) {
                return ZR_FALSE;
            }
            if (didReturn != ZR_NULL) {
                *didReturn = ZR_TRUE;
            }
            return ZR_TRUE;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *decl = &node->data.variableDeclaration;
            SZrTypeValue value;
            if (frame == ZR_NULL) {
                return register_compile_time_variable_declaration(cs, node, node->location);
            }
            if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_TRUE;
            }
            if (decl->value != ZR_NULL && !evaluate_compile_time_expression_internal(cs, decl->value, frame, &value)) {
                return ZR_FALSE;
            }
            if (decl->value == ZR_NULL) {
                ZrCore_Value_ResetAsNull(&value);
            }
            return ct_frame_set(cs, frame, decl->pattern->data.identifier.name, &value);
        }
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *expr = &node->data.ifExpression;
            SZrTypeValue condValue;
            SZrAstNode *branch;
            if (!evaluate_compile_time_expression_internal(cs, expr->condition, frame, &condValue)) {
                return ZR_FALSE;
            }
            branch = ct_truthy(&condValue) ? expr->thenExpr : expr->elseExpr;
            if (branch == ZR_NULL) {
                return ZR_TRUE;
            }
            if (branch->type == ZR_AST_BLOCK || branch->type == ZR_AST_RETURN_STATEMENT ||
                branch->type == ZR_AST_IF_EXPRESSION || branch->type == ZR_AST_VARIABLE_DECLARATION ||
                branch->type == ZR_AST_FUNCTION_DECLARATION ||
                branch->type == ZR_AST_EXPRESSION_STATEMENT) {
                return execute_compile_time_statement(cs, branch, frame, didReturn, result);
            }
            return result != ZR_NULL ? evaluate_compile_time_expression_internal(cs, branch, frame, result) : ZR_TRUE;
        }
        case ZR_AST_FUNCTION_DECLARATION:
            if (frame == ZR_NULL) {
                return register_compile_time_function_declaration(cs, node, node->location);
            }
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                               "Nested compile-time function declarations are not supported in local frames",
                               node->location);
            return ZR_FALSE;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return ZrParser_CompileTimeDeclaration_Execute(cs, node);
        default: {
            SZrTypeValue ignored;
            return evaluate_compile_time_expression_internal(cs, node, frame, result != ZR_NULL ? result : &ignored);
        }
    }
}

TZrBool execute_compile_time_block(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        SZrCompileTimeFrame *frame,
                                        TZrBool *didReturn,
                                        SZrTypeValue *result) {
    SZrBlock *block;
    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return ZR_FALSE;
    }
    block = &node->data.block;
    if (block->body == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize i = 0; i < block->body->count; i++) {
        TZrBool returned = ZR_FALSE;
        if (!execute_compile_time_statement(cs, block->body->nodes[i], frame, &returned, result)) {
            return ZR_FALSE;
        }
        if (returned) {
            if (didReturn != ZR_NULL) {
                *didReturn = ZR_TRUE;
            }
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_CompileTimeDeclaration_Execute(SZrCompilerState *cs, SZrAstNode *node) {
    SZrCompileTimeDeclaration *decl;
    SZrAstNode *body;
    TZrBool oldContext;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_COMPILE_TIME_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &node->data.compileTimeDeclaration;
    body = decl->declaration;
    if (body == ZR_NULL) {
        return ZR_FALSE;
    }

    oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;

    switch (decl->declarationType) {
        case ZR_COMPILE_TIME_VARIABLE: {
            if (!register_compile_time_variable_declaration(cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;
        }

        case ZR_COMPILE_TIME_FUNCTION: {
            if (!register_compile_time_function_declaration(cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;
        }

        case ZR_COMPILE_TIME_CLASS:
            if (!ct_register_compile_time_decorator_class(cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;
        case ZR_COMPILE_TIME_STRUCT:
            if (!ct_register_compile_time_decorator_class(cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;

        case ZR_COMPILE_TIME_STATEMENT: {
            TZrBool didReturn = ZR_FALSE;
            SZrTypeValue ignored;
            TZrBool ok = body->type == ZR_AST_BLOCK
                           ? execute_compile_time_block(cs, body, ZR_NULL, &didReturn, &ignored)
                           : execute_compile_time_statement(cs, body, ZR_NULL, &didReturn, &ignored);
            cs->isInCompileTimeContext = oldContext;
            return ok;
        }

        case ZR_COMPILE_TIME_EXPRESSION: {
            SZrTypeValue ignored;
            TZrBool ok = evaluate_compile_time_expression_internal(cs, body, ZR_NULL, &ignored);
            cs->isInCompileTimeContext = oldContext;
            return ok;
        }
    }

    cs->isInCompileTimeContext = oldContext;
    return ZR_FALSE;
}
