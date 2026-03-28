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
static const TZrChar *get_base_type_name(EZrValueType baseType) {
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
            SZrInferredType *type = (SZrInferredType *)ZrCore_Array_Get(types, i);
            if (type != ZR_NULL) {
                ZrParser_InferredType_Free(state, type);
            }
        }
        ZrCore_Array_Free(state, types);
    }
}

static TZrBool append_text_fragment(TZrChar *buffer,
                                  TZrSize bufferSize,
                                  TZrSize *offset,
                                  const TZrChar *fragment) {
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

static TZrBool append_inferred_type_display_name(const SZrInferredType *type,
                                               TZrChar *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *offset) {
    const TZrChar *baseName;

    if (type == ZR_NULL) {
        return append_text_fragment(buffer, bufferSize, offset, "object");
    }

    if (type->typeName != ZR_NULL) {
        return append_text_fragment(buffer,
                                    bufferSize,
                                    offset,
                                    ZrCore_String_GetNativeString(type->typeName));
    }

    if (type->baseType == ZR_VALUE_TYPE_ARRAY && type->elementTypes.length > 0) {
        SZrInferredType *elementType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0);
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
    TZrChar buffer[512];
    TZrSize offset = 0;

    if (state == ZR_NULL || baseName == ZR_NULL || typeArguments == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!append_text_fragment(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(baseName)) ||
        !append_text_fragment(buffer, sizeof(buffer), &offset, "<")) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < typeArguments->length; i++) {
        SZrInferredType *argumentType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)typeArguments, i);
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

    return ZrCore_String_Create(state, buffer, offset);
}

static TZrBool infer_function_call_argument_types(SZrCompilerState *cs,
                                                SZrAstNodeArray *args,
                                                SZrArray *argTypes) {
    if (cs == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(argTypes);
    if (args == ZR_NULL || args->count == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, argTypes, sizeof(SZrInferredType), args->count);
    for (TZrSize i = 0; i < args->count; i++) {
        SZrAstNode *argNode = args->nodes[i];
        SZrInferredType argType;

        ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
        if (argNode == ZR_NULL || !ZrParser_ExpressionType_Infer(cs, argNode, &argType)) {
            ZrParser_InferredType_Free(cs->state, &argType);
            free_inferred_type_array(cs->state, argTypes);
            ZrCore_Array_Construct(argTypes);
            return ZR_FALSE;
        }

        ZrCore_Array_Push(cs->state, argTypes, &argType);
    }

    return ZR_TRUE;
}

static TZrBool function_declaration_matches_candidate(SZrCompilerState *cs,
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

    ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
    if (decl->returnType != ZR_NULL) {
        if (!ZrParser_AstTypeToInferredType_Convert(cs, decl->returnType, &returnType)) {
            ZrParser_InferredType_Free(cs->state, &returnType);
            return ZR_FALSE;
        }
    }

    if (!ZrParser_InferredType_Equal(&returnType, &funcType->returnType)) {
        ZrParser_InferredType_Free(cs->state, &returnType);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &returnType);

    for (TZrSize i = 0; i < decl->params->count; i++) {
        SZrAstNode *paramNode = decl->params->nodes[i];
        SZrParameter *param;
        SZrInferredType paramType;
        SZrInferredType *expectedType;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            return ZR_FALSE;
        }

        param = &paramNode->data.parameter;
        ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
        if (param->typeInfo != ZR_NULL) {
            if (!ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                ZrParser_InferredType_Free(cs->state, &paramType);
                return ZR_FALSE;
            }
        }

        expectedType = (SZrInferredType *) ZrCore_Array_Get((SZrArray *) &funcType->paramTypes, i);
        if (expectedType == ZR_NULL || !ZrParser_InferredType_Equal(&paramType, expectedType)) {
            ZrParser_InferredType_Free(cs->state, &paramType);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Free(cs->state, &paramType);
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
                (SZrCompileTimeFunction **) ZrCore_Array_Get(&cs->compileTimeFunctions, i);
            if (funcPtr == ZR_NULL || *funcPtr == ZR_NULL || (*funcPtr)->name == ZR_NULL ||
                !ZrCore_String_Equal((*funcPtr)->name, funcName)) {
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
            !ZrCore_String_Equal(decl->name->name, funcName)) {
            continue;
        }

        if (function_declaration_matches_candidate(cs, stmt, funcType)) {
            return stmt;
        }
    }

    return ZR_NULL;
}

static TZrBool infer_call_argument_type_node(SZrCompilerState *cs,
                                           SZrAstNode *argNode,
                                           SZrArray *argTypes) {
    SZrInferredType argType;

    if (cs == ZR_NULL || argNode == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, argNode, &argType)) {
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_FALSE;
    }

    ZrCore_Array_Push(cs->state, argTypes, &argType);
    return ZR_TRUE;
}

static TZrBool infer_function_call_argument_types_for_candidate(SZrCompilerState *cs,
                                                              SZrTypeEnvironment *env,
                                                              SZrString *funcName,
                                                              SZrFunctionCall *call,
                                                              const SZrFunctionTypeInfo *funcType,
                                                              SZrArray *argTypes,
                                                              TZrBool *mismatch) {
    SZrAstNode *declNode;
    SZrAstNodeArray *paramList;
    TZrSize argCount;
    TZrSize paramCount;
    TZrSize positionalCount = 0;
    TZrBool *provided = ZR_NULL;

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

    ZrCore_Array_Construct(argTypes);
    if (paramCount == 0) {
        if (argCount > 0) {
            if (mismatch != ZR_NULL) {
                *mismatch = ZR_TRUE;
            }
            return ZR_TRUE;
        }
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, argTypes, sizeof(SZrInferredType), paramCount);
    provided = (TZrBool *) ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                   sizeof(TZrBool) * paramCount,
                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL) {
        free_inferred_type_array(cs->state, argTypes);
        ZrCore_Array_Construct(argTypes);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < paramCount; i++) {
        provided[i] = ZR_FALSE;
    }

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        for (TZrSize i = 0; i < argCount && i < call->argNames->length; i++) {
            SZrString **namePtr = (SZrString **) ZrCore_Array_Get(call->argNames, i);
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
            SZrString **namePtr = (SZrString **) ZrCore_Array_Get(call->argNames, i);
            TZrBool matched = ZR_FALSE;

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
                    !ZrCore_String_Equal(param->name->name, *namePtr)) {
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
                    ZrParser_InferredType_Init(cs->state, &placeholder, ZR_VALUE_TYPE_NULL);
                    ZrCore_Array_Push(cs->state, argTypes, &placeholder);
                }

                if (argTypes->length == j) {
                    if (!infer_call_argument_type_node(cs, call->args->nodes[i], argTypes)) {
                        goto error;
                    }
                } else {
                    SZrInferredType *existing = (SZrInferredType *) ZrCore_Array_Get(argTypes, j);
                    SZrInferredType argType;
                    ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
                    if (!ZrParser_ExpressionType_Infer(cs, call->args->nodes[i], &argType)) {
                        ZrParser_InferredType_Free(cs->state, &argType);
                        goto error;
                    }
                    if (existing != ZR_NULL) {
                        ZrParser_InferredType_Free(cs->state, existing);
                        ZrParser_InferredType_Copy(cs->state, existing, &argType);
                    }
                    ZrParser_InferredType_Free(cs->state, &argType);
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
                ZrParser_InferredType_Init(cs->state, &placeholder, ZR_VALUE_TYPE_OBJECT);
                ZrCore_Array_Push(cs->state, argTypes, &placeholder);
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
            ZrParser_InferredType_Init(cs->state, &placeholder, ZR_VALUE_TYPE_NULL);
            ZrCore_Array_Push(cs->state, argTypes, &placeholder);
        }

        if (argTypes->length == i) {
            if (!infer_call_argument_type_node(cs, param->defaultValue, argTypes)) {
                goto error;
            }
        } else {
            SZrInferredType *existing = (SZrInferredType *) ZrCore_Array_Get(argTypes, i);
            SZrInferredType argType;
            ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
            if (!ZrParser_ExpressionType_Infer(cs, param->defaultValue, &argType)) {
                ZrParser_InferredType_Free(cs->state, &argType);
                goto error;
            }
            if (existing != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, existing);
                ZrParser_InferredType_Copy(cs->state, existing, &argType);
            }
            ZrParser_InferredType_Free(cs->state, &argType);
        }
    }

cleanup:
    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                provided,
                                sizeof(TZrBool) * paramCount,
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZR_TRUE;

error:
    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                provided,
                                sizeof(TZrBool) * paramCount,
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    free_inferred_type_array(cs->state, argTypes);
    ZrCore_Array_Construct(argTypes);
    return ZR_FALSE;
}

static TZrInt32 score_function_overload_candidate(const SZrFunctionTypeInfo *funcType,
                                                const SZrArray *argTypes) {
    if (funcType == ZR_NULL || argTypes == ZR_NULL) {
        return -1;
    }

    if (funcType->paramTypes.length != argTypes->length) {
        return -1;
    }

    {
        TZrInt32 score = 0;
        for (TZrSize i = 0; i < argTypes->length; i++) {
            SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)argTypes, i);
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&funcType->paramTypes, i);

            if (argType == ZR_NULL || paramType == ZR_NULL) {
                return -1;
            }

            if (ZrParser_InferredType_Equal(argType, paramType)) {
                continue;
            }

            if (!ZrParser_InferredType_IsCompatible(argType, paramType)) {
                return -1;
            }

            score += 1;
        }
        return score;
    }
}

static TZrBool resolve_best_function_overload(SZrCompilerState *cs,
                                            SZrTypeEnvironment *env,
                                            SZrString *funcName,
                                            SZrFunctionCall *call,
                                            SZrFileRange location,
                                            SZrFunctionTypeInfo **resolvedFunction) {
    SZrArray candidates;
    TZrInt32 bestScore = INT_MAX;
    SZrFunctionTypeInfo *bestCandidate = ZR_NULL;
    TZrBool hasTie = ZR_FALSE;
    TZrChar errorMsg[256];

    if (cs == ZR_NULL || env == ZR_NULL || funcName == ZR_NULL || resolvedFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_TypeEnvironment_LookupFunctions(cs->state, env, funcName, &candidates)) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < candidates.length; i++) {
        SZrFunctionTypeInfo **candidatePtr =
            (SZrFunctionTypeInfo **)ZrCore_Array_Get(&candidates, i);
        SZrArray candidateArgTypes;
        TZrBool mismatch = ZR_FALSE;
        TZrInt32 score;

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
                ZrCore_Array_Free(cs->state, &candidates);
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
        ZrCore_Array_Free(cs->state, &candidates);
    }

    if (bestCandidate == ZR_NULL) {
        snprintf(errorMsg,
                 sizeof(errorMsg),
                 "No matching overload for function '%s'",
                 ZrCore_String_GetNativeString(funcName));
        ZrParser_Compiler_Error(cs, errorMsg, location);
        return ZR_FALSE;
    }

    if (hasTie) {
        snprintf(errorMsg,
                 sizeof(errorMsg),
                 "Ambiguous overload for function '%s'",
                 ZrCore_String_GetNativeString(funcName));
        ZrParser_Compiler_Error(cs, errorMsg, location);
        return ZR_FALSE;
    }

    *resolvedFunction = bestCandidate;
    return ZR_TRUE;
}

static TZrBool zr_string_equals_cstr(SZrString *value, const TZrChar *literal) {
    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    TZrNativeString valueStr;
    TZrSize valueLen;
    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        valueStr = ZrCore_String_GetNativeStringShort(value);
        valueLen = value->shortStringLength;
    } else {
        valueStr = ZrCore_String_GetNativeString(value);
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

static TZrBool resolve_compile_time_array_size(SZrCompilerState *cs,
                                             const SZrType *astType,
                                             TZrSize *resolvedSize) {
    SZrTypeValue evaluatedValue;
    TZrInt64 signedSize;

    if (cs == ZR_NULL || astType == ZR_NULL || resolvedSize == ZR_NULL ||
        astType->arraySizeExpression == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_Compiler_EvaluateCompileTimeExpression(cs, astType->arraySizeExpression, &evaluatedValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(evaluatedValue.type)) {
        signedSize = evaluatedValue.value.nativeObject.nativeInt64;
        if (signedSize < 0) {
            ZrParser_Compiler_Error(cs,
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

    ZrParser_Compiler_Error(cs,
                    "Array size expression must evaluate to an integer",
                    astType->arraySizeExpression->location);
    return ZR_FALSE;
}

// 获取类型名称字符串（用于错误报告）
const TZrChar *ZrParser_TypeNameString_Get(SZrState *state, const SZrInferredType *type, TZrChar *buffer, TZrSize bufferSize) {
    if (state == ZR_NULL || type == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return "unknown";
    }
    
    const TZrChar *baseName = get_base_type_name(type->baseType);
    
    // 如果有类型名（用户定义类型），使用类型名
    if (type->typeName != ZR_NULL) {
        TZrNativeString typeNameStr;
        TZrSize nameLen;
        if (type->typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            typeNameStr = ZrCore_String_GetNativeStringShort(type->typeName);
            nameLen = type->typeName->shortStringLength;
        } else {
            typeNameStr = ZrCore_String_GetNativeString(type->typeName);
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
void ZrParser_TypeError_Report(SZrCompilerState *cs, const TZrChar *message, const SZrInferredType *expectedType, const SZrInferredType *actualType, SZrFileRange location) {
    if (cs == ZR_NULL || message == ZR_NULL) {
        return;
    }
    
    static TZrChar errorMsg[512];
    static TZrChar expectedTypeStr[128];
    static TZrChar actualTypeStr[128];
    
    const TZrChar *expectedName = "unknown";
    const TZrChar *actualName = "unknown";
    
    if (expectedType != ZR_NULL) {
        expectedName = ZrParser_TypeNameString_Get(cs->state, expectedType, expectedTypeStr, sizeof(expectedTypeStr));
    }
    if (actualType != ZR_NULL) {
        actualName = ZrParser_TypeNameString_Get(cs->state, actualType, actualTypeStr, sizeof(actualTypeStr));
    }
    
    // 构建详细的错误消息，包含类型信息
    snprintf(errorMsg, sizeof(errorMsg), 
             "Type Error: %s (expected: %s, actual: %s). "
             "Check variable types, function signatures, and type annotations. "
             "Ensure the actual type is compatible with the expected type. "
             "Consider adding explicit type conversions if needed.",
             message, expectedName, actualName);
    
    ZrParser_Compiler_Error(cs, errorMsg, location);
}

// 检查类型兼容性（用于赋值等场景）
TZrBool ZrParser_TypeCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *fromType, const SZrInferredType *toType, SZrFileRange location) {
    if (cs == ZR_NULL || fromType == ZR_NULL || toType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (ZrParser_InferredType_IsCompatible(fromType, toType)) {
        return ZR_TRUE;
    }
    
    // 类型不兼容，报告错误
    ZrParser_TypeError_Report(cs, "Type mismatch", toType, fromType, location);
    return ZR_FALSE;
}

// 检查赋值兼容性
TZrBool ZrParser_AssignmentCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *leftType, const SZrInferredType *rightType, SZrFileRange location) {
    if (cs == ZR_NULL || leftType == ZR_NULL || rightType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 首先检查基本类型兼容性
    if (!ZrParser_TypeCompatibility_Check(cs, rightType, leftType, location)) {
        return ZR_FALSE;
    }
    
    // 检查范围约束（如果目标类型有范围约束）
    if (leftType->hasRangeConstraint) {
        // 对于字面量，在 ZrParser_LiteralRange_Check 中已经检查
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
TZrBool ZrParser_FunctionCallCompatibility_Check(SZrCompilerState *cs,
                                        SZrTypeEnvironment *env,
                                        SZrString *funcName,
                                        SZrFunctionCall *call,
                                        SZrFunctionTypeInfo *funcType,
                                        SZrFileRange location) {
    SZrArray argTypes;
    TZrBool mismatch = ZR_FALSE;

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
        SZrInferredType *argType = (SZrInferredType *) ZrCore_Array_Get(&argTypes, i);
        SZrInferredType *paramType = (SZrInferredType *) ZrCore_Array_Get(&funcType->paramTypes, i);

        if (argType == ZR_NULL || paramType == ZR_NULL) {
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }

        if (!ZrParser_InferredType_IsCompatible(argType, paramType)) {
            ZrParser_TypeError_Report(cs, "Argument type mismatch", paramType, argType, location);
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }
    }

    free_inferred_type_array(cs->state, &argTypes);
    return ZR_TRUE;
}

// 从字面量推断类型
TZrBool ZrParser_LiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
            return ZR_TRUE;
            
        case ZR_AST_INTEGER_LITERAL: {
            // 未加后缀的整数字面量统一按 int64 推断。
            // 后续若需要字面量收窄，应由语义层在约束上下文中完成，而不是在基础推断阶段直接缩小。
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_INT64);
            return ZR_TRUE;
        }
            
        case ZR_AST_FLOAT_LITERAL:
            // 默认使用DOUBLE
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
            
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_STRING);
            return ZR_TRUE;
            
        case ZR_AST_CHAR_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_INT8);
            return ZR_TRUE;
            
        case ZR_AST_NULL_LITERAL:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_NULL);
            result->isNullable = ZR_TRUE;
            return ZR_TRUE;
            
        default:
            return ZR_FALSE;
    }
}

// 从标识符推断类型
TZrBool ZrParser_IdentifierType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 在类型环境中查找变量类型
    if (cs->typeEnv != ZR_NULL) {
        if (ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, name, result)) {
            return ZR_TRUE;
        }
    }
    
    // 未找到变量类型，不立即报错
    // 可能是全局对象 zr 的属性访问，或者子函数，或者全局对象的其他属性
    // 返回默认的 OBJECT 类型，让 compile_identifier 继续处理
    // compile_identifier 会尝试作为全局对象属性访问、子函数访问等
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从一元表达式推断类型
TZrBool ZrParser_UnaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_UNARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TZrChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;

    if ((strcmp(op, "new") == 0 || strcmp(op, "$") == 0) && arg != ZR_NULL) {
        SZrString *typeName = extract_constructed_type_name(arg);
        if (typeName != ZR_NULL) {
            ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
            return ZR_TRUE;
        }
    }
    
    // 推断操作数类型
    SZrInferredType argType;
    ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, arg, &argType)) {
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_FALSE;
    }
    
    if (strcmp(op, "!") == 0) {
        // 逻辑非：结果类型是bool
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_TRUE;
    } else if (strcmp(op, "~") == 0) {
        // 位非：结果类型与操作数类型相同（整数类型）
        ZrParser_InferredType_Copy(cs->state, result, &argType);
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_TRUE;
    } else if (strcmp(op, "-") == 0 || strcmp(op, "+") == 0) {
        // 取负/正号：结果类型与操作数类型相同
        ZrParser_InferredType_Copy(cs->state, result, &argType);
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_TRUE;
    }
    
    ZrParser_InferredType_Free(cs->state, &argType);
    return ZR_FALSE;
}

// 从二元表达式推断类型
TZrBool ZrParser_BinaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    const TZrChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    
    // 推断左右操作数类型
    SZrInferredType leftType, rightType;
    TZrBool hasLeftType = ZR_FALSE;
    TZrBool hasRightType = ZR_FALSE;
    ZrParser_InferredType_Init(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &rightType, ZR_VALUE_TYPE_OBJECT);
    hasLeftType = ZrParser_ExpressionType_Infer(cs, left, &leftType);
    hasRightType = hasLeftType ? ZrParser_ExpressionType_Infer(cs, right, &rightType) : ZR_FALSE;
    if (!hasLeftType || !hasRightType) {
        if (hasLeftType) {
            ZrParser_InferredType_Free(cs->state, &leftType);
        }
        if (hasRightType) {
            ZrParser_InferredType_Free(cs->state, &rightType);
        }
        return ZR_FALSE;
    }
    
    // 根据操作符确定结果类型
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || 
        strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
        // 比较运算符：结果类型是bool
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        // 逻辑运算符：结果类型是bool
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || 
               strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || 
               strcmp(op, "%") == 0 || strcmp(op, "**") == 0) {
        // 算术运算符：获取公共类型（类型提升）
        if (!ZrParser_InferredType_GetCommonType(cs->state, result, &leftType, &rightType)) {
            // 对类成员访问、动态调用等暂未精确建模的表达式，降级为 object，
            // 避免在 M3 运行时闭环阶段被 M6 的类型系统债务阻塞。
            if (leftType.baseType == ZR_VALUE_TYPE_OBJECT || rightType.baseType == ZR_VALUE_TYPE_OBJECT) {
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Free(cs->state, &leftType);
                ZrParser_InferredType_Free(cs->state, &rightType);
                return ZR_TRUE;
            }

            ZrParser_TypeError_Report(cs, "Incompatible types for arithmetic operation", &leftType, &rightType, node->location);
            ZrParser_InferredType_Free(cs->state, &leftType);
            ZrParser_InferredType_Free(cs->state, &rightType);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    } else if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0 ||
               strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0) {
        // 位运算符：结果类型与左操作数类型相同（整数类型）
        ZrParser_InferredType_Copy(cs->state, result, &leftType);
        ZrParser_InferredType_Free(cs->state, &leftType);
        ZrParser_InferredType_Free(cs->state, &rightType);
        return ZR_TRUE;
    }
    
    ZrParser_InferredType_Free(cs->state, &leftType);
    ZrParser_InferredType_Free(cs->state, &rightType);
    return ZR_FALSE;
}

// 从函数调用推断类型
TZrBool ZrParser_FunctionCallType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
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
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从Lambda表达式推断类型
TZrBool ZrParser_LambdaType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }
    
    // Lambda表达式返回函数类型
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_CLOSURE);
    return ZR_TRUE;
}

// 检查数组字面量大小是否符合约束
static TZrBool check_array_literal_size(SZrCompilerState *cs, SZrAstNode *arrayLiteralNode, const SZrInferredType *targetType, SZrFileRange location) {
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
            static TZrChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size mismatch: expected %zu elements, got %zu",
                    targetType->arrayFixedSize, arraySize);
            ZrParser_TypeError_Report(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    // 检查范围约束
    if (targetType->arrayMinSize > 0) {
        if (arraySize < targetType->arrayMinSize) {
            static TZrChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size too small: expected at least %zu elements, got %zu",
                    targetType->arrayMinSize, arraySize);
            ZrParser_TypeError_Report(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    if (targetType->arrayMaxSize > 0) {
        if (arraySize > targetType->arrayMaxSize) {
            static TZrChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array literal size too large: expected at most %zu elements, got %zu",
                    targetType->arrayMaxSize, arraySize);
            ZrParser_TypeError_Report(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
    }
    
    return ZR_TRUE;
}

// 从数组字面量推断类型
TZrBool ZrParser_ArrayLiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ARRAY_LITERAL) {
        return ZR_FALSE;
    }
    
    // 数组字面量返回数组类型
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
    
    // 推断元素类型（如果需要）
    // TODO: 注意：元素类型推断需要遍历数组元素，这里暂时跳过
    // 未来可以实现元素类型推断
    // 1. 推断所有元素类型
    // 2. 找到公共类型
    // 3. 设置elementTypes
    
    return ZR_TRUE;
}

// 从对象字面量推断类型
TZrBool ZrParser_ObjectLiteralType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_OBJECT_LITERAL) {
        return ZR_FALSE;
    }
    
    // 对象字面量返回对象类型
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从条件表达式推断类型
TZrBool ZrParser_ConditionalType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
    
    // 推断then和else分支类型
    SZrInferredType thenType, elseType;
    TZrBool hasThenType = ZR_FALSE;
    TZrBool hasElseType = ZR_FALSE;
    ZrParser_InferredType_Init(cs->state, &thenType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &elseType, ZR_VALUE_TYPE_OBJECT);
    hasThenType = ZrParser_ExpressionType_Infer(cs, condExpr->consequent, &thenType);
    hasElseType = hasThenType ? ZrParser_ExpressionType_Infer(cs, condExpr->alternate, &elseType) : ZR_FALSE;
    if (!hasThenType || !hasElseType) {
        if (hasThenType) {
            ZrParser_InferredType_Free(cs->state, &thenType);
        }
        if (hasElseType) {
            ZrParser_InferredType_Free(cs->state, &elseType);
        }
        return ZR_FALSE;
    }
    
    // 获取公共类型
    if (!ZrParser_InferredType_GetCommonType(cs->state, result, &thenType, &elseType)) {
        // 类型不兼容，报告错误
        ZrParser_TypeError_Report(cs, "Incompatible types in conditional expression branches", &thenType, &elseType, node->location);
        ZrParser_InferredType_Free(cs->state, &thenType);
        ZrParser_InferredType_Free(cs->state, &elseType);
        return ZR_FALSE;
    }
    
    ZrParser_InferredType_Free(cs->state, &thenType);
    ZrParser_InferredType_Free(cs->state, &elseType);
    return ZR_TRUE;
}

// 从赋值表达式推断类型
TZrBool ZrParser_AssignmentType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
    
    // 推断右值类型
    if (!ZrParser_ExpressionType_Infer(cs, assignExpr->right, result)) {
        return ZR_FALSE;
    }
    
    // 检查与左值类型的兼容性
    // 1. 推断左值类型
    SZrInferredType leftType;
    ZrParser_InferredType_Init(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, assignExpr->left, &leftType)) {
        // 2. 检查类型兼容性
        if (leftType.baseType != ZR_VALUE_TYPE_OBJECT && result->baseType != ZR_VALUE_TYPE_OBJECT &&
            !ZrParser_InferredType_IsCompatible(result, &leftType)) {
            // 3. 报告错误如果不兼容
            ZrParser_TypeError_Report(cs, "Assignment type mismatch", &leftType, result, node->location);
            ZrParser_InferredType_Free(cs->state, &leftType);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Free(cs->state, &leftType);
    }
    
    return ZR_TRUE;
}

// 从primary expression推断类型（包括函数调用）
TZrBool ZrParser_PrimaryExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    
    // 如果没有members，直接推断property的类型
    if (primary->members == ZR_NULL || primary->members->count == 0) {
        if (primary->property != ZR_NULL) {
            return ZrParser_ExpressionType_Infer(cs, primary->property, result);
        }
        // 如果没有property，返回对象类型
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
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
                    ZrParser_InferredType_Init(cs->state, &objType, ZR_VALUE_TYPE_OBJECT);
                    if (ZrParser_ExpressionType_Infer(cs, primary->property, &objType)) {
                        // 对于对象类型，方法调用返回对象类型
                        // 未来可以查找对象类型的方法定义来获取精确的返回类型
                        // 目前需要结构体/类的类型信息来查找方法，这是更高级的功能
                        // 如果objType有typeName，可以尝试从类型环境查找方法定义
                        if (objType.typeName != ZR_NULL && cs->typeEnv != ZR_NULL) {
                            // 可以尝试查找类型的方法定义，但需要更复杂的类型系统支持
                            // TODO: 暂时使用默认的对象类型
                        }
                        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        ZrParser_InferredType_Free(cs->state, &objType);
                        return ZR_TRUE;
                    }
                    ZrParser_InferredType_Free(cs->state, &objType);
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
                TZrBool hasRuntimeFunction = ZR_FALSE;
                TZrBool hasCompileTimeFunction = ZR_FALSE;

                if (cs->typeEnv != ZR_NULL) {
                    hasRuntimeFunction = ZrParser_TypeEnvironment_LookupFunction(cs->typeEnv, funcName, &funcTypeInfo);
                    funcTypeInfo = ZR_NULL;
                }

                if (hasRuntimeFunction &&
                    resolve_best_function_overload(cs,
                                                   cs->typeEnv,
                                                   funcName,
                                                   call,
                                                   node->location,
                                                   &funcTypeInfo)) {
                    ZrParser_InferredType_Copy(cs->state, result, &funcTypeInfo->returnType);
                    ZrParser_FunctionCallCompatibility_Check(cs,
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
                        ZrParser_TypeEnvironment_LookupFunction(cs->compileTimeTypeEnv, funcName, &funcTypeInfo);
                    funcTypeInfo = ZR_NULL;
                }

                if (hasCompileTimeFunction &&
                    resolve_best_function_overload(cs,
                                                   cs->compileTimeTypeEnv,
                                                   funcName,
                                                   call,
                                                   node->location,
                                                   &funcTypeInfo)) {
                    ZrParser_InferredType_Copy(cs->state, result, &funcTypeInfo->returnType);
                    ZrParser_FunctionCallCompatibility_Check(cs,
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
                if (cs->typeEnv != ZR_NULL && ZrParser_TypeEnvironment_LookupType(cs->typeEnv, funcName)) {
                    // 找到类型名称，推断返回类型为对应的 struct 类型
                    ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, funcName);
                    return ZR_TRUE;
                }

                // import 是运行时内建函数，由宿主负责解析模块，返回对象类型。
                if (zr_string_equals_cstr(funcName, "import")) {
                    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                    return ZR_TRUE;
                }
                
                // 函数和类型都未找到时，保持动态 object fallback。
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        }
        
        // property不是标识符，或者函数名未找到，返回对象类型
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }
    
    // 不是函数调用，或者是成员访问等其他情况
    // 先推断property的类型，然后根据members推断最终类型
    // 实现完整的成员访问链类型推断（如 obj.prop）
    if (primary->property != ZR_NULL) {
        SZrInferredType baseType;
        ZrParser_InferredType_Init(cs->state, &baseType, ZR_VALUE_TYPE_OBJECT);
        if (ZrParser_ExpressionType_Infer(cs, primary->property, &baseType)) {
            // 如果有members，需要根据members推断最终类型
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                // 遍历members链，逐步推断类型
                SZrInferredType currentType;
                ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Copy(cs->state, &currentType, &baseType);
                ZrParser_InferredType_Free(cs->state, &baseType);
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    SZrAstNode *member = primary->members->nodes[i];
                    if (member != ZR_NULL && member->type == ZR_AST_MEMBER_EXPRESSION) {
                        // 成员访问：从当前类型推断成员类型
                        // TODO: 注意：这需要类型系统支持，暂时返回对象类型
                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        return ZR_TRUE;
                    }
                }
                ZrParser_InferredType_Copy(cs->state, result, &currentType);
                ZrParser_InferredType_Free(cs->state, &currentType);
                return ZR_TRUE;
            } else {
                // 没有members，直接返回property的类型
                ZrParser_InferredType_Copy(cs->state, result, &baseType);
                ZrParser_InferredType_Free(cs->state, &baseType);
                return ZR_TRUE;
            }
        }
    }
    
    // 默认返回对象类型
    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

// 从AST节点推断类型（主入口函数）
TZrBool ZrParser_ExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result) {
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
            return ZrParser_LiteralType_Infer(cs, node, result);
        
        case ZR_AST_IDENTIFIER_LITERAL:
            return ZrParser_IdentifierType_Infer(cs, node, result);
        
        // 表达式
        case ZR_AST_BINARY_EXPRESSION:
            return ZrParser_BinaryExpressionType_Infer(cs, node, result);
        
        case ZR_AST_UNARY_EXPRESSION:
            return ZrParser_UnaryExpressionType_Infer(cs, node, result);
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ZrParser_ConditionalType_Infer(cs, node, result);
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ZrParser_AssignmentType_Infer(cs, node, result);
        
        case ZR_AST_FUNCTION_CALL:
            return ZrParser_FunctionCallType_Infer(cs, node, result);
        
        case ZR_AST_LAMBDA_EXPRESSION:
            return ZrParser_LambdaType_Infer(cs, node, result);
        
        case ZR_AST_ARRAY_LITERAL:
            return ZrParser_ArrayLiteralType_Infer(cs, node, result);
        
        case ZR_AST_OBJECT_LITERAL:
            return ZrParser_ObjectLiteralType_Infer(cs, node, result);
        
        // TODO: 处理其他表达式类型
        case ZR_AST_PRIMARY_EXPRESSION:
            return ZrParser_PrimaryExpressionType_Infer(cs, node, result);
        
        case ZR_AST_MEMBER_EXPRESSION:
            // 实现member expression的类型推断
            // member expression的类型推断需要知道对象类型和成员名称
            // TODO: 这里简化处理，返回对象类型
            // 完整的实现需要从对象类型查找成员定义
            // TODO: 注意：member expression的类型推断需要知道对象类型，暂时返回对象类型
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
        
        case ZR_AST_IF_EXPRESSION:
            // 实现if expression的类型推断
            // if expression的类型是thenExpr和elseExpr的公共类型
            {
                SZrIfExpression *ifExpr = &node->data.ifExpression;
                SZrInferredType thenType, elseType;
                ZrParser_InferredType_Init(cs->state, &thenType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(cs->state, &elseType, ZR_VALUE_TYPE_OBJECT);
                if (ifExpr->thenExpr != ZR_NULL && ifExpr->elseExpr != ZR_NULL) {
                    if (ZrParser_ExpressionType_Infer(cs, ifExpr->thenExpr, &thenType) &&
                        ZrParser_ExpressionType_Infer(cs, ifExpr->elseExpr, &elseType)) {
                        // 获取公共类型
                        if (ZrParser_InferredType_GetCommonType(cs->state, result, &thenType, &elseType)) {
                            ZrParser_InferredType_Free(cs->state, &thenType);
                            ZrParser_InferredType_Free(cs->state, &elseType);
                            return ZR_TRUE;
                        }
                        ZrParser_InferredType_Free(cs->state, &thenType);
                        ZrParser_InferredType_Free(cs->state, &elseType);
                    }
                } else if (ifExpr->thenExpr != ZR_NULL) {
                    return ZrParser_ExpressionType_Infer(cs, ifExpr->thenExpr, result);
                } else if (ifExpr->elseExpr != ZR_NULL) {
                    return ZrParser_ExpressionType_Infer(cs, ifExpr->elseExpr, result);
                }
                // 默认返回对象类型
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        case ZR_AST_SWITCH_EXPRESSION:
            // 实现switch expression的类型推断
            // switch expression的类型是所有case和default的公共类型
            {
                SZrSwitchExpression *switchExpr = &node->data.switchExpression;
                SZrInferredType commonType;
                ZrParser_InferredType_Init(cs->state, &commonType, ZR_VALUE_TYPE_OBJECT);
                TZrBool hasType = ZR_FALSE;
                
                // 遍历所有case，推断类型
                if (switchExpr->cases != ZR_NULL) {
                    for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
                        SZrAstNode *caseNode = switchExpr->cases->nodes[i];
                        if (caseNode != ZR_NULL && caseNode->type == ZR_AST_SWITCH_CASE) {
                            SZrSwitchCase *switchCase = &caseNode->data.switchCase;
                            if (switchCase->block != ZR_NULL) {
                                SZrInferredType caseType;
                                ZrParser_InferredType_Init(cs->state, &caseType, ZR_VALUE_TYPE_OBJECT);
                                if (ZrParser_ExpressionType_Infer(cs, switchCase->block, &caseType)) {
                                    if (!hasType) {
                                        ZrParser_InferredType_Copy(cs->state, &commonType, &caseType);
                                        hasType = ZR_TRUE;
                                    } else {
                                        SZrInferredType newCommonType;
                                        ZrParser_InferredType_Init(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                        if (ZrParser_InferredType_GetCommonType(cs->state, &newCommonType, &commonType, &caseType)) {
                                            ZrParser_InferredType_Free(cs->state, &commonType);
                                            commonType = newCommonType;
                                        }
                                    }
                                    ZrParser_InferredType_Free(cs->state, &caseType);
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
                        ZrParser_InferredType_Init(cs->state, &defaultType, ZR_VALUE_TYPE_OBJECT);
                        if (ZrParser_ExpressionType_Infer(cs, switchDefault->block, &defaultType)) {
                            if (!hasType) {
                                ZrParser_InferredType_Copy(cs->state, &commonType, &defaultType);
                                hasType = ZR_TRUE;
                            } else {
                                SZrInferredType newCommonType;
                                ZrParser_InferredType_Init(cs->state, &newCommonType, ZR_VALUE_TYPE_OBJECT);
                                if (ZrParser_InferredType_GetCommonType(cs->state, &newCommonType, &commonType, &defaultType)) {
                                    ZrParser_InferredType_Free(cs->state, &commonType);
                                    commonType = newCommonType;
                                }
                            }
                            ZrParser_InferredType_Free(cs->state, &defaultType);
                        }
                    }
                }
                
                if (hasType) {
                    ZrParser_InferredType_Copy(cs->state, result, &commonType);
                    ZrParser_InferredType_Free(cs->state, &commonType);
                    return ZR_TRUE;
                }
                
                // 默认返回对象类型
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        
        default:
            return ZR_FALSE;
    }
}

// 将AST类型注解转换为推断类型
TZrBool ZrParser_AstTypeToInferredType_Convert(SZrCompilerState *cs, const SZrType *astType, SZrInferredType *result) {
    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 如果没有类型注解，返回对象类型
    if (astType == ZR_NULL || astType->name == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        return ZR_TRUE;
    }
    
    EZrValueType baseType = ZR_VALUE_TYPE_OBJECT;
    
    // 根据类型名称节点的类型处理
    if (astType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        // 标识符类型（如 int, float, bool, string, 或用户定义类型）
        SZrString *typeName = astType->name->data.identifier.name;
        if (typeName == ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            result->ownershipQualifier = astType->ownershipQualifier;
            return ZR_TRUE;
        }
        
        // 获取类型名称字符串
        TZrNativeString nameStr;
        TZrSize nameLen;
        if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            nameStr = ZrCore_String_GetNativeStringShort(typeName);
            nameLen = typeName->shortStringLength;
        } else {
            nameStr = ZrCore_String_GetNativeString(typeName);
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
            ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
            result->ownershipQualifier = astType->ownershipQualifier;
            if (cs->semanticContext != ZR_NULL) {
                ZrParser_Semantic_RegisterNamedType(cs->semanticContext,
                                            typeName,
                                            ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                            astType->name);
                ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
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
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            result->ownershipQualifier = astType->ownershipQualifier;
            return ZR_TRUE;
        }

        ZrParser_InferredType_InitFull(cs->state,
                               result,
                               ZR_VALUE_TYPE_OBJECT,
                               ZR_FALSE,
                               genericType->name->name);
        result->ownershipQualifier = astType->ownershipQualifier;

        if (genericType->params != ZR_NULL && genericType->params->count > 0) {
            ZrCore_Array_Init(cs->state,
                        &result->elementTypes,
                        sizeof(SZrInferredType),
                        genericType->params->count);
            for (TZrSize i = 0; i < genericType->params->count; i++) {
                SZrAstNode *paramNode = genericType->params->nodes[i];
                SZrInferredType paramType;

                if (paramNode == ZR_NULL || paramNode->type != ZR_AST_TYPE) {
                    ZrParser_InferredType_Free(cs->state, result);
                    ZrParser_Compiler_Error(cs, "Generic type parameter must be a type annotation", astType->name->location);
                    return ZR_FALSE;
                }

                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                if (!ZrParser_AstTypeToInferredType_Convert(cs, &paramNode->data.type, &paramType)) {
                    ZrParser_InferredType_Free(cs->state, &paramType);
                    ZrParser_InferredType_Free(cs->state, result);
                    return ZR_FALSE;
                }

                ZrCore_Array_Push(cs->state, &result->elementTypes, &paramType);
            }
        }

        canonicalName = build_generic_instance_name(cs->state,
                                                    genericType->name->name,
                                                    &result->elementTypes);
        if (canonicalName != ZR_NULL) {
            result->typeName = canonicalName;
        }

        if (cs->semanticContext != ZR_NULL) {
            ZrParser_Semantic_RegisterNamedType(cs->semanticContext,
                                        genericType->name->name,
                                        ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                        astType->name);
            ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                                           result,
                                           ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
                                           result->typeName,
                                           astType->name);
        }
        return ZR_TRUE;
    } else if (astType->name->type == ZR_AST_TUPLE_TYPE) {
        // 元组类型（TODO: 处理元组）
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        result->ownershipQualifier = astType->ownershipQualifier;
        return ZR_TRUE;
    } else {
        // 未知类型节点类型
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        result->ownershipQualifier = astType->ownershipQualifier;
        return ZR_TRUE;
    }
    
    // 处理数组维度
    if (astType->dimensions > 0) {
        // 数组类型
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
        result->ownershipQualifier = astType->ownershipQualifier;
        // 处理元素类型
        if (astType->subType != ZR_NULL) {
            // 递归转换子类型
            SZrInferredType elementType;
            ZrParser_InferredType_Init(cs->state, &elementType, ZR_VALUE_TYPE_OBJECT);
            if (ZrParser_AstTypeToInferredType_Convert(cs, astType->subType, &elementType)) {
                // 将元素类型添加到elementTypes数组
                ZrCore_Array_Init(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
                ZrCore_Array_Push(cs->state, &result->elementTypes, &elementType);
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
        ZrParser_InferredType_Init(cs->state, result, baseType);
        result->ownershipQualifier = astType->ownershipQualifier;
    }
    
    return ZR_TRUE;
}

// 获取类型的范围限制（用于整数类型）
static void get_type_range(EZrValueType baseType, TZrInt64 *minValue, TZrInt64 *maxValue) {
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
TZrBool ZrParser_LiteralRange_Check(SZrCompilerState *cs, SZrAstNode *literalNode, const SZrInferredType *targetType, SZrFileRange location) {
    if (cs == ZR_NULL || literalNode == ZR_NULL || targetType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查整数类型字面量
    if (literalNode->type == ZR_AST_INTEGER_LITERAL && ZR_VALUE_IS_TYPE_INT(targetType->baseType)) {
        TZrInt64 literalValue = literalNode->data.integerLiteral.value;
        TZrInt64 minValue, maxValue;
        get_type_range(targetType->baseType, &minValue, &maxValue);
        
        if (literalValue < minValue || literalValue > maxValue) {
            static TZrChar errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Integer literal %lld is out of range for type (expected range: %lld to %lld)",
                    (long long)literalValue, (long long)minValue, (long long)maxValue);
            ZrParser_TypeError_Report(cs, errorMsg, targetType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 检查用户定义的范围约束
        if (targetType->hasRangeConstraint) {
            if (literalValue < targetType->minValue || literalValue > targetType->maxValue) {
                static TZrChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Integer literal %lld is out of range constraint (expected range: %lld to %lld)",
                        (long long)literalValue, (long long)targetType->minValue, (long long)targetType->maxValue);
                ZrParser_TypeError_Report(cs, errorMsg, targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 检查浮点数类型字面量（NaN, Infinity）
    if (literalNode->type == ZR_AST_FLOAT_LITERAL) {
        TZrDouble floatValue = literalNode->data.floatLiteral.value;
        // 检查是否为 NaN 或 Infinity（如果目标类型不允许）
        // 根据目标类型决定是否允许 NaN/Infinity
        if (isnan(floatValue) || isinf(floatValue)) {
            // 检查目标类型是否允许NaN/Infinity
            // 对于整数类型，不允许NaN/Infinity
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(targetType->baseType) || 
                ZR_VALUE_IS_TYPE_UNSIGNED_INT(targetType->baseType)) {
                ZrParser_TypeError_Report(cs, "NaN/Infinity cannot be assigned to integer type", 
                                 targetType, ZR_NULL, location);
                return ZR_FALSE;
            }
            // 对于浮点类型，允许NaN/Infinity
        }
    }
    
    return ZR_TRUE;
}

// 检查数组索引边界
TZrBool ZrParser_ArrayIndexBounds_Check(SZrCompilerState *cs, SZrAstNode *indexExpr, const SZrInferredType *arrayType, SZrFileRange location) {
    if (cs == ZR_NULL || indexExpr == ZR_NULL || arrayType == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 只对字面量索引进行编译期检查
    if (indexExpr->type == ZR_AST_INTEGER_LITERAL) {
        TZrInt64 indexValue = indexExpr->data.integerLiteral.value;
        
        if (indexValue < 0) {
            static TZrChar errorMsg[128];
            snprintf(errorMsg, sizeof(errorMsg),
                    "Array index %lld is negative", (long long)indexValue);
            ZrParser_TypeError_Report(cs, errorMsg, arrayType, ZR_NULL, location);
            return ZR_FALSE;
        }
        
        // 如果数组有固定大小，检查索引是否越界
        if (arrayType->hasArraySizeConstraint && arrayType->arrayFixedSize > 0) {
            if ((TZrSize)indexValue >= arrayType->arrayFixedSize) {
                static TZrChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg),
                        "Array index %lld is out of bounds (array size: %zu)",
                        (long long)indexValue, arrayType->arrayFixedSize);
                ZrParser_TypeError_Report(cs, errorMsg, arrayType, ZR_NULL, location);
                return ZR_FALSE;
            }
        }
    }
    
    // 对于非字面量索引，编译期无法检查，将在运行时检查（如果启用）
    return ZR_TRUE;
}
