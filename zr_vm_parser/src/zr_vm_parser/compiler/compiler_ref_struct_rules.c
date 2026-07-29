#include "compiler_internal.h"

#include <string.h>

typedef enum EZrRefStructTypeUse {
    ZR_REF_STRUCT_TYPE_USE_GENERAL = 0,
    ZR_REF_STRUCT_TYPE_USE_CLASS_FIELD,
    ZR_REF_STRUCT_TYPE_USE_RESOURCE_FIELD,
    ZR_REF_STRUCT_TYPE_USE_PLAIN_STRUCT_FIELD,
    ZR_REF_STRUCT_TYPE_USE_REF_STRUCT_FIELD,
    ZR_REF_STRUCT_TYPE_USE_GLOBAL,
    ZR_REF_STRUCT_TYPE_USE_NATIVE
} EZrRefStructTypeUse;

typedef struct SZrRefStructRuleContext {
    SZrCompilerState *compiler;
    SZrRefStructTypeSet typeSet;
} SZrRefStructRuleContext;

static TZrBool ref_struct_string_equals(
        SZrString *left,
        SZrString *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           ZrCore_String_Equal(left, right);
}

static TZrBool ref_struct_name_array_contains(
        const SZrArray *names,
        SZrString *name) {
    TZrSize index;

    if (names == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < names->length; index++) {
        SZrString *const *candidate =
                (SZrString *const *)ZrCore_Array_Get((SZrArray *)names, index);
        if (candidate != ZR_NULL &&
            ref_struct_string_equals(*candidate, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void ref_struct_collect_type_declarations(
        SZrRefStructTypeSet *typeSet,
        SZrState *state,
        SZrAstNode *node);

static void ref_struct_collect_node_array(
        SZrRefStructTypeSet *typeSet,
        SZrState *state,
        SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return;
    }
    for (index = 0U; index < nodes->count; index++) {
        ref_struct_collect_type_declarations(
                typeSet, state, nodes->nodes[index]);
    }
}

static void ref_struct_collect_type_declarations(
        SZrRefStructTypeSet *typeSet,
        SZrState *state,
        SZrAstNode *node) {
    SZrString *name;

    if (typeSet == ZR_NULL || state == ZR_NULL || node == ZR_NULL) {
        return;
    }
    switch (node->type) {
        case ZR_AST_SCRIPT:
            ref_struct_collect_node_array(
                    typeSet, state, node->data.script.statements);
            break;
        case ZR_AST_BLOCK:
            ref_struct_collect_node_array(
                    typeSet, state, node->data.block.body);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            name = node->data.structDeclaration.name != ZR_NULL
                           ? node->data.structDeclaration.name->name
                           : ZR_NULL;
            if (node->data.structDeclaration.isRefLike && name != ZR_NULL &&
                !ref_struct_name_array_contains(
                        &typeSet->refLikeTypeNames, name)) {
                ZrCore_Array_Push(state, &typeSet->refLikeTypeNames, &name);
            }
            ref_struct_collect_node_array(
                    typeSet, state, node->data.structDeclaration.members);
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            name = node->data.interfaceDeclaration.name != ZR_NULL
                           ? node->data.interfaceDeclaration.name->name
                           : ZR_NULL;
            if (name != ZR_NULL &&
                !ref_struct_name_array_contains(
                        &typeSet->interfaceTypeNames, name)) {
                ZrCore_Array_Push(state, &typeSet->interfaceTypeNames, &name);
            }
            ref_struct_collect_node_array(
                    typeSet, state, node->data.interfaceDeclaration.members);
            break;
        case ZR_AST_CLASS_DECLARATION:
            ref_struct_collect_node_array(
                    typeSet, state, node->data.classDeclaration.members);
            break;
        case ZR_AST_FUNCTION_DECLARATION:
            ref_struct_collect_type_declarations(
                    typeSet, state, node->data.functionDeclaration.body);
            break;
        case ZR_AST_CLASS_METHOD:
            ref_struct_collect_type_declarations(
                    typeSet, state, node->data.classMethod.body);
            break;
        case ZR_AST_STRUCT_METHOD:
            ref_struct_collect_type_declarations(
                    typeSet, state, node->data.structMethod.body);
            break;
        case ZR_AST_EXTERN_BLOCK:
            ref_struct_collect_node_array(
                    typeSet, state, node->data.externBlock.declarations);
            break;
        default:
            break;
    }
}

TZrBool compiler_ref_struct_type_set_init(
        SZrRefStructTypeSet *typeSet,
        SZrState *state,
        SZrAstNode *root) {
    if (typeSet == ZR_NULL || state == ZR_NULL || root == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(typeSet, 0, sizeof(*typeSet));
    ZrCore_Array_Init(
            state,
            &typeSet->refLikeTypeNames,
            sizeof(SZrString *),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(
            state,
            &typeSet->interfaceTypeNames,
            sizeof(SZrString *),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
    ref_struct_collect_type_declarations(typeSet, state, root);
    return ZR_TRUE;
}

void compiler_ref_struct_type_set_free(
        SZrRefStructTypeSet *typeSet,
        SZrState *state) {
    if (typeSet == ZR_NULL || state == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(state, &typeSet->refLikeTypeNames);
    ZrCore_Array_Free(state, &typeSet->interfaceTypeNames);
    memset(typeSet, 0, sizeof(*typeSet));
}

static SZrString *ref_struct_type_segment_name(const SZrType *type) {
    if (type == ZR_NULL || type->name == ZR_NULL) {
        return ZR_NULL;
    }
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return type->name->data.identifier.name;
    }
    if (type->name->type == ZR_AST_GENERIC_TYPE &&
        type->name->data.genericType.name != ZR_NULL) {
        return type->name->data.genericType.name->name;
    }
    return ZR_NULL;
}

TZrBool compiler_ref_struct_type_is_ref_like(
        const SZrRefStructTypeSet *typeSet,
        const SZrType *type) {
    const SZrType *segment;

    if (typeSet == ZR_NULL) {
        return ZR_FALSE;
    }
    for (segment = type; segment != ZR_NULL; segment = segment->subType) {
        if (ref_struct_name_array_contains(
                    &typeSet->refLikeTypeNames,
                    ref_struct_type_segment_name(segment))) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool ref_struct_type_name_is_literal(
        const SZrType *type,
        const TZrChar *literal) {
    SZrString *name = ref_struct_type_segment_name(type);
    TZrNativeString nativeName;

    if (name == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }
    nativeName = ZrCore_String_GetNativeString(name);
    return nativeName != ZR_NULL && strcmp(nativeName, literal) == 0;
}

TZrBool compiler_ref_struct_type_is_boxing_target(
        const SZrRefStructTypeSet *typeSet,
        const SZrType *type) {
    const SZrType *segment;

    if (type == ZR_NULL) {
        return ZR_FALSE;
    }
    for (segment = type; segment != ZR_NULL; segment = segment->subType) {
        SZrString *name = ref_struct_type_segment_name(segment);
        if (ref_struct_type_name_is_literal(segment, "object") ||
            ref_struct_type_name_is_literal(segment, "dynamic") ||
            (typeSet != ZR_NULL &&
             ref_struct_name_array_contains(
                     &typeSet->interfaceTypeNames, name))) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static SZrFileRange ref_struct_type_range(
        const SZrType *type,
        SZrFileRange fallback) {
    return type != ZR_NULL && type->name != ZR_NULL
                   ? type->name->location
                   : fallback;
}

static TZrBool ref_struct_report(
        SZrRefStructRuleContext *context,
        const TZrChar *message,
        SZrFileRange range) {
    SZrStructuredDiagnostic diagnostic;

    if (context == ZR_NULL || context->compiler == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (ZrParser_DiagnosticBuilder_Build(
                context->compiler->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                range,
                "ref_struct_restriction",
                message,
                "Ref-like values are stack/region bound and cannot enter this storage boundary",
                "Keep the value in a local/ref-like aggregate or shorten its lifetime")) {
        ZrParser_Compiler_StructuredError(context->compiler, &diagnostic);
    } else {
        ZrParser_Compiler_Error(context->compiler, message, range);
    }
    return ZR_FALSE;
}

static TZrBool ref_struct_validate_type(
        SZrRefStructRuleContext *context,
        const SZrType *type,
        EZrRefStructTypeUse use,
        SZrFileRange fallback) {
    const SZrType *segment;
    TZrBool isRefLike;

    if (type == ZR_NULL) {
        return ZR_TRUE;
    }
    isRefLike = compiler_ref_struct_type_is_ref_like(&context->typeSet, type);
    if (type->dimensions > 0 && isRefLike) {
        return ref_struct_report(
                context,
                "A ref struct cannot be an array element",
                ref_struct_type_range(type, fallback));
    }
    for (segment = type; segment != ZR_NULL; segment = segment->subType) {
        if (segment->name != ZR_NULL &&
            segment->name->type == ZR_AST_GENERIC_TYPE &&
            segment->name->data.genericType.params != ZR_NULL) {
            TZrSize index;
            for (index = 0U;
                 index < segment->name->data.genericType.params->count;
                 index++) {
                SZrAstNode *argument =
                        segment->name->data.genericType.params->nodes[index];
                if (argument != ZR_NULL && argument->type == ZR_AST_TYPE) {
                    if (compiler_ref_struct_type_is_ref_like(
                                &context->typeSet, &argument->data.type)) {
                        return ref_struct_report(
                                context,
                                "A ref struct cannot be used as an unconstrained generic argument",
                                argument->location);
                    }
                    if (!ref_struct_validate_type(
                                context,
                                &argument->data.type,
                                ZR_REF_STRUCT_TYPE_USE_GENERAL,
                                argument->location)) {
                        return ZR_FALSE;
                    }
                }
            }
        }
    }
    if (use == ZR_REF_STRUCT_TYPE_USE_NATIVE && isRefLike) {
        return ref_struct_report(
                context,
                "A ref struct cannot cross a native opaque ABI boundary",
                ref_struct_type_range(type, fallback));
    }
    if (use == ZR_REF_STRUCT_TYPE_USE_GLOBAL && isRefLike) {
        return ref_struct_report(
                context,
                "A ref struct cannot be stored in module/global storage",
                ref_struct_type_range(type, fallback));
    }
    if (use == ZR_REF_STRUCT_TYPE_USE_CLASS_FIELD && isRefLike) {
        return ref_struct_report(
                context,
                "A ref struct cannot be stored in a class field",
                ref_struct_type_range(type, fallback));
    }
    if (use == ZR_REF_STRUCT_TYPE_USE_RESOURCE_FIELD && isRefLike) {
        return ref_struct_report(
                context,
                "A ref struct cannot be stored in a resource class field",
                ref_struct_type_range(type, fallback));
    }
    if (use == ZR_REF_STRUCT_TYPE_USE_PLAIN_STRUCT_FIELD) {
        if (type->referenceAccess != ZR_REFERENCE_ACCESS_NONE) {
            return ref_struct_report(
                    context,
                    "Only a ref struct may contain a ref field",
                    ref_struct_type_range(type, fallback));
        }
        if (isRefLike) {
            return ref_struct_report(
                    context,
                    "Only a ref struct may contain a ref-like field",
                    ref_struct_type_range(type, fallback));
        }
    }
    if (use == ZR_REF_STRUCT_TYPE_USE_REF_STRUCT_FIELD &&
        type->dimensions > 0 &&
        (isRefLike || type->referenceAccess != ZR_REFERENCE_ACCESS_NONE)) {
        return ref_struct_report(
                context,
                "A ref struct cannot be an array element",
                ref_struct_type_range(type, fallback));
    }
    return ZR_TRUE;
}

static TZrBool ref_struct_validate_node(
        SZrRefStructRuleContext *context,
        SZrAstNode *node,
        TZrBool isModuleScope);

static TZrBool ref_struct_validate_node_array(
        SZrRefStructRuleContext *context,
        SZrAstNodeArray *nodes,
        TZrBool isModuleScope) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < nodes->count; index++) {
        if (!ref_struct_validate_node(
                    context, nodes->nodes[index], isModuleScope)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool ref_struct_validate_parameters(
        SZrRefStructRuleContext *context,
        SZrAstNodeArray *parameters,
        SZrParameter *vararg,
        EZrRefStructTypeUse use) {
    TZrSize index;

    if (parameters != ZR_NULL) {
        for (index = 0U; index < parameters->count; index++) {
            SZrAstNode *parameterNode = parameters->nodes[index];
            if (parameterNode != ZR_NULL &&
                parameterNode->type == ZR_AST_PARAMETER &&
                !ref_struct_validate_type(
                        context,
                        parameterNode->data.parameter.typeInfo,
                        use,
                        parameterNode->location)) {
                return ZR_FALSE;
            }
        }
    }
    return vararg == ZR_NULL ||
           ref_struct_validate_type(
                   context, vararg->typeInfo, use, vararg->nameLocation);
}

static TZrBool ref_struct_validate_callable(
        SZrRefStructRuleContext *context,
        SZrAstNodeArray *parameters,
        SZrParameter *vararg,
        SZrType *returnType,
        SZrAstNode *body,
        EZrRefStructTypeUse use,
        SZrFileRange range) {
    return ref_struct_validate_parameters(
                   context, parameters, vararg, use) &&
           ref_struct_validate_type(
                   context, returnType, use, range) &&
           ref_struct_validate_node(context, body, ZR_FALSE);
}

static TZrBool ref_struct_validate_class(
        SZrRefStructRuleContext *context,
        SZrAstNode *node) {
    SZrClassDeclaration *declaration = &node->data.classDeclaration;
    TZrSize index;

    for (index = 0U;
         declaration->members != ZR_NULL &&
         index < declaration->members->count;
         index++) {
        SZrAstNode *member = declaration->members->nodes[index];
        if (member == ZR_NULL) {
            continue;
        }
        if (member->type == ZR_AST_CLASS_FIELD) {
            EZrRefStructTypeUse use = member->data.classField.isStatic
                                              ? ZR_REF_STRUCT_TYPE_USE_GLOBAL
                                              : declaration->isOwned
                                                        ? ZR_REF_STRUCT_TYPE_USE_RESOURCE_FIELD
                                                        : ZR_REF_STRUCT_TYPE_USE_CLASS_FIELD;
            if (!ref_struct_validate_type(
                        context,
                        member->data.classField.typeInfo,
                        use,
                        member->location)) {
                return ZR_FALSE;
            }
        } else if (member->type == ZR_AST_CLASS_METHOD) {
            if (!ref_struct_validate_callable(
                        context,
                        member->data.classMethod.params,
                        member->data.classMethod.args,
                        member->data.classMethod.returnType,
                        member->data.classMethod.body,
                        ZR_REF_STRUCT_TYPE_USE_GENERAL,
                        member->location)) {
                return ZR_FALSE;
            }
        } else if (member->type == ZR_AST_CLASS_META_FUNCTION) {
            if (!ref_struct_validate_callable(
                        context,
                        member->data.classMetaFunction.params,
                        member->data.classMetaFunction.args,
                        member->data.classMetaFunction.returnType,
                        member->data.classMetaFunction.body,
                        ZR_REF_STRUCT_TYPE_USE_GENERAL,
                        member->location)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool ref_struct_validate_struct(
        SZrRefStructRuleContext *context,
        SZrAstNode *node) {
    SZrStructDeclaration *declaration = &node->data.structDeclaration;
    TZrSize index;

    for (index = 0U;
         declaration->members != ZR_NULL &&
         index < declaration->members->count;
         index++) {
        SZrAstNode *member = declaration->members->nodes[index];
        if (member == ZR_NULL) {
            continue;
        }
        if (member->type == ZR_AST_STRUCT_FIELD) {
            EZrRefStructTypeUse use = declaration->isRefLike
                                              ? ZR_REF_STRUCT_TYPE_USE_REF_STRUCT_FIELD
                                              : ZR_REF_STRUCT_TYPE_USE_PLAIN_STRUCT_FIELD;
            if (member->data.structField.isStatic) {
                use = ZR_REF_STRUCT_TYPE_USE_GLOBAL;
            }
            if (!ref_struct_validate_type(
                        context,
                        member->data.structField.typeInfo,
                        use,
                        member->location)) {
                return ZR_FALSE;
            }
        } else if (member->type == ZR_AST_STRUCT_METHOD) {
            if (!ref_struct_validate_callable(
                        context,
                        member->data.structMethod.params,
                        member->data.structMethod.args,
                        member->data.structMethod.returnType,
                        member->data.structMethod.body,
                        ZR_REF_STRUCT_TYPE_USE_GENERAL,
                        member->location)) {
                return ZR_FALSE;
            }
        } else if (member->type == ZR_AST_STRUCT_META_FUNCTION) {
            if (!ref_struct_validate_callable(
                        context,
                        member->data.structMetaFunction.params,
                        member->data.structMetaFunction.args,
                        member->data.structMetaFunction.returnType,
                        member->data.structMetaFunction.body,
                        ZR_REF_STRUCT_TYPE_USE_GENERAL,
                        member->location)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool ref_struct_validate_extern(
        SZrRefStructRuleContext *context,
        SZrAstNode *node) {
    if (node->type == ZR_AST_EXTERN_FUNCTION_DECLARATION) {
        return ref_struct_validate_parameters(
                       context,
                       node->data.externFunctionDeclaration.params,
                       node->data.externFunctionDeclaration.args,
                       ZR_REF_STRUCT_TYPE_USE_NATIVE) &&
               ref_struct_validate_type(
                       context,
                       node->data.externFunctionDeclaration.returnType,
                       ZR_REF_STRUCT_TYPE_USE_NATIVE,
                       node->location);
    }
    if (node->type == ZR_AST_EXTERN_DELEGATE_DECLARATION) {
        return ref_struct_validate_parameters(
                       context,
                       node->data.externDelegateDeclaration.params,
                       node->data.externDelegateDeclaration.args,
                       ZR_REF_STRUCT_TYPE_USE_NATIVE) &&
               ref_struct_validate_type(
                       context,
                       node->data.externDelegateDeclaration.returnType,
                       ZR_REF_STRUCT_TYPE_USE_NATIVE,
                       node->location);
    }
    return ref_struct_validate_node(context, node, ZR_FALSE);
}

static TZrBool ref_struct_validate_node(
        SZrRefStructRuleContext *context,
        SZrAstNode *node,
        TZrBool isModuleScope) {
    TZrSize index;

    if (node == ZR_NULL || context->compiler->hasError) {
        return node == ZR_NULL || !context->compiler->hasError;
    }
    switch (node->type) {
        case ZR_AST_SCRIPT:
            return ref_struct_validate_node_array(
                    context, node->data.script.statements, ZR_TRUE);
        case ZR_AST_COMPILE_TIME_DECLARATION: {
            SZrAstNode *selectedBranch =
                    node->data.compileTimeDeclaration.selectedBranch;
            if (selectedBranch == ZR_NULL) {
                return ZR_TRUE;
            }
            return selectedBranch->type == ZR_AST_BLOCK
                           ? ref_struct_validate_node_array(
                                     context,
                                     selectedBranch->data.block.body,
                                     isModuleScope)
                           : ref_struct_validate_node(
                                     context, selectedBranch, isModuleScope);
        }
        case ZR_AST_BLOCK:
            return ref_struct_validate_node_array(
                    context, node->data.block.body, ZR_FALSE);
        case ZR_AST_VARIABLE_DECLARATION:
            return ref_struct_validate_type(
                    context,
                    node->data.variableDeclaration.typeInfo,
                    isModuleScope
                            ? ZR_REF_STRUCT_TYPE_USE_GLOBAL
                            : ZR_REF_STRUCT_TYPE_USE_GENERAL,
                    node->location);
        case ZR_AST_FUNCTION_DECLARATION:
            return ref_struct_validate_callable(
                    context,
                    node->data.functionDeclaration.params,
                    node->data.functionDeclaration.args,
                    node->data.functionDeclaration.returnType,
                    node->data.functionDeclaration.body,
                    ZR_REF_STRUCT_TYPE_USE_GENERAL,
                    node->location);
        case ZR_AST_CLASS_DECLARATION:
            return ref_struct_validate_class(context, node);
        case ZR_AST_STRUCT_DECLARATION:
            return ref_struct_validate_struct(context, node);
        case ZR_AST_EXTERN_BLOCK:
            for (index = 0U;
                 node->data.externBlock.declarations != ZR_NULL &&
                 index < node->data.externBlock.declarations->count;
                 index++) {
                if (!ref_struct_validate_extern(
                            context,
                            node->data.externBlock.declarations->nodes[index])) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_FOREACH_LOOP:
            return ref_struct_validate_type(
                           context,
                           node->data.foreachLoop.typeInfo,
                           ZR_REF_STRUCT_TYPE_USE_GENERAL,
                           node->location) &&
                   ref_struct_validate_node(
                           context, node->data.foreachLoop.block, ZR_FALSE);
        default:
            return ZR_TRUE;
    }
}

ZR_PARSER_API TZrBool compiler_validate_ref_struct_rules(
        SZrCompilerState *compiler,
        SZrAstNode *node) {
    SZrRefStructRuleContext context;
    TZrBool success;

    if (compiler == ZR_NULL || node == ZR_NULL || compiler->state == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&context, 0, sizeof(context));
    context.compiler = compiler;
    if (!compiler_ref_struct_type_set_init(
                &context.typeSet, compiler->state, node)) {
        return ZR_FALSE;
    }
    success = ref_struct_validate_node(&context, node, ZR_TRUE);
    compiler_ref_struct_type_set_free(&context.typeSet, compiler->state);
    return (TZrBool)(success && !compiler->hasError);
}
