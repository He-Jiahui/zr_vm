//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "type_inference_internal.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

static SZrString *type_inference_create_hidden_property_accessor_name(SZrCompilerState *cs,
                                                                      SZrString *propertyName,
                                                                      TZrBool isSetter) {
    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrNativeString propertyNameString;
    TZrSize prefixLength;
    TZrSize propertyNameLength;
    TZrSize bufferSize;
    TZrChar *buffer;
    SZrString *result;

    if (cs == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    propertyNameString = ZrCore_String_GetNativeString(propertyName);
    if (propertyNameString == ZR_NULL) {
        return ZR_NULL;
    }

    prefixLength = strlen(prefix);
    propertyNameLength = propertyName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                 ? propertyName->shortStringLength
                                 : propertyName->longStringLength;
    bufferSize = prefixLength + propertyNameLength + 1;
    buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, bufferSize);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, prefix, prefixLength);
    memcpy(buffer + prefixLength, propertyNameString, propertyNameLength);
    buffer[bufferSize - 1] = '\0';

    result = ZrCore_String_CreateFromNative(cs->state, buffer);
    ZrCore_Memory_RawFree(cs->state->global, buffer, bufferSize);
    return result;
}

//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

// 辅助函数：获取类型名称字符串（用于错误报告）
const TZrChar *get_base_type_name(EZrValueType baseType) {
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

TZrBool inferred_type_from_type_name(SZrCompilerState *cs,
                                            SZrString *typeName,
                                            SZrInferredType *result);
TZrBool ensure_native_module_compile_info(SZrCompilerState *cs, SZrString *moduleName);
static TZrBool receiver_ownership_can_call_member(EZrOwnershipQualifier receiverQualifier,
                                                  EZrOwnershipQualifier memberQualifier);
const TZrChar *receiver_ownership_call_error(EZrOwnershipQualifier receiverQualifier);

TZrBool inferred_type_try_map_primitive_name(const TZrNativeString nameStr,
                                             TZrSize nameLen,
                                             EZrValueType *outBaseType) {
    if (nameStr == ZR_NULL || outBaseType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (nameLen == 3 && memcmp(nameStr, "int", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_INT64;
        return ZR_TRUE;
    }
    if (nameLen == 4 && memcmp(nameStr, "uint", 4) == 0) {
        *outBaseType = ZR_VALUE_TYPE_UINT64;
        return ZR_TRUE;
    }
    if (nameLen == 5 && memcmp(nameStr, "float", 5) == 0) {
        *outBaseType = ZR_VALUE_TYPE_DOUBLE;
        return ZR_TRUE;
    }
    if (nameLen == 4 && memcmp(nameStr, "bool", 4) == 0) {
        *outBaseType = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }
    if (nameLen == 6 && memcmp(nameStr, "string", 6) == 0) {
        *outBaseType = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }
    if (nameLen == 4 && memcmp(nameStr, "null", 4) == 0) {
        *outBaseType = ZR_VALUE_TYPE_NULL;
        return ZR_TRUE;
    }
    if (nameLen == 4 && memcmp(nameStr, "void", 4) == 0) {
        *outBaseType = ZR_VALUE_TYPE_NULL;
        return ZR_TRUE;
    }
    if (nameLen == 2 && memcmp(nameStr, "i8", 2) == 0) {
        *outBaseType = ZR_VALUE_TYPE_INT8;
        return ZR_TRUE;
    }
    if (nameLen == 2 && memcmp(nameStr, "u8", 2) == 0) {
        *outBaseType = ZR_VALUE_TYPE_UINT8;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "i16", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_INT16;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "u16", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_UINT16;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "i32", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_INT32;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "u32", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_UINT32;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "i64", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_INT64;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "u64", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_UINT64;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "f32", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_FLOAT;
        return ZR_TRUE;
    }
    if (nameLen == 3 && memcmp(nameStr, "f64", 3) == 0) {
        *outBaseType = ZR_VALUE_TYPE_DOUBLE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

void free_inferred_type_array(SZrState *state, SZrArray *types) {
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

SZrString *build_generic_instance_name(SZrState *state,
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

TZrBool infer_function_call_argument_types_for_candidate(SZrCompilerState *cs,
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

TZrBool resolve_best_function_overload(SZrCompilerState *cs,
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

TZrBool ZrParser_FunctionCallOverload_Resolve(SZrCompilerState *cs,
                                              SZrTypeEnvironment *env,
                                              SZrString *funcName,
                                              SZrFunctionCall *call,
                                              SZrFileRange location,
                                              SZrFunctionTypeInfo **resolvedFunction) {
    return resolve_best_function_overload(cs,
                                          env,
                                          funcName,
                                          call,
                                          location,
                                          resolvedFunction);
}

TZrBool zr_string_equals_cstr(SZrString *value, const TZrChar *literal) {
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

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    if (cs->currentTypePrototypeInfo != ZR_NULL &&
        cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    return ZR_NULL;
}

static SZrTypeMemberInfo *find_compiler_type_member_recursive_inference(SZrCompilerState *cs,
                                                                        SZrTypePrototypeInfo *info,
                                                                        SZrString *memberName,
                                                                        TZrUInt32 depth) {
    if (cs == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || depth > 32) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (memberInfo != ZR_NULL &&
            memberInfo->name != ZR_NULL &&
            ZrCore_String_Equal(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    for (TZrSize i = 0; i < info->inherits.length; i++) {
        SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&info->inherits, i);
        if (inheritTypeNamePtr == ZR_NULL || *inheritTypeNamePtr == ZR_NULL) {
            continue;
        }

        {
            SZrTypePrototypeInfo *superInfo = find_compiler_type_prototype_inference(cs, *inheritTypeNamePtr);
            SZrTypeMemberInfo *inheritedMember;
            if (superInfo == ZR_NULL || superInfo == info) {
                continue;
            }

            inheritedMember = find_compiler_type_member_recursive_inference(cs, superInfo, memberName, depth + 1);
            if (inheritedMember != ZR_NULL) {
                return inheritedMember;
            }
        }
    }

    return ZR_NULL;
}

SZrTypeMemberInfo *find_compiler_type_member_inference(SZrCompilerState *cs,
                                                       SZrString *typeName,
                                                       SZrString *memberName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype_inference(cs, typeName);
    SZrTypeMemberInfo *memberInfo;
    SZrString *accessorName;

    if (info == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    memberInfo = find_compiler_type_member_recursive_inference(cs, info, memberName, 0);
    if (memberInfo != ZR_NULL) {
        return memberInfo;
    }

    accessorName = type_inference_create_hidden_property_accessor_name(cs, memberName, ZR_FALSE);
    if (accessorName != ZR_NULL) {
        memberInfo = find_compiler_type_member_recursive_inference(cs, info, accessorName, 0);
        if (memberInfo != ZR_NULL) {
            return memberInfo;
        }
    }

    accessorName = type_inference_create_hidden_property_accessor_name(cs, memberName, ZR_TRUE);
    if (accessorName != ZR_NULL) {
        memberInfo = find_compiler_type_member_recursive_inference(cs, info, accessorName, 0);
        if (memberInfo != ZR_NULL) {
            return memberInfo;
        }
    }

    return ZR_NULL;
}

TZrBool type_name_is_module_prototype_inference(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype_inference(cs, typeName);
    return info != ZR_NULL && info->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
}

TZrBool resolve_prototype_target_inference(SZrCompilerState *cs,
                                           SZrAstNode *node,
                                           SZrTypePrototypeInfo **outPrototype,
                                           SZrString **outTypeName) {
    SZrAstNode *targetNode = node;
    SZrString *typeName = ZR_NULL;
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    SZrInferredType inferredType;

    if (outPrototype != ZR_NULL) {
        *outPrototype = ZR_NULL;
    }
    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        targetNode = node->data.prototypeReferenceExpression.target;
    }
    if (targetNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        typeName = targetNode->data.identifier.name;
        prototype = find_compiler_type_prototype_inference(cs, typeName);
    }

    if (prototype == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, targetNode, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }
        typeName = inferredType.typeName;
        prototype = find_compiler_type_prototype_inference(cs, typeName);
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }

    if (prototype == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outPrototype != ZR_NULL) {
        *outPrototype = prototype;
    }
    if (outTypeName != ZR_NULL) {
        *outTypeName = typeName;
    }
    return ZR_TRUE;
}

TZrBool infer_prototype_reference_type(SZrCompilerState *cs,
                                       SZrAstNode *node,
                                       SZrInferredType *result) {
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    SZrString *typeName = ZR_NULL;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!resolve_prototype_target_inference(cs, node, &prototype, &typeName)) {
        ZrParser_Compiler_Error(cs,
                        "Prototype reference target must resolve to a registered prototype",
                        node->location);
        return ZR_FALSE;
    }

    return inferred_type_from_type_name(cs, typeName, result);
}

TZrBool infer_construct_expression_type(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        SZrInferredType *result) {
    SZrConstructExpression *construct;
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    SZrString *typeName = ZR_NULL;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        node->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        return ZR_FALSE;
    }

    construct = &node->data.constructExpression;
    if (!construct->isNew &&
        (construct->isUsing ||
         construct->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE)) {
        if (!ZrParser_ExpressionType_Infer(cs, construct->target, result)) {
            return ZR_FALSE;
        }

        result->ownershipQualifier = construct->isUsing
                                             ? ZR_OWNERSHIP_QUALIFIER_UNIQUE
                                             : construct->ownershipQualifier;
        return ZR_TRUE;
    }

    if (!resolve_prototype_target_inference(cs, construct->target, &prototype, &typeName)) {
        ZrParser_Compiler_Error(cs,
                        "Construct target must resolve to a registered prototype",
                        node->location);
        return ZR_FALSE;
    }

    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
        ZrParser_Compiler_Error(cs, "Interfaces cannot be constructed", node->location);
        return ZR_FALSE;
    }

    if (!construct->isNew && !prototype->allowValueConstruction) {
        ZrParser_Compiler_Error(cs, "Prototype does not allow value construction", node->location);
        return ZR_FALSE;
    }

    if (construct->isNew && !prototype->allowBoxedConstruction) {
        ZrParser_Compiler_Error(cs, "Prototype does not allow boxed construction", node->location);
        return ZR_FALSE;
    }

    if (!inferred_type_from_type_name(cs, typeName, result)) {
        return ZR_FALSE;
    }

    result->ownershipQualifier = construct->isUsing
                                     ? ZR_OWNERSHIP_QUALIFIER_UNIQUE
                                     : construct->ownershipQualifier;
    return ZR_TRUE;
}

static TZrBool resolve_constructor_chain_member_type_inference(SZrCompilerState *cs, SZrString **ioTypeName,
                                                               SZrAstNode *memberNode) {
    SZrMemberExpression *memberExpr;
    SZrString *memberName;
    SZrTypeMemberInfo *memberInfo;

    if (cs == ZR_NULL || ioTypeName == ZR_NULL || *ioTypeName == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_FALSE;
    }

    memberExpr = &memberNode->data.memberExpression;
    if (memberExpr->computed || memberExpr->property == ZR_NULL ||
        memberExpr->property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    memberName = memberExpr->property->data.identifier.name;
    memberInfo = find_compiler_type_member_inference(cs, *ioTypeName, memberName);
    if (memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
        memberInfo->fieldTypeName != ZR_NULL) {
        *ioTypeName = memberInfo->fieldTypeName;
        return ZR_TRUE;
    }

    if ((memberInfo->memberType == ZR_AST_STRUCT_METHOD || memberInfo->memberType == ZR_AST_CLASS_METHOD ||
         memberInfo->memberType == ZR_AST_STRUCT_META_FUNCTION ||
         memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION) &&
        memberInfo->returnTypeName != ZR_NULL) {
        *ioTypeName = memberInfo->returnTypeName;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool consume_constructor_chain_members_inference(SZrCompilerState *cs, SZrString **ioTypeName,
                                                           SZrAstNodeArray *members, TZrSize *outConsumedCount) {
    TZrSize consumedCount = 0;

    if (outConsumedCount != ZR_NULL) {
        *outConsumedCount = 0;
    }
    if (members == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize i = 0; i < members->count; i++) {
        SZrAstNode *memberNode = members->nodes[i];

        if (memberNode == ZR_NULL) {
            consumedCount = i + 1;
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            consumedCount = i + 1;
            if (outConsumedCount != ZR_NULL) {
                *outConsumedCount = consumedCount;
            }
            return ZR_TRUE;
        }

        if (!resolve_constructor_chain_member_type_inference(cs, ioTypeName, memberNode)) {
            return ZR_FALSE;
        }

        consumedCount = i + 1;
        if (!type_name_is_module_prototype_inference(cs, *ioTypeName)) {
            if (i + 1 < members->count && members->nodes[i + 1] != ZR_NULL &&
                members->nodes[i + 1]->type == ZR_AST_FUNCTION_CALL) {
                consumedCount = i + 2;
            }
            if (outConsumedCount != ZR_NULL) {
                *outConsumedCount = consumedCount;
            }
            return ZR_TRUE;
        }
    }

    if (outConsumedCount != ZR_NULL) {
        *outConsumedCount = consumedCount;
    }
    return ZR_TRUE;
}
