#include "zr_vm_parser/const_assignment.h"

#include <stdio.h>
#include <string.h>

static TZrBool const_assignment_range_contains(const SZrFileRange *outer,
                                               const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        (outer->source != ZR_NULL && inner->source != ZR_NULL &&
         !ZrCore_String_Equal(outer->source, inner->source))) {
        return ZR_FALSE;
    }
    if (outer->end.offset > outer->start.offset &&
        inner->end.offset >= inner->start.offset) {
        return outer->start.offset <= inner->start.offset &&
               inner->end.offset <= outer->end.offset;
    }
    if (outer->start.line <= 0 || outer->end.line <= 0 ||
        inner->start.line <= 0 || inner->end.line <= 0) {
        return ZR_FALSE;
    }
    return (outer->start.line < inner->start.line ||
            (outer->start.line == inner->start.line &&
             outer->start.column <= inner->start.column)) &&
           (inner->end.line < outer->end.line ||
            (inner->end.line == outer->end.line &&
             inner->end.column <= outer->end.column));
}

static TZrBool const_assignment_string_equals(SZrString *value,
                                              const TZrChar *expected) {
    const TZrChar *text;

    if (value == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL && strcmp(text, expected) == 0;
}

static SZrAstNodeArray *const_assignment_owner_members(const SZrAstNode *owner) {
    if (owner == ZR_NULL) {
        return ZR_NULL;
    }
    if (owner->type == ZR_AST_CLASS_DECLARATION) {
        return owner->data.classDeclaration.members;
    }
    if (owner->type == ZR_AST_STRUCT_DECLARATION) {
        return owner->data.structDeclaration.members;
    }
    return ZR_NULL;
}

static const SZrAstNode *const_assignment_find_field_owner(
        const SZrAstNode *moduleRoot,
        const SZrAstNode *targetDeclaration) {
    SZrAstNodeArray *statements;

    if (moduleRoot == ZR_NULL || targetDeclaration == ZR_NULL) {
        return ZR_NULL;
    }
    if (moduleRoot->type == ZR_AST_CLASS_DECLARATION ||
        moduleRoot->type == ZR_AST_STRUCT_DECLARATION) {
        statements = const_assignment_owner_members(moduleRoot);
        for (TZrSize index = 0U;
             statements != ZR_NULL && index < statements->count;
             index++) {
            if (statements->nodes[index] == targetDeclaration) {
                return moduleRoot;
            }
        }
        return ZR_NULL;
    }
    if (moduleRoot->type != ZR_AST_SCRIPT ||
        moduleRoot->data.script.statements == ZR_NULL) {
        return ZR_NULL;
    }

    statements = moduleRoot->data.script.statements;
    for (TZrSize statementIndex = 0U;
         statementIndex < statements->count;
         statementIndex++) {
        const SZrAstNode *candidateOwner = statements->nodes[statementIndex];
        SZrAstNodeArray *members = const_assignment_owner_members(candidateOwner);
        for (TZrSize memberIndex = 0U;
             members != ZR_NULL && memberIndex < members->count;
             memberIndex++) {
            if (members->nodes[memberIndex] == targetDeclaration) {
                return candidateOwner;
            }
        }
    }
    return ZR_NULL;
}

static TZrBool const_assignment_is_constructor(const SZrAstNode *member) {
    if (member == ZR_NULL) {
        return ZR_FALSE;
    }
    if (member->type == ZR_AST_CLASS_META_FUNCTION) {
        return member->data.classMetaFunction.meta != ZR_NULL &&
               const_assignment_string_equals(
                       member->data.classMetaFunction.meta->name,
                       "constructor");
    }
    if (member->type == ZR_AST_STRUCT_META_FUNCTION) {
        return member->data.structMetaFunction.meta != ZR_NULL &&
               const_assignment_string_equals(
                       member->data.structMetaFunction.meta->name,
                       "constructor");
    }
    return ZR_FALSE;
}

static TZrBool const_assignment_is_inside_owner_constructor(
        const SZrAstNode *owner,
        const SZrAstNode *assignment) {
    SZrAstNodeArray *members = const_assignment_owner_members(owner);

    for (TZrSize index = 0U;
         members != ZR_NULL && assignment != ZR_NULL && index < members->count;
         index++) {
        const SZrAstNode *member = members->nodes[index];
        if (const_assignment_is_constructor(member) &&
            const_assignment_range_contains(&member->location,
                                            &assignment->location)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool const_assignment_targets_current_instance(
        const SZrAstNode *assignment) {
    const SZrAstNode *left;
    const SZrPrimaryExpression *primary;

    if (assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_FALSE;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }
    if (left->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &left->data.primaryExpression;
    return primary->property != ZR_NULL &&
           primary->property->type == ZR_AST_IDENTIFIER_LITERAL &&
            const_assignment_string_equals(
                    primary->property->data.identifier.name,
                    "this");
}

static SZrString *const_assignment_target_name(const SZrAstNode *assignment) {
    const SZrAstNode *left;
    const SZrPrimaryExpression *primary;
    const SZrAstNode *lastMember;
    const SZrAstNode *property;

    if (assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_NULL;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        return left->data.identifier.name;
    }
    if (left->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_NULL;
    }

    primary = &left->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count == 0U) {
        return ZR_NULL;
    }
    lastMember = primary->members->nodes[primary->members->count - 1U];
    if (lastMember == ZR_NULL ||
        lastMember->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_NULL;
    }
    property = lastMember->data.memberExpression.property;
    return property != ZR_NULL && property->type == ZR_AST_IDENTIFIER_LITERAL
                   ? property->data.identifier.name
                   : ZR_NULL;
}

static TZrBool const_assignment_context_receiver_kind(
        const SZrTypePrototypeInfo *prototype,
        const SZrAstNode *assignment,
        TZrBool *outIsStatic) {
    const SZrAstNode *left;
    const SZrAstNode *receiver;

    if (prototype == ZR_NULL || outIsStatic == ZR_NULL ||
        assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_FALSE;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    receiver = left->data.primaryExpression.property;
    if (receiver == ZR_NULL ||
        receiver->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    if (const_assignment_string_equals(
                receiver->data.identifier.name, "this")) {
        *outIsStatic = ZR_FALSE;
        return ZR_TRUE;
    }
    if (prototype->name != ZR_NULL &&
        receiver->data.identifier.name != ZR_NULL &&
        ZrCore_String_Equal(
                prototype->name, receiver->data.identifier.name)) {
        *outIsStatic = ZR_TRUE;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static const SZrAstNode *const_assignment_resolve_context_field(
        const SZrCompilerState *compilerState,
        const SZrAstNode *assignment) {
    const SZrTypePrototypeInfo *prototype;
    SZrString *targetName;
    const SZrAstNode *resolvedDeclaration = ZR_NULL;
    TZrBool targetIsStatic;

    if (compilerState == ZR_NULL ||
        compilerState->currentTypePrototypeInfo == ZR_NULL) {
        return ZR_NULL;
    }
    prototype = compilerState->currentTypePrototypeInfo;
    if (!const_assignment_context_receiver_kind(
                prototype, assignment, &targetIsStatic)) {
        return ZR_NULL;
    }
    targetName = const_assignment_target_name(assignment);
    if (targetName == ZR_NULL || !prototype->members.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members, index);
        if (member == ZR_NULL || member->declarationNode == ZR_NULL ||
            (member->memberType != ZR_AST_CLASS_FIELD &&
             member->memberType != ZR_AST_STRUCT_FIELD) ||
            member->isStatic != targetIsStatic ||
            member->name == ZR_NULL ||
            !ZrCore_String_Equal(member->name, targetName)) {
            continue;
        }
        if (resolvedDeclaration != ZR_NULL &&
            resolvedDeclaration != member->declarationNode) {
            return ZR_NULL;
        }
        resolvedDeclaration = member->declarationNode;
    }
    return resolvedDeclaration;
}

TZrBool ZrParser_ConstAssignment_DescribeTarget(
        const SZrAstNode *targetDeclaration,
        SZrConstAssignmentResult *outResult) {
    if (targetDeclaration == ZR_NULL || outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outResult, 0, sizeof(*outResult));
    outResult->declarationRange = targetDeclaration->location;
    switch (targetDeclaration->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            if (targetDeclaration->data.variableDeclaration.pattern == ZR_NULL ||
                targetDeclaration->data.variableDeclaration.pattern->type !=
                        ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_FALSE;
            }
            outResult->targetKind = ZR_CONST_ASSIGNMENT_TARGET_LOCAL;
            outResult->targetName = targetDeclaration->data.variableDeclaration
                                            .pattern->data.identifier.name;
            outResult->declarationRange =
                    targetDeclaration->data.variableDeclaration.pattern->location;
            outResult->isConstTarget =
                    targetDeclaration->data.variableDeclaration.isConst;
            break;

        case ZR_AST_PARAMETER:
            outResult->targetKind = ZR_CONST_ASSIGNMENT_TARGET_PARAMETER;
            outResult->targetName = targetDeclaration->data.parameter.name != ZR_NULL
                                            ? targetDeclaration->data.parameter.name->name
                                            : ZR_NULL;
            outResult->declarationRange =
                    targetDeclaration->data.parameter.nameLocation;
            outResult->isConstTarget = targetDeclaration->data.parameter.isConst;
            break;

        case ZR_AST_CLASS_FIELD:
            outResult->targetKind = targetDeclaration->data.classField.isStatic
                                            ? ZR_CONST_ASSIGNMENT_TARGET_STATIC_FIELD
                                            : ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD;
            outResult->targetName = targetDeclaration->data.classField.name != ZR_NULL
                                            ? targetDeclaration->data.classField.name->name
                                            : ZR_NULL;
            outResult->declarationRange =
                    targetDeclaration->data.classField.nameLocation;
            outResult->isConstTarget = targetDeclaration->data.classField.isConst;
            break;

        case ZR_AST_STRUCT_FIELD:
            outResult->targetKind = targetDeclaration->data.structField.isStatic
                                            ? ZR_CONST_ASSIGNMENT_TARGET_STATIC_FIELD
                                            : ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD;
            outResult->targetName = targetDeclaration->data.structField.name != ZR_NULL
                                            ? targetDeclaration->data.structField.name->name
                                            : ZR_NULL;
            outResult->isConstTarget = targetDeclaration->data.structField.isConst;
            break;

        default:
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ConstAssignment_Evaluate(
        const SZrAstNode *moduleRoot,
        const SZrAstNode *assignment,
        const SZrAstNode *targetDeclaration,
        SZrConstAssignmentResult *outResult) {
    const SZrAstNode *owner;

    if (assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        !ZrParser_ConstAssignment_DescribeTarget(
                targetDeclaration, outResult)) {
        return ZR_FALSE;
    }

    outResult->assignmentRange = assignment->location;
    if (!outResult->isConstTarget) {
        return ZR_TRUE;
    }
    if (outResult->targetKind !=
            ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD) {
        outResult->isViolation = ZR_TRUE;
        return ZR_TRUE;
    }

    owner = const_assignment_find_field_owner(moduleRoot, targetDeclaration);
    outResult->isViolation =
            owner == ZR_NULL ||
            !const_assignment_targets_current_instance(assignment) ||
            !const_assignment_is_inside_owner_constructor(owner, assignment);
    return ZR_TRUE;
}

TZrBool ZrParser_ConstAssignment_EvaluateContext(
        const SZrCompilerState *compilerState,
        const SZrAstNode *moduleRoot,
        const SZrAstNode *assignment,
        const SZrAstNode *resolvedTargetDeclaration,
        SZrConstAssignmentResult *outResult) {
    const SZrAstNode *targetDeclaration = resolvedTargetDeclaration;

    if (targetDeclaration == ZR_NULL) {
        targetDeclaration = const_assignment_resolve_context_field(
                compilerState, assignment);
    }
    return ZrParser_ConstAssignment_Evaluate(
            moduleRoot, assignment, targetDeclaration, outResult);
}

TZrBool ZrParser_ConstAssignment_BuildDiagnostic(
        SZrState *state,
        const SZrConstAssignmentResult *result,
        SZrStructuredDiagnostic *outDiagnostic) {
    TZrChar message[192];
    const TZrChar *name;
    const TZrChar *cause;
    const TZrChar *suggestion;

    if (state == ZR_NULL || result == ZR_NULL || outDiagnostic == ZR_NULL ||
        !result->isConstTarget || !result->isViolation) {
        return ZR_FALSE;
    }

    name = result->targetName != ZR_NULL
                   ? ZrCore_String_GetNativeString(result->targetName)
                   : ZR_NULL;
    switch (result->targetKind) {
        case ZR_CONST_ASSIGNMENT_TARGET_PARAMETER:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to const parameter '%s'"
                             : "Cannot assign to const parameter",
                     name != ZR_NULL ? name : "");
            cause = "A const parameter is immutable for the entire callable body.";
            suggestion = "Pass a different value or introduce a mutable local copy.";
            break;

        case ZR_CONST_ASSIGNMENT_TARGET_LOCAL:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to const variable '%s' after declaration"
                             : "Cannot assign to const variable after declaration",
                     name != ZR_NULL ? name : "");
            cause = "A const local is initialized by its declaration and cannot be assigned again.";
            suggestion = "Use a mutable variable when later assignment is required.";
            break;

        case ZR_CONST_ASSIGNMENT_TARGET_STATIC_FIELD:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to const static field '%s'"
                             : "Cannot assign to const static field",
                     name != ZR_NULL ? name : "");
            cause = "A const static field cannot be mutated after its declaration initializer.";
            suggestion = "Keep the field immutable or redesign the state as an explicitly mutable static field.";
            break;

        case ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to immutable field '%s' outside initialization"
                             : "Cannot assign to immutable field outside initialization",
                     name != ZR_NULL ? name : "");
            cause = "An immutable instance field can only initialize the current instance inside its declaring type's constructor.";
            suggestion = "Move the assignment into the constructor or make the field mutable.";
            break;

        default:
            return ZR_FALSE;
    }

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                outDiagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                result->assignmentRange,
                "const_assignment",
                message,
                cause,
                suggestion) ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                outDiagnostic,
                result->declarationRange,
                "Immutable declaration is here") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                outDiagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(state, outDiagnostic);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
