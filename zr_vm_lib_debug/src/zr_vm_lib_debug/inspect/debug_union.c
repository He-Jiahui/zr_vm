#include "debug_internal.h"

#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

/* 名称必须与 compiler_union.c 生成的 boxed carrier 字段一致。 */
#define ZR_DEBUG_UNION_TYPE_FIELD "__zr_unionType"
#define ZR_DEBUG_UNION_VARIANT_FIELD "__zr_unionVariant"
#define ZR_DEBUG_UNION_PAYLOAD_PREFIX "__zr_unionPayload"

/* 借用入口函数 prototypeData 内的连续记录，不拥有成员表内存。 */
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

/* 嵌套函数的 union 原型记录通常存于入口函数，缺席时沿用显式上下文。 */
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

/* 按编码计数遍历变长原型表；每段先做溢出与长度校验才借出内部指针。 */
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

/* 运行时泛型名可能带实参，而编译原型仅保存基名。 */
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

/* variant 的 payloadFields/tagSize 来自编译期装饰器常量，视图只在函数有效期内借用。 */
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

/** @brief 统计 carrier 自身的内部 payload 字段条目，供子节点形状与无原型展示回退使用。 */
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

/** @brief 以字符串型内部 variant 字段作展示形状判别，不证明对象的编译来源或 canonical 身份。 */
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

/** @brief 将 boxed carrier 和可用编译元数据合成统一展示视图。 */
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
    /* 无对应原型时按实存字段降级；找到变体后改用声明参数数目。 */
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

/** @brief 从栈槽的类型布局读取 tag，并用编译原型定位当前变体。 */
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

    /* 先确认 tag 在帧槽边界内，再读取原始字节，避免把失效布局当作有效 union。 */
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

/** @brief 把当前调用帧的 inline 槽映射为原始地址和 union 展示视图。 */
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

/* 编译装饰器记录的字段类型与字节宽度必须吻合，才能安全解码 primitive。 */
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

    /* TODO: 此处仅凭宽度判定 SZrTypeValue 槽；compiler_union.c 对无所有权的
     * 非泛型字段可写入原始内联布局。需用大于 SZrTypeValue 的 struct payload
     * 检查元数据是否可能把原始字节误读为带 type 标签的值。 */
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

/* 只按布局描述读取 POD 或值槽，避免执行脚本层成员访问。 */
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

/** @brief 将 payload 元数据中的用户字段名映射到调试子节点名。 */
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

/* carrier 读取使用编译器的内部字段名；显示名可由用户声明覆盖。 */
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

/** @brief 直接读取 boxed carrier 的 payload，缺失槽表示可展示的 null。 */
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

/** @brief 按编译元数据中的偏移和长度读取当前栈帧的 payload。 */
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

/** @brief legacy 求值按需把 inline union 物化为 carrier；当前显式 GC 根不会在本路径解除。 */
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

    /* BUG: 允许 legacy 回退的求值可在解析失败表达式左侧物化 inline union；
     * 新建 carrier 的显式 ignored root 未解除，重复成功物化可滞留至 VM/GC 销毁；
     * Ignore 成功后的字段或 payload 普通失败/Throw 也保留该根，且先前字段不回滚。
     * inspect/debug_snapshot.c 的 zr_debug_resolve_identifier_value 可达本函数；
     * 句柄清理仅释放数组；gc_roots.c::ZrCore_GarbageCollector_IgnoreObject 登记后，
     * gc_mark.c::garbage_collector_mark_ignored_roots 每轮标根，临时根lease不撤销显式标志。
     * 本路径无 Unignore；此为静态完整路径证据，未运行重复求值/故障复现。 */
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

/** @brief 只读映射 variant/type/payload 名称，不调用脚本 getter。 */
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

/** @brief 给 boxed union 输出稳定的类型和变体预览文本。 */
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

/**
 * @fn static TZrBool zr_debug_union_checked_add_size(TZrSize left, TZrSize right, TZrSize *out)
 * @unit UNION-002
 *
 * ## Summary
 * 为变长 prototypeData 游标推进计算可表示的偏移，先拒绝溢出再交付结果。
 *
 * ## Parameters
 * @param[in] left 当前字节偏移或已累计长度。
 * @param[in] right 待追加的字节长度。
 * @param[out] out 结果槽；NULL 被拒绝，失败时原值不变。
 *
 * ## Returns
 * @return TRUE 表示和可由 TZrSize 表示且已写 out；FALSE 表示空输出或溢出。
 *
 * ## Constraints and lifecycle
 * 纯数值检查，不验证结果是否落在实际缓冲区内；调用者还须与 prototypeDataLength 比较。
 *
 * ## References
 * - zr_debug_union_find_prototype_record。
 */

/**
 * @fn static TZrBool zr_debug_union_checked_mul_size(TZrSize left, TZrSize right, TZrSize *out)
 * @unit UNION-003
 *
 * ## Summary
 * 为变长 prototypeData 段计算元素数对应的字节长度，避免乘法先溢出后再做边界检查。
 *
 * ## Parameters
 * @param[in] left 段的元素数。
 * @param[in] right 单元素字节宽度，可为零。
 * @param[out] out 结果槽；NULL 被拒绝，失败时原值不变。
 *
 * ## Returns
 * @return TRUE 表示乘积可由 TZrSize 表示且已写 out；FALSE 表示空输出或溢出。
 *
 * ## Constraints and lifecycle
 * 只处理可表示性；不分配内存，也不证明后续数据实际存在。
 *
 * ## References
 * - zr_debug_union_find_prototype_record。
 */

/**
 * @fn static const SZrFunction *zr_debug_union_entry_function(const SZrFunction *function)
 * @unit UNION-004
 *
 * ## Summary
 * 从嵌套函数选择 union 原型记录的所属函数，优先使用有 prototypeData 的最外层 owner，再考虑显式 prototypeContextFunction。
 *
 * ## Parameters
 * @param[in] function 借用当前函数；NULL 直接返回 NULL。
 *
 * ## Returns
 * @return 返回借用的元数据上下文；可能是最外层 owner 或显式上下文，不转移所有权。
 *
 * ## Constraints and lifecycle
 * ownerFunction 链须为有效、无环的函数关系。此选择只做最小表头存在性检查；记录边界仍由后续查找验证。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_inline_union_view。
 * - zr_debug_union_find_prototype_record。
 */

/**
 * @fn static TZrBool zr_debug_union_find_prototype_record( const SZrFunction *function, TZrUInt32 targetIndex, ZrDebugUnionPrototypeRecord *outRecord)
 * @unit UNION-005
 *
 * ## Summary
 * 按运行时原型序号定位变长编译记录，逐段验证长度和算术后借出该记录及其成员起点。
 *
 * ## Parameters
 * @param[in] function 借用持有 prototypeData 的有效函数；空值或缺少表头时失败。
 * @param[in] targetIndex 要求小于 function->prototypeCount 的记录序号。
 * @param[out] outRecord 可为 NULL，仅验证目标可达；非空时先清零，成功后写入借用指针。
 *
 * ## Returns
 * @return TRUE 表示目标记录前的全部遍历段及目标段均在当前数据长度内；FALSE 表示参数、计数或边界不满足。
 *
 * ## Constraints and lifecycle
 * 编码原型数只要求不小于运行时 prototypeCount；找到目标后不验证其后的记录。指针依赖同一函数的原型缓冲区和本机编译结构布局，不延长其生命期。
 *
 * ## References
 * - zr_debug_union_checked_add_size。
 * - zr_debug_union_checked_mul_size。
 * - zr_debug_union_find_prototype_record_by_name。
 * - zr_debug_inline_union_view。
 */

/**
 * @fn static SZrString *zr_debug_union_function_string_constant(const SZrFunction *function, TZrUInt32 constantIndex)
 * @unit UNION-006
 *
 * ## Summary
 * 从入口函数常量表取得原型或变体的字符串名称，过滤空表、越界索引和非字符串值。
 *
 * ## Parameters
 * @param[in] function 借用具有有效常量表的函数；NULL 或缺表时返回 NULL。
 * @param[in] constantIndex 常量表的零基索引，须落在 constantValueLength 内。
 *
 * ## Returns
 * @return 返回借用的 SZrString，或在不满足字符串条件时返回 NULL；不创建或释放字符串。
 *
 * ## Constraints and lifecycle
 * 返回对象仍由函数常量及 VM 的存活关系维持，不能跨越相应常量释放后使用。
 *
 * ## References
 * - zr_debug_union_find_variant_member。
 * - zr_debug_union_find_prototype_record_by_name。
 * - zr_debug_inline_union_view。
 */

/**
 * @fn static TZrBool zr_debug_union_type_name_matches(const TZrChar *recordName, const TZrChar *runtimeName)
 * @unit UNION-007
 *
 * ## Summary
 * 在展示层将运行时带泛型实参的名称关联到编译原型基名；这只是名称匹配，不承担 canonical 类型身份判断。
 *
 * ## Parameters
 * @param[in] recordName NUL 终止的编译原型名，NULL 不匹配。
 * @param[in] runtimeName NUL 终止的运行时名称，NULL 不匹配。
 *
 * ## Returns
 * @return TRUE 表示完全相等，或 runtimeName 第一个 '<' 前去尾空白的非空部分等于 recordName。
 *
 * ## Constraints and lifecycle
 * 不解析或校验泛型实参，也不处理任意等价别名；字符串仅借用到本次调用结束。
 *
 * ## References
 * - zr_debug_union_find_prototype_record_by_name。
 */

/**
 * @fn static TZrBool zr_debug_union_find_prototype_record_by_name( const SZrFunction *entryFunction, const TZrChar *typeName, ZrDebugUnionPrototypeRecord *outRecord)
 * @unit UNION-008
 *
 * ## Summary
 * 在入口函数的 union 原型中按运行时展示类型名查找首个匹配记录，为 carrier 补充可选编译元数据。
 *
 * ## Parameters
 * @param[in] entryFunction 借用已选择的函数元数据上下文。
 * @param[in] typeName NUL 终止的运行时类型名。
 * @param[out] outRecord 必需输出；非空时入口先清零，仅成功时写入借用记录。
 *
 * ## Returns
 * @return TRUE 表示找到 union 类型记录及匹配名称；FALSE 表示缺少参数或没有可用匹配。
 *
 * ## Constraints and lifecycle
 * 单个无效记录或非 union 记录被跳过；匹配遵循展示层的泛型基名规则。返回记录不拥有底层表。
 *
 * ## References
 * - zr_debug_union_find_prototype_record。
 * - zr_debug_union_type_name_matches。
 * - zr_debug_union_value_view。
 */

/**
 * @fn static const SZrCompiledMemberInfo *zr_debug_union_find_variant_member( const SZrFunction *entryFunction, const ZrDebugUnionPrototypeRecord *record, const TZrChar *variantName)
 * @unit UNION-009
 *
 * ## Summary
 * 在已选 union 原型的成员表中按展示变体名定位编译变体记录。
 *
 * ## Parameters
 * @param[in] entryFunction 借用用于解析名称常量的入口函数。
 * @param[in] record 借用已通过边界检查的 prototype/members 视图。
 * @param[in] variantName NUL 终止的目标变体名，NULL 不匹配。
 *
 * ## Returns
 * @return 返回首个类型为 union variant、名称索引非零且名称相等的借用成员；否则 NULL。
 *
 * ## Constraints and lifecycle
 * 本函数依赖记录成员区确实覆盖 membersCount，不重复验证缓冲区长度；指针随函数元数据失效。
 *
 * ## References
 * - zr_debug_union_function_string_constant。
 * - zr_debug_union_value_view。
 */

/**
 * @fn static const SZrTypeValue *zr_debug_union_object_get_field(SZrState *state, SZrObject *object, const TZrChar *fieldName)
 * @unit UNION-010
 *
 * ## Summary
 * 为 union 展示读取普通对象或其原型中的字段，构造 VM 字符串键后直接访问存储，不派发脚本 getter。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，用于字符串键创建和字段读取。
 * @param[in] object 借用有效对象；NULL 被拒绝。
 * @param[in] fieldName 借用 NUL 终止的字段名；NULL 被拒绝。
 *
 * ## Returns
 * @return 返回借用的字段值槽，或缺参、键创建普通失败、字段缺失时返回 NULL。
 *
 * ## Constraints and lifecycle
 * 字符串键创建可能分配并通过 Throw 非局部退出；NULL 不统一表示所有异常。返回槽不能在对象或原型变更、回收后使用。
 *
 * ## References
 * - ZrCore_String_CreateFromNative。
 * - ZrCore_Object_GetValue。
 * - zr_debug_union_payload_value_from_carrier。
 */

/**
 * @fn static TZrBool zr_debug_union_object_set_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, const SZrTypeValue *value)
 * @unit UNION-011
 *
 * ## Summary
 * 将构建中 carrier 的一个内部字段交给 VM 普通对象写入路径，供类型、变体和 payload 顺序发布。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态；写入可分配并改变错误状态。
 * @param[in,out] object 借用待构建 carrier；已经写入的字段不回滚。
 * @param[in] fieldName NUL 终止的内部字段名，调用期间有效。
 * @param[in] value 借用待写值，非 NULL；具体复制/所有权规则沿用对象 setter。
 *
 * ## Returns
 * @return TRUE 表示 setter 正常返回后 threadStatus 为 FINE；普通参数或键创建失败返回 FALSE。
 *
 * ## Constraints and lifecycle
 * ZrCore_Object_SetValue 为 void，写入失败可 Throw，不能当作必定返回 FALSE。后续字段失败不撤销先前已发布字段；调用者负责非局部退出边界。
 *
 * ## References
 * - ZrCore_Object_SetValue。
 * - zr_debug_materialize_inline_union_slot。
 */

/**
 * @fn static TZrBool zr_debug_union_make_string_value(SZrState *state, const TZrChar *text, SZrTypeValue *outValue)
 * @unit UNION-012
 *
 * ## Summary
 * 将 union 展示名称构造成 VM 字符串值，供受限成员访问和 carrier 内部字段写入。
 *
 * ## Parameters
 * @param[in,out] state 借用用于创建字符串的 VM 状态。
 * @param[in] text 非 NULL、NUL 终止的输入文本，本次调用内借用。
 * @param[out] outValue 非 NULL 的目标值槽；入口先重置为 null，不得与仍需读取的输入存储重叠。
 *
 * ## Returns
 * @return TRUE 表示已交付字符串值；普通参数或分配返回 NULL 时 FALSE，输出保持 null。
 *
 * ## Constraints and lifecycle
 * 字符串由 VM 管理，本函数不登记独立持久根；调用者须维持适当存活上下文。分配路径也可能 Throw，入口重置不释放目标原先持有的资源。
 *
 * ## References
 * - ZrCore_String_CreateFromNative。
 * - ZrCore_Value_InitAsRawObject。
 * - zr_debug_union_safe_get_member_value。
 */

/**
 * @fn static SZrString *zr_debug_union_object_string_field(SZrState *state, SZrObject *object, const TZrChar *name)
 * @unit UNION-013
 *
 * ## Summary
 * 读取 union 元数据中的字符串字段，在直接字段访问后按字符串标签筛选结果。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，字段名字符串键创建可能分配。
 * @param[in] object 借用元数据或 carrier 对象；NULL 导致无结果。
 * @param[in] name 借用 NUL 终止的字段名。
 *
 * ## Returns
 * @return 返回字段中的借用字符串；缺字段、非字符串或空对象值时返回 NULL。
 *
 * ## Constraints and lifecycle
 * 不执行脚本 getter，但字段键创建仍可能 Throw；返回对象不由调用者单独释放。
 *
 * ## References
 * - zr_debug_union_object_get_field。
 * - zr_debug_union_value_view。
 * - zr_debug_union_payload_field_layout。
 */

/**
 * @fn static TZrBool zr_debug_union_object_int_field(SZrState *state, SZrObject *object, const TZrChar *name, TZrUInt32 *outValue)
 * @unit UNION-014
 *
 * ## Summary
 * 从装饰器或变体元数据提取整数并转换成 TZrUInt32，为布局与 tagSize 读取提供统一入口。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，字段查找可分配字符串键。
 * @param[in] object 借用元数据对象。
 * @param[in] name NUL 终止的字段名。
 * @param[out] outValue 结果槽；正常字段查找返回后先写零，非空且整数时再写转换值。
 *
 * ## Returns
 * @return TRUE 表示字段具有整数标签且输出存在；FALSE 表示未取得整数或缺少输出。
 *
 * ## Constraints and lifecycle
 * 有符号或宽整数通过 C 转换写入 UInt32，未拒绝负值或超范围截断。字段查找发生在清零前，Throw 可使旧输出尚未被重置；布局调用者另做范围验证。
 *
 * ## References
 * - zr_debug_union_object_get_field。
 * - zr_debug_union_payload_field_layout。
 * - zr_debug_union_value_view。
 */

/**
 * @fn static SZrObject *zr_debug_union_object_array_field(SZrState *state, SZrObject *object, const TZrChar *name)
 * @unit UNION-015
 *
 * ## Summary
 * 读取变体元数据的数组字段，供 payloadFields 的有序元数据访问。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，字段键创建可分配。
 * @param[in] object 借用待查元数据对象。
 * @param[in] name 借用 NUL 终止的字段名。
 *
 * ## Returns
 * @return 仅当字段标签为 ARRAY 且对象非空时返回借用数组对象，否则 NULL。
 *
 * ## Constraints and lifecycle
 * 字段存储和原型查找不调用脚本 getter；仍可能因字符串键分配 Throw，返回值不转移所有权。
 *
 * ## References
 * - zr_debug_union_object_get_field。
 * - zr_debug_union_value_view。
 * - zr_debug_inline_union_view。
 */

/**
 * @fn static SZrObject *zr_debug_union_array_object_at(SZrState *state, SZrObject *arrayObject, TZrUInt32 index)
 * @unit UNION-016
 *
 * ## Summary
 * 按数值索引读取 payloadFields 中的对象型字段元数据，为名称和字节布局解释提供借用对象。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，用于构造整数键和对象读取。
 * @param[in] arrayObject 借用数组或可按整数键读取的对象。
 * @param[in] index 零基索引；本函数不以数组长度预先拒绝，交由对象查找。
 *
 * ## Returns
 * @return 只接受所取值为 OBJECT 且非空，返回借用对象；缺参、缺槽或其它标签返回 NULL。
 *
 * ## Constraints and lifecycle
 * 依赖普通对象读取契约；不把 ARRAY 标签的元素当作字段元数据对象。结果随容器及 VM 生命周期有效。
 *
 * ## References
 * - ZrCore_Object_GetValue。
 * - zr_debug_union_payload_display_name。
 * - zr_debug_union_payload_value_from_inline。
 */

/**
 * @fn static SZrObject *zr_debug_union_variant_metadata(ZrDebugAgent *agent, const SZrFunction *entryFunction, const SZrCompiledMemberInfo *variantMember)
 * @unit UNION-017
 *
 * ## Summary
 * 借出编译变体装饰器常量中的元数据对象，连接原型成员与 payloadFields/tagSize 描述。
 *
 * ## Parameters
 * @param[in] agent 借用提供非空 state 的调试代理。
 * @param[in] entryFunction 借用有效函数常量上下文。
 * @param[in] variantMember 借用该上下文中的变体成员记录。
 *
 * ## Returns
 * @return 成员标明元数据且索引有效、常量标签为 OBJECT 时返回借用对象，否则 NULL。
 *
 * ## Constraints and lifecycle
 * 假设有效函数的 constantValueList 与 constantValueLength 一致；不复制对象或延长函数/VM 生命周期。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_inline_union_view。
 * - ZrDebugUnionView。
 */

/**
 * @fn static TZrBool zr_debug_union_parse_payload_index(const TZrChar *text, const TZrChar *prefix, TZrUInt32 *outIndex)
 * @unit UNION-018
 *
 * ## Summary
 * 将约定前缀后的十进制后缀解析成 payload 序号，为内部键计数和受限成员映射共用。
 *
 * ## Parameters
 * @param[in] text 借用 NUL 终止的待解析文本。
 * @param[in] prefix 借用 NUL 终止的精确前缀。
 * @param[out] outIndex 非 NULL 的结果槽，入口先写零。
 *
 * ## Returns
 * @return TRUE 表示前缀匹配、至少一个数字且整个后缀可表示为 UInt32；FALSE 保持输出零。
 *
 * ## Constraints and lifecycle
 * 不接收符号、空后缀或其它尾字符；不要求索引连续，也不验证 payload 数量上限。
 *
 * ## References
 * - zr_debug_count_union_carrier_payload_fields。
 * - zr_debug_union_safe_get_member_value。
 */

/**
 * @fn TZrSize zr_debug_count_union_carrier_payload_fields(SZrState *state, const SZrTypeValue *value)
 * @unit UNION-019
 *
 * ## Summary
 * 统计 carrier 自身哈希表内采用内部 payload 数字键的条目，为子节点形状和缺编译元数据时的展示提供数量。
 *
 * ## Parameters
 * @param[in] state 借用状态，用于把字符串对象解释成原生文本。
 * @param[in] value 借用 OBJECT/ARRAY 值；空值、无对象或无有效 nodeMap 时计零。
 *
 * ## Returns
 * @return 返回自身哈希条目中前缀和十进制后缀可解析的数量；零不能区分无 payload 与无效输入。
 *
 * ## Constraints and lifecycle
 * 只计自身表，不计原型继承字段；不是最大索引加一，不保证索引连续，也不校验编译来源。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_value_child_shape。
 * - zr_debug_union_parse_payload_index。
 */

/**
 * @fn TZrBool zr_debug_value_is_union_carrier(SZrState *state, const SZrTypeValue *value)
 * @unit UNION-020
 *
 * ## Summary
 * 检查对象能否提供字符串型内部 variant 字段，为 union 形状分支提供窄判别。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，查字段名可能分配字符串键。
 * @param[in] value 借用 OBJECT/ARRAY 值；其它标签或空对象不匹配。
 *
 * ## Returns
 * @return TRUE 只说明内部 variant 字段读取到非空字符串对象；FALSE 表示类型/字段不满足。
 *
 * ## Constraints and lifecycle
 * 不要求 type 字段、原型记录或 canonical 事实，不证明对象由编译器生成。可经原型取得字段，不调用脚本 getter但可能分配或 Throw。
 *
 * ## References
 * - zr_debug_union_object_string_field。
 * - zr_debug_value_child_shape。
 * - zr_debug_evaluate_with_policy。
 */

/**
 * @fn TZrBool zr_debug_union_value_view(ZrDebugAgent *agent, const SZrTypeValue *value, ZrDebugUnionView *outView)
 * @unit UNION-021
 *
 * ## Summary
 * 为 boxed carrier 构造统一 union 展示视图，先使用运行时名称和自身 payload 数，再在名称匹配时补充编译原型。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及 state，元数据字段名读取可能分配。
 * @param[in] value 借用非空 OBJECT/ARRAY carrier 值。
 * @param[out] outView 非 NULL 的独立视图槽，入口清零；名称为固定容量拷贝，元数据指针均借用。
 *
 * ## Returns
 * @return TRUE 表示取得非空 variant 文本并完成展示视图；即使没有匹配编译原型也可 TRUE。
 *
 * ## Constraints and lifecycle
 * 有匹配变体时 payload_count 改为声明参数数；否则保留自身字段计数。名称可截断，视图不是类型身份认证；对象、函数元数据及暂停上下文必须保持有效。字段读取可 Throw。
 *
 * ## References
 * - zr_debug_union_find_prototype_record_by_name。
 * - zr_debug_union_find_variant_member。
 * - zr_debug_expand_union_value_handle。
 */

/**
 * @fn static TZrBool zr_debug_union_load_unsigned_int(const TZrByte *address, TZrUInt32 byteSize, TZrUInt64 *outValue)
 * @unit UNION-022
 *
 * ## Summary
 * 从已经验证可读的原始布局地址按无符号宽度读取整数，避免依赖该地址的对齐方式。
 *
 * ## Parameters
 * @param[in] address 借用至少 byteSize 字节可读的地址，非 NULL。
 * @param[in] byteSize 支持的本机UInt8/16/32/64宽度。
 * @param[out] outValue 非 NULL 输出，失败时保持原值。
 *
 * ## Returns
 * @return TRUE 表示支持该宽度并写入扩展后的 UInt64；FALSE 表示空参数或不支持宽度。
 *
 * ## Constraints and lifecycle
 * memcpy 只规避非对齐加载；不验证地址所属对象、边界或端序，边界须由调用者先检查。
 *
 * ## References
 * - zr_debug_inline_union_view。
 * - zr_debug_union_load_field。
 */

/**
 * @fn static const SZrCompiledMemberInfo *zr_debug_union_find_variant_by_tag( const ZrDebugUnionPrototypeRecord *record, TZrUInt64 tagValue)
 * @unit UNION-023
 *
 * ## Summary
 * 将帧内 tag 值关联到编译成员表中的 union 变体记录。
 *
 * ## Parameters
 * @param[in] record 借用已验证 prototype/members 覆盖 membersCount 的原型视图。
 * @param[in] tagValue 从当前帧 union tag 读取的无符号值。
 *
 * ## Returns
 * @return 返回首个 union variant 且 fieldOffset 转为 UInt64 后等于 tagValue 的借用成员；否则 NULL。
 *
 * ## Constraints and lifecycle
 * fieldOffset 在此承载变体 tag 编码；本函数不读取字段字节，也不重新验证成员区长度。
 *
 * ## References
 * - zr_debug_inline_union_view。
 * - zr_debug_union_find_prototype_record。
 */

/**
 * @fn TZrBool zr_debug_inline_union_view(ZrDebugAgent *agent, const SZrFunction *function, const SZrFunctionFrameSlotLayout *slotLayout, const SZrStackFramePlace *place, ZrDebugUnionView *outView)
 * @unit UNION-024
 *
 * ## Summary
 * 从暂停帧的 inline union 存储读取 tag，并连接函数布局、编译原型和变体元数据形成展示视图。
 *
 * ## Parameters
 * @param[in,out] agent 借用提供 state 的代理，元数据字段读取可能分配。
 * @param[in] function 借用当前有效函数及其帧布局上下文。
 * @param[in] slotLayout 借用该函数中 INLINE_STRUCT 且有 typeLayoutId 的槽布局。
 * @param[in] place 借用已取得的槽地址及 byteSize，至少在本次暂停访问期间有效。
 * @param[out] outView 非 NULL 的输出视图，入口清零；成功时交付拷贝名称和借用元数据。
 *
 * ## Returns
 * @return TRUE 表示 union 类型布局、tag边界、原型及tag对应变体均有效；FALSE 可留下已清零或部分构建的视图。
 *
 * ## Constraints and lifecycle
 * 读取 tag 前验证宽度和 place 边界；不证明所有 payload 字段都可解码。元数据读取仍可能分配/Throw；继续执行或函数释放后不可继续借用该视图。
 *
 * ## References
 * - ZrCore_Function_ResolvePrototypeFrameTypeLayout。
 * - zr_debug_union_find_variant_by_tag。
 * - zr_debug_inline_union_slot_view。
 */

/**
 * @fn TZrBool zr_debug_inline_union_slot_view(ZrDebugAgent *agent, const SZrFunction *function, const SZrCallInfo *callInfo, TZrUInt32 stackSlot, SZrStackFramePlace *outPlace, ZrDebugUnionView *outView)
 * @unit UNION-025
 *
 * ## Summary
 * 将当前调用帧的指定 inline 槽解析为可读原始地址与 union 视图，供展开、预览及 legacy 物化共用。
 *
 * ## Parameters
 * @param[in,out] agent 借用提供 state 的代理，后续视图构造可分配。
 * @param[in] function 借用与 callInfo/stackSlot 相符的函数。
 * @param[in] callInfo 借用仍存活且 functionBase.valuePointer 非空的调用帧。
 * @param[in] stackSlot 函数帧布局中的槽号。
 * @param[out] outPlace 可为 NULL；非空入口清零，只有整体成功后发布槽地址。
 * @param[out] outView 必需视图，非空入口清零；底层视图构造失败可留下部分内容。
 *
 * ## Returns
 * @return TRUE 表示槽为 inline struct、帧位置取得成功并可识别为 union；FALSE 表示任一步不满足。
 *
 * ## Constraints and lifecycle
 * frameBase 为函数基指针之后一个值槽；要求在同一暂停窗口访问，不能跨恢复执行保存 place 地址。普通失败不等同捕获 Throw。
 *
 * ## References
 * - ZrCore_Function_FindFrameSlotLayout。
 * - ZrCore_Function_MakeFrameSlotPlace。
 * - zr_debug_inline_union_view。
 */

/**
 * @fn static TZrBool zr_debug_union_payload_field_layout(SZrState *state, SZrObject *fieldMetadata, SZrFunctionFrameFieldLayout *outLayout)
 * @unit UNION-026
 *
 * ## Summary
 * 把 payload 字段装饰器元数据转换成调试读取布局，已知 primitive 必须同时匹配名称与本机宽度。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，元数据字段查找可分配字符串键。
 * @param[in] fieldMetadata 借用含 byteOffset/byteSize/byteAlign 的字段对象。
 * @param[out] outLayout 非 NULL 输出，入口清零；失败可保留已写 offset/size 和 UNKNOWN 类型。
 *
 * ## Returns
 * @return TRUE 表示匹配 primitive 或按当前宽度启发式归类为值槽；FALSE 表示缺少整数元数据或无法归类。
 *
 * ## Constraints and lifecycle
 * byteAlign 必须读到但目前不参与校验；整数转换本身可能截断。TODO: 大宽度不证明 SZrTypeValue 布局，需对照 compiler_union.c 的 ownershipQualifier/泛型路径并用大型原始 struct payload 核查；当前不授该路径正确性。
 *
 * ## References
 * - zr_debug_union_payload_value_from_inline。
 * - zr_debug_union_object_int_field。
 * - ZR_AST_CONSTANT_UNION_VARIANT。
 */

/**
 * @fn static TZrBool zr_debug_union_load_signed_int(const TZrByte *address, TZrUInt32 byteSize, TZrInt64 *outValue)
 * @unit UNION-027
 *
 * ## Summary
 * 从已验证地址按有符号本机宽度读取并扩展整数，供 primitive payload 解码。
 *
 * ## Parameters
 * @param[in] address 借用至少 byteSize 字节可读的非空地址。
 * @param[in] byteSize 支持的本机Int8/16/32/64宽度。
 * @param[out] outValue 非 NULL 输出；失败不修改原值。
 *
 * ## Returns
 * @return TRUE 表示支持宽度且已写扩展后的 Int64；FALSE 表示空参数或不支持宽度。
 *
 * ## Constraints and lifecycle
 * 用 memcpy 处理对齐，不自行验证范围和所有权；字节按当前宿主端序解释。
 *
 * ## References
 * - zr_debug_union_load_field。
 */

/**
 * @fn static TZrBool zr_debug_union_load_field(SZrState *state, const SZrFunctionFrameFieldLayout *fieldLayout, const TZrByte *fieldAddress, SZrTypeValue *result)
 * @unit UNION-028
 *
 * ## Summary
 * 把已选 payload 布局的原始字节转换成调试值，直接解码 primitive 或复制值槽，避免脚本成员访问。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，值槽复制可能涉及所有权克隆或分配。
 * @param[in] fieldLayout 借用已正确分类且范围已由上游验证的字段布局。
 * @param[in] fieldAddress 借用覆盖该字段的有效字节地址。
 * @param[in,out] result 非 NULL、已初始化且与源独立的结果槽；本函数不入口清零，普通失败保留原值。
 *
 * ## Returns
 * @return TRUE 表示已按 primitive 宽度写值或完成 ZrCore_Value_Copy 调用；FALSE 表示不支持的布局/宽度或空参数。
 *
 * ## Constraints and lifecycle
 * TRUE 不额外检查 Copy 后的 threadStatus。值槽 Copy 遵循核心覆盖/所有权协议，可先释放目标旧所有权并分配或 Throw；primitive 分支直接初始化，不替旧资源作统一清理，调用者应交付已初始化的独立结果槽。大原始字段误分类风险沿用布局 TODO，不能仅凭 byteSize 证明安全。
 *
 * ## References
 * - zr_debug_union_payload_field_layout。
 * - ZrCore_Value_Copy。
 * - zr_debug_union_payload_value_from_inline。
 */

/**
 * @fn TZrBool zr_debug_union_payload_display_name(SZrState *state, const ZrDebugUnionView *view, TZrUInt32 payloadIndex, TZrChar *buffer, TZrSize bufferSize)
 * @unit UNION-029
 *
 * ## Summary
 * 为 payload 子节点选择装饰器用户名称，缺失时生成 payloadN，统一展示与受限成员查找的名称。
 *
 * ## Parameters
 * @param[in,out] state 可为空；非空时用于借用元数据字段，查询可分配。
 * @param[in] view 可为空；缺少 payload_fields 时使用编号回退。
 * @param[in] payloadIndex 欲展示的零基 payload 序号，本函数不校验 payload_count。
 * @param[out] buffer 非空、至少 bufferSize 字节的写入缓冲区。
 * @param[in] bufferSize 必须大于零，包括结尾 NUL；容量不足会截断。
 *
 * ## Returns
 * @return TRUE 表示有效输出缓冲区已写元数据名称或编号回退；FALSE 只表示缓冲区为空或零容量。
 *
 * ## Constraints and lifecycle
 * 入口在有容量时先清零；名称截断不视为失败。输入元数据及文本只借用；元数据查询仍可能 Throw。
 *
 * ## References
 * - zr_debug_union_array_object_at。
 * - zr_debug_union_safe_get_member_value。
 * - zr_debug_expand_inline_union_handle。
 */

/**
 * @fn static TZrBool zr_debug_union_payload_internal_name(SZrState *state, const ZrDebugUnionView *view, TZrUInt32 payloadIndex, TZrChar *buffer, TZrSize bufferSize)
 * @unit UNION-030
 *
 * ## Summary
 * 为 carrier 存储访问选择内部字段名，有 internalName 元数据则借用其文本，否则使用约定的内部 payload 前缀加序号。
 *
 * ## Parameters
 * @param[in,out] state 可为空；用于元数据字段读取，可分配字符串键。
 * @param[in] view 可为空，缺少 payload_fields 时使用约定回退。
 * @param[in] payloadIndex 内部 payload 序号，本函数不验证数量边界。
 * @param[out] buffer 非空且可写的名称缓冲区。
 * @param[in] bufferSize 必须大于零，包括 NUL；过短时仍返回成功但名称截断。
 *
 * ## Returns
 * @return TRUE 表示已交付内部名或回退名；FALSE 表示输出缓冲区缺失。
 *
 * ## Constraints and lifecycle
 * 当前编译器发布 storageName，读取分支查询的是 internalName，因此通常沿约定回退。不能把展示 name 用作存储键；不声称两元数据字段已等同。
 *
 * ## References
 * - zr_debug_union_payload_value_from_carrier。
 * - zr_debug_materialize_inline_union_slot。
 * - ZR_DEBUG_UNION_PAYLOAD_PREFIX。
 */

/**
 * @fn TZrBool zr_debug_union_payload_value_from_carrier(ZrDebugAgent *agent, const SZrTypeValue *value, const ZrDebugUnionView *view, TZrUInt32 payloadIndex, SZrTypeValue *outValue)
 * @unit UNION-031
 *
 * ## Summary
 * 从 boxed carrier 直接读取一个内部 payload，缺槽时保留可展示的 null，供展开及受限成员求值。
 *
 * ## Parameters
 * @param[in,out] agent 借用提供 state 的代理，内部字段名和键读取可分配。
 * @param[in] value 借用非空 OBJECT/ARRAY carrier；须与 outValue 独立。
 * @param[in] view 可为空，只参与内部名选择；本函数不重新验证整个视图。
 * @param[in] payloadIndex 调用者验证的 payload 序号，本函数不检查 payload_count。
 * @param[out] outValue 非 NULL 输出，入口重置为 null；成功时为存储值的浅拷贝。
 *
 * ## Returns
 * @return TRUE 表示字段取得或字段缺失按 null 展示；FALSE 表示参数或名称缓冲区规则不满足。
 *
 * ## Constraints and lifecycle
 * NULL 字段也可能来自字符串键创建普通失败，故 TRUE/null 不是强错误区分。浅拷贝不增加所有权或独立 GC 根；源 carrier 必须维持存活。Throw 可非局部退出。
 *
 * ## References
 * - zr_debug_union_object_get_field。
 * - zr_debug_union_payload_internal_name。
 * - zr_debug_union_safe_get_member_value。
 */

/**
 * @fn TZrBool zr_debug_union_payload_value_from_inline(ZrDebugAgent *agent, const SZrFunction *function, const SZrStackFramePlace *place, const ZrDebugUnionView *view, TZrUInt32 payloadIndex, SZrTypeValue *outValue)
 * @unit UNION-032
 *
 * ## Summary
 * 根据当前变体的 payload 元数据读取暂停帧字节，为 inline 展开或 carrier 物化提供一个值。
 *
 * ## Parameters
 * @param[in,out] agent 借用 state，元数据读取和带所有权值的复制可分配。
 * @param[in] function 必须非空的当前函数上下文；当前函数体仅保留该参数，不参与字段解码。
 * @param[in] place 借用有效地址和完整 byteSize 的帧槽位置。
 * @param[in] view 借用与该槽当前变体对应的视图及 payload_fields。
 * @param[in] payloadIndex 必须小于 view->payload_count 的零基序号。
 * @param[out] outValue 非 NULL 独立输出，入口重置为 null，不释放其旧资源。
 *
 * ## Returns
 * @return TRUE 表示字段元数据可解释、offset/size 落在 place 内且底层解码返回成功；FALSE 时输出通常为入口 null。
 *
 * ## Constraints and lifecycle
 * 不验证 place 与 view 的身份一致性，调用者须从同一次槽视图取得二者。地址只在暂停帧有效；布局启发式 TODO 与 Copy/分配 Throw 限制均沿用。
 *
 * ## References
 * - zr_debug_union_payload_field_layout。
 * - zr_debug_union_load_field。
 * - zr_debug_inline_union_slot_view。
 */

/**
 * @fn TZrBool zr_debug_materialize_inline_union_slot(ZrDebugAgent *agent, const SZrFunction *function, const SZrCallInfo *callInfo, TZrUInt32 stackSlot, SZrTypeValue *outValue)
 * @unit UNION-033
 *
 * ## Summary
 * 在 legacy 标识符解析需要值对象时，将当前 inline union 槽物化成包含类型、变体与 payload 的 carrier。
 *
 * ## Parameters
 * @param[in,out] agent 借用当前暂停代理与 state，创建对象和写字段可分配/Throw。
 * @param[in] function 借用与调用帧和槽号相符的函数。
 * @param[in] callInfo 借用仍处于当前暂停窗口的调用帧。
 * @param[in] stackSlot 欲物化的 inline union 槽号。
 * @param[out] outValue 非 NULL 独立输出，入口重置为 null，只有全部字段成功后才发布 carrier。
 *
 * ## Returns
 * @return TRUE 表示 carrier 已完整构建并发布；普通失败返回 FALSE，输出保持 null；Throw 可绕过普通返回。
 *
 * ## Constraints and lifecycle
 * BUG: 允许 legacy fallback 的公开求值在解析失败表达式左侧 inline union 局部变量时可达；对象被显式 Ignore 后本路径及句柄清理都无 Unignore，成功重复物化或后续构造失败均可使 carrier 根滞留至 VM/GC 销毁。先前字段无回滚；本注释仅记录完整静态路径，未运行故障复现。
 *
 * ## References
 * - zr_debug_resolve_identifier_value。
 * - ZrCore_GarbageCollector_IgnoreObject。
 * - ZrCore_GarbageCollector_UnignoreObject。
 * - zr_debug_variable_handles_clear。
 */

/**
 * @fn TZrBool zr_debug_union_safe_get_member_value(ZrDebugAgent *agent, const SZrTypeValue *receiver, const TZrChar *memberName, SZrTypeValue *outValue)
 * @unit UNION-034
 *
 * ## Summary
 * 为 boxed union 暴露受限的 type、variant 和 payload 名称访问，使用直接存储而不执行脚本 getter。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理和 state，名称或字段查询可分配/Throw。
 * @param[in] receiver 借用可构造 union 展示视图的 OBJECT/ARRAY 值，不与输出重叠。
 * @param[in] memberName 借用 NUL 终止的请求名称。
 * @param[out] outValue 非 NULL 输出，入口清为 null；类型/变体为 VM 字符串，payload 为浅拷贝。
 *
 * ## Returns
 * @return TRUE 表示命中 type/variant、有效编号或匹配的展示名并交付结果；FALSE 表示输入/视图/成员不满足。
 *
 * ## Constraints and lifecycle
 * 只读语义不保证零分配；不存在脚本 getter 派发不等于可以绕过正式求值策略。名称截断及 payload 浅借用规则沿用下层。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_union_make_string_value。
 * - zr_debug_safe_get_member_value。
 * - zr_debug_expand_union_value_handle。
 */

/**
 * @fn TZrBool zr_debug_format_union_value_text(ZrDebugAgent *agent, const SZrTypeValue *value, TZrChar *buffer, TZrSize bufferSize)
 * @unit UNION-035
 *
 * ## Summary
 * 在普通值预览之前尝试生成 boxed union 的类型与变体文本，供 snapshot 及公开求值结果复用。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理与 state，构造展示视图时可分配。
 * @param[in] value 借用待识别的 carrier 值。
 * @param[out] buffer 非空且可写的输出文本缓冲区，有容量时入口清空。
 * @param[in] bufferSize 必须大于零，包含结尾 NUL。
 *
 * ## Returns
 * @return TRUE 表示成功构成 union 视图并格式化，文本可截断；FALSE 表示缓冲区或视图条件不满足，调用者可采用其它格式器。
 *
 * ## Constraints and lifecycle
 * 不校验 canonical 类型身份，不承诺无分配或捕获 Throw；输出只是停点时的展示文本，不延长底层值存活。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_format_inline_union_text。
 * - zr_debug_fill_value_preview。
 * - zr_debug_evaluate_with_policy。
 */

/**
 * @fn void zr_debug_format_inline_union_text(const ZrDebugUnionView *view, TZrChar *buffer, TZrSize bufferSize)
 * @unit UNION-036
 *
 * ## Summary
 * 将已构成的 union 视图格式化为固定的类型.变体预览，不再读取 VM 字段。
 *
 * ## Parameters
 * @param[in] view 借用视图，可为 NULL；非空时两个固定容量名称应已 NUL 终止。
 * @param[out] buffer 可为空；非空且有容量时写入文本并保证结尾 NUL。
 * @param[in] bufferSize 缓冲区总容量；零表示不写入。
 *
 * ## Returns
 * 无返回值；无效缓冲区直接返回，NULL 视图使有效缓冲区保持空串，正常输出允许截断。
 *
 * ## Constraints and lifecycle
 * 只读取视图内已拷贝名称，无分配或字段访问；不由文本判断原型有效性。
 *
 * ## References
 * - zr_debug_format_union_value_text。
 * - zr_debug_fill_inline_union_preview。
 */

/**
 * @def ZR_DEBUG_UNION_TYPE_FIELD
 * @unit UNION-037
 *
 * ## Summary
 * 约定 boxed carrier 的展示类型名称存储键，连接编译生成对象与调试视图。
 *
 * ## Value model
 * 值为 NUL 终止字面量 `__zr_unionType`；不分配存储或转移对象所有权。
 *
 * ## Constraints and lifecycle
 * 字面值须与现行编译生成carrier和执行层使用的协议一致；这是文件局部字符串宏，不可当作编译器同名宏被调用。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_materialize_inline_union_slot。
 */

/**
 * @def ZR_DEBUG_UNION_VARIANT_FIELD
 * @unit UNION-038
 *
 * ## Summary
 * 约定 boxed carrier 的变体字符串存储键，用于窄形状判别和统一视图。
 *
 * ## Value model
 * 值为 NUL 终止字面量 `__zr_unionVariant`；不分配存储或转移对象所有权。
 *
 * ## Constraints and lifecycle
 * 字面值须与现行编译生成carrier和执行层使用的协议一致；这是文件局部字符串宏，不可当作编译器同名宏被调用。
 *
 * ## References
 * - zr_debug_value_is_union_carrier。
 * - zr_debug_union_value_view。
 * - zr_debug_materialize_inline_union_slot。
 */

/**
 * @def ZR_DEBUG_UNION_PAYLOAD_PREFIX
 * @unit UNION-039
 *
 * ## Summary
 * 约定 carrier 内部 payload 数字键的前缀，区别于用户展示名称和 payloadN 别名。
 *
 * ## Value model
 * 值为 NUL 终止字面量 `__zr_unionPayload`；不分配存储或转移对象所有权。
 *
 * ## Constraints and lifecycle
 * 字面值须与现行编译生成carrier和执行层使用的协议一致；这是文件局部字符串宏，不可当作编译器同名宏被调用。
 *
 * ## References
 * - zr_debug_count_union_carrier_payload_fields。
 * - zr_debug_union_payload_internal_name。
 * - zr_debug_union_safe_get_member_value。
 */

/**
 * @struct ZrDebugUnionPrototypeRecord
 * @unit UNION-040
 *
 * ## Summary
 * 借用变长 prototypeData 中已定位的一个编译原型及其连续成员区，避免复制元数据。
 *
 * ## Fields
 * - `prototype`：该记录的原型头，含类型、名称索引和成员计数。
 * - `members`：跳过继承项和装饰器索引后的成员数组起点，元素数由 prototype->membersCount 提供。
 *
 * ## Invariants and lifecycle
 * 仅由已验证长度的查找路径填入；全零表示尚无记录。两指针都不拥有内存，函数原型缓冲区重建或释放后不可再访问；使用依赖当前编译结构的本机布局。
 *
 * ## References
 * - zr_debug_union_find_prototype_record。
 * - zr_debug_union_find_variant_member。
 * - zr_debug_union_find_variant_by_tag。
 */
