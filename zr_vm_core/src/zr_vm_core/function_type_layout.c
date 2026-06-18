//
// Created by Codex on 2026/5/18.
//

#include "zr_vm_core/function.h"

#include <string.h>

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"

#define ZR_FUNCTION_TYPE_LAYOUT_CACHE_UNKNOWN ((TZrUInt8)0u)
#define ZR_FUNCTION_TYPE_LAYOUT_CACHE_RESOLVING ((TZrUInt8)1u)
#define ZR_FUNCTION_TYPE_LAYOUT_CACHE_READY ((TZrUInt8)2u)
#define ZR_FUNCTION_TYPE_LAYOUT_CACHE_FAILED ((TZrUInt8)3u)

typedef struct SZrFunctionPrototypeRecord {
    const SZrCompiledPrototypeInfo *prototype;
    const SZrCompiledMemberInfo *members;
} SZrFunctionPrototypeRecord;

typedef enum EZrFunctionTypeLayoutResolvedFieldKind {
    ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_SKIP = 0,
    ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_VALUE = 1,
    ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_NESTED = 2
} EZrFunctionTypeLayoutResolvedFieldKind;

typedef struct SZrFunctionTypeLayoutResolvedField {
    EZrFunctionTypeLayoutResolvedFieldKind kind;
    TZrUInt32 nestedTypeLayoutId;
    const SZrTypeLayout *nestedLayout;
} SZrFunctionTypeLayoutResolvedField;

static TZrBool function_type_layout_checked_add_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || left > ZR_MAX_SIZE - right) {
        return ZR_FALSE;
    }

    *out = left + right;
    return ZR_TRUE;
}

static TZrBool function_type_layout_checked_mul_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || (right != 0u && left > ZR_MAX_SIZE / right)) {
        return ZR_FALSE;
    }

    *out = left * right;
    return ZR_TRUE;
}

static TZrBool function_type_layout_checked_add_u32(TZrUInt32 left, TZrUInt32 right, TZrUInt32 *out) {
    if (out == ZR_NULL || left > UINT32_MAX - right) {
        return ZR_FALSE;
    }

    *out = left + right;
    return ZR_TRUE;
}

static TZrBool function_type_layout_checked_mul_u32(TZrUInt32 left, TZrUInt32 right, TZrUInt32 *out) {
    TZrSize result;

    if (out == ZR_NULL ||
        !function_type_layout_checked_mul_size(left, right, &result) ||
        result > UINT32_MAX) {
        return ZR_FALSE;
    }

    *out = (TZrUInt32)result;
    return ZR_TRUE;
}

static TZrBool function_type_layout_function_has_prototype_blob(const SZrFunction *function) {
    return (TZrBool)(function != ZR_NULL &&
                     function->prototypeData != ZR_NULL &&
                     function->prototypeDataLength >= sizeof(TZrUInt32) &&
                     function->prototypeCount > 0u);
}

static const SZrFunction *function_type_layout_entry_function_from_call_stack(SZrState *state) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    for (callInfo = state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        SZrFunction *candidateFunction;

        if (callInfo->functionBase.valuePointer == ZR_NULL ||
            callInfo->functionBase.valuePointer < state->stackBase.valuePointer ||
            callInfo->functionBase.valuePointer >= state->stackTop.valuePointer) {
            continue;
        }

        candidateFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
        if (function_type_layout_function_has_prototype_blob(candidateFunction)) {
            return candidateFunction;
        }
    }

    return ZR_NULL;
}

static const SZrFunction *function_type_layout_entry_function(SZrState *state, const SZrFunction *function) {
    const SZrFunction *entryFunction = function;
    const SZrFunction *prototypeContextFunction;
    const SZrFunction *stackEntryFunction;

    if (entryFunction == ZR_NULL) {
        return ZR_NULL;
    }

    while (entryFunction->ownerFunction != ZR_NULL) {
        entryFunction = entryFunction->ownerFunction;
    }

    if (function_type_layout_function_has_prototype_blob(entryFunction)) {
        return entryFunction;
    }

    prototypeContextFunction = function->prototypeContextFunction;
    if (function_type_layout_function_has_prototype_blob(prototypeContextFunction)) {
        return prototypeContextFunction;
    }

    stackEntryFunction = function_type_layout_entry_function_from_call_stack(state);
    return stackEntryFunction != ZR_NULL ? stackEntryFunction : entryFunction;
}

static TZrBool function_type_layout_read_prototype_count(const SZrFunction *function, TZrUInt32 *outCount) {
    if (function == ZR_NULL || outCount == ZR_NULL ||
        function->prototypeData == ZR_NULL ||
        function->prototypeDataLength < sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }

    memcpy(outCount, function->prototypeData, sizeof(*outCount));
    return ZR_TRUE;
}

static TZrBool function_type_layout_find_prototype_record(const SZrFunction *function,
                                                          TZrUInt32 targetIndex,
                                                          SZrFunctionPrototypeRecord *outRecord,
                                                          TZrUInt32 *outTotalMemberCapacity) {
    const TZrByte *data;
    TZrUInt32 encodedPrototypeCount;
    TZrSize cursor;
    TZrUInt32 totalMemberCapacity = 0u;
    TZrBool found = ZR_FALSE;
    SZrFunctionPrototypeRecord foundRecord = {0};

    if (outRecord != ZR_NULL) {
        memset(outRecord, 0, sizeof(*outRecord));
    }
    if (outTotalMemberCapacity != ZR_NULL) {
        *outTotalMemberCapacity = 0u;
    }
    if (function == ZR_NULL || function->prototypeData == ZR_NULL ||
        !function_type_layout_read_prototype_count(function, &encodedPrototypeCount) ||
        encodedPrototypeCount < function->prototypeCount ||
        targetIndex >= function->prototypeCount) {
        return ZR_FALSE;
    }

    data = function->prototypeData;
    cursor = sizeof(TZrUInt32);
    for (TZrUInt32 index = 0u; index < function->prototypeCount; index++) {
        const SZrCompiledPrototypeInfo *prototype;
        const SZrCompiledMemberInfo *members;
        TZrSize inheritsBytes;
        TZrSize decoratorsBytes;
        TZrSize membersBytes;
        TZrSize afterPrototype;
        TZrSize afterInherits;
        TZrSize membersOffset;
        TZrSize nextCursor;

        if (!function_type_layout_checked_add_size(cursor, sizeof(SZrCompiledPrototypeInfo), &afterPrototype) ||
            afterPrototype > function->prototypeDataLength) {
            return ZR_FALSE;
        }

        prototype = (const SZrCompiledPrototypeInfo *)(data + cursor);
        if (!function_type_layout_checked_mul_size(prototype->inheritsCount, sizeof(TZrUInt32), &inheritsBytes) ||
            !function_type_layout_checked_mul_size(prototype->decoratorsCount, sizeof(TZrUInt32), &decoratorsBytes) ||
            !function_type_layout_checked_mul_size(prototype->membersCount,
                                                   sizeof(SZrCompiledMemberInfo),
                                                   &membersBytes) ||
            !function_type_layout_checked_add_size(afterPrototype, inheritsBytes, &afterInherits) ||
            !function_type_layout_checked_add_size(afterInherits, decoratorsBytes, &membersOffset) ||
            !function_type_layout_checked_add_size(membersOffset, membersBytes, &nextCursor) ||
            nextCursor > function->prototypeDataLength ||
            totalMemberCapacity > UINT32_MAX - prototype->membersCount) {
            return ZR_FALSE;
        }

        members = (const SZrCompiledMemberInfo *)(data + membersOffset);
        totalMemberCapacity += prototype->membersCount;
        if (index == targetIndex) {
            foundRecord.prototype = prototype;
            foundRecord.members = members;
            found = ZR_TRUE;
        }
        cursor = nextCursor;
    }

    if (!found) {
        return ZR_FALSE;
    }

    if (outRecord != ZR_NULL) {
        *outRecord = foundRecord;
    }
    if (outTotalMemberCapacity != ZR_NULL) {
        *outTotalMemberCapacity = totalMemberCapacity;
    }
    return ZR_TRUE;
}

static TZrBool function_type_layout_member_is_field(const SZrCompiledMemberInfo *member) {
    return (TZrBool)(member != ZR_NULL &&
                     (member->memberType == ZR_AST_CONSTANT_STRUCT_FIELD ||
                      member->memberType == ZR_AST_CONSTANT_CLASS_FIELD));
}

static TZrBool function_type_layout_member_requires_value_lifecycle(const SZrCompiledMemberInfo *member) {
    return (TZrBool)(function_type_layout_member_is_field(member) &&
                     (member->isUsingManaged != 0u ||
                      member->ownershipQualifier != 0u ||
                      member->callsClose != 0u ||
                      member->callsDestructor != 0u));
}

static TZrBool function_type_layout_member_field_is_within_prototype(const SZrCompiledPrototypeInfo *prototype,
                                                                     const SZrCompiledMemberInfo *member) {
    TZrUInt32 fieldEnd;

    return (TZrBool)(prototype != ZR_NULL &&
                     member != ZR_NULL &&
                     function_type_layout_checked_add_u32(member->fieldOffset, member->fieldSize, &fieldEnd) &&
                     fieldEnd <= prototype->layoutByteSize);
}

static TZrBool function_type_layout_member_value_field_is_safe(const SZrCompiledPrototypeInfo *prototype,
                                                               const SZrCompiledMemberInfo *member) {
    TZrUInt32 fieldEnd;
    TZrUInt32 valueEnd;

    if (prototype == ZR_NULL || member == ZR_NULL ||
        member->fieldSize < sizeof(SZrTypeValue) ||
        !function_type_layout_checked_add_u32(member->fieldOffset, member->fieldSize, &fieldEnd) ||
        !function_type_layout_checked_add_u32(member->fieldOffset, (TZrUInt32)sizeof(SZrTypeValue), &valueEnd) ||
        fieldEnd > prototype->layoutByteSize ||
        valueEnd > prototype->layoutByteSize) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool function_type_layout_native_text_equals(const TZrChar *left, const TZrChar *right) {
    return (TZrBool)(left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0);
}

static TZrBool function_type_layout_primitive_pod_type_size(const TZrChar *typeNameText, TZrUInt32 *outSize) {
    if (outSize != ZR_NULL) {
        *outSize = 0u;
    }
    if (typeNameText == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function_type_layout_native_text_equals(typeNameText, "i8") ||
        function_type_layout_native_text_equals(typeNameText, "int8")) {
        *outSize = (TZrUInt32)sizeof(TZrInt8);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "i16") ||
        function_type_layout_native_text_equals(typeNameText, "int16")) {
        *outSize = (TZrUInt32)sizeof(TZrInt16);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "i32") ||
        function_type_layout_native_text_equals(typeNameText, "int32")) {
        *outSize = (TZrUInt32)sizeof(TZrInt32);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "int") ||
        function_type_layout_native_text_equals(typeNameText, "i64") ||
        function_type_layout_native_text_equals(typeNameText, "int64")) {
        *outSize = (TZrUInt32)sizeof(TZrInt64);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "u8") ||
        function_type_layout_native_text_equals(typeNameText, "uint8")) {
        *outSize = (TZrUInt32)sizeof(TZrUInt8);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "u16") ||
        function_type_layout_native_text_equals(typeNameText, "uint16")) {
        *outSize = (TZrUInt32)sizeof(TZrUInt16);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "u32") ||
        function_type_layout_native_text_equals(typeNameText, "uint32")) {
        *outSize = (TZrUInt32)sizeof(TZrUInt32);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "uint") ||
        function_type_layout_native_text_equals(typeNameText, "u64") ||
        function_type_layout_native_text_equals(typeNameText, "uint64")) {
        *outSize = (TZrUInt32)sizeof(TZrUInt64);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "float") ||
        function_type_layout_native_text_equals(typeNameText, "f32")) {
        *outSize = (TZrUInt32)sizeof(TZrFloat32);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "double") ||
        function_type_layout_native_text_equals(typeNameText, "f64") ||
        function_type_layout_native_text_equals(typeNameText, "f")) {
        *outSize = (TZrUInt32)sizeof(TZrDouble);
        return ZR_TRUE;
    }
    if (function_type_layout_native_text_equals(typeNameText, "bool")) {
        *outSize = (TZrUInt32)sizeof(TZrBool);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool function_type_layout_primitive_pod_value_type(const TZrChar *typeNameText,
                                                             EZrValueType *outValueType,
                                                             TZrUInt32 *outSize) {
    TZrUInt32 podSize;
    EZrValueType valueType = ZR_VALUE_TYPE_UNKNOWN;

    if (outValueType != ZR_NULL) {
        *outValueType = ZR_VALUE_TYPE_UNKNOWN;
    }
    if (outSize != ZR_NULL) {
        *outSize = 0u;
    }
    if (!function_type_layout_primitive_pod_type_size(typeNameText, &podSize)) {
        return ZR_FALSE;
    }

    if (function_type_layout_native_text_equals(typeNameText, "i8") ||
        function_type_layout_native_text_equals(typeNameText, "int8")) {
        valueType = ZR_VALUE_TYPE_INT8;
    } else if (function_type_layout_native_text_equals(typeNameText, "i16") ||
               function_type_layout_native_text_equals(typeNameText, "int16")) {
        valueType = ZR_VALUE_TYPE_INT16;
    } else if (function_type_layout_native_text_equals(typeNameText, "i32") ||
               function_type_layout_native_text_equals(typeNameText, "int32")) {
        valueType = ZR_VALUE_TYPE_INT32;
    } else if (function_type_layout_native_text_equals(typeNameText, "int") ||
               function_type_layout_native_text_equals(typeNameText, "i64") ||
               function_type_layout_native_text_equals(typeNameText, "int64")) {
        valueType = ZR_VALUE_TYPE_INT64;
    } else if (function_type_layout_native_text_equals(typeNameText, "u8") ||
               function_type_layout_native_text_equals(typeNameText, "uint8")) {
        valueType = ZR_VALUE_TYPE_UINT8;
    } else if (function_type_layout_native_text_equals(typeNameText, "u16") ||
               function_type_layout_native_text_equals(typeNameText, "uint16")) {
        valueType = ZR_VALUE_TYPE_UINT16;
    } else if (function_type_layout_native_text_equals(typeNameText, "u32") ||
               function_type_layout_native_text_equals(typeNameText, "uint32")) {
        valueType = ZR_VALUE_TYPE_UINT32;
    } else if (function_type_layout_native_text_equals(typeNameText, "uint") ||
               function_type_layout_native_text_equals(typeNameText, "u64") ||
               function_type_layout_native_text_equals(typeNameText, "uint64")) {
        valueType = ZR_VALUE_TYPE_UINT64;
    } else if (function_type_layout_native_text_equals(typeNameText, "float") ||
               function_type_layout_native_text_equals(typeNameText, "f32")) {
        valueType = ZR_VALUE_TYPE_FLOAT;
    } else if (function_type_layout_native_text_equals(typeNameText, "double") ||
               function_type_layout_native_text_equals(typeNameText, "f64") ||
               function_type_layout_native_text_equals(typeNameText, "f")) {
        valueType = ZR_VALUE_TYPE_DOUBLE;
    } else if (function_type_layout_native_text_equals(typeNameText, "bool")) {
        valueType = ZR_VALUE_TYPE_BOOL;
    } else {
        return ZR_FALSE;
    }

    if (outValueType != ZR_NULL) {
        *outValueType = valueType;
    }
    if (outSize != ZR_NULL) {
        *outSize = podSize;
    }
    return ZR_TRUE;
}

static TZrBool function_type_layout_reference_value_type_name(const TZrChar *typeNameText) {
    if (typeNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(function_type_layout_native_text_equals(typeNameText, "string") ||
                     function_type_layout_native_text_equals(typeNameText, "str") ||
                     function_type_layout_native_text_equals(typeNameText, "String") ||
                     function_type_layout_native_text_equals(typeNameText, "zr.builtin.String") ||
                     function_type_layout_native_text_equals(typeNameText, "object") ||
                     function_type_layout_native_text_equals(typeNameText, "obj") ||
                     function_type_layout_native_text_equals(typeNameText, "Object") ||
                     function_type_layout_native_text_equals(typeNameText, "zr.builtin.Object") ||
                     function_type_layout_native_text_equals(typeNameText, "array") ||
                     function_type_layout_native_text_equals(typeNameText, "Array") ||
                     function_type_layout_native_text_equals(typeNameText, "function") ||
                     function_type_layout_native_text_equals(typeNameText, "Function") ||
                     function_type_layout_native_text_equals(typeNameText, "closure") ||
                     function_type_layout_native_text_equals(typeNameText, "Closure") ||
                     function_type_layout_native_text_equals(typeNameText, "buffer") ||
                     function_type_layout_native_text_equals(typeNameText, "Buffer") ||
                     function_type_layout_native_text_equals(typeNameText, "thread") ||
                     function_type_layout_native_text_equals(typeNameText, "Thread") ||
                     function_type_layout_native_text_equals(typeNameText, "Module") ||
                     function_type_layout_native_text_equals(typeNameText, "zr.builtin.Module") ||
                     function_type_layout_native_text_equals(typeNameText, "TypeInfo") ||
                     function_type_layout_native_text_equals(typeNameText, "zr.builtin.TypeInfo"));
}

static SZrString *function_type_layout_constant_string(SZrState *state,
                                                       const SZrFunction *function,
                                                       TZrUInt32 constantIndex) {
    const SZrTypeValue *constant;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_NULL;
    }

    constant = &function->constantValueList[constantIndex];
    if (constant->type != ZR_VALUE_TYPE_STRING || constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, constant->value.object);
}

static SZrObject *function_type_layout_constant_object(SZrState *state,
                                                       const SZrFunction *function,
                                                       TZrUInt32 constantIndex) {
    const SZrTypeValue *constant;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_NULL;
    }

    constant = &function->constantValueList[constantIndex];
    if ((constant->type != ZR_VALUE_TYPE_OBJECT && constant->type != ZR_VALUE_TYPE_ARRAY) ||
        constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, constant->value.object);
}

static const SZrTypeValue *function_type_layout_object_member_value(SZrState *state,
                                                                    SZrObject *object,
                                                                    const TZrChar *name) {
    SZrString *memberName;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    memberName = ZrCore_String_CreateFromNative(state, (TZrNativeString)name);
    if (memberName == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static TZrBool function_type_layout_object_int_field(SZrState *state,
                                                     SZrObject *object,
                                                     const TZrChar *name,
                                                     TZrUInt32 *outValue) {
    const SZrTypeValue *value;

    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    value = function_type_layout_object_member_value(state, object, name);
    if (value == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }

    *outValue = ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)
                        ? (TZrUInt32)value->value.nativeObject.nativeInt64
                        : (TZrUInt32)value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

static TZrUInt32 function_type_layout_optional_object_int_field(SZrState *state,
                                                                SZrObject *object,
                                                                const TZrChar *name) {
    TZrUInt32 value = 0u;

    (void)function_type_layout_object_int_field(state, object, name, &value);
    return value;
}

static SZrObject *function_type_layout_object_array_field(SZrState *state,
                                                          SZrObject *object,
                                                          const TZrChar *name) {
    const SZrTypeValue *value = function_type_layout_object_member_value(state, object, name);

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_ARRAY || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrObject *function_type_layout_array_object_at(SZrState *state, SZrObject *arrayObject, TZrUInt32 index) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || arrayObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    value = ZrCore_Object_GetValue(state, arrayObject, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static TZrBool function_type_layout_prototype_is_inline_layout(const SZrCompiledPrototypeInfo *prototype) {
    return (TZrBool)(prototype != ZR_NULL &&
                     (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ||
                      prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_UNION) &&
                     prototype->layoutByteSize > 0u &&
                     prototype->layoutByteAlign > 0u);
}

static TZrBool function_type_layout_find_local_inline_prototype_index(SZrState *state,
                                                                      const SZrFunction *function,
                                                                      SZrString *typeName,
                                                                      TZrUInt32 *outPrototypeIndex) {
    if (outPrototypeIndex != ZR_NULL) {
        *outPrototypeIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }
    if (state == ZR_NULL || function == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->prototypeCount; index++) {
        SZrFunctionPrototypeRecord record;
        SZrString *prototypeName;

        if (!function_type_layout_find_prototype_record(function, index, &record, ZR_NULL) ||
            record.prototype == ZR_NULL ||
            !function_type_layout_prototype_is_inline_layout(record.prototype)) {
            continue;
        }

        prototypeName = function_type_layout_constant_string(state, function, record.prototype->nameStringIndex);
        if (prototypeName != ZR_NULL && ZrCore_String_Equal(prototypeName, typeName)) {
            if (outPrototypeIndex != ZR_NULL) {
                *outPrototypeIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_type_layout_resolve_non_lifecycle_field(
        SZrState *state,
        SZrFunction *function,
        const SZrCompiledPrototypeInfo *prototype,
        const SZrCompiledMemberInfo *member,
        SZrFunctionTypeLayoutResolvedField *outField) {
    SZrString *fieldTypeName;
    const TZrChar *fieldTypeNameText;
    TZrUInt32 nestedPrototypeIndex;
    const SZrTypeLayout *nestedLayout;
    TZrUInt32 podSize;

    if (outField != ZR_NULL) {
        outField->kind = ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_SKIP;
        outField->nestedTypeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        outField->nestedLayout = ZR_NULL;
    }
    if (state == ZR_NULL || function == ZR_NULL || prototype == ZR_NULL || member == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!function_type_layout_member_is_field(member)) {
        return ZR_TRUE;
    }
    if (member->fieldTypeNameStringIndex == 0u) {
        return ZR_FALSE;
    }

    fieldTypeName = function_type_layout_constant_string(state, function, member->fieldTypeNameStringIndex);
    if (fieldTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!function_type_layout_find_local_inline_prototype_index(state,
                                                                function,
                                                                fieldTypeName,
                                                                &nestedPrototypeIndex)) {
        fieldTypeNameText = ZrCore_String_GetNativeString(fieldTypeName);
        if (function_type_layout_primitive_pod_type_size(fieldTypeNameText, &podSize)) {
            return (TZrBool)(member->fieldSize == podSize &&
                             function_type_layout_member_field_is_within_prototype(prototype, member));
        }
        if (function_type_layout_reference_value_type_name(fieldTypeNameText)) {
            if (member->fieldSize != sizeof(SZrTypeValue) ||
                !function_type_layout_member_value_field_is_safe(prototype, member)) {
                return ZR_FALSE;
            }
            if (outField != ZR_NULL) {
                outField->kind = ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_VALUE;
            }
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    nestedLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, nestedPrototypeIndex, state);
    if (nestedLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outField != ZR_NULL) {
        outField->kind = ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_NESTED;
        outField->nestedTypeLayoutId = nestedPrototypeIndex;
        outField->nestedLayout = nestedLayout;
    }
    return ZR_TRUE;
}

static TZrBool function_type_layout_member_nested_layout_is_safe(const SZrCompiledPrototypeInfo *prototype,
                                                                 const SZrCompiledMemberInfo *member,
                                                                 const SZrTypeLayout *nestedLayout) {
    TZrUInt32 fieldEnd;
    TZrUInt32 nestedEnd;

    if (nestedLayout == ZR_NULL) {
        return ZR_TRUE;
    }

    if (prototype == ZR_NULL || member == ZR_NULL ||
        member->fieldSize < nestedLayout->byteSize ||
        !function_type_layout_checked_add_u32(member->fieldOffset, member->fieldSize, &fieldEnd) ||
        !function_type_layout_checked_add_u32(member->fieldOffset, nestedLayout->byteSize, &nestedEnd) ||
        fieldEnd > prototype->layoutByteSize ||
        nestedEnd > prototype->layoutByteSize ||
        (nestedLayout->fieldCount > 0u && nestedLayout->fields == ZR_NULL)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

void ZrCore_Function_FreePrototypeFrameTypeLayoutCache(struct SZrState *state, SZrFunction *function) {
    SZrGlobalState *global = state != ZR_NULL ? state->global : ZR_NULL;

    if (function == ZR_NULL || global == ZR_NULL) {
        return;
    }

    if (function->prototypeFrameTypeLayouts != ZR_NULL && function->prototypeFrameTypeLayoutLength > 0u) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->prototypeFrameTypeLayouts,
                                      sizeof(SZrTypeLayout) * function->prototypeFrameTypeLayoutLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->prototypeFrameTypeLayoutFields != ZR_NULL && function->prototypeFrameTypeLayoutFieldCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->prototypeFrameTypeLayoutFields,
                                      sizeof(SZrTypeLayoutField) * function->prototypeFrameTypeLayoutFieldCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->prototypeFrameTypeLayoutStates != ZR_NULL && function->prototypeFrameTypeLayoutLength > 0u) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->prototypeFrameTypeLayoutStates,
                                      sizeof(TZrUInt8) * function->prototypeFrameTypeLayoutLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    function->prototypeFrameTypeLayouts = ZR_NULL;
    function->prototypeFrameTypeLayoutFields = ZR_NULL;
    function->prototypeFrameTypeLayoutStates = ZR_NULL;
    function->prototypeFrameTypeLayoutLength = 0u;
    function->prototypeFrameTypeLayoutFieldCount = 0u;
    function->prototypeFrameTypeLayoutFieldCapacity = 0u;
}

static TZrBool function_type_layout_count_union_payload_field_capacity(SZrState *state,
                                                                       SZrFunction *function,
                                                                       TZrUInt32 *outCapacity) {
    TZrUInt32 capacity = 0u;

    if (outCapacity != ZR_NULL) {
        *outCapacity = 0u;
    }
    if (state == ZR_NULL || function == ZR_NULL || outCapacity == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 prototypeIndex = 0u; prototypeIndex < function->prototypeCount; prototypeIndex++) {
        SZrFunctionPrototypeRecord record;

        if (!function_type_layout_find_prototype_record(function, prototypeIndex, &record, ZR_NULL)) {
            return ZR_FALSE;
        }
        if (record.prototype == ZR_NULL ||
            record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION ||
            record.prototype->membersCount == 0u) {
            continue;
        }
        if (record.members == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrUInt32 memberIndex = 0u; memberIndex < record.prototype->membersCount; memberIndex++) {
            const SZrCompiledMemberInfo *member = &record.members[memberIndex];
            SZrObject *metadata;
            SZrObject *payloadFields;
            TZrSize payloadFieldCount;

            if (member->memberType != ZR_AST_CONSTANT_UNION_VARIANT ||
                member->hasDecoratorMetadata == 0u) {
                continue;
            }

            metadata = function_type_layout_constant_object(state,
                                                            function,
                                                            member->decoratorMetadataConstantIndex);
            payloadFields = function_type_layout_object_array_field(state, metadata, "payloadFields");
            if (payloadFields == ZR_NULL) {
                continue;
            }

            payloadFieldCount = payloadFields->nodeMap.elementCount;
            if (payloadFieldCount > (TZrSize)(UINT32_MAX - capacity)) {
                return ZR_FALSE;
            }
            capacity += (TZrUInt32)payloadFieldCount;
        }
    }

    *outCapacity = capacity;
    return ZR_TRUE;
}

static TZrBool function_type_layout_ensure_cache(SZrState *state, SZrFunction *function) {
    SZrFunctionPrototypeRecord record;
    TZrUInt32 totalMemberCapacity = 0u;
    TZrUInt32 unionPayloadFieldCapacity = 0u;
    TZrUInt32 totalFieldCapacity = 0u;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || function->prototypeCount == 0u) {
        return ZR_FALSE;
    }

    if (function->prototypeFrameTypeLayouts != ZR_NULL &&
        function->prototypeFrameTypeLayoutStates != ZR_NULL &&
        function->prototypeFrameTypeLayoutLength == function->prototypeCount) {
        return ZR_TRUE;
    }

    ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, function);
    if (!function_type_layout_find_prototype_record(function, 0u, &record, &totalMemberCapacity)) {
        return ZR_FALSE;
    }
    if (!function_type_layout_count_union_payload_field_capacity(state,
                                                                 function,
                                                                 &unionPayloadFieldCapacity)) {
        return ZR_FALSE;
    }

    if (totalMemberCapacity > 0u &&
        !function_type_layout_checked_mul_u32(totalMemberCapacity,
                                              function->prototypeCount,
                                              &totalFieldCapacity)) {
        return ZR_FALSE;
    }
    if (unionPayloadFieldCapacity > 0u &&
        !function_type_layout_checked_add_u32(totalFieldCapacity,
                                              unionPayloadFieldCapacity,
                                              &totalFieldCapacity)) {
        return ZR_FALSE;
    }

    function->prototypeFrameTypeLayouts = (SZrTypeLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeLayout) * function->prototypeCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->prototypeFrameTypeLayoutStates = (TZrUInt8 *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrUInt8) * function->prototypeCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (totalFieldCapacity > 0u) {
        function->prototypeFrameTypeLayoutFields = (SZrTypeLayoutField *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrTypeLayoutField) * totalFieldCapacity,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (function->prototypeFrameTypeLayouts == ZR_NULL ||
        function->prototypeFrameTypeLayoutStates == ZR_NULL ||
        (totalFieldCapacity > 0u && function->prototypeFrameTypeLayoutFields == ZR_NULL)) {
        ZrCore_Function_FreePrototypeFrameTypeLayoutCache(state, function);
        return ZR_FALSE;
    }

    memset(function->prototypeFrameTypeLayouts, 0, sizeof(SZrTypeLayout) * function->prototypeCount);
    memset(function->prototypeFrameTypeLayoutStates,
           ZR_FUNCTION_TYPE_LAYOUT_CACHE_UNKNOWN,
           sizeof(TZrUInt8) * function->prototypeCount);
    if (function->prototypeFrameTypeLayoutFields != ZR_NULL) {
        memset(function->prototypeFrameTypeLayoutFields,
               0,
               sizeof(SZrTypeLayoutField) * totalFieldCapacity);
    }
    function->prototypeFrameTypeLayoutLength = function->prototypeCount;
    function->prototypeFrameTypeLayoutFieldCount = 0u;
    function->prototypeFrameTypeLayoutFieldCapacity = totalFieldCapacity;
    return ZR_TRUE;
}

static TZrBool function_type_layout_union_payload_value_field_is_safe(const SZrCompiledPrototypeInfo *prototype,
                                                                      TZrUInt32 byteOffset,
                                                                      TZrUInt32 byteSize) {
    TZrUInt32 fieldEnd;
    TZrUInt32 valueEnd;

    return (TZrBool)(prototype != ZR_NULL &&
                     byteSize >= (TZrUInt32)sizeof(SZrTypeValue) &&
                     function_type_layout_checked_add_u32(byteOffset, byteSize, &fieldEnd) &&
                     function_type_layout_checked_add_u32(byteOffset,
                                                          (TZrUInt32)sizeof(SZrTypeValue),
                                                          &valueEnd) &&
                     fieldEnd <= prototype->layoutByteSize &&
                     valueEnd <= prototype->layoutByteSize);
}

static TZrBool function_type_layout_union_payload_field_layout(SZrState *state,
                                                               const SZrCompiledPrototypeInfo *prototype,
                                                               SZrObject *fieldMetadata,
                                                               TZrUInt32 activeTag,
                                                               SZrTypeLayoutField *outField,
                                                               TZrBool *outIsManaged) {
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 ownershipQualifier;

    if (outIsManaged != ZR_NULL) {
        *outIsManaged = ZR_FALSE;
    }
    if (outField != ZR_NULL) {
        memset(outField, 0, sizeof(*outField));
        outField->typeLayoutIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }
    if (state == ZR_NULL ||
        prototype == ZR_NULL ||
        fieldMetadata == ZR_NULL ||
        !function_type_layout_object_int_field(state, fieldMetadata, "byteOffset", &byteOffset) ||
        !function_type_layout_object_int_field(state, fieldMetadata, "byteSize", &byteSize)) {
        return ZR_FALSE;
    }

    if (byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
        return ZR_TRUE;
    }
    if (!function_type_layout_union_payload_value_field_is_safe(prototype, byteOffset, byteSize)) {
        return ZR_FALSE;
    }

    ownershipQualifier = function_type_layout_optional_object_int_field(state, fieldMetadata, "ownershipQualifier");
    if (outIsManaged != ZR_NULL) {
        *outIsManaged = ZR_TRUE;
    }
    if (outField != ZR_NULL) {
        outField->byteOffset = byteOffset;
        outField->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        outField->typeLayoutIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        outField->flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                          ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
        if (ownershipQualifier != 0u) {
            outField->flags |= ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
        }
        outField->activeTag = activeTag;
    }
    return ZR_TRUE;
}

static SZrObject *function_type_layout_union_variant_payload_fields(SZrState *state,
                                                                    SZrFunction *function,
                                                                    const SZrCompiledMemberInfo *member) {
    SZrObject *metadata;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        member == ZR_NULL ||
        member->memberType != ZR_AST_CONSTANT_UNION_VARIANT ||
        member->hasDecoratorMetadata == 0u) {
        return ZR_NULL;
    }

    metadata = function_type_layout_constant_object(state, function, member->decoratorMetadataConstantIndex);
    return function_type_layout_object_array_field(state, metadata, "payloadFields");
}

static TZrBool function_type_layout_build_union_managed_fields(SZrState *state,
                                                               SZrFunction *function,
                                                               const SZrCompiledPrototypeInfo *prototype,
                                                               const SZrCompiledMemberInfo *members,
                                                               SZrTypeLayoutField **outFields,
                                                               TZrUInt32 *outFieldCount) {
    TZrUInt32 managedFieldCount = 0u;
    SZrTypeLayoutField *fields;
    TZrUInt32 fieldCursor = 0u;

    if (outFields != ZR_NULL) {
        *outFields = ZR_NULL;
    }
    if (outFieldCount != ZR_NULL) {
        *outFieldCount = 0u;
    }
    if (state == ZR_NULL ||
        function == ZR_NULL ||
        prototype == ZR_NULL ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION ||
        (prototype->membersCount > 0u && members == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 memberIndex = 0u; memberIndex < prototype->membersCount; memberIndex++) {
        const SZrCompiledMemberInfo *member = &members[memberIndex];
        SZrObject *payloadFields = function_type_layout_union_variant_payload_fields(state, function, member);

        if (payloadFields == ZR_NULL) {
            continue;
        }
        for (TZrUInt32 fieldIndex = 0u; fieldIndex < (TZrUInt32)payloadFields->nodeMap.elementCount; fieldIndex++) {
            SZrObject *fieldMetadata = function_type_layout_array_object_at(state, payloadFields, fieldIndex);
            TZrBool isManaged = ZR_FALSE;

            if (!function_type_layout_union_payload_field_layout(state,
                                                                 prototype,
                                                                 fieldMetadata,
                                                                 member->fieldOffset,
                                                                 ZR_NULL,
                                                                 &isManaged)) {
                return ZR_FALSE;
            }
            if (isManaged &&
                !function_type_layout_checked_add_u32(managedFieldCount, 1u, &managedFieldCount)) {
                return ZR_FALSE;
            }
        }
    }

    if (managedFieldCount == 0u) {
        return ZR_TRUE;
    }
    if (function->prototypeFrameTypeLayoutFields == ZR_NULL ||
        function->prototypeFrameTypeLayoutFieldCount > function->prototypeFrameTypeLayoutFieldCapacity ||
        managedFieldCount > function->prototypeFrameTypeLayoutFieldCapacity - function->prototypeFrameTypeLayoutFieldCount) {
        return ZR_FALSE;
    }

    fields = function->prototypeFrameTypeLayoutFields + function->prototypeFrameTypeLayoutFieldCount;
    for (TZrUInt32 memberIndex = 0u; memberIndex < prototype->membersCount; memberIndex++) {
        const SZrCompiledMemberInfo *member = &members[memberIndex];
        SZrObject *payloadFields = function_type_layout_union_variant_payload_fields(state, function, member);

        if (payloadFields == ZR_NULL) {
            continue;
        }
        for (TZrUInt32 fieldIndex = 0u; fieldIndex < (TZrUInt32)payloadFields->nodeMap.elementCount; fieldIndex++) {
            SZrObject *fieldMetadata = function_type_layout_array_object_at(state, payloadFields, fieldIndex);
            TZrBool isManaged = ZR_FALSE;
            SZrTypeLayoutField field;

            if (!function_type_layout_union_payload_field_layout(state,
                                                                 prototype,
                                                                 fieldMetadata,
                                                                 member->fieldOffset,
                                                                 &field,
                                                                 &isManaged)) {
                return ZR_FALSE;
            }
            if (!isManaged) {
                continue;
            }

            fields[fieldCursor++] = field;
        }
    }

    function->prototypeFrameTypeLayoutFieldCount += managedFieldCount;
    if (outFields != ZR_NULL) {
        *outFields = fields;
    }
    if (outFieldCount != ZR_NULL) {
        *outFieldCount = managedFieldCount;
    }
    return ZR_TRUE;
}

static TZrBool function_type_layout_build_managed_fields(SZrState *state,
                                                         SZrFunction *function,
                                                         const SZrCompiledPrototypeInfo *prototype,
                                                         const SZrCompiledMemberInfo *members,
                                                         SZrTypeLayoutField **outFields,
                                                         TZrUInt32 *outFieldCount) {
    TZrUInt32 managedFieldCount = 0u;
    SZrTypeLayoutField *fields;
    TZrUInt32 fieldCursor;

    if (outFields != ZR_NULL) {
        *outFields = ZR_NULL;
    }
    if (outFieldCount != ZR_NULL) {
        *outFieldCount = 0u;
    }
    if (state == ZR_NULL || function == ZR_NULL || prototype == ZR_NULL ||
        (prototype->membersCount > 0u && members == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_UNION) {
        return function_type_layout_build_union_managed_fields(state,
                                                               function,
                                                               prototype,
                                                               members,
                                                               outFields,
                                                               outFieldCount);
    }

    for (TZrUInt32 index = 0u; index < prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &members[index];

        if (!function_type_layout_member_requires_value_lifecycle(member)) {
            SZrFunctionTypeLayoutResolvedField resolvedField;

            if (!function_type_layout_resolve_non_lifecycle_field(state,
                                                                  function,
                                                                  prototype,
                                                                  member,
                                                                  &resolvedField)) {
                return ZR_FALSE;
            }
            if (resolvedField.kind == ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_SKIP) {
                continue;
            }
            if (resolvedField.kind == ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_VALUE) {
                if (!function_type_layout_checked_add_u32(managedFieldCount, 1u, &managedFieldCount)) {
                    return ZR_FALSE;
                }
                continue;
            }
            if (resolvedField.nestedLayout == ZR_NULL) {
                continue;
            }
            if (!function_type_layout_member_nested_layout_is_safe(prototype, member, resolvedField.nestedLayout) ||
                !function_type_layout_checked_add_u32(managedFieldCount,
                                                      resolvedField.nestedLayout->fieldCount > 0u
                                                              ? resolvedField.nestedLayout->fieldCount
                                                              : 1u,
                                                      &managedFieldCount)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (!function_type_layout_member_value_field_is_safe(prototype, member)) {
            return ZR_FALSE;
        }
        if (!function_type_layout_checked_add_u32(managedFieldCount, 1u, &managedFieldCount)) {
            return ZR_FALSE;
        }
    }

    if (managedFieldCount == 0u) {
        return ZR_TRUE;
    }

    if (function->prototypeFrameTypeLayoutFields == ZR_NULL ||
        function->prototypeFrameTypeLayoutFieldCount > function->prototypeFrameTypeLayoutFieldCapacity ||
        managedFieldCount > function->prototypeFrameTypeLayoutFieldCapacity - function->prototypeFrameTypeLayoutFieldCount) {
        return ZR_FALSE;
    }

    fields = function->prototypeFrameTypeLayoutFields + function->prototypeFrameTypeLayoutFieldCount;
    fieldCursor = 0u;
    for (TZrUInt32 index = 0u; index < prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &members[index];

        if (!function_type_layout_member_requires_value_lifecycle(member)) {
            SZrFunctionTypeLayoutResolvedField resolvedField;

            if (!function_type_layout_resolve_non_lifecycle_field(state,
                                                                  function,
                                                                  prototype,
                                                                  member,
                                                                  &resolvedField)) {
                return ZR_FALSE;
            }
            if (resolvedField.kind == ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_SKIP) {
                continue;
            }
            if (resolvedField.kind == ZR_FUNCTION_TYPE_LAYOUT_RESOLVED_FIELD_VALUE) {
                fields[fieldCursor].byteOffset = member->fieldOffset;
                fields[fieldCursor].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
                fields[fieldCursor].typeLayoutIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
                fields[fieldCursor].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                            ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
                fieldCursor++;
                continue;
            }
            if (resolvedField.nestedLayout == ZR_NULL) {
                continue;
            }
            if (!function_type_layout_member_nested_layout_is_safe(prototype, member, resolvedField.nestedLayout)) {
                return ZR_FALSE;
            }
            if (resolvedField.nestedLayout->fieldCount == 0u) {
                fields[fieldCursor].byteOffset = member->fieldOffset;
                fields[fieldCursor].byteSize = member->fieldSize;
                fields[fieldCursor].typeLayoutIndex = resolvedField.nestedTypeLayoutId;
                fields[fieldCursor].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
                fieldCursor++;
                continue;
            }
            for (TZrUInt32 nestedIndex = 0u; nestedIndex < resolvedField.nestedLayout->fieldCount; nestedIndex++) {
                const SZrTypeLayoutField *nestedField = &resolvedField.nestedLayout->fields[nestedIndex];

                fields[fieldCursor] = *nestedField;
                if (!function_type_layout_checked_add_u32(member->fieldOffset,
                                                          nestedField->byteOffset,
                                                          &fields[fieldCursor].byteOffset)) {
                    return ZR_FALSE;
                }
                fieldCursor++;
            }
            continue;
        }

        fields[fieldCursor].byteOffset = member->fieldOffset;
        fields[fieldCursor].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        fields[fieldCursor].typeLayoutIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        fields[fieldCursor].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                    ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                    ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
        fieldCursor++;
    }

    function->prototypeFrameTypeLayoutFieldCount += managedFieldCount;
    if (outFields != ZR_NULL) {
        *outFields = fields;
    }
    if (outFieldCount != ZR_NULL) {
        *outFieldCount = managedFieldCount;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Function_ResolvePrototypeFrameFieldLayout(SZrState *state,
                                                         const SZrFunction *function,
                                                         TZrUInt32 typeLayoutId,
                                                         SZrString *fieldName,
                                                         SZrFunctionFrameFieldLayout *outFieldLayout) {
    const SZrFunction *entryFunction = function_type_layout_entry_function(state, function);
    SZrFunctionPrototypeRecord record;

    if (outFieldLayout != ZR_NULL) {
        memset(outFieldLayout, 0, sizeof(*outFieldLayout));
        outFieldLayout->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        outFieldLayout->valueType = ZR_VALUE_TYPE_UNKNOWN;
    }
    if (state == ZR_NULL ||
        entryFunction == ZR_NULL ||
        fieldName == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= entryFunction->prototypeCount ||
        !function_type_layout_find_prototype_record(entryFunction, typeLayoutId, &record, ZR_NULL) ||
        record.prototype == ZR_NULL ||
        (record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
         record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION)) {
        return ZR_FALSE;
    }
    if (record.prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_UNION) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < record.prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &record.members[index];
        SZrString *memberName;
        SZrString *fieldTypeName;
        const TZrChar *fieldTypeNameText;

        if (!function_type_layout_member_is_field(member) || member->nameStringIndex == 0u) {
            continue;
        }

        memberName = function_type_layout_constant_string(state, entryFunction, member->nameStringIndex);
        if (memberName == ZR_NULL || !ZrCore_String_Equal(memberName, fieldName)) {
            continue;
        }

        if (!function_type_layout_member_field_is_within_prototype(record.prototype, member)) {
            return ZR_FALSE;
        }

        if (outFieldLayout != ZR_NULL) {
            outFieldLayout->byteOffset = member->fieldOffset;
            outFieldLayout->byteSize = member->fieldSize;
        }
        if (member->fieldTypeNameStringIndex == 0u) {
            return ZR_TRUE;
        }

        fieldTypeName = function_type_layout_constant_string(state, entryFunction, member->fieldTypeNameStringIndex);
        fieldTypeNameText = fieldTypeName != ZR_NULL ? ZrCore_String_GetNativeString(fieldTypeName) : ZR_NULL;
        if (fieldTypeNameText == ZR_NULL) {
            return ZR_TRUE;
        }

        if (outFieldLayout != ZR_NULL) {
            EZrValueType valueType;
            TZrUInt32 podSize;
            TZrUInt32 nestedPrototypeIndex;

            if (function_type_layout_primitive_pod_value_type(fieldTypeNameText, &valueType, &podSize) &&
                member->fieldSize == podSize) {
                outFieldLayout->valueType = valueType;
                outFieldLayout->isPrimitivePod = ZR_TRUE;
            } else if (fieldTypeName != ZR_NULL &&
                       function_type_layout_find_local_inline_prototype_index(state,
                                                                              entryFunction,
                                                                              fieldTypeName,
                                                                              &nestedPrototypeIndex)) {
                outFieldLayout->typeLayoutId = nestedPrototypeIndex;
            } else if (function_type_layout_reference_value_type_name(fieldTypeNameText) &&
                       function_type_layout_member_value_field_is_safe(record.prototype, member)) {
                outFieldLayout->isValueSlot = ZR_TRUE;
            }
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool ZrCore_Function_VisitPrototypeFrameFieldLayouts(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId,
        FZrFunctionPrototypeFrameFieldVisitor visitor,
        TZrPtr userData) {
    const SZrFunction *entryFunction = function_type_layout_entry_function(state, function);
    SZrFunctionPrototypeRecord record;

    if (state == ZR_NULL ||
        entryFunction == ZR_NULL ||
        visitor == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= entryFunction->prototypeCount ||
        !function_type_layout_find_prototype_record(entryFunction, typeLayoutId, &record, ZR_NULL) ||
        record.prototype == ZR_NULL ||
        record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < record.prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &record.members[index];
        SZrFunctionFrameFieldLayout fieldLayout;
        SZrString *memberName;

        if (!function_type_layout_member_is_field(member) || member->nameStringIndex == 0u) {
            continue;
        }

        memberName = function_type_layout_constant_string(state, entryFunction, member->nameStringIndex);
        if (memberName == ZR_NULL ||
            !ZrCore_Function_ResolvePrototypeFrameFieldLayout(state,
                                                              function,
                                                              typeLayoutId,
                                                              memberName,
                                                              &fieldLayout) ||
            !visitor(state, function, typeLayoutId, memberName, &fieldLayout, userData)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool function_type_layout_union_tag_size(SZrState *state,
                                                   SZrFunction *function,
                                                   const SZrCompiledPrototypeInfo *prototype,
                                                   const SZrCompiledMemberInfo *members,
                                                   TZrUInt32 *outTagSize) {
    if (outTagSize != ZR_NULL) {
        *outTagSize = 0u;
    }
    if (state == ZR_NULL ||
        function == ZR_NULL ||
        prototype == ZR_NULL ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION ||
        outTagSize == ZR_NULL ||
        (prototype->membersCount > 0u && members == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &members[index];
        SZrObject *metadata;
        TZrUInt32 tagSize;

        if (member->memberType != ZR_AST_CONSTANT_UNION_VARIANT ||
            member->hasDecoratorMetadata == 0u) {
            continue;
        }

        metadata = function_type_layout_constant_object(state, function, member->decoratorMetadataConstantIndex);
        if (metadata == ZR_NULL) {
            continue;
        }
        if (!function_type_layout_object_int_field(state, metadata, "tagSize", &tagSize)) {
            return ZR_FALSE;
        }
        if (tagSize == 0u || tagSize > prototype->layoutByteSize) {
            return ZR_FALSE;
        }

        *outTagSize = tagSize;
        return ZR_TRUE;
    }

    *outTagSize = prototype->layoutByteSize > 0u ? 1u : 0u;
    return (TZrBool)(*outTagSize != 0u);
}

const SZrTypeLayout *ZrCore_Function_ResolvePrototypeFrameTypeLayout(const SZrFunction *function,
                                                                     TZrUInt32 typeLayoutId,
                                                                     TZrPtr userData) {
    SZrState *state = (SZrState *)userData;
    const SZrFunction *entryFunctionConst = function_type_layout_entry_function(state, function);
    SZrFunction *entryFunction = (SZrFunction *)entryFunctionConst;
    SZrFunctionPrototypeRecord record;
    SZrTypeLayoutField *fields = ZR_NULL;
    TZrUInt32 fieldCount = 0u;
    TZrUInt32 unionTagSize = 0u;
    TZrUInt8 stateFlag;

    if (entryFunction == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= entryFunction->prototypeCount) {
        return ZR_NULL;
    }

    if (!function_type_layout_ensure_cache(state, entryFunction) ||
        entryFunction->prototypeFrameTypeLayouts == ZR_NULL ||
        entryFunction->prototypeFrameTypeLayoutStates == ZR_NULL ||
        typeLayoutId >= entryFunction->prototypeFrameTypeLayoutLength) {
        return ZR_NULL;
    }

    stateFlag = entryFunction->prototypeFrameTypeLayoutStates[typeLayoutId];
    if (stateFlag == ZR_FUNCTION_TYPE_LAYOUT_CACHE_READY) {
        return &entryFunction->prototypeFrameTypeLayouts[typeLayoutId];
    }
    if (stateFlag == ZR_FUNCTION_TYPE_LAYOUT_CACHE_FAILED ||
        stateFlag == ZR_FUNCTION_TYPE_LAYOUT_CACHE_RESOLVING) {
        return ZR_NULL;
    }

    entryFunction->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_FUNCTION_TYPE_LAYOUT_CACHE_RESOLVING;
    if (!function_type_layout_find_prototype_record(entryFunction, typeLayoutId, &record, ZR_NULL) ||
        record.prototype == ZR_NULL ||
        !function_type_layout_prototype_is_inline_layout(record.prototype) ||
        record.prototype->layoutByteSize == 0u ||
        record.prototype->layoutByteAlign == 0u) {
        entryFunction->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_FUNCTION_TYPE_LAYOUT_CACHE_FAILED;
        return ZR_NULL;
    }

    if (record.prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_UNION) {
        if (!function_type_layout_build_union_managed_fields(state,
                                                             entryFunction,
                                                             record.prototype,
                                                             record.members,
                                                             &fields,
                                                             &fieldCount)) {
            entryFunction->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_FUNCTION_TYPE_LAYOUT_CACHE_FAILED;
            return ZR_NULL;
        }

        if (!function_type_layout_union_tag_size(state,
                                                 entryFunction,
                                                 record.prototype,
                                                 record.members,
                                                 &unionTagSize)) {
            entryFunction->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_FUNCTION_TYPE_LAYOUT_CACHE_FAILED;
            return ZR_NULL;
        }

        ZrCore_TypeLayout_InitUnion(&entryFunction->prototypeFrameTypeLayouts[typeLayoutId],
                                    record.prototype->layoutByteSize,
                                    record.prototype->layoutByteAlign,
                                    0u,
                                    unionTagSize,
                                    fieldCount > 0u ? ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY
                                                    : ZR_TYPE_LAYOUT_COPY_KIND_POD,
                                    fieldCount > 0u ? ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP
                                                    : ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                    fields,
                                    fieldCount);
    } else {
        if (!function_type_layout_build_managed_fields(state,
                                                       entryFunction,
                                                       record.prototype,
                                                       record.members,
                                                       &fields,
                                                       &fieldCount)) {
            entryFunction->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_FUNCTION_TYPE_LAYOUT_CACHE_FAILED;
            return ZR_NULL;
        }

        ZrCore_TypeLayout_InitStruct(&entryFunction->prototypeFrameTypeLayouts[typeLayoutId],
                                     record.prototype->layoutByteSize,
                                     record.prototype->layoutByteAlign,
                                     fieldCount > 0u ? ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY
                                                     : ZR_TYPE_LAYOUT_COPY_KIND_POD,
                                     fieldCount > 0u ? ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP
                                                     : ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                     fields,
                                     fieldCount);
    }
    entryFunction->prototypeFrameTypeLayoutStates[typeLayoutId] = ZR_FUNCTION_TYPE_LAYOUT_CACHE_READY;
    return &entryFunction->prototypeFrameTypeLayouts[typeLayoutId];
}

SZrObjectPrototype *ZrCore_Function_ResolvePrototypeFrameStructPrototype(SZrState *state,
                                                                         const SZrFunction *function,
                                                                         TZrUInt32 typeLayoutId) {
    const SZrFunction *entryFunctionConst = function_type_layout_entry_function(state, function);
    SZrFunction *entryFunction = (SZrFunction *)entryFunctionConst;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL ||
        entryFunction == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= entryFunction->prototypeCount) {
        return ZR_NULL;
    }

    if (entryFunction->prototypeInstances == ZR_NULL ||
        entryFunction->prototypeInstancesLength <= typeLayoutId ||
        entryFunction->prototypeInstances[typeLayoutId] == ZR_NULL) {
        ZrCore_Module_CreatePrototypesFromData(state, ZR_NULL, entryFunction);
    }

    if (entryFunction->prototypeInstances == ZR_NULL ||
        entryFunction->prototypeInstancesLength <= typeLayoutId) {
        return ZR_NULL;
    }

    prototype = entryFunction->prototypeInstances[typeLayoutId];
    if (prototype == ZR_NULL || prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        return ZR_NULL;
    }

    return prototype;
}
