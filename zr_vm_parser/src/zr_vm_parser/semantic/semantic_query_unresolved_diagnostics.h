#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_UNRESOLVED_DIAGNOSTICS_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_UNRESOLVED_DIAGNOSTICS_H

#include "zr_vm_parser/semantic_query.h"

TZrBool ZrParser_SemanticQueryUnresolved_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact);

#endif
