//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"
#include <string.h>

#include "zr_vm_core/array.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/utf8.h"
#include "zr_vm_core/value.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(ZR_DEBUG)
static TZrBool string_trace_enabled(void);
static void string_trace(const TZrChar *format, ...);
#else
/* 非调试构建把日志调用及实参一并裁去；调用点因此只应提供无副作用的诊断值。 */
#define string_trace(...) ((void)0)
#endif
static SZrString *string_create_short(SZrState *state, TZrNativeString string, TZrSize length);

/* 哈希表容量变化时重建 minor GC 的桶索引存储；失败保留旧索引供调用方判断。 */
static TZrBool string_table_resize_minor_young_bucket_flags(SZrGlobalState *global,
                                                            SZrStringTable *stringTable,
                                                            TZrSize capacity) {
    TZrSize oldFlagBytes;
    TZrSize oldIndexBytes;
    TZrUInt8 *flags = ZR_NULL;
    TZrSize *indexes = ZR_NULL;

    if (global == ZR_NULL || stringTable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (capacity > 0u) {
        flags = (TZrUInt8 *)ZrCore_Memory_RawMallocWithType(global,
                                                            capacity * sizeof(TZrUInt8),
                                                            ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
        indexes = (TZrSize *)ZrCore_Memory_RawMallocWithType(global,
                                                             capacity * sizeof(TZrSize),
                                                             ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
        if (flags == ZR_NULL || indexes == ZR_NULL) {
            if (flags != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global,
                                              flags,
                                              capacity * sizeof(TZrUInt8),
                                              ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
            }
            if (indexes != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global,
                                              indexes,
                                              capacity * sizeof(TZrSize),
                                              ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
            }
            return ZR_FALSE;
        }
    }

    oldFlagBytes = stringTable->minorYoungBucketCapacity * sizeof(TZrUInt8);
    oldIndexBytes = stringTable->minorYoungBucketCapacity * sizeof(TZrSize);
    if (stringTable->minorYoungBucketFlags != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      stringTable->minorYoungBucketFlags,
                                      oldFlagBytes,
                                      ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
    }
    if (stringTable->minorYoungBucketIndexes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      stringTable->minorYoungBucketIndexes,
                                      oldIndexBytes,
                                      ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET);
    }

    stringTable->minorYoungBucketFlags = flags;
    stringTable->minorYoungBucketIndexes = indexes;
    stringTable->minorYoungBucketCapacity = capacity;
    stringTable->minorYoungBucketCount = 0u;
    return ZR_TRUE;
}

/* 从当前驻留桶恢复年轻代位置，供哈希扩容和 GC 周期共用。 */
static TZrBool string_table_rebuild_minor_young_bucket_flags(SZrGlobalState *global, SZrStringTable *stringTable) {
    const SZrHashSet *set;

    if (global == ZR_NULL || stringTable == ZR_NULL) {
        return ZR_FALSE;
    }

    set = &stringTable->stringHashSet;
    if (!string_table_resize_minor_young_bucket_flags(global, stringTable, set->capacity)) {
        return ZR_FALSE;
    }

    stringTable->minorYoungBucketCount = 0u;
    if (stringTable->minorYoungBucketFlags == ZR_NULL || set->capacity == 0u) {
        return ZR_TRUE;
    }

    ZrCore_Memory_RawSet(stringTable->minorYoungBucketFlags, 0, set->capacity * sizeof(TZrUInt8));
    if (!set->isValid || set->buckets == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < set->capacity; bucketIndex++) {
        const SZrHashKeyValuePair *pair = set->buckets[bucketIndex];

        while (pair != ZR_NULL) {
            SZrRawObject *object = pair->key.value.object;

            if (object != ZR_NULL &&
                object->garbageCollectMark.storageKind == ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) {
                (void)ZrCore_StringTable_RecordMinorYoungBucket(stringTable, bucketIndex);
                break;
            }
            pair = pair->next;
        }
    }

    return ZR_TRUE;
}

/* 新驻留的年轻代短串需加入增量索引，否则 minor GC 可能漏扫其桶。 */
static ZR_FORCE_INLINE void string_table_record_young_short_string(SZrGlobalState *global, const SZrString *stringValue) {
    SZrStringTable *stringTable;
    TZrSize bucketIndex;

    if (global == ZR_NULL || global->stringTable == ZR_NULL || stringValue == ZR_NULL ||
        stringValue->super.garbageCollectMark.storageKind != ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE) {
        return;
    }

    stringTable = global->stringTable;
    if (!ZrCore_StringTable_MinorYoungBucketFlagsReady(stringTable) &&
        !string_table_rebuild_minor_young_bucket_flags(global, stringTable)) {
        return;
    }
    if (stringTable->minorYoungBucketFlags == ZR_NULL ||
        stringTable->minorYoungBucketIndexes == ZR_NULL ||
        stringTable->stringHashSet.capacity == 0u) {
        return;
    }

    bucketIndex = ZR_HASH_MOD(stringValue->super.hash, stringTable->stringHashSet.capacity);
    (void)ZrCore_StringTable_RecordMinorYoungBucket(stringTable, bucketIndex);
}

/* pair cache 按已计算的内容哈希定位；命中仍以 GC 字符串身份确认。 */
static ZR_FORCE_INLINE TZrSize string_concat_pair_cache_bucket_index(const SZrString *left, const SZrString *right) {
    TZrUInt64 leftHash;
    TZrUInt64 rightHash;
    TZrUInt64 mixedHash;

    if (left == ZR_NULL || right == ZR_NULL) {
        return 0u;
    }

    leftHash = left->super.hash;
    rightHash = right->super.hash;
    mixedHash = (leftHash * 1315423911u) ^ (rightHash + (leftHash << 7u) + (rightHash >> 3u));
    return (TZrSize)(mixedHash % ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_COUNT);
}

/* 全局有界缓存拼接对象对及结果并作为 GC 根；默认双路次槽命中后提升到首槽。 */
static ZR_FORCE_INLINE SZrString *string_concat_pair_cache_lookup(SZrState *state,
                                                                  const SZrString *left,
                                                                  const SZrString *right) {
    SZrGlobalState *global;
    TZrSize bucketIndex;
    ZrStringConcatPairCacheEntry *bucket;

    if (state == ZR_NULL || state->global == ZR_NULL || left == ZR_NULL || right == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    bucketIndex = string_concat_pair_cache_bucket_index(left, right);
    bucket = global->stringConcatPairCache[bucketIndex];
    if (bucket[0].left == left && bucket[0].right == right) {
        return bucket[0].result;
    }

#if ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_DEPTH == 2U
    if (bucket[1].left == left && bucket[1].right == right) {
        ZrStringConcatPairCacheEntry hitEntry = bucket[1];

        bucket[1] = bucket[0];
        bucket[0] = hitEntry;
        return hitEntry.result;
    }
#else
    for (TZrSize depthIndex = 1; depthIndex < ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_DEPTH; depthIndex++) {
        if (bucket[depthIndex].left == left && bucket[depthIndex].right == right) {
            ZrStringConcatPairCacheEntry hitEntry = bucket[depthIndex];

            for (; depthIndex > 0; depthIndex--) {
                bucket[depthIndex] = bucket[depthIndex - 1u];
            }
            bucket[0] = hitEntry;
            return hitEntry.result;
        }
    }
#endif

    return ZR_NULL;
}

/* 仅缓存成功创建的结果；调用方须在输入对象仍由当前执行栈保护时写入。 */
static ZR_FORCE_INLINE void string_concat_pair_cache_store(SZrState *state,
                                                           const SZrString *left,
                                                           const SZrString *right,
                                                           SZrString *result) {
    SZrGlobalState *global;
    TZrSize bucketIndex;
    ZrStringConcatPairCacheEntry *bucket;

    if (state == ZR_NULL || state->global == ZR_NULL || left == ZR_NULL || right == ZR_NULL || result == ZR_NULL) {
        return;
    }

    global = state->global;
    bucketIndex = string_concat_pair_cache_bucket_index(left, right);
    bucket = global->stringConcatPairCache[bucketIndex];

    for (TZrSize depthIndex = 0; depthIndex < ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_DEPTH; depthIndex++) {
        if (bucket[depthIndex].left == left && bucket[depthIndex].right == right) {
            ZrStringConcatPairCacheEntry hitEntry = bucket[depthIndex];

            hitEntry.result = result;
            for (; depthIndex > 0; depthIndex--) {
                bucket[depthIndex] = bucket[depthIndex - 1u];
            }
            bucket[0] = hitEntry;
            return;
        }
    }

    for (TZrSize depthIndex = ZR_GLOBAL_CONCAT_PAIR_CACHE_BUCKET_DEPTH - 1u; depthIndex > 0; depthIndex--) {
        bucket[depthIndex] = bucket[depthIndex - 1u];
    }

    bucket[0].left = (SZrString *)left;
    bucket[0].right = (SZrString *)right;
    bucket[0].result = result;
}

/* 先复制两个借用片段再分配 GC 对象，避免创建期间移动输入对象后继续读旧视图。 */
static SZrString *string_create_native_concat_segments(SZrState *state,
                                                       TZrNativeString leftNative,
                                                       TZrSize leftLength,
                                                       TZrNativeString rightNative,
                                                       TZrSize rightLength) {
    TZrSize totalLength;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    totalLength = leftLength + rightLength;
    if (totalLength <= ZR_VM_SHORT_STRING_MAX) {
        TZrChar stackBuffer[ZR_VM_SHORT_STRING_MAX + 1];

        if (leftLength > 0 && leftNative != ZR_NULL) {
            memcpy(stackBuffer, leftNative, leftLength);
        }
        if (rightLength > 0 && rightNative != ZR_NULL) {
            memcpy(stackBuffer + leftLength, rightNative, rightLength);
        }
        stackBuffer[totalLength] = '\0';
        return string_create_short(state, stackBuffer, totalLength);
    }

    {
        SZrGlobalState *global = state->global;
        TZrNativeString buffer = ZR_CAST(TZrNativeString,
                                         ZrCore_Memory_RawMallocWithType(global,
                                                                         totalLength + 1,
                                                                         ZR_MEMORY_NATIVE_TYPE_STRING));
        SZrString *result;
        TZrSize totalSize = sizeof(SZrString);

        if (buffer == ZR_NULL) {
            return ZR_NULL;
        }
        if (leftLength > 0 && leftNative != ZR_NULL) {
            memcpy(buffer, leftNative, leftLength);
        }
        if (rightLength > 0 && rightNative != ZR_NULL) {
            memcpy(buffer + leftLength, rightNative, rightLength);
        }
        buffer[totalLength] = '\0';

        result = (SZrString *)ZrCore_RawObject_New(state, ZR_VALUE_TYPE_STRING, totalSize, ZR_TRUE);
        if (result == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global, buffer, totalLength + 1, ZR_MEMORY_NATIVE_TYPE_STRING);
            return ZR_NULL;
        }

        result->longString = buffer;
        result->shortStringLength = ZR_VM_LONG_STRING_FLAG;
        result->longStringLength = totalLength;
        ZrCore_RawObject_InitHash(ZR_CAST_RAW_OBJECT_AS_SUPER(result), ZrCore_Hash_Create(global, buffer, totalLength));
        return result;
    }
}

/* 栈拼接始终按字节计长，允许 UTF-8 多字节和内嵌 NUL。 */
static TZrSize zr_string_length_local(const SZrString *string) {
    return ZrCore_String_GetByteLength(string);
}

/* 将 count 个输入所在的栈窗口收束成一个结果槽；失败时保留 null 占位。 */
static void zr_string_collapse_stack_window(SZrState *state,
                                            TZrStackValuePointer firstSlot,
                                            SZrString *result) {
    if (state == ZR_NULL || firstSlot == ZR_NULL) {
        return;
    }

    if (result != ZR_NULL) {
        ZrCore_Stack_SetRawObjectValue(state, firstSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
        ZrCore_Stack_GetValueNoProfile(firstSlot)->type = ZR_VALUE_TYPE_STRING;
    } else {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValueNoProfile(firstSlot));
    }

    state->stackTop.valuePointer = firstSlot + 1;
}

/* 执行器/格式化器共用的栈合并；锚点在类型转换引发栈迁移后恢复首槽。 */
static void zr_string_concat_stack_values(SZrState *state, TZrSize count, TZrBool convertNonStrings) {
    SZrGlobalState *global;
    TZrSize availableCount;
    TZrStackValuePointer firstSlot;
    SZrFunctionStackAnchor firstSlotAnchor;
    SZrString **stringValues = ZR_NULL;
    TZrNativeString buffer = ZR_NULL;
    TZrSize totalLength = 0;

    if (state == ZR_NULL || count == 0 || state->global == ZR_NULL) {
        return;
    }

    availableCount = (TZrSize)(state->stackTop.valuePointer - state->stackBase.valuePointer);
    if (availableCount < count) {
        return;
    }

    global = state->global;
    firstSlot = state->stackTop.valuePointer - count;
    ZrCore_Function_StackAnchorInit(state, firstSlot, &firstSlotAnchor);
    if (count == 1) {
        SZrTypeValue *value = ZrCore_Stack_GetValueNoProfile(firstSlot);
        SZrString *singleString = ZR_NULL;

        if (value == ZR_NULL) {
            zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
            return;
        }

        if (value->type == ZR_VALUE_TYPE_STRING) {
            state->stackTop.valuePointer = firstSlot + 1;
            return;
        }

        if (!convertNonStrings) {
            zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
            return;
        }

        singleString = ZrCore_Value_ConvertToString(state, value);
        firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
        zr_string_collapse_stack_window(state, firstSlot, singleString);
        return;
    }

    stringValues = (SZrString **)ZrCore_Memory_RawMallocWithType(global,
                                                           count * sizeof(SZrString *),
                                                           ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (stringValues == ZR_NULL) {
        zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
        return;
    }

    for (TZrSize i = 0; i < count; i++) {
        SZrTypeValue *value = ZrCore_Stack_GetValueNoProfile(firstSlot + i);
        SZrString *stringValue = ZR_NULL;

        if (value == ZR_NULL) {
            goto concat_fail;
        }

        if (value->type == ZR_VALUE_TYPE_STRING) {
            stringValue = ZR_CAST_STRING(state, value->value.object);
        } else if (convertNonStrings) {
            stringValue = ZrCore_Value_ConvertToString(state, value);
            firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
        }

        if (stringValue == ZR_NULL) {
            goto concat_fail;
        }

        stringValues[i] = stringValue;
        totalLength += zr_string_length_local(stringValue);
    }

    buffer = (TZrNativeString)ZrCore_Memory_RawMallocWithType(global,
                                                      totalLength + 1,
                                                      ZR_MEMORY_NATIVE_TYPE_STRING);
    if (buffer == ZR_NULL) {
        goto concat_fail;
    }

    {
        TZrChar *cursor = buffer;
        for (TZrSize i = 0; i < count; i++) {
            SZrString *stringValue = stringValues[i];
            TZrSize length = zr_string_length_local(stringValue);
            TZrNativeString nativeString = ZrCore_String_GetNativeString(stringValue);

            if (length > 0 && nativeString != ZR_NULL) {
                memcpy(cursor, nativeString, length);
                cursor += length;
            }
        }
        *cursor = '\0';
    }

    {
        SZrString *result = ZrCore_String_Create(state, buffer, totalLength);
        firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
        ZrCore_Memory_RawFreeWithType(global,
                                buffer,
                                totalLength + 1,
                                ZR_MEMORY_NATIVE_TYPE_STRING);
        ZrCore_Memory_RawFreeWithType(global,
                                stringValues,
                                count * sizeof(SZrString *),
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        zr_string_collapse_stack_window(state, firstSlot, result);
        return;
    }

concat_fail:
    firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
    if (buffer != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                buffer,
                                totalLength + 1,
                                ZR_MEMORY_NATIVE_TYPE_STRING);
    }
    if (stringValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                stringValues,
                                count * sizeof(SZrString *),
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
}

/* 全局状态构造期建立驻留管理器外壳，主线程启动后再初始化哈希桶。 */
void ZrCore_StringTable_New(SZrGlobalState *global) {
    SZrStringTable *stringTable =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrStringTable), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    global->stringTable = stringTable;
    if (stringTable == ZR_NULL) {
        return;
    }
    // stringTable->bucketSize = 0;
    // stringTable->elementCount = 0;
    // stringTable->capacity = 0;
    // stringTable->buckets = ZR_NULL;
    ZrCore_HashSet_Construct(&stringTable->stringHashSet);
    stringTable->isValid = ZR_FALSE;
    stringTable->minorYoungBucketFlags = ZR_NULL;
    stringTable->minorYoungBucketIndexes = ZR_NULL;
    stringTable->minorYoungBucketCapacity = 0u;
    stringTable->minorYoungBucketCount = 0u;
    stringTable->shortStringListHead = ZR_NULL;
}

/* 全局状态销毁时释放驻留表原生索引；字符串对象本身交由 GC 管理。 */
void ZrCore_StringTable_Free(struct SZrGlobalState *global, SZrStringTable *stringTable) {
    if (global == ZR_NULL || stringTable == ZR_NULL) {
        return;
    }
    SZrState *mainThread = global->mainThreadState;

    string_table_resize_minor_young_bucket_flags(global, stringTable, 0u);
    ZrCore_HashSet_Deconstruct(mainThread, &stringTable->stringHashSet);

    // 字符串对象由 GC 管理；这里只释放驻留表的原生索引。
    ZrCore_Memory_RawFreeWithType(global, stringTable, sizeof(SZrStringTable), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    // ZR_MEMORY_NATIVE_TYPE_MANAGER);
}

/* 主线程建立可用驻留表与永久 OOM 文本，随后才允许常规短串创建。 */
void ZrCore_StringTable_Init(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = global->stringTable;
    string_trace("string table init enter state=%p global=%p table=%p", (void *)state, (void *)global, (void *)stringTable);
    // stringTable
    ZrCore_HashSet_Init(state, &stringTable->stringHashSet, ZR_STRING_TABLE_INITIAL_SIZE_LOG2);
    if (!stringTable->stringHashSet.isValid) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        return;
    }
    string_trace("string table hash init done buckets=%p capacity=%llu valid=%d",
                 (void *)stringTable->stringHashSet.buckets,
                 (unsigned long long)stringTable->stringHashSet.capacity,
                 (int)stringTable->stringHashSet.isValid);
    ZrCore_StringTable_SyncMinorYoungBucketFlags(global);
    // this is the first string we created
    global->memoryErrorMessage = ZR_STRING_LITERAL(state, ZR_ERROR_MESSAGE_NOT_ENOUGH_MEMORY);
    string_trace("string table memoryErrorMessage=%p", (void *)global->memoryErrorMessage);
    /* TODO: 首个短串 Create 返回 null 并继续到永久标记的合法路径未证；核 GcMalloc 重试/Throw 与 RawObject_New 注册失败路径后再定标签。 */
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->memoryErrorMessage));
    // fill api cache with valid string
    for (TZrSize i = 0; i < ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT; i++) {
        for (TZrSize j = 0; j < ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH; j++) {
            global->stringHashApiCache[i][j] = global->memoryErrorMessage;
        }
    }
    stringTable->isValid = ZR_TRUE;
    string_trace("string table init exit isValid=%d", (int)stringTable->isValid);
}

/* minor GC 标记前核对桶容量并在扩容后恢复年轻代索引；失败以 false 返回当前调用方。 */
TZrBool ZrCore_StringTable_SyncMinorYoungBucketFlags(SZrGlobalState *global) {
    SZrStringTable *stringTable;

    if (global == ZR_NULL || global->stringTable == ZR_NULL) {
        return ZR_FALSE;
    }

    stringTable = global->stringTable;
    if (ZrCore_StringTable_MinorYoungBucketFlagsReady(stringTable)) {
        return ZR_TRUE;
    }

    return string_table_rebuild_minor_young_bucket_flags(global, stringTable);
}


/* 字符串布局的共同构造点：短串字节与 GC 对象共存，长串缓冲区独立分配。 */
static SZrString *string_object_create(SZrState *state, TZrNativeString string, TZrSize length, TZrUInt64 hash) {
    SZrGlobalState *global = state->global;
    SZrString *constantString = ZR_NULL;
    TZrSize totalSize = sizeof(SZrString);
    TZrNativeString stringBuffer = ZR_NULL;
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (SZrString *) ZrCore_RawObject_New(state, ZR_VALUE_TYPE_STRING, totalSize, ZR_TRUE);
        if (constantString == ZR_NULL) {
            return ZR_NULL;
        }
        if (length > 0) {
            ZrCore_Memory_RawCopy(constantString->stringDataExtend, string, length);
        }
        ((TZrNativeString) constantString->stringDataExtend)[length] = '\0';
        constantString->shortStringLength = (TZrUInt8) length;
        constantString->nextShortString = ZR_NULL;
        stringBuffer = (TZrNativeString) constantString->stringDataExtend;
    } else {
        constantString = (SZrString *) ZrCore_RawObject_New(state, ZR_VALUE_TYPE_STRING, totalSize, ZR_TRUE);
        if (constantString == ZR_NULL) {
            return ZR_NULL;
        }
        /* RawObject_New has already registered this object with GC. Establish
         * a destructible long-string state before the fallible raw allocation;
         * GC destruction explicitly permits a NULL long payload. */
        constantString->shortStringLength = ZR_VM_LONG_STRING_FLAG;
        constantString->longStringLength = length;
        constantString->longString = ZR_NULL;
        constantString->longString =
                (TZrNativeString)ZrCore_Memory_RawMallocWithType(global, length + 1, ZR_MEMORY_NATIVE_TYPE_STRING);
        if (constantString->longString == ZR_NULL) {
            return ZR_NULL;
        }

        ZrCore_Memory_RawCopy(constantString->longString, string, length);

        constantString->longString[length] = '\0';
        stringBuffer = constantString->longString;
    }

    ZrCore_RawObject_InitHash(ZR_CAST_RAW_OBJECT_AS_SUPER(constantString),
                        hash == 0 ? ZrCore_Hash_Create(global, stringBuffer, length) : hash);
    string_trace("string object create length=%llu string=%p object=%p hash=%llu short=%d",
                 (unsigned long long)length,
                 (const void *)string,
                 (void *)constantString,
                 (unsigned long long)ZR_CAST_RAW_OBJECT_AS_SUPER(constantString)->hash,
                 length <= ZR_VM_SHORT_STRING_MAX ? 1 : 0);
    return constantString;
}

typedef struct SZrShortStringInsertion {
    SZrStringTable *table;
    SZrString *candidate;
    SZrHashKeyValuePair *result;
} SZrShortStringInsertion;

static void string_insert_short_run(SZrState *state, TZrPtr arguments) {
    SZrShortStringInsertion *context = arguments;
    context->result = ZrCore_HashSet_AddRawObject(state, &context->table->stringHashSet,
                                               &context->candidate->super);
}

/* Rollback removes only this candidate; normalization may prepend strings. */
static void string_unlink_short_candidate(SZrStringTable *table, SZrString *candidate) {
    SZrString **link = &table->shortStringListHead;
    while (*link != ZR_NULL && *link != candidate) {
        link = &(*link)->nextShortString;
    }
    if (*link == candidate) {
        *link = candidate->nextShortString;
    }
    candidate->nextShortString = ZR_NULL;
    /* Candidate remains GC-owned; unlinking does not authorize raw free. */
}

/* 按内容驻留短串；新对象须先进入哈希表，再登记供 minor GC 扫描的桶。 */
static SZrString *string_create_short(SZrState *state, TZrNativeString string, TZrSize length) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = global->stringTable;
    TZrUInt64 hash = ZrCore_Hash_Create(global, string, length);
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(&stringTable->stringHashSet, hash);
    ZR_ASSERT(string != ZR_NULL);
    for (; object != ZR_NULL; object = object->next) {
        ZR_ASSERT(object->key.type == ZR_VALUE_TYPE_STRING);
        SZrRawObject *rawObject = ZrCore_Value_GetRawObject(&object->key);
        SZrString *stringObject = ZR_CAST_STRING(state, rawObject);
        // we customized string compare function for speed
        if (stringObject->shortStringLength == length &&
            ZrCore_Memory_RawCompare(ZrCore_String_GetNativeStringShort(stringObject), string, length * sizeof(TZrChar)) == 0) {
            if (ZrCore_RawObject_IsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject))) {
                ZrCore_RawObject_MarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
            }
            return stringObject;
        }
    }
    {
        // create a new string
        SZrString *newString = string_object_create(state, string, length, hash);
        TZrSize previousCapacity;
        SZrGcRootLease lease = {0};

        if (newString == ZR_NULL) {
            string_trace("string create short object create failed length=%llu", (unsigned long long)length);
            return ZR_NULL;
        }
        /* The intern table is weak, including during OOM recovery in AddRawObject. */
        if (!ZrCore_Gc_RootLeaseAcquire(state, &newString->super, &lease)) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
            return ZR_NULL;
        }
        previousCapacity = stringTable->stringHashSet.capacity;
        newString->nextShortString = stringTable->shortStringListHead;
        stringTable->shortStringListHead = newString;
        string_trace("string create short add raw object start string=%p hash=%llu tableBuckets=%p",
                     (void *)newString,
                     (unsigned long long)hash,
                     (void *)stringTable->stringHashSet.buckets);
        SZrShortStringInsertion insertion = {stringTable, newString, ZR_NULL};
        EZrThreadStatus status = ZrCore_Exception_TryRun(state, string_insert_short_run, &insertion);
        if (status != ZR_THREAD_STATUS_FINE || insertion.result == ZR_NULL) {
            string_unlink_short_candidate(stringTable, newString);
            ZrCore_Gc_RootLeaseRelease(global, &lease);
            string_trace("string create short add raw object failed string=%p threadStatus=%d",
                         (void *)newString,
                         state != ZR_NULL ? (int)state->threadStatus : -1);
            if (status != ZR_THREAD_STATUS_FINE) {
                ZrCore_Exception_Throw(state, status);
            }
            return ZR_NULL;
        }
        if (previousCapacity != stringTable->stringHashSet.capacity) {
            (void)string_table_rebuild_minor_young_bucket_flags(global, stringTable);
        }
        string_table_record_young_short_string(global, newString);
        ZrCore_Gc_RootLeaseRelease(global, &lease);
        string_trace("string create short add raw object done string=%p elementCount=%llu",
                     (void *)newString,
                     (unsigned long long)stringTable->stringHashSet.elementCount);
        return newString;
    }
}

/* 长串不参与短串驻留，结果归属 GC 对象及其独立原生字节缓冲区。 */
static ZR_FORCE_INLINE SZrString *string_create_long(SZrState *state, TZrNativeString string, TZrSize length) {
    ZR_ASSERT(string != ZR_NULL);
    SZrString *newString = string_object_create(state, string, length, 0);
    return newString;
}

/* 执行器双字符串拼接快路径；短结果按输入身份缓存，长结果单独创建。 */
SZrString *ZrCore_String_ConcatPair(SZrState *state, const SZrString *left, const SZrString *right) {
    SZrGlobalState *global;
    TZrSize leftLength;
    TZrSize rightLength;
    TZrSize totalLength;
    TZrNativeString leftNative;
    TZrNativeString rightNative;
    SZrString *cachedResult;

    if (state == ZR_NULL || left == ZR_NULL || right == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    cachedResult = string_concat_pair_cache_lookup(state, left, right);
    if (cachedResult != ZR_NULL) {
        return cachedResult;
    }

    leftLength = ZrCore_String_GetByteLength(left);
    rightLength = ZrCore_String_GetByteLength(right);
    totalLength = leftLength + rightLength;
    leftNative = ZrCore_String_GetNativeString(left);
    rightNative = ZrCore_String_GetNativeString(right);

    if (totalLength <= ZR_VM_SHORT_STRING_MAX) {
        TZrChar stackBuffer[ZR_VM_SHORT_STRING_MAX + 1];
        SZrString *result;

        if (leftLength > 0 && leftNative != ZR_NULL) {
            memcpy(stackBuffer, leftNative, leftLength);
        }
        if (rightLength > 0 && rightNative != ZR_NULL) {
            memcpy(stackBuffer + leftLength, rightNative, rightLength);
        }
        stackBuffer[totalLength] = '\0';
        result = string_create_short(state, stackBuffer, totalLength);
        string_concat_pair_cache_store(state, left, right, result);
        return result;
    }

    global = state->global;
    {
        TZrNativeString buffer =
                (TZrNativeString)ZrCore_Memory_RawMallocWithType(global, totalLength + 1, ZR_MEMORY_NATIVE_TYPE_STRING);
        SZrString *result;

        if (buffer == ZR_NULL) {
            return ZR_NULL;
        }

        if (leftLength > 0 && leftNative != ZR_NULL) {
            memcpy(buffer, leftNative, leftLength);
        }
        if (rightLength > 0 && rightNative != ZR_NULL) {
            memcpy(buffer + leftLength, rightNative, rightLength);
        }
        buffer[totalLength] = '\0';

        result = string_create_long(state, buffer, totalLength);
        ZrCore_Memory_RawFreeWithType(global, buffer, totalLength + 1, ZR_MEMORY_NATIVE_TYPE_STRING);
        return result;
    }
}

/* 字符串和借用原生片段拼接，内部先复制片段以跨越可能发生的 GC 分配。 */
SZrString *ZrCore_String_ConcatStringAndNative(SZrState *state,
                                               const SZrString *stringValue,
                                               TZrNativeString nativeString,
                                               TZrSize nativeLength,
                                               TZrBool stringOnLeft) {
    TZrNativeString stringNative;
    TZrSize stringLength;

    if (state == ZR_NULL || stringValue == ZR_NULL || (nativeString == ZR_NULL && nativeLength > 0)) {
        return ZR_NULL;
    }

    stringNative = ZrCore_String_GetNativeString(stringValue);
    stringLength = ZrCore_String_GetByteLength(stringValue);
    return stringOnLeft ? string_create_native_concat_segments(state, stringNative, stringLength, nativeString, nativeLength)
                        : string_create_native_concat_segments(state, nativeString, nativeLength, stringNative, stringLength);
}


/* 字节长度决定驻留布局；调用方保留输入缓冲区所有权，结果由 GC 持有。 */
SZrString *ZrCore_String_Create(SZrState *state, TZrNativeString string, TZrSize length) {
    string_trace("string create dispatch length=%llu text=%p", (unsigned long long)length, (const void *)string);
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        return string_create_short(state, string, length);
    }
    {
        // create a long string
        return string_create_long(state, string, length);
    }
}

#if defined(ZR_DEBUG)
/* TODO: 首次 Debug trace 把环境开关缓存到进程级 static；核 GlobalState_New 并发首建约束并用 fixture/TSAN 验证，未串行时再定 once 同步。 */
static TZrBool string_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_CORE_BOOTSTRAP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

/* 调试诊断写 stderr；仅接收内部固定格式字符串。 */
static void string_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!string_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-string] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
#endif

/* 项目导入、运行时 worker 与 native binding 的 NUL 文本缓存入口；桶索引按输入地址散列。 */
SZrString *ZrCore_String_CreateTryHitCache(SZrState *state, TZrNativeString string) {
    SZrGlobalState *global = state->global;
    TZrUInt64 addressHash = ZR_CAST_UINT64(string) % ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT;
    SZrString **apiCache = global->stringHashApiCache[addressHash];
    SZrString *createdString;
    for (TZrSize i = 0; i < ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH; i++) {
        if (ZrCore_NativeString_Compare(ZrCore_String_GetNativeString(apiCache[i]), string) == 0) {
            return apiCache[i];
        }
    }
    /* Creation can return NULL or throw. Keep every existing cache entry and
     * its order until a valid replacement is available. */
    createdString = ZrCore_String_Create(state, string, ZrCore_NativeString_Length(string));
    if (createdString == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize i = ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH - 1; i > 0; i--) {
        apiCache[i] = apiCache[i - 1];
    }
    apiCache[0] = createdString;
    return createdString;
}

/* UTF-8 验证和码点计数的 VM 字符串适配层；输入长度取真实字节数。 */
TZrBool ZrCore_String_GetCodePointLength(const SZrString *string, TZrSize *outLength) {
    TZrNativeString nativeString;
    TZrSize byteLength;

    if (outLength == ZR_NULL || string == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeString = ZrCore_String_GetNativeString(string);
    byteLength = ZrCore_String_GetByteLength(string);
    return ZrCore_Utf8_CountCodePoints(nativeString, byteLength, outLength);
}

/* 为按码点切片的调用方求边界，非法 UTF-8 或越界交由 UTF-8 层返回失败。 */
TZrBool ZrCore_String_CodePointCountToByteOffset(const SZrString *string,
                                                 TZrSize codePointCount,
                                                 TZrSize *outOffset) {
    TZrNativeString nativeString;
    TZrSize byteLength;

    if (outOffset == ZR_NULL || string == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeString = ZrCore_String_GetNativeString(string);
    byteLength = ZrCore_String_GetByteLength(string);
    return ZrCore_Utf8_CodePointCountToByteOffset(nativeString, byteLength, codePointCount, outOffset);
}

/* 标准库/调试器的 UTF-8 字节数组转换；只把有效 UTF-8 复制进新 VM 数组。 */
TZrBool ZrCore_String_ToByteArray(SZrState *state,
                                  const SZrString *string,
                                  SZrObject **outArray) {
    SZrObject *array;
    TZrNativeString nativeString;
    TZrSize byteLength;
    const TZrUInt8 *bytes;

    if (outArray != ZR_NULL) {
        *outArray = ZR_NULL;
    }

    if (state == ZR_NULL || string == ZR_NULL || outArray == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeString = ZrCore_String_GetNativeString(string);
    byteLength = ZrCore_String_GetByteLength(string);
    if (!ZrCore_Utf8_IsValid(nativeString, byteLength)) {
        return ZR_FALSE;
    }

    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, array);
    bytes = (const TZrUInt8 *)nativeString;
    for (TZrSize index = 0; index < byteLength; index++) {
        SZrTypeValue indexValue;
        SZrTypeValue elementValue;

        ZrCore_Value_InitAsInt(state, &indexValue, (TZrInt64)index);
        ZrCore_Value_InitAsUInt(state, &elementValue, (TZrUInt64)bytes[index]);
        if (!ZrCore_Object_TrySetValue(state, array, &indexValue, &elementValue)) {
            return ZR_FALSE;
        }
    }

    *outArray = array;
    return ZR_TRUE;
}

/* 哈希桶/运行时共享内容比较；哈希相同时仍比较布局、字节长度和原始字节。 */
TZrBool ZrCore_String_Equal(SZrString *string1, SZrString *string2) {
    if (string1 == string2) {
        return ZR_TRUE;
    }
    if (string1->super.hash != string2->super.hash) {
        return ZR_FALSE;
    }
    if (string1->shortStringLength != string2->shortStringLength) {
        return ZR_FALSE;
    }
    if (string1->shortStringLength == ZR_VM_LONG_STRING_FLAG) {
        // this is a long string
        if (string1->longStringLength != string2->longStringLength) {
            return ZR_FALSE;
        }
        return ZrCore_Memory_RawCompare(*ZrCore_String_GetNativeStringLong(string1), *ZrCore_String_GetNativeStringLong(string2),
                                  string1->longStringLength * sizeof(TZrChar)) == 0;
    }

    // short string
    return ZrCore_Memory_RawCompare(ZrCore_String_GetNativeStringShort(string1), ZrCore_String_GetNativeStringShort(string2),
                              string1->shortStringLength * sizeof(TZrChar)) == 0;
}

/* 栈顶 count 个值必须已是字符串；失败由公共实现折叠为 null。 */
void ZrCore_String_Concat(struct SZrState *state, TZrSize count) {
    zr_string_concat_stack_values(state, count, ZR_FALSE);
}

/* 元方法等调用可先转换非字符串值，再将栈窗口合并。 */
void ZrCore_String_ConcatSafe(struct SZrState *state, TZrSize count) {
    zr_string_concat_stack_values(state, count, ZR_TRUE);
}
