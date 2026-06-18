#include "compiler_task_effects_internal.h"

typedef enum EZrTaskEffectBindingKind {
    ZR_TASK_EFFECT_BINDING_NONE = 0,
    ZR_TASK_EFFECT_BINDING_THREAD_MODULE = 1,
    ZR_TASK_EFFECT_BINDING_UNIQUE_MUTEX = 2,
    ZR_TASK_EFFECT_BINDING_SHARED_MUTEX = 3,
    ZR_TASK_EFFECT_BINDING_AFFINE_GUARD = 4,
    ZR_TASK_EFFECT_BINDING_BORROWED = 5,
    ZR_TASK_EFFECT_BINDING_LOANED = 6,
    ZR_TASK_EFFECT_BINDING_PLUGIN_GUARD = 7
} EZrTaskEffectBindingKind;

typedef struct ZrTaskEffectBinding {
    SZrString *name;
    TZrInt32 scopeDepth;
    TZrBool isBorrowed;
    EZrTaskEffectBindingKind bindingKind;
    TZrBool inheritedAfterAwait;
} ZrTaskEffectBinding;

struct ZrTaskEffectContext {
    SZrCompilerState *cs;
    SZrArray bindings;
    TZrInt32 scopeDepth;
    TZrBool asyncAllowed;
    TZrBool awaitSeen;
};

static TZrBool task_effects_string_equals(SZrString *value, const TZrChar *literal) {
    const TZrChar *nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

static TZrBool task_effects_identifier_equals(const SZrIdentifier *identifier, const TZrChar *literal) {
    return identifier != ZR_NULL && task_effects_string_equals(identifier->name, literal);
}

static TZrBool task_effects_ast_type_contains_ownership(const SZrType *typeInfo,
                                                        EZrOwnershipQualifier qualifier) {
    const SZrType *innerType = ZR_NULL;
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    TZrSize index;

    if (typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeInfo->ownershipQualifier == qualifier) {
        return ZR_TRUE;
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(typeInfo, &ownershipQualifier, &innerType)) {
        if (ownershipQualifier == qualifier) {
            return ZR_TRUE;
        }
        if (task_effects_ast_type_contains_ownership(innerType, qualifier)) {
            return ZR_TRUE;
        }
    }

    if (task_effects_ast_type_contains_ownership(typeInfo->subType, qualifier)) {
        return ZR_TRUE;
    }

    if (typeInfo->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE && typeInfo->name->data.genericType.params != ZR_NULL) {
        SZrAstNodeArray *params = typeInfo->name->data.genericType.params;
        for (index = 0; index < params->count; index++) {
            SZrAstNode *argument = params->nodes[index];
            if (argument != ZR_NULL &&
                argument->type == ZR_AST_TYPE &&
                task_effects_ast_type_contains_ownership(&argument->data.type, qualifier)) {
                return ZR_TRUE;
            }
        }
    } else if (typeInfo->name->type == ZR_AST_TUPLE_TYPE &&
               typeInfo->name->data.tupleType.elements != ZR_NULL) {
        SZrAstNodeArray *elements = typeInfo->name->data.tupleType.elements;
        for (index = 0; index < elements->count; index++) {
            SZrAstNode *element = elements->nodes[index];
            if (element != ZR_NULL &&
                element->type == ZR_AST_TYPE &&
                task_effects_ast_type_contains_ownership(&element->data.type, qualifier)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool task_effects_type_is_borrowed(const SZrType *typeInfo) {
    return task_effects_ast_type_contains_ownership(typeInfo, ZR_OWNERSHIP_QUALIFIER_BORROWED);
}

static TZrBool task_effects_type_is_loaned(const SZrType *typeInfo) {
    return task_effects_ast_type_contains_ownership(typeInfo, ZR_OWNERSHIP_QUALIFIER_LOANED);
}

static TZrBool task_effects_binding_kind_is_mutex(EZrTaskEffectBindingKind bindingKind) {
    return bindingKind == ZR_TASK_EFFECT_BINDING_UNIQUE_MUTEX ||
           bindingKind == ZR_TASK_EFFECT_BINDING_SHARED_MUTEX;
}

static TZrBool task_effects_binding_kind_is_affine_guard(EZrTaskEffectBindingKind bindingKind) {
    return bindingKind == ZR_TASK_EFFECT_BINDING_AFFINE_GUARD;
}

static TZrBool task_effects_binding_kind_is_plugin_guard(EZrTaskEffectBindingKind bindingKind) {
    return bindingKind == ZR_TASK_EFFECT_BINDING_PLUGIN_GUARD;
}

static TZrBool task_effects_context_init(ZrTaskEffectContext *context,
                                         SZrCompilerState *cs,
                                         TZrBool asyncAllowed,
                                         const ZrTaskEffectContext *parent) {
    TZrSize capacity = 8;

    if (context == ZR_NULL || cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (parent != ZR_NULL && parent->bindings.length > capacity) {
        capacity = parent->bindings.length + 4;
    }

    context->cs = cs;
    context->scopeDepth = 0;
    context->asyncAllowed = asyncAllowed;
    context->awaitSeen = ZR_FALSE;
    ZrCore_Array_Init(cs->state, &context->bindings, sizeof(ZrTaskEffectBinding), capacity);

    if (parent != ZR_NULL) {
        TZrSize index;
        for (index = 0; index < parent->bindings.length; index++) {
            ZrTaskEffectBinding *parentBinding =
                    (ZrTaskEffectBinding *)ZrCore_Array_Get((SZrArray *)&parent->bindings, index);
            ZrTaskEffectBinding inheritedBinding;

            if (parentBinding == ZR_NULL) {
                continue;
            }

            inheritedBinding = *parentBinding;
            inheritedBinding.scopeDepth = 0;
            inheritedBinding.inheritedAfterAwait =
                    (TZrBool)(inheritedBinding.inheritedAfterAwait || parent->awaitSeen);
            ZrCore_Array_Push(cs->state, &context->bindings, &inheritedBinding);
        }
    }

    return ZR_TRUE;
}

static void task_effects_context_free(ZrTaskEffectContext *context) {
    if (context == ZR_NULL || context->cs == ZR_NULL || context->cs->state == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(context->cs->state, &context->bindings);
}

static void task_effects_enter_scope(ZrTaskEffectContext *context) {
    if (context != ZR_NULL) {
        context->scopeDepth++;
    }
}

static void task_effects_leave_scope(ZrTaskEffectContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    while (context->bindings.length > 0) {
        ZrTaskEffectBinding *binding =
                (ZrTaskEffectBinding *)ZrCore_Array_Get(&context->bindings, context->bindings.length - 1);
        if (binding == ZR_NULL || binding->scopeDepth < context->scopeDepth) {
            break;
        }
        ZrCore_Array_Pop(&context->bindings);
    }

    if (context->scopeDepth > 0) {
        context->scopeDepth--;
    }
}

static void task_effects_push_binding(ZrTaskEffectContext *context,
                                      SZrString *name,
                                      TZrBool isBorrowed,
                                      EZrTaskEffectBindingKind bindingKind) {
    ZrTaskEffectBinding binding;

    if (context == ZR_NULL || context->cs == ZR_NULL || context->cs->state == ZR_NULL || name == ZR_NULL) {
        return;
    }

    binding.name = name;
    binding.scopeDepth = context->scopeDepth;
    binding.isBorrowed = isBorrowed;
    binding.bindingKind = bindingKind;
    binding.inheritedAfterAwait = ZR_FALSE;
    ZrCore_Array_Push(context->cs->state, &context->bindings, &binding);
}

static const ZrTaskEffectBinding *task_effects_find_binding(const ZrTaskEffectContext *context, SZrString *name) {
    TZrSize index;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = context->bindings.length; index > 0; index--) {
        ZrTaskEffectBinding *binding =
                (ZrTaskEffectBinding *)ZrCore_Array_Get((SZrArray *)&context->bindings, index - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Compare(context->cs->state, binding->name, name)) {
            return binding;
        }
    }

    return ZR_NULL;
}

static ZrTaskEffectBinding *task_effects_find_mutable_binding(ZrTaskEffectContext *context, SZrString *name) {
    TZrSize index;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = context->bindings.length; index > 0; index--) {
        ZrTaskEffectBinding *binding =
                (ZrTaskEffectBinding *)ZrCore_Array_Get(&context->bindings, index - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Compare(context->cs->state, binding->name, name)) {
            return binding;
        }
    }

    return ZR_NULL;
}

static SZrString *task_effects_bare_identifier_name(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION &&
        node->data.primaryExpression.property != ZR_NULL &&
        node->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
        (node->data.primaryExpression.members == ZR_NULL ||
         node->data.primaryExpression.members->count == 0)) {
        return node->data.primaryExpression.property->data.identifier.name;
    }

    return ZR_NULL;
}

static void task_effects_report_borrow_after_await(ZrTaskEffectContext *context,
                                                   SZrString *name,
                                                   SZrFileRange location) {
    TZrNativeString nativeName;
    TZrChar message[256];

    if (context == ZR_NULL || context->cs == ZR_NULL) {
        return;
    }

    nativeName = name != ZR_NULL ? ZrCore_String_GetNativeStringShort(name) : ZR_NULL;
    if (nativeName == ZR_NULL) {
        nativeName = "<borrowed>";
    }

    snprintf(message,
             sizeof(message),
             "Borrowed binding '%s' cannot be used after an await boundary",
             nativeName);
    ZrParser_Compiler_Error(context->cs, message, location);
}

static void task_effects_report_loan_after_await(ZrTaskEffectContext *context,
                                                 SZrString *name,
                                                 SZrFileRange location) {
    TZrNativeString nativeName;
    TZrChar message[256];

    if (context == ZR_NULL || context->cs == ZR_NULL) {
        return;
    }

    nativeName = name != ZR_NULL ? ZrCore_String_GetNativeStringShort(name) : ZR_NULL;
    if (nativeName == ZR_NULL) {
        nativeName = "<loaned>";
    }

    snprintf(message,
             sizeof(message),
             "Loaned binding '%s' cannot be used after an await boundary",
             nativeName);
    ZrParser_Compiler_Error(context->cs, message, location);
}

static void task_effects_report_affine_guard_after_await(ZrTaskEffectContext *context,
                                                         SZrString *name,
                                                         SZrFileRange location) {
    TZrNativeString nativeName;
    TZrChar message[256];

    if (context == ZR_NULL || context->cs == ZR_NULL) {
        return;
    }

    nativeName = name != ZR_NULL ? ZrCore_String_GetNativeStringShort(name) : ZR_NULL;
    if (nativeName == ZR_NULL) {
        nativeName = "<lock>";
    }

    snprintf(message,
             sizeof(message),
             "Affine guard '%s' cannot be used after an await boundary",
             nativeName);
    ZrParser_Compiler_Error(context->cs, message, location);
}

static void task_effects_report_plugin_guard_after_await(ZrTaskEffectContext *context,
                                                         SZrString *name,
                                                         SZrFileRange location) {
    TZrNativeString nativeName;
    TZrChar message[256];

    if (context == ZR_NULL || context->cs == ZR_NULL) {
        return;
    }

    nativeName = name != ZR_NULL ? ZrCore_String_GetNativeStringShort(name) : ZR_NULL;
    if (nativeName == ZR_NULL) {
        nativeName = "<plugin>";
    }

    snprintf(message,
             sizeof(message),
             "Plugin guard binding '%s' cannot be used after an await boundary",
             nativeName);
    ZrParser_Compiler_Error(context->cs, message, location);
}

void task_effects_validate_node(ZrTaskEffectContext *context, SZrAstNode *node);
static EZrTaskEffectBindingKind task_effects_infer_binding_kind(const ZrTaskEffectContext *context, SZrAstNode *node);
static void task_effects_validate_type_info(ZrTaskEffectContext *context, const SZrType *typeInfo);

void task_effects_validate_node_array(ZrTaskEffectContext *context, SZrAstNodeArray *nodes) {
    TZrSize index;

    if (context == ZR_NULL || nodes == ZR_NULL) {
        return;
    }

    for (index = 0; index < nodes->count && !context->cs->hasError; index++) {
        task_effects_validate_node(context, nodes->nodes[index]);
    }
}

static void task_effects_validate_generic_arguments(ZrTaskEffectContext *context, SZrAstNodeArray *arguments) {
    TZrSize index;

    if (context == ZR_NULL || context->cs == ZR_NULL || arguments == ZR_NULL) {
        return;
    }

    for (index = 0; index < arguments->count && !context->cs->hasError; index++) {
        SZrAstNode *argument = arguments->nodes[index];
        if (argument == ZR_NULL) {
            continue;
        }

        if (argument->type == ZR_AST_TYPE) {
            task_effects_validate_type_info(context, &argument->data.type);
        } else {
            task_effects_validate_node(context, argument);
        }
    }
}

static void task_effects_validate_type_name_metadata(ZrTaskEffectContext *context, SZrAstNode *nameNode) {
    if (context == ZR_NULL || context->cs == ZR_NULL || nameNode == ZR_NULL || context->cs->hasError) {
        return;
    }

    switch (nameNode->type) {
        case ZR_AST_GENERIC_TYPE:
            task_effects_validate_generic_arguments(context, nameNode->data.genericType.params);
            break;
        case ZR_AST_TUPLE_TYPE:
            task_effects_validate_generic_arguments(context, nameNode->data.tupleType.elements);
            break;
        case ZR_AST_IDENTIFIER_LITERAL:
            task_effects_validate_node(context, nameNode);
            break;
        case ZR_AST_TYPE:
            task_effects_validate_type_info(context, &nameNode->data.type);
            break;
        default:
            break;
    }
}

static void task_effects_validate_type_info(ZrTaskEffectContext *context, const SZrType *typeInfo) {
    if (context == ZR_NULL || context->cs == ZR_NULL || typeInfo == ZR_NULL || context->cs->hasError) {
        return;
    }

    task_effects_validate_type_name_metadata(context, typeInfo->name);
    if (context->cs->hasError) {
        return;
    }

    task_effects_validate_type_info(context, typeInfo->subType);
    if (context->cs->hasError) {
        return;
    }

    task_effects_validate_node(context, typeInfo->arraySizeExpression);
}

static void task_effects_validate_function_call(ZrTaskEffectContext *context, const SZrFunctionCall *call) {
    if (context == ZR_NULL || context->cs == ZR_NULL || call == ZR_NULL || context->cs->hasError) {
        return;
    }

    task_effects_validate_generic_arguments(context, call->genericArguments);
    if (context->cs->hasError) {
        return;
    }
    task_effects_validate_node_array(context, call->args);
}

static void task_effects_register_pattern_binding(ZrTaskEffectContext *context,
                                                  SZrAstNode *pattern,
                                                  TZrBool isBorrowed,
                                                  EZrTaskEffectBindingKind bindingKind) {
    TZrSize index;

    if (context == ZR_NULL || pattern == ZR_NULL) {
        return;
    }

    switch (pattern->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            task_effects_push_binding(context, pattern->data.identifier.name, isBorrowed, bindingKind);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            if (!pattern->data.keyValuePair.keyIsComputed &&
                pattern->data.keyValuePair.key != ZR_NULL &&
                pattern->data.keyValuePair.key->type == ZR_AST_IDENTIFIER_LITERAL) {
                task_effects_register_pattern_binding(context,
                                                      pattern->data.keyValuePair.key,
                                                      isBorrowed,
                                                      bindingKind);
            }
            break;
        case ZR_AST_DESTRUCTURING_OBJECT:
            if (pattern->data.destructuringObject.keys != ZR_NULL) {
                for (index = 0; index < pattern->data.destructuringObject.keys->count; index++) {
                    task_effects_register_pattern_binding(context,
                                                          pattern->data.destructuringObject.keys->nodes[index],
                                                          isBorrowed,
                                                          bindingKind);
                }
            }
            break;
        case ZR_AST_DESTRUCTURING_ARRAY:
            if (pattern->data.destructuringArray.keys != ZR_NULL) {
                for (index = 0; index < pattern->data.destructuringArray.keys->count; index++) {
                    task_effects_register_pattern_binding(context,
                                                          pattern->data.destructuringArray.keys->nodes[index],
                                                          isBorrowed,
                                                          bindingKind);
                }
            }
            break;
        default:
            break;
    }
}

static void task_effects_register_using_guard_pattern_binding(ZrTaskEffectContext *context,
                                                              SZrAstNode *pattern,
                                                              EZrTaskEffectBindingKind bindingKind) {
    TZrSize index;

    if (context == ZR_NULL || pattern == ZR_NULL) {
        return;
    }

    switch (pattern->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            task_effects_push_binding(context, pattern->data.identifier.name, ZR_FALSE, bindingKind);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            if (!pattern->data.keyValuePair.keyIsComputed &&
                pattern->data.keyValuePair.value != ZR_NULL &&
                pattern->data.keyValuePair.value->type == ZR_AST_IDENTIFIER_LITERAL) {
                task_effects_register_using_guard_pattern_binding(context,
                                                                  pattern->data.keyValuePair.value,
                                                                  bindingKind);
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            if (pattern->data.objectLiteral.properties != ZR_NULL) {
                for (index = 0; index < pattern->data.objectLiteral.properties->count; index++) {
                    task_effects_register_using_guard_pattern_binding(
                            context,
                            pattern->data.objectLiteral.properties->nodes[index],
                            bindingKind);
                }
            }
            break;
        case ZR_AST_DESTRUCTURING_ARRAY:
            if (pattern->data.destructuringArray.keys != ZR_NULL) {
                for (index = 0; index < pattern->data.destructuringArray.keys->count; index++) {
                    task_effects_register_using_guard_pattern_binding(
                            context,
                            pattern->data.destructuringArray.keys->nodes[index],
                            bindingKind);
                }
            }
            break;
        default:
            break;
    }
}

static void task_effects_register_parameter_node(ZrTaskEffectContext *context, SZrAstNode *parameterNode) {
    EZrTaskEffectBindingKind bindingKind = ZR_TASK_EFFECT_BINDING_NONE;

    if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
        return;
    }

    if (task_effects_type_is_loaned(parameterNode->data.parameter.typeInfo)) {
        bindingKind = ZR_TASK_EFFECT_BINDING_LOANED;
    }

    task_effects_push_binding(context,
                              parameterNode->data.parameter.name != ZR_NULL ? parameterNode->data.parameter.name->name
                                                                            : ZR_NULL,
                              task_effects_type_is_borrowed(parameterNode->data.parameter.typeInfo),
                              bindingKind);
}

static void task_effects_register_parameter_list(ZrTaskEffectContext *context, SZrAstNodeArray *params) {
    TZrSize index;

    if (context == ZR_NULL || params == ZR_NULL) {
        return;
    }

    for (index = 0; index < params->count; index++) {
        task_effects_register_parameter_node(context, params->nodes[index]);
    }
}

static void task_effects_register_vararg_parameter(ZrTaskEffectContext *context, SZrParameter *parameter) {
    EZrTaskEffectBindingKind bindingKind = ZR_TASK_EFFECT_BINDING_NONE;

    if (context == ZR_NULL || parameter == ZR_NULL || parameter->name == ZR_NULL) {
        return;
    }

    if (task_effects_type_is_loaned(parameter->typeInfo)) {
        bindingKind = ZR_TASK_EFFECT_BINDING_LOANED;
    }

    task_effects_push_binding(context,
                              parameter->name->name,
                              task_effects_type_is_borrowed(parameter->typeInfo),
                              bindingKind);
}

static TZrBool task_effects_is_zr_task_import(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IMPORT_EXPRESSION || node->data.importExpression.modulePath == ZR_NULL ||
        node->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }

    return task_effects_string_equals(node->data.importExpression.modulePath->data.stringLiteral.value, "zr.task");
}

static TZrBool task_effects_is_zr_thread_import(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IMPORT_EXPRESSION || node->data.importExpression.modulePath == ZR_NULL ||
        node->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }

    return task_effects_string_equals(node->data.importExpression.modulePath->data.stringLiteral.value, "zr.thread");
}

static TZrBool task_effects_type_node_is_named(SZrAstNode *node, const TZrChar *literal) {
    if (node == ZR_NULL || literal == ZR_NULL || node->type != ZR_AST_TYPE || node->data.type.name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->data.type.name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return task_effects_identifier_equals(&node->data.type.name->data.identifier, literal);
    }

    return node->data.type.name->type == ZR_AST_GENERIC_TYPE && node->data.type.name->data.genericType.name != ZR_NULL &&
           task_effects_string_equals(node->data.type.name->data.genericType.name->name, literal);
}

static TZrBool task_effects_member_is_named(SZrAstNode *node, const TZrChar *literal) {
    return node != ZR_NULL && node->type == ZR_AST_MEMBER_EXPRESSION && !node->data.memberExpression.computed &&
           node->data.memberExpression.property != ZR_NULL &&
           ((node->data.memberExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
             task_effects_identifier_equals(&node->data.memberExpression.property->data.identifier, literal)) ||
            task_effects_type_node_is_named(node->data.memberExpression.property, literal));
}

static EZrTaskEffectBindingKind task_effects_infer_construct_binding_kind(const ZrTaskEffectContext *context,
                                                                          SZrAstNode *node) {
    SZrAstNode *target;
    SZrPrimaryExpression *primary;
    EZrTaskEffectBindingKind targetKind;

    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    if (!node->data.constructExpression.isNew) {
        if (node->data.constructExpression.builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_BORROW) {
            return ZR_TASK_EFFECT_BINDING_BORROWED;
        }
        if (node->data.constructExpression.builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_LOAN) {
            return ZR_TASK_EFFECT_BINDING_LOANED;
        }
    }

    target = node->data.constructExpression.target;
    if (target == ZR_NULL || target->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    primary = &target->data.primaryExpression;
    targetKind = task_effects_infer_binding_kind(context, primary->property);
    if (targetKind != ZR_TASK_EFFECT_BINDING_THREAD_MODULE || primary->members == ZR_NULL || primary->members->count == 0) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    if (task_effects_member_is_named(primary->members->nodes[0], "UniqueMutex")) {
        return ZR_TASK_EFFECT_BINDING_UNIQUE_MUTEX;
    }

    if (task_effects_member_is_named(primary->members->nodes[0], "SharedMutex")) {
        return ZR_TASK_EFFECT_BINDING_SHARED_MUTEX;
    }

    return ZR_TASK_EFFECT_BINDING_NONE;
}

static EZrTaskEffectBindingKind task_effects_infer_primary_binding_kind(const ZrTaskEffectContext *context,
                                                                        SZrAstNode *node) {
    SZrPrimaryExpression *primary;
    EZrTaskEffectBindingKind receiverKind;

    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    primary = &node->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count < 2) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    receiverKind = task_effects_infer_binding_kind(context, primary->property);
    if (!task_effects_binding_kind_is_mutex(receiverKind) || primary->members->nodes[1] == ZR_NULL ||
        primary->members->nodes[1]->type != ZR_AST_FUNCTION_CALL) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    if (receiverKind == ZR_TASK_EFFECT_BINDING_UNIQUE_MUTEX &&
        task_effects_member_is_named(primary->members->nodes[0], "lock")) {
        return ZR_TASK_EFFECT_BINDING_AFFINE_GUARD;
    }

    if (receiverKind == ZR_TASK_EFFECT_BINDING_SHARED_MUTEX &&
        (task_effects_member_is_named(primary->members->nodes[0], "read") ||
         task_effects_member_is_named(primary->members->nodes[0], "write"))) {
        return ZR_TASK_EFFECT_BINDING_AFFINE_GUARD;
    }

    return ZR_TASK_EFFECT_BINDING_NONE;
}

static TZrBool task_effects_binding_kind_propagates_through_assignment(EZrTaskEffectBindingKind bindingKind) {
    return bindingKind == ZR_TASK_EFFECT_BINDING_BORROWED ||
           bindingKind == ZR_TASK_EFFECT_BINDING_LOANED ||
           task_effects_binding_kind_is_affine_guard(bindingKind) ||
           task_effects_binding_kind_is_plugin_guard(bindingKind);
}

static EZrTaskEffectBindingKind task_effects_merge_value_binding_kind(EZrTaskEffectBindingKind current,
                                                                      EZrTaskEffectBindingKind candidate) {
    if (!task_effects_binding_kind_propagates_through_assignment(candidate)) {
        return current;
    }

    if (!task_effects_binding_kind_propagates_through_assignment(current)) {
        return candidate;
    }

    return current;
}

static EZrTaskEffectBindingKind task_effects_infer_binding_kind(const ZrTaskEffectContext *context,
                                                                SZrAstNode *node);

static EZrTaskEffectBindingKind task_effects_infer_node_array_binding_kind(const ZrTaskEffectContext *context,
                                                                           SZrAstNodeArray *nodes) {
    EZrTaskEffectBindingKind resultKind = ZR_TASK_EFFECT_BINDING_NONE;

    if (context == ZR_NULL || nodes == ZR_NULL) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        resultKind = task_effects_merge_value_binding_kind(
                resultKind,
                task_effects_infer_binding_kind(context, nodes->nodes[index]));
    }

    return resultKind;
}

static EZrTaskEffectBindingKind task_effects_infer_block_expression_binding_kind(const ZrTaskEffectContext *context,
                                                                                 SZrAstNode *node) {
    SZrAstNodeArray *body;
    SZrAstNode *lastStatement;

    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        node->data.block.body == ZR_NULL || node->data.block.body->count == 0) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    body = node->data.block.body;
    lastStatement = body->nodes[body->count - 1];
    if (lastStatement == ZR_NULL || lastStatement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    return task_effects_infer_binding_kind(context, lastStatement->data.expressionStatement.expr);
}

static EZrTaskEffectBindingKind task_effects_infer_binding_kind(const ZrTaskEffectContext *context, SZrAstNode *node) {
    const ZrTaskEffectBinding *binding;

    if (context == ZR_NULL || node == ZR_NULL) {
        return ZR_TASK_EFFECT_BINDING_NONE;
    }

    switch (node->type) {
        case ZR_AST_IMPORT_EXPRESSION:
            return task_effects_is_zr_thread_import(node) ? ZR_TASK_EFFECT_BINDING_THREAD_MODULE
                                                          : ZR_TASK_EFFECT_BINDING_NONE;
        case ZR_AST_IDENTIFIER_LITERAL:
            binding = task_effects_find_binding(context, node->data.identifier.name);
            return binding != ZR_NULL ? binding->bindingKind : ZR_TASK_EFFECT_BINDING_NONE;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return task_effects_infer_construct_binding_kind(context, node);
        case ZR_AST_PRIMARY_EXPRESSION:
            return task_effects_infer_primary_binding_kind(context, node);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return task_effects_infer_binding_kind(context, node->data.typeCastExpression.expression);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return task_effects_infer_binding_kind(context, node->data.typeQueryExpression.operand);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return task_effects_infer_binding_kind(context, node->data.prototypeReferenceExpression.target);
        case ZR_AST_EXPRESSION_STATEMENT:
            return task_effects_infer_binding_kind(context, node->data.expressionStatement.expr);
        case ZR_AST_BLOCK:
            return task_effects_infer_block_expression_binding_kind(context, node);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (node->data.assignmentExpression.op.op != ZR_NULL &&
                strcmp(node->data.assignmentExpression.op.op, "=") == 0) {
                return task_effects_infer_binding_kind(context, node->data.assignmentExpression.right);
            }
            return ZR_TASK_EFFECT_BINDING_NONE;
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            EZrTaskEffectBindingKind resultKind = ZR_TASK_EFFECT_BINDING_NONE;
            resultKind = task_effects_merge_value_binding_kind(
                    resultKind,
                    task_effects_infer_binding_kind(context, node->data.conditionalExpression.consequent));
            resultKind = task_effects_merge_value_binding_kind(
                    resultKind,
                    task_effects_infer_binding_kind(context, node->data.conditionalExpression.alternate));
            return resultKind;
        }
        case ZR_AST_LOGICAL_EXPRESSION: {
            EZrTaskEffectBindingKind resultKind = ZR_TASK_EFFECT_BINDING_NONE;
            resultKind = task_effects_merge_value_binding_kind(
                    resultKind,
                    task_effects_infer_binding_kind(context, node->data.logicalExpression.left));
            resultKind = task_effects_merge_value_binding_kind(
                    resultKind,
                    task_effects_infer_binding_kind(context, node->data.logicalExpression.right));
            return resultKind;
        }
        case ZR_AST_ARRAY_LITERAL:
            return task_effects_infer_node_array_binding_kind(context, node->data.arrayLiteral.elements);
        case ZR_AST_OBJECT_LITERAL:
            return task_effects_infer_node_array_binding_kind(context, node->data.objectLiteral.properties);
        case ZR_AST_KEY_VALUE_PAIR: {
            EZrTaskEffectBindingKind resultKind = ZR_TASK_EFFECT_BINDING_NONE;
            if (node->data.keyValuePair.keyIsComputed) {
                resultKind = task_effects_merge_value_binding_kind(
                        resultKind,
                        task_effects_infer_binding_kind(context, node->data.keyValuePair.key));
            }
            resultKind = task_effects_merge_value_binding_kind(
                    resultKind,
                    task_effects_infer_binding_kind(context, node->data.keyValuePair.value));
            return resultKind;
        }
        case ZR_AST_UNPACK_LITERAL:
            return task_effects_infer_binding_kind(context, node->data.unpackLiteral.element);
        default:
            return ZR_TASK_EFFECT_BINDING_NONE;
    }
}

static void task_effects_infer_assignment_source_effect(const ZrTaskEffectContext *context,
                                                        SZrAstNode *source,
                                                        TZrBool *isBorrowed,
                                                        EZrTaskEffectBindingKind *bindingKind) {
    const ZrTaskEffectBinding *sourceBinding;
    SZrString *sourceName;

    if (isBorrowed != ZR_NULL) {
        *isBorrowed = ZR_FALSE;
    }
    if (bindingKind != ZR_NULL) {
        *bindingKind = ZR_TASK_EFFECT_BINDING_NONE;
    }

    if (context == ZR_NULL || source == ZR_NULL) {
        return;
    }

    if (bindingKind != ZR_NULL) {
        *bindingKind = task_effects_infer_binding_kind(context, source);
    }

    sourceName = task_effects_bare_identifier_name(source);
    sourceBinding = task_effects_find_binding(context, sourceName);
    if (sourceBinding == ZR_NULL) {
        return;
    }

    if (isBorrowed != ZR_NULL) {
        *isBorrowed = (TZrBool)(sourceBinding->isBorrowed ||
                                sourceBinding->bindingKind == ZR_TASK_EFFECT_BINDING_BORROWED);
    }

    if (bindingKind != ZR_NULL &&
        task_effects_binding_kind_propagates_through_assignment(sourceBinding->bindingKind)) {
        *bindingKind = sourceBinding->bindingKind;
    }
}

static void task_effects_register_assignment_binding(ZrTaskEffectContext *context, SZrAstNode *node) {
    ZrTaskEffectBinding *targetBinding;
    SZrString *targetName;
    EZrTaskEffectBindingKind bindingKind = ZR_TASK_EFFECT_BINDING_NONE;
    TZrBool isBorrowed = ZR_FALSE;

    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return;
    }

    task_effects_infer_assignment_source_effect(context,
                                                node->data.assignmentExpression.right,
                                                &isBorrowed,
                                                &bindingKind);
    if (!isBorrowed && !task_effects_binding_kind_propagates_through_assignment(bindingKind)) {
        return;
    }

    targetName = task_effects_bare_identifier_name(node->data.assignmentExpression.left);
    targetBinding = task_effects_find_mutable_binding(context, targetName);
    if (targetBinding == ZR_NULL) {
        return;
    }

    targetBinding->isBorrowed = (TZrBool)(targetBinding->isBorrowed ||
                                          isBorrowed ||
                                          bindingKind == ZR_TASK_EFFECT_BINDING_BORROWED);
    if (task_effects_binding_kind_propagates_through_assignment(bindingKind)) {
        targetBinding->bindingKind = bindingKind;
    }
}

static TZrBool task_effects_primary_is_await_call(SZrAstNode *node, TZrSize *callMemberIndex) {
    SZrPrimaryExpression *primary;
    SZrAstNode *memberNode;

    if (callMemberIndex != ZR_NULL) {
        *callMemberIndex = 0;
    }

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || !task_effects_is_zr_task_import(node->data.primaryExpression.property)) {
        return ZR_FALSE;
    }

    primary = &node->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count < 2) {
        return ZR_FALSE;
    }

    memberNode = primary->members->nodes[0];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION || memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        !task_effects_identifier_equals(&memberNode->data.memberExpression.property->data.identifier, "__awaitTask")) {
        return ZR_FALSE;
    }

    memberNode = primary->members->nodes[1];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }

    if (callMemberIndex != ZR_NULL) {
        *callMemberIndex = 1;
    }
    return ZR_TRUE;
}

static void task_effects_validate_member_node(ZrTaskEffectContext *context, SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL || context->cs->hasError) {
        return;
    }

    if (node->type == ZR_AST_MEMBER_EXPRESSION) {
        if (node->data.memberExpression.computed) {
            task_effects_validate_node(context, node->data.memberExpression.property);
        }
        return;
    }

    if (node->type == ZR_AST_FUNCTION_CALL) {
        task_effects_validate_function_call(context, &node->data.functionCall);
        return;
    }

    task_effects_validate_node(context, node);
}

static void task_effects_validate_primary_expression(ZrTaskEffectContext *context, SZrAstNode *node) {
    SZrPrimaryExpression *primary;
    TZrSize memberIndex;
    TZrSize awaitCallIndex = 0;

    if (context == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || context->cs->hasError) {
        return;
    }

    primary = &node->data.primaryExpression;
    if (task_effects_primary_is_await_call(node, &awaitCallIndex)) {
        SZrAstNode *callNode = primary->members->nodes[awaitCallIndex];
        task_effects_validate_function_call(context, &callNode->data.functionCall);
        if (context->cs->hasError) {
            return;
        }
        if (!context->asyncAllowed) {
            ZrParser_Compiler_Error(context->cs,
                                    "%await is only allowed inside %async bodies or scheduler-managed top-level coroutines",
                                    node->location);
            return;
        }
        context->awaitSeen = ZR_TRUE;
        for (memberIndex = awaitCallIndex + 1; memberIndex < primary->members->count && !context->cs->hasError;
             memberIndex++) {
            task_effects_validate_member_node(context, primary->members->nodes[memberIndex]);
        }
        return;
    }

    task_effects_validate_node(context, primary->property);
    for (memberIndex = 0; memberIndex < primary->members->count && !context->cs->hasError; memberIndex++) {
        task_effects_validate_member_node(context, primary->members->nodes[memberIndex]);
    }
}

void task_effects_validate_function_like(ZrTaskEffectContext *parentContext,
                                         TZrBool asyncAllowed,
                                         SZrAstNodeArray *params,
                                         SZrParameter *args,
                                         SZrAstNode *body) {
    ZrTaskEffectContext childContext;

    if (parentContext == ZR_NULL || body == ZR_NULL || parentContext->cs == ZR_NULL) {
        return;
    }

    if (!task_effects_context_init(&childContext, parentContext->cs, asyncAllowed, parentContext)) {
        ZrParser_Compiler_Error(parentContext->cs, "Failed to initialize task effect validation context", body->location);
        return;
    }

    task_effects_register_parameter_list(&childContext, params);
    task_effects_register_vararg_parameter(&childContext, args);
    task_effects_validate_node(&childContext, body);
    task_effects_context_free(&childContext);
}

void task_effects_validate_node(ZrTaskEffectContext *context, SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL || context->cs == ZR_NULL || context->cs->hasError) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            task_effects_validate_node_array(context, node->data.script.statements);
            break;
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_ENUM_DECLARATION:
            task_effects_validate_declaration(context, node);
            break;
        case ZR_AST_BLOCK:
            task_effects_enter_scope(context);
            task_effects_validate_node_array(context, node->data.block.body);
            task_effects_leave_scope(context);
            break;
        case ZR_AST_FUNCTION_DECLARATION:
            if (node->data.functionDeclaration.name != ZR_NULL) {
                task_effects_push_binding(context,
                                          node->data.functionDeclaration.name->name,
                                          ZR_FALSE,
                                          ZR_TASK_EFFECT_BINDING_NONE);
            }
            task_effects_validate_decorators(context, node->data.functionDeclaration.decorators);
            task_effects_validate_function_like(context,
                                                node->data.functionDeclaration.isAsync,
                                                node->data.functionDeclaration.params,
                                                node->data.functionDeclaration.args,
                                                node->data.functionDeclaration.body);
            break;
        case ZR_AST_TEST_DECLARATION:
            task_effects_validate_function_like(context, ZR_TRUE, node->data.testDeclaration.params,
                                                node->data.testDeclaration.args, node->data.testDeclaration.body);
            break;
        case ZR_AST_LAMBDA_EXPRESSION:
            task_effects_validate_function_like(context, node->data.lambdaExpression.isAsync,
                                                node->data.lambdaExpression.params, node->data.lambdaExpression.args,
                                                node->data.lambdaExpression.block);
            break;
        case ZR_AST_VARIABLE_DECLARATION:
            {
                EZrTaskEffectBindingKind bindingKind =
                        task_effects_infer_binding_kind(context, node->data.variableDeclaration.value);
                TZrBool typeIsBorrowed = task_effects_type_is_borrowed(node->data.variableDeclaration.typeInfo);
                TZrBool typeIsLoaned = task_effects_type_is_loaned(node->data.variableDeclaration.typeInfo);
                TZrBool isBorrowed = typeIsBorrowed || bindingKind == ZR_TASK_EFFECT_BINDING_BORROWED;

                if (typeIsLoaned) {
                    bindingKind = ZR_TASK_EFFECT_BINDING_LOANED;
                }

                task_effects_validate_node(context, node->data.variableDeclaration.value);
                task_effects_register_pattern_binding(context,
                                                     node->data.variableDeclaration.pattern,
                                                     isBorrowed,
                                                     bindingKind);
            }
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            task_effects_validate_primary_expression(context, node);
            break;
        case ZR_AST_DECORATOR_EXPRESSION:
            task_effects_validate_node(context, node->data.decoratorExpression.expr);
            break;
        case ZR_AST_IDENTIFIER_LITERAL: {
            const ZrTaskEffectBinding *binding = ZR_NULL;

            binding = task_effects_find_binding(context, node->data.identifier.name);
            if (binding == ZR_NULL || (!context->awaitSeen && !binding->inheritedAfterAwait)) {
                break;
            }
            if (binding != ZR_NULL &&
                (binding->isBorrowed || binding->bindingKind == ZR_TASK_EFFECT_BINDING_BORROWED)) {
                task_effects_report_borrow_after_await(context, node->data.identifier.name, node->location);
            } else if (binding != ZR_NULL && binding->bindingKind == ZR_TASK_EFFECT_BINDING_LOANED) {
                task_effects_report_loan_after_await(context, node->data.identifier.name, node->location);
            } else if (binding != ZR_NULL && task_effects_binding_kind_is_affine_guard(binding->bindingKind)) {
                task_effects_report_affine_guard_after_await(context, node->data.identifier.name, node->location);
            } else if (binding != ZR_NULL && task_effects_binding_kind_is_plugin_guard(binding->bindingKind)) {
                task_effects_report_plugin_guard_after_await(context, node->data.identifier.name, node->location);
            }
            break;
        }
        case ZR_AST_FUNCTION_CALL:
            task_effects_validate_function_call(context, &node->data.functionCall);
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            if (node->data.memberExpression.computed) {
                task_effects_validate_node(context, node->data.memberExpression.property);
            }
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            task_effects_validate_node(context, node->data.assignmentExpression.left);
            task_effects_validate_node(context, node->data.assignmentExpression.right);
            if (!context->cs->hasError) {
                task_effects_register_assignment_binding(context, node);
            }
            break;
        case ZR_AST_BINARY_EXPRESSION:
            task_effects_validate_node(context, node->data.binaryExpression.left);
            task_effects_validate_node(context, node->data.binaryExpression.right);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            task_effects_validate_node(context, node->data.logicalExpression.left);
            task_effects_validate_node(context, node->data.logicalExpression.right);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            task_effects_validate_node(context, node->data.conditionalExpression.test);
            task_effects_validate_node(context, node->data.conditionalExpression.consequent);
            task_effects_validate_node(context, node->data.conditionalExpression.alternate);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            task_effects_validate_node(context, node->data.unaryExpression.argument);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            task_effects_validate_node(context, node->data.typeCastExpression.expression);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            task_effects_validate_node(context, node->data.typeQueryExpression.operand);
            break;
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            task_effects_validate_node(context, node->data.prototypeReferenceExpression.target);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            task_effects_validate_node(context, node->data.constructExpression.target);
            task_effects_validate_node_array(context, node->data.constructExpression.args);
            break;
        case ZR_AST_ARRAY_LITERAL:
            task_effects_validate_node_array(context, node->data.arrayLiteral.elements);
            break;
        case ZR_AST_OBJECT_LITERAL:
            task_effects_validate_node_array(context, node->data.objectLiteral.properties);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            if (node->data.keyValuePair.key != ZR_NULL &&
                (node->data.keyValuePair.keyIsComputed ||
                 (node->data.keyValuePair.key->type != ZR_AST_IDENTIFIER_LITERAL &&
                  node->data.keyValuePair.key->type != ZR_AST_STRING_LITERAL))) {
                task_effects_validate_node(context, node->data.keyValuePair.key);
            }
            task_effects_validate_node(context, node->data.keyValuePair.value);
            break;
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            task_effects_validate_node_array(context, node->data.templateStringLiteral.segments);
            break;
        case ZR_AST_INTERPOLATED_SEGMENT:
            task_effects_validate_node(context, node->data.interpolatedSegment.expression);
            break;
        case ZR_AST_GENERATOR_EXPRESSION:
            task_effects_validate_node(context, node->data.generatorExpression.block);
            break;
        case ZR_AST_RETURN_STATEMENT:
            task_effects_validate_node(context, node->data.returnStatement.expr);
            break;
        case ZR_AST_EXPRESSION_STATEMENT:
            task_effects_validate_node(context, node->data.expressionStatement.expr);
            break;
        case ZR_AST_USING_STATEMENT:
            task_effects_validate_node(context, node->data.usingStatement.resource);
            task_effects_enter_scope(context);
            if (node->data.usingStatement.guardKind == ZR_USING_GUARD_PLUGIN ||
                (node->data.usingStatement.guardKind == ZR_USING_GUARD_PATTERN &&
                 node->data.usingStatement.resource != ZR_NULL &&
                 node->data.usingStatement.resource->type == ZR_AST_IMPORT_EXPRESSION)) {
                task_effects_register_using_guard_pattern_binding(context,
                                                                  node->data.usingStatement.pattern,
                                                                  ZR_TASK_EFFECT_BINDING_PLUGIN_GUARD);
            }
            task_effects_validate_node(context, node->data.usingStatement.body);
            task_effects_leave_scope(context);
            task_effects_validate_node(context, node->data.usingStatement.elseBody);
            break;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            task_effects_validate_node(context, node->data.breakContinueStatement.expr);
            break;
        case ZR_AST_THROW_STATEMENT:
            task_effects_validate_node(context, node->data.throwStatement.expr);
            break;
        case ZR_AST_OUT_STATEMENT:
            task_effects_validate_node(context, node->data.outStatement.expr);
            break;
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            task_effects_validate_node(context, node->data.tryCatchFinallyStatement.block);
            task_effects_validate_node_array(context, node->data.tryCatchFinallyStatement.catchClauses);
            task_effects_validate_node(context, node->data.tryCatchFinallyStatement.finallyBlock);
            break;
        case ZR_AST_CATCH_CLAUSE:
            task_effects_enter_scope(context);
            task_effects_validate_node_array(context, node->data.catchClause.pattern);
            task_effects_validate_node(context, node->data.catchClause.block);
            task_effects_leave_scope(context);
            break;
        case ZR_AST_IF_EXPRESSION:
            task_effects_validate_node(context, node->data.ifExpression.condition);
            task_effects_validate_node(context, node->data.ifExpression.thenExpr);
            task_effects_validate_node(context, node->data.ifExpression.elseExpr);
            break;
        case ZR_AST_SWITCH_EXPRESSION:
            task_effects_validate_node(context, node->data.switchExpression.expr);
            task_effects_validate_node_array(context, node->data.switchExpression.cases);
            task_effects_validate_node(context, node->data.switchExpression.defaultCase);
            break;
        case ZR_AST_SWITCH_CASE:
            task_effects_validate_node(context, node->data.switchCase.value);
            task_effects_validate_node(context, node->data.switchCase.block);
            break;
        case ZR_AST_SWITCH_DEFAULT:
            task_effects_validate_node(context, node->data.switchDefault.block);
            break;
        case ZR_AST_WHILE_LOOP:
            task_effects_validate_node(context, node->data.whileLoop.cond);
            task_effects_validate_node(context, node->data.whileLoop.block);
            break;
        case ZR_AST_FOR_LOOP:
            task_effects_validate_node(context, node->data.forLoop.init);
            task_effects_validate_node(context, node->data.forLoop.cond);
            task_effects_validate_node(context, node->data.forLoop.step);
            task_effects_validate_node(context, node->data.forLoop.block);
            break;
        case ZR_AST_FOREACH_LOOP:
            task_effects_validate_node(context, node->data.foreachLoop.expr);
            task_effects_enter_scope(context);
            task_effects_register_pattern_binding(context,
                                                 node->data.foreachLoop.pattern,
                                                 task_effects_type_is_borrowed(node->data.foreachLoop.typeInfo),
                                                 ZR_TASK_EFFECT_BINDING_NONE);
            task_effects_validate_node(context, node->data.foreachLoop.block);
            task_effects_leave_scope(context);
            break;
        default:
            break;
    }
}

ZR_PARSER_API TZrBool compiler_validate_task_effects(SZrCompilerState *cs, SZrAstNode *node) {
    ZrTaskEffectContext rootContext;
    TZrBool succeeded;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    succeeded = task_effects_context_init(&rootContext, cs, ZR_TRUE, ZR_NULL);
    if (!succeeded) {
        ZrParser_Compiler_Error(cs, "Failed to initialize task effect validation context", node->location);
        return ZR_FALSE;
    }

    task_effects_validate_node(&rootContext, node);
    task_effects_context_free(&rootContext);
    return !cs->hasError;
}
