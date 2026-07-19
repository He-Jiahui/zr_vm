#include "compiler_reference_escape_internal.h"

#include <string.h>

static TZrBool reference_escape_analyze_variable(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node) {
    SZrVariableDeclaration *declaration = &node->data.variableDeclaration;
    SZrReferenceEscapeProvenance provenance;
    SZrReferenceEscapeBinding binding;
    SZrString *name;
    TZrBool declaredReference;

    if (declaration->pattern == ZR_NULL ||
        declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
        return reference_escape_analyze_expression(
                context, declaration->value, ZR_FALSE, &provenance);
    }
    name = declaration->pattern->data.identifier.name;
    declaredReference = reference_escape_type_is_reference(declaration->typeInfo);
    if (!reference_escape_analyze_expression(
                context, declaration->value, declaredReference, &provenance)) {
        return ZR_FALSE;
    }
    memset(&binding, 0, sizeof(binding));
    binding.name = name;
    binding.scopeDepth = context->scopeDepth;
    binding.isReference = declaredReference;
    binding.isScoped = declaration->typeInfo != ZR_NULL &&
                       declaration->typeInfo->isScopedReference;
    binding.isWritable = declaration->typeInfo == ZR_NULL ||
                         declaration->typeInfo->referenceAccess !=
                                 ZR_REFERENCE_ACCESS_READONLY;
    binding.escapeBound = binding.isScoped
                                  ? ZR_SEMANTIC_ESCAPE_FUNCTION
                                  : ZR_SEMANTIC_ESCAPE_LOCAL;
    binding.originRange = node->location;
    binding.declarationSuspensionEpoch = context->suspensionEpoch;
    if (declaredReference && provenance.isReference) {
        binding.escapeBound = provenance.escapeBound;
        if (binding.isScoped && binding.escapeBound > ZR_SEMANTIC_ESCAPE_FUNCTION) {
            binding.escapeBound = ZR_SEMANTIC_ESCAPE_FUNCTION;
        }
        binding.originRange = provenance.originRange;
        binding.isScoped = (TZrBool)(binding.isScoped || provenance.isScoped);
        binding.isOut = provenance.isOut;
    }
    if (provenance.isClosure) {
        binding.isClosure = ZR_TRUE;
        binding.closureEscapeBound = provenance.closureEscapeBound;
        binding.closureWritableCaptureName =
                provenance.closureWritableCaptureName;
        binding.closureCaptureOriginRange =
                provenance.closureCaptureOriginRange;
    }
    reference_escape_push_binding(context, &binding);
    if (binding.isClosure && binding.closureWritableCaptureName != ZR_NULL) {
        SZrReferenceEscapeBinding *captured = reference_escape_find_binding(
                context, binding.closureWritableCaptureName);
        if (captured != ZR_NULL) {
            captured->mutableCaptureLastUseOffset =
                    reference_escape_last_identifier_offset(
                            context->bodyRoot, binding.name);
            captured->mutableCaptureRange = node->location;
        }
    }
    if (context->parent == ZR_NULL && provenance.isReference &&
        !reference_escape_validate_target(
                context,
                &provenance,
                ZR_SEMANTIC_ESCAPE_HEAP_STATIC,
                node->location,
                "module/global store")) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool reference_escape_analyze_return(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node) {
    SZrReferenceEscapeProvenance provenance;
    TZrBool returnsReference = reference_escape_type_is_reference(
            context->returnType);

    if (!reference_escape_analyze_expression(
                context,
                node->data.returnStatement.expr,
                returnsReference,
                &provenance)) {
        return ZR_FALSE;
    }
    if (returnsReference &&
        !reference_escape_validate_target(
                context,
                &provenance,
                ZR_SEMANTIC_ESCAPE_CALLER,
                node->location,
                "return")) {
        return ZR_FALSE;
    }
    if (provenance.isClosure &&
        provenance.closureEscapeBound < ZR_SEMANTIC_ESCAPE_CALLER) {
        SZrReferenceEscapeProvenance closureCapture;
        reference_escape_provenance_reset(&closureCapture);
        closureCapture.isReference = ZR_TRUE;
        closureCapture.escapeBound = provenance.closureEscapeBound;
        closureCapture.originRange = provenance.closureCaptureOriginRange;
        closureCapture.bindingName = provenance.closureWritableCaptureName;
        return reference_escape_validate_target(
                context,
                &closureCapture,
                ZR_SEMANTIC_ESCAPE_CALLER,
                node->location,
                "escaping closure return");
    }
    return ZR_TRUE;
}

static TZrBool reference_escape_analyze_function_like(
        SZrReferenceEscapeContext *parent,
        SZrAstNodeArray *parameters,
        SZrParameter *vararg,
        SZrType *returnType,
        SZrAstNode *body,
        SZrReferenceEscapeProvenance *provenance) {
    SZrReferenceEscapeContext child;

    if (provenance != ZR_NULL) {
        reference_escape_provenance_reset(provenance);
    }
    if (body == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!reference_escape_context_init(&child, parent->compiler, parent)) {
        return ZR_FALSE;
    }
    child.isFunctionBody = ZR_TRUE;
    child.isClosureBody = parent->isFunctionBody;
    child.bodyRoot = body;
    child.returnType = returnType;
    child.closureRange = body->location;
    reference_escape_register_parameters(&child, parameters, vararg);
    if (!reference_escape_analyze_node(&child, body)) {
        reference_escape_context_free(&child);
        return ZR_FALSE;
    }
    if (provenance != ZR_NULL && child.isClosureBody) {
        provenance->isClosure = ZR_TRUE;
        provenance->closureEscapeBound = child.hasReferenceCapture
                                                 ? child.captureEscapeBound
                                                 : ZR_SEMANTIC_ESCAPE_HEAP_STATIC;
        provenance->closureWritableCaptureName = child.writableCaptureName;
        provenance->closureCaptureOriginRange = child.captureOriginRange;
    }
    reference_escape_context_free(&child);
    return ZR_TRUE;
}

static TZrBool reference_escape_analyze_function_declaration(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node) {
    SZrFunctionDeclaration *declaration = &node->data.functionDeclaration;
    SZrReferenceEscapeProvenance provenance;
    SZrReferenceEscapeBinding binding;

    if (!reference_escape_analyze_function_like(
                context,
                declaration->params,
                declaration->args,
                declaration->returnType,
                declaration->body,
                &provenance)) {
        return ZR_FALSE;
    }
    if (!context->isFunctionBody || !provenance.isClosure ||
        declaration->name == ZR_NULL || declaration->name->name == ZR_NULL) {
        return ZR_TRUE;
    }
    memset(&binding, 0, sizeof(binding));
    binding.name = declaration->name->name;
    binding.scopeDepth = context->scopeDepth;
    binding.isClosure = ZR_TRUE;
    binding.closureEscapeBound = provenance.closureEscapeBound;
    binding.closureWritableCaptureName = provenance.closureWritableCaptureName;
    binding.closureCaptureOriginRange = provenance.closureCaptureOriginRange;
    reference_escape_push_binding(context, &binding);
    if (binding.closureWritableCaptureName != ZR_NULL) {
        SZrReferenceEscapeBinding *captured = reference_escape_find_binding(
                context, binding.closureWritableCaptureName);
        if (captured != ZR_NULL) {
            captured->mutableCaptureLastUseOffset =
                    reference_escape_last_identifier_offset(
                            context->bodyRoot, binding.name);
            captured->mutableCaptureRange = node->location;
        }
    }
    return ZR_TRUE;
}

static TZrBool reference_escape_analyze_declaration_members(
        SZrReferenceEscapeContext *context,
        SZrAstNodeArray *members) {
    TZrSize index;

    if (members == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        if (member == ZR_NULL) {
            continue;
        }
        switch (member->type) {
            case ZR_AST_CLASS_METHOD:
                if (!reference_escape_analyze_function_like(
                            context,
                            member->data.classMethod.params,
                            member->data.classMethod.args,
                            member->data.classMethod.returnType,
                            member->data.classMethod.body,
                            ZR_NULL)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_STRUCT_METHOD:
                if (!reference_escape_analyze_function_like(
                            context,
                            member->data.structMethod.params,
                            member->data.structMethod.args,
                            member->data.structMethod.returnType,
                            member->data.structMethod.body,
                            ZR_NULL)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_CLASS_META_FUNCTION:
                if (!reference_escape_analyze_function_like(
                            context,
                            member->data.classMetaFunction.params,
                            member->data.classMetaFunction.args,
                            member->data.classMetaFunction.returnType,
                            member->data.classMetaFunction.body,
                            ZR_NULL)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_STRUCT_META_FUNCTION:
                if (!reference_escape_analyze_function_like(
                            context,
                            member->data.structMetaFunction.params,
                            member->data.structMetaFunction.args,
                            member->data.structMetaFunction.returnType,
                            member->data.structMetaFunction.body,
                            ZR_NULL)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_CLASS_FIELD: {
                SZrReferenceEscapeProvenance ignored;
                if (!reference_escape_analyze_expression(
                            context,
                            member->data.classField.init,
                            reference_escape_type_is_reference(
                                    member->data.classField.typeInfo),
                            &ignored)) {
                    return ZR_FALSE;
                }
                break;
            }
            case ZR_AST_STRUCT_FIELD: {
                SZrReferenceEscapeProvenance ignored;
                if (!reference_escape_analyze_expression(
                            context,
                            member->data.structField.init,
                            reference_escape_type_is_reference(
                                    member->data.structField.typeInfo),
                            &ignored)) {
                    return ZR_FALSE;
                }
                break;
            }
            default:
                break;
        }
    }
    return ZR_TRUE;
}

static TZrBool reference_escape_analyze_node_array(
        SZrReferenceEscapeContext *context,
        SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < nodes->count; index++) {
        if (!reference_escape_analyze_node(context, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool reference_escape_analyze_node(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node) {
    SZrReferenceEscapeProvenance ignored;

    if (context == ZR_NULL || node == ZR_NULL || context->compiler->hasError) {
        return context != ZR_NULL && !context->compiler->hasError;
    }
    switch (node->type) {
        case ZR_AST_SCRIPT:
            return reference_escape_analyze_node_array(
                    context, node->data.script.statements);
        case ZR_AST_BLOCK:
            reference_escape_enter_scope(context);
            if (!reference_escape_analyze_node_array(
                        context, node->data.block.body)) {
                reference_escape_leave_scope(context);
                return ZR_FALSE;
            }
            reference_escape_leave_scope(context);
            return ZR_TRUE;
        case ZR_AST_FUNCTION_DECLARATION:
            return reference_escape_analyze_function_declaration(context, node);
        case ZR_AST_LAMBDA_EXPRESSION:
            return reference_escape_analyze_expression(
                    context, node, ZR_FALSE, &ignored);
        case ZR_AST_VARIABLE_DECLARATION:
            return reference_escape_analyze_variable(context, node);
        case ZR_AST_RETURN_STATEMENT:
            return reference_escape_analyze_return(context, node);
        case ZR_AST_EXPRESSION_STATEMENT:
            return reference_escape_analyze_expression(
                    context,
                    node->data.expressionStatement.expr,
                    ZR_FALSE,
                    &ignored);
        case ZR_AST_OUT_STATEMENT:
            if (!reference_escape_analyze_expression(
                        context,
                        node->data.outStatement.expr,
                        ZR_FALSE,
                        &ignored)) {
                return ZR_FALSE;
            }
            if (context->isGeneratorBody) {
                context->suspensionEpoch++;
                context->suspensionRange = node->location;
                context->suspensionName = "yield";
            }
            return ZR_TRUE;
        case ZR_AST_IF_EXPRESSION:
            return reference_escape_analyze_node(
                           context, node->data.ifExpression.condition) &&
                   reference_escape_analyze_node(
                           context, node->data.ifExpression.thenExpr) &&
                   reference_escape_analyze_node(
                           context, node->data.ifExpression.elseExpr);
        case ZR_AST_WHILE_LOOP:
            return reference_escape_analyze_node(
                           context, node->data.whileLoop.cond) &&
                   reference_escape_analyze_node(
                           context, node->data.whileLoop.block);
        case ZR_AST_FOR_LOOP:
            return reference_escape_analyze_node(
                           context, node->data.forLoop.init) &&
                   reference_escape_analyze_node(
                           context, node->data.forLoop.cond) &&
                   reference_escape_analyze_node(
                           context, node->data.forLoop.step) &&
                   reference_escape_analyze_node(
                           context, node->data.forLoop.block);
        case ZR_AST_FOREACH_LOOP:
            return reference_escape_analyze_node(
                           context, node->data.foreachLoop.expr) &&
                   reference_escape_analyze_node(
                           context, node->data.foreachLoop.block);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return reference_escape_analyze_node(
                           context, node->data.tryCatchFinallyStatement.block) &&
                   reference_escape_analyze_node_array(
                           context,
                           node->data.tryCatchFinallyStatement.catchClauses) &&
                   reference_escape_analyze_node(
                           context,
                           node->data.tryCatchFinallyStatement.finallyBlock);
        case ZR_AST_CATCH_CLAUSE:
            return reference_escape_analyze_node(
                    context, node->data.catchClause.block);
        case ZR_AST_CLASS_DECLARATION:
            return reference_escape_analyze_declaration_members(
                    context, node->data.classDeclaration.members);
        case ZR_AST_STRUCT_DECLARATION:
            return reference_escape_analyze_declaration_members(
                    context, node->data.structDeclaration.members);
        default:
            return reference_escape_analyze_expression(
                    context, node, ZR_FALSE, &ignored);
    }
}

TZrBool compiler_validate_reference_escapes(
        SZrCompilerState *compiler,
        SZrAstNode *node) {
    SZrReferenceEscapeContext context;
    TZrBool success;

    if (compiler == ZR_NULL || node == ZR_NULL || compiler->state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!reference_escape_context_init(&context, compiler, ZR_NULL)) {
        return ZR_FALSE;
    }
    context.bodyRoot = node;
    success = reference_escape_analyze_node(&context, node);
    reference_escape_context_free(&context);
    return (TZrBool)(success && !compiler->hasError);
}
