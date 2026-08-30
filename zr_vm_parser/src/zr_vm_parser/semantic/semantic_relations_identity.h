#ifndef ZR_VM_PARSER_SEMANTIC_RELATIONS_IDENTITY_H
#define ZR_VM_PARSER_SEMANTIC_RELATIONS_IDENTITY_H

#include "zr_vm_parser/semantic.h"

TZrBool ZrParser_SemanticRelations_RangesEqual(
        const SZrFileRange *left,
        const SZrFileRange *right);
TZrBool ZrParser_SemanticRelations_FactsEqual(
        const SZrSemanticRelationFact *left,
        const SZrSemanticRelationFact *right);

#endif // ZR_VM_PARSER_SEMANTIC_RELATIONS_IDENTITY_H
