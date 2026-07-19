#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_SYMBOLS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_SYMBOLS_H

#include "zr_vm_parser/semantic.h"

#define ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID ((TZrSize)-1)

typedef struct SZrSemanticOwnershipSymbolEntry {
    TZrSymbolId symbolId;
    EZrOwnershipQualifier qualifier;
    TZrLifetimeRegionId regionId;
    TZrSize ownerIndex;
} SZrSemanticOwnershipSymbolEntry;

typedef struct SZrSemanticOwnershipSymbolMap {
    SZrArray entries;
} SZrSemanticOwnershipSymbolMap;

void ZrParser_DataflowOwnership_SymbolMapConstruct(SZrSemanticOwnershipSymbolMap *map);
TZrBool ZrParser_DataflowOwnership_SymbolMapBuild(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map);
void ZrParser_DataflowOwnership_SymbolMapFree(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map);
TZrBool ZrParser_DataflowOwnership_SymbolFind(
        const SZrSemanticOwnershipSymbolMap *map,
        TZrSymbolId symbolId,
        TZrSize *outIndex);
SZrSemanticOwnershipSymbolEntry *ZrParser_DataflowOwnership_SymbolEntry(
        SZrSemanticOwnershipSymbolMap *map,
        TZrSize index);

#endif
