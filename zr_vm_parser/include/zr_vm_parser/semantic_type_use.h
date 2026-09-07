#ifndef ZR_VM_PARSER_SEMANTIC_TYPE_USE_H
#define ZR_VM_PARSER_SEMANTIC_TYPE_USE_H

#include "zr_vm_parser/semantic_facts.h"

/* Publish during analysis; TypeId and declaration identity belong to context. */
ZR_PARSER_API TZrBool ZrParser_SemanticTypeUse_Publish(
        SZrSemanticContext *context,
        const SZrType *typeUse,
        TZrTypeId typeId,
        TZrBool isResolved);

#endif
