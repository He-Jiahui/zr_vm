//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "compiler_internal.h"
#include "compiler_type_declaration.h"
#include "compiler_type_value_alias.h"
#include "type_inference_internal.h"
#include "type_inference_generic_prototype.h"
#include "type_inference_generic_substitution.h"
#include "type_inference_nominal_prototype.h"
#include "type_inference_nominal_member.h"
#include "type_inference_call_diagnostics.h"
#include "compiler/statement/iteration_contract.h"
#include "type_inference_semantic_facts.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define ZR_TYPE_INFERENCE_GENERIC_START_NOT_FOUND ((TZrSize)-1)
#define ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE ((TZrInt32)-1)

TZrBool resolve_expression_root_type(SZrCompilerState *cs,
                                     SZrAstNode *node,
                                     SZrString **outTypeName,
                                     TZrBool *outIsTypeReference);


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
    if (nameLen == 6 && memcmp(nameStr, "object", 6) == 0) {
        *outBaseType = ZR_VALUE_TYPE_OBJECT;
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


static TZrBool append_inferred_type_display_name(const SZrInferredType *type,
                                               TZrChar *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *offset) {
    const TZrChar *baseName;

    if (type == ZR_NULL) {
        return type_inference_append_type_text(buffer, bufferSize, offset, "object");
    }

    if (type->typeName != ZR_NULL) {
        return type_inference_append_type_text(buffer,
                                    bufferSize,
                                    offset,
                                    ZrCore_String_GetNativeString(type->typeName));
    }

    if (type->baseType == ZR_VALUE_TYPE_ARRAY && type->elementTypes.length > 0) {
        SZrInferredType *elementType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0);
        if (!append_inferred_type_display_name(elementType, buffer, bufferSize, offset)) {
            return ZR_FALSE;
        }
        if (type->hasArraySizeConstraint && type->arrayFixedSize > 0) {
            TZrChar sizeBuffer[ZR_PARSER_ARRAY_SIZE_BUFFER_LENGTH];
            TZrInt32 written = snprintf(sizeBuffer, sizeof(sizeBuffer), "[%zu]", type->arrayFixedSize);
            if (written <= 0 || (TZrSize)written >= sizeof(sizeBuffer)) {
                return ZR_FALSE;
            }
            return type_inference_append_type_text(buffer, bufferSize, offset, sizeBuffer);
        }
        return type_inference_append_type_text(buffer, bufferSize, offset, "[]");
    }

    baseName = get_base_type_name(type->baseType);
    if (baseName == ZR_NULL) {
        baseName = "object";
    }
    return type_inference_append_type_text(buffer, bufferSize, offset, baseName);
}

SZrString *build_generic_instance_name(SZrState *state,
                                       SZrString *baseName,
                                       const SZrArray *typeArguments) {
    TZrChar buffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
    TZrSize offset = 0;

    if (state == ZR_NULL || baseName == ZR_NULL || typeArguments == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!type_inference_append_type_text(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(baseName)) ||
        !type_inference_append_type_text(buffer, sizeof(buffer), &offset, "<")) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < typeArguments->length; i++) {
        SZrInferredType *argumentType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)typeArguments, i);
        if (i > 0 && !type_inference_append_type_text(buffer, sizeof(buffer), &offset, ", ")) {
            return ZR_NULL;
        }
        if (!append_inferred_type_display_name(argumentType, buffer, sizeof(buffer), &offset)) {
            return ZR_NULL;
        }
    }

    if (!type_inference_append_type_text(buffer, sizeof(buffer), &offset, ">")) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, buffer, offset);
}

static TZrBool create_trimmed_type_name_segment(SZrState *state,
                                                const TZrChar *source,
                                                TZrSize start,
                                                TZrSize end,
                                                SZrString **outName) {
    while (start < end && (source[start] == ' ' || source[start] == '\t' || source[start] == '\r' || source[start] == '\n')) {
        start++;
    }
    while (end > start &&
           (source[end - 1] == ' ' || source[end - 1] == '\t' || source[end - 1] == '\r' || source[end - 1] == '\n')) {
        end--;
    }

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (state == ZR_NULL || source == ZR_NULL || outName == ZR_NULL || end <= start) {
        return ZR_FALSE;
    }

    *outName = ZrCore_String_Create(state, (TZrNativeString)(source + start), end - start);
    return *outName != ZR_NULL;
}

static const TZrChar *type_inference_find_top_level_last_dot(const TZrChar *typeNameText) {
    const TZrChar *lastDot = ZR_NULL;
    TZrInt32 genericDepth = 0;

    if (typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    for (const TZrChar *cursor = typeNameText; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            genericDepth++;
            continue;
        }
        if (*cursor == '>') {
            if (genericDepth > 0) {
                genericDepth--;
            }
            continue;
        }
        if (*cursor == '.' && genericDepth == 0) {
            lastDot = cursor;
        }
    }

    return lastDot;
}

static SZrString *type_inference_extract_member_lookup_name_from_type_text(SZrCompilerState *cs,
                                                                           const TZrChar *memberTypeText) {
    TZrSize memberTypeLength;
    TZrSize rootEnd;
    SZrString *memberLookupName = ZR_NULL;

    if (cs == ZR_NULL || memberTypeText == ZR_NULL || memberTypeText[0] == '\0') {
        return ZR_NULL;
    }

    memberTypeLength = strlen(memberTypeText);
    rootEnd = memberTypeLength;
    for (TZrSize index = 0; index < memberTypeLength; index++) {
        if (memberTypeText[index] == '<') {
            rootEnd = index;
            break;
        }
    }

    if (!create_trimmed_type_name_segment(cs->state, memberTypeText, 0, rootEnd, &memberLookupName)) {
        return ZR_NULL;
    }

    return memberLookupName;
}

static SZrString *type_inference_build_canonical_module_member_type_name(SZrCompilerState *cs,
                                                                         SZrString *canonicalMemberTypeName,
                                                                         SZrString *requestedMemberTypeName) {
    const TZrChar *canonicalText;
    const TZrChar *requestedText;
    const TZrChar *requestedGenericStart;
    const TZrChar *canonicalGenericStart;
    TZrSize canonicalBaseLength;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || canonicalMemberTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    canonicalText = ZrCore_String_GetNativeString(canonicalMemberTypeName);
    if (canonicalText == ZR_NULL) {
        return ZR_NULL;
    }

    requestedText = requestedMemberTypeName != ZR_NULL ? ZrCore_String_GetNativeString(requestedMemberTypeName) : ZR_NULL;
    requestedGenericStart = requestedText != ZR_NULL ? strchr(requestedText, '<') : ZR_NULL;
    if (requestedGenericStart == ZR_NULL) {
        return canonicalMemberTypeName;
    }

    canonicalGenericStart = strchr(canonicalText, '<');
    canonicalBaseLength = canonicalGenericStart != ZR_NULL ? (TZrSize)(canonicalGenericStart - canonicalText)
                                                           : strlen(canonicalText);
    written = snprintf(buffer,
                       sizeof(buffer),
                       "%.*s%s",
                       (int)canonicalBaseLength,
                       canonicalText,
                       requestedGenericStart);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static TZrBool type_prototype_pointer_is_currently_registered(SZrCompilerState *cs,
                                                              const SZrTypePrototypeInfo *info);

static SZrTypePrototypeInfo *find_registered_type_prototype_inference_exact_only_local(SZrCompilerState *cs,
                                                                                        SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL &&
            type_inference_prototype_is_root_visible(info) &&
            type_inference_generic_prototype_is_root_visible(cs, info) && ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    if (type_prototype_pointer_is_currently_registered(cs, cs->currentTypePrototypeInfo) &&
        type_inference_prototype_is_root_visible(cs->currentTypePrototypeInfo) &&
        type_inference_generic_prototype_is_root_visible(cs, cs->currentTypePrototypeInfo) &&
        cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    return ZR_NULL;
}

TZrBool try_parse_generic_instance_type_name(SZrState *state,
                                             SZrString *typeName,
                                             SZrString **outBaseName,
                                             SZrArray *outArgumentTypeNames) {
    TZrNativeString nativeTypeName;
    TZrSize nativeTypeNameLength;
    TZrSize genericStart = ZR_TYPE_INFERENCE_GENERIC_START_NOT_FOUND;
    TZrInt32 depth = 0;
    TZrSize segmentStart;

    if (outBaseName != ZR_NULL) {
        *outBaseName = ZR_NULL;
    }
    if (outArgumentTypeNames != ZR_NULL) {
        ZrCore_Array_Construct(outArgumentTypeNames);
    }
    if (state == ZR_NULL || typeName == ZR_NULL || outBaseName == ZR_NULL || outArgumentTypeNames == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeTypeName = ZrCore_String_GetNativeString(typeName);
    nativeTypeNameLength = nativeTypeName != ZR_NULL ? strlen(nativeTypeName) : 0;
    if (nativeTypeName == ZR_NULL || nativeTypeNameLength < 3) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nativeTypeNameLength; index++) {
        if (nativeTypeName[index] == '<') {
            genericStart = index;
            break;
        }
    }

    if (genericStart == ZR_TYPE_INFERENCE_GENERIC_START_NOT_FOUND || nativeTypeName[nativeTypeNameLength - 1] != '>') {
        return ZR_FALSE;
    }

    if (!create_trimmed_type_name_segment(state, nativeTypeName, 0, genericStart, outBaseName)) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state,
                      outArgumentTypeNames,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);
    segmentStart = genericStart + 1;
    depth = 0;
    for (TZrSize index = genericStart + 1; index < nativeTypeNameLength - 1; index++) {
        TZrChar current = nativeTypeName[index];
        if (current == '<') {
            depth++;
            continue;
        }
        if (current == '>') {
            if (depth == 0) {
                ZrCore_Array_Free(state, outArgumentTypeNames);
                ZrCore_Array_Construct(outArgumentTypeNames);
                *outBaseName = ZR_NULL;
                return ZR_FALSE;
            }
            depth--;
            continue;
        }
        if (current == ',' && depth == 0) {
            SZrString *argumentName;
            if (!create_trimmed_type_name_segment(state, nativeTypeName, segmentStart, index, &argumentName)) {
                ZrCore_Array_Free(state, outArgumentTypeNames);
                ZrCore_Array_Construct(outArgumentTypeNames);
                *outBaseName = ZR_NULL;
                return ZR_FALSE;
            }
            ZrCore_Array_Push(state, outArgumentTypeNames, &argumentName);
            segmentStart = index + 1;
        }
    }

    if (depth != 0) {
        ZrCore_Array_Free(state, outArgumentTypeNames);
        ZrCore_Array_Construct(outArgumentTypeNames);
        *outBaseName = ZR_NULL;
        return ZR_FALSE;
    }

    {
        SZrString *lastArgumentName;
        if (!create_trimmed_type_name_segment(state,
                                              nativeTypeName,
                                              segmentStart,
                                              nativeTypeNameLength - 1,
                                              &lastArgumentName)) {
            ZrCore_Array_Free(state, outArgumentTypeNames);
            ZrCore_Array_Construct(outArgumentTypeNames);
            *outBaseName = ZR_NULL;
            return ZR_FALSE;
        }
        ZrCore_Array_Push(state, outArgumentTypeNames, &lastArgumentName);
    }

    return outArgumentTypeNames->length > 0;
}

SZrTypePrototypeInfo *find_compiler_type_prototype_inference_exact(SZrCompilerState *cs, SZrString *typeName) {
    SZrString *bestMatchName = ZR_NULL;
    TZrBool bestMatchIsImportedNative = ZR_TRUE;
    const TZrChar *typeNameText;
    const TZrChar *lastDot;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
        if (info != ZR_NULL && info->name != ZR_NULL &&
            type_inference_prototype_is_root_visible(info) &&
            type_inference_generic_prototype_is_root_visible(cs, info) && ZrCore_String_Equal(info->name, typeName)) {
            if (bestMatchName == ZR_NULL || (bestMatchIsImportedNative && !info->isImportedNative)) {
                bestMatchName = info->name;
                bestMatchIsImportedNative = info->isImportedNative;
                if (!info->isImportedNative) {
                    return info;
                }
            }
        }
    }

    if (cs->currentTypePrototypeInfo != ZR_NULL &&
        type_inference_prototype_is_root_visible(cs->currentTypePrototypeInfo) &&
        type_inference_generic_prototype_is_root_visible(cs, cs->currentTypePrototypeInfo) &&
        cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName) &&
        (bestMatchName == ZR_NULL ||
         (bestMatchIsImportedNative && !cs->currentTypePrototypeInfo->isImportedNative))) {
        return cs->currentTypePrototypeInfo;
    }

    {
        SZrTypeBinding *binding = compiler_find_type_value_alias(cs, typeName, ZrParser_SyntaxContext_Root());
        if (binding != ZR_NULL &&
            binding->type.typeName != ZR_NULL &&
            !ZrCore_String_Equal(binding->type.typeName, typeName)) {
            SZrInferredType aliasType;
            SZrTypePrototypeInfo *aliasMatch;
            SZrString *bindingName = binding->name;

            ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Copy(cs->state, &aliasType, &binding->type);
            if (aliasType.nominalDefinitionToken != 0U ||
                (aliasType.baseType == ZR_VALUE_TYPE_OBJECT &&
                 aliasType.elementTypes.length > 0U)) {
                ensure_generic_instance_type_prototype_from_inferred(cs, &aliasType);
                aliasMatch = type_inference_find_nominal_prototype(cs, &aliasType);
                ZrParser_InferredType_Free(cs->state, &aliasType);
                return aliasMatch;
            }
            aliasMatch = find_compiler_type_prototype_inference_exact(cs, aliasType.typeName);
            if (aliasMatch == ZR_NULL) {
                if (aliasType.typeName != ZR_NULL &&
                    getenv("ZR_VM_TRACE_PROJECT_STARTUP") != ZR_NULL &&
                    ZrCore_String_GetNativeString(aliasType.typeName) != ZR_NULL &&
                    strchr(ZrCore_String_GetNativeString(aliasType.typeName), '<') != ZR_NULL) {
                    fprintf(stderr,
                            "[zr-debug-site] type_inference_core.bindingType=%s bindingName=%s lookup=%s\n",
                            ZrCore_String_GetNativeString(aliasType.typeName),
                            bindingName != ZR_NULL ? ZrCore_String_GetNativeString(bindingName) : "<null>",
                            typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeName) : "<null>");
                }
                if (ensure_import_module_compile_info(cs, aliasType.typeName)) {
                    aliasMatch = find_compiler_type_prototype_inference_exact(cs, aliasType.typeName);
                }
            }
            ZrParser_InferredType_Free(cs->state, &aliasType);
            if (aliasMatch != ZR_NULL) {
                return aliasMatch;
            }
        }
    }

    if (bestMatchName != ZR_NULL) {
        return find_registered_type_prototype_inference_exact_only_local(cs, bestMatchName);
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    lastDot = type_inference_find_top_level_last_dot(typeNameText);
    if (lastDot != ZR_NULL && lastDot > typeNameText && lastDot[1] != '\0') {
        SZrString *moduleName =
                ZrCore_String_Create(cs->state, (TZrNativeString)typeNameText, (TZrSize)(lastDot - typeNameText));
        SZrString *memberTypeName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)(lastDot + 1));
        SZrString *memberName = type_inference_extract_member_lookup_name_from_type_text(cs, lastDot + 1);
        SZrTypePrototypeInfo *moduleInfo = ZR_NULL;

        if (moduleName != ZR_NULL && memberName != ZR_NULL && memberTypeName != ZR_NULL) {
            moduleInfo = find_compiler_type_prototype_inference_exact(cs, moduleName);
            if (moduleInfo == ZR_NULL && cs->typeEnv != ZR_NULL) {
                SZrInferredType aliasType;

                ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
                if (ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, moduleName, &aliasType) &&
                    aliasType.typeName != ZR_NULL &&
                    !ZrCore_String_Equal(aliasType.typeName, moduleName)) {
                    moduleInfo = find_compiler_type_prototype_inference(cs, aliasType.typeName);
                    if (moduleInfo == ZR_NULL &&
                        aliasType.typeName != ZR_NULL &&
                        getenv("ZR_VM_TRACE_PROJECT_STARTUP") != ZR_NULL &&
                        ZrCore_String_GetNativeString(aliasType.typeName) != ZR_NULL &&
                        strchr(ZrCore_String_GetNativeString(aliasType.typeName), '<') != ZR_NULL) {
                        fprintf(stderr,
                                "[zr-debug-site] type_inference_core.aliasType=%s\n",
                                ZrCore_String_GetNativeString(aliasType.typeName));
                    }
                    if (moduleInfo == ZR_NULL && ensure_import_module_compile_info(cs, aliasType.typeName)) {
                        moduleInfo = find_compiler_type_prototype_inference(cs, aliasType.typeName);
                    }
                }
                ZrParser_InferredType_Free(cs->state, &aliasType);
            }
            if (moduleInfo == ZR_NULL && ensure_import_module_compile_info(cs, moduleName)) {
                moduleInfo = find_compiler_type_prototype_inference_exact(cs, moduleName);
            }
        }
        if (moduleInfo != ZR_NULL && moduleInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
            for (TZrSize memberIndex = 0; memberIndex < moduleInfo->members.length; memberIndex++) {
                SZrTypeMemberInfo *memberInfo =
                        (SZrTypeMemberInfo *)ZrCore_Array_Get(&moduleInfo->members, memberIndex);
                if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL &&
                    memberInfo->fieldTypeName != ZR_NULL &&
                    ZrCore_String_Equal(memberInfo->name, memberName)) {
                    SZrString *candidateTypeName =
                            type_inference_build_canonical_module_member_type_name(cs,
                                                                                   memberInfo->fieldTypeName,
                                                                                   memberTypeName);
                    if (candidateTypeName != ZR_NULL && !ZrCore_String_Equal(candidateTypeName, typeName)) {
                        SZrTypePrototypeInfo *candidateExact =
                                find_registered_type_prototype_inference_exact_only_local(cs, candidateTypeName);
                        if (candidateExact != ZR_NULL) {
                            return candidateExact;
                        }
                        if (ZrCore_String_GetNativeString(candidateTypeName) != ZR_NULL &&
                            strchr(ZrCore_String_GetNativeString(candidateTypeName), '<') != ZR_NULL) {
                            return find_compiler_type_prototype_inference_exact(cs, candidateTypeName);
                        }
                    }
                    if (!ZrCore_String_Equal(memberInfo->fieldTypeName, typeName)) {
                        SZrTypePrototypeInfo *fieldExact =
                                find_registered_type_prototype_inference_exact_only_local(cs,
                                                                                           memberInfo->fieldTypeName);
                        if (fieldExact != ZR_NULL) {
                            return fieldExact;
                        }
                    }
                    if (bestMatchName != ZR_NULL) {
                        return find_registered_type_prototype_inference_exact_only_local(cs, bestMatchName);
                    }
                    return ZR_NULL;
                }
            }
        }
    }

    if (bestMatchName != ZR_NULL) {
        return find_registered_type_prototype_inference_exact_only_local(cs, bestMatchName);
    }
    return ZR_NULL;
}

static TZrBool generic_declaration_find_parameter_kind_inference(
        SZrGenericDeclaration *generic,
        SZrString *typeName,
        EZrGenericParameterKind *outKind) {
    if (generic == ZR_NULL || generic->params == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < generic->params->count; index++) {
        SZrAstNode *parameterNode = generic->params->nodes[index];
        SZrParameter *parameter;

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &parameterNode->data.parameter;
        if (parameter->name != ZR_NULL &&
            parameter->name->name != ZR_NULL &&
            ZrCore_String_Equal(parameter->name->name, typeName)) {
            if (outKind != ZR_NULL) {
                *outKind = parameter->genericKind;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrGenericDeclaration *generic_declaration_from_function_node_inference(
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return node->data.functionDeclaration.generic;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.generic;
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.generic;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return node->data.interfaceMethodSignature.generic;
        default:
            return ZR_NULL;
    }
}

static SZrGenericDeclaration *generic_declaration_from_type_node_inference(
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            return node->data.classDeclaration.generic;
        case ZR_AST_STRUCT_DECLARATION:
            return node->data.structDeclaration.generic;
        case ZR_AST_INTERFACE_DECLARATION:
            return node->data.interfaceDeclaration.generic;
        case ZR_AST_UNION_DECLARATION:
            return node->data.unionDeclaration.generic;
        default:
            return ZR_NULL;
    }
}

TZrBool type_inference_is_const_generic_parameter_reference(
        SZrCompilerState *cs,
        SZrString *name) {
    EZrGenericParameterKind kind;
    SZrGenericDeclaration *generic;

    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    generic = generic_declaration_from_function_node_inference(cs->currentFunctionNode);
    if (generic_declaration_find_parameter_kind_inference(generic, name, &kind)) {
        return kind == ZR_GENERIC_PARAMETER_CONST_INT;
    }
    generic = generic_declaration_from_type_node_inference(cs->currentTypeNode);
    if (generic_declaration_find_parameter_kind_inference(generic, name, &kind)) {
        return kind == ZR_GENERIC_PARAMETER_CONST_INT;
    }
    if (cs->currentTypePrototypeInfo != ZR_NULL) {
        TZrSize index;
        for (index = 0; index < cs->currentTypePrototypeInfo->genericParameters.length; index++) {
            const SZrTypeGenericParameterInfo *parameter =
                    (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                            &cs->currentTypePrototypeInfo->genericParameters,
                            index);
            if (parameter != ZR_NULL && parameter->name != ZR_NULL &&
                ZrCore_String_Equal(parameter->name, name)) {
                return parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT;
            }
        }
    }
    return ZR_FALSE;
}

TZrBool type_inference_current_generic_context_contains_type_name(SZrCompilerState *cs,
                                                                 SZrString *typeName) {
    SZrGenericDeclaration *generic;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cs->currentFunctionNode != ZR_NULL) {
        generic = generic_declaration_from_function_node_inference(cs->currentFunctionNode);
        if (generic_declaration_find_parameter_kind_inference(generic, typeName, ZR_NULL)) {
            return ZR_TRUE;
        }
    }

    if (cs->currentTypeNode != ZR_NULL) {
        generic = generic_declaration_from_type_node_inference(cs->currentTypeNode);
        if (generic_declaration_find_parameter_kind_inference(generic, typeName, ZR_NULL)) {
            return ZR_TRUE;
        }
    }

    if (cs->currentTypePrototypeInfo != ZR_NULL) {
        for (TZrSize index = 0; index < cs->currentTypePrototypeInfo->genericParameters.length; index++) {
            SZrTypeGenericParameterInfo *genericInfo =
                    (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&cs->currentTypePrototypeInfo->genericParameters,
                                                                    index);
            if (genericInfo != ZR_NULL && genericInfo->name != ZR_NULL &&
                ZrCore_String_Equal(genericInfo->name, typeName)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static SZrString *resolve_source_declaration_lookup_type_name_inference(SZrCompilerState *cs, SZrString *typeName) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    if (try_parse_generic_instance_type_name(cs->state, typeName, &baseName, &argumentTypeNames)) {
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return baseName != ZR_NULL ? baseName : typeName;
    }

    return typeName;
}

static TZrBool resolve_type_name_from_target_node_inference(SZrCompilerState *cs,
                                                            SZrAstNode *node,
                                                            SZrString **outTypeName) {
    SZrAstNode *targetNode = node;
    SZrInferredType inferredType;
    SZrString *fallbackTypeName = ZR_NULL;

    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || node == ZR_NULL || outTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetNode->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        targetNode = targetNode->data.prototypeReferenceExpression.target;
    }
    if (targetNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outTypeName = targetNode->data.identifier.name;
        return *outTypeName != ZR_NULL;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (targetNode->type == ZR_AST_TYPE) {
        fallbackTypeName = extract_type_name_string(cs, &targetNode->data.type);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, &targetNode->data.type, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }
    } else if (!ZrParser_ExpressionType_Infer(cs, targetNode, &inferredType)) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return ZR_FALSE;
    }

    *outTypeName = inferredType.typeName != ZR_NULL ? inferredType.typeName : fallbackTypeName;
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return *outTypeName != ZR_NULL;
}

ZR_PARSER_API TZrBool resolve_source_type_declaration_target_inference(SZrCompilerState *cs,
                                                                       SZrAstNode *node,
                                                                       SZrString **outTypeName,
                                                                       EZrObjectPrototypeType *outPrototypeType,
                                                                       TZrBool *outAllowValueConstruction,
                                                                       TZrBool *outAllowBoxedConstruction) {
    SZrString *resolvedTypeName = ZR_NULL;
    SZrString *lookupTypeName;
    SZrAstNode *declarationNode;

    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }
    if (outPrototypeType != ZR_NULL) {
        *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    }
    if (outAllowValueConstruction != ZR_NULL) {
        *outAllowValueConstruction = ZR_FALSE;
    }
    if (outAllowBoxedConstruction != ZR_NULL) {
        *outAllowBoxedConstruction = ZR_FALSE;
    }
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!resolve_type_name_from_target_node_inference(cs, node, &resolvedTypeName) || resolvedTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    lookupTypeName = resolve_source_declaration_lookup_type_name_inference(cs, resolvedTypeName);
    if (lookupTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    declarationNode = find_type_declaration(cs, lookupTypeName);
    if (declarationNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outTypeName != ZR_NULL) {
        *outTypeName = resolvedTypeName;
    }

    switch (declarationNode->type) {
        case ZR_AST_STRUCT_DECLARATION:
            if (outPrototypeType != ZR_NULL) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            }
            if (outAllowValueConstruction != ZR_NULL) {
                *outAllowValueConstruction = ZR_TRUE;
            }
            if (outAllowBoxedConstruction != ZR_NULL) {
                *outAllowBoxedConstruction = ZR_TRUE;
            }
            return ZR_TRUE;

        case ZR_AST_CLASS_DECLARATION:
            if (outPrototypeType != ZR_NULL) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            }
            if (outAllowValueConstruction != ZR_NULL) {
                *outAllowValueConstruction =
                        (declarationNode->data.classDeclaration.modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) == 0;
            }
            if (outAllowBoxedConstruction != ZR_NULL) {
                *outAllowBoxedConstruction =
                        (declarationNode->data.classDeclaration.modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) == 0;
            }
            return ZR_TRUE;

        case ZR_AST_INTERFACE_DECLARATION:
            if (outPrototypeType != ZR_NULL) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE;
            }
            return ZR_TRUE;

        case ZR_AST_ENUM_DECLARATION:
            if (outPrototypeType != ZR_NULL) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
            }
            if (outAllowValueConstruction != ZR_NULL) {
                *outAllowValueConstruction = ZR_TRUE;
            }
            if (outAllowBoxedConstruction != ZR_NULL) {
                *outAllowBoxedConstruction = ZR_TRUE;
            }
            return ZR_TRUE;

        case ZR_AST_UNION_DECLARATION:
            if (outPrototypeType != ZR_NULL) {
                *outPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_UNION;
            }
            if (outAllowValueConstruction != ZR_NULL) {
                *outAllowValueConstruction = ZR_TRUE;
            }
            if (outAllowBoxedConstruction != ZR_NULL) {
                *outAllowBoxedConstruction = ZR_TRUE;
            }
            return ZR_TRUE;

        default:
            break;
    }

    return ZR_FALSE;
}

static TZrBool protocol_id_from_mask(TZrUInt64 protocolMask, EZrProtocolId *outProtocolId) {
    EZrProtocolId matched = ZR_PROTOCOL_ID_NONE;

    if (outProtocolId != ZR_NULL) {
        *outProtocolId = ZR_PROTOCOL_ID_NONE;
    }

    if (outProtocolId == ZR_NULL || protocolMask == 0) {
        return ZR_FALSE;
    }

    for (EZrProtocolId protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
         protocolId <= ZR_PROTOCOL_ID_MAX;
         protocolId = (EZrProtocolId)(protocolId + 1)) {
        if ((protocolMask & ZR_PROTOCOL_BIT(protocolId)) == 0) {
            continue;
        }

        if (matched != ZR_PROTOCOL_ID_NONE) {
            return ZR_FALSE;
        }
        matched = protocolId;
    }

    if (matched == ZR_PROTOCOL_ID_NONE) {
        return ZR_FALSE;
    }

    *outProtocolId = matched;
    return ZR_TRUE;
}

static TZrBool try_parse_protocol_type_name(SZrCompilerState *cs,
                                            SZrString *typeName,
                                            EZrProtocolId *outProtocolId,
                                            SZrArray *outArgumentTypeNames) {
    SZrString *baseName = ZR_NULL;
    SZrTypePrototypeInfo *prototype = ZR_NULL;

    if (outProtocolId != ZR_NULL) {
        *outProtocolId = ZR_PROTOCOL_ID_NONE;
    }

    if (cs == ZR_NULL || typeName == ZR_NULL || outProtocolId == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outArgumentTypeNames != ZR_NULL) {
        ZrCore_Array_Construct(outArgumentTypeNames);
        if (!try_parse_generic_instance_type_name(cs->state, typeName, &baseName, outArgumentTypeNames)) {
            ZrCore_Array_Free(cs->state, outArgumentTypeNames);
            ZrCore_Array_Construct(outArgumentTypeNames);
            baseName = typeName;
        }
        ensure_generic_instance_type_prototype(cs, baseName != ZR_NULL ? baseName : typeName);
        prototype = find_compiler_type_prototype_inference_exact(cs, baseName != ZR_NULL ? baseName : typeName);
        if (prototype == ZR_NULL ||
            prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
            !protocol_id_from_mask(prototype->protocolMask, outProtocolId)) {
            ZrCore_Array_Free(cs->state, outArgumentTypeNames);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (try_parse_generic_instance_type_name(cs->state, typeName, &baseName, ZR_NULL)) {
        ensure_generic_instance_type_prototype(cs, baseName);
        prototype = find_compiler_type_prototype_inference_exact(cs, baseName);
    } else {
        ensure_generic_instance_type_prototype(cs, typeName);
        prototype = find_compiler_type_prototype_inference_exact(cs, typeName);
    }
    return prototype != ZR_NULL &&
           prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
           protocol_id_from_mask(prototype->protocolMask, outProtocolId);
}

ZR_PARSER_API TZrBool inferred_type_implements_protocol_mask(SZrCompilerState *cs,
                                                             const SZrInferredType *type,
                                                             TZrUInt64 protocolMask) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || type == ZR_NULL || protocolMask == 0 || type->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = type_inference_find_nominal_prototype(cs, type);
    if (prototype == ZR_NULL && type->nominalDefinitionToken == 0U) {
        ensure_generic_instance_type_prototype(cs, type->typeName);
        prototype = type_inference_find_nominal_prototype(cs, type);
    }

    return prototype != ZR_NULL && (prototype->protocolMask & protocolMask) == protocolMask;
}

static SZrString *builtin_box_wrapper_type_name(SZrState *state, EZrValueType valueType) {
    const TZrChar *typeName = ZR_NULL;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    switch (valueType) {
        case ZR_VALUE_TYPE_BOOL:
            typeName = "zr.builtin.Bool";
            break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_UINT8:
            typeName = "zr.builtin.Byte";
            break;
        case ZR_VALUE_TYPE_UINT64:
            typeName = "zr.builtin.UInt64";
            break;
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
            typeName = "zr.builtin.Integer";
            break;
        case ZR_VALUE_TYPE_FLOAT:
            typeName = "zr.builtin.Float";
            break;
        case ZR_VALUE_TYPE_DOUBLE:
            typeName = "zr.builtin.Double";
            break;
        case ZR_VALUE_TYPE_STRING:
            typeName = "zr.builtin.String";
            break;
        default:
            return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)typeName, strlen(typeName));
}

ZR_PARSER_API TZrBool infer_member_call_contract_return_type(SZrCompilerState *cs,
                                                             const SZrTypeMemberInfo *memberInfo,
                                                             SZrFunctionCall *call,
                                                             SZrInferredType *result) {
    SZrAstNode *argumentNode;
    SZrInferredType argumentType;
    SZrString *wrapperTypeName;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (memberInfo->contractRole != ZR_MEMBER_CONTRACT_ROLE_BUILTIN_BOX ||
        call == ZR_NULL ||
        call->args == ZR_NULL ||
        call->args->count == 0) {
        return ZR_FALSE;
    }

    argumentNode = call->args->nodes[0];
    if (argumentNode == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &argumentType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, argumentNode, &argumentType)) {
        ZrParser_InferredType_Free(cs->state, &argumentType);
        return ZR_FALSE;
    }

    wrapperTypeName = builtin_box_wrapper_type_name(cs->state, argumentType.baseType);
    if (wrapperTypeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, argumentType.isNullable, wrapperTypeName);
        result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        ZrParser_InferredType_Free(cs->state, &argumentType);
        return ZR_TRUE;
    }

    if (argumentType.baseType == ZR_VALUE_TYPE_OBJECT ||
        argumentType.baseType == ZR_VALUE_TYPE_ARRAY ||
        argumentType.baseType == ZR_VALUE_TYPE_NULL) {
        ZrParser_InferredType_Copy(cs->state, result, &argumentType);
        ZrParser_InferredType_Free(cs->state, &argumentType);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Free(cs->state, &argumentType);
    return ZR_FALSE;
}

static TZrBool protocol_argument_type_names_match(SZrCompilerState *cs,
                                                  const SZrArray *boundArgumentTypeNames,
                                                  const SZrArray *constraintArgumentTypeNames) {
    if (cs == ZR_NULL || boundArgumentTypeNames == ZR_NULL || constraintArgumentTypeNames == ZR_NULL) {
        return ZR_FALSE;
    }

    if (constraintArgumentTypeNames->length == 0) {
        return ZR_TRUE;
    }

    if (boundArgumentTypeNames->length < constraintArgumentTypeNames->length) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < constraintArgumentTypeNames->length; index++) {
        SZrString **boundNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)boundArgumentTypeNames, index);
        SZrString **constraintNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)constraintArgumentTypeNames, index);
        SZrInferredType boundType;
        SZrInferredType expectedType;
        TZrBool matches = ZR_FALSE;

        if (boundNamePtr == ZR_NULL || *boundNamePtr == ZR_NULL || constraintNamePtr == ZR_NULL || *constraintNamePtr == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(cs->state, &boundType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Init(cs->state, &expectedType, ZR_VALUE_TYPE_OBJECT);
        if (inferred_type_from_type_name(cs, *boundNamePtr, &boundType) &&
            inferred_type_from_type_name(cs, *constraintNamePtr, &expectedType)) {
            matches = ZrParser_InferredType_Equal(&boundType, &expectedType) ||
                      ZrParser_InferredType_IsCompatible(&boundType, &expectedType);
        }
        ZrParser_InferredType_Free(cs->state, &boundType);
        ZrParser_InferredType_Free(cs->state, &expectedType);

        if (!matches) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool array_type_satisfies_protocol_constraint(SZrCompilerState *cs,
                                                        const SZrInferredType *type,
                                                        EZrProtocolId protocolId,
                                                        const SZrArray *constraintArgumentTypeNames) {
    SZrArray boundArgumentTypeNames;
    SZrInferredType *elementType;
    SZrString *elementTypeName = ZR_NULL;
    TZrChar elementTypeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrBool matches = ZR_FALSE;

    if (cs == ZR_NULL || type == ZR_NULL || type->elementTypes.length == 0) {
        return ZR_FALSE;
    }

    if (protocolId != ZR_PROTOCOL_ID_ARRAY_LIKE &&
        protocolId != ZR_PROTOCOL_ID_ITERABLE &&
        protocolId != ZR_PROTOCOL_ID_ITERATOR) {
        return ZR_FALSE;
    }

    elementType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0);
    if (elementType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (constraintArgumentTypeNames == ZR_NULL || constraintArgumentTypeNames->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Construct(&boundArgumentTypeNames);
    ZrCore_Array_Init(cs->state, &boundArgumentTypeNames, sizeof(SZrString *), 1);
    elementTypeName = elementType->typeName;
    if (elementTypeName == ZR_NULL) {
        const TZrChar *elementTypeText =
            ZrParser_TypeNameString_Get(cs->state, elementType, elementTypeBuffer, sizeof(elementTypeBuffer));
        if (elementTypeText != ZR_NULL && elementTypeText[0] != '\0') {
            elementTypeName = ZrCore_String_Create(cs->state,
                                                   (TZrNativeString)elementTypeText,
                                                   strlen(elementTypeText));
        }
    }

    if (elementTypeName != ZR_NULL) {
        ZrCore_Array_Push(cs->state, &boundArgumentTypeNames, &elementTypeName);
        matches = protocol_argument_type_names_match(cs, &boundArgumentTypeNames, constraintArgumentTypeNames);
    }
    ZrCore_Array_Free(cs->state, &boundArgumentTypeNames);
    return matches;
}

static TZrBool primitive_type_satisfies_protocol_constraint(SZrCompilerState *cs,
                                                            const SZrInferredType *type,
                                                            EZrProtocolId protocolId,
                                                            const SZrArray *constraintArgumentTypeNames) {
    if (cs == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (protocolId) {
        case ZR_PROTOCOL_ID_EQUATABLE:
        case ZR_PROTOCOL_ID_HASHABLE:
            return type->baseType == ZR_VALUE_TYPE_BOOL ||
                   type->baseType == ZR_VALUE_TYPE_STRING ||
                   ZR_VALUE_IS_TYPE_INT(type->baseType) ||
                   ZR_VALUE_IS_TYPE_FLOAT(type->baseType);
        case ZR_PROTOCOL_ID_COMPARABLE:
            return type->baseType == ZR_VALUE_TYPE_STRING ||
                   ZR_VALUE_IS_TYPE_INT(type->baseType) ||
                   ZR_VALUE_IS_TYPE_FLOAT(type->baseType);
        case ZR_PROTOCOL_ID_ARRAY_LIKE:
        case ZR_PROTOCOL_ID_ITERABLE:
        case ZR_PROTOCOL_ID_ITERATOR:
            if (type->baseType == ZR_VALUE_TYPE_ARRAY) {
                return array_type_satisfies_protocol_constraint(cs, type, protocolId, constraintArgumentTypeNames);
            }
            return ZR_FALSE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool prototype_implements_protocol_recursive(SZrCompilerState *cs,
                                                       SZrTypePrototypeInfo *prototype,
                                                       EZrProtocolId protocolId,
                                                       const SZrArray *constraintArgumentTypeNames,
                                                       TZrUInt32 depth) {
    SZrArray implementsSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || prototype == ZR_NULL || protocolId == ZR_PROTOCOL_ID_NONE ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }

    if ((prototype->protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0 &&
        (constraintArgumentTypeNames == ZR_NULL || constraintArgumentTypeNames->length == 0)) {
        return ZR_TRUE;
    }

    implementsSnapshot = prototype->implements;
    inheritsSnapshot = prototype->inherits;

    for (TZrSize index = 0; index < implementsSnapshot.length; index++) {
        SZrString **implementedNamePtr = (SZrString **)ZrCore_Array_Get(&implementsSnapshot, index);
        SZrTypePrototypeInfo *implementedPrototype;
        EZrProtocolId implementedProtocolId = ZR_PROTOCOL_ID_NONE;
        SZrArray boundArgumentTypeNames;
        if (implementedNamePtr == ZR_NULL || *implementedNamePtr == ZR_NULL) {
            continue;
        }
        if (try_parse_protocol_type_name(cs, *implementedNamePtr, &implementedProtocolId, &boundArgumentTypeNames)) {
            TZrBool matches = implementedProtocolId == protocolId &&
                              protocol_argument_type_names_match(cs, &boundArgumentTypeNames, constraintArgumentTypeNames);
            ZrCore_Array_Free(cs->state, &boundArgumentTypeNames);
            if (matches) {
                return ZR_TRUE;
            }
        }
        ensure_generic_instance_type_prototype(cs, *implementedNamePtr);
        implementedPrototype = find_compiler_type_prototype_inference_exact(cs, *implementedNamePtr);
        if (implementedPrototype != ZR_NULL &&
            prototype_implements_protocol_recursive(cs,
                                                   implementedPrototype,
                                                   protocolId,
                                                   constraintArgumentTypeNames,
                                                   depth + 1)) {
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
        SZrTypePrototypeInfo *superPrototype;
        EZrProtocolId inheritedProtocolId = ZR_PROTOCOL_ID_NONE;
        SZrArray boundArgumentTypeNames;
        if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
            continue;
        }
        if (try_parse_protocol_type_name(cs, *inheritNamePtr, &inheritedProtocolId, &boundArgumentTypeNames)) {
            TZrBool matches = inheritedProtocolId == protocolId &&
                              protocol_argument_type_names_match(cs, &boundArgumentTypeNames, constraintArgumentTypeNames);
            ZrCore_Array_Free(cs->state, &boundArgumentTypeNames);
            if (matches) {
                return ZR_TRUE;
            }
        }
        ensure_generic_instance_type_prototype(cs, *inheritNamePtr);
        superPrototype = find_compiler_type_prototype_inference_exact(cs, *inheritNamePtr);
        if (superPrototype != ZR_NULL &&
            prototype_implements_protocol_recursive(cs,
                                                   superPrototype,
                                                   protocolId,
                                                   constraintArgumentTypeNames,
                                                   depth + 1)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool prototype_satisfies_named_constraint_recursive(SZrCompilerState *cs,
                                                              SZrTypePrototypeInfo *prototype,
                                                              SZrString *constraintTypeName,
                                                              TZrUInt32 depth) {
    SZrArray implementsSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || prototype == ZR_NULL || constraintTypeName == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }

    if (prototype->name != ZR_NULL && ZrCore_String_Equal(prototype->name, constraintTypeName)) {
        return ZR_TRUE;
    }

    implementsSnapshot = prototype->implements;
    inheritsSnapshot = prototype->inherits;

    for (TZrSize index = 0; index < implementsSnapshot.length; index++) {
        SZrString **implementedNamePtr = (SZrString **)ZrCore_Array_Get(&implementsSnapshot, index);
        SZrTypePrototypeInfo *implementedPrototype;

        if (implementedNamePtr == ZR_NULL || *implementedNamePtr == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(*implementedNamePtr, constraintTypeName)) {
            return ZR_TRUE;
        }

        ensure_generic_instance_type_prototype(cs, *implementedNamePtr);
        implementedPrototype = find_compiler_type_prototype_inference_exact(cs, *implementedNamePtr);
        if (implementedPrototype != ZR_NULL &&
            prototype_satisfies_named_constraint_recursive(cs,
                                                           implementedPrototype,
                                                           constraintTypeName,
                                                           depth + 1)) {
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
        SZrTypePrototypeInfo *superPrototype;

        if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(*inheritNamePtr, constraintTypeName)) {
            return ZR_TRUE;
        }

        ensure_generic_instance_type_prototype(cs, *inheritNamePtr);
        superPrototype = find_compiler_type_prototype_inference_exact(cs, *inheritNamePtr);
        if (superPrototype != ZR_NULL &&
            prototype_satisfies_named_constraint_recursive(cs,
                                                           superPrototype,
                                                           constraintTypeName,
                                                           depth + 1)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrTypePrototypeInfo *resolve_constraint_actual_prototype(SZrCompilerState *cs,
                                                                 const SZrInferredType *actualType) {
    if (cs == ZR_NULL || actualType == ZR_NULL || actualType->typeName == ZR_NULL) {
        return ZR_NULL;
    }

    ensure_generic_instance_type_prototype_from_inferred(cs, actualType);
    return type_inference_find_nominal_prototype(cs, actualType);
}

static TZrBool inferred_type_is_primitive_value_type(const SZrInferredType *actualType) {
    if (actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    return actualType->baseType == ZR_VALUE_TYPE_BOOL ||
           ZR_VALUE_IS_TYPE_INT(actualType->baseType) ||
           ZR_VALUE_IS_TYPE_FLOAT(actualType->baseType);
}

static TZrBool inferred_type_has_thread_unsafe_ownership(const SZrInferredType *actualType) {
    TZrSize index;

    if (actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (actualType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
        actualType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK ||
        actualType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        actualType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) {
        return ZR_TRUE;
    }

    for (index = 0; index < actualType->elementTypes.length; index++) {
        SZrInferredType *elementType =
                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&actualType->elementTypes, index);
        if (inferred_type_has_thread_unsafe_ownership(elementType)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool inferred_type_satisfies_thread_marker_primitive(const SZrInferredType *actualType,
                                                               TZrBool requireSync) {
    SZrInferredType *elementType;

    ZR_UNUSED_PARAMETER(requireSync);
    if (actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (inferred_type_has_thread_unsafe_ownership(actualType)) {
        return ZR_FALSE;
    }

    if (actualType->baseType == ZR_VALUE_TYPE_NULL ||
        actualType->baseType == ZR_VALUE_TYPE_STRING ||
        inferred_type_is_primitive_value_type(actualType)) {
        return ZR_TRUE;
    }

    if (actualType->baseType == ZR_VALUE_TYPE_ARRAY) {
        if (actualType->elementTypes.length == 0) {
            return ZR_TRUE;
        }

        elementType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&actualType->elementTypes, 0);
        return inferred_type_satisfies_thread_marker_primitive(elementType, requireSync);
    }

    return ZR_FALSE;
}

static TZrBool inferred_type_satisfies_class_constraint(SZrCompilerState *cs,
                                                        const SZrInferredType *actualType) {
    SZrTypePrototypeInfo *actualPrototype;

    if (cs == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (actualType->baseType == ZR_VALUE_TYPE_STRING || actualType->baseType == ZR_VALUE_TYPE_ARRAY) {
        return ZR_TRUE;
    }

    actualPrototype = resolve_constraint_actual_prototype(cs, actualType);
    if (actualPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return actualPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS ||
           actualPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
           actualPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
}

static TZrBool inferred_type_satisfies_struct_constraint(SZrCompilerState *cs,
                                                         const SZrInferredType *actualType) {
    SZrTypePrototypeInfo *actualPrototype;

    if (cs == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (inferred_type_is_primitive_value_type(actualType)) {
        return ZR_TRUE;
    }

    actualPrototype = resolve_constraint_actual_prototype(cs, actualType);
    if (actualPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return actualPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ||
           actualPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
}

static TZrBool prototype_has_parameterless_constructor(const SZrTypePrototypeInfo *prototype) {
    TZrNativeString constructorSignature;
    const TZrChar *openParen;
    const TZrChar *closeParen;

    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ||
        prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        return ZR_TRUE;
    }

    constructorSignature = prototype->constructorSignature != ZR_NULL
                                   ? ZrCore_String_GetNativeString(prototype->constructorSignature)
                                   : ZR_NULL;
    if (constructorSignature == ZR_NULL) {
        return prototype->allowValueConstruction || prototype->allowBoxedConstruction;
    }

    openParen = strchr(constructorSignature, '(');
    closeParen = openParen != ZR_NULL ? strchr(openParen + 1, ')') : ZR_NULL;
    return openParen != ZR_NULL && closeParen != ZR_NULL && closeParen == openParen + 1;
}

static TZrBool inferred_type_satisfies_new_constraint(SZrCompilerState *cs,
                                                      const SZrInferredType *actualType) {
    SZrTypePrototypeInfo *actualPrototype;

    if (cs == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (inferred_type_is_primitive_value_type(actualType)) {
        return ZR_TRUE;
    }

    actualPrototype = resolve_constraint_actual_prototype(cs, actualType);
    if (actualPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    if (actualPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
        actualPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        return ZR_FALSE;
    }

    return prototype_has_parameterless_constructor(actualPrototype);
}

static TZrBool inferred_type_satisfies_owner_constraint(const SZrInferredType *actualType) {
    return actualType != ZR_NULL &&
           actualType->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE;
}

static const TZrChar *ownership_constraint_qualifier_name(EZrOwnershipQualifier qualifier) {
    switch (qualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return "unique";
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return "shared";
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return "weak";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return "borrow";
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return "loan";
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        default:
            return "owner";
    }
}

static TZrBool inferred_type_satisfies_specific_owner_constraint(const SZrInferredType *actualType,
                                                                 EZrOwnershipQualifier requiredQualifier) {
    if (requiredQualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        return inferred_type_satisfies_owner_constraint(actualType);
    }

    return actualType != ZR_NULL && actualType->ownershipQualifier == requiredQualifier;
}

static TZrBool inferred_type_satisfies_constraint(SZrCompilerState *cs,
                                                  const SZrInferredType *actualType,
                                                  SZrString *constraintTypeName) {
    SZrTypePrototypeInfo *actualPrototype;
    EZrProtocolId protocolId = ZR_PROTOCOL_ID_NONE;
    SZrArray constraintArgumentTypeNames;
    TZrBool isProtocolConstraint;
    TZrBool primitiveMatches;
    TZrBool isThreadCapabilityConstraint;
    TZrBool isSyncConstraint;

    if (cs == ZR_NULL || actualType == ZR_NULL || constraintTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&constraintArgumentTypeNames);
    isProtocolConstraint = try_parse_protocol_type_name(cs, constraintTypeName, &protocolId, &constraintArgumentTypeNames);
    isThreadCapabilityConstraint = isProtocolConstraint &&
                                  (protocolId == ZR_PROTOCOL_ID_THREAD_SEND ||
                                   protocolId == ZR_PROTOCOL_ID_THREAD_SYNC);
    isSyncConstraint = protocolId == ZR_PROTOCOL_ID_THREAD_SYNC;
    if (isThreadCapabilityConstraint && inferred_type_has_thread_unsafe_ownership(actualType)) {
        ZrCore_Array_Free(cs->state, &constraintArgumentTypeNames);
        return ZR_FALSE;
    }
    if (isThreadCapabilityConstraint &&
        inferred_type_satisfies_thread_marker_primitive(actualType, isSyncConstraint)) {
        ZrCore_Array_Free(cs->state, &constraintArgumentTypeNames);
        return ZR_TRUE;
    }
    primitiveMatches = isProtocolConstraint &&
                       primitive_type_satisfies_protocol_constraint(cs,
                                                                   actualType,
                                                                   protocolId,
                                                                   &constraintArgumentTypeNames);
    if (primitiveMatches) {
        ZrCore_Array_Free(cs->state, &constraintArgumentTypeNames);
        return ZR_TRUE;
    }

    if (actualType->typeName == ZR_NULL) {
        ZrCore_Array_Free(cs->state, &constraintArgumentTypeNames);
        return ZR_FALSE;
    }

    actualPrototype = resolve_constraint_actual_prototype(cs, actualType);
    if (actualPrototype == ZR_NULL) {
        ZrCore_Array_Free(cs->state, &constraintArgumentTypeNames);
        return ZR_FALSE;
    }

    if (isProtocolConstraint) {
        TZrBool result = prototype_implements_protocol_recursive(cs,
                                                                 actualPrototype,
                                                                 protocolId,
                                                                 &constraintArgumentTypeNames,
                                                                 0);
        ZrCore_Array_Free(cs->state, &constraintArgumentTypeNames);
        return result;
    }

    ZrCore_Array_Free(cs->state, &constraintArgumentTypeNames);
    return prototype_satisfies_named_constraint_recursive(cs, actualPrototype, constraintTypeName, 0);
}

TZrBool ZrParser_InferredType_SatisfiesNamedConstraint(SZrCompilerState *cs,
                                                       const SZrInferredType *actualType,
                                                       SZrString *constraintTypeName) {
    return inferred_type_satisfies_constraint(cs, actualType, constraintTypeName);
}

static TZrBool inferred_type_resolves_to_same_exact_prototype(SZrCompilerState *cs,
                                                              const SZrInferredType *leftType,
                                                              const SZrInferredType *rightType) {
    SZrTypePrototypeInfo *leftPrototype;
    SZrTypePrototypeInfo *rightPrototype;

    if (cs == ZR_NULL || leftType == ZR_NULL || rightType == ZR_NULL ||
        leftType->typeName == ZR_NULL || rightType->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (leftType->baseType != rightType->baseType) {
        return ZR_FALSE;
    }

    if (leftType->isNullable && !rightType->isNullable) {
        return ZR_FALSE;
    }

    leftPrototype = resolve_constraint_actual_prototype(cs, leftType);
    rightPrototype = resolve_constraint_actual_prototype(cs, rightType);
    if (leftPrototype == ZR_NULL || rightPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    if (leftPrototype == rightPrototype) {
        return ZR_TRUE;
    }

    return leftPrototype->name != ZR_NULL &&
           rightPrototype->name != ZR_NULL &&
           ZrCore_String_Equal(leftPrototype->name, rightPrototype->name);
}

TZrBool inferred_type_can_use_named_constraint_fallback(SZrCompilerState *cs,
                                                        const SZrInferredType *actualType,
                                                        const SZrInferredType *expectedType) {
    if (cs == ZR_NULL || actualType == ZR_NULL || expectedType == ZR_NULL || expectedType->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (actualType->typeName != ZR_NULL &&
        ZrCore_String_Equal(actualType->typeName, expectedType->typeName)) {
        return ZR_FALSE;
    }

    if (inferred_type_resolves_to_same_exact_prototype(cs, actualType, expectedType)) {
        return ZR_TRUE;
    }

    return ZrParser_InferredType_SatisfiesNamedConstraint(cs, actualType, expectedType->typeName);
}

static TZrBool bind_protocol_argument_from_type_name(SZrCompilerState *cs,
                                                     SZrString *implementedTypeName,
                                                     EZrProtocolId protocolId,
                                                     TZrUInt32 argumentIndex,
                                                     SZrInferredType *outType) {
    EZrProtocolId implementedProtocolId = ZR_PROTOCOL_ID_NONE;
    SZrArray argumentTypeNames;
    SZrString **argumentNamePtr;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || implementedTypeName == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!try_parse_protocol_type_name(cs, implementedTypeName, &implementedProtocolId, &argumentTypeNames)) {
        return ZR_FALSE;
    }

    if (implementedProtocolId == protocolId && argumentIndex < argumentTypeNames.length) {
        argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentTypeNames, argumentIndex);
        if (argumentNamePtr != ZR_NULL && *argumentNamePtr != ZR_NULL) {
            success = inferred_type_from_type_name(cs, *argumentNamePtr, outType);
        }
    }

    ZrCore_Array_Free(cs->state, &argumentTypeNames);
    return success;
}

static TZrBool prototype_bind_protocol_argument_recursive(SZrCompilerState *cs,
                                                          SZrTypePrototypeInfo *prototype,
                                                          EZrProtocolId protocolId,
                                                          TZrUInt32 argumentIndex,
                                                          SZrInferredType *outType,
                                                          TZrUInt32 depth) {
    SZrArray implementsSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || prototype == ZR_NULL || outType == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }

    implementsSnapshot = prototype->implements;
    inheritsSnapshot = prototype->inherits;

    for (TZrSize index = 0; index < implementsSnapshot.length; index++) {
        SZrString **implementedNamePtr = (SZrString **)ZrCore_Array_Get(&implementsSnapshot, index);
        SZrTypePrototypeInfo *implementedPrototype;

        if (implementedNamePtr == ZR_NULL || *implementedNamePtr == ZR_NULL) {
            continue;
        }

        if (bind_protocol_argument_from_type_name(cs, *implementedNamePtr, protocolId, argumentIndex, outType)) {
            return ZR_TRUE;
        }

        ensure_generic_instance_type_prototype(cs, *implementedNamePtr);
        implementedPrototype = find_compiler_type_prototype_inference(cs, *implementedNamePtr);
        if (implementedPrototype != ZR_NULL &&
            prototype_bind_protocol_argument_recursive(cs,
                                                      implementedPrototype,
                                                      protocolId,
                                                      argumentIndex,
                                                      outType,
                                                      depth + 1)) {
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
        SZrTypePrototypeInfo *superPrototype;

        if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
            continue;
        }

        if (bind_protocol_argument_from_type_name(cs, *inheritNamePtr, protocolId, argumentIndex, outType)) {
            return ZR_TRUE;
        }

        ensure_generic_instance_type_prototype(cs, *inheritNamePtr);
        superPrototype = find_compiler_type_prototype_inference(cs, *inheritNamePtr);
        if (superPrototype != ZR_NULL &&
            prototype_bind_protocol_argument_recursive(cs,
                                                      superPrototype,
                                                      protocolId,
                                                      argumentIndex,
                                                      outType,
                                                      depth + 1)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool ZrParser_TypeInference_BindProtocolElementType(SZrCompilerState *cs,
                                                        const SZrInferredType *sourceType,
                                                        EZrProtocolId protocolId,
                                                        SZrInferredType *outType) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || sourceType == ZR_NULL || outType == ZR_NULL ||
        (protocolId != ZR_PROTOCOL_ID_ITERATOR && protocolId != ZR_PROTOCOL_ID_ITERABLE)) {
        return ZR_FALSE;
    }

    if (sourceType->typeName != ZR_NULL) {
        ensure_generic_instance_type_prototype(cs, sourceType->typeName);
        prototype = find_compiler_type_prototype_inference(cs, sourceType->typeName);
        if (prototype != ZR_NULL) {
            if (bind_protocol_argument_from_type_name(
                        cs, sourceType->typeName, protocolId, 0, outType) ||
                prototype_bind_protocol_argument_recursive(
                        cs, prototype, protocolId, 0, outType, 0)) {
                return ZR_TRUE;
            }
        }
    }

    /* elementTypes stores generic arguments for named types, so it is only a
     * direct protocol projection when no named prototype supplied a contract. */
    if ((sourceType->protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0 &&
        sourceType->elementTypes.length > 0) {
        SZrInferredType *elementType =
                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&sourceType->elementTypes, 0);
        if (elementType != ZR_NULL) {
            ZrParser_InferredType_Copy(cs->state, outType, elementType);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

ZR_PARSER_API TZrBool bind_foreach_element_type_from_inferred_iterable(SZrCompilerState *cs,
                                                                       const SZrInferredType *iterableType,
                                                                       SZrInferredType *outType) {
    return ZrParser_EnumeratorBinding_ResolveElementType(cs, iterableType, outType);
}


static SZrString *generic_call_binding_type_name(SZrCompilerState *cs, const SZrInferredType *argumentType) {
    TZrChar typeNameBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    const TZrChar *typeNameText;

    if (cs == ZR_NULL || argumentType == ZR_NULL) {
        return ZR_NULL;
    }

    if (argumentType->typeName != ZR_NULL) {
        return argumentType->typeName;
    }

    typeNameText = ZrParser_TypeNameString_Get(cs->state, argumentType, typeNameBuffer, sizeof(typeNameBuffer));
    if (typeNameText == ZR_NULL || typeNameText[0] == '\0') {
        return ZR_NULL;
    }

    return ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)typeNameText);
}

static void format_generic_call_constraint_failure(TZrChar *diagnosticBuffer,
                                                   TZrSize diagnosticBufferSize,
                                                   const TZrChar *detailFormat,
                                                   SZrString *argumentTypeName,
                                                   SZrString *constraintTypeName) {
    const TZrChar *argumentTypeText = argumentTypeName != ZR_NULL
                                              ? ZrCore_String_GetNativeString(argumentTypeName)
                                              : "<unknown>";
    const TZrChar *constraintTypeText = constraintTypeName != ZR_NULL
                                                ? ZrCore_String_GetNativeString(constraintTypeName)
                                                : ZR_NULL;

    if (diagnosticBuffer == ZR_NULL || diagnosticBufferSize == 0 || detailFormat == ZR_NULL) {
        return;
    }

    if (constraintTypeText != ZR_NULL) {
        snprintf(diagnosticBuffer, diagnosticBufferSize, detailFormat, argumentTypeText, constraintTypeText);
        return;
    }

    snprintf(diagnosticBuffer, diagnosticBufferSize, detailFormat, argumentTypeText);
}

static void format_generic_call_ownership_constraint_failure(TZrChar *diagnosticBuffer,
                                                             TZrSize diagnosticBufferSize,
                                                             SZrString *argumentTypeName,
                                                             EZrOwnershipQualifier requiredQualifier) {
    const TZrChar *argumentTypeText = argumentTypeName != ZR_NULL
                                              ? ZrCore_String_GetNativeString(argumentTypeName)
                                              : "<unknown>";

    if (diagnosticBuffer == ZR_NULL || diagnosticBufferSize == 0) {
        return;
    }

    snprintf(diagnosticBuffer,
             diagnosticBufferSize,
             "generic argument '%s' does not satisfy %s ownership constraint",
             argumentTypeText,
             ownership_constraint_qualifier_name(requiredQualifier));
}

ZR_PARSER_API EZrGenericCallResolveStatus validate_generic_call_bindings_constraints(
        SZrCompilerState *cs,
        const SZrArray *bindings,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize) {
    SZrArray genericParametersSnapshot;
    SZrArray argumentTypeNames;

    if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0) {
        diagnosticBuffer[0] = '\0';
    }

    if (cs == ZR_NULL || bindings == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    ZrCore_Array_Construct(&genericParametersSnapshot);
    ZrCore_Array_Construct(&argumentTypeNames);
    if (bindings->length > 0) {
        ZrCore_Array_Init(cs->state,
                          &genericParametersSnapshot,
                          sizeof(SZrTypeGenericParameterInfo),
                          bindings->length);
        ZrCore_Array_Init(cs->state, &argumentTypeNames, sizeof(SZrString *), bindings->length);
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get((SZrArray *)bindings, index);
        SZrString *argumentTypeName;
        SZrTypeGenericParameterInfo parameterSnapshot;

        if (binding == ZR_NULL || binding->parameterInfo == ZR_NULL || !binding->isBound) {
            continue;
        }

        parameterSnapshot = *binding->parameterInfo;
        ZrCore_Array_Push(cs->state, &genericParametersSnapshot, &parameterSnapshot);
        argumentTypeName = generic_call_binding_type_name(cs, &binding->inferredType);
        ZrCore_Array_Push(cs->state, &argumentTypeNames, &argumentTypeName);
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get((SZrArray *)bindings, index);
        const SZrTypeGenericParameterInfo *parameterInfo;
        SZrString *argumentTypeName;

        if (binding == ZR_NULL || binding->parameterInfo == ZR_NULL || !binding->isBound) {
            continue;
        }

        parameterInfo = binding->parameterInfo;
        argumentTypeName = generic_call_binding_type_name(cs, &binding->inferredType);

        if (parameterInfo->requiresClass && !inferred_type_satisfies_class_constraint(cs, &binding->inferredType)) {
            format_generic_call_constraint_failure(diagnosticBuffer,
                                                   diagnosticBufferSize,
                                                   "generic argument '%s' does not satisfy class constraint",
                                                   argumentTypeName,
                                                   ZR_NULL);
            ZrCore_Array_Free(cs->state, &genericParametersSnapshot);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        if (parameterInfo->requiresStruct && !inferred_type_satisfies_struct_constraint(cs, &binding->inferredType)) {
            format_generic_call_constraint_failure(diagnosticBuffer,
                                                   diagnosticBufferSize,
                                                   "generic argument '%s' does not satisfy struct constraint",
                                                   argumentTypeName,
                                                   ZR_NULL);
            ZrCore_Array_Free(cs->state, &genericParametersSnapshot);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        if (parameterInfo->requiresNew && !inferred_type_satisfies_new_constraint(cs, &binding->inferredType)) {
            format_generic_call_constraint_failure(diagnosticBuffer,
                                                   diagnosticBufferSize,
                                                   "generic argument '%s' does not satisfy new() constraint",
                                                   argumentTypeName,
                                                   ZR_NULL);
            ZrCore_Array_Free(cs->state, &genericParametersSnapshot);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        if (parameterInfo->requiredOwnershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE &&
            !inferred_type_satisfies_specific_owner_constraint(&binding->inferredType,
                                                               parameterInfo->requiredOwnershipQualifier)) {
            format_generic_call_ownership_constraint_failure(diagnosticBuffer,
                                                             diagnosticBufferSize,
                                                             argumentTypeName,
                                                             parameterInfo->requiredOwnershipQualifier);
            ZrCore_Array_Free(cs->state, &genericParametersSnapshot);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        if (parameterInfo->requiresOwner && !inferred_type_satisfies_owner_constraint(&binding->inferredType)) {
            format_generic_call_constraint_failure(diagnosticBuffer,
                                                   diagnosticBufferSize,
                                                   "generic argument '%s' does not satisfy owner constraint",
                                                   argumentTypeName,
                                                   ZR_NULL);
            ZrCore_Array_Free(cs->state, &genericParametersSnapshot);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        for (TZrSize constraintIndex = 0; constraintIndex < parameterInfo->constraintTypeNames.length; constraintIndex++) {
            SZrString **constraintNamePtr =
                    (SZrString **)ZrCore_Array_Get((SZrArray *)&parameterInfo->constraintTypeNames, constraintIndex);
            SZrString *resolvedConstraintName;

            if (constraintNamePtr == ZR_NULL || *constraintNamePtr == ZR_NULL) {
                continue;
            }

            resolvedConstraintName = substitute_generic_type_name(cs->state,
                                                                  *constraintNamePtr,
                                                                  &genericParametersSnapshot,
                                                                  &argumentTypeNames);
            if (resolvedConstraintName == ZR_NULL) {
                resolvedConstraintName = *constraintNamePtr;
            }

            if (inferred_type_satisfies_constraint(cs, &binding->inferredType, resolvedConstraintName)) {
                continue;
            }

            format_generic_call_constraint_failure(diagnosticBuffer,
                                                   diagnosticBufferSize,
                                                   "generic argument '%s' does not satisfy constraint '%s'",
                                                   argumentTypeName,
                                                   resolvedConstraintName);
            ZrCore_Array_Free(cs->state, &genericParametersSnapshot);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }
    }

    if (genericParametersSnapshot.isValid && genericParametersSnapshot.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &genericParametersSnapshot);
    }
    if (argumentTypeNames.isValid && argumentTypeNames.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
    }
    return ZR_GENERIC_CALL_RESOLVE_OK;
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
    SZrAstNodeArray *params = ZR_NULL;
    const SZrType *returnTypeNode = ZR_NULL;
    SZrInferredType returnType;

    if (cs == ZR_NULL || declNode == ZR_NULL || funcType == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (declNode->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            params = declNode->data.functionDeclaration.params;
            returnTypeNode = declNode->data.functionDeclaration.returnType;
            break;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            params = declNode->data.externFunctionDeclaration.params;
            returnTypeNode = declNode->data.externFunctionDeclaration.returnType;
            break;
        default:
            return ZR_FALSE;
    }

    if (params == ZR_NULL) {
        return funcType->paramTypes.length == 0;
    }

    if (params->count != funcType->paramTypes.length) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
    if (returnTypeNode != ZR_NULL) {
        if (!ZrParser_AstTypeToInferredType_Convert(cs, returnTypeNode, &returnType)) {
            ZrParser_InferredType_Free(cs->state, &returnType);
            return ZR_FALSE;
        }
    }

    if (!ZrParser_InferredType_Equal(&returnType, &funcType->returnType)) {
        ZrParser_InferredType_Free(cs->state, &returnType);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &returnType);

    for (TZrSize i = 0; i < params->count; i++) {
        SZrAstNode *paramNode = params->nodes[i];
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

        if (stmt == ZR_NULL) {
            continue;
        }

        if (stmt->type == ZR_AST_FUNCTION_DECLARATION) {
            decl = &stmt->data.functionDeclaration;
            if (decl->name == ZR_NULL || decl->name->name == ZR_NULL ||
                !ZrCore_String_Equal(decl->name->name, funcName)) {
                continue;
            }

            if (function_declaration_matches_candidate(cs, stmt, funcType)) {
                return stmt;
            }
        } else if (stmt->type == ZR_AST_EXTERN_BLOCK &&
                   stmt->data.externBlock.declarations != ZR_NULL) {
            for (TZrSize declarationIndex = 0; declarationIndex < stmt->data.externBlock.declarations->count;
                 declarationIndex++) {
                SZrAstNode *declaration = stmt->data.externBlock.declarations->nodes[declarationIndex];
                SZrExternFunctionDeclaration *externDecl;

                if (declaration == ZR_NULL || declaration->type != ZR_AST_EXTERN_FUNCTION_DECLARATION) {
                    continue;
                }

                externDecl = &declaration->data.externFunctionDeclaration;
                if (externDecl->name == ZR_NULL || externDecl->name->name == ZR_NULL ||
                    !ZrCore_String_Equal(externDecl->name->name, funcName)) {
                    continue;
                }

                if (function_declaration_matches_candidate(cs, declaration, funcType)) {
                    return declaration;
                }
            }
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeFunction *find_compile_time_function_projection_for_candidate(SZrCompilerState *cs,
                                                                                    SZrTypeEnvironment *env,
                                                                                    SZrString *funcName,
                                                                                    const SZrFunctionTypeInfo *funcType) {
    if (cs == ZR_NULL || env != cs->compileTimeTypeEnv || funcName == ZR_NULL || funcType == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, index);
        SZrCompileTimeFunction *func = funcPtr != ZR_NULL ? *funcPtr : ZR_NULL;

        if (func == ZR_NULL || func->declaration != ZR_NULL || func->name == ZR_NULL ||
            !ZrCore_String_Equal(func->name, funcName) ||
            func->paramTypes.length != funcType->paramTypes.length ||
            !ZrParser_InferredType_Equal(&func->returnType, &funcType->returnType)) {
            continue;
        }

        {
            TZrBool matches = ZR_TRUE;
            for (TZrSize paramIndex = 0; paramIndex < func->paramTypes.length; paramIndex++) {
                SZrInferredType *lhs = (SZrInferredType *)ZrCore_Array_Get(&func->paramTypes, paramIndex);
                SZrInferredType *rhs = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&funcType->paramTypes, paramIndex);
                if (lhs == ZR_NULL || rhs == ZR_NULL || !ZrParser_InferredType_Equal(lhs, rhs)) {
                    matches = ZR_FALSE;
                    break;
                }
            }

            if (matches) {
                return func;
            }
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

static TZrBool infer_spread_call_argument_types_for_candidate(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        const SZrFunctionTypeInfo *funcType,
        SZrArray *argTypes,
        TZrBool *mismatch,
        TZrBool *handled) {
    SZrAstNode *spreadNode;
    SZrAstNode *spreadExpression;
    SZrInferredType spreadType;
    const SZrInferredType *elementType;
    TZrSize prefixCount;
    TZrSize spreadCount;
    TZrSize parameterCount;

    if (handled != ZR_NULL) {
        *handled = ZR_FALSE;
    }
    if (cs == ZR_NULL || funcType == ZR_NULL || argTypes == ZR_NULL ||
        call == ZR_NULL || !compiler_call_has_spread_argument(call)) {
        return ZR_TRUE;
    }
    if (handled == ZR_NULL || mismatch == ZR_NULL || call->args == ZR_NULL ||
        call->args->count == 0U) {
        return ZR_FALSE;
    }

    *handled = ZR_TRUE;
    prefixCount = call->args->count - 1U;
    parameterCount = funcType->paramTypes.length;
    spreadNode = call->args->nodes[prefixCount];
    if (spreadNode == ZR_NULL || spreadNode->type != ZR_AST_SPREAD_ARGUMENT ||
        spreadNode->data.spreadArgument.expression == ZR_NULL) {
        return ZR_FALSE;
    }
    spreadExpression = spreadNode->data.spreadArgument.expression;

    ZrCore_Array_Construct(argTypes);
    if (prefixCount > parameterCount) {
        *mismatch = ZR_TRUE;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(
            cs->state, argTypes, sizeof(SZrInferredType), parameterCount);
    for (TZrSize index = 0U; index < prefixCount; index++) {
        if (!infer_call_argument_type_node(
                    cs, call->args->nodes[index], argTypes)) {
            free_inferred_type_array(cs->state, argTypes);
            ZrCore_Array_Construct(argTypes);
            return ZR_FALSE;
        }
    }

    ZrParser_InferredType_Init(
            cs->state, &spreadType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, spreadExpression, &spreadType)) {
        ZrParser_InferredType_Free(cs->state, &spreadType);
        free_inferred_type_array(cs->state, argTypes);
        ZrCore_Array_Construct(argTypes);
        return ZR_FALSE;
    }
    if (spreadType.baseType != ZR_VALUE_TYPE_ARRAY) {
        ZrParser_Compiler_Error(
                cs,
                "Spread argument must have an array type",
                spreadExpression->location);
        ZrParser_InferredType_Free(cs->state, &spreadType);
        free_inferred_type_array(cs->state, argTypes);
        ZrCore_Array_Construct(argTypes);
        return ZR_FALSE;
    }

    spreadCount = parameterCount - prefixCount;
    if (spreadType.hasArraySizeConstraint &&
        spreadType.arrayFixedSize != spreadCount) {
        *mismatch = ZR_TRUE;
        ZrParser_InferredType_Free(cs->state, &spreadType);
        free_inferred_type_array(cs->state, argTypes);
        ZrCore_Array_Construct(argTypes);
        return ZR_TRUE;
    }

    elementType = spreadType.elementTypes.length == 1U
                          ? (const SZrInferredType *)ZrCore_Array_Get(
                                    &spreadType.elementTypes, 0U)
                          : ZR_NULL;
    for (TZrSize index = prefixCount; index < parameterCount; index++) {
        const SZrInferredType *sourceType = elementType;
        SZrInferredType copiedType;

        if (sourceType == ZR_NULL) {
            sourceType = (const SZrInferredType *)ZrCore_Array_Get(
                    (SZrArray *)&funcType->paramTypes, index);
        }
        if (sourceType == ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &spreadType);
            free_inferred_type_array(cs->state, argTypes);
            ZrCore_Array_Construct(argTypes);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Init(
                cs->state, &copiedType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, &copiedType, sourceType);
        ZrCore_Array_Push(cs->state, argTypes, &copiedType);
    }
    ZrParser_InferredType_Free(cs->state, &spreadType);
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
    TZrBool handledSpread = ZR_FALSE;

    if (mismatch != ZR_NULL) {
        *mismatch = ZR_FALSE;
    }

    if (cs == ZR_NULL || funcType == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!infer_spread_call_argument_types_for_candidate(
                cs,
                call,
                funcType,
                argTypes,
                mismatch,
                &handledSpread)) {
        return ZR_FALSE;
    }
    if (handledSpread) {
        return ZR_TRUE;
    }

    declNode = funcType->declarationNode;
    if (declNode == ZR_NULL ||
        (declNode->type != ZR_AST_FUNCTION_DECLARATION &&
         declNode->type != ZR_AST_EXTERN_FUNCTION_DECLARATION)) {
        declNode = find_function_declaration_for_candidate(cs, env, funcName, funcType);
    }
    if (declNode == ZR_NULL) {
        SZrCompileTimeFunction *projection =
                find_compile_time_function_projection_for_candidate(cs, env, funcName, funcType);
        if (projection == ZR_NULL) {
            return infer_function_call_argument_types(cs, call != ZR_NULL ? call->args : ZR_NULL, argTypes);
        }

        paramCount = projection->paramTypes.length;
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
        provided = (TZrBool *)ZrCore_Memory_RawMallocWithType(cs->state->global,
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
                SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
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
                SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
                TZrBool matched = ZR_FALSE;

                if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
                    if (mismatch != ZR_NULL) {
                        *mismatch = ZR_TRUE;
                    }
                    goto cleanup;
                }

                for (TZrSize j = 0; j < paramCount; j++) {
                    SZrString **paramNamePtr = (SZrString **)ZrCore_Array_Get(&projection->paramNames, j);

                    if (paramNamePtr == ZR_NULL || *paramNamePtr == ZR_NULL ||
                        !ZrCore_String_Equal(*paramNamePtr, *namePtr)) {
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
                        SZrInferredType *existing = (SZrInferredType *)ZrCore_Array_Get(argTypes, j);
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
            TZrBool *hasDefaultPtr =
                    (TZrBool *)ZrCore_Array_Get(&projection->paramHasDefaultValues, i);
            SZrInferredType *paramType =
                    (SZrInferredType *)ZrCore_Array_Get(&projection->paramTypes, i);

            if (provided[i]) {
                if (i >= argTypes->length) {
                    SZrInferredType placeholder;
                    ZrParser_InferredType_Init(cs->state, &placeholder, ZR_VALUE_TYPE_OBJECT);
                    ZrCore_Array_Push(cs->state, argTypes, &placeholder);
                }
                continue;
            }

            if (hasDefaultPtr == ZR_NULL || !*hasDefaultPtr || paramType == ZR_NULL) {
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
                SZrInferredType copiedType;
                ZrParser_InferredType_Init(cs->state, &copiedType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Copy(cs->state, &copiedType, paramType);
                ZrCore_Array_Push(cs->state, argTypes, &copiedType);
            } else {
                SZrInferredType *existing = (SZrInferredType *)ZrCore_Array_Get(argTypes, i);
                if (existing != ZR_NULL) {
                    ZrParser_InferredType_Free(cs->state, existing);
                    ZrParser_InferredType_Copy(cs->state, existing, paramType);
                }
            }
        }

        goto cleanup;
    }

    if (declNode->type == ZR_AST_FUNCTION_DECLARATION) {
        paramList = declNode->data.functionDeclaration.params;
    } else if (declNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION) {
        paramList = declNode->data.externFunctionDeclaration.params;
    } else {
        return infer_function_call_argument_types(cs, call != ZR_NULL ? call->args : ZR_NULL, argTypes);
    }
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

static TZrInt32 score_function_overload_candidate(SZrCompilerState *cs,
                                                  const SZrFunctionTypeInfo *funcType,
                                                  const SZrResolvedCallSignature *resolvedSignature,
                                                  const SZrArray *argTypes,
                                                  const TZrChar **outOwnershipDiagnostic,
                                                  TZrSize *outTypeMismatchIndex) {
    if (cs == ZR_NULL || resolvedSignature == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE;
    }

    if (outOwnershipDiagnostic != ZR_NULL) {
        *outOwnershipDiagnostic = ZR_NULL;
    }
    if (outTypeMismatchIndex != ZR_NULL) {
        *outTypeMismatchIndex = argTypes->length;
    }

    if (resolvedSignature->parameterTypes.length != argTypes->length) {
        return ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE;
    }

    {
        TZrInt32 score = 0;
        for (TZrSize i = 0; i < argTypes->length; i++) {
            SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)argTypes, i);
            SZrInferredType *paramType =
                    (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterTypes, i);
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
            EZrParameterPassingMode *passingModePtr =
                    i < resolvedSignature->parameterPassingModes.length
                            ? (EZrParameterPassingMode *)ZrCore_Array_Get(
                                      (SZrArray *)&resolvedSignature->parameterPassingModes,
                                      i)
                            : ZR_NULL;

            if (passingModePtr != ZR_NULL) {
                passingMode = *passingModePtr;
            }

            if (argType == ZR_NULL || paramType == ZR_NULL) {
                return ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE;
            }

            if (!type_inference_in_parameter_accepts_owner_reborrow(
                        passingMode, paramType, argType)) {
                const TZrChar *ownershipDiagnostic =
                        type_inference_call_diagnostic_ownership_message(
                                passingMode, paramType, argType);
                if (ownershipDiagnostic != ZR_NULL) {
                    if (outOwnershipDiagnostic != ZR_NULL) {
                        *outOwnershipDiagnostic = ownershipDiagnostic;
                    }
                    if (outTypeMismatchIndex != ZR_NULL) {
                        *outTypeMismatchIndex = i;
                    }
                    return ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE;
                }
            }

            if (passingMode == ZR_PARAMETER_PASSING_MODE_REF ||
                passingMode == ZR_PARAMETER_PASSING_MODE_OUT) {
                if (!type_inference_reference_argument_type_equal(
                            argType, paramType)) {
                    return ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE;
                }
                continue;
            }

            if (ZrParser_InferredType_Equal(argType, paramType)) {
                continue;
            }

            if (!ZrParser_InferredType_IsCompatible(argType, paramType) &&
                !type_inference_in_parameter_accepts_owner_reborrow(
                        passingMode, paramType, argType) &&
                !inferred_type_can_use_named_constraint_fallback(cs, argType, paramType) &&
                !ffi_function_call_argument_is_native_boundary_compatible(cs, funcType, i, argType, paramType)) {
                if (outOwnershipDiagnostic != ZR_NULL && *outOwnershipDiagnostic == ZR_NULL) {
                    *outOwnershipDiagnostic = type_inference_ownership_flow_diagnostic_message(paramType, argType);
                }
                if (outTypeMismatchIndex != ZR_NULL) {
                    *outTypeMismatchIndex = i;
                }
                return ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE;
            }

            score += 1;
        }
        return score;
    }
}

TZrBool resolve_best_function_overload_with_context(SZrCompilerState *cs,
                                       SZrTypeEnvironment *env,
                                       SZrString *funcName,
                                       SZrSyntaxContext syntaxContext,
                                       SZrFunctionCall *call,
                                       SZrFileRange location,
                                       SZrFunctionTypeInfo **resolvedFunction,
                                       SZrResolvedCallSignature *resolvedSignature) {
    SZrArray candidates;
    TZrInt32 bestScore = INT_MAX;
    SZrFunctionTypeInfo *bestCandidate = ZR_NULL;
    SZrResolvedCallSignature bestResolvedSignature;
    TZrBool hasTie = ZR_FALSE;
    TZrBool sawGenericArityMismatch = ZR_FALSE;
    TZrBool sawGenericInferenceFailure = ZR_FALSE;
    TZrBool sawGenericKindMismatch = ZR_FALSE;
    TZrBool sawGenericConflict = ZR_FALSE;
    const TZrChar *ownershipDiagnostic = ZR_NULL;
    TZrChar genericDiagnostic[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL || env == ZR_NULL || funcName == ZR_NULL || resolvedFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    genericDiagnostic[0] = '\0';
    memset(&bestResolvedSignature, 0, sizeof(bestResolvedSignature));
    ZrParser_InferredType_Init(cs->state, &bestResolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&bestResolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&bestResolvedSignature.parameterPassingModes);
    if (!ZrParser_TypeEnvironment_LookupFunctionsWithContext(
                cs->state, env, funcName, syntaxContext, &candidates)) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < candidates.length; i++) {
        SZrFunctionTypeInfo **candidatePtr =
            (SZrFunctionTypeInfo **)ZrCore_Array_Get(&candidates, i);
        SZrArray candidateArgTypes;
        SZrResolvedCallSignature candidateResolvedSignature;
        TZrBool mismatch = ZR_FALSE;
        const TZrChar *candidateOwnershipDiagnostic = ZR_NULL;
        TZrSize typeMismatchIndex;
        TZrInt32 score;
        EZrGenericCallResolveStatus genericStatus;

        if (candidatePtr == ZR_NULL || *candidatePtr == ZR_NULL) {
            continue;
        }

        memset(&candidateResolvedSignature, 0, sizeof(candidateResolvedSignature));
        ZrParser_InferredType_Init(cs->state, &candidateResolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Construct(&candidateResolvedSignature.parameterTypes);
        ZrCore_Array_Construct(&candidateResolvedSignature.parameterPassingModes);
        genericStatus = resolve_generic_function_call_signature_detailed(cs,
                                                                         env,
                                                                         funcName,
                                                                         *candidatePtr,
                                                                         call,
                                                                         &candidateResolvedSignature,
                                                                         errorMsg,
                                                                         sizeof(errorMsg));
        if (genericStatus != ZR_GENERIC_CALL_RESOLVE_OK) {
            if (genericStatus == ZR_GENERIC_CALL_RESOLVE_ARITY_MISMATCH) {
                sawGenericArityMismatch = ZR_TRUE;
                if (genericDiagnostic[0] == '\0' && errorMsg[0] != '\0') {
                    snprintf(genericDiagnostic, sizeof(genericDiagnostic), "%s", errorMsg);
                }
            } else if (genericStatus == ZR_GENERIC_CALL_RESOLVE_CANNOT_INFER) {
                sawGenericInferenceFailure = ZR_TRUE;
                if (genericDiagnostic[0] == '\0' && errorMsg[0] != '\0') {
                    snprintf(genericDiagnostic, sizeof(genericDiagnostic), "%s", errorMsg);
                }
            } else if (genericStatus == ZR_GENERIC_CALL_RESOLVE_KIND_MISMATCH) {
                sawGenericKindMismatch = ZR_TRUE;
                if (genericDiagnostic[0] == '\0' && errorMsg[0] != '\0') {
                    snprintf(genericDiagnostic, sizeof(genericDiagnostic), "%s", errorMsg);
                }
            } else if (genericStatus == ZR_GENERIC_CALL_RESOLVE_CONFLICTING_INFERENCE) {
                sawGenericConflict = ZR_TRUE;
                if (genericDiagnostic[0] == '\0' && errorMsg[0] != '\0') {
                    snprintf(genericDiagnostic, sizeof(genericDiagnostic), "%s", errorMsg);
                }
            } else if (genericStatus == ZR_GENERIC_CALL_RESOLVE_CONFLICT) {
                sawGenericConflict = ZR_TRUE;
                if (genericDiagnostic[0] == '\0' && errorMsg[0] != '\0') {
                    snprintf(genericDiagnostic, sizeof(genericDiagnostic), "%s", errorMsg);
                }
            }
            free_resolved_call_signature(cs->state, &candidateResolvedSignature);
            continue;
        }

        if (!infer_function_call_argument_types_for_candidate(cs,
                                                              env,
                                                              funcName,
                                                              call,
                                                              *candidatePtr,
                                                              &candidateArgTypes,
                                                              &mismatch)) {
            free_resolved_call_signature(cs->state, &candidateResolvedSignature);
            if (candidates.isValid && candidates.head != ZR_NULL) {
                ZrCore_Array_Free(cs->state, &candidates);
            }
            free_resolved_call_signature(cs->state, &bestResolvedSignature);
            return ZR_FALSE;
        }

        if (mismatch) {
            free_inferred_type_array(cs->state, &candidateArgTypes);
            free_resolved_call_signature(cs->state, &candidateResolvedSignature);
            continue;
        }

        score = score_function_overload_candidate(cs,
                                                  *candidatePtr,
                                                  &candidateResolvedSignature,
                                                  &candidateArgTypes,
                                                  &candidateOwnershipDiagnostic,
                                                  &typeMismatchIndex);
        if (score == ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE) {
            if (ownershipDiagnostic == ZR_NULL && candidateOwnershipDiagnostic != ZR_NULL) {
                ownershipDiagnostic = candidateOwnershipDiagnostic;
            }
            if (candidates.length == 1U &&
                typeMismatchIndex < candidateArgTypes.length) {
                const SZrInferredType *argumentType =
                        (const SZrInferredType *)ZrCore_Array_Get(
                                &candidateArgTypes,
                                typeMismatchIndex);
                if (candidateOwnershipDiagnostic != ZR_NULL) {
                    (void)type_inference_call_diagnostic_report_ownership_mismatch(
                            cs,
                            *candidatePtr,
                            &candidateResolvedSignature,
                            call,
                            typeMismatchIndex,
                            argumentType);
                } else {
                    (void)type_inference_call_diagnostic_report_argument_mismatch(
                            cs,
                            *candidatePtr,
                            &candidateResolvedSignature,
                            call,
                            typeMismatchIndex,
                            argumentType);
                }
            }
            free_inferred_type_array(cs->state, &candidateArgTypes);
            free_resolved_call_signature(cs->state, &candidateResolvedSignature);
            continue;
        }
        free_inferred_type_array(cs->state, &candidateArgTypes);

        if (score < bestScore) {
            free_resolved_call_signature(cs->state, &bestResolvedSignature);
            bestScore = score;
            bestCandidate = *candidatePtr;
            bestResolvedSignature = candidateResolvedSignature;
            hasTie = ZR_FALSE;
        } else if (score == bestScore) {
            free_resolved_call_signature(cs->state, &candidateResolvedSignature);
            hasTie = ZR_TRUE;
        } else {
            free_resolved_call_signature(cs->state, &candidateResolvedSignature);
        }
    }

    if (candidates.isValid && candidates.head != ZR_NULL) {
        ZrCore_Array_Free(cs->state, &candidates);
    }

    if (bestCandidate == ZR_NULL) {
        if (genericDiagnostic[0] != '\0' &&
            (sawGenericArityMismatch || sawGenericInferenceFailure || sawGenericKindMismatch || sawGenericConflict)) {
            snprintf(errorMsg, sizeof(errorMsg), "%s", genericDiagnostic);
        } else if (ownershipDiagnostic != ZR_NULL) {
            snprintf(errorMsg, sizeof(errorMsg), "%s", ownershipDiagnostic);
        } else {
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "No matching overload for function '%s'",
                     ZrCore_String_GetNativeString(funcName));
        }
        if (!cs->hasError) {
            ZrParser_Compiler_Error(cs, errorMsg, location);
        }
        free_resolved_call_signature(cs->state, &bestResolvedSignature);
        return ZR_FALSE;
    }

    if (hasTie) {
        snprintf(errorMsg,
                 sizeof(errorMsg),
                 "Ambiguous overload for function '%s'",
                 ZrCore_String_GetNativeString(funcName));
        ZrParser_Compiler_Error(cs, errorMsg, location);
        free_resolved_call_signature(cs->state, &bestResolvedSignature);
        return ZR_FALSE;
    }

    *resolvedFunction = bestCandidate;
    if (resolvedSignature != ZR_NULL) {
        *resolvedSignature = bestResolvedSignature;
    } else {
        free_resolved_call_signature(cs->state, &bestResolvedSignature);
    }
    return ZR_TRUE;
}

TZrBool resolve_best_function_overload(
        SZrCompilerState *cs,
        SZrTypeEnvironment *env,
        SZrString *funcName,
        SZrFunctionCall *call,
        SZrFileRange location,
        SZrFunctionTypeInfo **resolvedFunction,
        SZrResolvedCallSignature *resolvedSignature) {
    return resolve_best_function_overload_with_context(
            cs, env, funcName, ZrParser_SyntaxContext_Root(), call, location,
            resolvedFunction, resolvedSignature);
}

TZrBool ZrParser_FunctionCallOverload_Resolve(SZrCompilerState *cs,
                                              SZrTypeEnvironment *env,
                                              SZrString *funcName,
                                              SZrFunctionCall *call,
                                              SZrFileRange location,
                                              SZrFunctionTypeInfo **resolvedFunction,
                                              SZrResolvedCallSignature *resolvedSignature) {
    return resolve_best_function_overload(cs,
                                          env,
                                          funcName,
                                          call,
                                          location,
                                          resolvedFunction,
                                          resolvedSignature);
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

static TZrBool type_prototype_pointer_is_currently_registered(SZrCompilerState *cs,
                                                              const SZrTypePrototypeInfo *info) {
    const TZrUInt8 *head;
    const TZrUInt8 *ptr;
    TZrSize bytesLength;
    TZrSize offset;

    if (cs == ZR_NULL || info == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cs->currentTypePrototypeInfo == info) {
        return ZR_TRUE;
    }

    if (!cs->typePrototypes.isValid ||
        cs->typePrototypes.head == ZR_NULL ||
        cs->typePrototypes.elementSize == 0 ||
        cs->typePrototypes.length == 0) {
        return ZR_FALSE;
    }

    head = (const TZrUInt8 *)cs->typePrototypes.head;
    ptr = (const TZrUInt8 *)info;
    bytesLength = cs->typePrototypes.elementSize * cs->typePrototypes.length;
    if (ptr < head || ptr >= head + bytesLength) {
        return ZR_FALSE;
    }

    offset = (TZrSize)(ptr - head);
    return (offset % cs->typePrototypes.elementSize) == 0;
}

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype_inference_exact(cs, typeName);
    if (info != ZR_NULL && !type_prototype_pointer_is_currently_registered(cs, info)) {
        info = find_compiler_type_prototype_inference_exact(cs, typeName);
        if (info != ZR_NULL && !type_prototype_pointer_is_currently_registered(cs, info)) {
            info = find_registered_type_prototype_inference_exact_only_local(cs, typeName);
        }
    }
    if (info != ZR_NULL) {
        return info;
    }
    if (ensure_generic_instance_type_prototype(cs, typeName)) {
        return find_compiler_type_prototype_inference_exact(cs, typeName);
    }
    return ZR_NULL;
}

SZrTypePrototypeInfo *type_inference_find_member_relation_prototype_root(
        SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    info = find_registered_type_prototype_inference_exact_only_local(cs, typeName);
    if (info != ZR_NULL) {
        return info;
    }

    return find_compiler_type_prototype_inference(cs, typeName);
}

TZrBool type_inference_member_prototype_is_registered(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype) {
    return type_prototype_pointer_is_currently_registered(cs, prototype);
}

TZrBool type_name_is_module_prototype_inference(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype_inference(cs, typeName);
    return info != ZR_NULL && info->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
}

ZR_PARSER_API TZrBool resolve_prototype_target_inference(SZrCompilerState *cs,
                                                         SZrAstNode *node,
                                                         SZrTypePrototypeInfo **outPrototype,
                                                         SZrString **outTypeName) {
    SZrAstNode *targetNode = node;
    SZrString *typeName = ZR_NULL;
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    SZrInferredType inferredType;
    TZrBool identifierBoundAsVariable = ZR_FALSE;

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
        TZrBool identifierNamesVisibleAsType = ZR_FALSE;

        typeName = targetNode->data.identifier.name;
        if (typeName != ZR_NULL) {
            prototype = find_compiler_type_prototype_inference(cs, typeName);
            identifierNamesVisibleAsType =
                    type_name_is_explicitly_available_in_context_inference(cs, typeName);
        }

        if (prototype != ZR_NULL && !identifierNamesVisibleAsType) {
            prototype = ZR_NULL;
        }

        /* Prototype-target contexts must prefer registered type names even when
         * source extern bindings also publish a same-named descriptor value. */
        if (prototype == ZR_NULL && !identifierNamesVisibleAsType &&
            cs->typeEnv != ZR_NULL && typeName != ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
            identifierBoundAsVariable =
                    ZrParser_TypeEnvironment_LookupVariableWithContext(
                            cs->state,
                            cs->typeEnv,
                            typeName,
                            targetNode->data.identifier.syntaxContext,
                            &inferredType);
            ZrParser_InferredType_Free(cs->state, &inferredType);
        }

        if (!identifierBoundAsVariable && prototype == ZR_NULL && identifierNamesVisibleAsType) {
            prototype = find_compiler_type_prototype_inference(cs, typeName);
        }
    } else if (targetNode->type == ZR_AST_TYPE) {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, &targetNode->data.type, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }
        typeName = inferredType.typeName;
        ensure_generic_instance_type_prototype_from_inferred(cs, &inferredType);
        prototype = type_inference_find_nominal_prototype(cs, &inferredType);
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }

    if (prototype == ZR_NULL &&
        targetNode->type != ZR_AST_IDENTIFIER_LITERAL &&
        targetNode->type != ZR_AST_PRIMARY_EXPRESSION &&
        targetNode->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        /* A constructed object is a value receiver, even when its closed
         * inferred type name resolves to a registered prototype. */
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, targetNode, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }
        typeName = inferredType.typeName;
        prototype = find_compiler_type_prototype_inference(cs, typeName);
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }

    if (prototype == ZR_NULL && targetNode->type == ZR_AST_PRIMARY_EXPRESSION) {
        TZrBool isTypeReference = ZR_FALSE;
        SZrString *resolvedRootTypeName = ZR_NULL;

        if (resolve_expression_root_type(cs, targetNode, &resolvedRootTypeName, &isTypeReference) &&
            isTypeReference &&
            resolvedRootTypeName != ZR_NULL) {
            typeName = resolvedRootTypeName;
            prototype = find_compiler_type_prototype_inference(cs, typeName);
        }
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
        if (cs->hasError) {
            return ZR_FALSE;
        }
        if (!resolve_source_type_declaration_target_inference(cs,
                                                              node,
                                                              &typeName,
                                                              ZR_NULL,
                                                              ZR_NULL,
                                                              ZR_NULL)) {
            ZrParser_Compiler_Error(cs,
                            "Prototype reference target must resolve to a registered prototype",
                            node->location);
            return ZR_FALSE;
        }
    }

    return inferred_type_from_type_name(cs, typeName, result);
}

static const TZrChar *ownership_builtin_operand_error_message(EZrOwnershipBuiltinKind builtinKind) {
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARE:
            return "share(owner) requires a Unique owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE:
            return "degrade(shared) requires a Shared owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            return "A mutable reference requires a Unique owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_WAKE:
            return "wake(weak) requires a Weak owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_DROP:
            return "drop(owner) requires a Unique or Shared owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC:
            return "intoGc(owner) requires a Unique<T> resource owner";
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
        default:
            return ZR_NULL;
    }
}

static TZrBool ownership_builtin_operand_matches_qualifier(EZrOwnershipBuiltinKind builtinKind,
                                                           EZrOwnershipQualifier qualifier) {
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARE:
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
        case ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE;
        case ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED;
        case ZR_OWNERSHIP_BUILTIN_KIND_WAKE:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK;
        case ZR_OWNERSHIP_BUILTIN_KIND_DROP:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
                   qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED;
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
        default:
            return ZR_TRUE;
    }
}

static TZrBool validate_ownership_builtin_operand_type(SZrCompilerState *cs,
                                                       EZrOwnershipBuiltinKind builtinKind,
                                                       const SZrInferredType *operandType,
                                                       SZrFileRange errorLocation) {
    const TZrChar *errorMessage = ownership_builtin_operand_error_message(builtinKind);

    if (errorMessage == ZR_NULL) {
        return ZR_TRUE;
    }

    if (operandType == ZR_NULL ||
        !ownership_builtin_operand_matches_qualifier(builtinKind, operandType->ownershipQualifier)) {
        ZrParser_Compiler_Error(cs, errorMessage, errorLocation);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static SZrTypeMemberInfo *find_type_constructor_member_inference(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    prototype = find_compiler_type_prototype_inference(cs, typeName);
    if (prototype == ZR_NULL) {
        ensure_generic_instance_type_prototype(cs, typeName);
        prototype = find_compiler_type_prototype_inference(cs, typeName);
    }
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < prototype->members.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, index);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == ZR_META_CONSTRUCTOR) {
            return memberInfo;
        }
    }

    return ZR_NULL;
}

static TZrBool type_name_is_resource_declaration_inference(SZrCompilerState *cs,
                                                            SZrTypePrototypeInfo *prototype,
                                                            SZrString *typeName) {
    SZrString *lookupTypeName;
    SZrAstNode *declarationNode;

    if (prototype != ZR_NULL) {
        return (TZrBool)((prototype->modifierFlags & ZR_DECLARATION_MODIFIER_RESOURCE) != 0);
    }
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    lookupTypeName = resolve_source_declaration_lookup_type_name_inference(cs, typeName);
    declarationNode = lookupTypeName != ZR_NULL
                              ? find_type_declaration(cs, lookupTypeName)
                              : ZR_NULL;
    return (TZrBool)(declarationNode != ZR_NULL &&
                     declarationNode->type == ZR_AST_CLASS_DECLARATION &&
                     (declarationNode->data.classDeclaration.modifierFlags &
                      ZR_DECLARATION_MODIFIER_RESOURCE) != 0);
}

static void infer_construct_apply_task_job_ownership(
        SZrCompilerState *cs,
        SZrInferredType *result) {
    if (cs != ZR_NULL &&
        result != ZR_NULL &&
        result->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
        inferred_type_implements_protocol_mask(
                cs,
                result,
                ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_JOB))) {
        result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
    }
}

TZrBool infer_struct_init_expression_type(SZrCompilerState *cs,
                                          SZrAstNode *node,
                                          SZrInferredType *result) {
    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        node->type != ZR_AST_STRUCT_INIT_EXPRESSION ||
        node->data.structInitExpression.typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrParser_AstTypeToInferredType_Convert(
                cs, node->data.structInitExpression.typeInfo, result)) {
        return ZR_FALSE;
    }
    infer_construct_apply_task_job_ownership(cs, result);
    return ZR_TRUE;
}

TZrBool infer_construct_expression_type(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        SZrInferredType *result) {
    SZrConstructExpression *construct;
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    SZrString *typeName = ZR_NULL;
    EZrOwnershipBuiltinKind builtinKind;
    EZrObjectPrototypeType resolvedPrototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    TZrBool allowValueConstruction = ZR_FALSE;
    TZrBool allowBoxedConstruction = ZR_FALSE;
    TZrBool isResourceType;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        node->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        return ZR_FALSE;
    }

    construct = &node->data.constructExpression;
    builtinKind = construct->builtinKind;

    if (!construct->isNew && builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE) {
        switch (builtinKind) {
            case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
            case ZR_OWNERSHIP_BUILTIN_KIND_SHARE:
            case ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE:
            case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
            case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            case ZR_OWNERSHIP_BUILTIN_KIND_WAKE:
            case ZR_OWNERSHIP_BUILTIN_KIND_DROP:
            case ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC:
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
            case ZR_OWNERSHIP_BUILTIN_KIND_RETURN_LOAN:
            default:
                ZrParser_Compiler_Error(
                        cs, "Unknown ownership builtin kind", node->location);
                return ZR_FALSE;
        }
        if (!ZrParser_ExpressionType_Infer(cs, construct->target, result)) {
            return ZR_FALSE;
        }

        if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_BORROW &&
            result->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE) {
            builtinKind = ZR_OWNERSHIP_BUILTIN_KIND_LOAN;
            construct->builtinKind = builtinKind;
        }

        if (!validate_ownership_builtin_operand_type(cs,
                                                     builtinKind,
                                                     result,
                                                     construct->target != ZR_NULL ? construct->target->location : node->location)) {
            return ZR_FALSE;
        }

        if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_DROP) {
            result->baseType = ZR_VALUE_TYPE_NULL;
            result->isNullable = ZR_FALSE;
            result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            result->referenceAccess = ZR_REFERENCE_ACCESS_NONE;
            result->gcBridgeKind = ZR_GC_BRIDGE_NONE;
            result->typeName = ZR_NULL;
            ZrCore_Array_Construct(&result->elementTypes);
            result->minValue = 0;
            result->maxValue = 0;
            result->hasRangeConstraint = ZR_FALSE;
            ZrCore_Array_Construct(&result->rangeExtraSegments);
            ZrParser_InferredType_ResetRangeSegments(result);
            result->arrayFixedSize = 0;
            result->arrayMinSize = 0;
            result->arrayMaxSize = 0;
            result->hasArraySizeConstraint = ZR_FALSE;
            return ZR_TRUE;
        }

        switch (builtinKind) {
            case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_UNIQUE;
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_SHARE:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
                result->referenceAccess = ZR_REFERENCE_ACCESS_READONLY;
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
                result->referenceAccess = ZR_REFERENCE_ACCESS_WRITABLE;
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
                result->referenceAccess = ZR_REFERENCE_ACCESS_NONE;
                result->gcBridgeKind = ZR_GC_BRIDGE_BOX;
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_WAKE:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
                result->referenceAccess = ZR_REFERENCE_ACCESS_NONE;
                result->isNullable = ZR_TRUE;
                break;
            case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
            case ZR_OWNERSHIP_BUILTIN_KIND_RETURN_LOAN:
            default:
                result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
                break;
        }
        type_inference_record_ownership_builtin_fact(cs, node, builtinKind, result->ownershipQualifier);
        return ZR_TRUE;
    }

    if (!resolve_prototype_target_inference(cs, construct->target, &prototype, &typeName)) {
        if (cs->hasError) {
            return ZR_FALSE;
        }
        if (!resolve_source_type_declaration_target_inference(cs,
                                                              construct->target,
                                                              &typeName,
                                                              &resolvedPrototypeType,
                                                              &allowValueConstruction,
                                                              &allowBoxedConstruction)) {
            ZrParser_Compiler_Error(cs,
                            "Construct target must resolve to a registered prototype",
                            node->location);
            return ZR_FALSE;
        }
    } else {
        resolvedPrototypeType = prototype->type;
        allowValueConstruction = prototype->allowValueConstruction;
        allowBoxedConstruction = prototype->allowBoxedConstruction;
    }

    isResourceType = type_name_is_resource_declaration_inference(cs, prototype, typeName);

    if (construct->isResourceSurface && construct->isNew &&
        builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE &&
        !isResourceType) {
        ZrParser_Compiler_Error(cs,
                               "'own' requires a resource class target",
                               node->location);
        return ZR_FALSE;
    }

    if (isResourceType &&
        (!construct->isNew || builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE)) {
        ZrParser_Compiler_Error(cs,
                               "Resource classes must be constructed with 'own'",
                               node->location);
        return ZR_FALSE;
    }

    if (resolvedPrototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
        ZrParser_Compiler_Error(cs, "Interfaces cannot be constructed", node->location);
        return ZR_FALSE;
    }

    if (!construct->isNew && !allowValueConstruction) {
        ZrParser_Compiler_Error(cs, "Prototype does not allow value construction", node->location);
        return ZR_FALSE;
    }

    if (construct->isNew && !allowBoxedConstruction) {
        if (prototype != ZR_NULL &&
            (prototype->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT) != 0) {
            ZrParser_Compiler_Error(cs, "Abstract classes cannot be constructed", node->location);
        } else {
            ZrParser_Compiler_Error(cs, "Prototype does not allow boxed construction", node->location);
        }
        return ZR_FALSE;
    }

    if (prototype != ZR_NULL && prototype->genericParameters.length > 0 && construct->args != ZR_NULL &&
        construct->args->count > 0) {
        SZrTypeMemberInfo *constructorMember = find_type_constructor_member_inference(cs, typeName);
        SZrString *materializedTypeName = ZR_NULL;
        TZrChar diagnosticBuffer[ZR_PARSER_ERROR_BUFFER_LENGTH];
        EZrGenericCallResolveStatus genericStatus;

        if (constructorMember != ZR_NULL) {
            genericStatus = resolve_generic_constructed_type_name(cs,
                                                                  typeName,
                                                                  &prototype->genericParameters,
                                                                  &constructorMember->parameterTypes,
                                                                  construct->args,
                                                                  &materializedTypeName,
                                                                  diagnosticBuffer,
                                                                  sizeof(diagnosticBuffer));
            if (genericStatus != ZR_GENERIC_CALL_RESOLVE_OK) {
                ZrParser_Compiler_Error(cs,
                                        diagnosticBuffer[0] != '\0'
                                                ? diagnosticBuffer
                                                : "Generic construct target could not be resolved from constructor arguments",
                                        node->location);
                return ZR_FALSE;
            }

            if (materializedTypeName != ZR_NULL) {
                typeName = materializedTypeName;
                ensure_generic_instance_type_prototype(cs, typeName);
                prototype = find_compiler_type_prototype_inference(cs, typeName);
            }
        }
    }

    {
        SZrAstNode *targetNode = construct->target;

        if (targetNode != ZR_NULL &&
            targetNode->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
            targetNode = targetNode->data.prototypeReferenceExpression.target;
        }
        if (targetNode != ZR_NULL && targetNode->type == ZR_AST_TYPE) {
            SZrInferredType targetType;

            ZrParser_InferredType_Init(cs->state, &targetType, ZR_VALUE_TYPE_OBJECT);
            if (ZrParser_AstTypeToInferredType_Convert(
                        cs,
                        &targetNode->data.type,
                        &targetType) &&
                targetType.typeName != ZR_NULL &&
                typeName != ZR_NULL &&
                ZrCore_String_Equal(targetType.typeName, typeName)) {
                ZrParser_InferredType_Copy(cs->state, result, &targetType);
                ZrParser_InferredType_Free(cs->state, &targetType);
                result->ownershipQualifier =
                        builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE
                                ? ZR_OWNERSHIP_QUALIFIER_UNIQUE
                                : ZR_OWNERSHIP_QUALIFIER_NONE;
                infer_construct_apply_task_job_ownership(cs, result);
                return ZR_TRUE;
            }
            ZrParser_InferredType_Free(cs->state, &targetType);
            if (cs->hasError) {
                return ZR_FALSE;
            }
        }
        if (!inferred_type_from_type_name(cs, typeName, result)) {
            return ZR_FALSE;
        }
    }

    result->ownershipQualifier =
            builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE
                    ? ZR_OWNERSHIP_QUALIFIER_UNIQUE
                    : ZR_OWNERSHIP_QUALIFIER_NONE;
    infer_construct_apply_task_job_ownership(cs, result);
    return ZR_TRUE;
}
