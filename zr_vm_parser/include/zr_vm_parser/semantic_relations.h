#ifndef ZR_VM_PARSER_SEMANTIC_RELATIONS_H
#define ZR_VM_PARSER_SEMANTIC_RELATIONS_H

#include "zr_vm_parser/conf.h"

typedef enum EZrSemanticRelationKind {
    ZR_SEMANTIC_RELATION_UNKNOWN = 0,
    ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION,
    ZR_SEMANTIC_RELATION_OVERRIDE,
    ZR_SEMANTIC_RELATION_IMPLEMENTATION,
    ZR_SEMANTIC_RELATION_BASE_TYPE,
    ZR_SEMANTIC_RELATION_CONSTRUCTOR,
    ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR,
    ZR_SEMANTIC_RELATION_ALIAS_TARGET,
    ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN,
} EZrSemanticRelationKind;

typedef struct SZrSemanticRelationFact {
    EZrSemanticRelationKind kind;
    TZrSymbolId sourceSymbolId;
    TZrSymbolId targetSymbolId;
    TZrTypeId sourceTypeId;
    TZrTypeId targetTypeId;
    SZrFileRange sourceRange;
    SZrFileRange targetRange;
    SZrString *externalOriginUri;
    TZrBool hasSourceRange;
    TZrBool hasTargetRange;
    TZrBool isExternal;
} SZrSemanticRelationFact;

struct SZrSemanticContext;
typedef struct SZrSemanticContext SZrSemanticContext;

ZR_PARSER_API void ZrParser_SemanticRelations_Init(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticRelations_Reset(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticRelations_Free(SZrSemanticContext *context);
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_Append(
        SZrSemanticContext *context,
        const SZrSemanticRelationFact *fact);
/* Publishes property-to-accessor edges from existing canonical contracts only. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishPropertyContracts(
        SZrSemanticContext *context);
/* Publishes declaration-to-write edges from resolved reference facts only. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishReferenceDefinitions(
        SZrSemanticContext *context);
/* Publishes external import origins from existing source scope facts only. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishImportOrigins(
        SZrSemanticContext *context);

#endif // ZR_VM_PARSER_SEMANTIC_RELATIONS_H
