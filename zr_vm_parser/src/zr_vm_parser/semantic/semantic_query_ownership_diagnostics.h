#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_OWNERSHIP_DIAGNOSTICS_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_OWNERSHIP_DIAGNOSTICS_H

#include "zr_vm_parser/semantic_query.h"

TZrBool ZrParser_SemanticQueryOwnership_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticOwnershipFact *fact);

#endif
