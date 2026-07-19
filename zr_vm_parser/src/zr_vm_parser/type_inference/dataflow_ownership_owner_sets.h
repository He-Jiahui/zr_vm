#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OWNER_SETS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OWNER_SETS_H

#include "dataflow_ownership_symbols.h"

#define ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN ((TZrSize)0)
#define ZR_DATAFLOW_OWNERSHIP_OWNER_SET_EMPTY ((TZrSize)1)

typedef struct SZrDataflowOwnershipOwnerSetPool {
    SZrArray entries;
} SZrDataflowOwnershipOwnerSetPool;

void ZrParser_DataflowOwnership_OwnerSetPoolConstruct(
        SZrDataflowOwnershipOwnerSetPool *pool);
TZrBool ZrParser_DataflowOwnership_OwnerSetPoolInit(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool);
void ZrParser_DataflowOwnership_OwnerSetPoolFree(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool);
TZrSize ZrParser_DataflowOwnership_OwnerSetSingleton(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize ownerIndex);
TZrSize ZrParser_DataflowOwnership_OwnerSetUnion(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize leftSetId,
        TZrSize rightSetId);
TZrBool ZrParser_DataflowOwnership_OwnerSetIsUnknown(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId);
TZrSize ZrParser_DataflowOwnership_OwnerSetCount(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId);
TZrSize ZrParser_DataflowOwnership_OwnerSetAt(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId,
        TZrSize index);

#endif
