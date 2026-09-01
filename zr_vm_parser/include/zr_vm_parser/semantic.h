//
// Minimal semantic/HIR scaffold for the staged parser -> semantic -> compiler migration.
//

#ifndef ZR_VM_PARSER_SEMANTIC_H
#define ZR_VM_PARSER_SEMANTIC_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"

#ifndef ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
#define ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
typedef TZrUInt32 TZrTypeId;
typedef TZrUInt32 TZrSymbolId;
typedef TZrUInt32 TZrOverloadSetId;
typedef TZrUInt32 TZrLifetimeRegionId;
#endif

#ifndef ZR_VM_PARSER_SEMANTIC_SCOPE_ID_TYPE_DECLARED
#define ZR_VM_PARSER_SEMANTIC_SCOPE_ID_TYPE_DECLARED
typedef TZrUInt32 TZrSemanticScopeId;
#endif

// Semantic IDs reserve 0 as "invalid / not assigned"; allocation starts at 1.
#ifndef ZR_SEMANTIC_ID_INVALID
#define ZR_SEMANTIC_ID_INVALID ((TZrUInt32)0U)
#endif
#ifndef ZR_SEMANTIC_ID_FIRST
#define ZR_SEMANTIC_ID_FIRST ((TZrUInt32)1U)
#endif

#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_relations.h"
#include "zr_vm_parser/canonical_type.h"

enum EZrSemanticTypeKind {
    ZR_SEMANTIC_TYPE_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_TYPE_KIND_VALUE,
    ZR_SEMANTIC_TYPE_KIND_REFERENCE,
    ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
    ZR_SEMANTIC_TYPE_KIND_GENERIC_PARAMETER,
    ZR_SEMANTIC_TYPE_KIND_UNION,
};

typedef enum EZrSemanticTypeKind EZrSemanticTypeKind;

enum EZrSemanticSymbolKind {
    ZR_SEMANTIC_SYMBOL_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
    ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
    ZR_SEMANTIC_SYMBOL_KIND_FIELD,
    ZR_SEMANTIC_SYMBOL_KIND_PROPERTY,
};

typedef enum EZrSemanticSymbolKind EZrSemanticSymbolKind;

enum EZrDeterministicCleanupKind {
    ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE = 0,
    ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD,
    ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD,
};

typedef enum EZrDeterministicCleanupKind EZrDeterministicCleanupKind;

typedef struct SZrSemanticTypeRecord {
    TZrTypeId id;
    EZrSemanticTypeKind kind;
    EZrValueType baseType;
    EZrOwnershipQualifier ownershipQualifier;
    SZrString *name;
    SZrAstNode *astNode;
    SZrInferredType inferredType;
} SZrSemanticTypeRecord;

typedef struct SZrSemanticSymbolRecord {
    TZrSymbolId id;
    EZrSemanticSymbolKind kind;
    SZrString *name;
    TZrTypeId typeId;
    TZrOverloadSetId overloadSetId;
    SZrAstNode *astNode;
    SZrFileRange location;
} SZrSemanticSymbolRecord;

typedef enum EZrSemanticScopeKind {
    ZR_SEMANTIC_SCOPE_KIND_MODULE = 0,
    ZR_SEMANTIC_SCOPE_KIND_TYPE,
    ZR_SEMANTIC_SCOPE_KIND_FUNCTION,
    ZR_SEMANTIC_SCOPE_KIND_BLOCK,
    ZR_SEMANTIC_SCOPE_KIND_GENERIC
} EZrSemanticScopeKind;

typedef struct SZrSemanticScopeFact {
    TZrSemanticScopeId id;
    TZrSemanticScopeId parentScopeId;
    EZrSemanticScopeKind kind;
    SZrFileRange range;
    TZrSymbolId ownerSymbolId;
    TZrBool isStaticContext;
} SZrSemanticScopeFact;

typedef enum EZrSemanticCallEdgeResolution {
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_UNKNOWN = 0,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_CALLER_UNAVAILABLE,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_UNRESOLVED,
    ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_DECLARATION_UNAVAILABLE
} EZrSemanticCallEdgeResolution;

/*
 * A call edge is produced from existing CALL references and lexical scope
 * ownership. It never resolves an endpoint by spelling.
 */
typedef struct SZrSemanticCallEdgeFact {
    TZrSymbolId callerSymbolId;
    TZrSymbolId targetSymbolId;
    TZrTypeId callableTypeId;
    SZrFileRange callSiteRange;
    SZrFileRange targetDeclarationRange;
    EZrSemanticCallEdgeResolution resolution;
    TZrBool hasTargetDeclarationRange;
} SZrSemanticCallEdgeFact;

/*
 * A producer publishes one candidate for every symbol it has already bound to
 * a lexical scope. The query only projects this fact; it never searches names
 * through the global symbol registry to infer visibility.
 */
typedef struct SZrSemanticVisibleSymbolFact {
    TZrSemanticScopeId scopeId;
    TZrSymbolId symbolId;
    TZrSymbolId ownerSymbolId;
    EZrAccessModifier access;
    TZrUInt32 declarationOrder;
    SZrFileRange declarationRange;
    SZrFileRange definitionRange;
    SZrString *signatureDisplay;
    SZrString *externalOriginUri;
    SZrFileRange externalOriginRange;
    TZrBool hasDefinitionRange;
    TZrBool hasExternalOriginRange;
    TZrBool isHoisted;
    TZrBool isAccessible;
    TZrBool isReceiverMember;
    TZrBool isStatic;
    TZrBool isImport;
    TZrBool isAlias;
    TZrBool isGenericParameter;
} SZrSemanticVisibleSymbolFact;

typedef struct SZrSemanticOverloadSetRecord {
    TZrOverloadSetId id;
    SZrString *name;
    SZrArray members; // TZrSymbolId
} SZrSemanticOverloadSetRecord;

typedef struct SZrTemplateSegment {
    TZrBool isInterpolation;
    SZrString *staticText;
    SZrAstNode *expression;
} SZrTemplateSegment;

typedef struct SZrDeterministicCleanupStep {
    EZrDeterministicCleanupKind kind;
    TZrLifetimeRegionId regionId;
    TZrLifetimeRegionId ownerRegionId;
    TZrSymbolId symbolId;
    TZrInt32 declarationOrder;
    EZrOwnershipQualifier ownershipQualifier;
    EZrOwnershipBuiltinKind ownershipBuiltinKind;
    TZrBool callsClose;
    TZrBool callsDestructor;
} SZrDeterministicCleanupStep;

typedef struct SZrSemanticPropertyContract {
    TZrSymbolId propertySymbolId;
    TZrTypeId propertyTypeId;
    TZrSymbolId getterSymbolId;
    TZrSymbolId setterSymbolId;
    TZrSymbolId initializerSymbolId;
    TZrSymbolId setterValueSymbolId;
    TZrSymbolId initializerValueSymbolId;
    TZrTypeId getterCallableTypeId;
    TZrTypeId setterCallableTypeId;
    TZrTypeId initializerCallableTypeId;
    EZrAccessModifier access;
    EZrAccessModifier getterAccess;
    EZrAccessModifier setterAccess;
    EZrAccessModifier initializerAccess;
    TZrUInt32 modifierFlags;
    EZrCanonicalReceiverEffect receiverEffect;
    EZrReferenceAccess referenceAccess;
    TZrBool exportsWritableRef;
    TZrBool isStatic;
    SZrFileRange declarationRange;
    SZrFileRange selectionRange;
} SZrSemanticPropertyContract;

typedef struct SZrSemanticContext {
    SZrState *state;
    TZrTypeId nextTypeId;
    TZrSymbolId nextSymbolId;
    TZrOverloadSetId nextOverloadSetId;
    TZrLifetimeRegionId nextLifetimeRegionId;
    TZrSemanticScopeId nextScopeId;
    SZrArray canonicalTypes;    // SZrCanonicalTypeNode
    SZrArray canonicalTypeHashBuckets; // internal TZrUInt32 bucket heads
    SZrArray canonicalTypeHashNext; // internal TZrUInt32 collision links
    SZrArray canonicalTypeDefinitions; // internal canonical TypeDef records
    SZrArray types;             // SZrSemanticTypeRecord
    SZrArray symbols;           // SZrSemanticSymbolRecord
    SZrArray scopeFacts;        // SZrSemanticScopeFact
    SZrArray visibleSymbolFacts; // SZrSemanticVisibleSymbolFact
    SZrArray overloadSets;      // SZrSemanticOverloadSetRecord
    SZrArray cleanupPlan;       // SZrDeterministicCleanupStep
    SZrArray templateSegments;  // SZrTemplateSegment
    SZrArray queryDiagnostics;  // SZrStructuredDiagnostic
    TZrBool queryDiagnosticsMaterialized;
    const SZrAstNode *queryDiagnosticsScopeRoot; // NULL for module scope
    SZrArray expressionFacts;   // SZrSemanticExpressionFact
    SZrArray referenceFacts;    // SZrSemanticReferenceFact
    SZrArray numericFacts;      // SZrSemanticNumericFact
    SZrArray reachabilityFacts; // SZrSemanticReachabilityFact
    SZrArray logicalFacts;      // SZrSemanticLogicalFact
    SZrArray ownershipFacts;    // SZrSemanticOwnershipFact
    SZrArray ownershipIntrinsicFacts; // SZrOwnershipIntrinsicFact
    SZrArray receiverGuardFacts; // SZrReceiverGuardFact
    SZrArray diagnosticFacts;   // SZrSemanticDiagnosticFact
    SZrArray propertyContracts; // SZrSemanticPropertyContract
    SZrArray relationFacts;     // SZrSemanticRelationFact
    SZrArray callEdgeFacts;     // SZrSemanticCallEdgeFact
    SZrArray typeDisplayAliasFacts; // SZrSemanticTypeDisplayAliasFact
    SZrArray documentationFacts; // SZrSemanticDocumentationFact
} SZrSemanticContext;

typedef struct SZrHirModule {
    SZrAstNode *rootAst;
    SZrSemanticContext *semantic;
} SZrHirModule;

ZR_PARSER_API SZrSemanticContext *ZrParser_SemanticContext_New(SZrState *state);
ZR_PARSER_API void ZrParser_SemanticContext_Free(SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_SemanticContext_Reset(SZrSemanticContext *context);

ZR_PARSER_API TZrTypeId ZrParser_Semantic_ReserveTypeId(SZrSemanticContext *context);
ZR_PARSER_API TZrSymbolId ZrParser_Semantic_ReserveSymbolId(SZrSemanticContext *context);
ZR_PARSER_API TZrOverloadSetId ZrParser_Semantic_ReserveOverloadSetId(SZrSemanticContext *context);
ZR_PARSER_API TZrLifetimeRegionId ZrParser_Semantic_ReserveLifetimeRegionId(SZrSemanticContext *context);
ZR_PARSER_API TZrSemanticScopeId ZrParser_Semantic_ReserveScopeId(SZrSemanticContext *context);

ZR_PARSER_API TZrTypeId ZrParser_Semantic_RegisterInferredType(SZrSemanticContext *context,
                                                       const SZrInferredType *type,
                                                       EZrSemanticTypeKind kind,
                                                       SZrString *name,
                                                       SZrAstNode *astNode);
ZR_PARSER_API TZrTypeId ZrParser_Semantic_RegisterNamedType(SZrSemanticContext *context,
                                                    SZrString *name,
                                                    EZrSemanticTypeKind kind,
                                                    SZrAstNode *astNode);
ZR_PARSER_API TZrBool ZrParser_Semantic_RegisterCanonicalType(SZrSemanticContext *context,
                                                       TZrTypeId typeId,
                                                       EZrSemanticTypeKind kind,
                                                       SZrString *name,
                                                       SZrAstNode *astNode);
ZR_PARSER_API TZrSymbolId ZrParser_Semantic_RegisterSymbol(SZrSemanticContext *context,
                                                   SZrString *name,
                                                   EZrSemanticSymbolKind kind,
                                                   TZrTypeId typeId,
                                                   TZrOverloadSetId overloadSetId,
                                                   SZrAstNode *astNode,
                                                   SZrFileRange location);
ZR_PARSER_API TZrSymbolId ZrParser_Semantic_RegisterSymbolWithId(SZrSemanticContext *context,
                                                         TZrSymbolId symbolId,
                                                         SZrString *name,
                                                         EZrSemanticSymbolKind kind,
                                                         TZrTypeId typeId,
                                                         TZrOverloadSetId overloadSetId,
                                                          SZrAstNode *astNode,
                                                          SZrFileRange location);
ZR_PARSER_API const SZrSemanticSymbolRecord *ZrParser_Semantic_FindSymbolById(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId);
ZR_PARSER_API TZrSemanticScopeId ZrParser_Semantic_PublishScopeFact(
        SZrSemanticContext *context,
        const SZrSemanticScopeFact *fact);
ZR_PARSER_API const SZrSemanticScopeFact *ZrParser_Semantic_FindScopeFactById(
        const SZrSemanticContext *context,
        TZrSemanticScopeId scopeId);
ZR_PARSER_API TZrBool ZrParser_Semantic_PublishVisibleSymbolFact(
        SZrSemanticContext *context,
        const SZrSemanticVisibleSymbolFact *fact);
ZR_PARSER_API const SZrSemanticSymbolRecord *ZrParser_Semantic_FindSymbolByNameAndKind(
        const SZrSemanticContext *context,
        SZrString *name,
        EZrSemanticSymbolKind kind);
ZR_PARSER_API TZrBool ZrParser_Semantic_RebindSymbolType(
        SZrSemanticContext *context,
        TZrSymbolId symbolId,
        TZrTypeId typeId);
ZR_PARSER_API TZrBool ZrParser_Semantic_PublishCanonicalTypeSymbol(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrSemanticTypeKind typeKind,
        SZrString *name,
        SZrAstNode *astNode,
        TZrSymbolId symbolId,
        SZrFileRange location);
ZR_PARSER_API TZrOverloadSetId ZrParser_Semantic_GetOrCreateOverloadSet(SZrSemanticContext *context,
                                                                SZrString *name);
ZR_PARSER_API TZrBool ZrParser_Semantic_AddOverloadMember(SZrSemanticContext *context,
                                                TZrOverloadSetId overloadSetId,
                                                TZrSymbolId symbolId);

ZR_PARSER_API TZrBool ZrParser_Semantic_AppendCleanupStep(SZrSemanticContext *context,
                                                const SZrDeterministicCleanupStep *step);
ZR_PARSER_API TZrBool ZrParser_Semantic_AppendTemplateSegment(SZrSemanticContext *context,
                                                    const SZrTemplateSegment *segment);
ZR_PARSER_API TZrBool ZrParser_Semantic_PublishPropertyContract(
        SZrSemanticContext *context,
        const SZrSemanticPropertyContract *contract);

ZR_PARSER_API SZrHirModule *ZrParser_HirModule_New(SZrState *state,
                                           SZrSemanticContext *context,
                                           SZrAstNode *rootAst);
ZR_PARSER_API void ZrParser_HirModule_Free(SZrState *state, SZrHirModule *module);

#endif // ZR_VM_PARSER_SEMANTIC_H
