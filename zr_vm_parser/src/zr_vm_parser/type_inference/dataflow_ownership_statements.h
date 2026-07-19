#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_STATEMENTS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_STATEMENTS_H

#include "zr_vm_parser/semantic_facts.h"

TZrBool ZrParser_DataflowOwnership_FactInStatement(
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);

#endif
