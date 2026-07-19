#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_REGIONS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_REGIONS_H

#include "zr_vm_parser/semantic_facts.h"

typedef struct SZrDataflowOwnershipRegionBinding {
    SZrAstNode *constructNode;
    const SZrSemanticReferenceFact *aliasReference;
    const SZrSemanticReferenceFact *ownerReference;
    EZrOwnershipQualifier qualifier;
    TZrBool isDeclaration;
} SZrDataflowOwnershipRegionBinding;

TZrBool ZrParser_DataflowOwnership_StatementRegionBinding(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        SZrDataflowOwnershipRegionBinding *outBinding);
const SZrSemanticReferenceFact *ZrParser_DataflowOwnership_ConstructTargetRead(
        const SZrSemanticContext *context,
        SZrAstNode *constructNode);
TZrBool ZrParser_DataflowOwnership_StatementReleasesRead(
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);

#endif
