#include "zr_vm_core/gc_young_allocation.h"

#include <stdint.h>
#include <string.h>

/* 统一填写可由调用方消费的字段诊断；诊断指针允许省略，因此失败状态仍以返回值为准。 */
static void gc_young_set_diagnostic_local(
        SZrGcYoungDiagnostic *diagnostic,
        EZrGcYoungDiagnosticCode code,
        TZrUInt32 field,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
}

/* 写屏障入口先拒绝未知代号，避免把无效枚举误当成“不需要记卡”的合法组合。 */
static TZrBool gc_young_generation_valid(
        EZrGarbageCollectHeapGenerationKind generation) {
    return generation == ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG ||
           generation == ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD ||
           generation == ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_PERMANENT;
}

/* 把被写入的字节范围映射到卡索引；调用方仅在旧代到年轻代时使用结果标脏。 */
static TZrBool gc_young_card_range_valid(
        const SZrGcCardTable *table,
        const TZrByte *address,
        TZrSize size,
        TZrSize *firstCard,
        TZrSize *lastCard) {
    uintptr_t heapStart;
    uintptr_t storeStart;
    uintptr_t storeEnd;
    TZrUInt64 offset;

    if (table == ZR_NULL || address == ZR_NULL || size == 0u ||
        table->heapBegin == ZR_NULL || table->heapBytes == 0u ||
        table->cardCount == 0u) {
        return ZR_FALSE;
    }
    heapStart = (uintptr_t)(const void *)table->heapBegin;
    storeStart = (uintptr_t)(const void *)address;
    if (storeStart < heapStart ||
        storeStart - heapStart >= (uintptr_t)table->heapBytes ||
        size > (TZrSize)(UINTPTR_MAX - storeStart)) {
        return ZR_FALSE;
    }
    storeEnd = storeStart + (uintptr_t)size - 1u;
    if (storeEnd < storeStart ||
        storeEnd - heapStart >= (uintptr_t)table->heapBytes) {
        return ZR_FALSE;
    }
    offset = (TZrUInt64)(storeStart - heapStart);
    *firstCard = (TZrSize)(offset >> ZR_GC_CARD_SHIFT);
    *lastCard = (TZrSize)(((TZrUInt64)(storeEnd - heapStart)) >> ZR_GC_CARD_SHIFT);
    return *firstCard <= *lastCard && *lastCard < table->cardCount;
}

/**
 * @brief 为外部提供的卡字节数组建立年轻代写屏障状态。
 * @pre cards 至少有 cardCount 字节，且 heapBegin/heapBytes 描述可寻址的堆区间。
 * @return 参数或容量不满足时返回 false；成功时所有卡均被重置为 clean。
 * @note 初始化会覆盖卡数组和 table，不可用于仍需保留脏卡信息的表。
 */
TZrBool ZrCore_GcCardTable_Init(
        SZrGcCardTable *table,
        TZrByte *cards,
        TZrSize cardCount,
        TZrByte *heapBegin,
        TZrSize heapBytes) {
    TZrSize requiredCardCount;

    if (table == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(table, 0, sizeof(*table));
    if (cards == ZR_NULL || cardCount == 0u || heapBegin == ZR_NULL || heapBytes == 0u) {
        return ZR_FALSE;
    }
    if (heapBytes > SIZE_MAX - (ZR_GC_CARD_BYTES - 1u)) {
        return ZR_FALSE;
    }
    requiredCardCount = (heapBytes + (ZR_GC_CARD_BYTES - 1u)) / ZR_GC_CARD_BYTES;
    if (cardCount < requiredCardCount) {
        return ZR_FALSE;
    }
    table->cards = cards;
    table->cardCount = cardCount;
    table->heapBegin = heapBegin;
    table->heapBytes = heapBytes;
    table->dirtyCount = 0u;
    memset(cards, ZR_GC_CARD_CLEAN, cardCount);
    return ZR_TRUE;
}

/**
 * @brief 检查卡表描述符能否安全参与后续记录或清理。
 * @return 描述符有效返回 true；诊断可为 NULL，调用方仍须检查返回值。
 * @note 校验容量关系，不扫描卡数组来重建 dirtyCount；数组需由本 API 族维护。
 */
TZrBool ZrCore_GcCardTable_Validate(
        const SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize requiredCardCount;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (table == ZR_NULL || table->cards == ZR_NULL || table->cardCount == 0u ||
        table->heapBegin == ZR_NULL || table->heapBytes == 0u) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (table->heapBytes > SIZE_MAX - (ZR_GC_CARD_BYTES - 1u)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                      3u, SIZE_MAX, table->heapBytes);
        return ZR_FALSE;
    }
    requiredCardCount = (table->heapBytes + (ZR_GC_CARD_BYTES - 1u)) / ZR_GC_CARD_BYTES;
    if (table->cardCount < requiredCardCount || table->dirtyCount > table->cardCount) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
                                      1u, (TZrUInt64)requiredCardCount,
                                      (TZrUInt64)table->cardCount);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 记录一次堆内写入，并为旧代/永久代到年轻代的跨代引用标脏卡片。
 * @pre table 已初始化；非零 size 时 address..address+size 必须完整落在其堆区间。
 * @return 越界、无效代号或描述符错误返回 false；合法但无需记卡的写入返回 true。
 * @note 零长度写入在校验表与代号后作为无操作成功，地址不参与范围检查。
 */
TZrBool ZrCore_GcCardTable_RecordStore(
        SZrGcCardTable *table,
        TZrByte *address,
        TZrSize size,
        EZrGarbageCollectHeapGenerationKind ownerGeneration,
        EZrGarbageCollectHeapGenerationKind valueGeneration,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize firstCard;
    TZrSize lastCard;
    TZrSize card;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (!ZrCore_GcCardTable_Validate(table, diagnostic) ||
        !gc_young_generation_valid(ownerGeneration) ||
        !gc_young_generation_valid(valueGeneration)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_GC_YOUNG_DIAGNOSTIC_NONE) {
            gc_young_set_diagnostic_local(diagnostic,
                                          ZR_GC_YOUNG_DIAGNOSTIC_INVALID_GENERATION,
                                          3u, 1u, 0u);
        }
        return ZR_FALSE;
    }
    if (size == 0u) {
        return ZR_TRUE;
    }
    if (!gc_young_card_range_valid(table, address, size, &firstCard, &lastCard)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
                                      1u, table->heapBytes, (TZrUInt64)size);
        return ZR_FALSE;
    }
    if ((ownerGeneration != ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD &&
         ownerGeneration != ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_PERMANENT) ||
        valueGeneration != ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG) {
        return ZR_TRUE;
    }
    /* 只记录老/永久到年轻的边；幼代内部写入由收集器正常遍历覆盖。 */
    for (card = firstCard; card <= lastCard; ++card) {
        if (table->cards[card] == ZR_GC_CARD_CLEAN) {
            table->cards[card] = ZR_GC_CARD_DIRTY;
            if (table->dirtyCount == SIZE_MAX) {
                table->cards[card] = ZR_GC_CARD_CLEAN;
                gc_young_set_diagnostic_local(diagnostic,
                                              ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                              4u, SIZE_MAX - 1u, SIZE_MAX);
                return ZR_FALSE;
            }
            table->dirtyCount++;
        }
        if (card == SIZE_MAX) {
            break;
        }
    }
    return ZR_TRUE;
}

/** @brief 查询有效卡索引是否已标脏；空表或越界索引按 clean 处理。 */
TZrBool ZrCore_GcCardTable_IsDirty(
        const SZrGcCardTable *table,
        TZrSize cardIndex) {
    return (TZrBool)(table != ZR_NULL && table->cards != ZR_NULL &&
                     cardIndex < table->cardCount &&
                     table->cards[cardIndex] == ZR_GC_CARD_DIRTY);
}

/** @brief 读取写屏障维护的脏卡计数；空描述符返回零。 */
TZrSize ZrCore_GcCardTable_DirtyCardCount(
        const SZrGcCardTable *table) {
    return table != ZR_NULL ? table->dirtyCount : 0u;
}

/**
 * @brief 次级收集完成后清除整张卡表，为下一轮跨代写屏障重新计数。
 * @return 描述符校验通过时返回 true，并将 dirtyCount 归零。
 * @note 必须在本轮依赖这些卡的扫描完成后调用，否则会丢失未处理的跨代边。
 */
TZrBool ZrCore_GcCardTable_Clear(
        SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (!ZrCore_GcCardTable_Validate(table, diagnostic)) {
        return ZR_FALSE;
    }
    memset(table->cards, ZR_GC_CARD_CLEAN, table->cardCount);
    table->dirtyCount = 0u;
    return ZR_TRUE;
}

/* 同一种 token 的重复登记幂等；kind 隔离卡索引与对象稳定标识的数值空间。 */
static TZrBool gc_young_remembered_root_equal(
        const SZrGcRememberedRoot *left,
        EZrGcRememberedRootKind kind,
        TZrUInt64 token) {
    return left != ZR_NULL && left->kind == kind && left->token == token;
}

/* 记录入口只接受这两类根；新增根类型时需同步更新记录与消费方。 */
static TZrBool gc_young_remembered_root_valid_kind(
        EZrGcRememberedRootKind kind) {
    return kind == ZR_GC_REMEMBERED_ROOT_CARD ||
           kind == ZR_GC_REMEMBERED_ROOT_OBJECT;
}

/**
 * @brief 初始化调用方提供的定长根集合，用于保存卡索引或稳定对象 token。
 * @pre entries 至少有 capacity 个元素；成功后其内容被清零并由 set 引用。
 * @note epoch 由调用方传入并保留为集合代际标签，本实现不解释或递增它。
 */
TZrBool ZrCore_GcRememberedRootSet_Init(
        SZrGcRememberedRootSet *set,
        SZrGcRememberedRoot *entries,
        TZrSize capacity,
        TZrUInt32 epoch) {
    if (set == ZR_NULL || entries == ZR_NULL || capacity == 0u ||
        capacity > SIZE_MAX / sizeof(*entries)) {
        return ZR_FALSE;
    }
    memset(set, 0, sizeof(*set));
    memset(entries, 0, capacity * sizeof(*entries));
    set->entries = entries;
    set->capacity = capacity;
    set->epoch = epoch;
    return ZR_TRUE;
}

/* 两个公开 Record 入口共享去重和容量策略，满表时失败而不静默丢弃根。 */
static TZrBool gc_young_remembered_root_record(
        SZrGcRememberedRootSet *set,
        EZrGcRememberedRootKind kind,
        TZrUInt64 token,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize index;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (set == ZR_NULL || set->entries == ZR_NULL || set->capacity == 0u ||
        set->count > set->capacity ||
        !gc_young_remembered_root_valid_kind(kind)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < set->count; ++index) {
        if (gc_young_remembered_root_equal(&set->entries[index], kind, token)) {
            return ZR_TRUE;
        }
    }
    if (set->count == set->capacity) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
                                      1u, (TZrUInt64)set->capacity,
                                      (TZrUInt64)set->count + 1u);
        return ZR_FALSE;
    }
    set->entries[set->count].kind = kind;
    set->entries[set->count].token = token;
    set->count++;
    return ZR_TRUE;
}

TZrBool ZrCore_GcRememberedRootSet_RecordCard(
        SZrGcRememberedRootSet *set,
        TZrSize cardIndex,
        SZrGcYoungDiagnostic *diagnostic) {
    /* 卡 token 按表内索引解释；关联具体堆区间由消费该根的收集器负责。 */
    return gc_young_remembered_root_record(
            set, ZR_GC_REMEMBERED_ROOT_CARD, (TZrUInt64)cardIndex, diagnostic);
}

TZrBool ZrCore_GcRememberedRootSet_RecordObject(
        SZrGcRememberedRootSet *set,
        TZrUInt64 objectToken,
        SZrGcYoungDiagnostic *diagnostic) {
    /* 零保留为无效对象标识，避免把未初始化 token 当作可追踪对象。 */
    if (objectToken == 0u) {
        ZrCore_GcYoung_DiagnosticClear(diagnostic);
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      1u, 1u, 0u);
        return ZR_FALSE;
    }
    return gc_young_remembered_root_record(
            set, ZR_GC_REMEMBERED_ROOT_OBJECT, objectToken, diagnostic);
}

/**
 * @brief 以调用方游标顺序读取集合中的下一条根记录。
 * @pre set 来自成功初始化且扫描期间不被并发修改；cursor 初值通常为零。
 * @return 读到一条记录时递增 cursor 并返回 true；到达末尾时返回 false 并给出 END_OF_SCAN。
 * BUG: 公开 set 的 count 若被改到超过 capacity，cursor < count 时会越界读 entries[cursor]；需在读取前校验计数。
 */
TZrBool ZrCore_GcRememberedRootSet_ScanNext(
        const SZrGcRememberedRootSet *set,
        TZrSize *cursor,
        SZrGcRememberedRoot *root,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (set == ZR_NULL || set->entries == ZR_NULL || cursor == ZR_NULL || root == ZR_NULL) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (*cursor >= set->count) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_END_OF_SCAN,
                                      1u, (TZrUInt64)set->count,
                                      (TZrUInt64)*cursor);
        return ZR_FALSE;
    }
    *root = set->entries[*cursor];
    (*cursor)++;
    return ZR_TRUE;
}

/**
 * @brief 清空本轮记住的根记录，但保留 backing storage、容量和 epoch 供集合复用。
 * @return 描述符有效时返回 true，集合变为空。
 * @note 必须在根已消费后调用；提前清空会使调用方漏掉待扫描的跨代根。
 */
TZrBool ZrCore_GcRememberedRootSet_Clear(
        SZrGcRememberedRootSet *set,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (set == ZR_NULL || set->entries == ZR_NULL || set->capacity == 0u ||
        set->capacity > SIZE_MAX / sizeof(*set->entries)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    memset(set->entries, 0, set->capacity * sizeof(*set->entries));
    set->count = 0u;
    return ZR_TRUE;
}
