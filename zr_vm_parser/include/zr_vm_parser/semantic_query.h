#ifndef ZR_VM_PARSER_SEMANTIC_QUERY_H
#define ZR_VM_PARSER_SEMANTIC_QUERY_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/semantic.h"

/*
 * Pointer fields returned by this API are borrowed views into the semantic
 * snapshot. They remain valid only while the owning semantic context remains
 * alive and unchanged. Callers that cross a snapshot boundary retain only
 * stable ids and copied ranges, never an AST or fact pointer.
 *
 * TypeAt has no exactness output slot and therefore fails closed for UNKNOWN
 * and APPROXIMATE expression facts. Queries that expose a fact pointer make
 * its exactness available to the caller, which must not reconstruct semantics
 * from text.
 */
static inline TZrBool ZrParser_SemanticQuery_ExactnessAllowsProjection(
        EZrSemanticFactExactness exactness) {
    return exactness == ZR_SEMANTIC_FACT_EXACT;
}

typedef enum EZrParserSemanticQueryScopeKind {
    ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE = 0,
    ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE
} EZrParserSemanticQueryScopeKind;

typedef struct SZrParserSemanticQueryScope {
    EZrParserSemanticQueryScopeKind kind;
    const SZrAstNode *root;
} SZrParserSemanticQueryScope;

typedef struct SZrParserSemanticQueryFacts {
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticNumericFact *numeric;
    const SZrSemanticReachabilityFact *reachability;
    const SZrSemanticLogicalFact *logical;
    const SZrSemanticOwnershipFact *ownership;
} SZrParserSemanticQueryFacts;

typedef struct SZrParserSemanticQueryDiagnostics {
    const SZrStructuredDiagnostic *items;
    TZrSize count;
} SZrParserSemanticQueryDiagnostics;

typedef struct SZrParserSemanticTypeQuery {
    TZrTypeId typeId;
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *reference;
} SZrParserSemanticTypeQuery;

typedef struct SZrParserSemanticCallQuery {
    TZrTypeId callableTypeId;
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *reference;
    TZrBool hasResolvedTarget;
    TZrSymbolId targetSymbolId;
    SZrFileRange targetDeclarationRange;
} SZrParserSemanticCallQuery;

typedef struct SZrParserSemanticPublicContractQuery {
    TZrUInt64 hash;
    TZrSize exportCount;
} SZrParserSemanticPublicContractQuery;

typedef SZrSemanticPropertyContract SZrParserSemanticPropertyQuery;

struct SZrCompilerState;
typedef struct SZrCompilerState SZrCompilerState;

ZR_PARSER_API void ZrParser_SemanticQueryScope_Module(SZrParserSemanticQueryScope *scope);
ZR_PARSER_API void ZrParser_SemanticQueryScope_Node(SZrParserSemanticQueryScope *scope,
                                                    const SZrAstNode *root);

ZR_PARSER_API TZrBool ZrParser_SemanticQuery_TypeAt(const SZrSemanticContext *context,
                                                    SZrFileRange position,
                                                    const SZrParserSemanticQueryScope *scope,
                                                    SZrInferredType *outType);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CanonicalTypeAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticTypeQuery *outQuery);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CallAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticCallQuery *outQuery);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_FormatCall(
        const SZrSemanticContext *context,
        const SZrParserSemanticCallQuery *query,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DefinitionOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope);
ZR_PARSER_API const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DeclarationOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_DefinitionsOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outDefinitions);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_ReferencesOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_FactsAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryFacts *outFacts);
/*
 * This is an analysis lifecycle operation, not a read-only query. It rebuilds
 * the borrowed diagnostic view for exactly one scope. Call it after semantic
 * facts are resolved; subsequent Diagnostics calls for that same scope do not
 * mutate the semantic context.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_MaterializeDiagnostics(
        SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_Diagnostics(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryDiagnostics *outDiagnostics);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_PublicContract(
        const SZrSemanticContext *context,
        const SZrTypeEnvironment *typeEnvironment,
        const SZrAstNode *moduleRoot,
        SZrParserSemanticPublicContractQuery *outQuery);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_PropertyAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticPropertyQuery *outQuery);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_PropertyBySymbolId(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        SZrParserSemanticPropertyQuery *outQuery);
ZR_PARSER_API SZrAstNode *ZrParser_SemanticQuery_FindUnionDeclarationByTypeName(
        SZrCompilerState *compilerState,
        SZrString *typeName);

#endif // ZR_VM_PARSER_SEMANTIC_QUERY_H
