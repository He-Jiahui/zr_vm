#include "compiler_out_definite_assignment_internal.h"

#include <stdlib.h>
#include <string.h>

const TZrChar *out_string_native(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }
    return ZrCore_String_GetNativeString(value);
}

static SZrString *out_type_name(const SZrType *typeInfo) {
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

static SZrAstNodeArray *out_struct_members(
        const SZrCompilerState *cs,
        SZrString *typeName) {
    SZrAstNodeArray *statements;

    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || typeName == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    statements = cs->scriptAst->data.script.statements;
    if (statements == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < statements->count; index++) {
        SZrAstNode *node = statements->nodes[index];
        if (node != ZR_NULL && node->type == ZR_AST_STRUCT_DECLARATION &&
            node->data.structDeclaration.name != ZR_NULL &&
            node->data.structDeclaration.name->name != ZR_NULL &&
            ZrCore_String_Equal(
                    node->data.structDeclaration.name->name, typeName)) {
            return node->data.structDeclaration.members;
        }
    }
    return ZR_NULL;
}

static TZrBool out_parameter_collect_fields(
        const SZrCompilerState *cs,
        const SZrParameter *parameter,
        SZrOutParameterInfo *info) {
    SZrAstNodeArray *members = out_struct_members(
            cs, out_type_name(parameter != ZR_NULL ? parameter->typeInfo : ZR_NULL));

    if (members == ZR_NULL) {
        info->slotCount = 1u;
        return ZR_TRUE;
    }
    for (TZrSize index = 0u; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        if (member != ZR_NULL && member->type == ZR_AST_STRUCT_FIELD &&
            !member->data.structField.isStatic &&
            member->data.structField.name != ZR_NULL &&
            member->data.structField.name->name != ZR_NULL) {
            info->fieldCount++;
        }
    }
    if (info->fieldCount == 0u) {
        info->slotCount = 1u;
        return ZR_TRUE;
    }
    info->fieldNames = (SZrString **)calloc(
            info->fieldCount, sizeof(SZrString *));
    if (info->fieldNames == ZR_NULL) {
        return ZR_FALSE;
    }
    info->fieldCount = 0u;
    for (TZrSize index = 0u; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        if (member != ZR_NULL && member->type == ZR_AST_STRUCT_FIELD &&
            !member->data.structField.isStatic &&
            member->data.structField.name != ZR_NULL &&
            member->data.structField.name->name != ZR_NULL) {
            info->fieldNames[info->fieldCount++] =
                    member->data.structField.name->name;
        }
    }
    info->slotCount = info->fieldCount;
    return ZR_TRUE;
}

static void out_tracked_free(SZrOutTrackedState *tracked) {
    if (tracked == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < tracked->parameterCount; index++) {
        free(tracked->parameters[index].fieldNames);
    }
    free(tracked->parameters);
    memset(tracked, 0, sizeof(*tracked));
}

static TZrBool out_tracked_init(
        SZrCompilerState *cs,
        SZrAstNodeArray *params,
        SZrOutTrackedState *tracked) {
    memset(tracked, 0, sizeof(*tracked));
    if (params == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0u; index < params->count; index++) {
        SZrAstNode *node = params->nodes[index];
        if (node != ZR_NULL && node->type == ZR_AST_PARAMETER &&
            node->data.parameter.passingMode == ZR_PARAMETER_PASSING_MODE_OUT &&
            node->data.parameter.name != ZR_NULL &&
            node->data.parameter.name->name != ZR_NULL) {
            tracked->parameterCount++;
        }
    }
    if (tracked->parameterCount == 0u) {
        return ZR_TRUE;
    }
    tracked->parameters = (SZrOutParameterInfo *)calloc(
            tracked->parameterCount, sizeof(SZrOutParameterInfo));
    if (tracked->parameters == ZR_NULL) {
        return ZR_FALSE;
    }
    tracked->parameterCount = 0u;
    for (TZrSize index = 0u; index < params->count; index++) {
        SZrAstNode *node = params->nodes[index];
        SZrOutParameterInfo *info;
        if (node == ZR_NULL || node->type != ZR_AST_PARAMETER ||
            node->data.parameter.passingMode != ZR_PARAMETER_PASSING_MODE_OUT ||
            node->data.parameter.name == ZR_NULL ||
            node->data.parameter.name->name == ZR_NULL) {
            continue;
        }
        info = &tracked->parameters[tracked->parameterCount++];
        info->name = node->data.parameter.name->name;
        info->slotOffset = tracked->slotCount;
        if (!out_parameter_collect_fields(cs, &node->data.parameter, info)) {
            out_tracked_free(tracked);
            return ZR_FALSE;
        }
        tracked->slotCount += info->slotCount;
    }
    return ZR_TRUE;
}

TZrBool *out_state_new(const SZrOutTrackedState *tracked) {
    if (tracked == ZR_NULL || tracked->slotCount == 0u) {
        return ZR_NULL;
    }
    return (TZrBool *)calloc(tracked->slotCount, sizeof(TZrBool));
}

void out_state_copy(
        const SZrOutTrackedState *tracked,
        const TZrBool *source,
        TZrBool *destination) {
    memcpy(destination, source, sizeof(TZrBool) * tracked->slotCount);
}

void out_state_intersect(
        const SZrOutTrackedState *tracked,
        TZrBool *destination,
        const TZrBool *other) {
    for (TZrSize index = 0u; index < tracked->slotCount; index++) {
        destination[index] = destination[index] && other[index];
    }
}

static TZrBool out_parameter_complete(
        const SZrOutParameterInfo *parameter,
        const TZrBool *state) {
    for (TZrSize index = 0u; index < parameter->slotCount; index++) {
        if (!state[parameter->slotOffset + index]) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool out_report_incomplete(
        SZrCompilerState *cs,
        const SZrOutTrackedState *tracked,
        const TZrBool *state,
        SZrFileRange location) {
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    for (TZrSize index = 0u; index < tracked->parameterCount; index++) {
        const SZrOutParameterInfo *parameter = &tracked->parameters[index];
        if (out_parameter_complete(parameter, state)) {
            continue;
        }
        if (parameter->fieldCount > 0u) {
            for (TZrSize fieldIndex = 0u;
                 fieldIndex < parameter->fieldCount;
                 fieldIndex++) {
                if (!state[parameter->slotOffset + fieldIndex]) {
                    snprintf(message,
                             sizeof(message),
                             "out parameter '%s' field '%s' must be assigned on every normal return path",
                             out_string_native(parameter->name),
                             out_string_native(parameter->fieldNames[fieldIndex]));
                    ZrParser_Compiler_Error(cs, message, location);
                    return ZR_FALSE;
                }
            }
        }
        snprintf(message,
                 sizeof(message),
                 "out parameter '%s' must be assigned on every normal return path",
                 out_string_native(parameter->name));
        ZrParser_Compiler_Error(cs, message, location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static SZrString *out_identifier_name(const SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }
    return node->data.identifier.name;
}

static SZrString *out_member_name(const SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_MEMBER_EXPRESSION ||
        node->data.memberExpression.computed) {
        return ZR_NULL;
    }
    return out_identifier_name(node->data.memberExpression.property);
}

static TZrInt32 out_parameter_index(
        const SZrOutTrackedState *tracked,
        SZrString *name) {
    if (name == ZR_NULL) {
        return ZR_PARSER_I32_NONE;
    }
    for (TZrSize index = 0u; index < tracked->parameterCount; index++) {
        if (ZrCore_String_Equal(tracked->parameters[index].name, name)) {
            return (TZrInt32)index;
        }
    }
    return ZR_PARSER_I32_NONE;
}

static TZrInt32 out_field_index(
        const SZrOutParameterInfo *parameter,
        SZrString *name) {
    if (name == ZR_NULL) {
        return ZR_PARSER_I32_NONE;
    }
    for (TZrSize index = 0u; index < parameter->fieldCount; index++) {
        if (ZrCore_String_Equal(parameter->fieldNames[index], name)) {
            return (TZrInt32)index;
        }
    }
    return ZR_PARSER_I32_NONE;
}

SZrOutPlaceRef out_resolve_place(
        const SZrOutTrackedState *tracked,
        const SZrAstNode *node) {
    SZrOutPlaceRef result = {ZR_PARSER_I32_NONE, ZR_PARSER_I32_NONE};
    SZrString *rootName = out_identifier_name(node);

    if (rootName != ZR_NULL) {
        result.parameterIndex = out_parameter_index(tracked, rootName);
        return result;
    }
    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return result;
    }
    rootName = out_identifier_name(node->data.primaryExpression.property);
    result.parameterIndex = out_parameter_index(tracked, rootName);
    if (result.parameterIndex == ZR_PARSER_I32_NONE ||
        node->data.primaryExpression.members == ZR_NULL ||
        node->data.primaryExpression.members->count == 0u) {
        return result;
    }
    if (node->data.primaryExpression.members->count == 1u) {
        SZrString *fieldName = out_member_name(
                node->data.primaryExpression.members->nodes[0]);
        result.fieldIndex = out_field_index(
                &tracked->parameters[result.parameterIndex], fieldName);
    }
    return result;
}

void out_mark_place(
        const SZrOutTrackedState *tracked,
        TZrBool *state,
        SZrOutPlaceRef place) {
    const SZrOutParameterInfo *parameter;
    if (place.parameterIndex == ZR_PARSER_I32_NONE) {
        return;
    }
    parameter = &tracked->parameters[place.parameterIndex];
    if (place.fieldIndex != ZR_PARSER_I32_NONE) {
        state[parameter->slotOffset + (TZrSize)place.fieldIndex] = ZR_TRUE;
        return;
    }
    for (TZrSize index = 0u; index < parameter->slotCount; index++) {
        state[parameter->slotOffset + index] = ZR_TRUE;
    }
}

TZrBool out_place_initialized(
        const SZrOutTrackedState *tracked,
        const TZrBool *state,
        SZrOutPlaceRef place) {
    const SZrOutParameterInfo *parameter;
    if (place.parameterIndex == ZR_PARSER_I32_NONE) {
        return ZR_TRUE;
    }
    parameter = &tracked->parameters[place.parameterIndex];
    if (place.fieldIndex != ZR_PARSER_I32_NONE) {
        return state[parameter->slotOffset + (TZrSize)place.fieldIndex];
    }
    return out_parameter_complete(parameter, state);
}

TZrBool compiler_validate_out_parameter_definite_assignment(
        SZrCompilerState *cs,
        SZrAstNodeArray *params,
        SZrAstNode *body,
        SZrFileRange fallbackLocation) {
    SZrOutTrackedState tracked;
    SZrOutFlowAnalysis analysis;
    TZrBool *before;
    TZrBool *after;
    TZrBool continues = ZR_TRUE;
    TZrBool ok;

    if (cs == ZR_NULL || !out_tracked_init(cs, params, &tracked)) {
        return ZR_FALSE;
    }
    if (tracked.parameterCount == 0u) {
        out_tracked_free(&tracked);
        return ZR_TRUE;
    }
    before = out_state_new(&tracked);
    after = out_state_new(&tracked);
    if (before == ZR_NULL || after == ZR_NULL) {
        free(before);
        free(after);
        out_tracked_free(&tracked);
        return ZR_FALSE;
    }
    memset(&analysis, 0, sizeof(analysis));
    analysis.compiler = cs;
    analysis.tracked = &tracked;
    ok = out_analyze_statement(
            &analysis, body, before, after, &continues);
    if (ok && continues) {
        ok = out_report_incomplete(
                cs, &tracked, after, fallbackLocation);
    }
    free(before);
    free(after);
    out_tracked_free(&tracked);
    return ok && !cs->hasError;
}


