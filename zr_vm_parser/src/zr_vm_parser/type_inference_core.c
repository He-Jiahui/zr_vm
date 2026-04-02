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

static TZrBool type_inference_copy_generic_parameter_info_array(SZrState *state,
                                                                SZrArray *dest,
                                                                const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(dest);
    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, dest, sizeof(SZrTypeGenericParameterInfo), src->length);
    for (TZrSize index = 0; index < src->length; index++) {
        SZrTypeGenericParameterInfo *srcInfo = (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)src, index);
        SZrTypeGenericParameterInfo destInfo;

        if (srcInfo == ZR_NULL) {
            continue;
        }

        memset(&destInfo, 0, sizeof(destInfo));
        destInfo = *srcInfo;
        ZrCore_Array_Construct(&destInfo.constraintTypeNames);
        if (srcInfo->constraintTypeNames.length > 0) {
            ZrCore_Array_Init(state,
                              &destInfo.constraintTypeNames,
                              sizeof(SZrString *),
                              srcInfo->constraintTypeNames.length);
            for (TZrSize constraintIndex = 0; constraintIndex < srcInfo->constraintTypeNames.length; constraintIndex++) {
                SZrString **constraintNamePtr =
                        (SZrString **)ZrCore_Array_Get(&srcInfo->constraintTypeNames, constraintIndex);
                if (constraintNamePtr != ZR_NULL && *constraintNamePtr != ZR_NULL) {
                    ZrCore_Array_Push(state, &destInfo.constraintTypeNames, constraintNamePtr);
                }
            }
        }

        ZrCore_Array_Push(state, dest, &destInfo);
    }

    return ZR_TRUE;
}

static TZrBool type_inference_copy_parameter_passing_mode_array(SZrState *state,
                                                                SZrArray *dest,
                                                                const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(dest);
    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, dest, sizeof(EZrParameterPassingMode), src->length);
    for (TZrSize index = 0; index < src->length; index++) {
        EZrParameterPassingMode *mode = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)src, index);
        if (mode != ZR_NULL) {
            ZrCore_Array_Push(state, dest, mode);
        }
    }

    return ZR_TRUE;
}

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
        if (type->hasArraySizeConstraint && type->arrayFixedSize > 0) {
            TZrChar sizeBuffer[32];
            TZrInt32 written = snprintf(sizeBuffer, sizeof(sizeBuffer), "[%zu]", type->arrayFixedSize);
            if (written <= 0 || (TZrSize)written >= sizeof(sizeBuffer)) {
                return ZR_FALSE;
            }
            return append_text_fragment(buffer, bufferSize, offset, sizeBuffer);
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

TZrBool try_parse_generic_instance_type_name(SZrState *state,
                                             SZrString *typeName,
                                             SZrString **outBaseName,
                                             SZrArray *outArgumentTypeNames) {
    TZrNativeString nativeTypeName;
    TZrSize nativeTypeNameLength;
    TZrSize genericStart = (TZrSize)-1;
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

    if (genericStart == (TZrSize)-1 || nativeTypeName[nativeTypeNameLength - 1] != '>') {
        return ZR_FALSE;
    }

    if (!create_trimmed_type_name_segment(state, nativeTypeName, 0, genericStart, outBaseName)) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, outArgumentTypeNames, sizeof(SZrString *), 2);
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

static SZrTypePrototypeInfo *find_compiler_type_prototype_inference_exact(SZrCompilerState *cs, SZrString *typeName) {
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

static TZrBool type_name_matches_constraint_base(SZrString *constraintTypeName, const TZrChar *baseName) {
    TZrNativeString constraintName;
    TZrSize baseLength;

    if (constraintTypeName == ZR_NULL || baseName == ZR_NULL) {
        return ZR_FALSE;
    }

    constraintName = ZrCore_String_GetNativeString(constraintTypeName);
    if (constraintName == ZR_NULL) {
        return ZR_FALSE;
    }

    baseLength = strlen(baseName);
    return strncmp(constraintName, baseName, baseLength) == 0 &&
           (constraintName[baseLength] == '\0' || constraintName[baseLength] == '<');
}

static TZrBool implemented_type_satisfies_constraint_name(SZrString *implementedTypeName, SZrString *constraintTypeName) {
    TZrNativeString constraintName;

    if (implementedTypeName == ZR_NULL || constraintTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_String_Equal(implementedTypeName, constraintTypeName)) {
        return ZR_TRUE;
    }

    constraintName = ZrCore_String_GetNativeString(constraintTypeName);
    if (constraintName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strchr(constraintName, '<') == ZR_NULL) {
        return type_name_matches_constraint_base(implementedTypeName, constraintName);
    }

    return ZR_FALSE;
}

static TZrBool array_type_matches_constraint_argument(SZrCompilerState *cs,
                                                      const SZrInferredType *type,
                                                      SZrString *constraintTypeName) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrInferredType *elementType;
    TZrBool matches = ZR_TRUE;

    if (cs == ZR_NULL || type == ZR_NULL || constraintTypeName == ZR_NULL || type->elementTypes.length == 0) {
        return ZR_FALSE;
    }

    elementType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0);
    if (elementType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    if (!try_parse_generic_instance_type_name(cs->state, constraintTypeName, &baseName, &argumentTypeNames)) {
        return ZR_TRUE;
    }
    ZR_UNUSED_PARAMETER(baseName);

    if (argumentTypeNames.length > 0) {
        SZrString **argumentTypeNamePtr = (SZrString **)ZrCore_Array_Get(&argumentTypeNames, 0);
        SZrInferredType expectedElementType;

        matches = ZR_FALSE;
        ZrParser_InferredType_Init(cs->state, &expectedElementType, ZR_VALUE_TYPE_OBJECT);
        if (argumentTypeNamePtr != ZR_NULL &&
            *argumentTypeNamePtr != ZR_NULL &&
            inferred_type_from_type_name(cs, *argumentTypeNamePtr, &expectedElementType)) {
            matches = ZrParser_InferredType_Equal(elementType, &expectedElementType) ||
                      ZrParser_InferredType_IsCompatible(elementType, &expectedElementType);
        }
        ZrParser_InferredType_Free(cs->state, &expectedElementType);
    }

    ZrCore_Array_Free(cs->state, &argumentTypeNames);
    return matches;
}

static TZrBool primitive_type_satisfies_named_constraint(SZrCompilerState *cs,
                                                         const SZrInferredType *type,
                                                         SZrString *constraintTypeName) {
    if (cs == ZR_NULL || type == ZR_NULL || constraintTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (type_name_matches_constraint_base(constraintTypeName, "Equatable") ||
        type_name_matches_constraint_base(constraintTypeName, "Hashable")) {
        return type->baseType == ZR_VALUE_TYPE_BOOL ||
               type->baseType == ZR_VALUE_TYPE_STRING ||
               ZR_VALUE_IS_TYPE_INT(type->baseType) ||
               ZR_VALUE_IS_TYPE_FLOAT(type->baseType);
    }

    if (type_name_matches_constraint_base(constraintTypeName, "Comparable")) {
        return type->baseType == ZR_VALUE_TYPE_STRING ||
               ZR_VALUE_IS_TYPE_INT(type->baseType) ||
               ZR_VALUE_IS_TYPE_FLOAT(type->baseType);
    }

    if (type->baseType == ZR_VALUE_TYPE_ARRAY &&
        (type_name_matches_constraint_base(constraintTypeName, "ArrayLike") ||
         type_name_matches_constraint_base(constraintTypeName, "Iterable"))) {
        return array_type_matches_constraint_argument(cs, type, constraintTypeName);
    }

    return ZR_FALSE;
}

static TZrBool prototype_implements_constraint_recursive(SZrCompilerState *cs,
                                                         SZrTypePrototypeInfo *prototype,
                                                         SZrString *constraintTypeName,
                                                         TZrUInt32 depth) {
    if (cs == ZR_NULL || prototype == ZR_NULL || constraintTypeName == ZR_NULL || depth > 32) {
        return ZR_FALSE;
    }

    if (prototype->name != ZR_NULL &&
        implemented_type_satisfies_constraint_name(prototype->name, constraintTypeName)) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < prototype->implements.length; index++) {
        SZrString **implementedNamePtr = (SZrString **)ZrCore_Array_Get(&prototype->implements, index);
        SZrTypePrototypeInfo *implementedPrototype;
        if (implementedNamePtr == ZR_NULL || *implementedNamePtr == ZR_NULL) {
            continue;
        }
        if (implemented_type_satisfies_constraint_name(*implementedNamePtr, constraintTypeName)) {
            return ZR_TRUE;
        }
        ensure_generic_instance_type_prototype(cs, *implementedNamePtr);
        implementedPrototype = find_compiler_type_prototype_inference_exact(cs, *implementedNamePtr);
        if (implementedPrototype != ZR_NULL &&
            prototype_implements_constraint_recursive(cs, implementedPrototype, constraintTypeName, depth + 1)) {
            return ZR_TRUE;
        }
    }

    for (TZrSize index = 0; index < prototype->inherits.length; index++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&prototype->inherits, index);
        SZrTypePrototypeInfo *superPrototype;
        if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
            continue;
        }
        if (implemented_type_satisfies_constraint_name(*inheritNamePtr, constraintTypeName)) {
            return ZR_TRUE;
        }
        ensure_generic_instance_type_prototype(cs, *inheritNamePtr);
        superPrototype = find_compiler_type_prototype_inference_exact(cs, *inheritNamePtr);
        if (superPrototype != ZR_NULL &&
            prototype_implements_constraint_recursive(cs, superPrototype, constraintTypeName, depth + 1)) {
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

    ensure_generic_instance_type_prototype(cs, actualType->typeName);
    return find_compiler_type_prototype_inference_exact(cs, actualType->typeName);
}

static TZrBool inferred_type_is_primitive_value_type(const SZrInferredType *actualType) {
    if (actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    return actualType->baseType == ZR_VALUE_TYPE_BOOL ||
           ZR_VALUE_IS_TYPE_INT(actualType->baseType) ||
           ZR_VALUE_IS_TYPE_FLOAT(actualType->baseType);
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

static TZrBool inferred_type_satisfies_constraint(SZrCompilerState *cs,
                                                  const SZrInferredType *actualType,
                                                  SZrString *constraintTypeName) {
    SZrTypePrototypeInfo *actualPrototype;

    if (cs == ZR_NULL || actualType == ZR_NULL || constraintTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (primitive_type_satisfies_named_constraint(cs, actualType, constraintTypeName)) {
        return ZR_TRUE;
    }

    if (actualType->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    actualPrototype = resolve_constraint_actual_prototype(cs, actualType);
    if (actualPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return prototype_implements_constraint_recursive(cs, actualPrototype, constraintTypeName, 0);
}

static SZrString *substitute_generic_type_name(SZrState *state,
                                               SZrString *sourceTypeName,
                                               const SZrArray *genericParameters,
                                               const SZrArray *argumentTypeNames) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentNames;
    TZrNativeString nativeTypeName;
    TZrSize nativeTypeNameLength;

    if (state == ZR_NULL || sourceTypeName == ZR_NULL || genericParameters == ZR_NULL || argumentTypeNames == ZR_NULL) {
        return sourceTypeName;
    }

    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *parameterInfo =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)genericParameters, index);
        SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)argumentTypeNames, index);
        if (parameterInfo == ZR_NULL || parameterInfo->name == ZR_NULL || argumentNamePtr == ZR_NULL || *argumentNamePtr == ZR_NULL) {
            continue;
        }
        if (ZrCore_String_Equal(parameterInfo->name, sourceTypeName)) {
            return *argumentNamePtr;
        }
    }

    nativeTypeName = ZrCore_String_GetNativeString(sourceTypeName);
    nativeTypeNameLength = nativeTypeName != ZR_NULL ? strlen(nativeTypeName) : 0;
    if (nativeTypeName != ZR_NULL &&
        nativeTypeNameLength > 2 &&
        strcmp(nativeTypeName + nativeTypeNameLength - 2, "[]") == 0) {
        SZrString *elementTypeName = ZrCore_String_Create(state, nativeTypeName, nativeTypeNameLength - 2);
        SZrString *substitutedElement = substitute_generic_type_name(state,
                                                                     elementTypeName,
                                                                     genericParameters,
                                                                     argumentTypeNames);
        TZrChar buffer[512];
        TZrNativeString elementNative = substitutedElement != ZR_NULL ? ZrCore_String_GetNativeString(substitutedElement) : ZR_NULL;
        if (elementNative == ZR_NULL) {
            return sourceTypeName;
        }
        snprintf(buffer, sizeof(buffer), "%s[]", elementNative);
        return ZrCore_String_CreateFromNative(state, buffer);
    }

    ZrCore_Array_Construct(&argumentNames);
    if (try_parse_generic_instance_type_name(state, sourceTypeName, &baseName, &argumentNames)) {
        TZrChar buffer[512];
        TZrSize offset = 0;
        SZrString *resultName = ZR_NULL;
        buffer[0] = '\0';
        if (!append_text_fragment(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(baseName)) ||
            !append_text_fragment(buffer, sizeof(buffer), &offset, "<")) {
            ZrCore_Array_Free(state, &argumentNames);
            return sourceTypeName;
        }
        for (TZrSize index = 0; index < argumentNames.length; index++) {
            SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentNames, index);
            SZrString *substitutedName;
            if (argumentNamePtr == ZR_NULL || *argumentNamePtr == ZR_NULL) {
                continue;
            }
            if (index > 0 && !append_text_fragment(buffer, sizeof(buffer), &offset, ", ")) {
                ZrCore_Array_Free(state, &argumentNames);
                return sourceTypeName;
            }
            substitutedName = substitute_generic_type_name(state, *argumentNamePtr, genericParameters, argumentTypeNames);
            if (substitutedName == ZR_NULL ||
                !append_text_fragment(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(substitutedName))) {
                ZrCore_Array_Free(state, &argumentNames);
                return sourceTypeName;
            }
        }
        if (append_text_fragment(buffer, sizeof(buffer), &offset, ">")) {
            resultName = ZrCore_String_Create(state, buffer, offset);
        }
        ZrCore_Array_Free(state, &argumentNames);
        if (resultName != ZR_NULL) {
            return resultName;
        }
    }

    return sourceTypeName;
}

static TZrBool substitute_parameter_type(SZrCompilerState *cs,
                                         const SZrInferredType *sourceType,
                                         const SZrArray *genericParameters,
                                         const SZrArray *argumentTypeNames,
                                         SZrInferredType *outType) {
    SZrString *substitutedTypeName;

    if (cs == ZR_NULL || sourceType == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, outType, ZR_VALUE_TYPE_OBJECT);
    if (sourceType->typeName == ZR_NULL) {
        ZrParser_InferredType_Copy(cs->state, outType, sourceType);
        return ZR_TRUE;
    }

    substitutedTypeName = substitute_generic_type_name(cs->state,
                                                       sourceType->typeName,
                                                       genericParameters,
                                                       argumentTypeNames);
    if (substitutedTypeName == ZR_NULL || !inferred_type_from_type_name(cs, substitutedTypeName, outType)) {
        return ZR_FALSE;
    }

    outType->ownershipQualifier = sourceType->ownershipQualifier;
    outType->isNullable = sourceType->isNullable;
    outType->minValue = sourceType->minValue;
    outType->maxValue = sourceType->maxValue;
    outType->hasRangeConstraint = sourceType->hasRangeConstraint;
    outType->arrayFixedSize = sourceType->arrayFixedSize;
    outType->arrayMinSize = sourceType->arrayMinSize;
    outType->arrayMaxSize = sourceType->arrayMaxSize;
    outType->hasArraySizeConstraint = sourceType->hasArraySizeConstraint;
    return ZR_TRUE;
}

TZrBool ensure_generic_instance_type_prototype(SZrCompilerState *cs, SZrString *typeName) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrArray argumentTypes;
    SZrTypePrototypeInfo *openPrototype;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (find_compiler_type_prototype_inference_exact(cs, typeName) != ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    if (!try_parse_generic_instance_type_name(cs->state, typeName, &baseName, &argumentTypeNames)) {
        return ZR_FALSE;
    }

    openPrototype = find_compiler_type_prototype_inference_exact(cs, baseName);
    if (openPrototype == ZR_NULL || openPrototype->genericParameters.length == 0 ||
        openPrototype->genericParameters.length != argumentTypeNames.length) {
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_FALSE;
    }

    ZrCore_Array_Init(cs->state, &argumentTypes, sizeof(SZrInferredType), argumentTypeNames.length);
    for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
        SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
        SZrInferredType argumentType;
        if (argumentNamePtr == ZR_NULL || *argumentNamePtr == ZR_NULL) {
            continue;
        }
        ZrParser_InferredType_Init(cs->state, &argumentType, ZR_VALUE_TYPE_OBJECT);
        if (!inferred_type_from_type_name(cs, *argumentNamePtr, &argumentType)) {
            ZrParser_InferredType_Free(cs->state, &argumentType);
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, &argumentTypes, &argumentType);
    }

    for (TZrSize index = 0; index < argumentTypes.length; index++) {
        SZrTypeGenericParameterInfo *parameterInfo;
        SZrInferredType *argumentType = (SZrInferredType *)ZrCore_Array_Get(&argumentTypes, index);
        openPrototype = find_compiler_type_prototype_inference_exact(cs, baseName);
        if (openPrototype == ZR_NULL) {
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        parameterInfo =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&openPrototype->genericParameters, index);
        if (parameterInfo == ZR_NULL || argumentType == ZR_NULL) {
            continue;
        }
        if (parameterInfo->requiresClass && !inferred_type_satisfies_class_constraint(cs, argumentType)) {
            static TZrChar errorMessage[256];
            SZrFileRange errorLocation;
            snprintf(errorMessage,
                     sizeof(errorMessage),
                     "Generic argument '%s' does not satisfy class constraint",
                     ZrCore_String_GetNativeString(argumentType->typeName != ZR_NULL ? argumentType->typeName : baseName));
            memset(&errorLocation, 0, sizeof(errorLocation));
            ZrParser_Compiler_Error(cs, errorMessage, errorLocation);
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        if (parameterInfo->requiresStruct && !inferred_type_satisfies_struct_constraint(cs, argumentType)) {
            static TZrChar errorMessage[256];
            SZrFileRange errorLocation;
            snprintf(errorMessage,
                     sizeof(errorMessage),
                     "Generic argument '%s' does not satisfy struct constraint",
                     ZrCore_String_GetNativeString(argumentType->typeName != ZR_NULL ? argumentType->typeName : baseName));
            memset(&errorLocation, 0, sizeof(errorLocation));
            ZrParser_Compiler_Error(cs, errorMessage, errorLocation);
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        if (parameterInfo->requiresNew && !inferred_type_satisfies_new_constraint(cs, argumentType)) {
            static TZrChar errorMessage[256];
            SZrFileRange errorLocation;
            snprintf(errorMessage,
                     sizeof(errorMessage),
                     "Generic argument '%s' does not satisfy new() constraint",
                     ZrCore_String_GetNativeString(argumentType->typeName != ZR_NULL ? argumentType->typeName : baseName));
            memset(&errorLocation, 0, sizeof(errorLocation));
            ZrParser_Compiler_Error(cs, errorMessage, errorLocation);
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        for (TZrSize constraintIndex = 0; constraintIndex < parameterInfo->constraintTypeNames.length; constraintIndex++) {
            SZrString **constraintNamePtr =
                    (SZrString **)ZrCore_Array_Get(&parameterInfo->constraintTypeNames, constraintIndex);
            static TZrChar errorMessage[256];
            SZrFileRange errorLocation;
            if (constraintNamePtr == ZR_NULL || *constraintNamePtr == ZR_NULL) {
                continue;
            }
            if (inferred_type_satisfies_constraint(cs, argumentType, *constraintNamePtr)) {
                continue;
            }
            snprintf(errorMessage,
                     sizeof(errorMessage),
                     "Generic argument '%s' does not satisfy constraint '%s'",
                     ZrCore_String_GetNativeString(argumentType->typeName != ZR_NULL ? argumentType->typeName : baseName),
                     ZrCore_String_GetNativeString(*constraintNamePtr));
            memset(&errorLocation, 0, sizeof(errorLocation));
            ZrParser_Compiler_Error(cs, errorMessage, errorLocation);
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
    }

    {
        SZrTypePrototypeInfo closedPrototype;
        SZrTypePrototypeInfo openPrototypeSnapshot;
        SZrTypePrototypeInfo *registeredPrototype;
        TZrSize registeredPrototypeIndex;
        openPrototype = find_compiler_type_prototype_inference_exact(cs, baseName);
        if (openPrototype == ZR_NULL) {
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        memset(&closedPrototype, 0, sizeof(closedPrototype));
        openPrototypeSnapshot = *openPrototype;
        closedPrototype.name = typeName;
        closedPrototype.type = openPrototypeSnapshot.type;
        closedPrototype.accessModifier = openPrototypeSnapshot.accessModifier;
        closedPrototype.isImportedNative = openPrototypeSnapshot.isImportedNative;
        closedPrototype.extendsTypeName = substitute_generic_type_name(cs->state,
                                                                       openPrototypeSnapshot.extendsTypeName,
                                                                       &openPrototypeSnapshot.genericParameters,
                                                                       &argumentTypeNames);
        closedPrototype.enumValueTypeName = substitute_generic_type_name(cs->state,
                                                                         openPrototypeSnapshot.enumValueTypeName,
                                                                         &openPrototypeSnapshot.genericParameters,
                                                                         &argumentTypeNames);
        closedPrototype.allowValueConstruction = openPrototypeSnapshot.allowValueConstruction;
        closedPrototype.allowBoxedConstruction = openPrototypeSnapshot.allowBoxedConstruction;
        closedPrototype.constructorSignature = openPrototypeSnapshot.constructorSignature;
        ZrCore_Array_Init(cs->state, &closedPrototype.inherits, sizeof(SZrString *), openPrototypeSnapshot.inherits.length);
        ZrCore_Array_Init(cs->state, &closedPrototype.implements, sizeof(SZrString *), openPrototypeSnapshot.implements.length);
        ZrCore_Array_Init(cs->state, &closedPrototype.genericParameters, sizeof(SZrTypeGenericParameterInfo), 1);
        ZrCore_Array_Init(cs->state, &closedPrototype.members, sizeof(SZrTypeMemberInfo), openPrototypeSnapshot.members.length);

        if (cs->typeEnv != ZR_NULL) {
            ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
        }
        ZrCore_Array_Push(cs->state, &cs->typePrototypes, &closedPrototype);
        registeredPrototypeIndex = cs->typePrototypes.length - 1;
        registeredPrototype =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, registeredPrototypeIndex);
        if (registeredPrototype == ZR_NULL) {
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < openPrototypeSnapshot.inherits.length; index++) {
            SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&openPrototypeSnapshot.inherits, index);
            SZrString *inheritName;
            if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
                continue;
            }
            inheritName = substitute_generic_type_name(cs->state,
                                                       *inheritNamePtr,
                                                       &openPrototypeSnapshot.genericParameters,
                                                       &argumentTypeNames);
            ZrCore_Array_Push(cs->state, &registeredPrototype->inherits, &inheritName);
        }

        for (TZrSize index = 0; index < openPrototypeSnapshot.implements.length; index++) {
            SZrString **implementNamePtr = (SZrString **)ZrCore_Array_Get(&openPrototypeSnapshot.implements, index);
            SZrString *implementName;
            if (implementNamePtr == ZR_NULL || *implementNamePtr == ZR_NULL) {
                continue;
            }
            implementName = substitute_generic_type_name(cs->state,
                                                         *implementNamePtr,
                                                         &openPrototypeSnapshot.genericParameters,
                                                         &argumentTypeNames);
            ZrCore_Array_Push(cs->state, &registeredPrototype->implements, &implementName);
        }

        for (TZrSize index = 0; index < openPrototypeSnapshot.members.length; index++) {
            SZrTypeMemberInfo *sourceMember = (SZrTypeMemberInfo *)ZrCore_Array_Get(&openPrototypeSnapshot.members, index);
            SZrTypeMemberInfo copiedMember;
            memset(&copiedMember, 0, sizeof(copiedMember));
            if (sourceMember == ZR_NULL) {
                continue;
            }

            copiedMember = *sourceMember;
            copiedMember.fieldTypeName = substitute_generic_type_name(cs->state,
                                                                      sourceMember->fieldTypeName,
                                                                      &openPrototypeSnapshot.genericParameters,
                                                                      &argumentTypeNames);
            copiedMember.returnTypeName = substitute_generic_type_name(cs->state,
                                                                       sourceMember->returnTypeName,
                                                                       &openPrototypeSnapshot.genericParameters,
                                                                       &argumentTypeNames);
            ZrCore_Array_Construct(&copiedMember.parameterTypes);
            ZrCore_Array_Construct(&copiedMember.genericParameters);
            ZrCore_Array_Construct(&copiedMember.parameterPassingModes);
            copiedMember.declarationNode = sourceMember->declarationNode;
            if (sourceMember->parameterTypes.length > 0) {
                ZrCore_Array_Init(cs->state,
                                  &copiedMember.parameterTypes,
                                  sizeof(SZrInferredType),
                                  sourceMember->parameterTypes.length);
                for (TZrSize paramIndex = 0; paramIndex < sourceMember->parameterTypes.length; paramIndex++) {
                    SZrInferredType *sourceParameter =
                            (SZrInferredType *)ZrCore_Array_Get(&sourceMember->parameterTypes, paramIndex);
                    SZrInferredType copiedParameter;
                    if (sourceParameter == ZR_NULL) {
                        continue;
                    }
                    if (!substitute_parameter_type(cs,
                                                   sourceParameter,
                                                   &openPrototypeSnapshot.genericParameters,
                                                   &argumentTypeNames,
                                                   &copiedParameter)) {
                        continue;
                    }
                    ZrCore_Array_Push(cs->state, &copiedMember.parameterTypes, &copiedParameter);
                }
            }
            type_inference_copy_generic_parameter_info_array(cs->state,
                                                             &copiedMember.genericParameters,
                                                             &sourceMember->genericParameters);
            type_inference_copy_parameter_passing_mode_array(cs->state,
                                                             &copiedMember.parameterPassingModes,
                                                             &sourceMember->parameterPassingModes);
            registeredPrototype =
                    (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, registeredPrototypeIndex);
            if (registeredPrototype == ZR_NULL) {
                free_inferred_type_array(cs->state, &argumentTypes);
                ZrCore_Array_Free(cs->state, &argumentTypeNames);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(cs->state, &registeredPrototype->members, &copiedMember);
        }
    }

    free_inferred_type_array(cs->state, &argumentTypes);
    ZrCore_Array_Free(cs->state, &argumentTypeNames);
    return ZR_TRUE;
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

    declNode = funcType->declarationNode;
    if (declNode == ZR_NULL || declNode->type != ZR_AST_FUNCTION_DECLARATION) {
        declNode = find_function_declaration_for_candidate(cs, env, funcName, funcType);
    }
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

static TZrInt32 score_function_overload_candidate(const SZrResolvedCallSignature *resolvedSignature,
                                                  const SZrArray *argTypes) {
    if (resolvedSignature == ZR_NULL || argTypes == ZR_NULL) {
        return -1;
    }

    if (resolvedSignature->parameterTypes.length != argTypes->length) {
        return -1;
    }

    {
        TZrInt32 score = 0;
        for (TZrSize i = 0; i < argTypes->length; i++) {
            SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)argTypes, i);
            SZrInferredType *paramType =
                    (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterTypes, i);

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
    TZrChar genericDiagnostic[256];
    TZrChar errorMsg[256];

    if (cs == ZR_NULL || env == ZR_NULL || funcName == ZR_NULL || resolvedFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    genericDiagnostic[0] = '\0';
    memset(&bestResolvedSignature, 0, sizeof(bestResolvedSignature));
    ZrParser_InferredType_Init(cs->state, &bestResolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&bestResolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&bestResolvedSignature.parameterPassingModes);
    if (!ZrParser_TypeEnvironment_LookupFunctions(cs->state, env, funcName, &candidates)) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < candidates.length; i++) {
        SZrFunctionTypeInfo **candidatePtr =
            (SZrFunctionTypeInfo **)ZrCore_Array_Get(&candidates, i);
        SZrArray candidateArgTypes;
        SZrResolvedCallSignature candidateResolvedSignature;
        TZrBool mismatch = ZR_FALSE;
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

        score = score_function_overload_candidate(&candidateResolvedSignature, &candidateArgTypes);
        free_inferred_type_array(cs->state, &candidateArgTypes);
        if (score < 0) {
            free_resolved_call_signature(cs->state, &candidateResolvedSignature);
            continue;
        }

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
        } else {
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "No matching overload for function '%s'",
                     ZrCore_String_GetNativeString(funcName));
        }
        ZrParser_Compiler_Error(cs, errorMsg, location);
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

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo *info = find_compiler_type_prototype_inference_exact(cs, typeName);
    if (info != ZR_NULL) {
        return info;
    }
    if (ensure_generic_instance_type_prototype(cs, typeName)) {
        return find_compiler_type_prototype_inference_exact(cs, typeName);
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
    } else if (targetNode->type == ZR_AST_TYPE) {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, &targetNode->data.type, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }
        typeName = inferredType.typeName;
        ensure_generic_instance_type_prototype(cs, typeName);
        prototype = find_compiler_type_prototype_inference(cs, typeName);
        ZrParser_InferredType_Free(cs->state, &inferredType);
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
        if (cs->hasError) {
            return ZR_FALSE;
        }
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
        if (cs->hasError) {
            return ZR_FALSE;
        }
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
