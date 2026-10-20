#ifndef ZR_VM_PARSER_SEMANTIC_TYPE_PUBLICATION_H
#define ZR_VM_PARSER_SEMANTIC_TYPE_PUBLICATION_H

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

const SZrCanonicalTypeNode *semantic_type_nominal_definition(
        const SZrSemanticContext *context, TZrTypeId typeId);

ZR_PARSER_API TZrBool ZrParser_TypeEnvironment_RegisterTypeDeclarationWithKind(
        SZrState *state, SZrTypeEnvironment *env, SZrString *typeName,
        SZrAstNode *declarationNode, EZrSemanticTypeKind typeKind);

#endif
