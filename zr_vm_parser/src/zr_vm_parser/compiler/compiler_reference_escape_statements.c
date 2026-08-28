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
    TZrBool declaredRefLike;

    if (declaration->pattern == ZR_NULL ||
        declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
        return reference_escape_analyze_expression(
                context, declaration->value, ZR_FALSE, &provenance);
    }
    name = declaration->pattern->data.identifier.name;
    declaredRefLike = reference_escape_type_is_ref_like(
            context, declaration->typeInfo);
    declaredReference = (TZrBool)(
            reference_escape_type_is_reference(declaration->typeInfo) ||
            declaredRefLike);
    if (!reference_escape_analyze_expression(
                context, declaration->value, declaredReference, &provenance)) {
        return ZR_FALSE;
    }
    memset(&binding, 0, sizeof(binding));
    binding.name = name;
    binding.declaredType = declaration->typeInfo;
    binding.scopeDepth = context->scopeDepth;
    binding.isReference = (TZrBool)(
            declaredReference || provenance.isRefLike);
    binding.isRefLike = (TZrBool)(
            declaredRefLike || provenance.isRefLike);
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
    if (binding.isReference && provenance.isReference) {
        binding.escapeBound = provenance.escapeBound;
        if (binding.isScoped && binding.escapeBound > ZR_SEMANTIC_ESCAPE_FUNCTION) {
            binding.escapeBound = ZR_SEMANTIC_ESCAPE_FUNCTION;
        }
        binding.originRange = provenance.originRange;
        binding.isScoped = (TZrBool)(binding.isScoped || provenance.isScoped);
        binding.isOut = provenance.isOut;
        binding.isRefLike = (TZrBool)(
                binding.isRefLike || provenance.isRefLike);
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
    if (context->parent == ZR_NULL && provenance.isReference) {
        SZrReferenceEscapeProvenance stored = provenance;
        if (stored.isRefLike &&
            stored.escapeBound >= ZR_SEMANTIC_ESCAPE_HEAP_STATIC) {
            stored.escapeBound = ZR_SEMANTIC_ESCAPE_CALLER;
        }
        if (!reference_escape_validate_target(
                    context,
                    &stored,
                    ZR_SEMANTIC_ESCAPE_HEAP_STATIC,
                    node->location,
                    "module/global store")) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool reference_escape_owner_return_qualifier(
        SZrReferenceEscapeContext *context,
        const SZrReferenceEscapeProvenance *provenance,
        EZrOwnershipQualifier *outQualifier) {
    SZrReferenceEscapeBinding *binding;
    EZrOwnershipQualifier sourceQualifier;

    if (context == ZR_NULL || provenance == ZR_NULL ||
        provenance->bindingName == ZR_NULL || outQualifier == ZR_NULL ||
        context->returnType == ZR_NULL) {
        return ZR_FALSE;
    }
    binding = reference_escape_find_binding(context, provenance->bindingName);
    if (binding == ZR_NULL || binding->declaredType == ZR_NULL) {
        return ZR_FALSE;
    }
    sourceQualifier = binding->declaredType->ownershipQualifier;
    if (sourceQualifier != ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
        sourceQualifier != ZR_OWNERSHIP_QUALIFIER_SHARED) {
        return ZR_FALSE;
    }
    if (context->returnType->referenceAccess == ZR_REFERENCE_ACCESS_READONLY) {
        *outQualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
        return ZR_TRUE;
    }
    if (context->returnType->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE) {
        *outQualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool reference_escape_report_owner_return(
        SZrReferenceEscapeContext *context,
        SZrAstNode *returnNode,
        const SZrReferenceEscapeProvenance *provenance,
        TZrBool *outHandled) {
    SZrAstNode *expression;
    SZrStructuredDiagnostic diagnostic;
    SZrSemanticOwnershipFact fact;
    SZrFileRange lifetimeEnd;
    EZrOwnershipQualifier qualifier;
    const TZrChar *sourceMessage;
    TZrBool built;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }
    if (context == ZR_NULL || returnNode == ZR_NULL ||
        returnNode->type != ZR_AST_RETURN_STATEMENT || outHandled == ZR_NULL ||
        provenance == ZR_NULL || context->bodyRoot == ZR_NULL ||
        !reference_escape_owner_return_qualifier(
                context, provenance, &qualifier)) {
        return ZR_TRUE;
    }
    expression = returnNode->data.returnStatement.expr;
    if (expression == ZR_NULL) {
        return ZR_TRUE;
    }
    *outHandled = ZR_TRUE;
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    built = qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED
                    ? ZrParser_DiagnosticBuilder_BuildBorrowEscape(
                              context->compiler->state,
                              &diagnostic,
                              expression->location)
                    : ZrParser_DiagnosticBuilder_BuildLoanEscape(
                              context->compiler->state,
                              &diagnostic,
                              expression->location);
    sourceMessage = qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED
                            ? "Borrow source is here"
                            : "Loan source is here";
    lifetimeEnd = context->bodyRoot->location;
    lifetimeEnd.start = lifetimeEnd.end;
    if (!built ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                context->compiler->state,
                &diagnostic,
                expression->location,
                sourceMessage) ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                context->compiler->state,
                &diagnostic,
                lifetimeEnd,
                "Source lifetime ends here")) {
        ZrParser_StructuredDiagnostic_Free(
                context->compiler->state, &diagnostic);
        ZrParser_Compiler_Error(
                context->compiler,
                "Failed to construct ownership return escape diagnostic",
                expression->location);
        return ZR_FALSE;
    }

    if (context->compiler->semanticContext != ZR_NULL) {
        memset(&fact, 0, sizeof(fact));
        fact.node = returnNode;
        fact.range = expression->location;
        fact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_ERROR;
        fact.qualifier = qualifier;
        fact.symbolId = ZR_SEMANTIC_ID_INVALID;
        fact.lifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
        fact.ownerLifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
        fact.relatedNode = expression;
        fact.isViolation = ZR_TRUE;
        fact.diagnosticMessage = diagnostic.message;
        if (!ZrParser_SemanticFacts_AppendOwnership(
                    context->compiler->semanticContext, &fact)) {
            ZrParser_StructuredDiagnostic_Free(
                    context->compiler->state, &diagnostic);
            ZrParser_Compiler_Error(
                    context->compiler,
                    "Failed to publish ownership return escape fact",
                    expression->location);
            return ZR_FALSE;
        }
    }
    ZrParser_Compiler_StructuredError(context->compiler, &diagnostic);
    return ZR_FALSE;
}

static TZrBool reference_escape_analyze_return(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node) {
    SZrReferenceEscapeProvenance provenance;
    TZrBool handledOwnershipEscape;
    TZrBool returnsReference = (TZrBool)(
            reference_escape_type_is_reference(context->returnType) ||
            reference_escape_type_is_ref_like(
                    context, context->returnType));

    if (!reference_escape_analyze_expression(
                context,
                node->data.returnStatement.expr,
                returnsReference,
                &provenance)) {
        return ZR_FALSE;
    }
    if (returnsReference &&
        provenance.isReference &&
        provenance.escapeBound < ZR_SEMANTIC_ESCAPE_CALLER) {
        if (!reference_escape_report_owner_return(
                    context,
                    node,
                    &provenance,
                    &handledOwnershipEscape)) {
            return ZR_FALSE;
        }
        if (!handledOwnershipEscape &&
            !reference_escape_validate_target(
                    context,
                    &provenance,
                    ZR_SEMANTIC_ESCAPE_CALLER,
                    node->location,
                    "return")) {
            return ZR_FALSE;
        }
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

static TZrBool reference_escape_analyze_property(
        SZrReferenceEscapeContext *context,
        SZrAstNode *propertyNode) {
    SZrPropertyDeclaration *property;

    if (context == ZR_NULL || propertyNode == ZR_NULL ||
        propertyNode->type != ZR_AST_PROPERTY_DECLARATION) {
        return ZR_FALSE;
    }
    property = &propertyNode->data.propertyDeclaration;
    for (TZrSize index = 0U;
         property->accessors != ZR_NULL && index < property->accessors->count;
         index++) {
        SZrAstNode *accessorNode = property->accessors->nodes[index];
        SZrPropertyAccessor *accessor;

        if (accessorNode == ZR_NULL ||
            accessorNode->type != ZR_AST_PROPERTY_ACCESSOR) {
            continue;
        }
        accessor = &accessorNode->data.propertyAccessor;
        if (accessor->kind != ZR_PROPERTY_ACCESSOR_GET ||
            accessor->body == ZR_NULL) {
            continue;
        }
        if (accessor->bodyKind == ZR_PROPERTY_ACCESSOR_BODY_EXPRESSION) {
            SZrAstNode returnNode;

            memset(&returnNode, 0, sizeof(returnNode));
            returnNode.type = ZR_AST_RETURN_STATEMENT;
            returnNode.location = accessor->body->location;
            returnNode.data.returnStatement.expr = accessor->body;
            returnNode.data.returnStatement.isReferenceReturn =
                    accessor->isReferenceResult;
            returnNode.data.returnStatement.referenceLocation =
                    accessor->referenceLocation;
            if (!reference_escape_analyze_function_like(
                        context,
                        ZR_NULL,
                        ZR_NULL,
                        property->typeInfo,
                        &returnNode,
                        ZR_NULL)) {
                return ZR_FALSE;
            }
        } else if (!reference_escape_analyze_function_like(
                           context,
                           ZR_NULL,
                           ZR_NULL,
                           property->typeInfo,
                           accessor->body,
                           ZR_NULL)) {
            return ZR_FALSE;
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
            case ZR_AST_PROPERTY_DECLARATION:
                if (!reference_escape_analyze_property(context, member)) {
                    return ZR_FALSE;
                }
                break;
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
                                    member->data.classField.typeInfo) ||
                                    reference_escape_type_is_ref_like(
                                            context,
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
                                    member->data.structField.typeInfo) ||
                                    reference_escape_type_is_ref_like(
                                            context,
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
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return node->data.compileTimeDeclaration.selectedBranch == ZR_NULL ||
                   reference_escape_analyze_node(
                           context,
                           node->data.compileTimeDeclaration.selectedBranch);
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
        case ZR_AST_YIELD_STATEMENT:
            if (!reference_escape_analyze_expression(
                        context,
                        node->data.yieldStatement.expr,
                        ZR_FALSE,
                        &ignored)) {
                return ZR_FALSE;
            }
            context->suspensionEpoch++;
            context->suspensionRange = node->location;
            context->suspensionName = "yield";
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
    if (!compiler_ref_struct_type_set_init(
                &context.refStructTypeStorage,
                compiler->state,
                compiler->semanticContext,
                node)) {
        reference_escape_context_free(&context);
        return ZR_FALSE;
    }
    context.refStructTypes = &context.refStructTypeStorage;
    context.bodyRoot = node;
    success = reference_escape_analyze_node(&context, node);
    compiler_ref_struct_type_set_free(
            &context.refStructTypeStorage, compiler->state);
    context.refStructTypes = ZR_NULL;
    reference_escape_context_free(&context);
    return (TZrBool)(success && !compiler->hasError);
}

TZrBool ZrParser_Compiler_ValidateReferenceEscapes(
        SZrCompilerState *compiler,
        SZrAstNode *node) {
    return compiler_validate_reference_escapes(compiler, node);
}
