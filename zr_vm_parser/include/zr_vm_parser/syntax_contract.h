#ifndef ZR_VM_PARSER_SYNTAX_CONTRACT_H
#define ZR_VM_PARSER_SYNTAX_CONTRACT_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/canonical_type.h"

struct SZrSemanticContext;

ZR_PARSER_API TZrBool ZrParser_SyntaxParameter_Normalize(
        struct SZrSemanticContext *context,
        const SZrParameter *parameter,
        TZrTypeId valueTypeId,
        SZrCanonicalParameterContract *outContract);

ZR_PARSER_API TZrTypeId ZrParser_SyntaxCallable_Intern(
        struct SZrSemanticContext *context,
        const SZrAstNodeArray *parameters,
        const TZrTypeId *parameterTypeIds,
        TZrTypeId returnTypeId,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags);

#endif
