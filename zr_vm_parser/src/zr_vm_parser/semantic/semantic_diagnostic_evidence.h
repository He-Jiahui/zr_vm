#ifndef ZR_VM_PARSER_SEMANTIC_DIAGNOSTIC_EVIDENCE_H
#define ZR_VM_PARSER_SEMANTIC_DIAGNOSTIC_EVIDENCE_H

#include "zr_vm_parser/semantic.h"

/* Known range evidence requires a live AST owner and immutable live input
 * strings. Native storage, owner pins, and budget usage publish together. */
TZrBool ZrParser_SemanticDiagnosticEvidence_Append(
        SZrSemanticContext *context, const SZrSemanticDiagnosticFact *fact);

#endif
