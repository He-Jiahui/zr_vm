#ifndef ZR_VM_PARSER_SEMANTIC_CALLS_H
#define ZR_VM_PARSER_SEMANTIC_CALLS_H

#include "zr_vm_parser/conf.h"

struct SZrSemanticContext;
typedef struct SZrSemanticContext SZrSemanticContext;

ZR_PARSER_API void ZrParser_SemanticCalls_Init(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticCalls_Reset(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticCalls_Free(SZrSemanticContext *context);
/*
 * Projects existing CALL reference facts to stable call edges. A missing
 * caller or target is recorded as an unresolved edge instead of guessed.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticCalls_Publish(SZrSemanticContext *context);

#endif /* ZR_VM_PARSER_SEMANTIC_CALLS_H */
