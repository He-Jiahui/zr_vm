#include "zr_vm_parser/semantic.h"

/*
 * 在单个语义快照中按稳定 SymbolId 找已登记符号；返回的记录仍由 symbols 数组借用，
 * 调用方只在本次校验中读取，不应跨数组增长或上下文重置保存该指针。
 */
static const SZrSemanticSymbolRecord *semantic_property_find_symbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols,
                        index);
        if (symbol != ZR_NULL && symbol->id == symbolId) {
            return symbol;
        }
    }
    return ZR_NULL;
}

/**
 * @brief 校验 property 的规范可见符号与 value 参数后，将契约记录追加到语义快照。
 * @pre SymbolId/TypeId 必须能在传入 context 的当前快照中解析；可定位范围也须属于该 context 的源码快照。
 *      不能混用已重置快照中的编号或位置；导入数据没有 source 位置时使用不可定位的占位范围。
 * @return 输入无效、链接基础条件不成立或 property SymbolId 重复时返回 ZR_FALSE。
 * @note 数组只复制契约结构体字节；范围中的 source 字符串不转移所有权，仍由快照外的 VM 字符串生命周期保证有效。
 */
TZrBool ZrParser_Semantic_PublishPropertyContract(
        SZrSemanticContext *context,
        const SZrSemanticPropertyContract *contract) {
    const SZrSemanticSymbolRecord *propertySymbol;
    const SZrSemanticSymbolRecord *setterValueSymbol;
    const SZrSemanticSymbolRecord *initializerValueSymbol;

    if (context == ZR_NULL || contract == ZR_NULL ||
        !context->propertyContracts.isValid ||
        contract->propertySymbolId == ZR_SEMANTIC_ID_INVALID ||
        contract->propertyTypeId == ZR_SEMANTIC_ID_INVALID ||
        (contract->getterSymbolId == ZR_SEMANTIC_ID_INVALID &&
         contract->setterSymbolId == ZR_SEMANTIC_ID_INVALID &&
         contract->initializerSymbolId == ZR_SEMANTIC_ID_INVALID)) {
        return ZR_FALSE;
    }
    propertySymbol = semantic_property_find_symbol(
            context,
            contract->propertySymbolId);
    if (propertySymbol == ZR_NULL ||
        propertySymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_PROPERTY ||
        propertySymbol->typeId != contract->propertyTypeId) {
        return ZR_FALSE;
    }
    setterValueSymbol = semantic_property_find_symbol(
            context,
            contract->setterValueSymbolId);
    initializerValueSymbol = semantic_property_find_symbol(
            context,
            contract->initializerValueSymbolId);
    if ((contract->setterSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                (setterValueSymbol != ZR_NULL) ||
        (contract->initializerSymbolId != ZR_SEMANTIC_ID_INVALID) !=
                (initializerValueSymbol != ZR_NULL) ||
        (setterValueSymbol != ZR_NULL &&
         (setterValueSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_PARAMETER ||
          setterValueSymbol->typeId != contract->propertyTypeId)) ||
        (initializerValueSymbol != ZR_NULL &&
         (initializerValueSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_PARAMETER ||
          initializerValueSymbol->typeId != contract->propertyTypeId))) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *existing =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        &context->propertyContracts,
                        index);
        if (existing != ZR_NULL &&
            existing->propertySymbolId == contract->propertySymbolId) {
            return ZR_FALSE;
        }
    }
    /*
     * BUG: propertyContracts 扩容时 Array_Push 可能因分配失败丢失原缓冲并向空指针复制，
     * 而该接口无法收到失败状态，随后仍返回成功；源属性或导入属性数量达到容量且扩容失败时会崩溃并破坏既有契约。
     * 需让数组追加保留旧缓冲并显式传播失败，再由此处返回 ZR_FALSE。
     */
    ZrCore_Array_Push(
            context->state,
            &context->propertyContracts,
            (TZrPtr)contract);
    return ZR_TRUE;
}
