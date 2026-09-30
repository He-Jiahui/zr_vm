#include "dataflow_ownership_observations.h"

#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_facts.h"

/** @brief 判断位置是否提供可优先用于区间匹配的非零字节偏移。
 * @pre position 可为空；空位置或零偏移表示调用方应使用行列回退。
 */
static TZrBool ownership_observation_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

/** @brief 按 AST 节点与事实种类查询当前语义上下文中的既有所有权事实。
 * @note 去重键是 node 和 kind；同节点同种事实只保留一条。
 */
static TZrBool ownership_observation_fact_exists(
        const SZrSemanticContext *context,
        SZrAstNode *node,
        EZrSemanticOwnershipFactKind kind) {
    TZrSize index;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->ownershipFacts.length; index++) {
        const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipFacts,
                        index);
        if (fact != ZR_NULL && fact->node == node && fact->kind == kind) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/** @brief 比较两个来源位置兼容且坐标相同的文件区间。
 * @note 当任一端提供非零偏移时整体比较起止偏移，否则比较起止行列。
 */
static TZrBool ownership_observation_same_range(const SZrFileRange *left,
                                                 const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 两侧来源都存在时先验证来源身份；一侧缺失来源时保留坐标回退。 */
    if (left->source != ZR_NULL &&
        right->source != ZR_NULL &&
        left->source != right->source &&
        !ZrCore_String_Equal(left->source, right->source)) {
        return ZR_FALSE;
    }
    /* 只要任一端含字节偏移，就要求双方起止偏移一致，不混用行列坐标。 */
    if (ownership_observation_has_offset(&left->start) ||
        ownership_observation_has_offset(&left->end) ||
        ownership_observation_has_offset(&right->start) ||
        ownership_observation_has_offset(&right->end)) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }
    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

/** @brief 为同一引用按节点或区间找到首条指定种类的事实供调用方补充。
 * @return 返回 ownershipFacts 数组内的借用指针；不得跨数组追加或重置持有它。
 */
static SZrSemanticOwnershipFact *ownership_observation_find_fact(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        EZrSemanticOwnershipFactKind kind) {
    TZrSize index;

    if (context == ZR_NULL || reference == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->ownershipFacts.length; index++) {
        SZrSemanticOwnershipFact *fact =
                (SZrSemanticOwnershipFact *)ZrCore_Array_Get(&context->ownershipFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == kind &&
            (fact->node == reference->node ||
             ownership_observation_same_range(&fact->range, &reference->range))) {
            return fact;
        }
    }
    return ZR_NULL;
}

/** @brief 将可用的 owner 索引解析为符号条目，不可用时退回引用自身条目。
 * @pre symbols 与 entry 来自本次已构建的符号映射。
 */
static const SZrSemanticOwnershipSymbolEntry *ownership_observation_owner_entry(
        SZrSemanticOwnershipSymbolMap *symbols,
        const SZrSemanticOwnershipSymbolEntry *entry,
        TZrSize ownerIndex) {
    const SZrSemanticOwnershipSymbolEntry *ownerEntry = entry;

    if (ownerIndex != ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID &&
        ownerIndex < symbols->entries.length) {
        const SZrSemanticOwnershipSymbolEntry *candidate =
                ZrParser_DataflowOwnership_SymbolEntry(symbols, ownerIndex);
        if (candidate != ZR_NULL) {
            ownerEntry = candidate;
        }
    }
    return ownerEntry;
}

/** @brief 去重后追加一条 MOVE 或 RELEASE 事实，并带上引用与 owner 的区域。
 * @note node、range.source 与 AST 节点保持借用；语义上下文只复制事实记录。
 */
static TZrBool ownership_observation_append_transition(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        const SZrSemanticOwnershipSymbolEntry *entry,
        const SZrSemanticOwnershipSymbolEntry *ownerEntry,
        EZrSemanticOwnershipFactKind kind) {
    SZrSemanticOwnershipFact fact;

    if (ownership_observation_fact_exists(context, reference->node, kind)) {
        return ZR_TRUE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = reference->node;
    fact.range = reference->range;
    fact.kind = kind;
    fact.qualifier = entry->qualifier;
    fact.symbolId = reference->symbolId;
    fact.lifetimeRegionId = entry->regionId;
    fact.ownerLifetimeRegionId = ownerEntry->regionId;
    return ZrParser_SemanticFacts_AppendOwnership(context, &fact);
}

/** @brief 将 CFG 暂存的 move、release 与 violation 投影为语义所有权事实。
 * @pre observations 必须由同一 context 的当前 referenceFacts 分配；分析期间引用数组和符号映射保持稳定。
 * @return 验证或事实追加失败时返回 false；已追加的事实不会回滚。
 * @note AST 节点与 range 内的来源字符串是借用引用，随 AST/语义上下文生命周期有效。
 */
TZrBool ZrParser_DataflowOwnership_AppendObservedFacts(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *symbols,
        const SZrDataflowOwnershipObservations *observations) {
    TZrSize index;

    if (context == ZR_NULL ||
        symbols == ZR_NULL ||
        observations == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    /* 观察数组与 referenceFacts 按同一索引生成；无符号映射的引用不产出所有权事实。 */
    for (index = 0; index < observations->count; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts,
                        index);
        const SZrSemanticOwnershipSymbolEntry *entry;
        const SZrSemanticOwnershipSymbolEntry *ownerEntry;
        const SZrSemanticOwnershipSymbolEntry *violationOwnerEntry;
        SZrSemanticOwnershipFact fact;
        SZrSemanticOwnershipFact *existingError;
        TZrSize symbolIndex;

        if (reference == ZR_NULL ||
            !ZrParser_DataflowOwnership_SymbolFind(symbols,
                                                   reference->symbolId,
                                                   &symbolIndex)) {
            continue;
        }
        entry = ZrParser_DataflowOwnership_SymbolEntry(symbols, symbolIndex);
        if (entry == ZR_NULL) {
            continue;
        }
        ownerEntry = ownership_observation_owner_entry(symbols, entry, entry->ownerIndex);
        violationOwnerEntry = ownership_observation_owner_entry(
                symbols,
                ownerEntry,
                observations->violationOwnerIndices[index]);

        /* 先追加状态迁移点，供诊断和导航把消费/释放位置映射回原引用。 */
        if (observations->moveSeen[index] &&
            !ownership_observation_append_transition(context,
                                                     reference,
                                                     entry,
                                                     ownerEntry,
                                                     ZR_SEMANTIC_OWNERSHIP_FACT_MOVE)) {
            return ZR_FALSE;
        }
        if (observations->releaseSeen[index] &&
            !ownership_observation_append_transition(context,
                                                     reference,
                                                     entry,
                                                     ownerEntry,
                                                     ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE)) {
            return ZR_FALSE;
        }

        /* weak 读取可能已有较具体的错误事实；优先补齐 owner 与 cause，避免重复节点错误。 */
        existingError = entry->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK
                                ? ownership_observation_find_fact(
                                          context,
                                          reference,
                                          ZR_SEMANTIC_OWNERSHIP_FACT_ERROR)
                                : ZR_NULL;
        if (observations->violationSeen[index] &&
            observations->violationCauses[index] != ZR_NULL &&
            existingError != ZR_NULL) {
            existingError->qualifier = entry->qualifier;
            existingError->symbolId = reference->symbolId;
            existingError->lifetimeRegionId = entry->regionId;
            existingError->ownerLifetimeRegionId = violationOwnerEntry->regionId;
            existingError->relatedNode = observations->violationCauses[index];
            existingError->isViolation = ZR_TRUE;
        } else if (observations->violationSeen[index] &&
                   observations->violationCauses[index] != ZR_NULL &&
                   !ownership_observation_fact_exists(
                           context,
                           reference->node,
                           ZR_SEMANTIC_OWNERSHIP_FACT_ERROR)) {
            memset(&fact, 0, sizeof(fact));
            fact.node = reference->node;
            fact.range = reference->range;
            fact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_ERROR;
            fact.qualifier = entry->qualifier;
            fact.symbolId = reference->symbolId;
            fact.lifetimeRegionId = entry->regionId;
            fact.ownerLifetimeRegionId = violationOwnerEntry->regionId;
            fact.relatedNode = observations->violationCauses[index];
            fact.isViolation = ZR_TRUE;
            if (!ZrParser_SemanticFacts_AppendOwnership(context, &fact)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

/** @brief 按 referenceFacts 的当前长度分配 CFG 阶段的平行观察数组。
 * @pre observations 是零初始化状态；context、其 state 与 referenceFacts 已初始化。
 * @return 失败时返回 false，count 与已成功分配的非空数组仍留在 observations，调用方须用同一 context 释放。
 * @note 数组只借存 violation cause AST 指针，不拥有节点；count 为零时不分配缓冲区。
 */
TZrBool ZrParser_DataflowOwnership_ObservationsAllocate(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations) {
    TZrSize boolBytes;
    TZrSize pointerBytes;
    TZrSize ownerIndexBytes;
    TZrSize index;

    if (context == ZR_NULL || context->state == ZR_NULL || observations == ZR_NULL) {
        return ZR_FALSE;
    }
    observations->count = context->referenceFacts.length;
    /* 无引用可观察时保留零计数的空状态，让调用方走同一个清理入口。 */
    if (observations->count == 0) {
        return ZR_TRUE;
    }
    /* 五个数组与 referenceFacts 一一对齐；原始分配失败不会触发 GC 重试。 */
    boolBytes = observations->count * sizeof(TZrBool);
    pointerBytes = observations->count * sizeof(SZrAstNode *);
    ownerIndexBytes = observations->count * sizeof(TZrSize);
    observations->moveSeen = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            context->state->global, boolBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->releaseSeen = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            context->state->global, boolBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->violationSeen = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            context->state->global, boolBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->violationCauses = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(
            context->state->global, pointerBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->violationOwnerIndices = (TZrSize *)ZrCore_Memory_RawMallocWithType(
            context->state->global, ownerIndexBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    /* 单个缓冲区失败就不启动 CFG；上层目前忽略编译诊断发布失败。
     * BUG: 唯一值按值传递后再读取的合法输入可绕过 use-after-move 拒绝并生成函数。
     * TODO: LSP 也忽略该解析结果；需在观察缓冲区分配点注入失败，核对 best-effort 诊断契约。
     */
    if (observations->moveSeen == ZR_NULL ||
        observations->releaseSeen == ZR_NULL ||
        observations->violationSeen == ZR_NULL ||
        observations->violationCauses == ZR_NULL ||
        observations->violationOwnerIndices == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 只有全部缓冲区就绪才清零标志；owner 索引另用无效哨兵表示无 owner。 */
    ZrCore_Memory_RawSet(observations->moveSeen, 0, boolBytes);
    ZrCore_Memory_RawSet(observations->releaseSeen, 0, boolBytes);
    ZrCore_Memory_RawSet(observations->violationSeen, 0, boolBytes);
    ZrCore_Memory_RawSet(observations->violationCauses, 0, pointerBytes);
    for (index = 0; index < observations->count; index++) {
        observations->violationOwnerIndices[index] =
                ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
    }
    return ZR_TRUE;
}

/** @brief 释放本次解析已分配的观察缓冲区并归零状态，AST cause 始终不由此函数释放。
 * @pre context 与 Allocate 时的语义状态一致；允许 count 非零但只有部分缓冲区分配成功。
 */
void ZrParser_DataflowOwnership_ObservationsFree(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations) {
    TZrSize boolBytes;
    TZrSize pointerBytes;
    TZrSize ownerIndexBytes;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        observations == ZR_NULL ||
        observations->count == 0) {
        return;
    }
    boolBytes = observations->count * sizeof(TZrBool);
    pointerBytes = observations->count * sizeof(SZrAstNode *);
    ownerIndexBytes = observations->count * sizeof(TZrSize);
    /* 局部宏只释放非空原始数组；大小与 Allocate 的元素计数和类型配对。 */
#define FREE_OBSERVATION(pointer, bytes) \
    do { \
        if ((pointer) != ZR_NULL) { \
            ZrCore_Memory_RawFreeWithType(context->state->global, \
                                         (pointer), \
                                         (bytes), \
                                         ZR_MEMORY_NATIVE_TYPE_ARRAY); \
        } \
    } while (0)
    FREE_OBSERVATION(observations->moveSeen, boolBytes);
    FREE_OBSERVATION(observations->releaseSeen, boolBytes);
    FREE_OBSERVATION(observations->violationSeen, boolBytes);
    FREE_OBSERVATION(observations->violationCauses, pointerBytes);
    FREE_OBSERVATION(observations->violationOwnerIndices, ownerIndexBytes);
#undef FREE_OBSERVATION
    /* 清掉计数和借用指针，防止调用方再次释放同一批临时数组。 */
    memset(observations, 0, sizeof(*observations));
}
