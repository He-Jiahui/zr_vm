#include "compile_time_binding_metadata.h"

#include <string.h>

static const TZrChar *binding_string_or_empty(SZrString *value) {
    const TZrChar *text = value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
    return text != ZR_NULL ? text : "";
}

static TZrBool binding_make_string(SZrState *state, const TZrChar *text, SZrString **outString) {
    TZrSize length;

    if (state == ZR_NULL || outString == ZR_NULL) {
        return ZR_FALSE;
    }

    if (text == ZR_NULL) {
        text = "";
    }

    length = strlen(text);
    *outString = ZrCore_String_Create(state, (TZrNativeString)text, length);
    return *outString != ZR_NULL;
}

static TZrBool binding_join_path(SZrState *state, SZrString *prefix, SZrString *suffix, SZrString **outPath) {
    const TZrChar *prefixText;
    const TZrChar *suffixText;
    TZrSize prefixLength;
    TZrSize suffixLength;
    TZrSize totalLength;
    TZrChar *buffer;
    TZrBool ok;

    if (state == ZR_NULL || outPath == ZR_NULL) {
        return ZR_FALSE;
    }

    prefixText = binding_string_or_empty(prefix);
    suffixText = binding_string_or_empty(suffix);
    prefixLength = strlen(prefixText);
    suffixLength = strlen(suffixText);
    if (prefixLength == 0) {
        return binding_make_string(state, suffixText, outPath);
    }
    if (suffixLength == 0) {
        return binding_make_string(state, prefixText, outPath);
    }

    totalLength = prefixLength + 1 + suffixLength;
    buffer = (TZrChar *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        totalLength + 1,
                                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(buffer, prefixText, prefixLength);
    buffer[prefixLength] = '.';
    memcpy(buffer + prefixLength + 1, suffixText, suffixLength);
    buffer[totalLength] = '\0';
    ok = binding_make_string(state, buffer, outPath);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  buffer,
                                  totalLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    return ok;
}

static TZrBool binding_extract_property_name(SZrState *state, SZrAstNode *keyNode, SZrString **outName) {
    if (state == ZR_NULL || keyNode == ZR_NULL || outName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (keyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outName = keyNode->data.identifier.name;
        return *outName != ZR_NULL;
    }

    if (keyNode->type == ZR_AST_STRING_LITERAL) {
        *outName = keyNode->data.stringLiteral.value;
        return *outName != ZR_NULL;
    }

    *outName = ZR_NULL;
    return ZR_FALSE;
}

static TZrBool binding_append(SZrState *state,
                              SZrArray *bindings,
                              SZrString *path,
                              TZrUInt8 targetKind,
                              SZrString *targetName) {
    SZrFunctionCompileTimePathBinding binding;

    if (state == ZR_NULL || bindings == ZR_NULL || targetKind == ZR_COMPILE_TIME_BINDING_TARGET_NONE ||
        targetName == ZR_NULL) {
        return ZR_FALSE;
    }

    binding.path = path;
    binding.targetKind = targetKind;
    binding.targetName = targetName;
    ZrCore_Array_Push(state, bindings, &binding);
    return ZR_TRUE;
}

static TZrBool binding_append_prefixed_bindings(SZrCompileTimeBindingResolver *resolver,
                                                SZrArray *bindings,
                                                SZrString *prefix,
                                                const SZrFunctionCompileTimeVariableInfo *info) {
    if (resolver == ZR_NULL || bindings == ZR_NULL || info == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < info->pathBindingCount; index++) {
        const SZrFunctionCompileTimePathBinding *sourceBinding = &info->pathBindings[index];
        SZrString *joinedPath = ZR_NULL;

        if (!binding_join_path(resolver->state, prefix, sourceBinding->path, &joinedPath) ||
            !binding_append(resolver->state, bindings, joinedPath, sourceBinding->targetKind, sourceBinding->targetName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool binding_path_matches_prefix(SZrString *path, SZrString *prefix, SZrString **outSuffix, SZrState *state) {
    const TZrChar *pathText;
    const TZrChar *prefixText;
    TZrSize prefixLength;

    if (outSuffix == ZR_NULL || state == ZR_NULL) {
        return ZR_FALSE;
    }

    *outSuffix = ZR_NULL;
    pathText = binding_string_or_empty(path);
    prefixText = binding_string_or_empty(prefix);
    prefixLength = strlen(prefixText);
    if (prefixLength == 0) {
        return binding_make_string(state, pathText, outSuffix);
    }

    if (strcmp(pathText, prefixText) == 0) {
        return binding_make_string(state, "", outSuffix);
    }

    if (strncmp(pathText, prefixText, prefixLength) != 0 || pathText[prefixLength] != '.') {
        return ZR_FALSE;
    }

    return binding_make_string(state, pathText + prefixLength + 1, outSuffix);
}

static TZrBool binding_collect_from_node(SZrCompileTimeBindingResolver *resolver,
                                         SZrAstNode *node,
                                         SZrString *prefix,
                                         SZrArray *bindings);

static TZrBool binding_resolve_source_variable(SZrCompileTimeBindingResolver *resolver,
                                               SZrCompileTimeBindingSourceVariable *variable) {
    SZrArray collectedBindings;

    if (resolver == ZR_NULL || resolver->state == ZR_NULL || variable == ZR_NULL || variable->info == ZR_NULL) {
        return ZR_FALSE;
    }

    if (variable->isResolved) {
        return ZR_TRUE;
    }
    if (variable->isResolving) {
        return ZR_TRUE;
    }

    variable->isResolving = ZR_TRUE;
    variable->info->pathBindings = ZR_NULL;
    variable->info->pathBindingCount = 0;
    ZrCore_Array_Init(resolver->state,
                      &collectedBindings,
                      sizeof(SZrFunctionCompileTimePathBinding),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);

    if (variable->value != ZR_NULL &&
        !binding_collect_from_node(resolver, variable->value, ZR_NULL, &collectedBindings)) {
        if (collectedBindings.isValid && collectedBindings.head != ZR_NULL) {
            ZrCore_Array_Free(resolver->state, &collectedBindings);
        }
        variable->isResolving = ZR_FALSE;
        return ZR_FALSE;
    }

    if (collectedBindings.length > 0) {
        variable->info->pathBindings = (SZrFunctionCompileTimePathBinding *)ZrCore_Memory_RawMallocWithType(
                resolver->state->global,
                sizeof(SZrFunctionCompileTimePathBinding) * collectedBindings.length,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (variable->info->pathBindings == ZR_NULL) {
            ZrCore_Array_Free(resolver->state, &collectedBindings);
            variable->isResolving = ZR_FALSE;
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(variable->info->pathBindings,
                              collectedBindings.head,
                              sizeof(SZrFunctionCompileTimePathBinding) * collectedBindings.length);
        variable->info->pathBindingCount = (TZrUInt32)collectedBindings.length;
    }

    if (collectedBindings.isValid && collectedBindings.head != ZR_NULL) {
        ZrCore_Array_Free(resolver->state, &collectedBindings);
    }
    variable->isResolving = ZR_FALSE;
    variable->isResolved = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool binding_collect_from_identifier(SZrCompileTimeBindingResolver *resolver,
                                               SZrString *identifier,
                                               SZrString *prefix,
                                               SZrArray *bindings) {
    SZrCompileTimeBindingSourceVariable *variable;
    SZrCompileTimeFunction *functionInfo;
    SZrCompileTimeDecoratorClass *decoratorClass;

    if (resolver == ZR_NULL || identifier == ZR_NULL || bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    functionInfo = resolver->findFunction != ZR_NULL ? resolver->findFunction(resolver->userData, identifier) : ZR_NULL;
    if (functionInfo != ZR_NULL) {
        return binding_append(resolver->state,
                              bindings,
                              prefix,
                              ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION,
                              functionInfo->name);
    }

    decoratorClass =
            resolver->findDecoratorClass != ZR_NULL ? resolver->findDecoratorClass(resolver->userData, identifier)
                                                    : ZR_NULL;
    if (decoratorClass != ZR_NULL) {
        return binding_append(resolver->state,
                              bindings,
                              prefix,
                              ZR_COMPILE_TIME_BINDING_TARGET_DECORATOR_CLASS,
                              decoratorClass->name);
    }

    variable = resolver->findVariable != ZR_NULL ? resolver->findVariable(resolver->userData, identifier) : ZR_NULL;
    if (variable == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!binding_resolve_source_variable(resolver, variable)) {
        return ZR_FALSE;
    }

    return binding_append_prefixed_bindings(resolver, bindings, prefix, variable->info);
}

static TZrBool binding_collect_from_primary(SZrCompileTimeBindingResolver *resolver,
                                            SZrPrimaryExpression *primary,
                                            SZrString *prefix,
                                            SZrArray *bindings) {
    SZrString *rootName;
    SZrString *requestedPath = ZR_NULL;
    SZrCompileTimeBindingSourceVariable *variable;
    SZrCompileTimeFunction *functionInfo;
    SZrCompileTimeDecoratorClass *decoratorClass;

    if (resolver == ZR_NULL || primary == ZR_NULL || bindings == ZR_NULL || primary->property == ZR_NULL ||
        primary->property->type != ZR_AST_IDENTIFIER_LITERAL || primary->property->data.identifier.name == ZR_NULL) {
        return ZR_TRUE;
    }

    rootName = primary->property->data.identifier.name;
    if (!ZrParser_CompileTimeBinding_BuildStaticMemberPath(resolver->state,
                                                           primary->members,
                                                           0,
                                                           primary->members != ZR_NULL ? primary->members->count : 0,
                                                           &requestedPath)) {
        return ZR_TRUE;
    }

    functionInfo = resolver->findFunction != ZR_NULL ? resolver->findFunction(resolver->userData, rootName) : ZR_NULL;
    if (functionInfo != ZR_NULL) {
        if (strcmp(binding_string_or_empty(requestedPath), "") == 0) {
            return binding_append(resolver->state,
                                  bindings,
                                  prefix,
                                  ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION,
                                  functionInfo->name);
        }
        return ZR_TRUE;
    }

    decoratorClass = resolver->findDecoratorClass != ZR_NULL ? resolver->findDecoratorClass(resolver->userData, rootName)
                                                              : ZR_NULL;
    if (decoratorClass != ZR_NULL) {
        if (strcmp(binding_string_or_empty(requestedPath), "") == 0) {
            return binding_append(resolver->state,
                                  bindings,
                                  prefix,
                                  ZR_COMPILE_TIME_BINDING_TARGET_DECORATOR_CLASS,
                                  decoratorClass->name);
        }
        return ZR_TRUE;
    }

    variable = resolver->findVariable != ZR_NULL ? resolver->findVariable(resolver->userData, rootName) : ZR_NULL;
    if (variable == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!binding_resolve_source_variable(resolver, variable)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < variable->info->pathBindingCount; index++) {
        const SZrFunctionCompileTimePathBinding *sourceBinding = &variable->info->pathBindings[index];
        SZrString *suffix = ZR_NULL;
        SZrString *joinedPath = ZR_NULL;

        if (!binding_path_matches_prefix(sourceBinding->path, requestedPath, &suffix, resolver->state)) {
            continue;
        }

        if (!binding_join_path(resolver->state, prefix, suffix, &joinedPath) ||
            !binding_append(resolver->state,
                            bindings,
                            joinedPath,
                            sourceBinding->targetKind,
                            sourceBinding->targetName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool binding_collect_from_object_literal(SZrCompileTimeBindingResolver *resolver,
                                                   SZrObjectLiteral *objectLiteral,
                                                   SZrString *prefix,
                                                   SZrArray *bindings) {
    if (resolver == ZR_NULL || objectLiteral == ZR_NULL || bindings == ZR_NULL || objectLiteral->properties == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < objectLiteral->properties->count; index++) {
        SZrAstNode *propertyNode = objectLiteral->properties->nodes[index];
        SZrString *propertyName = ZR_NULL;
        SZrString *propertyPath = ZR_NULL;

        if (propertyNode == ZR_NULL || propertyNode->type != ZR_AST_KEY_VALUE_PAIR ||
            propertyNode->data.keyValuePair.value == ZR_NULL ||
            !binding_extract_property_name(resolver->state, propertyNode->data.keyValuePair.key, &propertyName)) {
            continue;
        }

        if (!binding_join_path(resolver->state, prefix, propertyName, &propertyPath) ||
            !binding_collect_from_node(resolver,
                                       propertyNode->data.keyValuePair.value,
                                       propertyPath,
                                       bindings)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool binding_collect_from_node(SZrCompileTimeBindingResolver *resolver,
                                         SZrAstNode *node,
                                         SZrString *prefix,
                                         SZrArray *bindings) {
    if (resolver == ZR_NULL || node == ZR_NULL || bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return binding_collect_from_identifier(resolver, node->data.identifier.name, prefix, bindings);
        case ZR_AST_PRIMARY_EXPRESSION:
            return binding_collect_from_primary(resolver, &node->data.primaryExpression, prefix, bindings);
        case ZR_AST_OBJECT_LITERAL:
            return binding_collect_from_object_literal(resolver, &node->data.objectLiteral, prefix, bindings);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_CompileTimeBinding_ResolveAll(SZrCompileTimeBindingResolver *resolver,
                                               SZrCompileTimeBindingSourceVariable *variables,
                                               TZrSize variableCount) {
    if (resolver == ZR_NULL || resolver->state == ZR_NULL || variables == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < variableCount; index++) {
        if (!binding_resolve_source_variable(resolver, &variables[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrParser_CompileTimeBinding_BuildStaticMemberPath(SZrState *state,
                                                          const SZrAstNodeArray *members,
                                                          TZrSize startIndex,
                                                          TZrSize endIndex,
                                                          SZrString **outPath) {
    TZrSize totalLength = 0;
    TZrChar *buffer;
    TZrSize cursor = 0;
    TZrBool wroteAny = ZR_FALSE;

    if (state == ZR_NULL || outPath == ZR_NULL) {
        return ZR_FALSE;
    }

    *outPath = ZR_NULL;
    if (members == ZR_NULL || startIndex >= endIndex || startIndex >= members->count) {
        return binding_make_string(state, "", outPath);
    }

    if (endIndex > members->count) {
        endIndex = members->count;
    }

    for (TZrSize index = startIndex; index < endIndex; index++) {
        SZrAstNode *memberNode = members->nodes[index];
        SZrMemberExpression *memberExpr;
        const TZrChar *memberName;

        if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
            return ZR_FALSE;
        }
        memberExpr = &memberNode->data.memberExpression;
        if (memberExpr->computed || memberExpr->property == ZR_NULL ||
            memberExpr->property->type != ZR_AST_IDENTIFIER_LITERAL ||
            memberExpr->property->data.identifier.name == ZR_NULL) {
            return ZR_FALSE;
        }
        memberName = binding_string_or_empty(memberExpr->property->data.identifier.name);
        totalLength += strlen(memberName);
        if (wroteAny) {
            totalLength += 1;
        }
        wroteAny = ZR_TRUE;
    }

    if (!wroteAny) {
        return binding_make_string(state, "", outPath);
    }

    buffer = (TZrChar *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        totalLength + 1,
                                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = startIndex; index < endIndex; index++) {
        SZrAstNode *memberNode = members->nodes[index];
        const TZrChar *memberName = binding_string_or_empty(memberNode->data.memberExpression.property->data.identifier.name);
        TZrSize memberLength = strlen(memberName);

        if (cursor > 0) {
            buffer[cursor++] = '.';
        }
        memcpy(buffer + cursor, memberName, memberLength);
        cursor += memberLength;
    }
    buffer[cursor] = '\0';

    wroteAny = binding_make_string(state, buffer, outPath);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  buffer,
                                  totalLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    return wroteAny;
}

const SZrFunctionCompileTimePathBinding *ZrParser_CompileTimeBinding_FindPath(
        const SZrFunctionCompileTimeVariableInfo *info,
        SZrString *path) {
    if (info == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < info->pathBindingCount; index++) {
        const SZrFunctionCompileTimePathBinding *binding = &info->pathBindings[index];
        if (binding->path != ZR_NULL && ZrCore_String_Equal(binding->path, path)) {
            return binding;
        }
    }

    return ZR_NULL;
}
