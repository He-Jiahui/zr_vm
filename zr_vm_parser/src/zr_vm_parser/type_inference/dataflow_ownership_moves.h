#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_MOVES_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_MOVES_H

#include "zr_vm_parser/semantic_facts.h"

TZrBool ZrParser_DataflowOwnership_StatementMovesRead(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);
TZrBool ZrParser_DataflowOwnership_StatementWeakReadRequiresWake(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_MOVES_H
