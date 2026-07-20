#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_PUBLISHED_DIAGNOSTICS_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_PUBLISHED_DIAGNOSTICS_H

#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

TZrBool ZrParser_SemanticQueryPublished_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticDiagnosticFact *fact);

#endif
