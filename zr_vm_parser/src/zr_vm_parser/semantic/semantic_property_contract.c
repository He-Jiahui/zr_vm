#include "zr_vm_parser/semantic.h"

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
    ZrCore_Array_Push(
            context->state,
            &context->propertyContracts,
            (TZrPtr)contract);
    return ZR_TRUE;
}
