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
    SZrString *virtualDeclarationUri;
    TZrBool hasSourceRange;
    TZrBool hasTargetRange;
    TZrBool isExternal;
} SZrSemanticRelationFact;

struct SZrSemanticContext;
typedef struct SZrSemanticContext SZrSemanticContext;
struct SZrCompilerState;
typedef struct SZrCompilerState SZrCompilerState;

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
/* Publishes alias-to-type edges from existing visible alias facts only. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishAliasTargets(
        SZrSemanticContext *context);
/* Publishes a resolved source relation from stable canonical symbol identities only. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishSymbolRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        TZrSymbolId sourceSymbolId,
        TZrSymbolId targetSymbolId);
/* Publishes a resolved source relation from exact declaration identities only. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishSymbolDeclarationRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        const SZrAstNode *sourceDeclaration,
        const SZrAstNode *targetDeclaration);
/* Publishes a resolved type relation from compiler declaration identities only. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        const SZrAstNode *sourceDeclaration,
        const SZrAstNode *targetDeclaration);
/* Publishes a source type-to-constructor edge from exact declaration and symbol identities. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishConstructorRelation(
        SZrSemanticContext *context,
        const SZrAstNode *sourceTypeDeclaration,
        TZrSymbolId constructorSymbolId);
/* Publishes hierarchy/member relations from canonical compiler prototypes. */
ZR_PARSER_API TZrBool ZrParser_SemanticRelations_PublishCompilerContracts(
        SZrCompilerState *compilerState);

#endif // ZR_VM_PARSER_SEMANTIC_RELATIONS_H
