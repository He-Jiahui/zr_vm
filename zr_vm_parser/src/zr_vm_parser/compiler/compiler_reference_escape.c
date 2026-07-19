#include "compiler_reference_escape_internal.h"

#include <stdio.h>
#include <string.h>

static TZrBool reference_escape_string_equals(
        SZrString *left,
        SZrString *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           ZrCore_String_Equal(left, right);
}

static TZrBool reference_escape_string_equals_literal(
        SZrString *value,
        const TZrChar *literal) {
    TZrNativeString nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }
    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

void reference_escape_provenance_reset(
        SZrReferenceEscapeProvenance *provenance) {
    if (provenance == ZR_NULL) {
        return;
    }
    memset(provenance, 0, sizeof(*provenance));
    provenance->escapeBound = ZR_SEMANTIC_ESCAPE_HEAP_STATIC;
    provenance->closureEscapeBound = ZR_SEMANTIC_ESCAPE_HEAP_STATIC;
}

TZrBool reference_escape_context_init(
        SZrReferenceEscapeContext *context,
        SZrCompilerState *compiler,
        SZrReferenceEscapeContext *parent) {
    if (context == ZR_NULL || compiler == ZR_NULL || compiler->state == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(context, 0, sizeof(*context));
    context->compiler = compiler;
    context->parent = parent;
    context->captureEscapeBound = ZR_SEMANTIC_ESCAPE_HEAP_STATIC;
    if (parent != ZR_NULL) {
        context->suspensionEpoch = parent->suspensionEpoch;
        context->suspensionRange = parent->suspensionRange;
        context->suspensionName = parent->suspensionName;
    }
    ZrCore_Array_Init(
            compiler->state,
            &context->bindings,
            sizeof(SZrReferenceEscapeBinding),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

void reference_escape_context_free(
        SZrReferenceEscapeContext *context) {
    if (context == ZR_NULL || context->compiler == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(context->compiler->state, &context->bindings);
}

static SZrReferenceEscapeBinding *reference_escape_find_binding_owned(
        SZrReferenceEscapeContext *context,
        SZrString *name,
        SZrReferenceEscapeContext **owner) {
    SZrReferenceEscapeContext *current;

    if (owner != ZR_NULL) {
        *owner = ZR_NULL;
    }
    for (current = context; current != ZR_NULL; current = current->parent) {
        TZrSize index;
        for (index = current->bindings.length; index > 0U; index--) {
            SZrReferenceEscapeBinding *binding =
                    (SZrReferenceEscapeBinding *)ZrCore_Array_Get(
                            &current->bindings, index - 1U);
            if (binding != ZR_NULL &&
                reference_escape_string_equals(binding->name, name)) {
                if (owner != ZR_NULL) {
                    *owner = current;
                }
                return binding;
            }
        }
    }
    return ZR_NULL;
}

SZrReferenceEscapeBinding *reference_escape_find_binding(
        SZrReferenceEscapeContext *context,
        SZrString *name) {
    return reference_escape_find_binding_owned(context, name, ZR_NULL);
}

void reference_escape_enter_scope(
        SZrReferenceEscapeContext *context) {
    if (context != ZR_NULL) {
        context->scopeDepth++;
    }
}

void reference_escape_leave_scope(
        SZrReferenceEscapeContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    while (context->bindings.length > 0U) {
        SZrReferenceEscapeBinding *binding =
                (SZrReferenceEscapeBinding *)ZrCore_Array_Get(
                        &context->bindings, context->bindings.length - 1U);
        if (binding == ZR_NULL || binding->scopeDepth < context->scopeDepth) {
            break;
        }
        ZrCore_Array_Pop(&context->bindings);
    }
    if (context->scopeDepth > 0) {
        context->scopeDepth--;
    }
}

SZrReferenceEscapeBinding *reference_escape_push_binding(
        SZrReferenceEscapeContext *context,
        const SZrReferenceEscapeBinding *binding) {
    if (context == ZR_NULL || binding == ZR_NULL || binding->name == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Array_Push(context->compiler->state, &context->bindings, (TZrPtr)binding);
    return (SZrReferenceEscapeBinding *)ZrCore_Array_Get(
            &context->bindings, context->bindings.length - 1U);
}

static TZrBool reference_escape_report(
        SZrReferenceEscapeContext *context,
        SZrFileRange originRange,
        SZrFileRange escapeRange,
        const TZrChar *code,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion) {
    SZrStructuredDiagnostic diagnostic;

    if (context == ZR_NULL || context->compiler == ZR_NULL ||
        context->compiler->hasError) {
        return ZR_FALSE;
    }
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_Build(
                context->compiler->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                escapeRange,
                code,
                message,
                cause,
                suggestion)) {
        ZrParser_Compiler_Error(context->compiler, message, escapeRange);
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_AddRelatedInformation(
                context->compiler->state,
                &diagnostic,
                originRange,
                "Reference origin and maximum safe escape are declared here")) {
        ZrParser_StructuredDiagnostic_Free(context->compiler->state, &diagnostic);
        ZrParser_Compiler_Error(context->compiler, message, escapeRange);
        return ZR_FALSE;
    }
    ZrParser_Compiler_StructuredError(context->compiler, &diagnostic);
    return ZR_FALSE;
}

static const TZrChar *reference_escape_state_name(
        EZrSemanticEscapeState state) {
    switch (state) {
        case ZR_SEMANTIC_ESCAPE_LOCAL:
            return "block";
        case ZR_SEMANTIC_ESCAPE_FUNCTION:
            return "function";
        case ZR_SEMANTIC_ESCAPE_CALLER:
            return "caller";
        case ZR_SEMANTIC_ESCAPE_HEAP_STATIC:
            return "heap/static";
        case ZR_SEMANTIC_ESCAPE_UNKNOWN:
        default:
            return "unknown";
    }
}

TZrBool reference_escape_validate_target(
        SZrReferenceEscapeContext *context,
        const SZrReferenceEscapeProvenance *provenance,
        EZrSemanticEscapeState target,
        SZrFileRange escapeRange,
        const TZrChar *reason) {
    TZrChar message[320];
    TZrChar cause[256];
    TZrNativeString name;

    if (provenance == ZR_NULL || !provenance->isReference ||
        provenance->escapeBound >= target) {
        return ZR_TRUE;
    }
    name = provenance->bindingName != ZR_NULL
                   ? ZrCore_String_GetNativeString(provenance->bindingName)
                   : ZR_NULL;
    snprintf(message,
             sizeof(message),
             "Reference '%s' cannot escape to %s through %s",
             name != ZR_NULL ? name : "<reference>",
             reference_escape_state_name(target),
             reason != ZR_NULL ? reason : "this operation");
    snprintf(cause,
             sizeof(cause),
             "The reference is limited to %s but this operation requires %s",
             reference_escape_state_name(provenance->escapeBound),
             reference_escape_state_name(target));
    return reference_escape_report(
            context,
            provenance->originRange,
            escapeRange,
            "reference_escape",
            message,
            cause,
            "Return or store an owned value, or shorten the destination lifetime");
}

TZrBool reference_escape_type_is_reference(const SZrType *type) {
    return type != ZR_NULL &&
           type->referenceAccess != ZR_REFERENCE_ACCESS_NONE;
}

static TZrBool reference_escape_parameter_is_reference(
        const SZrParameter *parameter) {
    return parameter != ZR_NULL &&
           (parameter->sourcePassingForm != ZR_PARAMETER_SOURCE_VALUE ||
            reference_escape_type_is_reference(parameter->typeInfo));
}

static EZrSemanticEscapeState reference_escape_parameter_bound(
        const SZrParameter *parameter) {
    if (parameter == ZR_NULL) {
        return ZR_SEMANTIC_ESCAPE_FUNCTION;
    }
    switch (parameter->sourcePassingForm) {
        case ZR_PARAMETER_SOURCE_REF:
        case ZR_PARAMETER_SOURCE_REF_READONLY:
            return ZR_SEMANTIC_ESCAPE_CALLER;
        case ZR_PARAMETER_SOURCE_VALUE:
            return parameter->typeInfo != ZR_NULL &&
                           reference_escape_type_is_reference(parameter->typeInfo) &&
                           !parameter->typeInfo->isScopedReference
                           ? ZR_SEMANTIC_ESCAPE_CALLER
                           : ZR_SEMANTIC_ESCAPE_FUNCTION;
        case ZR_PARAMETER_SOURCE_IN:
        case ZR_PARAMETER_SOURCE_OUT:
        case ZR_PARAMETER_SOURCE_SCOPED_REF:
        case ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY:
        default:
            return ZR_SEMANTIC_ESCAPE_FUNCTION;
    }
}

static TZrBool reference_escape_parameter_is_scoped(
        const SZrParameter *parameter) {
    return parameter != ZR_NULL &&
           (parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_IN ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_OUT ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_SCOPED_REF ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY ||
            (parameter->typeInfo != ZR_NULL && parameter->typeInfo->isScopedReference));
}

static TZrBool reference_escape_parameter_is_writable(
        const SZrParameter *parameter) {
    return parameter != ZR_NULL &&
           parameter->sourcePassingForm != ZR_PARAMETER_SOURCE_IN &&
           parameter->sourcePassingForm != ZR_PARAMETER_SOURCE_REF_READONLY &&
           parameter->sourcePassingForm != ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY &&
           (parameter->typeInfo == ZR_NULL ||
            parameter->typeInfo->referenceAccess != ZR_REFERENCE_ACCESS_READONLY);
}

void reference_escape_register_parameters(
        SZrReferenceEscapeContext *context,
        SZrAstNodeArray *parameters,
        SZrParameter *vararg) {
    TZrSize index;

    if (context == ZR_NULL) {
        return;
    }
    if (parameters != ZR_NULL) {
        for (index = 0U; index < parameters->count; index++) {
            SZrAstNode *node = parameters->nodes[index];
            SZrParameter *parameter;
            SZrReferenceEscapeBinding binding;

            if (node == ZR_NULL || node->type != ZR_AST_PARAMETER) {
                continue;
            }
            parameter = &node->data.parameter;
            if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
                continue;
            }
            memset(&binding, 0, sizeof(binding));
            binding.name = parameter->name->name;
            binding.scopeDepth = context->scopeDepth;
            binding.isReference = reference_escape_parameter_is_reference(parameter);
            binding.isScoped = reference_escape_parameter_is_scoped(parameter);
            binding.isOut = parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_OUT;
            binding.isWritable = reference_escape_parameter_is_writable(parameter);
            binding.escapeBound = reference_escape_parameter_bound(parameter);
            binding.originRange = node->location;
            binding.declarationSuspensionEpoch = context->suspensionEpoch;
            reference_escape_push_binding(context, &binding);
        }
    }
    if (vararg != ZR_NULL && vararg->name != ZR_NULL) {
        SZrReferenceEscapeBinding binding;
        memset(&binding, 0, sizeof(binding));
        binding.name = vararg->name->name;
        binding.scopeDepth = context->scopeDepth;
        binding.isReference = reference_escape_parameter_is_reference(vararg);
        binding.isScoped = reference_escape_parameter_is_scoped(vararg);
        binding.isOut = vararg->sourcePassingForm == ZR_PARAMETER_SOURCE_OUT;
        binding.isWritable = reference_escape_parameter_is_writable(vararg);
        binding.escapeBound = reference_escape_parameter_bound(vararg);
        binding.originRange = vararg->nameLocation;
        binding.declarationSuspensionEpoch = context->suspensionEpoch;
        reference_escape_push_binding(context, &binding);
    }
}

TZrSize reference_escape_last_identifier_offset(
        SZrAstNode *node,
        SZrString *name) {
    TZrSize result = 0U;
    TZrSize index;

    if (node == ZR_NULL || name == ZR_NULL) {
        return 0U;
    }
    if (node->type == ZR_AST_IDENTIFIER_LITERAL &&
        reference_escape_string_equals(node->data.identifier.name, name)) {
        return node->location.end.offset;
    }
    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (index = 0U; index < node->data.script.statements->count; index++) {
                    TZrSize candidate = reference_escape_last_identifier_offset(
                            node->data.script.statements->nodes[index], name);
                    if (candidate > result) {
                        result = candidate;
                    }
                }
            }
            break;
        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL) {
                for (index = 0U; index < node->data.block.body->count; index++) {
                    TZrSize candidate = reference_escape_last_identifier_offset(
                            node->data.block.body->nodes[index], name);
                    if (candidate > result) {
                        result = candidate;
                    }
                }
            }
            break;
        case ZR_AST_VARIABLE_DECLARATION:
            result = reference_escape_last_identifier_offset(
                    node->data.variableDeclaration.value, name);
            break;
        case ZR_AST_EXPRESSION_STATEMENT:
            result = reference_escape_last_identifier_offset(
                    node->data.expressionStatement.expr, name);
            break;
        case ZR_AST_RETURN_STATEMENT:
            result = reference_escape_last_identifier_offset(
                    node->data.returnStatement.expr, name);
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            TZrSize left = reference_escape_last_identifier_offset(
                    node->data.assignmentExpression.left, name);
            TZrSize right = reference_escape_last_identifier_offset(
                    node->data.assignmentExpression.right, name);
            result = left > right ? left : right;
            break;
        }
        case ZR_AST_BINARY_EXPRESSION: {
            TZrSize left = reference_escape_last_identifier_offset(
                    node->data.binaryExpression.left, name);
            TZrSize right = reference_escape_last_identifier_offset(
                    node->data.binaryExpression.right, name);
            result = left > right ? left : right;
            break;
        }
        case ZR_AST_LOGICAL_EXPRESSION: {
            TZrSize left = reference_escape_last_identifier_offset(
                    node->data.logicalExpression.left, name);
            TZrSize right = reference_escape_last_identifier_offset(
                    node->data.logicalExpression.right, name);
            result = left > right ? left : right;
            break;
        }
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            TZrSize test = reference_escape_last_identifier_offset(
                    node->data.conditionalExpression.test, name);
            TZrSize consequent = reference_escape_last_identifier_offset(
                    node->data.conditionalExpression.consequent, name);
            TZrSize alternate = reference_escape_last_identifier_offset(
                    node->data.conditionalExpression.alternate, name);
            result = test > consequent ? test : consequent;
            if (alternate > result) {
                result = alternate;
            }
            break;
        }
        case ZR_AST_PRIMARY_EXPRESSION:
            result = reference_escape_last_identifier_offset(
                    node->data.primaryExpression.property, name);
            if (node->data.primaryExpression.members != ZR_NULL) {
                for (index = 0U; index < node->data.primaryExpression.members->count; index++) {
                    TZrSize candidate = reference_escape_last_identifier_offset(
                            node->data.primaryExpression.members->nodes[index], name);
                    if (candidate > result) {
                        result = candidate;
                    }
                }
            }
            break;
        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL) {
                for (index = 0U; index < node->data.functionCall.args->count; index++) {
                    TZrSize candidate = reference_escape_last_identifier_offset(
                            node->data.functionCall.args->nodes[index], name);
                    if (candidate > result) {
                        result = candidate;
                    }
                }
            }
            break;
        default:
            break;
    }
    return result;
}

static TZrBool reference_escape_primary_is_await(
        SZrAstNode *node,
        TZrSize *callIndex) {
    SZrPrimaryExpression *primary;
    SZrAstNode *member;

    if (callIndex != ZR_NULL) {
        *callIndex = 0U;
    }
    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION ||
        node->data.primaryExpression.property == ZR_NULL ||
        node->data.primaryExpression.property->type != ZR_AST_IMPORT_EXPRESSION ||
        node->data.primaryExpression.property->data.importExpression.modulePath == ZR_NULL ||
        node->data.primaryExpression.property->data.importExpression.modulePath->type !=
                ZR_AST_STRING_LITERAL ||
        !reference_escape_string_equals_literal(
                node->data.primaryExpression.property->data.importExpression.modulePath
                        ->data.stringLiteral.value,
                "zr.task")) {
        return ZR_FALSE;
    }
    primary = &node->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count < 2U) {
        return ZR_FALSE;
    }
    member = primary->members->nodes[0];
    if (member == ZR_NULL || member->type != ZR_AST_MEMBER_EXPRESSION ||
        member->data.memberExpression.computed ||
        member->data.memberExpression.property == ZR_NULL ||
        member->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        !reference_escape_string_equals_literal(
                member->data.memberExpression.property->data.identifier.name,
                "__awaitTask")) {
        return ZR_FALSE;
    }
    member = primary->members->nodes[1];
    if (member == ZR_NULL || member->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }
    if (callIndex != ZR_NULL) {
        *callIndex = 1U;
    }
    return ZR_TRUE;
}

TZrBool reference_escape_analyze_expression(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node,
        TZrBool wantReference,
        SZrReferenceEscapeProvenance *provenance);

static TZrBool reference_escape_analyze_expression_array(
        SZrReferenceEscapeContext *context,
        SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < nodes->count; index++) {
        SZrReferenceEscapeProvenance ignored;
        if (!reference_escape_analyze_expression(
                    context, nodes->nodes[index], ZR_FALSE, &ignored)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool reference_escape_check_identifier(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node,
        TZrBool wantReference,
        SZrReferenceEscapeProvenance *provenance) {
    SZrReferenceEscapeContext *owner = ZR_NULL;
    SZrReferenceEscapeBinding *binding;

    binding = reference_escape_find_binding_owned(
            context, node->data.identifier.name, &owner);
    if (binding == ZR_NULL) {
        return ZR_TRUE;
    }
    if (binding->isReference &&
        binding->declarationSuspensionEpoch < context->suspensionEpoch) {
        TZrChar message[256];
        TZrNativeString name = ZrCore_String_GetNativeString(binding->name);
        snprintf(message,
                 sizeof(message),
                 "Reference '%s' cannot cross a%s %s suspension",
                 name != ZR_NULL ? name : "<reference>",
                 context->suspensionName != ZR_NULL &&
                         context->suspensionName[0] == 'a'
                         ? "n"
                         : "",
                 context->suspensionName != ZR_NULL
                         ? context->suspensionName
                         : "coroutine");
        return reference_escape_report(
                context,
                binding->originRange,
                context->suspensionRange,
                "reference_suspension",
                message,
                "The reference was created before the suspension and is used after it",
                "End the reference's last use before the suspension or move an owned value into the frame");
    }
    if (binding->mutableCaptureLastUseOffset > 0U &&
        node->location.start.offset <= binding->mutableCaptureLastUseOffset &&
        !context->isClosureBody) {
        TZrChar message[256];
        TZrNativeString name = ZrCore_String_GetNativeString(binding->name);
        snprintf(message,
                 sizeof(message),
                 "Access to reference '%s' conflicts with writable closure capture",
                 name != ZR_NULL ? name : "<reference>");
        return reference_escape_report(
                context,
                binding->originRange,
                node->location,
                "reference_closure_conflict",
                message,
                "The closure keeps a mutable loan alive until its last use",
                "Use the closure before accessing the reference again, or capture readonly");
    }
    if (context->isClosureBody && owner != context &&
        (binding->isReference || wantReference)) {
        context->hasReferenceCapture = ZR_TRUE;
        if (binding->escapeBound < context->captureEscapeBound) {
            context->captureEscapeBound = binding->escapeBound;
            context->captureOriginRange = binding->originRange;
        }
        if (binding->isScoped || binding->isOut) {
            TZrChar message[256];
            TZrNativeString name = ZrCore_String_GetNativeString(binding->name);
            context->hasScopedCapture = ZR_TRUE;
            snprintf(message,
                     sizeof(message),
                     "Scoped reference cannot be captured by a closure: '%s'",
                     name != ZR_NULL ? name : "<reference>");
            return reference_escape_report(
                    context,
                    binding->originRange,
                    context->closureRange,
                    "reference_closure_escape",
                    message,
                    "in, out and scoped ref contracts are confined to the current function",
                    "Copy the referent value or pass an owned handle into the closure");
        }
        if (binding->isWritable && context->writableCaptureName == ZR_NULL) {
            context->writableCaptureName = binding->name;
            context->writableCaptureOriginRange = binding->originRange;
        }
    }
    if (binding->isReference || wantReference) {
        provenance->isReference = ZR_TRUE;
        provenance->isScoped = binding->isScoped;
        provenance->isOut = binding->isOut;
        provenance->isWritable = binding->isWritable;
        provenance->escapeBound = binding->escapeBound;
        provenance->originRange = binding->originRange;
        provenance->bindingName = binding->name;
    }
    if (binding->isClosure) {
        provenance->isClosure = ZR_TRUE;
        provenance->closureEscapeBound = binding->closureEscapeBound;
        provenance->closureWritableCaptureName =
                binding->closureWritableCaptureName;
        provenance->closureCaptureOriginRange =
                binding->closureCaptureOriginRange;
    }
    return ZR_TRUE;
}

static TZrBool reference_escape_analyze_lambda(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node,
        SZrReferenceEscapeProvenance *provenance) {
    SZrReferenceEscapeContext child;
    SZrLambdaExpression *lambda = &node->data.lambdaExpression;

    if (!reference_escape_context_init(&child, context->compiler, context)) {
        return ZR_FALSE;
    }
    child.isFunctionBody = ZR_TRUE;
    child.isClosureBody = ZR_TRUE;
    child.bodyRoot = lambda->block;
    child.returnType = lambda->returnType;
    child.closureRange = node->location;
    reference_escape_register_parameters(&child, lambda->params, lambda->args);
    if (!reference_escape_analyze_node(&child, lambda->block)) {
        reference_escape_context_free(&child);
        return ZR_FALSE;
    }
    provenance->isClosure = ZR_TRUE;
    provenance->closureEscapeBound = child.hasReferenceCapture
                                             ? child.captureEscapeBound
                                             : ZR_SEMANTIC_ESCAPE_HEAP_STATIC;
    provenance->closureWritableCaptureName = child.writableCaptureName;
    provenance->closureCaptureOriginRange = child.captureOriginRange;
    reference_escape_context_free(&child);
    return ZR_TRUE;
}

static TZrBool reference_escape_analyze_primary(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node,
        TZrBool wantReference,
        SZrReferenceEscapeProvenance *provenance) {
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    TZrSize awaitCallIndex = 0U;
    TZrSize index;

    if (reference_escape_primary_is_await(node, &awaitCallIndex)) {
        SZrAstNode *call = primary->members->nodes[awaitCallIndex];
        if (!reference_escape_analyze_expression_array(
                    context, call->data.functionCall.args)) {
            return ZR_FALSE;
        }
        context->suspensionEpoch++;
        context->suspensionRange = node->location;
        context->suspensionName = "await";
        return ZR_TRUE;
    }
    if (!reference_escape_analyze_expression(
                context, primary->property, wantReference, provenance)) {
        return ZR_FALSE;
    }
    if (primary->members == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < primary->members->count; index++) {
        SZrAstNode *member = primary->members->nodes[index];
        if (member == ZR_NULL) {
            continue;
        }
        if (member->type == ZR_AST_FUNCTION_CALL) {
            SZrFunctionCall *call = &member->data.functionCall;
            TZrSize argumentIndex;
            for (argumentIndex = 0U;
                 call->args != ZR_NULL && argumentIndex < call->args->count;
                 argumentIndex++) {
                SZrReferenceEscapeProvenance ignored;
                TZrBool argumentWantsReference = ZR_FALSE;
                if (call->argumentMarkers != ZR_NULL &&
                    argumentIndex < call->argumentMarkers->length) {
                    const SZrCallArgumentSyntax *syntax =
                            (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
                                    call->argumentMarkers, argumentIndex);
                    argumentWantsReference = syntax != ZR_NULL &&
                                             syntax->marker != ZR_CALL_ARGUMENT_MARKER_NONE;
                }
                if (!reference_escape_analyze_expression(
                            context,
                            call->args->nodes[argumentIndex],
                            argumentWantsReference,
                            &ignored)) {
                    return ZR_FALSE;
                }
            }
            reference_escape_provenance_reset(provenance);
        } else if (member->type == ZR_AST_MEMBER_EXPRESSION &&
                   member->data.memberExpression.computed) {
            SZrReferenceEscapeProvenance ignored;
            if (!reference_escape_analyze_expression(
                        context,
                        member->data.memberExpression.property,
                        ZR_FALSE,
                        &ignored)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static void reference_escape_merge_provenance(
        SZrReferenceEscapeProvenance *target,
        const SZrReferenceEscapeProvenance *candidate) {
    if (target == ZR_NULL || candidate == ZR_NULL) {
        return;
    }
    if (candidate->isReference &&
        (!target->isReference || candidate->escapeBound < target->escapeBound)) {
        *target = *candidate;
    }
    if (candidate->isClosure &&
        (!target->isClosure ||
         candidate->closureEscapeBound < target->closureEscapeBound)) {
        target->isClosure = ZR_TRUE;
        target->closureEscapeBound = candidate->closureEscapeBound;
        target->closureWritableCaptureName = candidate->closureWritableCaptureName;
        target->closureCaptureOriginRange = candidate->closureCaptureOriginRange;
    }
}

TZrBool reference_escape_analyze_expression(
        SZrReferenceEscapeContext *context,
        SZrAstNode *node,
        TZrBool wantReference,
        SZrReferenceEscapeProvenance *provenance) {
    SZrReferenceEscapeProvenance left;
    SZrReferenceEscapeProvenance right;
    TZrSize index;

    reference_escape_provenance_reset(provenance);
    if (context == ZR_NULL || node == ZR_NULL || context->compiler->hasError) {
        return context != ZR_NULL && !context->compiler->hasError;
    }
    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return reference_escape_check_identifier(
                    context, node, wantReference, provenance);
        case ZR_AST_LAMBDA_EXPRESSION:
            return reference_escape_analyze_lambda(context, node, provenance);
        case ZR_AST_PRIMARY_EXPRESSION:
            return reference_escape_analyze_primary(
                    context, node, wantReference, provenance);
        case ZR_AST_FUNCTION_CALL:
            return reference_escape_analyze_expression_array(
                    context, node->data.functionCall.args);
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAstNode *target = node->data.assignmentExpression.left;
            TZrBool targetIsHeap = target != ZR_NULL &&
                                   (target->type == ZR_AST_PRIMARY_EXPRESSION ||
                                    target->type == ZR_AST_MEMBER_EXPRESSION);
            SZrReferenceEscapeBinding *targetBinding = ZR_NULL;
            if (target != ZR_NULL && target->type == ZR_AST_IDENTIFIER_LITERAL) {
                targetBinding = reference_escape_find_binding(
                        context, target->data.identifier.name);
            }
            if (!reference_escape_analyze_expression(
                        context,
                        node->data.assignmentExpression.right,
                        targetBinding != ZR_NULL && targetBinding->isReference,
                        &right)) {
                return ZR_FALSE;
            }
            if (targetIsHeap &&
                !reference_escape_validate_target(
                        context,
                        &right,
                        ZR_SEMANTIC_ESCAPE_HEAP_STATIC,
                        node->location,
                        "heap/static store")) {
                return ZR_FALSE;
            }
            if (targetBinding != ZR_NULL && right.isReference) {
                targetBinding->escapeBound = right.escapeBound;
                targetBinding->originRange = right.originRange;
                targetBinding->isScoped = right.isScoped;
                targetBinding->isOut = right.isOut;
            }
            if (!reference_escape_analyze_expression(
                        context, target, ZR_FALSE, &left)) {
                return ZR_FALSE;
            }
            return ZR_TRUE;
        }
        case ZR_AST_CONDITIONAL_EXPRESSION:
            if (!reference_escape_analyze_expression(
                        context,
                        node->data.conditionalExpression.test,
                        ZR_FALSE,
                        &left) ||
                !reference_escape_analyze_expression(
                        context,
                        node->data.conditionalExpression.consequent,
                        wantReference,
                        &left) ||
                !reference_escape_analyze_expression(
                        context,
                        node->data.conditionalExpression.alternate,
                        wantReference,
                        &right)) {
                return ZR_FALSE;
            }
            reference_escape_merge_provenance(provenance, &left);
            reference_escape_merge_provenance(provenance, &right);
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            return reference_escape_analyze_expression(
                           context,
                           node->data.binaryExpression.left,
                           ZR_FALSE,
                           &left) &&
                   reference_escape_analyze_expression(
                           context,
                           node->data.binaryExpression.right,
                           ZR_FALSE,
                           &right);
        case ZR_AST_LOGICAL_EXPRESSION:
            return reference_escape_analyze_expression(
                           context,
                           node->data.logicalExpression.left,
                           ZR_FALSE,
                           &left) &&
                   reference_escape_analyze_expression(
                           context,
                           node->data.logicalExpression.right,
                           ZR_FALSE,
                           &right);
        case ZR_AST_UNARY_EXPRESSION:
            return reference_escape_analyze_expression(
                    context,
                    node->data.unaryExpression.argument,
                    wantReference,
                    provenance);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return reference_escape_analyze_expression(
                    context,
                    node->data.typeCastExpression.expression,
                    wantReference,
                    provenance);
        case ZR_AST_ARRAY_LITERAL:
            if (node->data.arrayLiteral.elements != ZR_NULL) {
                for (index = 0U; index < node->data.arrayLiteral.elements->count; index++) {
                    if (!reference_escape_analyze_expression(
                                context,
                                node->data.arrayLiteral.elements->nodes[index],
                                ZR_FALSE,
                                &left) ||
                        !reference_escape_validate_target(
                                context,
                                &left,
                                ZR_SEMANTIC_ESCAPE_HEAP_STATIC,
                                node->location,
                                "container store")) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_OBJECT_LITERAL:
            if (node->data.objectLiteral.properties != ZR_NULL) {
                for (index = 0U; index < node->data.objectLiteral.properties->count; index++) {
                    SZrAstNode *property =
                            node->data.objectLiteral.properties->nodes[index];
                    if (property != ZR_NULL && property->type == ZR_AST_KEY_VALUE_PAIR &&
                        (!reference_escape_analyze_expression(
                                 context,
                                 property->data.keyValuePair.value,
                                  ZR_FALSE,
                                 &left) ||
                         !reference_escape_validate_target(
                                 context,
                                 &left,
                                 ZR_SEMANTIC_ESCAPE_HEAP_STATIC,
                                 property->location,
                                 "container store"))) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_GENERATOR_EXPRESSION: {
            SZrReferenceEscapeContext child;
            if (!reference_escape_context_init(&child, context->compiler, context)) {
                return ZR_FALSE;
            }
            child.isGeneratorBody = ZR_TRUE;
            child.bodyRoot = node->data.generatorExpression.block;
            if (!reference_escape_analyze_node(
                        &child, node->data.generatorExpression.block)) {
                reference_escape_context_free(&child);
                return ZR_FALSE;
            }
            reference_escape_context_free(&child);
            return ZR_TRUE;
        }
        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (!reference_escape_analyze_expression(
                        context,
                        node->data.constructExpression.target,
                        ZR_FALSE,
                        &left)) {
                return ZR_FALSE;
            }
            return reference_escape_analyze_expression_array(
                    context, node->data.constructExpression.args);
        case ZR_AST_IF_EXPRESSION:
            return reference_escape_analyze_node(
                           context, node->data.ifExpression.condition) &&
                   reference_escape_analyze_node(
                           context, node->data.ifExpression.thenExpr) &&
                   reference_escape_analyze_node(
                           context, node->data.ifExpression.elseExpr);
        default:
            return ZR_TRUE;
    }
}
