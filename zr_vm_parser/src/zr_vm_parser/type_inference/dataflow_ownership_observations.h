#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OBSERVATIONS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OBSERVATIONS_H

#include "dataflow_ownership_symbols.h"

typedef struct SZrDataflowOwnershipObservations {
    TZrBool *moveSeen;
    TZrBool *releaseSeen;
    TZrBool *violationSeen;
    SZrAstNode **violationCauses;
    TZrSize *violationOwnerIndices;
    TZrSize count;
} SZrDataflowOwnershipObservations;

TZrBool ZrParser_DataflowOwnership_ObservationsAllocate(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations);
void ZrParser_DataflowOwnership_ObservationsFree(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations);
TZrBool ZrParser_DataflowOwnership_AppendObservedFacts(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *symbols,
        const SZrDataflowOwnershipObservations *observations);

#endif
