#include "writer_intermediate_generated_source_map.h"

#include <stdint.h>
#include <string.h>

#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"

/* 仅在遍历单个成员期间借用常量池字符串；来源标识和行号来自其 decorator 元数据。 */
typedef struct SZrWriterGeneratedSourceMap {
    const TZrChar *typeName;
    const TZrChar *memberName;
    TZrUInt64 originTargetSymbolId;
    TZrUInt64 sourceLineStart;
    TZrUInt64 sourceLineEnd;
} SZrWriterGeneratedSourceMap;

static void generated_source_map_write_indent(FILE *file, TZrUInt32 indentLevel) {
    for (TZrUInt32 index = 0U; file != ZR_NULL && index < indentLevel; index++) {
        fputc(' ', file);
    }
}

/* 映射行借用现有常量池名称，不在诊断输出期间建立额外的字符串所有权。 */
static const TZrChar *generated_source_map_string_constant(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 index) {
    const SZrTypeValue *value;

    if (state == ZR_NULL || function == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }
    value = &function->constantValueList[index];
    if (value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_GetNativeString(
            ZR_CAST_STRING(state, value->value.object));
}

/* 生成字段元数据保存在通用对象的哈希表中；按字段名取值而不依赖桶顺序。 */
static const SZrTypeValue *generated_source_map_object_field(
        SZrState *state,
        const SZrObject *object,
        const TZrChar *name) {
    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        !object->nodeMap.isValid || object->nodeMap.buckets == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize bucketIndex = 0U;
         bucketIndex < object->nodeMap.capacity;
         bucketIndex++) {
        const SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->key.type == ZR_VALUE_TYPE_STRING &&
                pair->key.value.object != ZR_NULL) {
                const SZrString *key = ZR_CAST_STRING(
                        state, pair->key.value.object);
                const TZrChar *keyText = key != ZR_NULL
                                                  ? ZrCore_String_GetNativeString(
                                                            (SZrString *)key)
                                                  : ZR_NULL;
                if (keyText != ZR_NULL && strcmp(keyText, name) == 0) {
                    return &pair->value;
                }
            }
            pair = pair->next;
        }
    }
    return ZR_NULL;
}

static TZrBool generated_source_map_read_uint(
        const SZrTypeValue *value,
        TZrUInt64 *result) {
    if (value == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *result = value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_INT(value->type) &&
        value->value.nativeObject.nativeInt64 >= 0) {
        *result = (TZrUInt64)value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

/* 只接受编译期声明补丁写入的完整来源四元组，避免把普通 decorator 当成生成字段。 */
static TZrBool generated_source_map_from_member(
        SZrState *state,
        const SZrFunction *function,
        const SZrCompiledPrototypeInfo *prototype,
        const SZrCompiledMemberInfo *member,
        SZrWriterGeneratedSourceMap *result) {
    const SZrTypeValue *metadataValue;
    const SZrObject *metadata;
    TZrUInt64 generated;

    if (state == ZR_NULL || function == ZR_NULL || prototype == ZR_NULL ||
        member == ZR_NULL || result == ZR_NULL ||
        !member->hasDecoratorMetadata ||
        member->decoratorMetadataConstantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }
    metadataValue = &function->constantValueList[
            member->decoratorMetadataConstantIndex];
    if (metadataValue->type != ZR_VALUE_TYPE_OBJECT ||
        metadataValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    metadata = ZR_CAST_OBJECT(state, metadataValue->value.object);
    if (!generated_source_map_read_uint(
                generated_source_map_object_field(
                        state, metadata, "generated"),
                &generated) ||
        generated != 1U ||
        !generated_source_map_read_uint(
                generated_source_map_object_field(
                        state, metadata, "originTargetSymbolId"),
                &result->originTargetSymbolId) ||
        result->originTargetSymbolId == 0U ||
        !generated_source_map_read_uint(
                generated_source_map_object_field(
                        state, metadata, "sourceLineStart"),
                &result->sourceLineStart) ||
        !generated_source_map_read_uint(
                generated_source_map_object_field(
                        state, metadata, "sourceLineEnd"),
                &result->sourceLineEnd) ||
        result->sourceLineStart > result->sourceLineEnd) {
        return ZR_FALSE;
    }
    result->typeName = generated_source_map_string_constant(
            state, function, prototype->nameStringIndex);
    result->memberName = generated_source_map_string_constant(
            state, function, member->nameStringIndex);
    return result->typeName != ZR_NULL && result->memberName != ZR_NULL;
}

/* 先核对打包协议的计数、乘法和剩余字节，再允许后续遍历成员记录。 */
static TZrBool generated_source_map_prototype_size(
        const SZrCompiledPrototypeInfo *prototype,
        TZrSize remaining,
        TZrSize *result) {
    TZrSize size = sizeof(*prototype);
    TZrSize count;

    if (prototype == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (prototype->inheritsCount >
        UINT32_MAX - prototype->decoratorsCount) {
        return ZR_FALSE;
    }
    count = (TZrSize)prototype->inheritsCount +
            (TZrSize)prototype->decoratorsCount;
    if (count > (SIZE_MAX - size) / sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }
    size += count * sizeof(TZrUInt32);
    if ((TZrSize)prototype->membersCount >
        (SIZE_MAX - size) / sizeof(SZrCompiledMemberInfo)) {
        return ZR_FALSE;
    }
    size += (TZrSize)prototype->membersCount *
            sizeof(SZrCompiledMemberInfo);
    if (size > remaining) {
        return ZR_FALSE;
    }
    *result = size;
    return ZR_TRUE;
}

/* 顶层和子函数共享同一 prototype 布局检查，拒绝截断或尾随数据。 */
static TZrBool generated_source_map_validate_prototype_payload(
        const SZrFunction *function) {
    const TZrByte *cursor;
    TZrSize remaining;
    TZrUInt32 encodedCount;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function->prototypeData == ZR_NULL) {
        return function->prototypeDataLength == 0U &&
               function->prototypeCount == 0U;
    }
    if (function->prototypeDataLength < sizeof(encodedCount)) {
        return ZR_FALSE;
    }
    memcpy(&encodedCount, function->prototypeData, sizeof(encodedCount));
    if (encodedCount != function->prototypeCount) {
        return ZR_FALSE;
    }

    cursor = function->prototypeData + sizeof(encodedCount);
    remaining = function->prototypeDataLength - sizeof(encodedCount);
    for (TZrUInt32 prototypeIndex = 0U;
         prototypeIndex < encodedCount;
         prototypeIndex++) {
        SZrCompiledPrototypeInfo prototype;
        TZrSize prototypeSize;

        if (remaining < sizeof(prototype)) {
            return ZR_FALSE;
        }
        memcpy(&prototype, cursor, sizeof(prototype));
        if (!generated_source_map_prototype_size(
                    &prototype, remaining, &prototypeSize)) {
            return ZR_FALSE;
        }
        cursor += prototypeSize;
        remaining -= prototypeSize;
    }
    return remaining == 0U;
}

/* 文件尚未打开时递归核验，防止某个深层子函数损坏后留下半份 .zri。 */
TZrBool writer_intermediate_validate_function_prototype_data(
        const SZrFunction *function) {
    if (!generated_source_map_validate_prototype_payload(function)) {
        return ZR_FALSE;
    }
    if (function->childFunctionLength > 0U &&
        function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 childIndex = 0U;
         childIndex < function->childFunctionLength;
         childIndex++) {
        if (!writer_intermediate_validate_function_prototype_data(
                    &function->childFunctionList[childIndex])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 按 prototype 和成员的序列化顺序遍历；空 file 用于预先计算是否需要输出段头。 */
static TZrSize generated_source_map_visit(
        FILE *file,
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 indentLevel) {
    const TZrByte *cursor;
    TZrSize remaining;
    TZrSize mapCount = 0U;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->prototypeData == ZR_NULL ||
        function->prototypeDataLength < sizeof(TZrUInt32)) {
        return 0U;
    }
    cursor = function->prototypeData + sizeof(TZrUInt32);
    remaining = function->prototypeDataLength - sizeof(TZrUInt32);
    for (TZrUInt32 prototypeIndex = 0U;
         prototypeIndex < function->prototypeCount;
         prototypeIndex++) {
        const SZrCompiledPrototypeInfo *prototype;
        const SZrCompiledMemberInfo *members;
        TZrSize prototypeSize;
        TZrSize headerSize;

        if (remaining < sizeof(SZrCompiledPrototypeInfo)) {
            return mapCount;
        }
        prototype = (const SZrCompiledPrototypeInfo *)cursor;
        if (!generated_source_map_prototype_size(
                    prototype, remaining, &prototypeSize)) {
            return mapCount;
        }
        headerSize = sizeof(*prototype) +
                     ((TZrSize)prototype->inheritsCount +
                      (TZrSize)prototype->decoratorsCount) *
                             sizeof(TZrUInt32);
        members = (const SZrCompiledMemberInfo *)(cursor + headerSize);
        for (TZrUInt32 memberIndex = 0U;
             memberIndex < prototype->membersCount;
             memberIndex++) {
            SZrWriterGeneratedSourceMap map = {0};
            if (!generated_source_map_from_member(
                        state,
                        function,
                        prototype,
                        &members[memberIndex],
                        &map)) {
                continue;
            }
            if (file != ZR_NULL) {
                generated_source_map_write_indent(file, indentLevel);
                fprintf(file,
                        "  [%zu] type=%s member=%s "
                        "originTargetSymbolId=%llu "
                        "sourceLineStart=%llu sourceLineEnd=%llu\n",
                        (size_t)mapCount,
                        map.typeName,
                        map.memberName,
                        (unsigned long long)map.originTargetSymbolId,
                        (unsigned long long)map.sourceLineStart,
                        (unsigned long long)map.sourceLineEnd);
            }
            mapCount++;
        }
        cursor += prototypeSize;
        remaining -= prototypeSize;
    }
    return mapCount;
}

/* 两次遍历使用同一筛选条件，使段头数量与后续行号一致，并省略空段。 */
void writer_intermediate_write_generated_source_maps(
        FILE *file,
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 indentLevel) {
    TZrSize mapCount;

    if (file == ZR_NULL || state == ZR_NULL || function == ZR_NULL) {
        return;
    }
    mapCount = generated_source_map_visit(
            ZR_NULL, state, function, indentLevel);
    if (mapCount == 0U) {
        return;
    }
    generated_source_map_write_indent(file, indentLevel);
    fprintf(file, "GENERATED_SOURCE_MAPS (%zu):\n", (size_t)mapCount);
    generated_source_map_visit(file, state, function, indentLevel);
    fprintf(file, "\n");
}
