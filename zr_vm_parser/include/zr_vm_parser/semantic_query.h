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
    SZrFileRange callSiteRange;
    SZrFileRange callTargetRange;
    TZrSize argumentCount;
    TZrBool hasNamedArguments;
    TZrBool isMemberCall;
    TZrBool hasResolvedTarget;
    TZrSymbolId targetSymbolId;
    SZrFileRange targetDeclarationRange;
    const SZrArray *argumentMappings;
} SZrParserSemanticCallQuery;

typedef struct SZrParserSemanticCallEdgeQuery {
    TZrSymbolId callerSymbolId;
    TZrSymbolId targetSymbolId;
    TZrTypeId callableTypeId;
    SZrFileRange callSiteRange;
    SZrFileRange targetDeclarationRange;
    EZrSemanticCallEdgeResolution resolution;
    TZrBool hasTargetDeclarationRange;
} SZrParserSemanticCallEdgeQuery;

typedef struct SZrParserSemanticCallCandidateQuery {
    TZrSymbolId symbolId;
    TZrTypeId callableTypeId;
    SZrFileRange declarationRange;
    TZrBool isSelected;
} SZrParserSemanticCallCandidateQuery;

typedef struct SZrParserSemanticSymbolQuery {
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrSymbolId ownerSymbolId;
    EZrSemanticSymbolKind kind;
    EZrSemanticReferenceKind role;
    SZrFileRange referenceRange;
    SZrFileRange declarationRange;
    SZrFileRange definitionRange;
    /* Borrowed from the semantic snapshot; never retain across generations. */
    const SZrAstNode *declarationNode;
    SZrString *displayName;
    SZrString *signatureDisplay;
} SZrParserSemanticSymbolQuery;

typedef struct SZrParserSemanticVisibleSymbolOptions {
    TZrBool includeReceiverMembers;
    TZrBool includeImports;
    TZrBool includeInaccessible;
} SZrParserSemanticVisibleSymbolOptions;

typedef struct SZrParserSemanticRelationQuery {
    EZrSemanticRelationKind kind;
    TZrSymbolId sourceSymbolId;
    TZrSymbolId targetSymbolId;
    TZrTypeId sourceTypeId;
    TZrTypeId targetTypeId;
    SZrString *sourceModuleIdentity;
    SZrString *targetModuleIdentity;
    /* Zero means unavailable; nonzero values participate in edge identity. */
    TZrUInt64 sourceProviderGeneration;
    TZrUInt64 targetProviderGeneration;
    SZrFileRange sourceRange;
    SZrFileRange targetRange;
    SZrString *externalOriginUri;
    SZrString *virtualDeclarationUri;
    TZrBool hasSourceRange;
    TZrBool hasTargetRange;
    TZrBool isExternal;
} SZrParserSemanticRelationQuery;

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
/* outEdges contains snapshot-borrowed value copies, never AST pointers. */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CallEdgesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_OutgoingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId callerSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_IncomingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId targetSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges);
/*
 * Projects the selected target's canonical overload-set membership. Candidate
 * callableTypeId values are declaration signatures; CallAt retains the closed
 * callable TypeId for the selected invocation.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_CallCandidatesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outCandidates);
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
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_SymbolAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticSymbolQuery *outSymbol);
/*
 * Projects exact resolved declarations in stable source-order. outSymbols
 * contains SZrParserSemanticSymbolQuery values and is cleared before reuse.
 * declarationNode and display strings remain borrowed from the snapshot.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_DeclaredSymbols(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outSymbols);
/*
 * outSymbols contains SZrParserSemanticSymbolQuery values, not fact pointers.
 * displayName and signatureDisplay within each value remain borrowed from the
 * semantic snapshot. The caller constructs or reuses the array; this query
 * clears a correctly typed reused array before appending results.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_VisibleSymbols(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        const SZrParserSemanticVisibleSymbolOptions *options,
        SZrArray *outSymbols);
/*
 * outRelations contains copied relation values whose URI and module identity
 * fields are borrowed from the semantic snapshot. Reused arrays are cleared
 * before projection.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_RelationsOfSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_ImplementationsOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_BaseTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_DerivedTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations);
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
