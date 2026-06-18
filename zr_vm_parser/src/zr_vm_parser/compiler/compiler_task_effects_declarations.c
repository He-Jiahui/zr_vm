#include "compiler_task_effects_internal.h"

void task_effects_validate_decorators(ZrTaskEffectContext *context, SZrAstNodeArray *decorators) {
    TZrSize index;

    if (context == ZR_NULL || decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        SZrAstNode *decorator = decorators->nodes[index];
        if (decorator != ZR_NULL && decorator->type == ZR_AST_DECORATOR_EXPRESSION) {
            task_effects_validate_node(context, decorator->data.decoratorExpression.expr);
        } else {
            task_effects_validate_node(context, decorator);
        }
    }
}

static void task_effects_validate_class_member(ZrTaskEffectContext *context, SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_CLASS_FIELD:
            task_effects_validate_decorators(context, node->data.classField.decorators);
            task_effects_validate_node(context, node->data.classField.init);
            break;
        case ZR_AST_CLASS_METHOD:
            task_effects_validate_decorators(context, node->data.classMethod.decorators);
            task_effects_validate_function_like(context,
                                                ZR_FALSE,
                                                node->data.classMethod.params,
                                                node->data.classMethod.args,
                                                node->data.classMethod.body);
            break;
        case ZR_AST_CLASS_META_FUNCTION:
            task_effects_validate_node_array(context, node->data.classMetaFunction.superArgs);
            task_effects_validate_function_like(context,
                                                ZR_FALSE,
                                                node->data.classMetaFunction.params,
                                                node->data.classMetaFunction.args,
                                                node->data.classMetaFunction.body);
            break;
        case ZR_AST_CLASS_PROPERTY:
            task_effects_validate_decorators(context, node->data.classProperty.decorators);
            task_effects_validate_node(context, node->data.classProperty.modifier);
            break;
        case ZR_AST_PROPERTY_GET:
            task_effects_validate_function_like(context,
                                                ZR_FALSE,
                                                ZR_NULL,
                                                ZR_NULL,
                                                node->data.propertyGet.body);
            break;
        case ZR_AST_PROPERTY_SET:
            task_effects_validate_function_like(context,
                                                ZR_FALSE,
                                                ZR_NULL,
                                                ZR_NULL,
                                                node->data.propertySet.body);
            break;
        default:
            break;
    }
}

static void task_effects_validate_struct_member(ZrTaskEffectContext *context, SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_STRUCT_FIELD:
            task_effects_validate_decorators(context, node->data.structField.decorators);
            task_effects_validate_node(context, node->data.structField.init);
            break;
        case ZR_AST_STRUCT_METHOD:
            task_effects_validate_decorators(context, node->data.structMethod.decorators);
            task_effects_validate_function_like(context,
                                                ZR_FALSE,
                                                node->data.structMethod.params,
                                                node->data.structMethod.args,
                                                node->data.structMethod.body);
            break;
        case ZR_AST_STRUCT_META_FUNCTION:
            task_effects_validate_function_like(context,
                                                ZR_FALSE,
                                                node->data.structMetaFunction.params,
                                                node->data.structMetaFunction.args,
                                                node->data.structMetaFunction.body);
            break;
        default:
            break;
    }
}

void task_effects_validate_declaration(ZrTaskEffectContext *context, SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            task_effects_validate_decorators(context, node->data.classDeclaration.decorators);
            if (node->data.classDeclaration.members != ZR_NULL) {
                for (index = 0; index < node->data.classDeclaration.members->count; index++) {
                    task_effects_validate_class_member(context,
                                                       node->data.classDeclaration.members->nodes[index]);
                }
            }
            break;
        case ZR_AST_STRUCT_DECLARATION:
            task_effects_validate_decorators(context, node->data.structDeclaration.decorators);
            if (node->data.structDeclaration.members != ZR_NULL) {
                for (index = 0; index < node->data.structDeclaration.members->count; index++) {
                    task_effects_validate_struct_member(context,
                                                        node->data.structDeclaration.members->nodes[index]);
                }
            }
            break;
        case ZR_AST_ENUM_MEMBER:
            task_effects_validate_decorators(context, node->data.enumMember.decorators);
            task_effects_validate_node(context, node->data.enumMember.value);
            break;
        case ZR_AST_ENUM_DECLARATION:
            task_effects_validate_decorators(context, node->data.enumDeclaration.decorators);
            task_effects_validate_node_array(context, node->data.enumDeclaration.members);
            break;
        default:
            break;
    }
}
