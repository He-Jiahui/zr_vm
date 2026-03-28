//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

// 辅助函数：获取类型名称字符串（用于错误报告）
static const TChar *get_base_type_name(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            return "int";
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return "uint";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        case ZR_VALUE_TYPE_OBJECT:
            return "object";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return "function";
        default:
            return "unknown";
    }
}

static void free_inferred_type_array(SZrState *state, SZrArray *types) {
    if (state == ZR_NULL || types == ZR_NULL) {
        return;
    }

    if (types->isValid && types->head != ZR_NULL && types->capacity > 0 && types->elementSize > 0) {
        for (TZrSize i = 0; i < types->length; i++) {
            SZrInferredType *type = (SZrInferredType *)ZrArrayGet(types, i);
            if (type != ZR_NULL) {
                ZrInferredTypeFree(state, type);
            }
        }
        ZrArrayFree(state, types);
    }
}

static TBool append_text_fragment(TChar *buffer,
                                  TZrSize bufferSize,
                                  TZrSize *offset,
                                  const TChar *fragment) {
    TZrSize fragmentLength;

    if (buffer == ZR_NULL || offset == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    fragmentLength = strlen(fragment);
    if (*offset + fragmentLength + 1 >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *offset, fragment, fragmentLength);
    *offset += fragmentLength;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

static TBool append_inferred_type_display_name(const SZrInferredType *type,
                                               TChar *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *offset) {
    const TChar *baseName;

    if (type == ZR_NULL) {
        return append_text_fragment(buffer, bufferSize, offset, "object");
    }

    if (type->typeName != ZR_NULL) {
        return append_text_fragment(buffer,
                                    bufferSize,
                                    offset,
                                    ZrStringGetNativeString(type->typeName));
    }

    if (type->baseType == ZR_VALUE_TYPE_ARRAY && type->elementTypes.length > 0) {
        SZrInferredType *elementType = (SZrInferredType *)ZrArrayGet((SZrArray *)&type->elementTypes, 0);
        if (!append_inferred_type_display_name(elementType, buffer, bufferSize, offset)) {
            return ZR_FALSE;
        }
        return append_text_fragment(buffer, bufferSize, offset, "[]");
    }

    baseName = get_base_type_name(type->baseType);
    if (baseName == ZR_NULL) {
        baseName = "object";
    }
    return append_text_fragment(buffer, bufferSize, offset, baseName);
}

static SZrString *build_generic_instance_name(SZrState *state,
                                              SZrString *baseName,
                                              const SZrArray *typeArguments) {
    TChar buffer[512];
    TZrSize offset = 0;

    if (state == ZR_NULL || baseName == ZR_NULL || typeArguments == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!append_text_fragment(buffer, sizeof(buffer), &offset, ZrStringGetNativeString(baseName)) ||
        !append_text_fragment(buffer, sizeof(buffer), &offset, "<")) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < typeArguments->length; i++) {
        SZrInferredType *argumentType = (SZrInferredType *)ZrArrayGet((SZrArray *)typeArguments, i);
        if (i > 0 && !append_text_fragment(buffer, sizeof(buffer), &offset, ", ")) {
            return ZR_NULL;
        }
        if (!append_inferred_type_display_name(argumentType, buffer, sizeof(buffer), &offset)) {
            return ZR_NULL;
        }
    }

    if (!append_text_fragment(buffer, sizeof(buffer), &offset, ">")) {
        return ZR_NULL;
    }

    return ZrStringCreate(state, buffer, offset);
}

static TBool infer_function_call_argument_types(SZrCompilerState *cs,
                                                SZrAstNodeArray *args,
                                                SZrArray *argTypes) {
    if (cs == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrArrayConstruct(argTypes);
    if (args == ZR_NULL || args->count == 0) {
        return ZR_TRUE;
    }

    ZrArrayInit(cs->state, argTypes, sizeof(SZrInferredType), args->count);
    for (TZrSize i = 0; i < args->count; i++) {
        SZrAstNode *argNode = args->nodes[i];
        SZrInferredType argType;

        ZrInferredTypeInit(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
        if (argNode == ZR_NULL || !infer_expression_type(cs, argNode, &argType)) {
            ZrInferredTypeFree(cs->state, &argType);
            free_inferred_type_array(cs->state, argTypes);
            ZrArrayConstruct(argTypes);
            return ZR_FALSE;
        }

        ZrArrayPush(cs->state, argTypes, &argType);
    }

    return ZR_TRUE;
}

static TBool function_declaration_matches_candidate(SZrCompilerState *cs,
                                                    SZrAstNode *declNode,
                                                    const SZrFunctionTypeInfo *funcType) {
    SZrFunctionDeclaration *decl;
    SZrInferredType returnType;

    if (cs == ZR_NULL || declNode == ZR_NULL || funcType == ZR_NULL ||
        declNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &declNode->data.functionDeclaration;
    if (decl->params == ZR_NULL) {
        return funcType->paramTypes.length == 0;
    }

    if (decl->params->count != funcType->paramTypes.length) {
        return ZR_FALSE;
    }

    ZrInferredTypeInit(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
    if (decl->returnType != ZR_NULL) {
        if (!convert_ast_type_to_inferred_type(cs, decl->returnType, &returnType)) {
            ZrInferredTypeFree(cs->state, &returnType);
            return ZR_FALSE;
        }
    }

    if (!ZrInferredTypeEqual(&returnType, &funcType->returnType)) {
        ZrInferredTypeFree(cs->state, &returnType);
        return ZR_FALSE;
    }
    ZrInferredTypeFree(cs->state, &returnType);

    for (TZrSize i = 0; i < decl->params->count; i++) {
        SZrAstNode *paramNode = decl->params->nodes[i];
        SZrParameter *param;
        SZrInferredType paramType;
        SZrInferredType *expectedType;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            return ZR_FALSE;
        }

        param = &paramNode->data.parameter;
        ZrInferredTypeInit(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
        if (param->typeInfo != ZR_NULL) {
            if (!convert_ast_type_to_inferred_type(cs, param->typeInfo, &paramType)) {
                ZrInferredTypeFree(cs->state, &paramType);
                return ZR_FALSE;
            }
        }

        expectedType = (SZrInferredType *) ZrArrayGet((SZrArray *) &funcType->paramTypes, i);
        if (expectedType == ZR_NULL || !ZrInferredTypeEqual(&paramType, expectedType)) {
            ZrInferredTypeFree(cs->state, &paramType);
            return ZR_FALSE;
        }

        ZrInferredTypeFree(cs->state, &paramType);
    }

    return ZR_TRUE;
}

static SZrAstNode *find_function_declaration_for_candidate(SZrCompilerState *cs,
                                                           SZrTypeEnvironment *env,
                                                           SZrString *funcName,
                                                           const SZrFunctionTypeInfo *funcType) {
    if (cs == ZR_NULL || env == ZR_NULL || funcName == ZR_NULL || funcType == ZR_NULL) {
        return ZR_NULL;
    }

    if (env == cs->compileTimeTypeEnv) {
        for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
            SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **) ZrArrayGet(&cs->compileTimeFunctions, i);
            if (funcPtr == ZR_NULL || *funcPtr == ZR_NULL || (*funcPtr)->name == ZR_NULL ||
                !ZrStringEqual((*funcPtr)->name, funcName)) {
                continue;
            }

            if (function_declaration_matches_candidate(cs, (*funcPtr)->declaration, funcType)) {
                return (*funcPtr)->declaration;
            }
        }
        return ZR_NULL;
    }

    if (cs->scriptAst == ZR_NULL || cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }

    if (cs->scriptAst->data.script.statements == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < cs->scriptAst->data.script.statements->count; i++) {
        SZrAstNode *stmt = cs->scriptAst->data.script.statements->nodes[i];
        SZrFunctionDeclaration *decl;

        if (stmt == ZR_NULL || stmt->type != ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }

        decl = &stmt->data.functionDeclaration;
        if (decl->name == ZR_NULL || decl->name->name == ZR_NULL ||
            !ZrStringEqual(decl->name->name, funcName)) {
            continue;
        }

        if (function_declaration_matches_candidate(cs, stmt, funcType)) {
            return stmt;
        }
    }

    return ZR_NULL;
}

static TBool infer_call_argument_type_node(SZrCompilerState *cs,
                                           SZrAstNode *argNode,
                                           SZrArray *argTypes) {
    SZrInferredType argType;

    if (cs == ZR_NULL || argNode == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrInferredTypeInit(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
    if (!infer_expression_type(cs, argNode, &argType)) {
        ZrInferredTypeFree(cs->state, &argType);
        return ZR_FALSE;
    }

    ZrArrayPush(cs->state, argTypes, &argType);
    return ZR_TRUE;
}

static TBool infer_function_call_argument_types_for_candidate(SZrCompilerState *cs,
                                                              SZrTypeEnvironment *env,
                                                              SZrString *funcName,
                                                              SZrFunctionCall *call,
                                                              const SZrFunctionTypeInfo *funcType,
                                                              SZrArray *argTypes,
                                                              TBool *mismatch) {
    SZrAstNode *declNode;
    SZrAstNodeArray *paramList;
    TZrSize argCount;
    TZrSize paramCount;
    TZrSize positionalCount = 0;
    TBool *provided = ZR_NULL;

    if (mismatch != ZR_NULL) {
        *mismatch = ZR_FALSE;
    }

    if (cs == ZR_NULL || funcType == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    declNode = find_function_declaration_for_candidate(cs, env, funcName, funcType);
    if (declNode == ZR_NULL || declNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return infer_function_call_argument_types(cs, call != ZR_NULL ? call->args : ZR_NULL, argTypes);
    }

    paramList = declNode->data.functionDeclaration.params;
    paramCount = paramList != ZR_NULL ? paramList->count : 0;
    argCount = (call != ZR_NULL && call->args != ZR_NULL) ? call->args->count : 0;

    ZrArrayConstruct(argTypes);
    if (paramCount == 0) {
        if (argCount > 0) {
            if (mismatch != ZR_NULL) {
                *mismatch = ZR_TRUE;
            }
            return ZR_TRUE;
        }
        return ZR_TRUE;
    }

    ZrArrayInit(cs->state, argTypes, sizeof(SZrInferredType), paramCount);
    provided = (TBool *) ZrMemoryRawMallocWithType(cs->state->global,
                                                   sizeof(TBool) * paramCount,
                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL) {
        free_inferred_type_array(cs->state, argTypes);
        ZrArrayConstruct(argTypes);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < paramCount; i++) {
        provided[i] = ZR_FALSE;
    }

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        for (TZrSize i = 0; i < argCount && i < call->argNames->length; i++) {
            SZrString **namePtr = (SZrString **) ZrArrayGet(call->argNames, i);
            if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (positionalCount > paramCount) {
            if (mismatch != ZR_NULL) {
                *mismatch = ZR_TRUE;
            }
            goto cleanup;
        }

        for (TZrSize i = 0; i < positionalCount; i++) {
            if (!infer_call_argument_type_node(cs, call->args->nodes[i], argTypes)) {
                goto error;
            }
            provided[i] = ZR_TRUE;
        }

        for (TZrSize i = positionalCount; i < argCount && i < call->argNames->length; i++) {
            SZrString **namePtr = (SZrString **) ZrArrayGet(call->argNames, i);
            TBool matched = ZR_FALSE;

            if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
                if (mismatch != ZR_NULL) {
                    *mismatch = ZR_TRUE;
                }
                goto cleanup;
            }

            for (TZrSize j = 0; j < paramCount; j++) {
                SZrAstNode *paramNode = paramList->nodes[j];
                SZrParameter *param;

                if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                    continue;
                }

                param = &paramNode->data.parameter;
                if (param->name == ZR_NULL || param->name->name == ZR_NULL ||
                    !ZrStringEqual(param->name->name, *namePtr)) {
                    continue;
                }

                if (provided[j]) {
                    if (mismatch != ZR_NULL) {
                        *mismatch = ZR_TRUE;
                    }
                    goto cleanup;
                }

                while (argTypes->length < j) {
                    SZrInferredType placeholder;
                    ZrInferredTypeInit(cs->state, &placeholder, ZR_VALUE_TYPE_NULL);
                    ZrArrayPush(cs->state, argTypes, &placeholder);
                }

                if (argTypes->length == j) {
                    if (!infer_call_argument_type_node(cs, call->args->nodes[i], argTypes)) {
                        goto error;
                    }
                } else {
                    SZrInferredType *existing = (SZrInferredType *) ZrArrayGet(argTypes, j);
                    SZrInferredType argType;
                    ZrInferredTypeInit(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
                    if (!infer_expression_type(cs, call->args->nodes[i], &argType)) {
                        ZrInferredTypeFree(cs->state, &argType);
                        goto error;
                    }
                    if (existing != ZR_NULL) {
                        ZrInferredTypeFree(cs->state, existing);
                        ZrInferredTypeCopy(cs->state, existing, &argType);
                    }
                    ZrInferredTypeFree(cs->state, &argType);
                }

                provided[j] = ZR_TRUE;
                matched = ZR_TRUE;
                break;
            }

            if (!matched) {
                if (mismatch != ZR_NULL) {
                    *mismatch = ZR_TRUE;
                }
                goto cleanup;
            }
        }
    } else {
        if (argCount > paramCount) {
            if (mismatch != ZR_NULL) {
                *mismatch = ZR_TRUE;
            }
            goto cleanup;
        }

        for (TZrSize i = 0; i < argCount; i++) {
            if (!infer_call_argument_type_node(cs, call->args->nodes[i], argTypes)) {
                goto error;
            }
            provided[i] = ZR_TRUE;
        }
    }

    for (TZrSize i = 0; i < paramCount; i++) {
        SZrAstNode *paramNode = paramList->nodes[i];
        SZrParameter *param;

        if (provided[i]) {
            if (i >= argTypes->length) {
                SZrInferredType placeholder;
                ZrInferredTypeInit(cs->state, &placeholder, ZR_VALUE_TYPE_OBJECT);
                ZrArrayPush(cs->state, argTypes, &placeholder);
            }
            continue;
        }

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            if (mismatch != ZR_NULL) {
                *mismatch = ZR_TRUE;
            }
            goto cleanup;
        }

        param = &paramNode->data.parameter;
        if (param->defaultValue == ZR_NULL) {
            if (mismatch != ZR_NULL) {
                *mismatch = ZR_TRUE;
            }
            goto cleanup;
        }

        while (argTypes->length < i) {
            SZrInferredType placeholder;
            ZrInferredTypeInit(cs->state, &placeholder, ZR_VALUE_TYPE_NULL);
            ZrArrayPush(cs->state, argTypes, &placeholder);
        }

        if (argTypes->length == i) {
            if (!infer_call_argument_type_node(cs, param->defaultValue, argTypes)) {
                goto error;
            }
        } else {
            SZrInferredType *existing = (SZrInferredType *) ZrArrayGet(argTypes, i);
            SZrInferredType argType;
            ZrInferredTypeInit(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
            if (!infer_expression_type(cs, param->defaultValue, &argType)) {
                ZrInferredTypeFree(cs->state, &argType);
                goto error;
            }
            if (existing != ZR_NULL) {
                ZrInferredTypeFree(cs->state, existing);
                ZrInferredTypeCopy(cs->state, existing, &argType);
            }
            ZrInferredTypeFree(cs->state, &argType);
        }
    }

cleanup:
    if (provided != ZR_NULL) {
        ZrMemoryRawFreeWithType(cs->state->global,
                                provided,
                                sizeof(TBool) * paramCount,
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZR_TRUE;

error:
    if (provided != ZR_NULL) {
        ZrMemoryRawFreeWithType(cs->state->global,
                                provided,
                                sizeof(TBool) * paramCount,
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    free_inferred_type_array(cs->state, argTypes);
    ZrArrayConstruct(argTypes);
    return ZR_FALSE;
}

static TInt32 score_function_overload_candidate(const SZrFunctionTypeInfo *funcType,
                                                const SZrArray *argTypes) {
    if (funcType == ZR_NULL || argTypes == ZR_NULL) {
        return -1;
    }

    if (funcType->paramTypes.length != argTypes->length) {
        return -1;
    }

    {
        TInt32 score = 0;
        for (TZrSize i = 0; i < argTypes->length; i++) {
            SZrInferredType *argType = (SZrInferredType *)ZrArrayGet((SZrArray *)argTypes, i);
            SZrInferredType *paramType = (SZrInferredType *)ZrArrayGet((SZrArray *)&funcType->paramTypes, i);

            if (argType == ZR_NULL || paramType == ZR_NULL) {
                return -1;
            }

            if (ZrInferredTypeEqual(argType, paramType)) {
                continue;
            }

            if (!ZrInferredTypeIsCompatible(argType, paramType)) {
                return -1;
            }

            score += 1;
        }
        return score;
    }
}

static TBool resolve_best_function_overload(SZrCompilerState *cs,
                                            SZrTypeEnvironment *env,
                                            SZrString *funcName,
                                            SZrFunctionCall *call,
                                            SZrFileRange location,
                                            SZrFunctionTypeInfo **resolvedFunction) {
    SZrArray candidates;
    TInt32 bestScore = INT_MAX;
    SZrFunctionTypeInfo *bestCandidate = ZR_NULL;
    TBool hasTie = ZR_FALSE;
    TChar errorMsg[256];

    if (cs == ZR_NULL || env == ZR_NULL || funcName == ZR_NULL || resolvedFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTypeEnvironmentLookupFunctions(cs->state, env, funcName, &candidates)) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < candidates.length; i++) {
        SZrFunctionTypeInfo **candidatePtr =
            (SZrFunctionTypeInfo **)ZrArrayGet(&candidates, i);
        SZrArray candidateArgTypes;
        TBool mismatch = ZR_FALSE;
        TInt32 score;

        if (candidatePtr == ZR_NULL || *candidatePtr == ZR_NULL) {
            continue;
        }

        if (!infer_function_call_argument_types_for_candidate(cs,
                                                              env,
                                                              funcName,
                                                              call,
                                                              *candidatePtr,
                                                              &candidateArgTypes,
                                                              &mismatch)) {
            if (candidates.isValid && candidates.head != ZR_NULL) {
                ZrArrayFree(cs->state, &candidates);
            }
            return ZR_FALSE;
        }

        if (mismatch) {
            free_inferred_type_array(cs->state, &candidateArgTypes);
            continue;
        }

        score = score_function_overload_candidate(*candidatePtr, &candidateArgTypes);
        free_inferred_type_array(cs->state, &candidateArgTypes);
        if (score < 0) {
            continue;
        }

        if (score < bestScore) {
            bestScore = score;
            bestCandidate = *candidatePtr;
            hasTie = ZR_FALSE;
        } else if (score == bestScore) {
            hasTie = ZR_TRUE;
        }
    }

    if (candidates.isValid && candidates.head != ZR_NULL) {
        ZrArrayFree(cs->state, &candidates);
    }

    if (bestCandidate == ZR_NULL) {
        snprintf(errorMsg,
                 sizeof(errorMsg),
                 "No matching overload for function '%s'",
                 ZrStringGetNativeString(funcName));
        ZrCompilerError(cs, errorMsg, location);
        return ZR_FALSE;
    }

    if (hasTie) {
        snprintf(errorMsg,
                 sizeof(errorMsg),
                 "Ambiguous overload for function '%s'",
                 ZrStringGetNativeString(funcName));
        ZrCompilerError(cs, errorMsg, location);
        return ZR_FALSE;
    }

    *resolvedFunction = bestCandidate;
    return ZR_TRUE;
}

static TBool zr_string_equals_cstr(SZrString *value, const TChar *literal) {
    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    TNativeString valueStr;
    TZrSize valueLen;
    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        valueStr = ZrStringGetNativeStringShort(value);
        valueLen = value->shortStringLength;
    } else {
        valueStr = ZrStringGetNativeString(value);
        valueLen = value->longStringLength;
    }

    TZrSize literalLen = strlen(literal);
    if (valueStr == ZR_NULL || valueLen != literalLen) {
        return ZR_FALSE;
    }

    return memcmp(valueStr, literal, literalLen) == 0;
}

static SZrString *extract_constructed_type_name(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION) {
        SZrPrimaryExpression *primary = &node->data.primaryExpression;
        if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
            return primary->property->data.identifier.name;
        }
    }

    return ZR_NULL;
}

static TBool resolve_compile_time_array_size(SZrCompilerState *cs,
                                             const SZrType *astType,
                                             TZrSize *resolvedSize) {
    SZrTypeValue evaluatedValue;
    TInt64 signedSize;

    if (cs == ZR_NULL || astType == ZR_NULL || resolvedSize == ZR_NULL ||
        astType->arraySizeExpression == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCompilerEvaluateCompileTimeExpression(cs, astType->arraySizeExpression, &evaluatedValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(evaluatedValue.type)) {
        signedSize = evaluatedValue.value.nativeObject.nativeInt64;
        if (signedSize < 0) {
            ZrCompilerError(cs,
                            "Array size expression must evaluate to a non-negative integer",
                            astType->arraySizeExpression->location);
            return ZR_FALSE;
        }
        *resolvedSize = (TZrSize)signedSize;
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(evaluatedValue.type)) {
        *resolvedSize = (TZrSize)evaluatedValue.value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }

    ZrCompilerError(cs,
                    "Array size expression must evaluate to an integer",
                    astType->arraySizeExpression->location);
    return ZR_FALSE;
}

// 获取类型名称字符串（用于错误报告）
const TChar *get_type_name_string(SZrState *state, const SZrInferredType *type, TChar *buffer, TZrSize bufferSize) {
    if (state == ZR_NULL || type == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return "unknown";
    }
    
    const TChar *baseName = get_base_type_name(type->baseType);
    
    // 如果有类型名（用户定义类型），使用类型名
    if (type->typeName != ZR_NULL) {
        TNativeString typeNameStr;
        TZrSize nameLen;
        if (type->typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            typeNameStr = ZrStringGetNativeStringShort(type->typeName);
            nameLen = type->typeName->shortStringLength;
        } else {
            typeNameStr = ZrStringGetNativeString(type->typeName);
            nameLen = type->typeName->longStringLength;
        }
        
        if (nameLen < bufferSize) {
            memcpy(buffer, typeNameStr, nameLen);
            buffer[nameLen] = '\0';
            return buffer;
        }
    }
    
    // 使用基础类型名
    TZrSize nameLen = strlen(baseName);
    if (nameLen < bufferSize) {
        memcpy(buffer, baseName, nameLen);
        buffer[nameLen] = '\0';
        if (type->isNullable) {
            if (nameLen + 6 < bufferSize) {
                memcpy(buffer + nameLen, "?", 1);
                buffer[nameLen + 1] = '\0';
            }
        }
        return buffer;
    }
    
    return baseName;
}

// 报告类型错误
void report_type_error(SZrCompilerState *cs, const TChar *message, const SZrInferredType *expectedType, const SZrInferredType *actualType, SZrFileRange location) {
    if (cs == ZR_NULL || message == ZR_NULL) {
        return;
    }
    
    static TChar errorMsg[512];
    static TChar expectedTypeStr[128];
    static TChar actualTypeStr[128];
    
    const TChar *expectedName = "unknown";
    const TChar *actualName = "unknown";
    
    if (expectedType != ZR_NULL) {
        expectedName = get_type_name_string(cs->state, expectedType, expectedTypeStr, sizeof(expectedTypeStr));
    }
    if (actualType != ZR_NULL) {
        actualName = get_type_name_string(cs->state, actualType, actualTypeStr, sizeof(actualTypeStr));
    }
    
    // 构建详细的错误消息，包含类型信息
    snprintf(errorMsg, sizeof(errorMsg), 
             "Type Error: %s (expected: %s, actual: %s). "
             "Check variable types, function signatures, and type annotations. "
             "Ensure the actual type is compatible with the expected type. "
             "Consider adding explicit type conversions if needed.",
             message, expectedName, actualName);
    
    ZrCompilerError(cs, errorMsg, location);
}

// 检查类型兼容性（用于赋值等场景）
TBool check_type_compatibility(SZrCompilerState *cs, const SZrInferredType *fromType, const SZrInferredType *toType, SZrFileRange location) {
    if (cs == ZR_NULL || fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (ZrInferredTypeIsCompatible(fromType, toType)) {
        return ZR_TRUE;
    }
    
    // 类型不兼容，报告错误
    report_type_error(cs, "Type mismatch", toType, fromType, location);
    return ZR_FALSE;
}

// 检查赋值兼容性
TBool check_assignment_compatibility(SZrCompilerState *cs, const SZrInferredType *leftType, const SZrInferredType *rightType, SZrFileRange location) {
    if (cs == ZR_NULL || leftType == ZR_NULL || rightType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 首先检查基本类型兼容性
    if (!check_type_compatibility(cs, rightType, leftType, location)) {
        return ZR_FALSE;
    }
    
    // 检查范围约束（如果目标类型有范围约束）
    if (leftType->hasRangeConstraint) {
        // 对于字面量，在 check_literal_range 中已经检查
        // 这里主要检查变量赋值时的范围约束
        // 如果 rightType 是编译期常量，可以在这里检查
        // 注意：编译期常量检查需要在编译时进行，这里只做类型兼容性检查
        // 实际的常量值检查在编译期执行器中完成
    }
    
    // 检查数组大小约束
    if (leftType->hasArraySizeConstraint && rightType->baseType == ZR_VALUE_TYPE_ARRAY) {
        // 如果目标数组有固定大小，检查源数组大小是否匹配
        // 注意：这里只能检查字面量数组，变量数组需要在运行时检查
        // 对于数组字面量，需要在赋值时检查（通过检查右值表达式）
        // 在赋值表达式编译时调用 check_array_literal_size
        // 注意：这里只做类型检查，实际的数组大小检查在编译时进行
        // 如果源数组有固定大小，检查是否匹配
        if (rightType->hasArraySizeConstraint && rightType->arrayFixedSize > 0) {
            if (leftType->arrayFixedSize > 0 && leftType->arrayFixedSize != rightType->arrayFixedSize) {
                // 数组大小不匹配，但这里只做类型检查，不报告错误
                // 错误报告在编译时进行
            }
        }
    }
    
    return ZR_TRUE;
}

// 检查函数调用参数兼容性
TBool check_function_call_compatibility(SZrCompilerState *cs,
                                        SZrTypeEnvironment *env,
                                        SZrString *funcName,
                                        SZrFunctionCall *call,
                                        SZrFunctionTypeInfo *funcType,
                                        SZrFileRange location) {
    SZrArray argTypes;
    TBool mismatch = ZR_FALSE;

    ZR_UNUSED_PARAMETER(location);
    if (cs == ZR_NULL || funcType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!infer_function_call_argument_types_for_candidate(cs,
                                                          env,
                                                          funcName,
                                                          call,
                                                          funcType,
                                                          &argTypes,
                                                          &mismatch)) {
        return ZR_FALSE;
    }

    if (mismatch) {
        free_inferred_type_array(cs->state, &argTypes);
        return ZR_FALSE;
    }

    if (argTypes.length != funcType->paramTypes.length) {
        free_inferred_type_array(cs->state, &argTypes);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < argTypes.length; i++) {
        SZrInferredType *argType = (SZrInferredType *) ZrArrayGet(&argTypes, i);
        SZrInferredType *paramType = (SZrInferredType *) ZrArrayGet(&funcType->paramTypes, i);

        if (argType == ZR_NULL || paramType == ZR_NULL) {
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }

        if (!ZrInferredTypeIsCompatible(argType, paramType)) {
            report_type_error(cs, "Argument type mismatch", paramType, argType, location);
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }
    }

    free_inferred_type_array(cs->state, &argTypes);
    return ZR_TRUE;
}

// 从字面量推断类型
TBool infer_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
            return ZR_TRUE;
            
        case ZR_AST_INTEGER_LITERAL: {
            // 未加后缀的整数字面量统一按 int64 推断。
            // 后续若需要字面量收窄，应由语义层在约束上下文中完成，而不是在基础推断阶段直接缩小。
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_INT64);
            return ZR_TRUE;
        }
            
        case ZR_AST_FLOAT_LITERAL:
            // 默认使用DOUBLE
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
            
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_STRING);
            return ZR_TRUE;
            
        case ZR_AST_CHAR_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_INT8);
            return ZR_TRUE;
            
        case ZR_AST_NULL_LITERAL:
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_NULL);
            result->isNullable = ZR_TRUE;
            return ZR_TRUE;
            
        default:
            return ZR_FALSE;
    }
}

// 从标识符推断类型
TBool infer_identifier_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在类型环境中查找变量类型
    if (cs->typeEnv != ZR_NULL) {
        if (ZrTypeEnvironmentLookupVariable(cs->state, cs->typeEnv, name, result)) {
            return ZR_TRUE;
        }
    }
    
    // 未找到变量类型，不立即报错
    // 可能是全局对象 zr 的属性访问，或者子函数，或者全局对象的其他属性
    // 返回默认的 OBJECT 类型，让 compile_identifier 继续处理
    // compile_identifier 会尝试作为全局对象属性访问、子函数访问等
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从一元表达式推断类型
TBool infer_unary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;

    if ((strcmp(op, "new") == 0 || strcmp(op, "$") == 0) && arg != ZR_NULL) {
        SZrString *typeName = extract_constructed_type_name(arg);
        if (typeName != ZR_NULL) {
            ZrInferredTypeInitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
            return ZR_TRUE;
        }
    }
    
    // 推断操作数类型
    SZrInferredType argType;
    ZrInferredTypeInit(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
    if (!infer_expression_type(cs, arg, &argType)) {
        ZrInferredTypeFree(cs->state, &argType);
        return ZR_FALSE;
    }
    
    if (strcmp(op, "!") == 0) {
        // 逻辑非：结果类型是bool
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrInferredTypeFree(cs->state, &argType);
        return ZR_TRUE;
    } else if (strcmp(op, "~") == 0) {
        // 位非：结果类型与操作数类型相同（整数类型）
        ZrInferredTypeCopy(cs->state, result, &argType);
        ZrInferredTypeFree(cs->state, &argType);
        return ZR_TRUE;
    } else if (strcmp(op, "-") == 0 || strcmp(op, "+") == 0) {
        // 取负/正号：结果类型与操作数类型相同
        ZrInferredTypeCopy(cs->state, result, &argType);
        ZrInferredTypeFree(cs->state, &argType);
        return ZR_TRUE;
    }
    
    ZrInferredTypeFree(cs->state, &argType);
    return ZR_FALSE;
}

// 从二元表达式推断类型
TBool infer_binary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    
    // 推断左右操作数类型
    SZrInferredType leftType, rightType;
    TBool hasLeftType = ZR_FALSE;
    TBool hasRightType = ZR_FALSE;
    ZrInferredTypeInit(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    ZrInferredTypeInit(cs->state, &rightType, ZR_VALUE_TYPE_OBJECT);
    hasLeftType = infer_expression_type(cs, left, &leftType);
    hasRightType = hasLeftType ? infer_expression_type(cs, right, &rightType) : ZR_FALSE;
    if (!hasLeftType || !hasRightType) {
        if (hasLeftType) {
            ZrInferredTypeFree(cs->state, &leftType);
        }
        if (hasRightType) {
            ZrInferredTypeFree(cs->state, &rightType);
        }
        return ZR_FALSE;
    }
    
    // 根据操作符确定结果类型
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || 
        strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        // 比较运算符：结果类型是bool
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        // 逻辑运算符：结果类型是bool
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || 
               strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || 
               strcmp(op, "%") == 0 || strcmp(op, "**") == 0) {
        // 算术运算符：获取公共类型（类型提升）
        if (!ZrInferredTypeGetCommonType(cs->state, result, &leftType, &rightType)) {
            // 对类成员访问、动态调用等暂未精确建模的表达式，降级为 object，
            // 避免在 M3 运行时闭环阶段被 M6 的类型系统债务阻塞。
            if (leftType.baseType == ZR_VALUE_TYPE_OBJECT || rightType.baseType == ZR_VALUE_TYPE_OBJECT) {
                ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                ZrInferredTypeFree(cs->state, &leftType);
                ZrInferredTypeFree(cs->state, &rightType);
                return ZR_TRUE;
            }

            report_type_error(cs, "Incompatible types for arithmetic operation", &leftType, &rightType, node->location);
            ZrInferredTypeFree(cs->state, &leftType);
            ZrInferredTypeFree(cs->state, &rightType);
            return ZR_FALSE;
        }
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0 ||
               strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0) {
        // 位运算符：结果类型与左操作数类型相同（整数类型）
        ZrInferredTypeCopy(cs->state, result, &leftType);
        ZrInferredTypeFree(cs->state, &leftType);
        ZrInferredTypeFree(cs->state, &rightType);
        return ZR_TRUE;
    }
    
    ZrInferredTypeFree(cs->state, &leftType);
    ZrInferredTypeFree(cs->state, &rightType);
    return ZR_FALSE;
}

// 从函数调用推断类型
TBool infer_function_call_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }
    
    ZR_UNUSED_PARAMETER(&node->data.functionCall);
    
    // 注意：函数调用在 PRIMARY_EXPRESSION 中处理
    // SZrFunctionCall 只有 args 字段，被调用的表达式在 PRIMARY_EXPRESSION 的 property 中
    // 这个函数应该从 PRIMARY_EXPRESSION 调用，而不是直接从 FUNCTION_CALL 调用
    // 如果直接调用，无法获取被调用的表达式，返回默认对象类型
    
    // 尝试从类型环境查找函数类型（如果函数名可以从上下文中推断）
    // 未来可以从 PRIMARY_EXPRESSION 中获取被调用的表达式进行类型推断
    // 注意：SZrFunctionCall没有callee成员，函数调用在primary expression中处理
    // TODO: 这里暂时跳过，因为函数调用的类型推断在infer_primary_expression_type中处理
    // 默认返回对象类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从Lambda表达式推断类型
TBool infer_lambda_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }
    
    // Lambda表达式返回函数类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_CLOSURE);
    return ZR_TRUE;
}

// 检查数组字面量大小是否符合约束
static TBool check_array_literal_size(SZrCompilerState *cs, SZrAstNode *arrayLiteralNode, const SZrInferredType *targetType, SZrFileRange location) {
    if (cs == ZR_NULL || arrayLiteralNode == ZR_NULL || targetType == ZR_NULL || 
        arrayLiteralNode->type != ZR_AST_ARRAY_LITERAL) {
        return ZR_FALSE;
    }
    
    if (!targetType->hasArraySizeConstraint) {
        return ZR_TRUE;  // 没有大小约束，通过
    }
    
    SZrArrayLiteral *arrayLiteral = &arrayLiteralNode->data.arrayLiteral;
    TZrSize arraySize = (arrayLiteral->elements != ZR_NULL) ? arrayLiteral->elements->count : 0;
    
    // 检查固定大小
    if (targetType->arrayFixedSize > 0) {
        if (arraySize != targetType->arrayFixedSize) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size mismatch: expected %zu elements, got %zu",
                    targetType->arrayFixedSize, arraySize);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    // 检查范围约束
    if (targetType->arrayMinSize > 0) {
        if (arraySize < targetType->arrayMinSize) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size too small: expected at least %zu elements, got %zu",
                    targetType->arrayMinSize, arraySize);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    if (targetType->arrayMaxSize > 0) {
        if (arraySize > targetType->arrayMaxSize) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size too large: expected at most %zu elements, got %zu",
                    targetType->arrayMaxSize, arraySize);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    return ZR_TRUE;
}

// 从数组字面量推断类型
TBool infer_array_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ARRAY_LITERAL) {
        return ZR_FALSE;
    }
    
    // 数组字面量返回数组类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_ARRAY);
    
    // 推断元素类型（如果需要）
    // TODO: 注意：元素类型推断需要遍历数组元素，这里暂时跳过
    // 未来可以实现元素类型推断
    // 1. 推断所有元素类型
    // 2. 找到公共类型
    // 3. 设置elementTypes
    
    return ZR_TRUE;
}

// 从对象字面量推断类型
TBool infer_object_literal_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_OBJECT_LITERAL) {
        return ZR_FALSE;
    }
    
    // 对象字面量返回对象类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从条件表达式推断类型
TBool infer_conditional_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
    
    // 推断then和else分支类型
    SZrInferredType thenType, elseType;
    TBool hasThenType = ZR_FALSE;
    TBool hasElseType = ZR_FALSE;
    ZrInferredTypeInit(cs->state, &thenType, ZR_VALUE_TYPE_OBJECT);
    ZrInferredTypeInit(cs->state, &elseType, ZR_VALUE_TYPE_OBJECT);
    hasThenType = infer_expression_type(cs, condExpr->consequent, &thenType);
    hasElseType = hasThenType ? infer_expression_type(cs, condExpr->alternate, &elseType) : ZR_FALSE;
    if (!hasThenType || !hasElseType) {
        if (hasThenType) {
            ZrInferredTypeFree(cs->state, &thenType);
        }
        if (hasElseType) {
            ZrInferredTypeFree(cs->state, &elseType);
        }
        return ZR_FALSE;
    }
    
    // 获取公共类型
    if (!ZrInferredTypeGetCommonType(cs->state, result, &thenType, &elseType)) {
        // 类型不兼容，报告错误
        report_type_error(cs, "Incompatible types in conditional expression branches", &thenType, &elseType, node->location);
        ZrInferredTypeFree(cs->state, &thenType);
        ZrInferredTypeFree(cs->state, &elseType);
        return ZR_FALSE;
    }
    
    ZrInferredTypeFree(cs->state, &thenType);
    ZrInferredTypeFree(cs->state, &elseType);
    return ZR_TRUE;
}

// 从赋值表达式推断类型
TBool infer_assignment_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
    
    // 推断右值类型
    if (!infer_expression_type(cs, assignExpr->right, result)) {
        return ZR_FALSE;
    }
    
    // 检查与左值类型的兼容性
    // 1. 推断左值类型
    SZrInferredType leftType;
    ZrInferredTypeInit(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    if (infer_expression_type(cs, assignExpr->left, &leftType)) {
        // 2. 检查类型兼容性
        if (leftType.baseType != ZR_VALUE_TYPE_OBJECT && result->baseType != ZR_VALUE_TYPE_OBJECT &&
            !ZrInferredTypeIsCompatible(result, &leftType)) {
            // 3. 报告错误如果不兼容
            report_type_error(cs, "Assignment type mismatch", &leftType, result, node->location);
            ZrInferredTypeFree(cs->state, &leftType);
            return ZR_FALSE;
        }
        ZrInferredTypeFree(cs->state, &leftType);
    }
    
    return ZR_TRUE;
}

// 从primary expression推断类型（包括函数调用）
TBool infer_primary_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    
    // 如果没有members，直接推断property的类型
    if (primary->members == ZR_NULL || primary->members->count == 0) {
        if (primary->property != ZR_NULL) {
            return infer_expression_type(cs, primary->property, result);
        }
        // 如果没有property，返回对象类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 检查是否是成员方法调用：obj.method()
    // 需要：members[0] 是 MemberExpression，members[1] 是 FunctionCall
    if (primary->members->count >= 2) {
        SZrAstNode *firstMember = primary->members->nodes[0];
        SZrAstNode *secondMember = primary->members->nodes[1];
        
        if (firstMember != ZR_NULL && firstMember->type == ZR_AST_MEMBER_EXPRESSION &&
            secondMember != ZR_NULL && secondMember->type == ZR_AST_FUNCTION_CALL) {
            // 成员方法调用：obj.method()
            SZrMemberExpression *memberExpr = &firstMember->data.memberExpression;
            
            // 从 MemberExpression 中提取方法名
            if (memberExpr->property != ZR_NULL && memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                // 推断 property (obj) 的类型
                if (primary->property != ZR_NULL) {
                    SZrInferredType objType;
                    ZrInferredTypeInit(cs->state, &objType, ZR_VALUE_TYPE_OBJECT);
                    if (infer_expression_type(cs, primary->property, &objType)) {
                        // 对于对象类型，方法调用返回对象类型
                        // 未来可以查找对象类型的方法定义来获取精确的返回类型
                        // 目前需要结构体/类的类型信息来查找方法，这是更高级的功能
                        // 如果objType有typeName，可以尝试从类型环境查找方法定义
                        if (objType.typeName != ZR_NULL && cs->typeEnv != ZR_NULL) {
                            // 可以尝试查找类型的方法定义，但需要更复杂的类型系统支持
                            // TODO: 暂时使用默认的对象类型
                        }
                        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        ZrInferredTypeFree(cs->state, &objType);
                        return ZR_TRUE;
                    }
                    ZrInferredTypeFree(cs->state, &objType);
                }
            }
        }
    }
    
    // 检查第一个member是否是函数调用：foo()
    SZrAstNode *firstMember = primary->members->nodes[0];
    if (firstMember != ZR_NULL && firstMember->type == ZR_AST_FUNCTION_CALL) {
        // 函数调用：从property中提取函数名
        if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
            SZrString *funcName = primary->property->data.identifier.name;
            if (funcName != ZR_NULL) {
                SZrFunctionTypeInfo *funcTypeInfo = ZR_NULL;
                SZrFunctionCall *call = &firstMember->data.functionCall;
                TBool hasRuntimeFunction = ZR_FALSE;
                TBool hasCompileTimeFunction = ZR_FALSE;

                if (cs->typeEnv != ZR_NULL) {
                    hasRuntimeFunction = ZrTypeEnvironmentLookupFunction(cs->typeEnv, funcName, &funcTypeInfo);
                    funcTypeInfo = ZR_NULL;
                }

                if (hasRuntimeFunction &&
                    resolve_best_function_overload(cs,
                                                   cs->typeEnv,
                                                   funcName,
                                                   call,
                                                   node->location,
                                                   &funcTypeInfo)) {
                    ZrInferredTypeCopy(cs->state, result, &funcTypeInfo->returnType);
                    check_function_call_compatibility(cs,
                                                      cs->typeEnv,
                                                      funcName,
                                                      call,
                                                      funcTypeInfo,
                                                      node->location);
                    return ZR_TRUE;
                }
                if (hasRuntimeFunction) {
                    return ZR_FALSE;
                }

                if (cs->compileTimeTypeEnv != ZR_NULL) {
                    hasCompileTimeFunction =
                        ZrTypeEnvironmentLookupFunction(cs->compileTimeTypeEnv, funcName, &funcTypeInfo);
                    funcTypeInfo = ZR_NULL;
                }

                if (hasCompileTimeFunction &&
                    resolve_best_function_overload(cs,
                                                   cs->compileTimeTypeEnv,
                                                   funcName,
                                                   call,
                                                   node->location,
                                                   &funcTypeInfo)) {
                    ZrInferredTypeCopy(cs->state, result, &funcTypeInfo->returnType);
                    check_function_call_compatibility(cs,
                                                      cs->compileTimeTypeEnv,
                                                      funcName,
                                                      call,
                                                      funcTypeInfo,
                                                      node->location);
                    return ZR_TRUE;
                }
                if (hasCompileTimeFunction) {
                    return ZR_FALSE;
                }
                
                // 函数未找到，检查是否是 struct 构造函数调用
                if (cs->typeEnv != ZR_NULL && ZrTypeEnvironmentLookupType(cs->typeEnv, funcName)) {
                    // 找到类型名称，推断返回类型为对应的 struct 类型
                    ZrInferredTypeInitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, funcName);
                    return ZR_TRUE;
                }

                // import 是运行时内建函数，由宿主负责解析模块，返回对象类型。
                if (zr_string_equals_cstr(funcName, "import")) {
                    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                    return ZR_TRUE;
                }
                
                // 函数和类型都未找到时，保持动态 object fallback。
                ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        }
        
        // property不是标识符，或者函数名未找到，返回对象类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 不是函数调用，或者是成员访问等其他情况
    // 先推断property的类型，然后根据members推断最终类型
    // 实现完整的成员访问链类型推断（如 obj.prop）
    if (primary->property != ZR_NULL) {
        SZrInferredType baseType;
        ZrInferredTypeInit(cs->state, &baseType, ZR_VALUE_TYPE_OBJECT);
        if (infer_expression_type(cs, primary->property, &baseType)) {
            // 如果有members，需要根据members推断最终类型
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                // 遍历members链，逐步推断类型
                SZrInferredType currentType;
                ZrInferredTypeInit(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
                ZrInferredTypeCopy(cs->state, &currentType, &baseType);
                ZrInferredTypeFree(cs->state, &baseType);
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    SZrAstNode *member = primary->members->nodes[i];
                    if (member != ZR_NULL && member->type == ZR_AST_MEMBER_EXPRESSION) {
                        // 成员访问：从当前类型推断成员类型
                        // TODO: 注意：这需要类型系统支持，暂时返回对象类型
                        ZrInferredTypeFree(cs->state, &currentType);
                        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        return ZR_TRUE;
                    }
                }
                ZrInferredTypeCopy(cs->state, result, &currentType);
                ZrInferredTypeFree(cs->state, &currentType);
                return ZR_TRUE;
            } else {
                // 没有members，直接返回property的类型
                ZrInferredTypeCopy(cs->state, result, &baseType);
                ZrInferredTypeFree(cs->state, &baseType);
                return ZR_TRUE;
            }
        }
    }
    
    // 默认返回对象类型
    ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从AST节点推断类型（主入口函数）
TBool infer_expression_type(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    switch (node->type) {
        // 字面量
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return infer_literal_type(cs, node, result);
        
        case ZR_AST_IDENTIFIER_LITERAL:
            return infer_identifier_type(cs, node, result);
        
        // 表达式
        case ZR_AST_BINARY_EXPRESSION:
            return infer_binary_expression_type(cs, node, result);
        
        case ZR_AST_UNARY_EXPRESSION:
            return infer_unary_expression_type(cs, node, result);
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return infer_conditional_type(cs, node, result);
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return infer_assignment_type(cs, node, result);
        
        case ZR_AST_FUNCTION_CALL:
            return infer_function_call_type(cs, node, result);
        
        case ZR_AST_LAMBDA_EXPRESSION:
            return infer_lambda_type(cs, node, result);
        
        case ZR_AST_ARRAY_LITERAL:
            return infer_array_literal_type(cs, node, result);
        
        case ZR_AST_OBJECT_LITERAL:
            return infer_object_literal_type(cs, node, result);
        
        // TODO: 处理其他表达式类型
        case ZR_AST_PRIMARY_EXPRESSION:
            return infer_primary_expression_type(cs, node, result);
        
        case ZR_AST_MEMBER_EXPRESSION:
            // 实现member expression的类型推断
            // member expression的类型推断需要知道对象类型和成员名称
            // TODO: 这里简化处理，返回对象类型
            // 完整的实现需要从对象类型查找成员定义
            // TODO: 注意：member expression的类型推断需要知道对象类型，暂时返回对象类型
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
        
        case ZR_AST_IF_EXPRESSION:
            // 实现if expression的类型推断
            // if expression的类型是thenExpr和elseExpr的公共类型
            {
                SZrIfExpression *ifExpr = &node->data.ifExpression;
                SZrInferredType thenType, elseType;
                ZrInferredTypeInit(cs->state, &thenType, ZR_VALUE_TYPE_OBJECT);
                ZrInferredTypeInit(cs->state, &elseType, ZR_VALUE_TYPE_OBJECT);
                if (ifExpr->thenExpr != ZR_NULL && ifExpr->elseExpr != ZR_NULL) {
                    if (infer_expression_type(cs, ifExpr->thenExpr, &thenType) &&
                        infer_expression_type(cs, ifExpr->elseExpr, &elseType)) {
                        // 获取公共类型
                        if (ZrInferredTypeGetCommonType(cs->state, result, &thenType, &elseType)) {
                            ZrInferredTypeFree(cs->state, &thenType);
                            ZrInferredTypeFree(cs->state, &elseType);
                            return ZR_TRUE;
                        }
                        ZrInferredTypeFree(cs->state, &thenType);
                        ZrInferredTypeFree(cs->state, &elseType);
                    }
                } else if (ifExpr->thenExpr != ZR_NULL) {
                    return infer_expression_type(cs, ifExpr->thenExpr, result);
                } else if (ifExpr->elseExpr != ZR_NULL) {
                    return infer_expression_type(cs, ifExpr->elseExpr, result);
                }
                // 默认返回对象类型
                ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        case ZR_AST_SWITCH_EXPRESSION:
            // 实现switch expression的类型推断
            // switch expression的类型是所有case和default的公共类型
            {
                SZrSwitchExpression *switchExpr = &node->data.switchExpression;
                SZrInferredType commonType;
                ZrInferredTypeInit(cs->state, &commonType, ZR_VALUE_TYPE_OBJECT);
                TBool hasType = ZR_FALSE;
                
                // 遍历所有case，推断类型
                if (switchExpr->cases != ZR_NULL) {
                    for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
                        SZrAstNode *caseNode = switchExpr->cases->nodes[i];
                        if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
                            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
                            if (switchCase->block != ZR_NULL) {
                                SZrInferredType caseType;
                                ZrInferredTypeInit(cs->state, &caseType, ZR_VALUE_TYPE_OBJECT);
                                if (infer_expression_type(cs, switchCase->block, &caseType)) {
                                    if (!hasType) {
                                        ZrInferredTypeCopy(cs->state, &commonType, &caseType);
                                        hasType = ZR_TRUE;
                                    } else {
                                        SZrInferredType newCommonType;
                                        ZrInferredTypeInit(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                        if (ZrInferredTypeGetCommonType(cs->state, &newCommonType, &commonType, &caseType)) {
                                            ZrInferredTypeFree(cs->state, &commonType);
                                            commonType = newCommonType;
                                        }
                                    }
                                    ZrInferredTypeFree(cs->state, &caseType);
                                }
                            }
                        }
                    }
                }
                
                // 处理default case
                if (switchExpr->defaultCase != ZR_NULL && switchExpr->defaultCase->type == ZR_AST_SWITCH_DEFAULT) {
                    SZrSwitchDefault *switchDefault = &switchExpr->defaultCase->data.switchDefault;
                    if (switchDefault->block != ZR_NULL) {
                        SZrInferredType defaultType;
                        ZrInferredTypeInit(cs->state, &defaultType, ZR_VALUE_TYPE_OBJECT);
                        if (infer_expression_type(cs, switchDefault->block, &defaultType)) {
                            if (!hasType) {
                                ZrInferredTypeCopy(cs->state, &commonType, &defaultType);
                                hasType = ZR_TRUE;
                            } else {
                                SZrInferredType newCommonType;
                                ZrInferredTypeInit(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                if (ZrInferredTypeGetCommonType(cs->state, &newCommonType, &commonType, &defaultType)) {
                                    ZrInferredTypeFree(cs->state, &commonType);
                                    commonType = newCommonType;
                                }
                            }
                            ZrInferredTypeFree(cs->state, &defaultType);
                        }
                    }
                }
                
                if (hasType) {
                    ZrInferredTypeCopy(cs->state, result, &commonType);
                    ZrInferredTypeFree(cs->state, &commonType);
                    return ZR_TRUE;
                }
                
                // 默认返回对象类型
                ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        default:
            return ZR_FALSE;
    }
}

// 将AST类型注解转换为推断类型
TBool convert_ast_type_to_inferred_type(SZrCompilerState *cs, const SZrType *astType, SZrInferredType *result) {
    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果没有类型注解，返回对象类型
    if (astType == ZR_NULL || astType->name == ZR_NULL) {
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        return ZR_TRUE;
    }
    
    EZrValueType baseType = ZR_VALUE_TYPE_OBJECT;
    
    // 根据类型名称节点的类型处理
    if (astType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        // 标识符类型（如 int, float, bool, string, 或用户定义类型）
        SZrString *typeName = astType->name->data.identifier.name;
        if (typeName == ZR_NULL) {
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            result->ownershipQualifier = astType->ownershipQualifier;
            return ZR_TRUE;
        }
        
        // 获取类型名称字符串
        TNativeString nameStr;
        TZrSize nameLen;
        if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            nameStr = ZrStringGetNativeStringShort(typeName);
            nameLen = typeName->shortStringLength;
        } else {
            nameStr = ZrStringGetNativeString(typeName);
            nameLen = typeName->longStringLength;
        }
        
        // 匹配基本类型名称
        if (nameLen == 3 && memcmp(nameStr, "int", 3) == 0) {
            baseType = ZR_VALUE_TYPE_INT64;
        } else if (nameLen == 5 && memcmp(nameStr, "float", 5) == 0) {
            baseType = ZR_VALUE_TYPE_DOUBLE;
        } else if (nameLen == 4 && memcmp(nameStr, "bool", 4) == 0) {
            baseType = ZR_VALUE_TYPE_BOOL;
        } else if (nameLen == 6 && memcmp(nameStr, "string", 6) == 0) {
            baseType = ZR_VALUE_TYPE_STRING;
        } else if (nameLen == 4 && memcmp(nameStr, "null", 4) == 0) {
            baseType = ZR_VALUE_TYPE_NULL;
        } else if (nameLen == 4 && memcmp(nameStr, "void", 4) == 0) {
            baseType = ZR_VALUE_TYPE_NULL; // void 视为 null
        } else {
            // 用户定义类型（struct/class等），存储类型名称
            ZrInferredTypeInitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
            result->ownershipQualifier = astType->ownershipQualifier;
            if (cs->semanticContext != ZR_NULL) {
                ZrSemanticRegisterNamedType(cs->semanticContext,
                                            typeName,
                                            ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                            astType->name);
                ZrSemanticRegisterInferredType(cs->semanticContext,
                                               result,
                                               ZR_SEMANTIC_TYPE_KIND_REFERENCE,
                                               result->typeName,
                                               astType->name);
            }
            return ZR_TRUE;
        }
    } else if (astType->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &astType->name->data.genericType;
        SZrString *canonicalName;

        if (genericType->name == ZR_NULL || genericType->name->name == ZR_NULL) {
            ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            result->ownershipQualifier = astType->ownershipQualifier;
            return ZR_TRUE;
        }

        ZrInferredTypeInitFull(cs->state,
                               result,
                               ZR_VALUE_TYPE_OBJECT,
                               ZR_FALSE,
                               genericType->name->name);
        result->ownershipQualifier = astType->ownershipQualifier;

        if (genericType->params != ZR_NULL && genericType->params->count > 0) {
            ZrArrayInit(cs->state,
                        &result->elementTypes,
                        sizeof(SZrInferredType),
                        genericType->params->count);
            for (TZrSize i = 0; i < genericType->params->count; i++) {
                SZrAstNode *paramNode = genericType->params->nodes[i];
                SZrInferredType paramType;

                if (paramNode == ZR_NULL || paramNode->type != ZR_AST_TYPE) {
                    ZrInferredTypeFree(cs->state, result);
                    ZrCompilerError(cs, "Generic type parameter must be a type annotation", astType->name->location);
                    return ZR_FALSE;
                }

                ZrInferredTypeInit(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                if (!convert_ast_type_to_inferred_type(cs, &paramNode->data.type, &paramType)) {
                    ZrInferredTypeFree(cs->state, &paramType);
                    ZrInferredTypeFree(cs->state, result);
                    return ZR_FALSE;
                }

                ZrArrayPush(cs->state, &result->elementTypes, &paramType);
            }
        }

        canonicalName = build_generic_instance_name(cs->state,
                                                    genericType->name->name,
                                                    &result->elementTypes);
        if (canonicalName != ZR_NULL) {
            result->typeName = canonicalName;
        }

        if (cs->semanticContext != ZR_NULL) {
            ZrSemanticRegisterNamedType(cs->semanticContext,
                                        genericType->name->name,
                                        ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                        astType->name);
            ZrSemanticRegisterInferredType(cs->semanticContext,
                                           result,
                                           ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
                                           result->typeName,
                                           astType->name);
        }
        return ZR_TRUE;
    } else if (astType->name->type == ZR_AST_TUPLE_TYPE) {
        // 元组类型（TODO: 处理元组）
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        result->ownershipQualifier = astType->ownershipQualifier;
        return ZR_TRUE;
    } else {
        // 未知类型节点类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        result->ownershipQualifier = astType->ownershipQualifier;
        return ZR_TRUE;
    }
    
    // 处理数组维度
    if (astType->dimensions > 0) {
        // 数组类型
        ZrInferredTypeInit(cs->state, result, ZR_VALUE_TYPE_ARRAY);
        result->ownershipQualifier = astType->ownershipQualifier;
        // 处理元素类型
        if (astType->subType != ZR_NULL) {
            // 递归转换子类型
            SZrInferredType elementType;
            ZrInferredTypeInit(cs->state, &elementType, ZR_VALUE_TYPE_OBJECT);
            if (convert_ast_type_to_inferred_type(cs, astType->subType, &elementType)) {
                // 将元素类型添加到elementTypes数组
                ZrArrayInit(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
                ZrArrayPush(cs->state, &result->elementTypes, &elementType);
            }
        }
        
        // 复制数组大小约束
        if (astType->hasArraySizeConstraint) {
            if (astType->arraySizeExpression != ZR_NULL) {
                TZrSize resolvedSize;
                if (!resolve_compile_time_array_size(cs, astType, &resolvedSize)) {
                    return ZR_FALSE;
                }
                result->arrayFixedSize = resolvedSize;
                result->arrayMinSize = resolvedSize;
                result->arrayMaxSize = resolvedSize;
            } else {
                result->arrayFixedSize = astType->arrayFixedSize;
                result->arrayMinSize = astType->arrayMinSize;
                result->arrayMaxSize = astType->arrayMaxSize;
            }
            result->hasArraySizeConstraint = ZR_TRUE;
        }
    } else {
        // 非数组类型
        ZrInferredTypeInit(cs->state, result, baseType);
        result->ownershipQualifier = astType->ownershipQualifier;
    }
    
    return ZR_TRUE;
}

// 获取类型的范围限制（用于整数类型）
static void get_type_range(EZrValueType baseType, TInt64 *minValue, TInt64 *maxValue) {
    switch (baseType) {
        case ZR_VALUE_TYPE_INT8:
            *minValue = -128;
            *maxValue = 127;
            break;
        case ZR_VALUE_TYPE_INT16:
            *minValue = -32768;
            *maxValue = 32767;
            break;
        case ZR_VALUE_TYPE_INT32:
            *minValue = -2147483648LL;
            *maxValue = 2147483647LL;
            break;
        case ZR_VALUE_TYPE_INT64:
            *minValue = -9223372036854775808LL;
            *maxValue = 9223372036854775807LL;
            break;
        case ZR_VALUE_TYPE_UINT8:
            *minValue = 0;
            *maxValue = 255;
            break;
        case ZR_VALUE_TYPE_UINT16:
            *minValue = 0;
            *maxValue = 65535;
            break;
        case ZR_VALUE_TYPE_UINT32:
            *minValue = 0;
            *maxValue = 4294967295LL;
            break;
        case ZR_VALUE_TYPE_UINT64:
            *minValue = 0;
            *maxValue = 18446744073709551615ULL;
            break;
        default:
            *minValue = 0;
            *maxValue = 0;
            break;
    }
}

// 检查字面量范围
TBool check_literal_range(SZrCompilerState *cs, SZrAstNode *literalNode, const SZrInferredType *targetType, SZrFileRange location) {
    if (cs == ZR_NULL || literalNode == ZR_NULL || targetType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查整数类型字面量
    if (literalNode->type == ZR_AST_INTEGER_LITERAL && ZR_VALUE_IS_TYPE_INT(targetType->baseType)) {
        TInt64 literalValue = literalNode->data.integerLiteral.value;
        TInt64 minValue, maxValue;
        get_type_range(targetType->baseType, &minValue, &maxValue);
        
        if (literalValue < minValue || literalValue > maxValue) {
            static TChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Integer literal %lld is out of range for type (expected range: %lld to %lld)",
                    (long long)literalValue, (long long)minValue, (long long)maxValue);
            report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 检查用户定义的范围约束
        if (targetType->hasRangeConstraint) {
            if (literalValue < targetType->minValue || literalValue > targetType->maxValue) {
                static TChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Integer literal %lld is out of range constraint (expected range: %lld to %lld)",
                        (long long)literalValue, (long long)targetType->minValue, (long long)targetType->maxValue);
                report_type_error(cs, errorMsg, targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 检查浮点数类型字面量（NaN, Infinity）
    if (literalNode->type == ZR_AST_FLOAT_LITERAL) {
        TDouble floatValue = literalNode->data.floatLiteral.value;
        // 检查是否为 NaN 或 Infinity（如果目标类型不允许）
        // 根据目标类型决定是否允许 NaN/Infinity
        if (isnan(floatValue) || isinf(floatValue)) {
            // 检查目标类型是否允许NaN/Infinity
            // 对于整数类型，不允许NaN/Infinity
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(targetType->baseType) || 
                ZR_VALUE_IS_TYPE_UNSIGNED_INT(targetType->baseType)) {
                report_type_error(cs, "NaN/Infinity cannot be assigned to integer type", 
                                 targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
            // 对于浮点类型，允许NaN/Infinity
        }
    }
    
    return ZR_TRUE;
}

// 检查数组索引边界
TBool check_array_index_bounds(SZrCompilerState *cs, SZrAstNode *indexExpr, const SZrInferredType *arrayType, SZrFileRange location) {
    if (cs == ZR_NULL || indexExpr == ZR_NULL || arrayType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 只对字面量索引进行编译期检查
    if (indexExpr->type == ZR_AST_INTEGER_LITERAL) {
        TInt64 indexValue = indexExpr->data.integerLiteral.value;
        
        if (indexValue < 0) {
            static TChar errorMsg[128];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array index %lld is negative", (long long)indexValue);
            report_type_error(cs, errorMsg, arrayType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 如果数组有固定大小，检查索引是否越界
        if (arrayType->hasArraySizeConstraint && arrayType->arrayFixedSize > 0) {
            if ((TZrSize)indexValue >= arrayType->arrayFixedSize) {
                static TChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Array index %lld is out of bounds (array size: %zu)",
                        (long long)indexValue, arrayType->arrayFixedSize);
                report_type_error(cs, errorMsg, arrayType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 对于非字面量索引，编译期无法检查，将在运行时检查（如果启用）
    return ZR_TRUE;
}
