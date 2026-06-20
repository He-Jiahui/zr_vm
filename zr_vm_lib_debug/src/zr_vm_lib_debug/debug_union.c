#include "debug_internal.h"

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

#define ZR_DEBUG_UNION_TYPE_FIELD "__zr_unionType"
#define ZR_DEBUG_UNION_VARIANT_FIELD "__zr_unionVariant"
#define ZR_DEBUG_UNION_PAYLOAD_PREFIX "__zr_unionPayload"

typedef struct ZrDebugUnionPrototypeRecord {
    const SZrCompiledPrototypeInfo *prototype;
    const SZrCompiledMemberInfo *members;
} ZrDebugUnionPrototypeRecord;

static TZrBool zr_debug_union_checked_add_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || left > ZR_MAX_SIZE - right) {
        return ZR_FALSE;
    }

    *out = left + right;
    return ZR_TRUE;
}

static TZrBool zr_debug_union_checked_mul_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || (right != 0u && left > ZR_MAX_SIZE / right)) {
        return ZR_FALSE;
    }

    *out = left * right;
    return ZR_TRUE;
}

static const SZrFunction *zr_debug_union_entry_function(const SZrFunction *function) {
    const SZrFunction *entryFunction = function;

    if (entryFunction == ZR_NULL) {
        return ZR_NULL;
    }

    while (entryFunction->ownerFunction != ZR_NULL) {
        entryFunction = entryFunction->ownerFunction;
    }

    if (entryFunction->prototypeData != ZR_NULL &&
        entryFunction->prototypeDataLength >= sizeof(TZrUInt32) &&
        entryFunction->prototypeCount > 0u) {
        return entryFunction;
    }

    return function->prototypeContextFunction != ZR_NULL ? function->prototypeContextFunction : entryFunction;
}

static TZrBool zr_debug_union_find_prototype_record(
        const SZrFunction *function,
        TZrUInt32 targetIndex,
        ZrDebugUnionPrototypeRecord *outRecord) {
    const TZrByte *data;
    TZrUInt32 encodedPrototypeCount;
    TZrSize cursor;

    if (outRecord != ZR_NULL) {
        memset(outRecord, 0, sizeof(*outRecord));
    }
    if (function == ZR_NULL ||
        function->prototypeData == ZR_NULL ||
        function->prototypeDataLength < sizeof(TZrUInt32) ||
        targetIndex >= function->prototypeCount) {
        return ZR_FALSE;
    }

    memcpy(&encodedPrototypeCount, function->prototypeData, sizeof(encodedPrototypeCount));
    if (encodedPrototypeCount < function->prototypeCount) {
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

        if (!zr_debug_union_checked_add_size(cursor, sizeof(SZrCompiledPrototypeInfo), &afterPrototype) ||
            afterPrototype > function->prototypeDataLength) {
            return ZR_FALSE;
        }

        prototype = (const SZrCompiledPrototypeInfo *)(data + cursor);
        if (!zr_debug_union_checked_mul_size(prototype->inheritsCount, sizeof(TZrUInt32), &inheritsBytes) ||
            !zr_debug_union_checked_mul_size(prototype->decoratorsCount, sizeof(TZrUInt32), &decoratorsBytes) ||
            !zr_debug_union_checked_mul_size(prototype->membersCount,
                                            sizeof(SZrCompiledMemberInfo),
                                            &membersBytes) ||
            !zr_debug_union_checked_add_size(afterPrototype, inheritsBytes, &afterInherits) ||
            !zr_debug_union_checked_add_size(afterInherits, decoratorsBytes, &membersOffset) ||
            !zr_debug_union_checked_add_size(membersOffset, membersBytes, &nextCursor) ||
            nextCursor > function->prototypeDataLength) {
            return ZR_FALSE;
        }

        members = (const SZrCompiledMemberInfo *)(data + membersOffset);
        if (index == targetIndex) {
            if (outRecord != ZR_NULL) {
                outRecord->prototype = prototype;
                outRecord->members = members;
            }
            return ZR_TRUE;
        }
        cursor = nextCursor;
    }

    return ZR_FALSE;
}

static SZrString *zr_debug_union_function_string_constant(const SZrFunction *function,
                                                          TZrUInt32 constantIndex) {
    const SZrTypeValue *constant;

    if (function == ZR_NULL ||
        function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_NULL;
    }

    constant = &function->constantValueList[constantIndex];
    if (constant->type != ZR_VALUE_TYPE_STRING || constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(ZR_NULL, constant->value.object);
}

static TZrBool zr_debug_union_type_name_matches(const TZrChar *recordName, const TZrChar *runtimeName) {
    const TZrChar *genericStart;
    TZrSize baseLength;

    if (recordName == ZR_NULL || runtimeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(recordName, runtimeName) == 0) {
        return ZR_TRUE;
    }

    genericStart = strchr(runtimeName, '<');
    if (genericStart == ZR_NULL) {
        return ZR_FALSE;
    }

    baseLength = (TZrSize)(genericStart - runtimeName);
    while (baseLength > 0u &&
           (runtimeName[baseLength - 1u] == ' ' ||
            runtimeName[baseLength - 1u] == '\t' ||
            runtimeName[baseLength - 1u] == '\r' ||
            runtimeName[baseLength - 1u] == '\n')) {
        baseLength--;
    }

    return (TZrBool)(baseLength > 0u &&
                     strlen(recordName) == baseLength &&
                     strncmp(recordName, runtimeName, baseLength) == 0);
}

static TZrBool zr_debug_union_find_prototype_record_by_name(
        const SZrFunction *entryFunction,
        const TZrChar *typeName,
        ZrDebugUnionPrototypeRecord *outRecord) {
    TZrUInt32 index;

    if (outRecord != ZR_NULL) {
        memset(outRecord, 0, sizeof(*outRecord));
    }
    if (entryFunction == ZR_NULL || typeName == ZR_NULL || outRecord == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0u; index < entryFunction->prototypeCount; index++) {
        ZrDebugUnionPrototypeRecord record;
        SZrString *prototypeName;
        const TZrChar *prototypeNameText;

        if (!zr_debug_union_find_prototype_record(entryFunction, index, &record) ||
            record.prototype == ZR_NULL ||
            record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION) {
            continue;
        }

        prototypeName = zr_debug_union_function_string_constant(entryFunction,
                                                                record.prototype->nameStringIndex);
        prototypeNameText = prototypeName != ZR_NULL ? ZrCore_String_GetNativeString(prototypeName) : ZR_NULL;
        if (zr_debug_union_type_name_matches(prototypeNameText, typeName)) {
            *outRecord = record;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrCompiledMemberInfo *zr_debug_union_find_variant_member(
        const SZrFunction *entryFunction,
        const ZrDebugUnionPrototypeRecord *record,
        const TZrChar *variantName) {
    TZrUInt32 index;

    if (entryFunction == ZR_NULL ||
        record == ZR_NULL ||
        record->prototype == ZR_NULL ||
        record->members == ZR_NULL ||
        variantName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < record->prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &record->members[index];
        SZrString *memberName;
        const TZrChar *memberNameText;

        if (member->memberType != ZR_AST_CONSTANT_UNION_VARIANT || member->nameStringIndex == 0u) {
            continue;
        }

        memberName = zr_debug_union_function_string_constant(entryFunction, member->nameStringIndex);
        memberNameText = memberName != ZR_NULL ? ZrCore_String_GetNativeString(memberName) : ZR_NULL;
        if (memberNameText != ZR_NULL && strcmp(memberNameText, variantName) == 0) {
            return member;
        }
    }

    return ZR_NULL;
}

static const SZrTypeValue *zr_debug_union_object_get_field(SZrState *state,
                                                           SZrObject *object,
                                                           const TZrChar *fieldName) {
    SZrTypeValue key;
    SZrString *fieldString;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static TZrBool zr_debug_union_object_set_field(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrString *fieldString;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
    return (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE);
}

static TZrBool zr_debug_union_make_string_value(SZrState *state,
                                                const TZrChar *text,
                                                SZrTypeValue *outValue) {
    SZrString *stringObject;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (state == ZR_NULL || text == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stringObject = ZrCore_String_CreateFromNative(state, (TZrNativeString)text);
    if (stringObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    outValue->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static SZrString *zr_debug_union_object_string_field(SZrState *state,
                                                     SZrObject *object,
                                                     const TZrChar *name) {
    const SZrTypeValue *value = zr_debug_union_object_get_field(state, object, name);

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, value->value.object);
}

static TZrBool zr_debug_union_object_int_field(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *name,
                                               TZrUInt32 *outValue) {
    const SZrTypeValue *value = zr_debug_union_object_get_field(state, object, name);

    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (value == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type) || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    *outValue = ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)
                        ? (TZrUInt32)value->value.nativeObject.nativeInt64
                        : (TZrUInt32)value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

static SZrObject *zr_debug_union_object_array_field(SZrState *state,
                                                    SZrObject *object,
                                                    const TZrChar *name) {
    const SZrTypeValue *value = zr_debug_union_object_get_field(state, object, name);

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_ARRAY || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrObject *zr_debug_union_array_object_at(SZrState *state,
                                                 SZrObject *arrayObject,
                                                 TZrUInt32 index) {
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

static SZrObject *zr_debug_union_variant_metadata(ZrDebugAgent *agent,
                                                  const SZrFunction *entryFunction,
                                                  const SZrCompiledMemberInfo *variantMember) {
    const SZrTypeValue *metadataValue;

    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        entryFunction == ZR_NULL ||
        variantMember == ZR_NULL ||
        !variantMember->hasDecoratorMetadata ||
        variantMember->decoratorMetadataConstantIndex >= entryFunction->constantValueLength) {
        return ZR_NULL;
    }

    metadataValue = &entryFunction->constantValueList[variantMember->decoratorMetadataConstantIndex];
    if (metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(agent->state, metadataValue->value.object);
}

static TZrBool zr_debug_union_parse_payload_index(const TZrChar *text,
                                                  const TZrChar *prefix,
                                                  TZrUInt32 *outIndex) {
    TZrSize prefixLength;
    TZrUInt32 value = 0u;

    if (outIndex != ZR_NULL) {
        *outIndex = 0u;
    }
    if (text == ZR_NULL || prefix == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    prefixLength = strlen(prefix);
    if (strncmp(text, prefix, prefixLength) != 0 || text[prefixLength] == '\0') {
        return ZR_FALSE;
    }

    for (const TZrChar *cursor = text + prefixLength; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return ZR_FALSE;
        }
        if (value > (UINT32_MAX - (TZrUInt32)(*cursor - '0')) / 10u) {
            return ZR_FALSE;
        }
        value = value * 10u + (TZrUInt32)(*cursor - '0');
    }

    *outIndex = value;
    return ZR_TRUE;
}

TZrSize zr_debug_count_union_carrier_payload_fields(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;
    TZrSize bucketIndex;
    TZrSize count = 0;

    if (state == ZR_NULL ||
        value == ZR_NULL ||
        value->value.object == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY)) {
        return 0;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL || !object->nodeMap.isValid || object->nodeMap.buckets == ZR_NULL) {
        return 0;
    }

    for (bucketIndex = 0; bucketIndex < object->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            const TZrChar *keyText = pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL
                                             ? ZrCore_String_GetNativeString(ZR_CAST_STRING(state, pair->key.value.object))
                                             : ZR_NULL;
            TZrUInt32 payloadIndex;
            if (zr_debug_union_parse_payload_index(keyText, ZR_DEBUG_UNION_PAYLOAD_PREFIX, &payloadIndex)) {
                count++;
            }
            pair = pair->next;
        }
    }

    return count;
}

TZrBool zr_debug_value_is_union_carrier(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;
    SZrString *variantName;

    if (state == ZR_NULL ||
        value == ZR_NULL ||
        value->value.object == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    variantName = zr_debug_union_object_string_field(state, object, ZR_DEBUG_UNION_VARIANT_FIELD);
    return (TZrBool)(variantName != ZR_NULL);
}

TZrBool zr_debug_union_value_view(ZrDebugAgent *agent,
                                  const SZrTypeValue *value,
                                  ZrDebugUnionView *outView) {
    SZrObject *object;
    SZrString *typeName;
    SZrString *variantName;
    const TZrChar *typeText;
    const TZrChar *variantText;
    const SZrFunction *entryFunction;
    ZrDebugUnionPrototypeRecord record;

    if (outView != ZR_NULL) {
        memset(outView, 0, sizeof(*outView));
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        value == ZR_NULL ||
        value->value.object == ZR_NULL ||
        outView == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(agent->state, value->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = zr_debug_union_object_string_field(agent->state, object, ZR_DEBUG_UNION_TYPE_FIELD);
    variantName = zr_debug_union_object_string_field(agent->state, object, ZR_DEBUG_UNION_VARIANT_FIELD);
    typeText = typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeName) : ZR_NULL;
    variantText = variantName != ZR_NULL ? ZrCore_String_GetNativeString(variantName) : ZR_NULL;
    if (variantText == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_copy_text(outView->type_name, sizeof(outView->type_name), typeText != ZR_NULL ? typeText : "union");
    zr_debug_copy_text(outView->variant_name, sizeof(outView->variant_name), variantText);
    outView->payload_count = (TZrUInt32)zr_debug_count_union_carrier_payload_fields(agent->state, value);

    entryFunction = zr_debug_union_entry_function(agent->entryFunction);
    if (entryFunction != ZR_NULL &&
        typeText != ZR_NULL &&
        zr_debug_union_find_prototype_record_by_name(entryFunction, typeText, &record)) {
        const SZrCompiledMemberInfo *variantMember =
                zr_debug_union_find_variant_member(entryFunction, &record, variantText);
        if (variantMember != ZR_NULL) {
            outView->entry_function = entryFunction;
            outView->prototype = record.prototype;
            outView->members = record.members;
            outView->variant_member = variantMember;
            outView->variant_metadata = zr_debug_union_variant_metadata(agent, entryFunction, variantMember);
            outView->payload_fields = zr_debug_union_object_array_field(agent->state,
                                                                        outView->variant_metadata,
                                                                        "payloadFields");
            outView->payload_count = variantMember->parameterCount;
            if (outView->variant_metadata != ZR_NULL) {
                (void)zr_debug_union_object_int_field(agent->state,
                                                      outView->variant_metadata,
                                                      "tagSize",
                                                      &outView->tag_size);
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_union_load_unsigned_int(const TZrByte *address,
                                                TZrUInt32 byteSize,
                                                TZrUInt64 *outValue) {
    if (address == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrUInt8): {
            TZrUInt8 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt16): {
            TZrUInt16 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt32): {
            TZrUInt32 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt64): {
            TZrUInt64 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static const SZrCompiledMemberInfo *zr_debug_union_find_variant_by_tag(
        const ZrDebugUnionPrototypeRecord *record,
        TZrUInt64 tagValue) {
    TZrUInt32 index;

    if (record == ZR_NULL || record->prototype == ZR_NULL || record->members == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < record->prototype->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &record->members[index];
        if (member->memberType == ZR_AST_CONSTANT_UNION_VARIANT &&
            (TZrUInt64)member->fieldOffset == tagValue) {
            return member;
        }
    }

    return ZR_NULL;
}

TZrBool zr_debug_inline_union_view(ZrDebugAgent *agent,
                                   const SZrFunction *function,
                                   const SZrFunctionFrameSlotLayout *slotLayout,
                                   const SZrStackFramePlace *place,
                                   ZrDebugUnionView *outView) {
    const SZrTypeLayout *typeLayout;
    const SZrFunction *entryFunction;
    ZrDebugUnionPrototypeRecord record;
    TZrUInt64 tagValue;
    const SZrCompiledMemberInfo *variantMember;
    SZrString *typeName;
    SZrString *variantName;

    if (outView != ZR_NULL) {
        memset(outView, 0, sizeof(*outView));
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        function == ZR_NULL ||
        slotLayout == ZR_NULL ||
        place == ZR_NULL ||
        place->address == ZR_NULL ||
        outView == ZR_NULL ||
        slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        slotLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, slotLayout->typeLayoutId, agent->state);
    if (typeLayout == ZR_NULL ||
        typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION ||
        typeLayout->byteSize > place->byteSize ||
        typeLayout->tagOffset > place->byteSize ||
        typeLayout->tagSize == 0u ||
        typeLayout->tagSize > place->byteSize - typeLayout->tagOffset ||
        !zr_debug_union_load_unsigned_int((const TZrByte *)place->address + typeLayout->tagOffset,
                                          typeLayout->tagSize,
                                          &tagValue)) {
        return ZR_FALSE;
    }

    entryFunction = zr_debug_union_entry_function(function);
    if (!zr_debug_union_find_prototype_record(entryFunction, slotLayout->typeLayoutId, &record) ||
        record.prototype == ZR_NULL ||
        record.members == ZR_NULL ||
        record.prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION) {
        return ZR_FALSE;
    }

    variantMember = zr_debug_union_find_variant_by_tag(&record, tagValue);
    if (variantMember == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = zr_debug_union_function_string_constant(entryFunction, record.prototype->nameStringIndex);
    variantName = zr_debug_union_function_string_constant(entryFunction, variantMember->nameStringIndex);
    zr_debug_copy_text(outView->type_name,
                       sizeof(outView->type_name),
                       typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeName) : "union");
    zr_debug_copy_text(outView->variant_name,
                       sizeof(outView->variant_name),
                       variantName != ZR_NULL ? ZrCore_String_GetNativeString(variantName) : "variant");
    outView->entry_function = entryFunction;
    outView->prototype = record.prototype;
    outView->members = record.members;
    outView->variant_member = variantMember;
    outView->variant_metadata = zr_debug_union_variant_metadata(agent, entryFunction, variantMember);
    outView->payload_fields = zr_debug_union_object_array_field(agent->state,
                                                                outView->variant_metadata,
                                                                "payloadFields");
    outView->payload_count = variantMember->parameterCount;
    outView->tag_size = typeLayout->tagSize;
    return ZR_TRUE;
}

TZrBool zr_debug_inline_union_slot_view(ZrDebugAgent *agent,
                                        const SZrFunction *function,
                                        const SZrCallInfo *callInfo,
                                        TZrUInt32 stackSlot,
                                        SZrStackFramePlace *outPlace,
                                        ZrDebugUnionView *outView) {
    TZrStackValuePointer frameBase;
    const SZrFunctionFrameSlotLayout *slotLayout;
    SZrStackFramePlace place;

    if (outPlace != ZR_NULL) {
        memset(outPlace, 0, sizeof(*outPlace));
    }
    if (outView != ZR_NULL) {
        memset(outView, 0, sizeof(*outView));
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        function == ZR_NULL ||
        callInfo == ZR_NULL ||
        callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    frameBase = callInfo->functionBase.valuePointer + 1;
    slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
    if (slotLayout == ZR_NULL ||
        slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        !ZrCore_Function_MakeFrameSlotPlace(agent->state, function, frameBase, stackSlot, &place) ||
        !zr_debug_inline_union_view(agent, function, slotLayout, &place, outView)) {
        return ZR_FALSE;
    }

    if (outPlace != ZR_NULL) {
        *outPlace = place;
    }
    return ZR_TRUE;
}

static TZrBool zr_debug_union_payload_field_layout(SZrState *state,
                                                   SZrObject *fieldMetadata,
                                                   SZrFunctionFrameFieldLayout *outLayout) {
    SZrString *typeName;
    const TZrChar *typeNameText;
    EZrValueType valueType = ZR_VALUE_TYPE_UNKNOWN;
    TZrUInt32 expectedSize = 0u;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;

    if (outLayout != ZR_NULL) {
        memset(outLayout, 0, sizeof(*outLayout));
    }
    if (state == ZR_NULL ||
        fieldMetadata == ZR_NULL ||
        outLayout == ZR_NULL ||
        !zr_debug_union_object_int_field(state, fieldMetadata, "byteOffset", &byteOffset) ||
        !zr_debug_union_object_int_field(state, fieldMetadata, "byteSize", &byteSize) ||
        !zr_debug_union_object_int_field(state, fieldMetadata, "byteAlign", &byteAlign)) {
        return ZR_FALSE;
    }

    ZR_UNUSED_PARAMETER(byteAlign);
    outLayout->byteOffset = byteOffset;
    outLayout->byteSize = byteSize;
    outLayout->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    outLayout->valueType = ZR_VALUE_TYPE_UNKNOWN;

    typeName = zr_debug_union_object_string_field(state, fieldMetadata, "type");
    typeNameText = typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeName) : ZR_NULL;
    if (typeNameText != ZR_NULL) {
        if (strcmp(typeNameText, "i8") == 0 || strcmp(typeNameText, "int8") == 0) {
            valueType = ZR_VALUE_TYPE_INT8;
            expectedSize = (TZrUInt32)sizeof(TZrInt8);
        } else if (strcmp(typeNameText, "i16") == 0 || strcmp(typeNameText, "int16") == 0) {
            valueType = ZR_VALUE_TYPE_INT16;
            expectedSize = (TZrUInt32)sizeof(TZrInt16);
        } else if (strcmp(typeNameText, "i32") == 0 || strcmp(typeNameText, "int32") == 0) {
            valueType = ZR_VALUE_TYPE_INT32;
            expectedSize = (TZrUInt32)sizeof(TZrInt32);
        } else if (strcmp(typeNameText, "int") == 0 || strcmp(typeNameText, "i64") == 0 ||
                   strcmp(typeNameText, "int64") == 0) {
            valueType = ZR_VALUE_TYPE_INT64;
            expectedSize = (TZrUInt32)sizeof(TZrInt64);
        } else if (strcmp(typeNameText, "u8") == 0 || strcmp(typeNameText, "uint8") == 0) {
            valueType = ZR_VALUE_TYPE_UINT8;
            expectedSize = (TZrUInt32)sizeof(TZrUInt8);
        } else if (strcmp(typeNameText, "u16") == 0 || strcmp(typeNameText, "uint16") == 0) {
            valueType = ZR_VALUE_TYPE_UINT16;
            expectedSize = (TZrUInt32)sizeof(TZrUInt16);
        } else if (strcmp(typeNameText, "u32") == 0 || strcmp(typeNameText, "uint32") == 0) {
            valueType = ZR_VALUE_TYPE_UINT32;
            expectedSize = (TZrUInt32)sizeof(TZrUInt32);
        } else if (strcmp(typeNameText, "uint") == 0 || strcmp(typeNameText, "u64") == 0 ||
                   strcmp(typeNameText, "uint64") == 0) {
            valueType = ZR_VALUE_TYPE_UINT64;
            expectedSize = (TZrUInt32)sizeof(TZrUInt64);
        } else if (strcmp(typeNameText, "float") == 0 || strcmp(typeNameText, "f32") == 0) {
            valueType = ZR_VALUE_TYPE_FLOAT;
            expectedSize = (TZrUInt32)sizeof(TZrFloat32);
        } else if (strcmp(typeNameText, "double") == 0 || strcmp(typeNameText, "f64") == 0 ||
                   strcmp(typeNameText, "f") == 0) {
            valueType = ZR_VALUE_TYPE_DOUBLE;
            expectedSize = (TZrUInt32)sizeof(TZrDouble);
        } else if (strcmp(typeNameText, "bool") == 0) {
            valueType = ZR_VALUE_TYPE_BOOL;
            expectedSize = (TZrUInt32)sizeof(TZrBool);
        }
    }

    if (valueType != ZR_VALUE_TYPE_UNKNOWN && expectedSize == byteSize) {
        outLayout->valueType = valueType;
        outLayout->isPrimitivePod = ZR_TRUE;
        return ZR_TRUE;
    }

    if (byteSize >= sizeof(SZrTypeValue)) {
        outLayout->isValueSlot = ZR_TRUE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool zr_debug_union_load_signed_int(const TZrByte *address,
                                              TZrUInt32 byteSize,
                                              TZrInt64 *outValue) {
    if (address == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrInt8): {
            TZrInt8 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt16): {
            TZrInt16 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt32): {
            TZrInt32 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt64): {
            TZrInt64 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_debug_union_load_field(SZrState *state,
                                         const SZrFunctionFrameFieldLayout *fieldLayout,
                                         const TZrByte *fieldAddress,
                                         SZrTypeValue *result) {
    if (fieldLayout == ZR_NULL || fieldAddress == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (fieldLayout->isPrimitivePod) {
        switch (fieldLayout->valueType) {
            ZR_VALUE_CASES_SIGNED_INT {
                TZrInt64 value;
                if (!zr_debug_union_load_signed_int(fieldAddress, fieldLayout->byteSize, &value)) {
                    return ZR_FALSE;
                }
                ZrCore_Value_InitAsInt(state, result, value);
                return ZR_TRUE;
            }
            ZR_VALUE_CASES_UNSIGNED_INT {
                TZrUInt64 value;
                if (!zr_debug_union_load_unsigned_int(fieldAddress, fieldLayout->byteSize, &value)) {
                    return ZR_FALSE;
                }
                ZrCore_Value_InitAsUInt(state, result, value);
                return ZR_TRUE;
            }
            case ZR_VALUE_TYPE_BOOL: {
                TZrBool value;
                if (fieldLayout->byteSize != sizeof(value)) {
                    return ZR_FALSE;
                }
                memcpy(&value, fieldAddress, sizeof(value));
                ZrCore_Value_InitAsBool(state, result, value);
                return ZR_TRUE;
            }
            case ZR_VALUE_TYPE_FLOAT: {
                TZrFloat32 value;
                if (fieldLayout->byteSize != sizeof(value)) {
                    return ZR_FALSE;
                }
                memcpy(&value, fieldAddress, sizeof(value));
                ZrCore_Value_InitAsFloat(state, result, (TZrFloat64)value);
                return ZR_TRUE;
            }
            case ZR_VALUE_TYPE_DOUBLE: {
                TZrDouble value;
                if (fieldLayout->byteSize != sizeof(value)) {
                    return ZR_FALSE;
                }
                memcpy(&value, fieldAddress, sizeof(value));
                ZrCore_Value_InitAsFloat(state, result, value);
                return ZR_TRUE;
            }
            default:
                return ZR_FALSE;
        }
    }

    if (fieldLayout->isValueSlot && fieldLayout->byteSize >= sizeof(SZrTypeValue)) {
        ZrCore_Value_Copy(state, result, (const SZrTypeValue *)fieldAddress);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool zr_debug_union_payload_display_name(SZrState *state,
                                            const ZrDebugUnionView *view,
                                            TZrUInt32 payloadIndex,
                                            TZrChar *buffer,
                                            TZrSize bufferSize) {
    SZrObject *fieldMetadata;
    SZrString *fieldName;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (state != ZR_NULL && view != ZR_NULL && view->payload_fields != ZR_NULL) {
        fieldMetadata = zr_debug_union_array_object_at(state, view->payload_fields, payloadIndex);
        fieldName = zr_debug_union_object_string_field(state, fieldMetadata, "name");
        if (fieldName != ZR_NULL && ZrCore_String_GetNativeString(fieldName) != ZR_NULL) {
            zr_debug_copy_text(buffer, bufferSize, ZrCore_String_GetNativeString(fieldName));
            return ZR_TRUE;
        }
    }

    snprintf(buffer, bufferSize, "payload%u", (unsigned)payloadIndex);
    buffer[bufferSize - 1u] = '\0';
    return ZR_TRUE;
}

static TZrBool zr_debug_union_payload_internal_name(SZrState *state,
                                                    const ZrDebugUnionView *view,
                                                    TZrUInt32 payloadIndex,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize) {
    SZrObject *fieldMetadata;
    SZrString *internalName;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (state != ZR_NULL && view != ZR_NULL && view->payload_fields != ZR_NULL) {
        fieldMetadata = zr_debug_union_array_object_at(state, view->payload_fields, payloadIndex);
        internalName = zr_debug_union_object_string_field(state, fieldMetadata, "internalName");
        if (internalName != ZR_NULL && ZrCore_String_GetNativeString(internalName) != ZR_NULL) {
            zr_debug_copy_text(buffer, bufferSize, ZrCore_String_GetNativeString(internalName));
            return ZR_TRUE;
        }
    }

    snprintf(buffer, bufferSize, ZR_DEBUG_UNION_PAYLOAD_PREFIX "%u", (unsigned)payloadIndex);
    buffer[bufferSize - 1u] = '\0';
    return ZR_TRUE;
}

TZrBool zr_debug_union_payload_value_from_carrier(ZrDebugAgent *agent,
                                                  const SZrTypeValue *value,
                                                  const ZrDebugUnionView *view,
                                                  TZrUInt32 payloadIndex,
                                                  SZrTypeValue *outValue) {
    SZrObject *object;
    TZrChar internalName[ZR_DEBUG_NAME_CAPACITY];
    const SZrTypeValue *payloadValue;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        value == ZR_NULL ||
        value->value.object == ZR_NULL ||
        outValue == ZR_NULL ||
        (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        !zr_debug_union_payload_internal_name(agent->state,
                                              view,
                                              payloadIndex,
                                              internalName,
                                              sizeof(internalName))) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(agent->state, value->value.object);
    payloadValue = zr_debug_union_object_get_field(agent->state, object, internalName);
    if (payloadValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
        return ZR_TRUE;
    }

    *outValue = *payloadValue;
    return ZR_TRUE;
}

TZrBool zr_debug_union_payload_value_from_inline(ZrDebugAgent *agent,
                                                 const SZrFunction *function,
                                                 const SZrStackFramePlace *place,
                                                 const ZrDebugUnionView *view,
                                                 TZrUInt32 payloadIndex,
                                                 SZrTypeValue *outValue) {
    SZrObject *fieldMetadata;
    SZrFunctionFrameFieldLayout fieldLayout;
    const TZrByte *fieldAddress;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        function == ZR_NULL ||
        place == ZR_NULL ||
        place->address == ZR_NULL ||
        view == ZR_NULL ||
        outValue == ZR_NULL ||
        payloadIndex >= view->payload_count) {
        return ZR_FALSE;
    }

    fieldMetadata = zr_debug_union_array_object_at(agent->state, view->payload_fields, payloadIndex);
    if (!zr_debug_union_payload_field_layout(agent->state, fieldMetadata, &fieldLayout) ||
        fieldLayout.byteOffset > place->byteSize ||
        fieldLayout.byteSize > place->byteSize - fieldLayout.byteOffset) {
        return ZR_FALSE;
    }

    ZR_UNUSED_PARAMETER(function);
    fieldAddress = (const TZrByte *)place->address + fieldLayout.byteOffset;
    return zr_debug_union_load_field(agent->state, &fieldLayout, fieldAddress, outValue);
}

TZrBool zr_debug_materialize_inline_union_slot(ZrDebugAgent *agent,
                                               const SZrFunction *function,
                                               const SZrCallInfo *callInfo,
                                               TZrUInt32 stackSlot,
                                               SZrTypeValue *outValue) {
    SZrStackFramePlace place;
    ZrDebugUnionView view;
    SZrObject *object;
    SZrTypeValue typeValue;
    SZrTypeValue variantValue;
    TZrUInt32 index;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        outValue == ZR_NULL ||
        !zr_debug_inline_union_slot_view(agent, function, callInfo, stackSlot, &place, &view)) {
        return ZR_FALSE;
    }

    object = ZrCore_Object_New(agent->state, ZR_NULL);
    if (object == ZR_NULL ||
        !ZrCore_GarbageCollector_IgnoreObject(agent->state, ZR_CAST_RAW_OBJECT_AS_SUPER(object)) ||
        !zr_debug_union_make_string_value(agent->state, view.type_name, &typeValue) ||
        !zr_debug_union_make_string_value(agent->state, view.variant_name, &variantValue) ||
        !zr_debug_union_object_set_field(agent->state, object, ZR_DEBUG_UNION_TYPE_FIELD, &typeValue) ||
        !zr_debug_union_object_set_field(agent->state, object, ZR_DEBUG_UNION_VARIANT_FIELD, &variantValue)) {
        return ZR_FALSE;
    }

    for (index = 0u; index < view.payload_count; index++) {
        TZrChar internalName[ZR_DEBUG_NAME_CAPACITY];
        SZrTypeValue payloadValue;

        ZrCore_Value_ResetAsNull(&payloadValue);
        if (!zr_debug_union_payload_internal_name(agent->state, &view, index, internalName, sizeof(internalName)) ||
            !zr_debug_union_payload_value_from_inline(agent, function, &place, &view, index, &payloadValue) ||
            !zr_debug_union_object_set_field(agent->state, object, internalName, &payloadValue)) {
            return ZR_FALSE;
        }
    }

    ZrCore_Value_InitAsRawObject(agent->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

TZrBool zr_debug_union_safe_get_member_value(ZrDebugAgent *agent,
                                             const SZrTypeValue *receiver,
                                             const TZrChar *memberName,
                                             SZrTypeValue *outValue) {
    ZrDebugUnionView view;
    TZrUInt32 payloadIndex;
    TZrUInt32 index;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        receiver == ZR_NULL ||
        memberName == ZR_NULL ||
        outValue == ZR_NULL ||
        !zr_debug_union_value_view(agent, receiver, &view)) {
        return ZR_FALSE;
    }

    if (strcmp(memberName, "variant") == 0) {
        return zr_debug_union_make_string_value(agent->state, view.variant_name, outValue);
    }
    if (strcmp(memberName, "type") == 0) {
        return zr_debug_union_make_string_value(agent->state, view.type_name, outValue);
    }
    if (zr_debug_union_parse_payload_index(memberName, "payload", &payloadIndex) &&
        payloadIndex < view.payload_count) {
        return zr_debug_union_payload_value_from_carrier(agent, receiver, &view, payloadIndex, outValue);
    }
    if (zr_debug_union_parse_payload_index(memberName, ZR_DEBUG_UNION_PAYLOAD_PREFIX, &payloadIndex) &&
        payloadIndex < view.payload_count) {
        return zr_debug_union_payload_value_from_carrier(agent, receiver, &view, payloadIndex, outValue);
    }

    for (index = 0u; index < view.payload_count; index++) {
        TZrChar displayName[ZR_DEBUG_NAME_CAPACITY];
        if (zr_debug_union_payload_display_name(agent->state, &view, index, displayName, sizeof(displayName)) &&
            strcmp(displayName, memberName) == 0) {
            return zr_debug_union_payload_value_from_carrier(agent, receiver, &view, index, outValue);
        }
    }

    return ZR_FALSE;
}

TZrBool zr_debug_format_union_value_text(ZrDebugAgent *agent,
                                         const SZrTypeValue *value,
                                         TZrChar *buffer,
                                         TZrSize bufferSize) {
    ZrDebugUnionView view;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (buffer == ZR_NULL || bufferSize == 0 ||
        !zr_debug_union_value_view(agent, value, &view)) {
        return ZR_FALSE;
    }

    zr_debug_format_inline_union_text(&view, buffer, bufferSize);
    return ZR_TRUE;
}

void zr_debug_format_inline_union_text(const ZrDebugUnionView *view,
                                       TZrChar *buffer,
                                       TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    if (view == ZR_NULL) {
        return;
    }

    snprintf(buffer, bufferSize, "<union %s.%s>", view->type_name, view->variant_name);
    buffer[bufferSize - 1u] = '\0';
}
