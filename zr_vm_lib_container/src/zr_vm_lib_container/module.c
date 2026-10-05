/** @file
 * @brief zr.container 的静态反射契约与原生容器回调。
 * @details Array/Map/Set 使用隐藏原生数组，LinkedList 使用 GC 节点；迭代器引用源而非快照。
 * 注册表按下方 method/meta 表创建闭包，直接下标派发另消费 Map 的两条内联回调。
 * 此文件不提供并发容器协议；process 静态缓存按 VM 身份重置，调用者仍须遵守域和派发约束。
 */
//
// Built-in zr.container module and runtime callbacks.
//

#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_iteration/module.h"
#include "contiguous_view.h"
#include "pooling.h"

#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

/* Array/Map/Set 的公开长度字段与隐藏 backing 分离；迭代器则通过隐藏 source
 * 维持 backing 的可达性。字段名也是热槽选择键，修改时须同步下方缓存映射。
 */
/* Array 包装对象的隐藏 backing 名称；不能改成公开 length。 */
static const TZrChar *kContainerItemsField = "__zr_items";
/* Map/Set 共享的隐藏 backing 名称。 */
static const TZrChar *kContainerEntriesField = "__zr_entries";
/* Enumerator 保活 backing 或链表实例的隐藏引用名。 */
static const TZrChar *kContainerSourceField = "__zr_source";
/* 数组 Enumerator 的下一读取位置字段名。 */
static const TZrChar *kContainerIndexField = "__zr_index";
/* 链表 Enumerator 的下一节点字段名。 */
static const TZrChar *kContainerNextNodeField = "__zr_nextNode";
/* Map/Set/LinkedList 可见元素数元数据名。 */
static const TZrChar *kContainerCountField = "count";
/* Array 可见长度及索引契约角色名。 */
static const TZrChar *kContainerLengthField = "length";
/* Array 的可见增长容量名，不等同宿主实际分配量。 */
static const TZrChar *kContainerCapacityField = "capacity";
/* Enumerator.current 协议槽名。 */
static const TZrChar *kContainerCurrentField = "current";
/* LinkedNode 持有元素值的字段名。 */
static const TZrChar *kContainerValueField = "value";
/* LinkedNode 后继链接字段名。 */
static const TZrChar *kContainerNextField = "next";
/* LinkedNode 前驱链接字段名。 */
static const TZrChar *kContainerPreviousField = "previous";
/* LinkedList 首节点字段名，与 Pair.first 文本相同。 */
static const TZrChar *kContainerFirstField = "first";
/* LinkedList 尾节点字段名，映射容量角色热槽。 */
static const TZrChar *kContainerLastField = "last";
/* Pair 的首分量字段名，映射长度角色热槽。 */
static const TZrChar *kContainerPairFirstField = "first";
/* Pair 的次分量字段名，映射容量角色热槽。 */
static const TZrChar *kContainerPairSecondField = "second";
/* 固定小缓存上限；不是容器元素数或 Map 桶容量，修改须同步数组及轮换取模。 */

enum {
    ZR_CONTAINER_FIELD_CACHE_CAPACITY = 48,
    ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT = 4
};

/** @brief 其他字段名缓存项；名称借用宿主文本，字符串经 permanent 标记属于 VM 域。 */
typedef struct ZrContainerFieldStringCacheEntry {
    /** @brief 借用且稳定的 C 名称文本；缓存匹配时仍会读取其内容。 */
    const TZrChar *fieldName;
    /** @brief 本域永久字段字符串，换域丢弃借用指针。 */
    SZrString *fieldString;
} ZrContainerFieldStringCacheEntry;

/** @brief 当前 VM 域的永久字段名指针集合；换域必须先清空，不能保活容器实例。 */
typedef struct ZrContainerHotFieldStringCache {
    /** @brief 区分 VM GlobalState 生命周期，防止跨域复用已销毁字符串。 */
    TZrUInt64 cacheIdentity;
    /** @brief 延迟创建的 items 角色永久字符串；身份切换时置空。 */
    SZrString *itemsFieldString;
    /** @brief 延迟创建的 entries 角色永久字符串；身份切换时置空。 */
    SZrString *entriesFieldString;
    /** @brief 延迟创建的 pairFirst 角色永久字符串；身份切换时置空。 */
    SZrString *pairFirstFieldString;
    /** @brief 延迟创建的 pairSecond 角色永久字符串；身份切换时置空。 */
    SZrString *pairSecondFieldString;
    /** @brief 延迟创建的 iteratorSource 角色永久字符串；身份切换时置空。 */
    SZrString *iteratorSourceFieldString;
    /** @brief 延迟创建的 iteratorCurrent 角色永久字符串；身份切换时置空。 */
    SZrString *iteratorCurrentFieldString;
    /** @brief 延迟创建的 iteratorIndex 角色永久字符串；身份切换时置空。 */
    SZrString *iteratorIndexFieldString;
    /** @brief 延迟创建的 iteratorNextNode 角色永久字符串；身份切换时置空。 */
    SZrString *iteratorNextNodeFieldString;
} ZrContainerHotFieldStringCache;

/** @brief 一次成功字符串键查找的借用位置；只在 backing 与成员版本匹配时使用。 */
typedef struct ZrContainerHotMapLookupCacheSlot {
    /** @brief 借用字符串对象身份；不是内容哈希，也不额外保活键。 */
    SZrRawObject *keyObject;
    /** @brief 记录 entries 结构版本，改变位置后旧槽不可命中。 */
    TZrUInt32 entriesMemberVersion;
    /** @brief 成功扫描得到的 entries 下标，未命中不读取。 */
    TZrSize index;
    /** @brief 对应 Pair 借用指针，依赖 backing 的可达性和结构稳定。 */
    SZrObject *entryObject;
} ZrContainerHotMapLookupCacheSlot;

/* Map 字符串键热路径只缓存 entries 中已有 Pair 的位置；同一 entries 的成员版本
 * 变化后，旧位置与 Pair 指针都不能再用于查找。
 */
/** @brief 进程静态字符串键热缓存；最近槽与四槽轮换不形成容器/Pairs 的独立 GC 根。 */
typedef struct ZrContainerHotMapLookupCache {
    /** @brief 本缓存的 VM 域身份，变化时清零所有借用状态。 */
    TZrUInt64 cacheIdentity;
    /** @brief 当前 Map backing 借用对象；切换 backing 清除全部候选槽。 */
    SZrObject *entries;
    /** @brief 最近成功查找的字符串对象身份，不缓存未找到结果。 */
    SZrRawObject *lastKeyObject;
    /** @brief 最近命中时的 backing 版本，命中前重新比较。 */
    TZrUInt32 lastEntriesMemberVersion;
    /** @brief 最近命中的数组位置，不独立证明 Pair 仍具有原键。 */
    TZrSize lastIndex;
    /** @brief 最近命中的借用 Pair，不作为 GC root。 */
    SZrObject *lastEntryObject;
    /** @brief 四槽循环替换游标；更新后按槽数取模。 */
    TZrUInt8 nextSlotIndex;
    /** @brief 成功查找的小候选集合，与 last 快槽使用同一版本约定。 */
    ZrContainerHotMapLookupCacheSlot slots[ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT];
} ZrContainerHotMapLookupCache;

#if defined(ZR_DEBUG)
/* 仅 DEBUG 的过程统计；entryValidationReadCount 本文件未递增，不能当实测验证次数。 */
static ZrVmLibContainerDebugHotMapLookupStats gZrContainerDebugHotMapLookupStats = {0};
#endif

static TZrInt64 zr_container_array_iterator_move_next_native(SZrState *state);
static TZrInt64 zr_container_linked_list_iterator_move_next_native(SZrState *state);
static ZR_FORCE_INLINE TZrBool zr_container_array_raw_int_active(const SZrObject *array);
static ZR_FORCE_INLINE SZrObject *zr_container_array_get_object_fast(SZrState *state,
                                                                     SZrObject *array,
                                                                     TZrSize index);
static ZR_FORCE_INLINE SZrHashKeyValuePair **zr_container_hot_field_pair_slot(SZrObject *object,
                                                                               const TZrChar *fieldName);
static ZR_FORCE_INLINE TZrBool zr_container_try_set_existing_pair_value_plain_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   SZrHashKeyValuePair *pair,
                                                                                   const SZrTypeValue *value);
static ZR_FORCE_INLINE void zr_container_refresh_cached_field_slot(SZrState *state,
                                                                   SZrObject *object,
                                                                   SZrString *fieldString,
                                                                   SZrHashKeyValuePair **hotPairSlot);
static ZR_FORCE_INLINE TZrBool zr_container_pair_matches_field_string(SZrState *state,
                                                                       SZrHashKeyValuePair *pair,
                                                                       SZrString *fieldString);
static ZR_FORCE_INLINE void zr_container_cache_string_lookup_pair_mru(SZrObject *object,
                                                                       SZrHashKeyValuePair *pair);

/** @brief 比较字段名指针或文本；空指针只可参与相同地址判断，不传给 strcmp。 */
static ZR_FORCE_INLINE TZrBool zr_container_field_name_equals(const TZrChar *fieldName, const TZrChar *expectedFieldName) {
    return fieldName == expectedFieldName ||
           (fieldName != ZR_NULL && expectedFieldName != ZR_NULL && strcmp(fieldName, expectedFieldName) == 0);
}

/** @brief 重置 DEBUG 专用热缓存统计；非 DEBUG 构建不操作状态，也不清除真实查找缓存。 */
void ZrVmLibContainer_Debug_ResetHotMapLookupStats(void) {
#if defined(ZR_DEBUG)
    memset(&gZrContainerDebugHotMapLookupStats, 0, sizeof(gZrContainerDebugHotMapLookupStats));
#endif
}

/** @brief 按值读取 DEBUG 统计快照；非 DEBUG 返回全零，此接口不能代替功能正确性检查。 */
ZrVmLibContainerDebugHotMapLookupStats ZrVmLibContainer_Debug_GetHotMapLookupStats(void) {
#if defined(ZR_DEBUG)
    return gZrContainerDebugHotMapLookupStats;
#else
    ZrVmLibContainerDebugHotMapLookupStats stats = {0};
    return stats;
#endif
}

/** @brief 首次创建并永久标记字段名字符串，再发布借用指针；缓存槽须属于当前 VM 域，失败不填槽。 */
static SZrString *zr_container_cache_field_string_once(SZrState *state,
                                                       const TZrChar *fieldName,
                                                       SZrString **slot) {
    SZrString *fieldString;

    if (state == ZR_NULL || fieldName == ZR_NULL || slot == ZR_NULL) {
        return ZR_NULL;
    }

    if (*slot != ZR_NULL) {
        return *slot;
    }

    fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    *slot = fieldString;
    return fieldString;
}

/* TODO: 核实多个 GlobalState 并行使用容器方法的支持范围。字段字符串、
 * Map 查找和迭代器原型均有进程级可写缓存，仅按 cacheIdentity 顺序失效；
 * 需并行双 GlobalState 测试确认隔离与同步契约。
 */
/** @brief 按 GlobalState cacheIdentity 切换进程静态字段名缓存；固定热名独立存槽，其他字面名称最多占 48 槽。借用 fieldName 文本须保持有效。 */
static SZrString *zr_container_cached_field_string(SZrState *state, const TZrChar *fieldName) {
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static TZrUInt64 cachedGlobalCacheIdentity = 0;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static ZrContainerFieldStringCacheEntry cache[ZR_CONTAINER_FIELD_CACHE_CAPACITY];
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedItemsFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedEntriesFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedCountFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedLengthFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedCapacityFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedValueFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedNextFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedPreviousFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedLastFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedPairFirstFieldString = ZR_NULL;
    /* 当前 zr_container_cached_field_string 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static SZrString *cachedPairSecondFieldString = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    if (cachedGlobalCacheIdentity != state->global->cacheIdentity) {
        cachedGlobalCacheIdentity = state->global->cacheIdentity;
        memset(cache, 0, sizeof(cache));
        cachedItemsFieldString = ZR_NULL;
        cachedEntriesFieldString = ZR_NULL;
        cachedCountFieldString = ZR_NULL;
        cachedLengthFieldString = ZR_NULL;
        cachedCapacityFieldString = ZR_NULL;
        cachedValueFieldString = ZR_NULL;
        cachedNextFieldString = ZR_NULL;
        cachedPreviousFieldString = ZR_NULL;
        cachedLastFieldString = ZR_NULL;
        cachedPairFirstFieldString = ZR_NULL;
        cachedPairSecondFieldString = ZR_NULL;
    }

    if (fieldName == kContainerItemsField) {
        return zr_container_cache_field_string_once(state, kContainerItemsField, &cachedItemsFieldString);
    }
    if (fieldName == kContainerEntriesField) {
        return zr_container_cache_field_string_once(state, kContainerEntriesField, &cachedEntriesFieldString);
    }
    if (fieldName == kContainerCountField) {
        return zr_container_cache_field_string_once(state, kContainerCountField, &cachedCountFieldString);
    }
    if (fieldName == kContainerLengthField) {
        return zr_container_cache_field_string_once(state, kContainerLengthField, &cachedLengthFieldString);
    }
    if (fieldName == kContainerCapacityField) {
        return zr_container_cache_field_string_once(state, kContainerCapacityField, &cachedCapacityFieldString);
    }
    if (fieldName == kContainerValueField) {
        return zr_container_cache_field_string_once(state, kContainerValueField, &cachedValueFieldString);
    }
    if (fieldName == kContainerNextField) {
        return zr_container_cache_field_string_once(state, kContainerNextField, &cachedNextFieldString);
    }
    if (fieldName == kContainerPreviousField) {
        return zr_container_cache_field_string_once(state, kContainerPreviousField, &cachedPreviousFieldString);
    }
    if (fieldName == kContainerLastField) {
        return zr_container_cache_field_string_once(state, kContainerLastField, &cachedLastFieldString);
    }
    if (fieldName == kContainerFirstField) {
        return zr_container_cache_field_string_once(state, kContainerPairFirstField, &cachedPairFirstFieldString);
    }
    if (fieldName == kContainerPairFirstField) {
        return zr_container_cache_field_string_once(state, kContainerPairFirstField, &cachedPairFirstFieldString);
    }
    if (fieldName == kContainerPairSecondField) {
        return zr_container_cache_field_string_once(state, kContainerPairSecondField, &cachedPairSecondFieldString);
    }

    if (zr_container_field_name_equals(fieldName, kContainerItemsField)) {
        return zr_container_cache_field_string_once(state, kContainerItemsField, &cachedItemsFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerEntriesField)) {
        return zr_container_cache_field_string_once(state, kContainerEntriesField, &cachedEntriesFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerCountField)) {
        return zr_container_cache_field_string_once(state, kContainerCountField, &cachedCountFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerLengthField)) {
        return zr_container_cache_field_string_once(state, kContainerLengthField, &cachedLengthFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerCapacityField)) {
        return zr_container_cache_field_string_once(state, kContainerCapacityField, &cachedCapacityFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerValueField)) {
        return zr_container_cache_field_string_once(state, kContainerValueField, &cachedValueFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerNextField)) {
        return zr_container_cache_field_string_once(state, kContainerNextField, &cachedNextFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPreviousField)) {
        return zr_container_cache_field_string_once(state, kContainerPreviousField, &cachedPreviousFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerLastField)) {
        return zr_container_cache_field_string_once(state, kContainerLastField, &cachedLastFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPairFirstField)) {
        return zr_container_cache_field_string_once(state, kContainerPairFirstField, &cachedPairFirstFieldString);
    }
    if (zr_container_field_name_equals(fieldName, kContainerPairSecondField)) {
        return zr_container_cache_field_string_once(state, kContainerPairSecondField, &cachedPairSecondFieldString);
    }

    for (TZrSize index = 0; index < ZR_CONTAINER_FIELD_CACHE_CAPACITY; index++) {
        if (cache[index].fieldName != ZR_NULL && strcmp(cache[index].fieldName, fieldName) == 0) {
            return cache[index].fieldString;
        }
    }

    for (TZrSize index = 0; index < ZR_CONTAINER_FIELD_CACHE_CAPACITY; index++) {
        SZrString *fieldString;

        if (cache[index].fieldName != ZR_NULL) {
            continue;
        }

        fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
        if (fieldString == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));

        cache[index].fieldName = fieldName;
        cache[index].fieldString = fieldString;
        return fieldString;
    }

    ZR_ASSERT(ZR_FALSE);
    return ZR_NULL;
}

/** @brief 把缓存的永久字段名封装成普通字符串键；取名失败返回 false，调用方不得使用未初始化的 key。 */
static TZrBool zr_container_make_field_key(SZrState *state, const TZrChar *fieldName, SZrTypeValue *outKey) {
    SZrString *fieldString;

    if (state == ZR_NULL || fieldName == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    outKey->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

/** @brief 返回当前 VM 域的热名缓存；域身份改变时清空全部裸指针，本函数不为它们建立独立 GC 根。 */
static ZR_FORCE_INLINE ZrContainerHotFieldStringCache *zr_container_hot_field_string_cache(SZrState *state) {
    /* 当前 zr_container_hot_field_string_cache 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static ZrContainerHotFieldStringCache cache = {0};

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache.cacheIdentity != state->global->cacheIdentity) {
        memset(&cache, 0, sizeof(cache));
        cache.cacheIdentity = state->global->cacheIdentity;
    }

    return &cache;
}

/** @brief 返回当前域的 Map 热查找状态；身份变化会清零上次命中和四个候选槽，状态在同进程调用之间共享。 */
static ZR_FORCE_INLINE ZrContainerHotMapLookupCache *zr_container_hot_map_lookup_cache(SZrState *state) {
    /* 当前 zr_container_hot_map_lookup_cache 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static ZrContainerHotMapLookupCache cache = {0};

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    if (cache.cacheIdentity != state->global->cacheIdentity) {
        memset(&cache, 0, sizeof(cache));
        cache.cacheIdentity = state->global->cacheIdentity;
    }

    return &cache;
}

/** @brief 把热 Map 状态切换到指定 entries；先丢弃全部旧位置和 Pair 借用指针，再允许在新 backing 上命中。 */
/* 切换 backing 时旧 Pair/下标不可延续；该清零并不执行容器元素析构。 */
static ZR_FORCE_INLINE void zr_container_reset_hot_map_lookup_cache_state(ZrContainerHotMapLookupCache *cache,
                                                                          SZrObject *entries) {
    if (cache == ZR_NULL) {
        return;
    }

    cache->entries = entries;
    cache->lastKeyObject = ZR_NULL;
    cache->lastEntriesMemberVersion = 0u;
    cache->lastIndex = 0u;
    cache->lastEntryObject = ZR_NULL;
    cache->nextSlotIndex = 0u;
    memset(cache->slots, 0, sizeof(cache->slots));
}

/** @brief 把成功位置写入最近命中状态及可选输出指针；输出 Pair 仅借用，不复制或接管 entry。 */
static ZR_FORCE_INLINE void zr_container_commit_hot_map_lookup_cache_entry_hit(ZrContainerHotMapLookupCache *cache,
                                                                               SZrRawObject *keyObject,
                                                                               TZrUInt32 entriesMemberVersion,
                                                                               TZrSize index,
                                                                               SZrObject *entryObject,
                                                                               TZrSize *outIndex,
                                                                               SZrObject **outEntryObject) {
    ZR_ASSERT(cache != ZR_NULL);
    ZR_ASSERT(keyObject != ZR_NULL);
    ZR_ASSERT(entryObject != ZR_NULL);

    cache->lastKeyObject = keyObject;
    cache->lastEntriesMemberVersion = entriesMemberVersion;
    cache->lastIndex = index;
    cache->lastEntryObject = entryObject;
#if defined(ZR_DEBUG)
    gZrContainerDebugHotMapLookupStats.hotHitCount++;
    gZrContainerDebugHotMapLookupStats.memberVersionHitCount++;
#endif
    if (outIndex != ZR_NULL) {
        *outIndex = index;
    }
    if (outEntryObject != ZR_NULL) {
        *outEntryObject = entryObject;
    }
}

/** @brief 复用最近一次命中的位置和 Pair；DEBUG 统计仅记录命中，不代表核验了 Pair 字段内容。 */
static ZR_FORCE_INLINE void zr_container_commit_hot_map_lookup_cache_hit(ZrContainerHotMapLookupCache *cache,
                                                                         const ZrContainerHotMapLookupCacheSlot *slot,
                                                                         TZrSize *outIndex,
                                                                         SZrObject **outEntryObject) {
    ZR_ASSERT(slot != ZR_NULL);
    zr_container_commit_hot_map_lookup_cache_entry_hit(cache,
                                                       slot->keyObject,
                                                       slot->entriesMemberVersion,
                                                       slot->index,
                                                       slot->entryObject,
                                                       outIndex,
                                                       outEntryObject);
}

/** @brief 只接受相同字符串对象和相同 entries 成员版本的槽；该验证不重新比较 Pair.first 的当前内容。 */
static ZR_FORCE_INLINE TZrBool zr_container_try_hot_map_lookup_cache_slot(
        ZrContainerHotMapLookupCache *cache,
        const ZrContainerHotMapLookupCacheSlot *slot,
        SZrRawObject *keyObject,
        TZrUInt32 entriesMemberVersion,
        TZrSize *outIndex,
        SZrObject **outEntryObject) {
    if (cache == ZR_NULL || slot == ZR_NULL || slot->keyObject != keyObject ||
        slot->entriesMemberVersion != entriesMemberVersion || slot->entryObject == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_commit_hot_map_lookup_cache_hit(cache, slot, outIndex, outEntryObject);
    return ZR_TRUE;
}

/** @brief 保存成功找到的字符串键 Pair；更新已有槽或轮换四槽，NULL entry 不作为负查找缓存。 */
static ZR_FORCE_INLINE void zr_container_update_hot_map_lookup_cache(SZrState *state,
                                                                     SZrObject *entries,
                                                                     SZrRawObject *keyObject,
                                                                     TZrSize index,
                                                                     SZrObject *entryObject) {
    ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);
    ZrContainerHotMapLookupCacheSlot slotValue = {0};
    TZrSize insertIndex;
    TZrUInt32 entriesMemberVersion;

    if (cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL || entryObject == ZR_NULL) {
        return;
    }

    entriesMemberVersion = entries->memberVersion;

    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
    }

    cache->lastKeyObject = keyObject;
    cache->lastEntriesMemberVersion = entriesMemberVersion;
    cache->lastIndex = index;
    cache->lastEntryObject = entryObject;

    slotValue.keyObject = keyObject;
    slotValue.entriesMemberVersion = entriesMemberVersion;
    slotValue.index = index;
    slotValue.entryObject = entryObject;

    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        if (cache->slots[slotIndex].keyObject == keyObject &&
            cache->slots[slotIndex].entriesMemberVersion == entriesMemberVersion) {
            cache->slots[slotIndex] = slotValue;
            return;
        }
    }

    insertIndex = ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT;
    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        if (cache->slots[slotIndex].keyObject == ZR_NULL ||
            cache->slots[slotIndex].entryObject == ZR_NULL) {
            insertIndex = slotIndex;
            break;
        }
    }

    if (insertIndex == ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT) {
        insertIndex = cache->nextSlotIndex % ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT;
    }
    cache->slots[insertIndex] = slotValue;
    cache->nextSlotIndex = (TZrUInt8)((insertIndex + 1u) % ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT);
}

/** @brief 尝试返回字符串键的缓存下标；先核验域和 backing，再检查最近命中及四槽，失败交给真实扫描。 */
static ZR_FORCE_INLINE TZrBool zr_container_try_hot_map_lookup_cache(SZrState *state,
                                                                     ZrContainerHotMapLookupCache *cache,
                                                                     SZrObject *entries,
                                                                     SZrRawObject *keyObject,
                                                                     TZrSize *outIndex,
                                                                     SZrObject **outEntryObject) {
    TZrUInt32 entriesMemberVersion;

    if (state == ZR_NULL || cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL) {
        return ZR_FALSE;
    }

    entriesMemberVersion = entries->memberVersion;

    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
        return ZR_FALSE;
    }

    if (cache->lastKeyObject == keyObject &&
        cache->lastEntryObject != ZR_NULL &&
        cache->lastEntriesMemberVersion == entriesMemberVersion) {
        ZrContainerHotMapLookupCacheSlot lastSlot = {
                .keyObject = cache->lastKeyObject,
                .entriesMemberVersion = cache->lastEntriesMemberVersion,
                .index = cache->lastIndex,
                .entryObject = cache->lastEntryObject};

        zr_container_commit_hot_map_lookup_cache_hit(cache, &lastSlot, outIndex, outEntryObject);
        return ZR_TRUE;
    }

    if (zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[0], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[1], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[2], keyObject, entriesMemberVersion, outIndex, outEntryObject) ||
        zr_container_try_hot_map_lookup_cache_slot(
                cache, &cache->slots[3], keyObject, entriesMemberVersion, outIndex, outEntryObject)) {
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/** @brief 尝试直接返回缓存 Pair 借用指针；成员版本不符时回退扫描，调用者不能跨失效操作保留此指针。 */
static ZR_FORCE_INLINE SZrObject *zr_container_try_hot_map_lookup_cache_entry(ZrContainerHotMapLookupCache *cache,
                                                                              SZrObject *entries,
                                                                              SZrRawObject *keyObject) {
    TZrUInt32 entriesMemberVersion;

    if (cache == ZR_NULL || entries == ZR_NULL || keyObject == ZR_NULL) {
        return ZR_NULL;
    }

    entriesMemberVersion = entries->memberVersion;

    if (cache->entries != entries) {
        zr_container_reset_hot_map_lookup_cache_state(cache, entries);
        return ZR_NULL;
    }

    if (cache->lastKeyObject == keyObject &&
        cache->lastEntryObject != ZR_NULL &&
        cache->lastEntriesMemberVersion == entriesMemberVersion) {
        zr_container_commit_hot_map_lookup_cache_entry_hit(cache,
                                                           keyObject,
                                                           entriesMemberVersion,
                                                           cache->lastIndex,
                                                           cache->lastEntryObject,
                                                           ZR_NULL,
                                                           ZR_NULL);
        return cache->lastEntryObject;
    }

    for (TZrSize slotIndex = 0; slotIndex < ZR_CONTAINER_HOT_MAP_LOOKUP_CACHE_SLOT_COUNT; slotIndex++) {
        const ZrContainerHotMapLookupCacheSlot *slot = &cache->slots[slotIndex];

        if (slot->keyObject != keyObject ||
            slot->entriesMemberVersion != entriesMemberVersion ||
            slot->entryObject == ZR_NULL) {
            continue;
        }

        zr_container_commit_hot_map_lookup_cache_entry_hit(cache,
                                                           keyObject,
                                                           entriesMemberVersion,
                                                           slot->index,
                                                           slot->entryObject,
                                                           ZR_NULL,
                                                           ZR_NULL);
        return slot->entryObject;
    }

    return ZR_NULL;
}

/** @brief 延迟取得隐藏 entries 键；指针归永久字符串缓存所有，非 Map 内容的 GC 根。 */
static ZR_FORCE_INLINE SZrString *zr_container_entries_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerEntriesField, &cache->entriesFieldString)
                            : ZR_NULL;
}

/** @brief 延迟取得隐藏 items 键，用于 backing 热槽查找；失败以 NULL 交给调用者处理。 */
static ZR_FORCE_INLINE SZrString *zr_container_items_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerItemsField, &cache->itemsFieldString)
                            : ZR_NULL;
}

/** @brief 取得 Pair.first 的永久字符串键；同名 LinkedList.first 共享文本而不共享对象槽。 */
static ZR_FORCE_INLINE SZrString *zr_container_pair_first_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerPairFirstField, &cache->pairFirstFieldString)
                            : ZR_NULL;
}

/** @brief 取得 Pair.second 的永久字符串键；供辅助构造的 Pair 字段热路径复用。 */
static ZR_FORCE_INLINE SZrString *zr_container_pair_second_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerPairSecondField, &cache->pairSecondFieldString)
                            : ZR_NULL;
}

/** @brief 取得迭代器 source 键；该字段的对象值保活 backing，键缓存本身不保活 backing。 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_source_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerSourceField, &cache->iteratorSourceFieldString)
                            : ZR_NULL;
}

/** @brief 取得 Enumerator.current 协议字段名；与运行时原型的 currentMemberName 文本一致。 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_current_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerCurrentField, &cache->iteratorCurrentFieldString)
                            : ZR_NULL;
}

/** @brief 取得数组迭代器的隐藏 index 键；返回缓存借用字符串，不改变游标。 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_index_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state, kContainerIndexField, &cache->iteratorIndexFieldString)
                            : ZR_NULL;
}

/** @brief 取得链表迭代器 nextNode 键；游标节点由实例字段保持可达。 */
static ZR_FORCE_INLINE SZrString *zr_container_iterator_next_node_field_string_fast(SZrState *state) {
    ZrContainerHotFieldStringCache *cache = zr_container_hot_field_string_cache(state);

    return cache != ZR_NULL ? zr_container_cache_field_string_once(state,
                                                                  kContainerNextNodeField,
                                                                  &cache->iteratorNextNodeFieldString)
                            : ZR_NULL;
}

/* getIterator 返回的对象必须有 Iterator 协议和原生 moveNext 契约，才能被 VM
 * 的统一迭代路径驱动。原型与闭包在所属 GlobalState 存续期间被永久标记。
 */
/** @brief 按域缓存数组/链表 Enumerator 原型，装配 current 字段及 native moveNext 协议；原型和闭包永久标记，实例由 GC 管理。 */
/* TODO：复核同一 VM 多 mutator/多域交错调用的静态缓存同步，以及 moving GC 后热 Map 裸指针更新；入口为 hot_*_cache、gc_cycle 对象 rewrite 与 native 派发 GC 模式。 */
static SZrObjectPrototype *zr_container_iterator_runtime_prototype(SZrState *state,
                                                                   FZrNativeFunction moveNextFunction) {
    /* 当前 zr_container_iterator_runtime_prototype 的跨调用缓存状态；身份重置仅维护域隔离，不提供线程同步。 */
    static TZrUInt64 cachedGlobalCacheIdentity = 0;
    /* 本域永久 Enumerator 原型；实例及 source 引用不归此缓存所有。 */
    static SZrObjectPrototype *arrayIteratorPrototype = ZR_NULL;
    /* 本域永久 Enumerator 原型；实例及 source 引用不归此缓存所有。 */
    static SZrObjectPrototype *linkedIteratorPrototype = ZR_NULL;
    /* 本域永久 current 协议字符串，与原型缓存一起换域重置。 */
    static SZrString *currentMemberName = ZR_NULL;
    SZrObjectPrototype **slot;

    if (state == ZR_NULL || state->global == ZR_NULL || moveNextFunction == ZR_NULL) {
        return ZR_NULL;
    }

    if (cachedGlobalCacheIdentity != state->global->cacheIdentity) {
        cachedGlobalCacheIdentity = state->global->cacheIdentity;
        arrayIteratorPrototype = ZR_NULL;
        linkedIteratorPrototype = ZR_NULL;
        currentMemberName = ZR_NULL;
    }

    if (moveNextFunction == zr_container_array_iterator_move_next_native) {
        slot = &arrayIteratorPrototype;
    } else if (moveNextFunction == zr_container_linked_list_iterator_move_next_native) {
        slot = &linkedIteratorPrototype;
    } else {
        return ZR_NULL;
    }

    if (*slot == ZR_NULL) {
        SZrString *prototypeName;
        SZrClosureNative *closure;
        SZrIteratorContract contract;

        prototypeName = ZrCore_String_CreateFromNative(state,
                                                       moveNextFunction == zr_container_array_iterator_move_next_native
                                                               ? "__zr_container_array_iterator"
                                                               : "__zr_container_linked_iterator");
        if (prototypeName == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototypeName));

        if (currentMemberName == ZR_NULL) {
            currentMemberName = ZrCore_String_CreateFromNative(state, "current");
            if (currentMemberName == ZR_NULL) {
                return ZR_NULL;
            }
            ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentMemberName));
        }

        /* TODO：核对原型槽先发布后装配闭包/协议的异常恢复边界；ClosureNative_New 当前
         * 分配后直接解引用返回对象，不能仅凭下方 NULL 分支认定可恢复 OOM 或完整原型重试。 */
        *slot = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        if (*slot == ZR_NULL) {
            return ZR_NULL;
        }
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(*slot));

        closure = ZrCore_ClosureNative_New(state, 0);
        if (closure == ZR_NULL) {
            return ZR_NULL;
        }
        closure->nativeFunction = moveNextFunction;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

        memset(&contract, 0, sizeof(contract));
        contract.moveNextFunction = ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
        contract.currentMemberName = currentMemberName;
        ZrCore_ObjectPrototype_SetIteratorContract(*slot, &contract);
        ZrCore_ObjectPrototype_AddProtocol(*slot, ZR_PROTOCOL_ID_ITERATOR);
    }

    return *slot;
}

/** @brief 从当前绑定接收值借用对象；仅检查 OBJECT/ARRAY 标签，不自行证明 ownerPrototype 或泛型契约。 */
static SZrObject *zr_container_self_object(ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

/** @brief 验证对象确为绑定 ownerPrototype 的实例；构造器据此决定是否复用现有接收者。 */
static TZrBool zr_container_object_is_owner_instance(const ZrLibCallContext *context, SZrObject *object) {
    SZrObjectPrototype *ownerPrototype = ZrLib_CallContext_OwnerPrototype(context);
    return ownerPrototype != ZR_NULL && object != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(object, ownerPrototype);
}

/** @brief 保留合法接收者的派生原型，否则按构造目标/owner 原型新建对象；返回值尚未发布给 result。 */
static SZrObject *zr_container_resolve_construct_target(ZrLibCallContext *context) {
    SZrObject *self;
    SZrObjectPrototype *targetPrototype;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    targetPrototype = ZrLib_CallContext_GetConstructTargetPrototype(context);
    self = zr_container_self_object(context);
    if (self != ZR_NULL &&
        ((targetPrototype != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(self, targetPrototype)) ||
         zr_container_object_is_owner_instance(context, self))) {
        return self;
    }

    if (targetPrototype == ZR_NULL) {
        targetPrototype = ZrLib_CallContext_OwnerPrototype(context);
    }
    return ZrLib_Type_NewInstanceWithPrototype(context->state, targetPrototype);
}

/** @brief 按对象内部数组标记选择 ARRAY 或 OBJECT 值标签，避免把 backing 数组当普通对象封装。 */
static EZrValueType zr_container_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY : ZR_VALUE_TYPE_OBJECT;
}

/** @brief 初始化普通无 ownership 的 GC 对象值；调用方必须已处理旧目标的 ownership，且对象属于当前域。 */
static ZR_FORCE_INLINE void zr_container_value_set_object_fast(SZrState *state,
                                                               SZrTypeValue *value,
                                                               SZrObject *object,
                                                               EZrValueType type) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(object != ZR_NULL);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = type;
}

/** @brief 将调用方已完成初始化的构造对象写入结果；只检查 context/result/object 非空，不额外验证线程状态或回滚初始化。 */
static TZrBool zr_container_finish_object(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object) {
    if (context == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_value_set_object_fast(context->state, result, object, zr_container_value_type_for_object(object));
    return ZR_TRUE;
}

/** @brief 封装缓存字段键并调用核心对象 setter；成功后刷新字段热槽与隐藏 backing 快指针，异常不报告字段设置成功。 */
static TZrBool zr_container_set_value_field_fast(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName,
                                                 const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrString *fieldString;
    SZrHashKeyValuePair **hotPairSlot;
    TZrBool isHiddenItemsField;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL ||
        !zr_container_make_field_key(state, fieldName, &key)) {
        return ZR_FALSE;
    }

    fieldString = ZR_CAST_STRING(state, key.value.object);
    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    ZrCore_Object_SetValue(state, object, &key, value);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    if (hotPairSlot != ZR_NULL && fieldString != ZR_NULL) {
        zr_container_refresh_cached_field_slot(state, object, fieldString, hotPairSlot);
    }

    isHiddenItemsField = zr_container_field_name_equals(fieldName, kContainerItemsField) ||
                         zr_container_field_name_equals(fieldName, kContainerEntriesField);
    if (isHiddenItemsField) {
        object->cachedHiddenItemsObject = ZR_NULL;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
            SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

            if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                object->cachedHiddenItemsObject = hiddenItemsObject;
            }
        }
    }
    return ZR_TRUE;
}

/** @brief 写容器计数/游标整数，已有热槽直接改写；这些内部标量槽必须仍为普通无 ownership 值。 */
/* TODO：核对公开可写元数据或动态字段是否可把内部整数槽改成 ownership 值；直写 int/null 路径不释放旧 owner，核查字段类型/动态写入口再决定是否需要回退。 */
static TZrBool zr_container_set_int_field_fast(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               TZrInt64 value) {
    SZrHashKeyValuePair **hotPairSlot;
    SZrString *fieldString;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    fieldString = zr_container_cached_field_string(state, fieldName);
    if (hotPairSlot != ZR_NULL &&
        fieldString != ZR_NULL &&
        zr_container_pair_matches_field_string(state, *hotPairSlot, fieldString)) {
        ZR_VALUE_FAST_SET(&(*hotPairSlot)->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
        object->memberVersion++;
        zr_container_cache_string_lookup_pair_mru(object, *hotPairSlot);
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

/** @brief 将内部标量/链接槽置空；已有热槽直接重置，调用者须保证旧槽不需 ownership 释放。 */
static TZrBool zr_container_set_null_field_fast(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrHashKeyValuePair **hotPairSlot;
    SZrString *fieldString;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    fieldString = zr_container_cached_field_string(state, fieldName);
    if (hotPairSlot != ZR_NULL &&
        fieldString != ZR_NULL &&
        zr_container_pair_matches_field_string(state, *hotPairSlot, fieldString)) {
        ZrCore_Value_ResetAsNullNoProfile(&(*hotPairSlot)->value);
        object->memberVersion++;
        zr_container_cache_string_lookup_pair_mru(object, *hotPairSlot);
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

/** @brief 封装借用 GC 对象再写字段；持久可达性由目标对象字段与写屏障建立，不由局部裸指针建立。 */
static TZrBool zr_container_set_object_field_fast(SZrState *state,
                                                  SZrObject *object,
                                                  const TZrChar *fieldName,
                                                  SZrObject *valueObject) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (valueObject == ZR_NULL) {
        return zr_container_set_null_field_fast(state, object, fieldName);
    }

    zr_container_value_set_object_fast(state, &fieldValue, valueObject, zr_container_value_type_for_object(valueObject));
    return zr_container_set_value_field_fast(state, object, fieldName, &fieldValue);
}

/** @brief 以永久字段名读取对象值；NULL 表示键创建失败或字段不存在，返回槽在结构修改后不可继续借用。 */
static const SZrTypeValue *zr_container_get_field_value(SZrState *state,
                                                        SZrObject *object,
                                                        const TZrChar *fieldName) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL ||
        !zr_container_make_field_key(state, fieldName, &key)) {
        return ZR_NULL;
    }
    return ZrCore_Object_GetValue(state, object, &key);
}

/** @brief 按字段角色选择对象内的专用缓存槽；同一个槽承载不同类型的同角色名称，使用前必须验证键。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair **zr_container_hot_field_pair_slot(SZrObject *object,
                                                                               const TZrChar *fieldName) {
    if (object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    if (fieldName == kContainerItemsField || fieldName == kContainerEntriesField) {
        return &object->cachedHiddenItemsPair;
    }
    if (fieldName == kContainerLengthField ||
        fieldName == kContainerCountField ||
        fieldName == kContainerFirstField ||
        fieldName == kContainerPairFirstField) {
        return &object->cachedLengthPair;
    }
    if (fieldName == kContainerCapacityField ||
        fieldName == kContainerLastField ||
        fieldName == kContainerPairSecondField) {
        return &object->cachedCapacityPair;
    }
    if (fieldName == kContainerSourceField) {
        return &object->cachedIteratorSourcePair;
    }
    if (fieldName == kContainerCurrentField) {
        return &object->cachedIteratorCurrentPair;
    }
    if (fieldName == kContainerIndexField) {
        return &object->cachedIteratorIndexPair;
    }
    if (fieldName == kContainerNextNodeField) {
        return &object->cachedIteratorNextNodePair;
    }
    if (fieldName == kContainerValueField ||
        fieldName == kContainerNextField ||
        fieldName == kContainerPreviousField) {
        return ZR_NULL;
    }

    if (zr_container_field_name_equals(fieldName, kContainerItemsField) ||
        zr_container_field_name_equals(fieldName, kContainerEntriesField)) {
        return &object->cachedHiddenItemsPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerLengthField) ||
        zr_container_field_name_equals(fieldName, kContainerCountField) ||
        zr_container_field_name_equals(fieldName, kContainerPairFirstField)) {
        return &object->cachedLengthPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerCapacityField) ||
        zr_container_field_name_equals(fieldName, kContainerLastField) ||
        zr_container_field_name_equals(fieldName, kContainerPairSecondField)) {
        return &object->cachedCapacityPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerSourceField)) {
        return &object->cachedIteratorSourcePair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerCurrentField)) {
        return &object->cachedIteratorCurrentPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerIndexField)) {
        return &object->cachedIteratorIndexPair;
    }
    if (zr_container_field_name_equals(fieldName, kContainerNextNodeField)) {
        return &object->cachedIteratorNextNodePair;
    }

    return ZR_NULL;
}

/** @brief 仅查本对象哈希桶中的相同字符串对象键；不沿原型链，也不进行字符串内容相等回退。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_string_pair_exact(SZrObject *object,
                                                                                     SZrString *fieldString) {
    SZrHashKeyValuePair *pair;
    TZrUInt64 hash;

    if (object == ZR_NULL || fieldString == ZR_NULL || !object->nodeMap.isValid || object->nodeMap.buckets == ZR_NULL ||
        object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    hash = ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)->hash;
    for (pair = ZrCore_HashSet_GetBucket(&object->nodeMap, hash); pair != ZR_NULL; pair = pair->next) {
        if (pair->key.type == ZR_VALUE_TYPE_STRING &&
            pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
            return pair;
        }
    }

    return ZR_NULL;
}

/** @brief 核验缓存节点的字符串键身份或内容；不以缓存槽非空代替字段身份验证。 */
static ZR_FORCE_INLINE TZrBool zr_container_pair_matches_field_string(SZrState *state,
                                                                      SZrHashKeyValuePair *pair,
                                                                      SZrString *fieldString) {
    SZrString *pairKeyString;

    if (state == ZR_NULL || pair == ZR_NULL || fieldString == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING ||
        pair->key.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        return ZR_TRUE;
    }

    pairKeyString = ZR_CAST_STRING(state, pair->key.value.object);
    return pairKeyString != ZR_NULL && ZrCore_String_Equal(pairKeyString, fieldString);
}

/** @brief 验证候选缓存节点后输出；只有精确字符串身份匹配才走最短读取分支。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_try_match_cached_field_pair_exact(SZrHashKeyValuePair *pair,
                                                                                            SZrString *fieldString) {
    if (pair == ZR_NULL || fieldString == ZR_NULL || pair->key.type != ZR_VALUE_TYPE_STRING ||
        pair->key.value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        return ZR_NULL;
    }

    return pair;
}

/** @brief 从对应热槽尝试借用字段值；缓存键不匹配即失败，调用方继续常规查找。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_try_get_hot_field_value_exact(SZrState *state,
                                                                                      SZrObject *object,
                                                                                      const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrHashKeyValuePair **hotPairSlot;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    if (hotPairSlot == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    pair = hotPairSlot != ZR_NULL ? zr_container_try_match_cached_field_pair_exact(*hotPairSlot, fieldString) : ZR_NULL;
    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

/** @brief 将确认的自有字符串字段提升到对象最近查找槽；仅重排缓存，不改变成员版本或键值。 */
static ZR_FORCE_INLINE void zr_container_cache_string_lookup_pair_mru(SZrObject *object, SZrHashKeyValuePair *pair) {
    if (object == ZR_NULL) {
        return;
    }

    if (pair == ZR_NULL) {
        object->cachedStringLookupPair = ZR_NULL;
        object->cachedStringLookupPair2 = ZR_NULL;
        return;
    }

    if (object->cachedStringLookupPair == pair) {
        return;
    }
    if (object->cachedStringLookupPair2 == pair) {
        object->cachedStringLookupPair2 = object->cachedStringLookupPair;
        object->cachedStringLookupPair = pair;
        return;
    }
    object->cachedStringLookupPair2 = object->cachedStringLookupPair;
    object->cachedStringLookupPair = pair;
}

/* Pair 的已存在字段走非拥有值快写；只有双方值均满足 normalized/no-ownership
 * 条件才跳过通用 setter，GC 值仍须对持有对象执行写屏障。
 */
/** @brief 仅在源和旧目标都无 ownership 时直接复制字段并写屏障；否则返回 false 让通用 setter 执行覆盖释放。 */
static ZR_FORCE_INLINE TZrBool zr_container_try_set_existing_pair_value_plain_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   SZrHashKeyValuePair *pair,
                                                                                   const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || pair == ZR_NULL || value == ZR_NULL ||
        !ZrCore_Value_HasNormalizedNoOwnership(&pair->value) ||
        !ZrCore_Value_HasNormalizedNoOwnership(value)) {
        return ZR_FALSE;
    }

    if (&pair->value != value) {
        pair->value = *value;
    }
    if (value->isGarbageCollectable) {
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->value);
    }
    zr_container_cache_string_lookup_pair_mru(object, pair);
    return ZR_TRUE;
}

/* 字段热槽和最近使用缓存只加速查找；未命中时仍回到对象自己的 nodeMap 核实键。 */
/** @brief 结合专用槽、最近查找和桶查找获取自有字段节点；可更新缓存，返回值依赖对象 nodeMap 的当前结构。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_cached_field_pair_fast(
        SZrState *state,
        SZrObject *object,
        SZrString *fieldString,
        SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.buckets == ZR_NULL || object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    pair = (hotPairSlot != ZR_NULL) ? *hotPairSlot : ZR_NULL;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        return pair;
    }

    pair = object->cachedStringLookupPair;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = object->cachedStringLookupPair2;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        zr_container_cache_string_lookup_pair_mru(object, pair);
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = zr_container_find_own_string_pair_exact(object, fieldString);
    if (pair == ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
        key.type = ZR_VALUE_TYPE_STRING;
        pair = ZrCore_HashSet_Find(state, &object->nodeMap, &key);
    }

    zr_container_cache_string_lookup_pair_mru(object, pair);
    if (hotPairSlot != ZR_NULL) {
        *hotPairSlot = pair;
    }
    return pair;
}

/** @brief 借用已核验的自有缓存字段值；不沿原型继承，NULL 交由调用方选择回退。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_get_cached_field_value_fast(
        SZrState *state,
        SZrObject *object,
        SZrString *fieldString,
        SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, hotPairSlot);

    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

/** @brief 结构写入后只重新查找字段并刷新 pair 缓存槽；隐藏 backing 对象快指针由调用方另行同步。 */
static ZR_FORCE_INLINE void zr_container_refresh_cached_field_slot(SZrState *state,
                                                                   SZrObject *object,
                                                                   SZrString *fieldString,
                                                                   SZrHashKeyValuePair **hotPairSlot) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || hotPairSlot == ZR_NULL) {
        return;
    }

    pair = object->cachedStringLookupPair;
    if (!zr_container_pair_matches_field_string(state, pair, fieldString)) {
        pair = object->cachedStringLookupPair2;
        if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
            zr_container_cache_string_lookup_pair_mru(object, pair);
        } else {
            pair = zr_container_find_own_string_pair_exact(object, fieldString);
            zr_container_cache_string_lookup_pair_mru(object, pair);
        }
    }
    *hotPairSlot = pair;
}

/* 字段更新需同步 memberVersion 与隐藏数组缓存，使 Map 热查找和迭代源看到新状态。 */
/** @brief 按缓存键更新字段并维护 memberVersion/隐藏 backing 缓存；缺字段走通用插入，线程异常时返回 false。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_cached_field_value_fast(SZrState *state,
                                                                        SZrObject *object,
                                                                        SZrString *fieldString,
                                                                        SZrHashKeyValuePair **hotPairSlot,
                                                                        const SZrTypeValue *value,
                                                                        TZrBool refreshHiddenItemsObject) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldString == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, hotPairSlot);
    if (pair != ZR_NULL) {
        if (!refreshHiddenItemsObject &&
            object->cachedHiddenItemsPair == ZR_NULL &&
            object->cachedHiddenItemsObject == ZR_NULL &&
            zr_container_try_set_existing_pair_value_plain_fast(state, object, pair, value)) {
            object->memberVersion++;
            return ZR_TRUE;
        }

        ZrCore_Object_SetExistingPairValueUnchecked(state, object, pair, value);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }

        object->memberVersion++;
        if (refreshHiddenItemsObject) {
            object->cachedHiddenItemsObject = ZR_NULL;
            if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
                SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

                if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                    object->cachedHiddenItemsObject = hiddenItemsObject;
                }
            }
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    zr_container_refresh_cached_field_slot(state, object, fieldString, hotPairSlot);
    if (refreshHiddenItemsObject) {
        object->cachedHiddenItemsObject = ZR_NULL;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
            SZrObject *hiddenItemsObject = ZR_CAST_OBJECT(state, value->value.object);

            if (hiddenItemsObject != ZR_NULL && hiddenItemsObject->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                object->cachedHiddenItemsObject = hiddenItemsObject;
            }
        }
    }
    return ZR_TRUE;
}

/** @brief 以迭代器专用缓存更新普通值；允许通用 setter 回退，避免把待复制 ownership 值作浅拷贝。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_cached_value_fast(SZrState *state,
                                                                           SZrObject *iterator,
                                                                           SZrString *fieldString,
                                                                           SZrHashKeyValuePair **hotPairSlot,
                                                                           const SZrTypeValue *value) {
    return fieldString != ZR_NULL &&
           zr_container_set_cached_field_value_fast(state, iterator, fieldString, hotPairSlot, value, ZR_FALSE);
}

/** @brief 发布迭代器借用 source 对象到实例字段；成功的字段及屏障使 backing 随迭代器保持可达。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_source_object_fast(SZrState *state,
                                                                            SZrObject *iterator,
                                                                            SZrObject *source,
                                                                            EZrValueType sourceType) {
    SZrTypeValue fieldValue;

    if (source == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    } else {
        zr_container_value_set_object_fast(state, &fieldValue, source, sourceType);
    }
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_source_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorSourcePair : ZR_NULL,
                                                       &fieldValue);
}

/** @brief 复制当前元素到 current；元素可携带 GC/ownership，须通过缓存 setter 的普通与通用分支选择。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_value_fast(SZrState *state,
                                                                            SZrObject *iterator,
                                                                            const SZrTypeValue *value) {
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_current_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorCurrentPair : ZR_NULL,
                                                       value);
}

/** @brief 直接写已找到的迭代器整数槽；调用者须保证该槽确为目标内部字段且旧值无 ownership。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_cached_int_pair_fast(SZrObject *iterator,
                                                                              SZrHashKeyValuePair **hotPairSlot,
                                                                              TZrInt64 value) {
    SZrHashKeyValuePair *pair;

    if (iterator == ZR_NULL || hotPairSlot == ZR_NULL || *hotPairSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = *hotPairSlot;
    if (pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(&pair->value, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    iterator->memberVersion++;
    iterator->cachedStringLookupPair = pair;
    return ZR_TRUE;
}

/** @brief 数组 raw-int 迭代直接写 current 整数；只用已初始化的标量字段缓存，失败回退标准整数写入。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_int_fast(SZrState *state,
                                                                          SZrObject *iterator,
                                                                          TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (zr_container_set_iterator_cached_int_pair_fast(iterator,
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorCurrentPair : ZR_NULL,
                                                       value)) {
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_iterator_current_value_fast(state, iterator, &fieldValue);
}

/** @brief 迭代结束时把 current 置空；当前字段的覆盖语义由缓存 setter 完成。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_current_null_fast(SZrState *state, SZrObject *iterator) {
    SZrTypeValue fieldValue;

    ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    return zr_container_set_iterator_current_value_fast(state, iterator, &fieldValue);
}

/** @brief 推进数组迭代游标并维护缓存；index 是实例状态，不捕获 backing 的长度或版本快照。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_index_fast(SZrState *state,
                                                                    SZrObject *iterator,
                                                                    TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (zr_container_set_iterator_cached_int_pair_fast(iterator,
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorIndexPair : ZR_NULL,
                                                       value)) {
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_index_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorIndexPair : ZR_NULL,
                                                       &fieldValue);
}

/** @brief 推进链表迭代游标并保活下一节点；NULL 是结束游标，不释放节点 value 所持资源。 */
static ZR_FORCE_INLINE TZrBool zr_container_set_iterator_next_node_fast(SZrState *state,
                                                                        SZrObject *iterator,
                                                                        SZrObject *nextNode) {
    SZrTypeValue fieldValue;

    if (nextNode == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(&fieldValue);
    } else {
        zr_container_value_set_object_fast(state, &fieldValue, nextNode, ZR_VALUE_TYPE_OBJECT);
    }
    return zr_container_set_iterator_cached_value_fast(state,
                                                       iterator,
                                                       zr_container_iterator_next_node_field_string_fast(state),
                                                       iterator != ZR_NULL ? &iterator->cachedIteratorNextNodePair : ZR_NULL,
                                                       &fieldValue);
}

/** @brief 先使用已核验的热字符串槽，再查自有 nodeMap；继承字段不作为自有节点缓存。 */
static ZR_FORCE_INLINE SZrHashKeyValuePair *zr_container_find_own_field_pair_fast(SZrState *state,
                                                                                   SZrObject *object,
                                                                                   const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrHashKeyValuePair *pair;
    SZrHashKeyValuePair **hotPairSlot;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.buckets == ZR_NULL || object->nodeMap.capacity == 0) {
        return ZR_NULL;
    }

    fieldString = zr_container_cached_field_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    hotPairSlot = zr_container_hot_field_pair_slot(object, fieldName);
    pair = hotPairSlot != ZR_NULL ? *hotPairSlot : ZR_NULL;
    if (zr_container_pair_matches_field_string(state, pair, fieldString)) {
        return pair;
    }

    pair = object->cachedStringLookupPair;
    if (pair != ZR_NULL &&
        pair->key.type == ZR_VALUE_TYPE_STRING &&
        pair->key.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)) {
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    pair = zr_container_find_own_string_pair_exact(object, fieldString);
    if (pair != ZR_NULL) {
        object->cachedStringLookupPair = pair;
        if (hotPairSlot != ZR_NULL) {
            *hotPairSlot = pair;
        }
        return pair;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    pair = ZrCore_HashSet_Find(state, &object->nodeMap, &key);
    object->cachedStringLookupPair = pair;
    if (hotPairSlot != ZR_NULL) {
        *hotPairSlot = pair;
    }
    return pair;
}

/** @brief 优先借用自有字段值，必要时回退对象读；回退可能沿原型查找，名称不保证返回值一定自有。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_get_own_field_value_fast(SZrState *state,
                                                                                  SZrObject *object,
                                                                                  const TZrChar *fieldName) {
    SZrHashKeyValuePair *pair;
    const SZrTypeValue *hotValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    hotValue = zr_container_try_get_hot_field_value_exact(state, object, fieldName);
    if (hotValue != ZR_NULL) {
        return hotValue;
    }

    pair = zr_container_find_own_field_pair_fast(state, object, fieldName);
    if (pair != ZR_NULL) {
        return &pair->value;
    }

    return zr_container_get_field_value(state, object, fieldName);
}

/** @brief 只接受 OBJECT/ARRAY 值并返回其借用对象；字段缺失、空值及其他标签均返回 NULL。 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_object_field_fast(SZrState *state,
                                                                     SZrObject *object,
                                                                     const TZrChar *fieldName) {
    const SZrTypeValue *value = zr_container_get_own_field_value_fast(state, object, fieldName);

    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

/** @brief 读取 Pair.first 的自有缓存槽，未命中回退字段读取；返回借用值，不自动初始化结果目标。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_pair_get_first_fast(SZrState *state, SZrObject *pair) {
    SZrString *fieldString = zr_container_pair_first_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair =
            zr_container_try_match_cached_field_pair_exact(pair != ZR_NULL ? pair->cachedLengthPair : ZR_NULL, fieldString);
    const SZrTypeValue *value =
            cachedPair != ZR_NULL
                    ? &cachedPair->value
                    : zr_container_get_cached_field_value_fast(state,
                                                               pair,
                                                               fieldString,
                                                               pair != ZR_NULL ? &pair->cachedLengthPair : ZR_NULL);

    return value != ZR_NULL ? value : zr_container_get_own_field_value_fast(state, pair, kContainerPairFirstField);
}

/** @brief 读取 Pair.second 的自有缓存槽，未命中回退字段读取；Pair 结构修改后须重新取得值槽。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_pair_get_second_fast(SZrState *state, SZrObject *pair) {
    SZrString *fieldString = zr_container_pair_second_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair =
            zr_container_try_match_cached_field_pair_exact(pair != ZR_NULL ? pair->cachedCapacityPair : ZR_NULL, fieldString);
    const SZrTypeValue *value =
            cachedPair != ZR_NULL
                    ? &cachedPair->value
                    : zr_container_get_cached_field_value_fast(state,
                                                               pair,
                                                               fieldString,
                                                               pair != ZR_NULL ? &pair->cachedCapacityPair : ZR_NULL);

    return value != ZR_NULL ? value : zr_container_get_own_field_value_fast(state, pair, kContainerPairSecondField);
}

/** @brief 更新 Pair.second，普通值走直接屏障分支，ownership 值走对象覆盖路径；成功依据线程状态。 */
static ZR_FORCE_INLINE TZrBool zr_container_pair_set_second_fast(SZrState *state,
                                                                 SZrObject *pair,
                                                                 const SZrTypeValue *value) {
    SZrString *fieldString = zr_container_pair_second_field_string_fast(state);
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || pair == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    cachedPair = zr_container_try_match_cached_field_pair_exact(pair->cachedCapacityPair, fieldString);
    if (cachedPair != ZR_NULL) {
        if (!zr_container_try_set_existing_pair_value_plain_fast(state, pair, cachedPair, value)) {
            ZrCore_Object_SetExistingPairValueUnchecked(state, pair, cachedPair, value);
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return ZR_FALSE;
            }

            pair->cachedStringLookupPair = cachedPair;
        }

        pair->memberVersion++;
        return ZR_TRUE;
    }

    if (fieldString != ZR_NULL &&
        zr_container_set_cached_field_value_fast(state,
                                                 pair,
                                                 fieldString,
                                                 pair != ZR_NULL ? &pair->cachedCapacityPair : ZR_NULL,
                                                 value,
                                                 ZR_FALSE)) {
        return ZR_TRUE;
    }
    return zr_container_set_value_field_fast(state, pair, kContainerPairSecondField, value);
}

/** @brief 读取模块辅助构造 Pair 的已装配 first 槽；快路径依赖该 Pair 的字段形状未被外部替换。 */
/* TODO：核对 guest 取得 Pair 后的结构复制与直接字段写是否可能改变缓存 entry.first 且不改变 entries.memberVersion；不能只从可写描述符断言热缓存已失效。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_map_entry_get_first_value_fast(SZrState *state,
                                                                                        SZrObject *entryObject) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL) {
        return ZR_NULL;
    }

    cachedPair = entryObject->cachedLengthPair;
    if (cachedPair != ZR_NULL) {
        return &cachedPair->value;
    }

    return zr_container_pair_get_first_fast(state, entryObject);
}

/** @brief 读取辅助 Pair 的 second 槽；返回值仅借用到下一次可能改变节点或执行用户回调的操作。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_map_entry_get_second_value_fast(SZrState *state,
                                                                                         SZrObject *entryObject) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL) {
        return ZR_NULL;
    }

    cachedPair = entryObject->cachedCapacityPair;
    if (cachedPair != ZR_NULL) {
        return &cachedPair->value;
    }

    return zr_container_pair_get_second_fast(state, entryObject);
}

/** @brief 更新已有 Map Pair 的值；快路径保留写屏障，非普通 ownership 或缺缓存退回 Pair setter。 */
static ZR_FORCE_INLINE TZrBool zr_container_map_entry_set_second_value_fast(SZrState *state,
                                                                            SZrObject *entryObject,
                                                                            const SZrTypeValue *value) {
    SZrHashKeyValuePair *cachedPair;

    if (state == ZR_NULL || entryObject == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    cachedPair = entryObject->cachedCapacityPair;
    if (cachedPair == ZR_NULL) {
        return zr_container_pair_set_second_fast(state, entryObject, value);
    }

    if (!zr_container_try_set_existing_pair_value_plain_fast(state, entryObject, cachedPair, value)) {
        ZrCore_Object_SetExistingPairValueUnchecked(state, entryObject, cachedPair, value);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }

        entryObject->cachedStringLookupPair = cachedPair;
    }

    entryObject->memberVersion++;
    return ZR_TRUE;
}

/** @brief 按 backing 的当前表示读取元素；raw-int 必要时物化槽，返回借用指针且不证明调用者给出的下标有效。 */
static ZR_FORCE_INLINE const SZrTypeValue *zr_container_array_get_value_fast(SZrState *state,
                                                                             SZrObject *array,
                                                                             TZrSize index) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    if (zr_container_array_raw_int_active(array) &&
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
        return ZR_NULL;
    }

    if (array->nodeMap.isValid && array->nodeMap.buckets != ZR_NULL && index < array->nodeMap.elementCount &&
        index < array->nodeMap.capacity) {
        pair = array->nodeMap.buckets[index];
        if (pair != ZR_NULL &&
            ZR_VALUE_IS_TYPE_SIGNED_INT(pair->key.type) &&
            pair->key.value.nativeObject.nativeInt64 == (TZrInt64)index) {
            return &pair->value;
        }
    }

    return ZrLib_Array_Get(state, array, index);
}

/** @brief 从 backing 元素数取得实际长度；不读取可写的公开 length/count 元数据。 */
static ZR_FORCE_INLINE TZrSize zr_container_array_length_fast(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0u;
    }

    return (array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
            array->superArrayRawIntData != ZR_NULL &&
            array->superArrayRawIntLength <= array->superArrayRawIntCapacity)
                   ? array->superArrayRawIntLength
                   : array->nodeMap.elementCount;
}

/** @brief 判定 canonical raw-int 表示；不能把旧的备用 buffer 非空当作当前有效表示。 */
static ZR_FORCE_INLINE TZrBool zr_container_array_raw_int_active(const SZrObject *array) {
    return array != ZR_NULL &&
           array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
           array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
           array->superArrayRawIntData != ZR_NULL &&
           array->superArrayRawIntLength <= array->superArrayRawIntCapacity;
}

/** @brief 清空桶与池使用计数而保留宿主分配；不逐槽执行 ownership release，调用者不可把它视作逐值析构。 */
/* TODO：核对合法 Array/Set 元素的 ownership 限制；clear/reuse 不遍历 ReleaseValue，需从泛型值复制、ownership 类型约束及 backing 析构入口验证完整资源释放。 */
static void zr_container_hash_set_clear_reuse_storage(SZrHashSet *set) {
    SZrHashPairPoolBlock *block;

    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return;
    }

    memset(set->buckets, 0, set->capacity * sizeof(set->buckets[0]));
    set->elementCount = 0;
    set->pairPoolUsed = 0;
    set->pairPoolActive = set->pairPoolHead;
    for (block = set->pairPoolHead; block != ZR_NULL; block = block->next) {
        block->used = 0;
    }
}

/* 清空元素和对象级热槽但保留已分配的散列表/原始整数缓冲，供下一轮容器操作复用。 */
/** @brief 清除 backing 内容并使稠密缓存/版本失效；raw buffer 可保留复用，公开长度由上层单独重置。 */
static void zr_container_array_clear_items_reuse_storage(SZrObject *items) {
    if (items == ZR_NULL || items->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return;
    }

    zr_container_hash_set_clear_reuse_storage(&items->nodeMap);
    items->cachedHiddenItemsPair = ZR_NULL;
    items->cachedHiddenItemsObject = ZR_NULL;
    items->cachedLengthPair = ZR_NULL;
    items->cachedCapacityPair = ZR_NULL;
    items->cachedStringLookupPair = ZR_NULL;
    items->cachedStringLookupPair2 = ZR_NULL;
    items->cachedIteratorSourcePair = ZR_NULL;
    items->cachedIteratorCurrentPair = ZR_NULL;
    items->cachedIteratorIndexPair = ZR_NULL;
    items->cachedIteratorNextNodePair = ZR_NULL;
    items->superArrayRawIntLength = 0;
    if (items->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
        items->superArrayRawIntData != ZR_NULL) {
        items->superArrayStorageGeneration++;
    } else {
        items->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_NONE;
        items->superArrayStorageGeneration++;
    }
    items->superArrayRawIntDirty = ZR_FALSE;
    items->memberVersion++;
}

/** @brief 只取得 backing 指定位置的 OBJECT 元素；ARRAY 及其他标签返回 NULL，节点值只作借用。 */
static ZR_FORCE_INLINE SZrObject *zr_container_array_get_object_fast(SZrState *state,
                                                                     SZrObject *array,
                                                                     TZrSize index) {
    const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, array, index);

    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

/** @brief 把 Array 包装对象 self 封装成普通接收值，供 SuperArraySetInt 解析其隐藏 backing；临时值不接管 self 的 ownership。 */
static ZR_FORCE_INLINE void zr_container_array_make_receiver_value(SZrState *state,
                                                                   SZrObject *self,
                                                                   SZrTypeValue *outReceiver) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(self != ZR_NULL);
    ZR_ASSERT(outReceiver != ZR_NULL);

    zr_container_value_set_object_fast(state, outReceiver, self, zr_container_value_type_for_object(self));
}

/** @brief 读取整数字段并支持指定 fallback；非整数或缺字段不会作为任意 nativeInt64 解读。 */
static TZrInt64 zr_container_get_int_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrInt64 defaultValue) {
    const SZrTypeValue *value = zr_container_get_own_field_value_fast(state, object, fieldName);

    if (value == ZR_NULL) {
        return defaultValue;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return defaultValue;
}

/** @brief 按名字查找用户方法并经 ZrLib_CallValue 调用；调用层初始化 result，失败/线程错误留给上层处理。 */
static TZrBool zr_container_call_method(SZrState *state,
                                        SZrObject *receiver,
                                        const TZrChar *methodName,
                                        const SZrTypeValue *arguments,
                                        TZrSize argumentCount,
                                        SZrTypeValue *result) {
    const SZrTypeValue *callable;
    SZrTypeValue receiverValue;

    if (state == ZR_NULL || receiver == ZR_NULL || methodName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    callable = zr_container_get_field_value(state, receiver, methodName);
    if (callable == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_value_set_object_fast(state, &receiverValue, receiver, zr_container_value_type_for_object(receiver));
    return ZrLib_CallValue(state, callable, &receiverValue, arguments, argumentCount, result);
}

static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value);

/* Map/Set/Pair 先尝试核心值相等，再回退到对象的 equals 协议。 */
/** @brief 优先直接值相等，再尝试对象 equals；用户回调失败与不相等都返回 false，调用者另看线程状态。 */
static TZrBool zr_container_values_equal(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue lhsCopy;
    SZrTypeValue rhsCopy;
    SZrTypeValue result;

    if (state == ZR_NULL || lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    lhsCopy = *lhs;
    rhsCopy = *rhs;
    if (ZrCore_Value_Equal(state, &lhsCopy, &rhsCopy) || ZrCore_Value_CompareDirectly(state, &lhsCopy, &rhsCopy)) {
        return ZR_TRUE;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "equals", rhs, 1, &result) && result.type == ZR_VALUE_TYPE_BOOL) {
            return (TZrBool)(result.value.nativeObject.nativeBool != 0);
        }
    }

    return ZR_FALSE;
}

/** @brief 比较有符号/无符号整数而不把负数当大 unsigned；非整数返回 false 交给通用相等。 */
static ZR_FORCE_INLINE TZrBool zr_container_int_values_equal_fast(const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    TZrBool lhsSigned;
    TZrBool rhsSigned;

    if (lhs == ZR_NULL || rhs == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(lhs->type) || !ZR_VALUE_IS_TYPE_INT(rhs->type)) {
        return ZR_FALSE;
    }

    lhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(lhs->type);
    rhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(rhs->type);
    if (lhsSigned && rhsSigned) {
        return (TZrBool)(lhs->value.nativeObject.nativeInt64 == rhs->value.nativeObject.nativeInt64);
    }
    if (!lhsSigned && !rhsSigned) {
        return (TZrBool)(lhs->value.nativeObject.nativeUInt64 == rhs->value.nativeObject.nativeUInt64);
    }
    if (lhsSigned) {
        return lhs->value.nativeObject.nativeInt64 >= 0 &&
               (TZrUInt64)lhs->value.nativeObject.nativeInt64 == rhs->value.nativeObject.nativeUInt64;
    }
    return rhs->value.nativeObject.nativeInt64 >= 0 &&
           lhs->value.nativeObject.nativeUInt64 == (TZrUInt64)rhs->value.nativeObject.nativeInt64;
}

/** @brief 将可表示的整数搜索值规范为 int64；超出范围的 unsigned 或非整数拒绝 raw-int 搜索分支。 */
static ZR_FORCE_INLINE TZrBool zr_container_try_read_int64_needle_fast(const SZrTypeValue *value, TZrInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (value->value.nativeObject.nativeUInt64 > (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX) {
        return ZR_FALSE;
    }
    *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

/** @brief 写已由绑定层初始化的普通结果槽；调用者不得借此覆盖仍持有 ownership 的旧结果。 */
static ZR_FORCE_INLINE void zr_container_result_set_int_fast(SZrTypeValue *result, TZrInt64 value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeInt64, value, ZR_VALUE_TYPE_INT64);
    }
}

/** @brief 写普通布尔结果并清除 ownership 标记；接收槽须已经完成旧值释放。 */
static ZR_FORCE_INLINE void zr_container_result_set_bool_fast(SZrTypeValue *result, TZrBool value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeBool, value, ZR_VALUE_TYPE_BOOL);
    }
}

/** @brief 写普通无符号整数结果；这是初始化式写入，不提供旧 ownership 的释放。 */
static ZR_FORCE_INLINE void zr_container_result_set_uint_fast(SZrTypeValue *result, TZrUInt64 value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeUInt64, value, ZR_VALUE_TYPE_UINT64);
    }
}

/** @brief 写普通 double 结果；目标须为绑定层准备的空/普通槽。 */
static ZR_FORCE_INLINE void zr_container_result_set_double_fast(SZrTypeValue *result, TZrDouble value) {
    if (result != ZR_NULL) {
        ZR_VALUE_FAST_SET(result, nativeDouble, value, ZR_VALUE_TYPE_DOUBLE);
    }
}

/** @brief 写普通空结果；有 ownership 的旧目标必须先按覆盖协议释放。 */
static ZR_FORCE_INLINE void zr_container_result_set_null_fast(SZrTypeValue *result) {
    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(result);
    }
}

/** @brief 标量结果走初始化快写，其他值走 CopyNoProfile；后者要求目标已初始化并可执行 ownership 覆盖。 */
static ZR_FORCE_INLINE TZrBool zr_container_result_copy_no_profile(SZrState *state,
                                                                    SZrTypeValue *result,
                                                                    const SZrTypeValue *value) {
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (value == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            zr_container_result_set_null_fast(result);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_BOOL:
            zr_container_result_set_bool_fast(result, value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT64:
            zr_container_result_set_int_fast(result, value->value.nativeObject.nativeInt64);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT64:
            zr_container_result_set_uint_fast(result, value->value.nativeObject.nativeUInt64);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_DOUBLE:
            zr_container_result_set_double_fast(result, value->value.nativeObject.nativeDouble);
            return ZR_TRUE;
        default:
            break;
    }

    ZrCore_Value_CopyNoProfile(state, result, value);
    return state == ZR_NULL || state->threadStatus == ZR_THREAD_STATUS_FINE;
}

/* Pair.compareTo 先比较可解释的字符串、数值及对象协议；无协议时的哈希/类型
 * 顺序只是容器的兜底排序，调用方不能把它当成跨运行实例的稳定序列化顺序。
 */
/** @brief 先判相等，再处理字符串和数字、对象 compareTo，最后以哈希/类型兜底；数字分支存在下方已记录的精度和 unsigned 排序缺陷。 */
static TZrInt64 zr_container_values_compare(SZrState *state, const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    SZrTypeValue result;
    const TZrChar *lhsText;
    const TZrChar *rhsText;
    TZrDouble lhsNumber;
    TZrDouble rhsNumber;
    TZrUInt64 lhsHash;
    TZrUInt64 rhsHash;

    if (lhs == ZR_NULL && rhs == ZR_NULL) {
        return 0;
    }
    if (lhs == ZR_NULL) {
        return -1;
    }
    if (rhs == ZR_NULL) {
        return 1;
    }
    if (zr_container_values_equal(state, lhs, rhs)) {
        return 0;
    }

    if (lhs->type == ZR_VALUE_TYPE_STRING && rhs->type == ZR_VALUE_TYPE_STRING) {
        lhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, lhs->value.object));
        rhsText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, rhs->value.object));
        if (lhsText == ZR_NULL) {
            return rhsText == ZR_NULL ? 0 : -1;
        }
        if (rhsText == ZR_NULL) {
            return 1;
        }
        return strcmp(lhsText, rhsText);
    }

    /* BUG: UInt64 经 nativeInt64 解释后大于 INT64_MAX 的值会变成负数；
     * 64 位整数再转 double 还会使 2^53 与 2^53+1 失去顺序，甚至双向比较均返回 1。
     * Pair.compareTo 通过 kPairMetaMethods 可达；需补齐有符号/无符号边界测试。
     */
    if ((ZR_VALUE_IS_TYPE_INT(lhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(lhs->type) || ZR_VALUE_IS_TYPE_FLOAT(lhs->type)) &&
        (ZR_VALUE_IS_TYPE_INT(rhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rhs->type) || ZR_VALUE_IS_TYPE_FLOAT(rhs->type))) {
        lhsNumber = ZR_VALUE_IS_TYPE_FLOAT(lhs->type) ? lhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)lhs->value.nativeObject.nativeInt64;
        rhsNumber = ZR_VALUE_IS_TYPE_FLOAT(rhs->type) ? rhs->value.nativeObject.nativeDouble
                                                      : (TZrDouble)rhs->value.nativeObject.nativeInt64;
        return lhsNumber < rhsNumber ? -1 : 1;
    }

    if ((lhs->type == ZR_VALUE_TYPE_OBJECT || lhs->type == ZR_VALUE_TYPE_ARRAY) && lhs->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, lhs->value.object);
        if (zr_container_call_method(state, receiver, "compareTo", rhs, 1, &result) &&
            (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type))) {
            return ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)
                           ? result.value.nativeObject.nativeInt64
                           : (TZrInt64)result.value.nativeObject.nativeUInt64;
        }
    }

    lhsHash = zr_container_value_hash(state, lhs);
    rhsHash = zr_container_value_hash(state, rhs);
    if (lhsHash == rhsHash) {
        return (TZrInt64)lhs->type - (TZrInt64)rhs->type;
    }
    return lhsHash < rhsHash ? -1 : 1;
}

/* 容器键遵从对象 hashCode 协议；未提供有效结果时回退核心值哈希。 */
/** @brief 对象可用 hashCode 覆盖核心哈希；非整数回调结果回退核心哈希，调用失败不在此清除线程异常。 */
static TZrUInt64 zr_container_value_hash(SZrState *state, const SZrTypeValue *value) {
    SZrTypeValue result;

    if (state == ZR_NULL || value == ZR_NULL) {
        return 0;
    }

    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL) {
        SZrObject *receiver = ZR_CAST_OBJECT(state, value->value.object);
        if (zr_container_call_method(state, receiver, "hashCode", ZR_NULL, 0, &result)) {
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) {
                return (TZrUInt64)result.value.nativeObject.nativeInt64;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
                return result.value.nativeObject.nativeUInt64;
            }
        }
    }

    return ZrCore_Value_GetHash(state, value);
}

/** @brief 构造整数 key 后对传入 backing 对象调用 Object_SetValue；按线程状态返回，不在本层封装接收值或回滚已发生写入。 */
static TZrBool zr_container_storage_set(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    ZrCore_Object_SetValue(state, array, &key, value);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

/** @brief 先确保 raw buffer 容量再追加有符号整数；更新 raw 长度、表示、generation 与 dirty 标记，本函数不递增 memberVersion。 */
static TZrBool zr_container_storage_push_raw_int_fast(SZrState *state, SZrObject *array, TZrInt64 value) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    if (array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL) {
        return ZR_FALSE;
    }
    if (zr_container_array_raw_int_active(array)) {
        length = array->superArrayRawIntLength;
    } else {
        length = 0;
        if (array->nodeMap.elementCount != 0) {
            return ZR_FALSE;
        }
    }

    if (!ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, array, length + 1) ||
        array->superArrayRawIntData == ZR_NULL) {
        return ZR_FALSE;
    }

    array->superArrayRawIntData[length] = value;
    array->superArrayRawIntLength = length + 1;
    array->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL;
    array->superArrayStorageGeneration++;
    array->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

/* 密集整数键数组可直接取预留 pair；写入 GC 值后仍需屏障与版本递增。 */
/** @brief 为 GC 值建立稠密索引节点并执行屏障；桶/池扩容失败不发布元素，值复制后的异常由线程状态承载。 */
static TZrBool zr_container_storage_push_gc_value_dense_pair_pool_fast(SZrState *state,
                                                                       SZrObject *array,
                                                                       const SZrTypeValue *value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL ||
        !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_FALSE;
    }

    nodeMap = &array->nodeMap;
    if (!nodeMap->isValid || nodeMap->buckets == ZR_NULL || nodeMap->capacity == 0) {
        return ZR_FALSE;
    }

    index = nodeMap->elementCount;
    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, nodeMap, index + 1) ||
        !ZrCore_HashSet_EnsurePairPoolForElementCount(state, nodeMap, nodeMap->pairPoolUsed + 1) ||
        index >= nodeMap->capacity ||
        nodeMap->buckets[index] != ZR_NULL) {
        return ZR_FALSE;
    }

    pair = ZrCore_HashSet_TakeReservedPair(nodeMap);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    pair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, (TZrInt64)index, ZR_VALUE_TYPE_INT64);
    ZrCore_Value_ResetAsNullNoProfile(&pair->value);
    ZrCore_Value_CopyNoProfile(state, &pair->value, value);
    nodeMap->buckets[index] = pair;
    nodeMap->elementCount++;
    array->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL;
    array->superArrayStorageGeneration++;
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(array), &pair->value);
    array->memberVersion++;
    return ZR_TRUE;
}

/** @brief 先尝试有符号整数 raw 追加，其他值按当前 length 用核心 setter 追加；Array.add 另在调用本函数前尝试稠密 GC 快路径。 */
static TZrBool zr_container_storage_push(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) &&
        zr_container_storage_push_raw_int_fast(state, array, value->value.nativeObject.nativeInt64)) {
        return ZR_TRUE;
    }

    return zr_container_storage_set(state, array, zr_container_array_length_fast(array), value);
}

/** @brief raw 分支缩短长度并递增 generation，通用分支只删除末尾整数键节点、不更新数组版本；返回的 key 是普通整数，本层未接管其他 ownership。 */
static TZrBool zr_container_storage_remove_last(SZrState *state, SZrObject *array) {
    SZrTypeValue key;
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (length == 0) {
        return ZR_FALSE;
    }

    if (array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
        array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL) {
        array->superArrayRawIntLength = length - 1;
        array->superArrayStorageGeneration++;
        array->superArrayRawIntDirty = ZR_FALSE;
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)(length - 1));
    ZrCore_HashSet_Remove(state, &array->nodeMap, &key);
    return ZR_TRUE;
}

/** @brief 校验并移除 canonical raw-int 中指定元素，向前 memmove 后缩短长度及更新 generation；不经过值析构。 */
static TZrBool zr_container_storage_remove_at_raw_int_fast(SZrState *state, SZrObject *array, TZrSize index) {
    TZrSize length;

    if (state == ZR_NULL || !zr_container_array_raw_int_active(array)) {
        return ZR_FALSE;
    }

    length = array->superArrayRawIntLength;
    if (index >= length) {
        return ZR_FALSE;
    }

    if (index + 1 < length) {
        TZrSize movedCount = length - index - 1;
        memmove(array->superArrayRawIntData + index,
                array->superArrayRawIntData + index + 1,
                movedCount * sizeof(array->superArrayRawIntData[0]));
    }
    array->superArrayRawIntLength = length - 1;
    array->superArrayStorageGeneration++;
    array->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

/** @brief 先确保 raw 容量再向后移动整数插入；只接受可直接存入 canonical int64 的普通值。 */
static TZrBool zr_container_storage_insert_raw_int_fast(SZrState *state,
                                                        SZrObject *array,
                                                        TZrSize index,
                                                        TZrInt64 value) {
    TZrSize length;

    if (state == ZR_NULL || !zr_container_array_raw_int_active(array)) {
        return ZR_FALSE;
    }

    length = array->superArrayRawIntLength;
    if (index > length ||
        !ZrCore_Object_SuperArrayEnsureRawIntCapacity(state, array, length + 1)) {
        return ZR_FALSE;
    }

    if (index < length) {
        TZrSize movedCount = length - index;
        memmove(array->superArrayRawIntData + index + 1,
                array->superArrayRawIntData + index,
                movedCount * sizeof(array->superArrayRawIntData[0]));
    }
    array->superArrayRawIntData[index] = value;
    array->superArrayRawIntLength = length + 1;
    array->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL;
    array->superArrayStorageGeneration++;
    array->superArrayRawIntDirty = ZR_FALSE;
    return ZR_TRUE;
}

/** @brief raw-int 优先，通用表示从尾向后复制再写新值；中途写失败缺少完整传播/回滚，不能许诺原子插入。 */
static TZrBool zr_container_storage_insert(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (index > length) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(array)) {
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) &&
            zr_container_storage_insert_raw_int_fast(state, array, index, value->value.nativeObject.nativeInt64)) {
            return ZR_TRUE;
        }
        if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
            return ZR_FALSE;
        }
    }

    /* TODO: 后移期间忽略 storage_set 的失败结果；需以注入式写入失败验证
     * Array.insert 是否会留下部分移动的 backing，并明确失败后的状态契约。
     */
    for (TZrSize cursor = length; cursor > index; cursor--) {
        const SZrTypeValue *source = zr_container_array_get_value_fast(state, array, cursor - 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_set(state, array, index, value);
}

/** @brief raw-int 优先，通用表示逐项左移再摘末节点；位移写入未逐次验证，失败不是事务回滚。 */
static TZrBool zr_container_storage_remove_at(SZrState *state, SZrObject *array, TZrSize index) {
    TZrSize length;

    if (state == ZR_NULL || array == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(array);
    if (index >= length) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(array)) {
        return zr_container_storage_remove_at_raw_int_fast(state, array, index);
    }

    /* TODO: 前移期间同样忽略 storage_set 的失败结果；需检查 Array.removeAt、
     * Map.remove、Set.remove 的失败传播及元素顺序/计数是否仍一致。
     */
    for (TZrSize cursor = index; cursor + 1 < length; cursor++) {
        const SZrTypeValue *source = zr_container_array_get_value_fast(state, array, cursor + 1);
        if (source != ZR_NULL) {
            zr_container_storage_set(state, array, cursor, source);
        }
    }

    return zr_container_storage_remove_last(state, array);
}

/** @brief 读取容器自有隐藏 backing，并维护对象的快捷 backing 指针；失败返回 NULL，不为只读操作创建新数组。 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_hidden_array_fast(SZrState *state,
                                                                     SZrObject *object,
                                                                     SZrString *fieldString) {
    SZrHashKeyValuePair *pair;
    SZrObject *array;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }

    array = object->cachedHiddenItemsObject;
    if (array != ZR_NULL && array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return array;
    }

    pair = zr_container_find_own_cached_field_pair_fast(state, object, fieldString, &object->cachedHiddenItemsPair);
    if (pair == ZR_NULL || (pair->value.type != ZR_VALUE_TYPE_OBJECT && pair->value.type != ZR_VALUE_TYPE_ARRAY) ||
        pair->value.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZR_CAST_OBJECT(state, pair->value.value.object);
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        object->cachedHiddenItemsObject = ZR_NULL;
        return ZR_NULL;
    }

    object->cachedHiddenItemsObject = array;
    return array;
}

/** @brief 按 entries 隐藏字段取得 Map/Set backing；返回借用对象，公开 count 不是 backing 有效性的证明。 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_entries_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_get_hidden_array_fast(state, object, zr_container_entries_field_string_fast(state));
}

/** @brief 优先使用包装对象现有 backing 快指针，否则共用 entries 字段读取；不分配 backing。 */
static ZR_FORCE_INLINE SZrObject *zr_container_get_entries_array_cached_fast(SZrState *state, SZrObject *object) {
    SZrObject *array;

    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    array = object->cachedHiddenItemsObject;
    if (array != ZR_NULL && array->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return array;
    }

    return zr_container_get_entries_array_fast(state, object);
}

/** @brief 复用已有合法 backing，否则创建原生数组并写隐藏字段；字段写入失败返回 NULL，GC 对象不由本地 free。 */
static ZR_FORCE_INLINE SZrObject *zr_container_ensure_hidden_array_fast(SZrState *state,
                                                                        SZrObject *object,
                                                                        SZrString *fieldString) {
    SZrObject *array = zr_container_get_hidden_array_fast(state, object, fieldString);

    if (array != ZR_NULL) {
        return array;
    }

    array = ZrLib_Array_New(state);
    if (array != ZR_NULL) {
        SZrTypeValue fieldValue;

        zr_container_value_set_object_fast(state, &fieldValue, array, ZR_VALUE_TYPE_ARRAY);
        if (fieldString == ZR_NULL ||
            !zr_container_set_cached_field_value_fast(state,
                                                      object,
                                                      fieldString,
                                                      &object->cachedHiddenItemsPair,
                                                      &fieldValue,
                                                      ZR_TRUE)) {
            return ZR_NULL;
        }
    }

    return array;
}

/** @brief 确保 Map/Set 的隐藏 entries backing 存在；仅初始化存储，不自动同步公开 count。 */
static ZR_FORCE_INLINE SZrObject *zr_container_ensure_entries_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_ensure_hidden_array_fast(state, object, zr_container_entries_field_string_fast(state));
}

/** @brief 确保 Array 的隐藏 items backing 存在；该引用写入包装对象后才能随包装对象保持可达。 */
static ZR_FORCE_INLINE SZrObject *zr_container_ensure_items_array_fast(SZrState *state, SZrObject *object) {
    return zr_container_ensure_hidden_array_fast(state, object, zr_container_items_field_string_fast(state));
}

/** @brief 按当前模块 Pair 原型新建并依次写 first/second，返回借用对象；任一步失败不返回完成的 Pair。 */
static SZrObject *zr_container_make_pair(SZrState *state, const SZrTypeValue *first, const SZrTypeValue *second) {
    SZrObject *pair = ZrLib_Type_NewInstance(state, "Pair");
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    if (first != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, pair, kContainerPairFirstField, first)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, pair, kContainerPairFirstField)) {
            return ZR_NULL;
        }
    }
    if (second != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, pair, kContainerPairSecondField, second)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, pair, kContainerPairSecondField)) {
            return ZR_NULL;
        }
    }
    return pair;
}

/** @brief 按 LinkedNode 原型新建并初始化 value/next/previous；失败返回 NULL，新对象由 VM GC 管理。 */
static SZrObject *zr_container_make_linked_node(SZrState *state, const SZrTypeValue *value) {
    SZrObject *node = ZrLib_Type_NewInstance(state, "LinkedNode");
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    if (value != ZR_NULL) {
        if (!zr_container_set_value_field_fast(state, node, kContainerValueField, value)) {
            return ZR_NULL;
        }
    } else {
        if (!zr_container_set_null_field_fast(state, node, kContainerValueField)) {
            return ZR_NULL;
        }
    }
    if (!zr_container_set_null_field_fast(state, node, kContainerNextField) ||
        !zr_container_set_null_field_fast(state, node, kContainerPreviousField)) {
        return ZR_NULL;
    }
    return node;
}

/* Map 的字符串键优先按对象身份与版本缓存命中；短字符串由 core 实习化，
 * 长字符串才需要同哈希后的内容比较。其他键仍走 hash 与语言层相等性契约。
 */
/** @brief 字符串键走身份/哈希/内容专用扫描，其他键含整数走通用 hash 与 equals；正命中才缓存字符串位置，回调异常由外层线程状态区分。 */
static TZrBool zr_container_map_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *key,
                                           TZrSize *outIndex,
                                           SZrObject **outEntryObject) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (outEntryObject != ZR_NULL) {
        *outEntryObject = ZR_NULL;
    }
    if (state == ZR_NULL || entries == ZR_NULL || key == ZR_NULL) {
        return ZR_FALSE;
    }

    if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL) {
        SZrRawObject *wantedRawObject = key->value.object;
        SZrString *wantedString = ZR_NULL;

        {
            ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);

            if (zr_container_try_hot_map_lookup_cache(state,
                                                      cache,
                                                      entries,
                                                      wantedRawObject,
                                                      outIndex,
                                                      outEntryObject)) {
                return ZR_TRUE;
            }
        }

        wantedHash = wantedRawObject->hash;
        wantedString = ZR_CAST_STRING(state, wantedRawObject);
        if (wantedString == ZR_NULL) {
            return ZR_FALSE;
        }

        length = zr_container_array_length_fast(entries);
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryKey;
            SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

            if (entryObject == ZR_NULL) {
                continue;
            }

            entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
            if (entryKey == ZR_NULL || entryKey->type != ZR_VALUE_TYPE_STRING || entryKey->value.object == ZR_NULL) {
                continue;
            }
            if (entryKey->value.object == wantedRawObject) {
                if (outIndex != ZR_NULL) {
                    *outIndex = index;
                }
                if (outEntryObject != ZR_NULL) {
                    *outEntryObject = entryObject;
                }
                zr_container_update_hot_map_lookup_cache(state,
                                                         entries,
                                                         wantedRawObject,
                                                         index,
                                                         entryObject);
                return ZR_TRUE;
            }
            if (entryKey->value.object->hash != wantedHash) {
                continue;
            }

            {
                SZrString *entryString = ZR_CAST_STRING(state, entryKey->value.object);

                if (entryString != ZR_NULL &&
                    !ZrCore_String_IsShort(entryString) &&
                    !ZrCore_String_IsShort(wantedString) &&
                    ZrCore_String_Equal(entryString, wantedString)) {
                    if (outIndex != ZR_NULL) {
                        *outIndex = index;
                    }
                    if (outEntryObject != ZR_NULL) {
                        *outEntryObject = entryObject;
                    }
                    zr_container_update_hot_map_lookup_cache(state,
                                                             entries,
                                                             wantedRawObject,
                                                             index,
                                                             entryObject);
                    return ZR_TRUE;
                }
            }
        }

        zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, 0u, ZR_NULL);
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, key);
    length = zr_container_array_length_fast(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryKey;
        SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

        if (entryObject == ZR_NULL) {
            continue;
        }

        entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
        if (entryKey != ZR_NULL && zr_container_value_hash(state, entryKey) == wantedHash &&
            zr_container_values_equal(state, entryKey, key)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            if (outEntryObject != ZR_NULL) {
                *outEntryObject = entryObject;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/** @brief 与下标搜索相同规则直接取得 Pair；返回借用 entry，仅字符串对象键使用版本化缓存。 */
static ZR_FORCE_INLINE SZrObject *zr_container_map_find_entry_object_fast(SZrState *state,
                                                                          SZrObject *entries,
                                                                          const SZrTypeValue *key) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (state == ZR_NULL || entries == ZR_NULL || key == ZR_NULL) {
        return ZR_NULL;
    }

    if (key->type == ZR_VALUE_TYPE_STRING && key->value.object != ZR_NULL) {
        SZrRawObject *wantedRawObject = key->value.object;
        SZrString *wantedString = ZR_NULL;
        SZrObject *entryObject = ZR_NULL;

        {
            ZrContainerHotMapLookupCache *cache = zr_container_hot_map_lookup_cache(state);

            entryObject = zr_container_try_hot_map_lookup_cache_entry(cache, entries, wantedRawObject);
            if (entryObject != ZR_NULL) {
                return entryObject;
            }
        }

        wantedHash = wantedRawObject->hash;
        wantedString = ZR_CAST_STRING(state, wantedRawObject);
        if (wantedString == ZR_NULL) {
            return ZR_NULL;
        }

        length = zr_container_array_length_fast(entries);
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryKey;

            entryObject = zr_container_array_get_object_fast(state, entries, index);
            if (entryObject == ZR_NULL) {
                continue;
            }

            entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
            if (entryKey == ZR_NULL || entryKey->type != ZR_VALUE_TYPE_STRING || entryKey->value.object == ZR_NULL) {
                continue;
            }
            if (entryKey->value.object == wantedRawObject) {
                zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, index, entryObject);
                return entryObject;
            }
            if (entryKey->value.object->hash != wantedHash) {
                continue;
            }

            {
                SZrString *entryString = ZR_CAST_STRING(state, entryKey->value.object);

                if (entryString != ZR_NULL &&
                    !ZrCore_String_IsShort(entryString) &&
                    !ZrCore_String_IsShort(wantedString) &&
                    ZrCore_String_Equal(entryString, wantedString)) {
                    zr_container_update_hot_map_lookup_cache(state, entries, wantedRawObject, index, entryObject);
                    return entryObject;
                }
            }
        }

        return ZR_NULL;
    }

    wantedHash = zr_container_value_hash(state, key);
    length = zr_container_array_length_fast(entries);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryKey;
        SZrObject *entryObject = zr_container_array_get_object_fast(state, entries, index);

        if (entryObject == ZR_NULL) {
            continue;
        }

        entryKey = zr_container_map_entry_get_first_value_fast(state, entryObject);
        if (entryKey != ZR_NULL && zr_container_value_hash(state, entryKey) == wantedHash &&
            zr_container_values_equal(state, entryKey, key)) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

/** @brief 按 backing 的实际长度线性查找，raw-int 用整数语义，普通元素可调用用户 equals；不使用哈希桶按元素键查找。 */
static TZrBool zr_container_set_find_index(SZrState *state,
                                           SZrObject *entries,
                                           const SZrTypeValue *value,
                                           TZrSize *outIndex) {
    TZrUInt64 wantedHash;
    TZrSize length;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (state == ZR_NULL || entries == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(entries);
    if (ZR_VALUE_IS_TYPE_INT(value->type)) {
        if (zr_container_array_raw_int_active(entries)) {
            TZrInt64 signedNeedle;

            if (zr_container_try_read_int64_needle_fast(value, &signedNeedle)) {
                for (TZrSize index = 0; index < length; index++) {
                    if (entries->superArrayRawIntData[index] == signedNeedle) {
                        if (outIndex != ZR_NULL) {
                            *outIndex = index;
                        }
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, entries, index);

            if (zr_container_int_values_equal_fast(entryValue, value)) {
                if (outIndex != ZR_NULL) {
                    *outIndex = index;
                }
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    wantedHash = zr_container_value_hash(state, value);
    for (TZrSize index = 0; index < length; index++) {
        const SZrTypeValue *entryValue = zr_container_array_get_value_fast(state, entries, index);
        if (entryValue != ZR_NULL && zr_container_value_hash(state, entryValue) == wantedHash &&
            zr_container_values_equal(state, entryValue, value)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/** @brief 维护公开 capacity 的倍增策略而非立即保留物理存储；乘法溢出风险见既有 TODO，追加仍须独立确保 backing 空间。 */
static TZrBool zr_container_array_ensure_capacity(SZrState *state, SZrObject *arrayObject, TZrSize requiredLength) {
    TZrInt64 capacity;

    if (state == ZR_NULL || arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    capacity = zr_container_get_int_field(state, arrayObject, kContainerCapacityField, 0);
    if (capacity <= 0) {
        capacity = ZR_CONTAINER_SEQUENCE_INITIAL_CAPACITY;
        if (!zr_container_set_int_field_fast(state, arrayObject, kContainerCapacityField, capacity)) {
            return ZR_FALSE;
        }
    }

    if ((TZrSize)capacity >= requiredLength) {
        return ZR_TRUE;
    }

    /* TODO: 正值扩容倍增仍需检查 TZrInt64 上界，防止溢出。 */
    while ((TZrSize)capacity < requiredLength) {
        capacity *= ZR_CONTAINER_SEQUENCE_GROWTH_FACTOR;
    }
    return zr_container_set_int_field_fast(state, arrayObject, kContainerCapacityField, capacity);
}

/** @brief 为初始容量准备稠密桶及 pair pool；失败返回 false，不单凭公开 capacity 宣称分配成功。 */
static TZrBool zr_container_array_prepare_backing_storage(SZrState *state,
                                                          SZrObject *itemsObject,
                                                          TZrSize requiredCapacity) {
    if (state == ZR_NULL || itemsObject == ZR_NULL) {
        return ZR_FALSE;
    }
    if (requiredCapacity == 0) {
        return ZR_TRUE;
    }

    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, &itemsObject->nodeMap, requiredCapacity)) {
        return ZR_FALSE;
    }

    return ZrCore_HashSet_EnsurePairPoolForElementCount(state, &itemsObject->nodeMap, requiredCapacity);
}

/* 迭代器持有 source 并绑定对应 moveNext 原生闭包，供 Iterable 协议统一驱动。 */
/** @brief 创建 Enumerator 实例并发布 source/current/index/nextNode；实例字段保活 source，不是拷贝容器快照。 */
/* source 发布到实例字段后承担可达性；后续遍历按当前 source 内容进行，不记录长度/version 快照。 */
static SZrObject *zr_container_iterator_make(SZrState *state,
                                             SZrObject *source,
                                             EZrValueType sourceType,
                                             TZrInt64 indexValue,
                                             SZrObject *nextNode,
                                             FZrNativeFunction moveNextFunction) {
    SZrObject *iterator;
    SZrObjectPrototype *iteratorPrototype;

    if (state == ZR_NULL || moveNextFunction == ZR_NULL) {
        return ZR_NULL;
    }

    iteratorPrototype = zr_container_iterator_runtime_prototype(state, moveNextFunction);
    if (iteratorPrototype == ZR_NULL) {
        return ZR_NULL;
    }

    iterator = ZrCore_Object_New(state, iteratorPrototype);
    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Object_Init(state, iterator);

    if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
        return ZR_NULL;
    }
    if (!zr_container_set_iterator_source_object_fast(state, iterator, source, sourceType) ||
        !zr_container_set_iterator_index_fast(state, iterator, indexValue) ||
        !zr_container_set_iterator_next_node_fast(state, iterator, nextNode)) {
        return ZR_NULL;
    }
    return iterator;
}

/** @brief 从 native 调用栈 functionBase+1 读取迭代器；只返回借用对象，错误接收值返回 NULL。 */
static SZrObject *zr_container_iterator_self(SZrState *state) {
    SZrCallInfo *callInfo;
    SZrTypeValue *selfValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = state->callInfoList;
    selfValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 1);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, selfValue->value.object);
}

/** @brief 把 bool 写回函数基址并设置一项 native 返回值；调用后不可继续依赖旧栈布局指针。 */
static TZrInt64 zr_container_iterator_finish_move_next(SZrState *state, TZrBool ok) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, ok, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

/** @brief 从迭代器 source 热槽取 backing；缺缓存回退字段读，OBJECT/ARRAY 外的值不构成有效源。 */
static ZR_FORCE_INLINE SZrObject *zr_container_iterator_source_object_fast(SZrState *state, SZrObject *iterator) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }

    pair = iterator->cachedIteratorSourcePair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
            value->value.object != ZR_NULL) {
            return ZR_CAST_OBJECT(state, value->value.object);
        }
        if (value->type == ZR_VALUE_TYPE_NULL) {
            return ZR_NULL;
        }
    }

    return zr_container_get_object_field_fast(state, iterator, kContainerSourceField);
}

/** @brief 取得链表下一节点借用指针；NULL 或非对象游标表示停止，节点仍由实例链接保活。 */
static ZR_FORCE_INLINE SZrObject *zr_container_iterator_next_node_object_fast(SZrState *state, SZrObject *iterator) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return ZR_NULL;
    }

    pair = iterator->cachedIteratorNextNodePair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
            value->value.object != ZR_NULL) {
            return ZR_CAST_OBJECT(state, value->value.object);
        }
        if (value->type == ZR_VALUE_TYPE_NULL) {
            return ZR_NULL;
        }
    }

    return zr_container_get_object_field_fast(state, iterator, kContainerNextNodeField);
}

/** @brief 读取数组迭代器整数游标；字段缺失或异常标签按默认位置处理，不校验 backing 版本。 */
static ZR_FORCE_INLINE TZrInt64 zr_container_iterator_index_fast(SZrState *state,
                                                                 SZrObject *iterator,
                                                                 TZrInt64 defaultValue) {
    SZrHashKeyValuePair *pair;
    SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return defaultValue;
    }

    pair = iterator->cachedIteratorIndexPair;
    if (pair != ZR_NULL) {
        value = &pair->value;
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
            return value->value.nativeObject.nativeInt64;
        }
        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
            return (TZrInt64)value->value.nativeObject.nativeUInt64;
        }
    }

    return zr_container_get_int_field(state, iterator, kContainerIndexField, defaultValue);
}

/* 内建 array 与容器的数组 backing 共用迭代逻辑，原始整数表示走直接读取路径。 */
/** @brief 以当前 backing 长度推进数组游标并复制 current；raw-int 不需物化元素，结束置空 current，源变更可影响后续遍历。 */
/* raw-int current 与对象 current 的复制成本不同；退出都必须把 current 置空，不能暴露上次元素。 */
static TZrInt64 zr_container_array_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *source;
    TZrInt64 index;
    const SZrTypeValue *current;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    source = zr_container_iterator_source_object_fast(state, iterator);
    index = zr_container_iterator_index_fast(state, iterator, 0);
    if (source != ZR_NULL &&
        source->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
        source->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL &&
        source->superArrayRawIntData != ZR_NULL &&
        source->superArrayRawIntLength <= source->superArrayRawIntCapacity) {
        if (index < 0 || (TZrUInt64)index >= (TZrUInt64)source->superArrayRawIntLength) {
            if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
                return zr_container_iterator_finish_move_next(state, ZR_FALSE);
            }
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        if (!zr_container_set_iterator_current_int_fast(state, iterator, source->superArrayRawIntData[(TZrSize)index]) ||
            !zr_container_set_iterator_index_fast(state, iterator, index + 1)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_TRUE);
    }

    current = (source != ZR_NULL && index >= 0) ? ZrLib_Array_Get(state, source, (TZrSize)index) : ZR_NULL;
    if (current == ZR_NULL) {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    if (!zr_container_set_iterator_current_value_fast(state, iterator, current) ||
        !zr_container_set_iterator_index_fast(state, iterator, index + 1)) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

/** @brief 读取当前 nextNode.value 后推进 next 链；结束置空 current，没有捕获链表版本或防循环快照。 */
static TZrInt64 zr_container_linked_list_iterator_move_next_native(SZrState *state) {
    SZrObject *iterator = zr_container_iterator_self(state);
    SZrObject *node;
    const SZrTypeValue *value;

    if (iterator == ZR_NULL) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    node = zr_container_iterator_next_node_object_fast(state, iterator);
    if (node == ZR_NULL) {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }

    value = zr_container_get_own_field_value_fast(state, node, kContainerValueField);
    if (value != ZR_NULL) {
        if (!zr_container_set_iterator_current_value_fast(state, iterator, value)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
    } else {
        if (!zr_container_set_iterator_current_null_fast(state, iterator)) {
            return zr_container_iterator_finish_move_next(state, ZR_FALSE);
        }
    }
    if (!zr_container_set_iterator_next_node_fast(state,
                                                  iterator,
                                                  zr_container_get_object_field_fast(state, node, kContainerNextField))) {
        return zr_container_iterator_finish_move_next(state, ZR_FALSE);
    }
    return zr_container_iterator_finish_move_next(state, ZR_TRUE);
}

/** @brief 只接受零或两个实参，初始化 first/second 并完成结构构造；一个实参虽处于元数据范围仍由回调拒绝。 */
static TZrBool zr_container_pair_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *pair;
    TZrSize argc;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    argc = ZrLib_CallContext_ArgumentCount(context);
    if (argc != 0 && argc != 2) {
        ZrLib_CallContext_RaiseArityError(context, 0, 2);
    }

    pair = zr_container_resolve_construct_target(context);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argc == 2) {
        if (!zr_container_set_value_field_fast(context->state,
                                               pair,
                                               kContainerPairFirstField,
                                               ZrLib_CallContext_Argument(context, 0)) ||
            !zr_container_set_value_field_fast(context->state,
                                               pair,
                                               kContainerPairSecondField,
                                               ZrLib_CallContext_Argument(context, 1))) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_null_field_fast(context->state, pair, kContainerPairFirstField) ||
            !zr_container_set_null_field_fast(context->state, pair, kContainerPairSecondField)) {
            return ZR_FALSE;
        }
    }

    return zr_container_finish_object(context, result, pair);
}

/** @brief 验证另一值属于绑定 Pair 原型，再比较 first 与 second；字段或用户相等回调失败不承诺成功结果。 */
static TZrBool zr_container_pair_equals(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    const SZrTypeValue *selfFirst;
    const SZrTypeValue *selfSecond;
    const SZrTypeValue *otherFirst;
    const SZrTypeValue *otherSecond;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    if (!zr_container_object_is_owner_instance(context, other)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    selfFirst = zr_container_pair_get_first_fast(context->state, self);
    selfSecond = zr_container_pair_get_second_fast(context->state, self);
    otherFirst = zr_container_pair_get_first_fast(context->state, other);
    otherSecond = zr_container_pair_get_second_fast(context->state, other);
    zr_container_result_set_bool_fast(result,
                                      zr_container_values_equal(context->state, selfFirst, otherFirst) &&
                                              zr_container_values_equal(context->state, selfSecond, otherSecond));
    return ZR_TRUE;
}

/** @brief 按 first 后 second 作字典序比较；普通 compareTo 与 COMPARE 元方法共用本回调，数字回退的缺陷可沿合法 Pair 调用到达。 */
static TZrBool zr_container_pair_compare(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrTypeValue *otherValue = ZrLib_CallContext_Argument(context, 0);
    SZrObject *other;
    TZrInt64 compare;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL || otherValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (otherValue->type != ZR_VALUE_TYPE_OBJECT || otherValue->value.object == ZR_NULL) {
        zr_container_result_set_int_fast(result, 1);
        return ZR_TRUE;
    }

    other = ZR_CAST_OBJECT(context->state, otherValue->value.object);
    compare = zr_container_values_compare(context->state,
                                          zr_container_pair_get_first_fast(context->state, self),
                                          zr_container_pair_get_first_fast(context->state, other));
    if (compare == 0) {
        compare = zr_container_values_compare(context->state,
                                              zr_container_pair_get_second_fast(context->state, self),
                                              zr_container_pair_get_second_fast(context->state, other));
    }
    zr_container_result_set_int_fast(result, compare);
    return ZR_TRUE;
}

/** @brief 分别取 first/second 哈希再组合为整数；用户 hashCode 可执行代码，本回调直接返回 true，线程异常由外层绑定派发消费。 */
static TZrBool zr_container_pair_hash_code(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    TZrUInt64 firstHash;
    TZrUInt64 secondHash;

    if (context == ZR_NULL || result == ZR_NULL || self == ZR_NULL) {
        return ZR_FALSE;
    }

    firstHash = zr_container_value_hash(context->state, zr_container_pair_get_first_fast(context->state, self));
    secondHash = zr_container_value_hash(context->state, zr_container_pair_get_second_fast(context->state, self));
    zr_container_result_set_int_fast(result,
                                     (TZrInt64)((firstHash * ZR_CONTAINER_HASH_MIX_PRIME) ^
                                                (secondHash + ZR_CONTAINER_HASH_MIX_OFFSET)));
    return ZR_TRUE;
}

/** @brief 读取可选非负容量、准备 backing，再初始化 length/capacity；失败不发布已完成对象，capacity 实参名称元数据尚为 index。 */
static TZrBool zr_container_array_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *arrayObject = zr_container_resolve_construct_target(context);
    TZrInt64 capacity = 0;
    SZrObject *items;

    if (arrayObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLib_CallContext_ArgumentCount(context) == 1 && !ZrLib_CallContext_ReadInt(context, 0, &capacity)) {
        return ZR_FALSE;
    }
    if (capacity < 0) {
        ZrCore_Debug_RunError(context->state, "Array capacity must be non-negative");
    }

    items = ZrLib_Array_New(context->state);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_container_array_prepare_backing_storage(context->state, items, capacity > 0 ? (TZrSize)capacity : 0)) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, arrayObject, kContainerItemsField, items) ||
        !zr_container_set_int_field_fast(context->state, arrayObject, kContainerLengthField, 0) ||
        !zr_container_set_int_field_fast(context->state, arrayObject, kContainerCapacityField, capacity)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, arrayObject);
}

/** @brief 确保公开容量、追加 backing 后更新 length；返回 null，不保证分配或最后字段写失败时回滚已追加元素。 */
static TZrBool zr_container_array_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *value;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    value = ZrLib_CallContext_Argument(context, 0);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    length = zr_container_array_length_fast(items);
    if (value == ZR_NULL || !zr_container_array_ensure_capacity(context->state, self, length + 1)) {
        return ZR_FALSE;
    }
    if (!zr_container_storage_push_gc_value_dense_pair_pool_fast(context->state, items, value) &&
        !zr_container_storage_push(context->state, items, value)) {
        return ZR_FALSE;
    }
    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)(length + 1))) {
        return ZR_FALSE;
    }

    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/** @brief 接受闭区间 0..length 的插入位置，位移后更新 length；通用位移失败传播不足见 storage_insert TODO。 */
static TZrBool zr_container_array_insert(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    length = zr_container_array_length_fast(items);
    if (indexValue < 0 || (TZrSize)indexValue > length) {
        ZrCore_Debug_RunError(context->state, "Array.insert index out of range");
    }
    if (!zr_container_array_ensure_capacity(context->state, self, length + 1) ||
        !zr_container_storage_insert(context->state, items, (TZrSize)indexValue, ZrLib_CallContext_Argument(context, 1)) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)(length + 1))) {
        return ZR_FALSE;
    }

    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/** @brief 只接受现存下标，移除后更新 length；存储失败也走当前 out-of-range 错误分支，不表示底层失败必为边界问题。 */
static TZrBool zr_container_array_remove_at(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (indexValue < 0 || !zr_container_storage_remove_at(context->state, items, (TZrSize)indexValue)) {
        ZrCore_Debug_RunError(context->state, "Array.removeAt index out of range");
    }

    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, (TZrInt64)zr_container_array_length_fast(items))) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/** @brief 清空并复用 backing 后将公开 length 归零，capacity 保留；不逐个回调元素析构。 */
static TZrBool zr_container_array_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(items);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerLengthField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/** @brief raw-int 专用搜索或逐值 equals，返回首个下标/未找到的 -1；用户回调错误仍须检查线程状态。 */
static TZrBool zr_container_array_index_of(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *needle;
    TZrSize length;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    needle = ZrLib_CallContext_Argument(context, 0);
    length = zr_container_array_length_fast(items);
    if (needle != ZR_NULL && ZR_VALUE_IS_TYPE_INT(needle->type)) {
        if (zr_container_array_raw_int_active(items)) {
            TZrInt64 signedNeedle;

            if (zr_container_try_read_int64_needle_fast(needle, &signedNeedle)) {
                for (TZrSize index = 0; index < length; index++) {
                    if (items->superArrayRawIntData[index] == signedNeedle) {
                        zr_container_result_set_int_fast(result, (TZrInt64)index);
                        return ZR_TRUE;
                    }
                }
            }

            zr_container_result_set_int_fast(result, -1);
            return ZR_TRUE;
        }

        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (zr_container_int_values_equal_fast(candidate, needle)) {
                zr_container_result_set_int_fast(result, (TZrInt64)index);
                return ZR_TRUE;
            }
        }
    } else {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (candidate != ZR_NULL && zr_container_values_equal(context->state, candidate, needle)) {
                zr_container_result_set_int_fast(result, (TZrInt64)index);
                return ZR_TRUE;
            }
        }
    }

    zr_container_result_set_int_fast(result, -1);
    return ZR_TRUE;
}

/** @brief 与 indexOf 同一相等语义返回 bool，raw-int 专用分支避免物化；不保证用户 equals 无副作用。 */
static TZrBool zr_container_array_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *items;
    const SZrTypeValue *needle;
    TZrSize length;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    self = zr_container_self_object(context);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    needle = ZrLib_CallContext_Argument(context, 0);
    length = zr_container_array_length_fast(items);
    if (zr_container_array_raw_int_active(items)) {
        TZrInt64 signedNeedle;

        if (zr_container_try_read_int64_needle_fast(needle, &signedNeedle)) {
            for (TZrSize index = 0; index < length; index++) {
                if (items->superArrayRawIntData[index] == signedNeedle) {
                    zr_container_result_set_bool_fast(result, ZR_TRUE);
                    return ZR_TRUE;
                }
            }
        }
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (needle != ZR_NULL && ZR_VALUE_IS_TYPE_INT(needle->type)) {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (zr_container_int_values_equal_fast(candidate, needle)) {
                zr_container_result_set_bool_fast(result, ZR_TRUE);
                return ZR_TRUE;
            }
        }
    } else {
        for (TZrSize index = 0; index < length; index++) {
            const SZrTypeValue *candidate = zr_container_array_get_value_fast(context->state, items, index);

            if (candidate != ZR_NULL && zr_container_values_equal(context->state, candidate, needle)) {
                zr_container_result_set_bool_fast(result, ZR_TRUE);
                return ZR_TRUE;
            }
        }
    }

    zr_container_result_set_bool_fast(result, ZR_FALSE);
    return ZR_TRUE;
}

/** @brief 将隐藏 items backing 发布为 Enumerator.source 并从 index 0 开始；迭代器持有源引用，不固定长度。 */
static TZrBool zr_container_array_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          items,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/** @brief 桥接 VM 内建 array 的原生 getIterator；失败接收值写 null 仍返回一项，成功实例引用原数组。 */
static TZrInt64 zr_container_native_array_get_iterator_native(SZrState *state) {
    SZrCallInfo *callInfo;
    TZrStackValuePointer base;
    SZrTypeValue *resultValue;
    SZrTypeValue *selfValue;
    SZrObject *self;
    SZrObject *iterator;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    base = callInfo->functionBase.valuePointer;
    resultValue = ZrCore_Stack_GetValue(base);
    selfValue = ZrCore_Stack_GetValue(base + 1);
    if (resultValue == ZR_NULL || selfValue == ZR_NULL ||
        selfValue->type != ZR_VALUE_TYPE_ARRAY || selfValue->value.object == ZR_NULL) {
        if (resultValue != ZR_NULL) {
            zr_container_result_set_null_fast(resultValue);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    iterator = zr_container_iterator_make(state,
                                          self,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    if (iterator == ZR_NULL) {
        zr_container_result_set_null_fast(resultValue);
    } else {
        zr_container_value_set_object_fast(state, resultValue, iterator, ZR_VALUE_TYPE_OBJECT);
    }

    state->stackTop.valuePointer = base + 1;
    return 1;
}

/** @brief 对负数或越界返回 null；合法位置复制 backing 值，目标由绑定层初始化并承担返回 ownership。 */
static TZrBool zr_container_array_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    const SZrTypeValue *value;
    TZrInt64 indexValue;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (indexValue < 0) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }
    if (zr_container_array_raw_int_active(items)) {
        if ((TZrUInt64)indexValue >= (TZrUInt64)items->superArrayRawIntLength) {
            zr_container_result_set_null_fast(result);
            return ZR_TRUE;
        }
        zr_container_result_set_int_fast(result, items->superArrayRawIntData[(TZrSize)indexValue]);
        return ZR_TRUE;
    }

    value = zr_container_array_get_value_fast(context->state, items, (TZrSize)indexValue);
    return zr_container_result_copy_no_profile(context->state, result, value);
}

/** @brief 拒绝越界赋值；普通 int64 可直接改 raw buffer，其余走核心数组 setter，再复制赋值结果。 */
static TZrBool zr_container_array_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *items;
    TZrInt64 indexValue;
    const SZrTypeValue *value;
    SZrTypeValue receiverValue;
    SZrTypeValue key;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &indexValue)) {
        return ZR_FALSE;
    }

    items = zr_container_ensure_items_array_fast(context->state, self);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (indexValue < 0 || (TZrSize)indexValue >= zr_container_array_length_fast(items)) {
        ZrCore_Debug_RunError(context->state, "Array index out of range");
    }

    value = ZrLib_CallContext_Argument(context, 1);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_container_array_raw_int_active(items) && ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        items->superArrayRawIntData[(TZrSize)indexValue] = value->value.nativeObject.nativeInt64;
        items->superArrayStorageGeneration++;
        items->superArrayRawIntDirty = ZR_FALSE;
        zr_container_result_set_int_fast(result, value->value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    zr_container_array_make_receiver_value(context->state, self, &receiverValue);
    ZrCore_Value_InitAsInt(context->state, &key, indexValue);
    if (!ZrCore_Object_SuperArraySetInt(context->state, &receiverValue, &key, value)) {
        return ZR_FALSE;
    }
    return zr_container_result_copy_no_profile(context->state, result, value);
}

/** @brief 确保隐藏 entries 并初始化 count 为零；构造失败不写完成结果。 */
static TZrBool zr_container_map_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, self, kContainerEntriesField, entries) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

/** @brief 通过实际 entries 查找键并返回 bool；字符串热缓存只覆盖已成功命中的键。 */
static TZrBool zr_container_map_contains_key(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrBool found;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    found = zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL, ZR_NULL);
    zr_container_result_set_bool_fast(result, found);
    return ZR_TRUE;
}

/** @brief 找到键后移除 entries 下标并更新 count；未找到或存储移除失败按 false 返回，失败可能不是纯缺键。 */
static TZrBool zr_container_map_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (!zr_container_map_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index, ZR_NULL) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

/** @brief 清空 entries 并将 count 置零；版本变化使位置缓存失效，保留 backing 分配。 */
static TZrBool zr_container_map_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(entries);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/** @brief 返回遍历 entries Pair<K,V> 的 Enumerator；source 是 backing，current 的结构复制服从 Value_Copy 语义。 */
static TZrBool zr_container_map_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/** @brief 确保 entries 后查找并复制 second；entry 为 NULL 就写 null/true，可能包括 ensure 或查找失败，线程异常仍由外层派发检查。 */
/* 普通读取会确保 backing；只读内联仅读现有 backing，缺 backing false 由正常状态派发转 null；两条读取的缺键均自行写 null/true。 */
static ZR_FORCE_INLINE TZrBool zr_container_map_get_item_core(SZrState *state,
                                                              const SZrTypeValue *selfValue,
                                                              const SZrTypeValue *keyValue,
                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entryObject;
    SZrObject *entries;
    const SZrTypeValue *mappedValue;

    if (state == ZR_NULL || selfValue == ZR_NULL || result == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(state, self);
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    mappedValue = zr_container_map_entry_get_second_value_fast(state, entryObject);
    return zr_container_result_copy_no_profile(state, result, mappedValue);
}

/** @brief 普通 GET_ITEM 回调从 context 提取 self/key 后共用核心读取；遵守普通绑定 result 初始化协议。 */
static TZrBool zr_container_map_get_item(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_map_get_item_core(context->state,
                                          ZrLib_CallContext_Self(context),
                                          ZrLib_CallContext_Argument(context, 0),
                                          result);
}

/* 只读内联派发只读取已经建立的 entries，避免一次下标访问隐式创建 backing；
 * entries 不存在时返回 false，由 core 的正常状态分支转换成 null 结果。
 */
/** @brief 栈根接收值快读既有 backing；缺 backing 返回 false 由正常状态派发转 null，缺键则本函数写 null 并返回 true。 */
static ZR_FORCE_INLINE TZrBool zr_container_map_get_item_readonly_inline_fast(SZrState *state,
                                                                              const SZrTypeValue *selfValue,
                                                                              const SZrTypeValue *keyValue,
                                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject;
    const SZrTypeValue *mappedValue;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || result == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_get_entries_array_cached_fast(state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(result);
        return ZR_TRUE;
    }

    mappedValue = zr_container_map_entry_get_second_value_fast(state, entryObject);
    return zr_container_result_copy_no_profile(state, result, mappedValue);
}

/** @brief 更新已存在 Pair.second 或新建并追加 Pair 后增 count；分配后重新从 selfValue 取对象，result 可省略。 */
/* 写入中可能创建 Pair 并触发分配；重新解析 selfValue 后才继续使用包装对象，避免沿用旧裸地址。 */
static ZR_FORCE_INLINE TZrBool zr_container_map_set_item_core(SZrState *state,
                                                              const SZrTypeValue *selfValue,
                                                              const SZrTypeValue *keyValue,
                                                              const SZrTypeValue *mappedValue,
                                                              SZrTypeValue *result) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject;
    TZrBool insertedNewEntry = ZR_FALSE;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || mappedValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(state, self);
    entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    if (entryObject != ZR_NULL) {
        if (!zr_container_map_entry_set_second_value_fast(state, entryObject, mappedValue)) {
            return ZR_FALSE;
        }
    } else {
        SZrObject *pair = zr_container_make_pair(state, keyValue, mappedValue);
        SZrTypeValue pairValue;
        if (pair == ZR_NULL) {
            return ZR_FALSE;
        }

        self = ZR_CAST_OBJECT(state, selfValue->value.object);
        if (self == ZR_NULL) {
            return ZR_FALSE;
        }
        entries = zr_container_get_entries_array_cached_fast(state, self);
        if (entries == ZR_NULL) {
            entries = zr_container_ensure_entries_array_fast(state, self);
            if (entries == ZR_NULL) {
                return ZR_FALSE;
            }
        }

        zr_container_value_set_object_fast(state, &pairValue, pair, ZR_VALUE_TYPE_OBJECT);
        if (!zr_container_storage_push(state, entries, &pairValue)) {
            return ZR_FALSE;
        }
        insertedNewEntry = ZR_TRUE;
    }

    if (insertedNewEntry &&
        !zr_container_set_int_field_fast(state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    return result == ZR_NULL ? ZR_TRUE : zr_container_result_copy_no_profile(state, result, mappedValue);
}

/** @brief 普通 SET_ITEM 回调共用写核心并按需要返回赋值值；readonly 快读标志不意味着 Map 内容不可变。 */
static TZrBool zr_container_map_set_item(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_map_set_item_core(context->state,
                                          ZrLib_CallContext_Self(context),
                                          ZrLib_CallContext_Argument(context, 0),
                                          ZrLib_CallContext_Argument(context, 1),
                                          result);
}

/** @brief 供栈根直接派发的无结果写分支；existing Pair 普通写优先，其他情况回退写核心，不能把 false 当事务回滚。 */
static ZR_FORCE_INLINE TZrBool zr_container_map_set_item_readonly_inline_no_result_fast(
        SZrState *state,
        const SZrTypeValue *selfValue,
        const SZrTypeValue *keyValue,
        const SZrTypeValue *mappedValue) {
    SZrObject *self;
    SZrObject *entries;
    SZrObject *entryObject = ZR_NULL;

    if (state == ZR_NULL || selfValue == ZR_NULL || keyValue == ZR_NULL || mappedValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    self = ZR_CAST_OBJECT(state, selfValue->value.object);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_get_entries_array_cached_fast(state, self);
    if (entries != ZR_NULL) {
        entryObject = zr_container_map_find_entry_object_fast(state, entries, keyValue);
    }
    if (entryObject != ZR_NULL) {
        return zr_container_map_entry_set_second_value_fast(state, entryObject, mappedValue);
    }

    return zr_container_map_set_item_core(state, selfValue, keyValue, mappedValue, ZR_NULL);
}

/** @brief 确保 entries backing 并将 count 初始化为零；泛型相等/哈希约束由描述符注册。 */
static TZrBool zr_container_set_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    SZrObject *entries = ZrLib_Array_New(context->state);

    if (self == ZR_NULL || entries == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_container_set_object_field_fast(context->state, self, kContainerEntriesField, entries) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

/** @brief 按相等规则拒绝重复元素并返回 false，新元素追加后更新 count 返回 true；失败不许诺回滚已发布内容。 */
static TZrBool zr_container_set_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    value = ZrLib_CallContext_Argument(context, 0);
    if (zr_container_set_find_index(context->state, entries, value, ZR_NULL)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_storage_push(context->state, entries, value) ||
        !zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

/** @brief 线性搜索 backing 并返回存在性；普通对象可触发 equals，不因 Set 名称宣称哈希复杂度。 */
static TZrBool zr_container_set_contains(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    zr_container_result_set_bool_fast(
            result,
            zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), ZR_NULL));
    return ZR_TRUE;
}

/** @brief 搜索首个相等元素并移动存储后更新 count；未找到/存储移除失败均返回 false。 */
static TZrBool zr_container_set_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    TZrSize index;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (!zr_container_set_find_index(context->state, entries, ZrLib_CallContext_Argument(context, 0), &index) ||
        !zr_container_storage_remove_at(context->state, entries, index)) {
        zr_container_result_set_bool_fast(result, ZR_FALSE);
        return ZR_TRUE;
    }

    if (!zr_container_set_int_field_fast(context->state,
                                         self,
                                         kContainerCountField,
                                         (TZrInt64)zr_container_array_length_fast(entries))) {
        return ZR_FALSE;
    }
    zr_container_result_set_bool_fast(result, ZR_TRUE);
    return ZR_TRUE;
}

/** @brief 清空并复用 entries，重置 count；元素 ownership 逐槽释放并不在这条快速清空路径完成。 */
static TZrBool zr_container_set_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_container_array_clear_items_reuse_storage(entries);
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/** @brief 返回遍历 entries 元素的 Enumerator；当前元素按值复制，源修改可影响后续结果。 */
static TZrBool zr_container_set_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *entries;
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entries = zr_container_ensure_entries_array_fast(context->state, self);
    iterator = zr_container_iterator_make(context->state,
                                          entries,
                                          ZR_VALUE_TYPE_ARRAY,
                                          0,
                                          ZR_NULL,
                                          zr_container_array_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/** @brief 支持零或一参构造：零参 value 为空，一参复制元素；初始化空链接，后续脱链不清空 value。 */
static TZrBool zr_container_linked_node_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *node = zr_container_resolve_construct_target(context);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLib_CallContext_ArgumentCount(context) > 0) {
        if (!zr_container_set_value_field_fast(context->state,
                                               node,
                                               kContainerValueField,
                                               ZrLib_CallContext_Argument(context, 0))) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_null_field_fast(context->state, node, kContainerValueField)) {
            return ZR_FALSE;
        }
    }
    if (!zr_container_set_null_field_fast(context->state, node, kContainerNextField) ||
        !zr_container_set_null_field_fast(context->state, node, kContainerPreviousField)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

/* TODO: 核实摘除节点时字段写入失败的传播与原子性。这里丢弃多次 setter 结果，
 * removeFirst/removeLast/remove 仍报告成功；需对字段写入做故障注入并检查首尾与 count。
 */
/** @brief 把被摘节点从前后/首尾连接中移除，清空其链接后更新 count；保留 value 供外部节点引用使用，多次 setter 失败未逐次检查。 */
/* 摘链保留 value，外部仍持有 node 时值继续可达；链接更新与 count 更新缺少统一回滚。 */
static void zr_container_linked_list_unlink_node(SZrState *state, SZrObject *list, SZrObject *node) {
    SZrObject *previous;
    SZrObject *next;
    TZrInt64 count;

    if (state == ZR_NULL || list == ZR_NULL || node == ZR_NULL) {
        return;
    }

    previous = zr_container_get_object_field_fast(state, node, kContainerPreviousField);
    next = zr_container_get_object_field_fast(state, node, kContainerNextField);
    if (previous != ZR_NULL) {
        (void)zr_container_set_object_field_fast(state, previous, kContainerNextField, next);
    } else {
        (void)zr_container_set_object_field_fast(state, list, kContainerFirstField, next);
    }
    if (next != ZR_NULL) {
        (void)zr_container_set_object_field_fast(state, next, kContainerPreviousField, previous);
    } else {
        (void)zr_container_set_object_field_fast(state, list, kContainerLastField, previous);
    }

    (void)zr_container_set_null_field_fast(state, node, kContainerNextField);
    (void)zr_container_set_null_field_fast(state, node, kContainerPreviousField);
    count = zr_container_get_int_field(state, list, kContainerCountField, 0);
    (void)zr_container_set_int_field_fast(state, list, kContainerCountField, count > 0 ? count - 1 : 0);
}

/** @brief 初始化空首尾和零 count；失败不发布构造结果。 */
static TZrBool zr_container_linked_list_constructor(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_resolve_construct_target(context);
    if (self == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerFirstField) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerLastField)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, self);
}

/** @brief 创建节点后串接旧 first 并更新首尾/计数，返回节点对象；这些字段写入不是可回滚事务。 */
static TZrBool zr_container_linked_list_add_first(ZrLibCallContext *context, SZrTypeValue *result) {
    /* TODO: addFirst/addLast/clear 分多次写首尾和节点连接；需用字段写入失败
     * 注入验证中途退出后 count、首尾与双向链接的一致性。
     */
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *first;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    if (!zr_container_set_object_field_fast(context->state, node, kContainerNextField, first)) {
        return ZR_FALSE;
    }
    if (first != ZR_NULL) {
        if (!zr_container_set_object_field_fast(context->state, first, kContainerPreviousField, node)) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_object_field_fast(context->state, self, kContainerLastField, node)) {
            return ZR_FALSE;
        }
    }
    count = zr_container_get_int_field(context->state, self, kContainerCountField, 0);
    if (!zr_container_set_object_field_fast(context->state, self, kContainerFirstField, node) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, count + 1)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

/** @brief 创建节点后串接旧 last 并更新首尾/计数，返回节点对象；外部保留节点引用可使其 value 继续可达。 */
static TZrBool zr_container_linked_list_add_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    SZrObject *last;
    TZrInt64 count;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_make_linked_node(context->state, ZrLib_CallContext_Argument(context, 0));
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field_fast(context->state, self, kContainerLastField);
    if (!zr_container_set_object_field_fast(context->state, node, kContainerPreviousField, last)) {
        return ZR_FALSE;
    }
    if (last != ZR_NULL) {
        if (!zr_container_set_object_field_fast(context->state, last, kContainerNextField, node)) {
            return ZR_FALSE;
        }
    } else {
        if (!zr_container_set_object_field_fast(context->state, self, kContainerFirstField, node)) {
            return ZR_FALSE;
        }
    }
    count = zr_container_get_int_field(context->state, self, kContainerCountField, 0);
    if (!zr_container_set_object_field_fast(context->state, self, kContainerLastField, node) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, count + 1)) {
        return ZR_FALSE;
    }
    return zr_container_finish_object(context, result, node);
}

/** @brief 先复制首节点 value 到结果，再摘链；空表返回 null，摘链不清空外部仍可持有的节点 value。 */
static TZrBool zr_container_linked_list_remove_first(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *first;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    first = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    if (first == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    value = zr_container_get_own_field_value_fast(context->state, first, kContainerValueField);
    if (!zr_container_result_copy_no_profile(context->state, result, value)) {
        return ZR_FALSE;
    }
    zr_container_linked_list_unlink_node(context->state, self, first);
    return ZR_TRUE;
}

/** @brief 先复制尾节点 value 再摘链；空表返回 null，copy 的 ownership 与节点原值各按值复制规则维护。 */
static TZrBool zr_container_linked_list_remove_last(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *last;
    const SZrTypeValue *value;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    last = zr_container_get_object_field_fast(context->state, self, kContainerLastField);
    if (last == ZR_NULL) {
        zr_container_result_set_null_fast(result);
        return ZR_TRUE;
    }

    value = zr_container_get_own_field_value_fast(context->state, last, kContainerValueField);
    if (!zr_container_result_copy_no_profile(context->state, result, value)) {
        return ZR_FALSE;
    }
    zr_container_linked_list_unlink_node(context->state, self, last);
    return ZR_TRUE;
}

/** @brief 从 first 按 next 线性查找首个相等值再摘链；equals 可执行用户代码，不捕获版本快照。 */
static TZrBool zr_container_linked_list_remove(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;
    const SZrTypeValue *needle;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    needle = ZrLib_CallContext_Argument(context, 0);
    node = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    while (node != ZR_NULL) {
        const SZrTypeValue *value = zr_container_get_own_field_value_fast(context->state, node, kContainerValueField);
        if (value != ZR_NULL && zr_container_values_equal(context->state, value, needle)) {
            zr_container_linked_list_unlink_node(context->state, self, node);
            zr_container_result_set_bool_fast(result, ZR_TRUE);
            return ZR_TRUE;
        }
        node = zr_container_get_object_field_fast(context->state, node, kContainerNextField);
    }

    zr_container_result_set_bool_fast(result, ZR_FALSE);
    return ZR_TRUE;
}

/** @brief 逐节点断开 next/previous，再清空首尾及计数；保留节点 value，字段失败会留下部分修改。 */
static TZrBool zr_container_linked_list_clear(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *node;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    node = zr_container_get_object_field_fast(context->state, self, kContainerFirstField);
    while (node != ZR_NULL) {
        SZrObject *next = zr_container_get_object_field_fast(context->state, node, kContainerNextField);
        if (!zr_container_set_null_field_fast(context->state, node, kContainerNextField) ||
            !zr_container_set_null_field_fast(context->state, node, kContainerPreviousField)) {
            return ZR_FALSE;
        }
        node = next;
    }

    if (!zr_container_set_null_field_fast(context->state, self, kContainerFirstField) ||
        !zr_container_set_null_field_fast(context->state, self, kContainerLastField) ||
        !zr_container_set_int_field_fast(context->state, self, kContainerCountField, 0)) {
        return ZR_FALSE;
    }
    zr_container_result_set_null_fast(result);
    return ZR_TRUE;
}

/** @brief 从当前 first 建立 nextNode 游标并保活 self；遍历之后跟随实际节点链接，没有修改检测协议。 */
static TZrBool zr_container_linked_list_get_iterator(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_container_self_object(context);
    SZrObject *iterator;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    iterator = zr_container_iterator_make(context->state,
                                          self,
                                          ZR_VALUE_TYPE_OBJECT,
                                          0,
                                          zr_container_get_object_field_fast(context->state, self, kContainerFirstField),
                                          zr_container_linked_list_iterator_move_next_native);
    return iterator != ZR_NULL && zr_container_finish_object(context, result, iterator);
}

/* Array.add 等元素参数的泛型 T 契约。 */
static const ZrLibParameterDescriptor kArrayValueParameter[] = {{"value", "T", ZR_NULL}};
/* 插入与 SET_ITEM 的 index/value 顺序契约。 */
static const ZrLibParameterDescriptor kArrayInsertParameters[] = {{"index", "int", ZR_NULL}, {"value", "T", ZR_NULL}};
/* 索引参数；当前构造器也借用它，capacity 命名差异见元方法 TODO。 */
static const ZrLibParameterDescriptor kArrayIndexParameter[] = {{"index", "int", ZR_NULL}};
/* Map 读取/查找/删除的 K 参数契约。 */
static const ZrLibParameterDescriptor kMapKeyParameter[] = {{"key", "K", ZR_NULL}};
/* Map 写入键 K 与值 V 的参数顺序契约。 */
static const ZrLibParameterDescriptor kMapSetItemParameters[] = {{"key", "K", ZR_NULL}, {"value", "V", ZR_NULL}};
/* Set 相等查找和新增的 T 参数契约。 */
static const ZrLibParameterDescriptor kSetValueParameter[] = {{"value", "T", ZR_NULL}};
/* Pair 两个分量的 K/V 构造参数契约。 */
static const ZrLibParameterDescriptor kPairParameters[] = {{"first", "K", ZR_NULL}, {"second", "V", ZR_NULL}};
/* Pair 的同泛型 equals/compareTo 参数契约。 */
static const ZrLibParameterDescriptor kPairOtherParameter[] = {{"other", "Pair<K,V>", ZR_NULL}};
/* 链表节点构造及链表操作的元素 T 参数契约。 */
static const ZrLibParameterDescriptor kLinkedNodeValueParameter[] = {{"value", "T", ZR_NULL}};
/* 连续视图的相对 start/length 参数，实际边界由视图回调校验。 */
static const ZrLibParameterDescriptor kSpanSliceParameters[] = {
        {"start", "int", ZR_NULL},
        {"length", "int", ZR_NULL},
};

/* Map 键须提供哈希与相等契约；当前实现仍以 entries 扫描而非按键哈希建表。 */
static const TZrChar *kMapKeyConstraints[] = {"zr.builtin.IHashable", "zr.builtin.IEquatable<K>"};
/* Set 元素的哈希与相等类型约束，不承诺 O(1) 查找。 */
static const TZrChar *kSetValueConstraints[] = {"zr.builtin.IHashable", "zr.builtin.IEquatable<T>"};
/* Array 同时声明索引与迭代接口，角色闭包由注册表绑定。 */
static const TZrChar *kArrayImplements[] = {"zr.builtin.IArrayLike<T>", "zr.iteration.Iterable<T>"};
/* Map 枚举 Pair<K,V>，不只枚举键。 */
static const TZrChar *kMapImplements[] = {"zr.iteration.Iterable<Pair<K,V>>"};
/* Set 枚举其 T 元素。 */
static const TZrChar *kSetImplements[] = {"zr.iteration.Iterable<T>"};
/* Pair 声明相等、排序和哈希接口，均绑定下方本地回调。 */
static const TZrChar *kPairImplements[] = {
        "zr.builtin.IEquatable<Pair<K,V>>",
        "zr.builtin.IComparable<Pair<K,V>>",
        "zr.builtin.IHashable"
};
/* LinkedList 枚举 T，迭代源是节点链接而非 entries backing。 */
static const TZrChar *kLinkedListImplements[] = {"zr.iteration.Iterable<T>"};

/* 无额外约束的单参数 T 契约，供 Array/LinkedList/Node/视图共享。 */
static const ZrLibGenericParameterDescriptor kSingleGenericT[] = {{"T", ZR_NULL, ZR_NULL, 0}};
/* Pair 的无附加约束 K/V 参数，不自动继承 Map 的键约束。 */
static const ZrLibGenericParameterDescriptor kGenericKV[] = {{"K", ZR_NULL, ZR_NULL, 0}, {"V", ZR_NULL, ZR_NULL, 0}};
/* 只给 K 附加哈希/相等约束，V 不增加约束。 */
static const ZrLibGenericParameterDescriptor kMapGenericParameters[] = {
        {"K", ZR_NULL, kMapKeyConstraints, ZR_ARRAY_COUNT(kMapKeyConstraints)},
        {"V", ZR_NULL, ZR_NULL, 0},
};
/* 给 T 附加哈希/相等约束。 */
static const ZrLibGenericParameterDescriptor kSetGenericParameters[] = {
        {"T", ZR_NULL, kSetValueConstraints, ZR_ARRAY_COUNT(kSetValueConstraints)},
};

/* 可见 length/capacity 字段；length 绑定 INDEX_LENGTH，隐藏 items 不在公开字段表。 */
static const ZrLibFieldDescriptor kArrayFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(
                "length", "int", ZR_NULL,
                ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("capacity", "int", ZR_NULL),
};
/* Array 的普通方法与 span/迭代角色绑定表；callback 由 registry 创建闭包后派发。 */
static const ZrLibMethodDescriptor kArrayMethods[] = {
        /* 绑定 Array.span 到 ContiguousView_FromArray，返回引用当前数组区间的 Span<T>。 实例无参；VIEW_CREATE 角色供连续视图 lowering 使用；不复制/拥有元素，区间与来源由视图实现检查。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("span", 0, 0, ZrVmLibContainer_ContiguousView_FromArray,
                                           "Span<T>", ZR_NULL, ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE),
        /* 绑定 Array.add，把 value:T 追加到 backing 并更新公开 length。 实例恰1参，默认派发flags0；普通 null 结果，追加/元数据失败不提供事务回滚。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, zr_container_array_add, "null", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        /* 绑定 Array.insert，在 index 处移位后插入 value:T。 实例恰2参 index:int/value:T，位置0..length；普通 null 结果，generic移位逐次失败传播见 TODO。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("insert", 2, 2, zr_container_array_insert, "null", ZR_NULL, ZR_FALSE,
                                      kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)),
        /* 绑定 Array.removeAt，移除指定现存位置并更新 length。 实例恰1参 index:int；普通 null 结果，非法位置或存储失败走当前边界错误路径。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeAt", 1, 1, zr_container_array_remove_at, "null", ZR_NULL, ZR_FALSE,
                                      kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter)),
        /* 绑定 Array.clear，复用存储并将 length 置零。 实例无参，普通 null 结果；capacity 保留，快速清空未逐槽 release，合法owner元素限制待核。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_array_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        /* 绑定 Array.contains，按当前元素相等规则返回存在性。 实例恰1参 value:T，普通 bool 结果；raw-int可专用搜索，对象equals可执行用户代码。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("contains", 1, 1, zr_container_array_contains, "bool", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        /* 绑定 Array.indexOf，返回首个相等位置或 -1。 实例恰1参 value:T，普通 int 结果；不假定equals无副作用，不搜索未在backing中的公开length虚值。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("indexOf", 1, 1, zr_container_array_index_of, "int", ZR_NULL, ZR_FALSE,
                                      kArrayValueParameter, ZR_ARRAY_COUNT(kArrayValueParameter)),
        /* 绑定 Array.getIterator，为 Iterable<T> 创建 backing Enumerator。 实例无参，ITERABLE_INIT 角色；返回GC迭代对象，其source保活backing，没有长度/版本快照。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_array_get_iterator, "zr.iteration.Enumerator<T>", ZR_NULL,
                                           ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* Array 构造和下标回调表；GET_ITEM 标注只读接收者，不限制隐藏缓存维护。 */
static const ZrLibMetaMethodDescriptor kArrayMetaMethods[] = {
        /* TODO: 此构造参数实际作为 capacity 读取，却复用名为 index 的元数据；
         * 需检查命名实参、签名帮助与 LSP 是否向用户暴露错误参数名。
         */
        /* 绑定 Array<T> 构造器，建立空 backing 与指定公开容量。 0..1参，返回已初始化Array<T>；可选参数实际capacity>=0，但元数据复用index名称；本项无GET_ITEM只读flag。 */
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_array_constructor, "Array<T>", ZR_NULL, kArrayIndexParameter, 1},
        /* 绑定 Array GET_ITEM，从有效下标复制元素，越界返回 null。 恰1参 index:int，READONLY_RECEIVER；结果目标由绑定层初始化，复制元素保留Value_Copy ownership语义。 */
        {ZR_META_GET_ITEM, 1, 1, zr_container_array_get_item, "T", ZR_NULL,
         kArrayIndexParameter, ZR_ARRAY_COUNT(kArrayIndexParameter), ZR_NULL, 0,
         ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER},
        /* 绑定 Array SET_ITEM，把 value:T 写到现存 index 并返回该值。 恰2参 index:int/value:T，默认flags0；越界错误，不允许本元方法扩长度，结果复制按值ownership。 */
        {ZR_META_SET_ITEM, 2, 2, zr_container_array_set_item, "T", ZR_NULL, kArrayInsertParameters, ZR_ARRAY_COUNT(kArrayInsertParameters)},
};

/* Map 公开 count 元数据，entries 保留为内部字段。 */
static const ZrLibFieldDescriptor kMapFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
};
/* Map 查询、删除、清空和 Pair 迭代入口绑定。 */
static const ZrLibMethodDescriptor kMapMethods[] = {
        /* 绑定 Map.containsKey，按实际entries查找K并返回存在性。 实例恰1参 key:K，普通bool结果；K受Map泛型哈希/相等约束，字符串正命中可缓存。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("containsKey", 1, 1, zr_container_map_contains_key, "bool", ZR_NULL, ZR_FALSE,
                                      kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)),
        /* 绑定 Map.remove，删除首个匹配键并同步count。 实例恰1参 key:K，普通bool结果；缺键或存储移除失败为guest false，callback状态另由派发消费。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_map_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kMapKeyParameter, ZR_ARRAY_COUNT(kMapKeyParameter)),
        /* 绑定 Map.clear，清空并复用entries、重置count。 实例无参，普通null结果；不逐Pair字段析构，保留空间，版本失效由清空backing实现。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_map_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        /* 绑定 Map.getIterator，供 Iterable<Pair<K,V>> 枚举条目。 实例无参，ITERABLE_INIT角色；返回GC迭代器引用entries，current的Pair复制遵循STRUCT语义而非保证别名。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_map_get_iterator, "zr.iteration.Enumerator<Pair<K,V>>",
                                           ZR_NULL, ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* Map 的下标操作同时提供普通 callback 与内联快路径；只读 get、无结果 set
 * 由派发标志选择，新增路径必须保留相同的缺失键与插入/更新语义。
 */
/* Map 下标的普通/栈根内联派发契约；RESULT_OPTIONAL 允许省略赋值结果，不表示禁止写内容。 */
static const ZrLibMetaMethodDescriptor kMapMetaMethods[] = {
        /* 绑定 Map<K,V> 无参构造，建立entries及零count。 恰0参，返回完成初始化的Map；本项默认flags0，没有GET/SET的RESULT_OPTIONAL或栈根快路径flag。 */
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_map_constructor, "Map<K,V>", ZR_NULL, ZR_NULL, 0},
        /* 绑定 Map GET_ITEM 普通getter及只读内联快读，返回V或null。 恰1参 key:K；STACK_ROOT_CONTEXT/NO_SELF_REBIND/INLINE_VALUE_CONTEXT/RESULT_ALWAYS_WRITTEN/READONLY_INLINE_VALUE_CONTEXT/READONLY_RECEIVER；inline缺backing false由派发转换，缺键自身null/true。 */
        {.metaType = ZR_META_GET_ITEM,
         .minArgumentCount = 1,
         .maxArgumentCount = 1,
         .callback = zr_container_map_get_item,
         .returnTypeName = "V",
         .documentation = ZR_NULL,
         .parameters = kMapKeyParameter,
         .parameterCount = ZR_ARRAY_COUNT(kMapKeyParameter),
         .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER,
         .readonlyInlineGetFastCallback = zr_container_map_get_item_readonly_inline_fast},
        /* 绑定 Map SET_ITEM 普通setter及省结果内联写，更新或新增K/V条目。 恰2参 key:K/value:V；STACK_ROOT_CONTEXT/NO_SELF_REBIND/INLINE_VALUE_CONTEXT/RESULT_ALWAYS_WRITTEN/READONLY_INLINE_VALUE_CONTEXT/RESULT_OPTIONAL；无READONLY_RECEIVER，result可省略，写入仍改变内容。 */
        {.metaType = ZR_META_SET_ITEM,
         .minArgumentCount = 2,
         .maxArgumentCount = 2,
         .callback = zr_container_map_set_item,
         .returnTypeName = "V",
         .documentation = ZR_NULL,
         .parameters = kMapSetItemParameters,
         .parameterCount = ZR_ARRAY_COUNT(kMapSetItemParameters),
         .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                          ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL,
         .readonlyInlineSetNoResultFastCallback = zr_container_map_set_item_readonly_inline_no_result_fast},
};

/* Set 公开 count 元数据；实际搜索长度仍取 backing。 */
static const ZrLibFieldDescriptor kSetFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
};
/* Set 普通操作及 T 迭代入口绑定，不提供下标元方法。 */
static const ZrLibMethodDescriptor kSetMethods[] = {
        /* 绑定 Set.add，按相等规则检查重复后新增T。 实例恰1参 value:T，普通bool结果；重复为guest false、callback true，新增后更新count，失败无事务回滚。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, zr_container_set_add, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        /* 绑定 Set.contains，在线性entries中查找T。 实例恰1参 value:T，普通bool结果；T的哈希/相等约束不等于此实现按元素哈希O(1)查找。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("contains", 1, 1, zr_container_set_contains, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        /* 绑定 Set.remove，删除首个相等T并同步count。 实例恰1参 value:T，普通bool结果；缺失或存储移除失败为guest false，generic位移失败传播待核。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_set_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kSetValueParameter, ZR_ARRAY_COUNT(kSetValueParameter)),
        /* 绑定 Set.clear，复用entries空间并清零count。 实例无参，普通null结果；快速clear不逐值release，owner元素可达限制与析构需核查。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_set_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
        /* 绑定 Set.getIterator，供 Iterable<T> 枚举实际entries。 实例无参，ITERABLE_INIT角色；GC迭代器source保持backing可达，不捕获修改版本。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_set_get_iterator, "zr.iteration.Enumerator<T>", ZR_NULL,
                                           ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* Set 仅公开无参构造元方法。 */
static const ZrLibMetaMethodDescriptor kSetMetaMethods[] = {
        /* 绑定 Set<T> 无参构造，建立entries和零count。 恰0参，返回Set<T>，默认flags0；T哈希/相等约束在type泛型表，本项没有下标元方法。 */
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_set_constructor, "Set<T>", ZR_NULL, ZR_NULL, 0},
};

/* Pair 两个泛型分量字段，字段描述符默认可写。 */
static const ZrLibFieldDescriptor kPairFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("first", "K", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("second", "V", ZR_NULL),
};
/* Pair 相等/字典序/哈希普通方法入口。 */
static const ZrLibMethodDescriptor kPairMethods[] = {
        /* 绑定 Pair.equals，确认同owner Pair再比较两个分量。 实例恰1参 other:Pair<K,V>，普通bool结果；对象equals可回调，K/V没有本表新增约束。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("equals", 1, 1, zr_container_pair_equals, "bool", ZR_NULL, ZR_FALSE,
                                      kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)),
        /* 绑定 Pair.compareTo，按first后second字典序比较。 实例恰1参 other:Pair<K,V>，普通int结果；与COMPARE共用pair_compare，合法int64数字回退有已记录BUG。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareTo", 1, 1, zr_container_pair_compare, "int", ZR_NULL, ZR_FALSE,
                                      kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)),
        /* 绑定 Pair.hashCode，组合两个分量的哈希供IHashable使用。 实例无参，普通int结果；用户hashCode可能置线程异常，本回调仍true，由外层派发检查。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("hashCode", 0, 0, zr_container_pair_hash_code, "int", ZR_NULL, ZR_FALSE, ZR_NULL,
                                      0),
};
/* Pair 零或两参构造，以及与 compareTo 共用的 COMPARE 回调。 */
static const ZrLibMetaMethodDescriptor kPairMetaMethods[] = {
        /* 绑定 Pair<K,V> 结构构造，初始化first/second。 元数据0..2参 first:K/second:V，回调只允许0或2并拒绝1；返回普通STRUCT值，复制不保证共享对象身份。 */
        {ZR_META_CONSTRUCTOR, 0, 2, zr_container_pair_constructor, "Pair<K,V>", ZR_NULL, kPairParameters, ZR_ARRAY_COUNT(kPairParameters)},
        /* 绑定 Pair COMPARE 到与compareTo相同的字典序回调。 恰1参 other:Pair<K,V>，普通int结果，默认flags0；数值回退BUG可经该合法元方法到达。 */
        {ZR_META_COMPARE, 1, 1, zr_container_pair_compare, "int", ZR_NULL, kPairOtherParameter, ZR_ARRAY_COUNT(kPairOtherParameter)},
};

/* 链表 count/first/last 可见字段，内部修改需要保持它们与链接一致。 */
static const ZrLibFieldDescriptor kLinkedListFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("count", "int", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("first", "LinkedNode<T>", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("last", "LinkedNode<T>", ZR_NULL),
};
/* 链表头尾新增/移除、线性删除和遍历绑定表。 */
static const ZrLibMethodDescriptor kLinkedListMethods[] = {
        /* 绑定 LinkedList.addFirst，新建含T的节点并连接为首节点。 实例恰1参 value:T，返回GC LinkedNode<T>；node.value保留元素，失败可能已有部分链接修改。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("addFirst", 1, 1, zr_container_linked_list_add_first, "LinkedNode<T>", ZR_NULL,
                                      ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        /* 绑定 LinkedList.addLast，新建含T的节点并连接为尾节点。 实例恰1参 value:T，返回GC LinkedNode<T>；外部node引用可在摘链后继续保活value。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("addLast", 1, 1, zr_container_linked_list_add_last, "LinkedNode<T>", ZR_NULL,
                                      ZR_FALSE, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        /* 绑定 LinkedList.removeFirst，复制首节点value再摘链。 实例无参，返回T或空表null；Value_Copy处理结果ownership，摘链不清空node.value。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeFirst", 0, 0, zr_container_linked_list_remove_first, "T", ZR_NULL,
                                      ZR_FALSE, ZR_NULL, 0),
        /* 绑定 LinkedList.removeLast，复制尾节点value再摘链。 实例无参，返回T或空表null；先复制值，后摘链接/count，多个setter并非事务。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("removeLast", 0, 0, zr_container_linked_list_remove_last, "T", ZR_NULL, ZR_FALSE,
                                      ZR_NULL, 0),
        /* 绑定 LinkedList.remove，沿next删除首个相等value。 实例恰1参 value:T，普通bool结果；用户equals可运行代码，遍历没有版本快照或原子删除保证。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("remove", 1, 1, zr_container_linked_list_remove, "bool", ZR_NULL, ZR_FALSE,
                                      kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)),
        /* 绑定 LinkedList.clear，断开节点链接并重置首尾/count。 实例无参，普通null结果；保留node.value，外部node持有者仍可达元素，失败可部分清链。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clear", 0, 0, zr_container_linked_list_clear, "null", ZR_NULL, ZR_FALSE, ZR_NULL,
                                      0),
        /* 绑定 LinkedList.getIterator，为Iterable<T>建立nextNode游标。 实例无参，ITERABLE_INIT角色；source保持list可达，之后跟随当前节点链接，未提供修改检测。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, zr_container_linked_list_get_iterator, "zr.iteration.Enumerator<T>",
                                           ZR_NULL, ZR_FALSE, ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};
/* LinkedList 无参空表构造元方法。 */
static const ZrLibMetaMethodDescriptor kLinkedListMetaMethods[] = {
        /* 绑定 LinkedList<T> 空表构造，first/last为空且count为零。 恰0参，返回GC LinkedList<T>，默认flags0；本项不分配节点或元素backing。 */
        {ZR_META_CONSTRUCTOR, 0, 0, zr_container_linked_list_constructor, "LinkedList<T>", ZR_NULL, ZR_NULL, 0},
};

/* 节点 value 及双向链接，脱链仍保留 value 可达性。 */
static const ZrLibFieldDescriptor kLinkedNodeFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("value", "T", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("next", "LinkedNode<T>", ZR_NULL),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("previous", "LinkedNode<T>", ZR_NULL),
};
/* LinkedNode 零或一参构造：省略元素时 value 为空。 */
static const ZrLibMetaMethodDescriptor kLinkedNodeMetaMethods[] = {
        /* 绑定 LinkedNode<T> 构造，初始化value和空双向链接。 0..1参 value:T，省略时value为空；返回GC LinkedNode<T>，脱链不等于释放value。 */
        {ZR_META_CONSTRUCTOR, 0, 1, zr_container_linked_node_constructor, "LinkedNode<T>", ZR_NULL, kLinkedNodeValueParameter, ZR_ARRAY_COUNT(kLinkedNodeValueParameter)},
};

/* 共享视图 source/start/length 角色表；readonly 视图复用字段表，写限制主要由协议/元方法决定。 */
static const ZrLibFieldDescriptor kSpanFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("source", "object", ZR_NULL,
                                          ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SOURCE),
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("start", "int", ZR_NULL,
                                          ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_START),
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("length", "int", ZR_NULL,
                                          ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH),
};

/* 可写视图的 slice 与 asReadOnly 角色；源区间由 contiguous_view 实现检查。 */
static const ZrLibMethodDescriptor kSpanMethods[] = {
        /* 绑定 Span.slice 到ContiguousView_Slice，创建同源相对区间。 实例恰2参 start:int/length:int，VIEW_SLICE角色；返回Span<T>，来源及合法边界由视图回调验证，不复制元素。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("slice", 2, 2, ZrVmLibContainer_ContiguousView_Slice,
                                           "Span<T>", ZR_NULL, ZR_FALSE,
                                           kSpanSliceParameters, ZR_ARRAY_COUNT(kSpanSliceParameters),
                                           ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE),
        /* 绑定 Span.asReadOnly，保留同源区间并转换只读视图类型。 实例无参，READONLY_VIEW_CONVERSION角色；返回ReadOnlySpan<T>而非冻结原数组，来源生命周期仍须满足借用约束。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("asReadOnly", 0, 0,
                                           ZrVmLibContainer_ContiguousView_AsReadOnly,
                                           "ReadOnlySpan<T>", ZR_NULL, ZR_FALSE,
                                           ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_READONLY_VIEW_CONVERSION),
};

/* 只读视图 slice 返回同只读类型，不提供转回可写入口。 */
static const ZrLibMethodDescriptor kReadOnlySpanMethods[] = {
        /* 绑定 ReadOnlySpan.slice，切分同源只读区间。 实例恰2参 start:int/length:int，VIEW_SLICE角色；返回ReadOnlySpan<T>，无转回可写视图接口。 */
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("slice", 2, 2, ZrVmLibContainer_ContiguousView_Slice,
                                           "ReadOnlySpan<T>", ZR_NULL, ZR_FALSE,
                                           kSpanSliceParameters, ZR_ARRAY_COUNT(kSpanSliceParameters),
                                           ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE),
};

/* 可写视图空构造与相对索引 GET/SET 入口。 */
static const ZrLibMetaMethodDescriptor kSpanMetaMethods[] = {
        /* 绑定 Span<T> 空构造到ContiguousView_Construct，重置空区间。 恰0参，返回RefLike Span<T>，默认flags0；type允许值构造但禁止装箱，不获得元素所有权。 */
        {ZR_META_CONSTRUCTOR, 0, 0, ZrVmLibContainer_ContiguousView_Construct,
         "Span<T>", ZR_NULL, ZR_NULL, 0},
        /* 绑定 Span GET_ITEM，按相对index读取来源元素。 恰1参 index:int，READONLY_RECEIVER；返回T，实际区间/来源检查在ContiguousView_GetItem，本flag不禁止Span SET_ITEM。 */
        {ZR_META_GET_ITEM, 1, 1, ZrVmLibContainer_ContiguousView_GetItem,
         "T", ZR_NULL, kArrayIndexParameter,
         ZR_ARRAY_COUNT(kArrayIndexParameter), ZR_NULL, 0,
         ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER},
        /* 绑定 Span SET_ITEM，按相对index修改来源并返回赋值结果。 恰2参 index:int/value:T，默认flags0；来源数组或lease有效性和边界由ContiguousView_SetItem校验，视图不拥有元素。 */
        {ZR_META_SET_ITEM, 2, 2, ZrVmLibContainer_ContiguousView_SetItem,
         "T", ZR_NULL, kArrayInsertParameters,
         ZR_ARRAY_COUNT(kArrayInsertParameters)},
};

/* 只读视图空构造和 GET 入口，没有 SET_ITEM。 */
static const ZrLibMetaMethodDescriptor kReadOnlySpanMetaMethods[] = {
        /* 绑定 ReadOnlySpan<T> 空构造到公共视图构造回调。 恰0参，返回RefLike ReadOnlySpan<T>，允许值构造禁止装箱；本项不声明SET_ITEM。 */
        {ZR_META_CONSTRUCTOR, 0, 0, ZrVmLibContainer_ContiguousView_Construct,
         "ReadOnlySpan<T>", ZR_NULL, ZR_NULL, 0},
        /* 绑定 ReadOnlySpan GET_ITEM，按相对index读取来源元素。 恰1参 index:int，READONLY_RECEIVER；返回T，来源可由其他持有者修改，本类型无SET_ITEM而非复制冻结快照。 */
        {ZR_META_GET_ITEM, 1, 1, ZrVmLibContainer_ContiguousView_GetItem,
         "T", ZR_NULL, kArrayIndexParameter,
         ZR_ARRAY_COUNT(kArrayIndexParameter), ZR_NULL, 0,
         ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER},
};

/* 容器类型表把构造权、泛型约束及 Iterable/RefLike 协议映射到运行时回调。 */
/* 八种容器类型的反射/泛型/协议表；Pair 是普通 STRUCT，Span 两型是 RefLike 且禁止装箱构造。 */
static const ZrLibTypeDescriptor g_container_types[] = {
        /* 声明Array<T>类，提供索引和T迭代接口，连接Array字段/普通方法/元方法表。 单T无额外约束；CLASS、允许值与装箱构造，ARRAY_LIKE|ITERABLE协议；length角色用于索引长度，隐藏items由回调维护。 */
        {"Array", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kArrayFields, ZR_ARRAY_COUNT(kArrayFields), kArrayMethods, ZR_ARRAY_COUNT(kArrayMethods), kArrayMetaMethods, ZR_ARRAY_COUNT(kArrayMetaMethods), ZR_NULL, ZR_NULL, kArrayImplements, ZR_ARRAY_COUNT(kArrayImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Array<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ARRAY_LIKE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明Map<K,V>类，迭代Pair<K,V>并提供键下标读写。 K受IHashable/IEquatable<K>约束、V无附加约束；CLASS、允许值与装箱构造、ITERABLE协议；count可见但实际长度取entries。 */
        {"Map", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kMapFields, ZR_ARRAY_COUNT(kMapFields), kMapMethods, ZR_ARRAY_COUNT(kMapMethods), kMapMetaMethods, ZR_ARRAY_COUNT(kMapMetaMethods), ZR_NULL, ZR_NULL, kMapImplements, ZR_ARRAY_COUNT(kMapImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Map<K,V>()", kMapGenericParameters, ZR_ARRAY_COUNT(kMapGenericParameters), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明Set<T>类，提供去重元素操作及T迭代。 T受IHashable/IEquatable<T>约束；CLASS、允许值与装箱构造、ITERABLE协议；无下标元方法，当前查询线性扫描。 */
        {"Set", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kSetFields, ZR_ARRAY_COUNT(kSetFields), kSetMethods, ZR_ARRAY_COUNT(kSetMethods), kSetMetaMethods, ZR_ARRAY_COUNT(kSetMetaMethods), ZR_NULL, ZR_NULL, kSetImplements, ZR_ARRAY_COUNT(kSetImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Set<T>()", kSetGenericParameters, ZR_ARRAY_COUNT(kSetGenericParameters), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明Pair<K,V>普通结构，提供分量、相等、排序及哈希方法。 K/V无附加泛型约束；STRUCT、允许值与装箱构造，EQUATABLE|COMPARABLE|HASHABLE协议；无RefLike位，值复制不能当共享别名。 */
        {"Pair", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kPairFields, ZR_ARRAY_COUNT(kPairFields), kPairMethods, ZR_ARRAY_COUNT(kPairMethods), kPairMetaMethods, ZR_ARRAY_COUNT(kPairMetaMethods), ZR_NULL, ZR_NULL, kPairImplements, ZR_ARRAY_COUNT(kPairImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Pair<K,V>(first: K, second: V)", kGenericKV, ZR_ARRAY_COUNT(kGenericKV), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) | ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)},
        /* 声明LinkedList<T>类，提供首尾节点操作和T迭代。 单T无额外约束；CLASS、允许值与装箱构造、ITERABLE协议；GC节点链接维护count/首尾，不使用entries数组。 */
        {"LinkedList", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedListFields, ZR_ARRAY_COUNT(kLinkedListFields), kLinkedListMethods, ZR_ARRAY_COUNT(kLinkedListMethods), kLinkedListMetaMethods, ZR_ARRAY_COUNT(kLinkedListMetaMethods), ZR_NULL, ZR_NULL, kLinkedListImplements, ZR_ARRAY_COUNT(kLinkedListImplements), ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedList<T>()", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)},
        /* 声明LinkedNode<T>类，保存value及双向链接。 单T无额外约束；CLASS、允许值与装箱构造、protocolMask0；零或一参构造，脱链后value仍可由外部节点引用保活。 */
        {"LinkedNode", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kLinkedNodeFields, ZR_ARRAY_COUNT(kLinkedNodeFields), ZR_NULL, 0, kLinkedNodeMetaMethods, ZR_ARRAY_COUNT(kLinkedNodeMetaMethods), ZR_NULL, ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "LinkedNode<T>(value: T)", kSingleGenericT, ZR_ARRAY_COUNT(kSingleGenericT), 0},
        /* 声明可写Span<T>连续非拥有视图，接入区间字段与slice/read/write回调。 单T无额外约束；STRUCT、允许值构造禁止装箱，REF_LIKE|CONTIGUOUS_VIEW_MUTABLE协议；source/start/length来源与边界须有效。 */
        {.name = "Span",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = kSpanFields,
         .fieldCount = ZR_ARRAY_COUNT(kSpanFields),
         .methods = kSpanMethods,
         .methodCount = ZR_ARRAY_COUNT(kSpanMethods),
         .metaMethods = kSpanMetaMethods,
         .metaMethodCount = ZR_ARRAY_COUNT(kSpanMetaMethods),
         .documentation = "Mutable non-owning contiguous view.",
         .allowValueConstruction = ZR_TRUE,
         .allowBoxedConstruction = ZR_FALSE,
         .constructorSignature = "Span<T>()",
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                         ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_MUTABLE)},
        /* 声明ReadOnlySpan<T>连续非拥有只读视图，复用区间字段并接入slice/read回调。 单T无额外约束；STRUCT、允许值构造禁止装箱，REF_LIKE|CONTIGUOUS_VIEW_READONLY协议；无SET_ITEM，不冻结其他持有者可写来源。 */
        {.name = "ReadOnlySpan",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .fields = kSpanFields,
         .fieldCount = ZR_ARRAY_COUNT(kSpanFields),
         .methods = kReadOnlySpanMethods,
         .methodCount = ZR_ARRAY_COUNT(kReadOnlySpanMethods),
         .metaMethods = kReadOnlySpanMetaMethods,
         .metaMethodCount = ZR_ARRAY_COUNT(kReadOnlySpanMetaMethods),
         .documentation = "Read-only non-owning contiguous view.",
         .allowValueConstruction = ZR_TRUE,
         .allowBoxedConstruction = ZR_FALSE,
         .constructorSignature = "ReadOnlySpan<T>()",
         .genericParameters = kSingleGenericT,
         .genericParameterCount = ZR_ARRAY_COUNT(kSingleGenericT),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_REF_LIKE) |
                         ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_READONLY)},
};

/* 描述符是 parser、原生绑定与运行时共享的类型/协议契约来源。 */
/* v1 运行时 provider 静态描述符；借用表和回调必须活到 registry 最后一次使用。 */
static const ZrLibModuleDescriptor g_container_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.container",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = ZR_NULL,
        .functionCount = 0,
        .types = g_container_types,
        .typeCount = ZR_ARRAY_COUNT(g_container_types),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Built-in generic container and interface module.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = ZR_NULL,
        .minRuntimeAbi = 0,
        .requiredCapabilities = 0,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
        .publicContractHash = "zr.container:v1:container-span-protocols",
};

/** @brief 返回进程静态 zr.container 描述符借用指针；宿主/registry 不应释放，插件卸载前须停止使用其中回调。 */
const ZrLibModuleDescriptor *ZrVmLibContainer_GetModuleDescriptor(void) {
    return &g_container_module_descriptor;
}

/** @brief 转交 pooling provider 的静态描述符；不注册、不创建池，注册消费者仍须先具备容器契约。 */
const ZrLibModuleDescriptor *ZrVmLibContainer_GetPoolingModuleDescriptor(void) {
    return ZrVmLibContainer_Pooling_GetModuleDescriptor();
}

/** @brief 在内建 array 原型缺字段时创建永久 native 闭包并写入；现存任意同名字段都视为已安装，不验证可调用性。 */
/* TODO：复核已有同名非闭包字段是否应算适配安装完成；当前 presence 检查不验证 callable 或 Enumerator 契约。 */
static TZrBool zr_container_install_basic_array_method(SZrState *state,
                                                       const TZrChar *methodName,
                                                       FZrNativeFunction nativeFunction) {
    SZrObjectPrototype *prototype;
    SZrObject *prototypeObject;
    SZrClosureNative *closure;
    SZrTypeValue closureValue;

    if (state == ZR_NULL || state->global == ZR_NULL || methodName == ZR_NULL || nativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    prototypeObject = &prototype->super;
    if (ZrLib_Object_GetFieldCString(state, prototypeObject, methodName) != ZR_NULL) {
        return ZR_TRUE;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    closure->nativeFunction = nativeFunction;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    ZrCore_Value_InitAsRawObject(state, &closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue.isNative = ZR_TRUE;
    ZrLib_Object_SetFieldCString(state, prototypeObject, methodName, &closureValue);
    /* BUG: SetFieldCString 是 void，分配或写入失败时可提前返回；此处仍报成功，
     * Register 可在内建 array 缺少 getIterator 时返回 true。证据见
     * native_binding_dispatch.c 的 SetFieldCString 失败路径；需用故障注入覆盖。
     */
    return ZR_TRUE;
}

/** @brief 要求已初始化的 global/mainThreadState 与基本 ARRAY 原型，为内建数组安装 getIterator 桥接。 */
static TZrBool zr_container_install_basic_array_adapter(SZrGlobalState *global) {
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_container_install_basic_array_method(global->mainThreadState,
                                                   "getIterator",
                                                   zr_container_native_array_get_iterator_native);
}

/* 先注册迭代依赖，再注册 Span 和引用其契约的池，最后桥接内建 array 迭代。 */
/** @brief 依次注册 iteration、容器、pooling 并安装内建 array 桥接；要求已初始化 registry/基本原型，失败保留此前成功步骤。 */
TZrBool ZrVmLibContainer_Register(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrVmLibIteration_Register(global) &&
           ZrLibrary_NativeRegistry_RegisterModule(global, &g_container_module_descriptor) &&
           ZrLibrary_NativeRegistry_RegisterModule(
                   global, ZrVmLibContainer_Pooling_GetModuleDescriptor()) &&
           zr_container_install_basic_array_adapter(global);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 共享库加载器按 v1 符号取得描述符；不等同 Register，不在此注册依赖或安装内建 array 桥接。 */
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibContainer_GetModuleDescriptor();
}
#endif
