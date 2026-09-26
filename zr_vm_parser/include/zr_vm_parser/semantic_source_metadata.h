#ifndef ZR_VM_PARSER_SEMANTIC_SOURCE_METADATA_H
#define ZR_VM_PARSER_SEMANTIC_SOURCE_METADATA_H

#include "zr_vm_parser/compiler.h"

/* Source metadata producers shared by compilation and semantic analysis. */
ZR_PARSER_API TZrBool ZrParser_SemanticMetadata_InferUsingResourceType(
        SZrCompilerState *cs, SZrAstNode *resource, SZrInferredType *outType);
ZR_PARSER_API EZrOwnershipBuiltinKind ZrParser_SemanticMetadata_UsingCleanupBuiltin(
        EZrOwnershipQualifier ownershipQualifier);
ZR_PARSER_API TZrBool ZrParser_SemanticMetadata_RecordUsingCleanup(
        SZrCompilerState *cs, SZrAstNode *usingNode, const SZrInferredType *resourceType);
ZR_PARSER_API TZrBool ZrParser_SemanticMetadata_RecordTemplateSegments(
        SZrSemanticContext *context, SZrAstNode *templateNode);

#endif
