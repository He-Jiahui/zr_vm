#include "dataflow_ownership_symbols.h"

#include <string.h>

#include "zr_vm_parser/semantic_facts.h"
/* 按语义 symbolId 查找本次分析对应的唯一流状态槽。 */
TZrBool ZrParser_DataflowOwnership_SymbolFind(
        const SZrSemanticOwnershipSymbolMap *map,
        TZrSymbolId symbolId,
        TZrSize *outIndex) {
    TZrSize index;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (map == ZR_NULL ||
        !map->entries.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (index = 0; index < map->entries.length; index++) {
        const SZrSemanticOwnershipSymbolEntry *candidate =
                (const SZrSemanticOwnershipSymbolEntry *)ZrCore_Array_Get(
                        (SZrArray *)&map->entries,
                        index);
        if (candidate != ZR_NULL && candidate->symbolId == symbolId) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}
/* 借出表内条目供状态传播读取；地址随表释放或扩容失效。 */
SZrSemanticOwnershipSymbolEntry *ZrParser_DataflowOwnership_SymbolEntry(
        SZrSemanticOwnershipSymbolMap *map,
        TZrSize index) {
    if (map == ZR_NULL || !map->entries.isValid || index >= map->entries.length) {
        return ZR_NULL;
    }
    return (SZrSemanticOwnershipSymbolEntry *)ZrCore_Array_Get(&map->entries, index);
}
/* 类型事实未直接带限定时，按 typeId 回查语义类型记录。 */
static EZrOwnershipQualifier ownership_symbol_type_qualifier(
        const SZrSemanticContext *context,
        TZrTypeId typeId) {
    TZrSize index;

    if (context == ZR_NULL ||
        !context->types.isValid ||
        typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    for (index = 0; index < context->types.length; index++) {
        const SZrSemanticTypeRecord *type =
                (const SZrSemanticTypeRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->types,
                        index);
        if (type != ZR_NULL && type->id == typeId) {
            return type->ownershipQualifier;
        }
    }
    return ZR_OWNERSHIP_QUALIFIER_NONE;
}
/* 引用 fact 未带限定时，先从同一 symbol 的声明 fact 回补。 */
static EZrOwnershipQualifier ownership_symbol_declaration_qualifier(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
            fact->symbolId == symbolId &&
            fact->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
            return fact->ownershipQualifier;
        }
    }
    return ZR_OWNERSHIP_QUALIFIER_NONE;
}
/* 按引用、声明、类型的优先级解析限定，保留最具体的来源。 */
static EZrOwnershipQualifier ownership_symbol_reference_qualifier(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact) {
    EZrOwnershipQualifier qualifier;

    if (fact == ZR_NULL) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
    if (fact->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        return fact->ownershipQualifier;
    }
    qualifier = ownership_symbol_declaration_qualifier(context, fact->symbolId);
    return qualifier != ZR_OWNERSHIP_QUALIFIER_NONE
                   ? qualifier
                   : ownership_symbol_type_qualifier(context, fact->typeId);
}
/* 找到已解析声明引用，为缺少声明 ownership fact 的槽补上位置。 */
static const SZrSemanticReferenceFact *ownership_symbol_declaration_reference(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
            fact->isResolved &&
            fact->symbolId == symbolId) {
            return fact;
        }
    }
    return ZR_NULL;
}
/* 复用同一 symbol 已登记的声明 region，保持生命周期身份稳定。 */
static TZrLifetimeRegionId ownership_symbol_declaration_region(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (index = 0; index < context->ownershipFacts.length; index++) {
        const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION &&
            fact->symbolId == symbolId &&
            fact->lifetimeRegionId != ZR_SEMANTIC_ID_INVALID) {
            return fact->lifetimeRegionId;
        }
    }
    return ZR_SEMANTIC_ID_INVALID;
}
/* TODO: RuntimeRoot 会清掉 source range，submission capture 在编译期先播种、随后被 SemanticContext_Reset 清掉本轮 declaration facts；identifier fact 仍保留 qualifier，MapBuild 可建槽和 region 后因无 declaration reference 跳过本 fact。确认非 source origin 是否要求合成 declaration/lifetime 锚点，当前未证明诊断或区域结果错误；证据入口见 zr_vm_parser/src/zr_vm_parser/type_system.c:1223-1255,1261-1301、zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c:157-219、zr_vm_parser/src/zr_vm_parser/compiler.c:1051,720、zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_identifier_facts.c:29-60。 */
static TZrBool ownership_symbol_append_declaration_fact(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        EZrOwnershipQualifier qualifier,
        TZrLifetimeRegionId regionId) {
    SZrSemanticOwnershipFact fact;

    if (context == ZR_NULL || reference == ZR_NULL) {
        return ZR_TRUE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = reference->node;
    fact.range = reference->range;
    fact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION;
    fact.qualifier = qualifier;
    fact.symbolId = reference->symbolId;
    fact.lifetimeRegionId = regionId;
    fact.ownerLifetimeRegionId = regionId;
    return ZrParser_SemanticFacts_AppendOwnership(context, &fact);
}
/* 首次遇到限定 symbol 时建立唯一槽，并补齐其声明 region。 */
static TZrBool ownership_symbol_map_add(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map,
        TZrSymbolId symbolId,
        EZrOwnershipQualifier qualifier) {
    SZrSemanticOwnershipSymbolEntry entry;
    const SZrSemanticReferenceFact *declaration;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        map == ZR_NULL ||
        !map->entries.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        return ZR_FALSE;
    }
    if (!ZrParser_DataflowOwnership_SymbolFind(map, symbolId, ZR_NULL)) {
        memset(&entry, 0, sizeof(entry));
        entry.symbolId = symbolId;
        entry.qualifier = qualifier;
        entry.ownerIndex = ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
        entry.regionId = ownership_symbol_declaration_region(context, symbolId);
        if (entry.regionId == ZR_SEMANTIC_ID_INVALID) {
            entry.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(context);
            declaration = ownership_symbol_declaration_reference(context, symbolId);
            if (!ownership_symbol_append_declaration_fact(context,
                                                          declaration,
                                                          qualifier,
                                                          entry.regionId)) {
                return ZR_FALSE;
            }
        }
        ZrCore_Array_Push(context->state, &map->entries, &entry);
    }
    return ZR_TRUE;
}
/* 置空一次性映射容器，供后续 Build 分配本轮槽数组。 */
void ZrParser_DataflowOwnership_SymbolMapConstruct(SZrSemanticOwnershipSymbolMap *map) {
    if (map != ZR_NULL) {
        ZrCore_Array_Construct(&map->entries);
    }
}
/* BUG: 合法 Unique 槽表建成后，OwnerSetPoolInit 的单次 RawMalloc 失败使 resolver/publisher 在违规筛选前返回 false；compile_script 丢弃状态并继续，导致 VALUE 移后重读未被拒绝（链见 zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c:734-738、zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c:112-121、zr_vm_parser/src/zr_vm_parser/compiler.c:824-828,1090-1111）。TODO: LSP 的相同失败由 zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c:165-175 忽略，而 zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c:224-235 按 best-effort 返回成功；其诊断完整性契约仍待确认。 */
TZrBool ZrParser_DataflowOwnership_SymbolMapBuild(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map) {
    TZrSize capacity;
    TZrSize index;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        map == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    capacity = context->referenceFacts.length > 0
                       ? context->referenceFacts.length
                       : ZR_PARSER_INITIAL_CAPACITY_TINY;
    ZrCore_Array_Init(context->state,
                      &map->entries,
                      sizeof(SZrSemanticOwnershipSymbolEntry),
                      capacity); // BUG: 已解析限定引用对应的 entries 初始分配失败时 Init 仍置 isValid；启用 ZR_DEBUG 时首个 Push 断言中止，未启用时对空目标 RawCopy（UB），Build 无法返回 false。
    /* 仅保留已解析、有效且带限定的声明/读/写引用；map_add 按 symbolId 去重以稳定槽位。 */
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts,
                        index);
        EZrOwnershipQualifier qualifier;

        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID) {
            continue;
        }
        qualifier = ownership_symbol_reference_qualifier(context, fact);
        if (qualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
            continue;
        }
        if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
            fact->kind == ZR_SEMANTIC_REFERENCE_READ ||
            fact->kind == ZR_SEMANTIC_REFERENCE_WRITE) {
            if (!ownership_symbol_map_add(context,
                                          map,
                                          fact->symbolId,
                                          qualifier)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}
/* 用创建时的 semantic context allocator 回收临时槽数组。 */
void ZrParser_DataflowOwnership_SymbolMapFree(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map) {
    if (context != ZR_NULL && context->state != ZR_NULL && map != ZR_NULL) {
        ZrCore_Array_Free(context->state, &map->entries);
    }
}
