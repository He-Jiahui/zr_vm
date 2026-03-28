//
// Minimal semantic/HIR scaffold for the staged parser -> semantic -> compiler migration.
//

#ifndef ZR_VM_PARSER_SEMANTIC_H
#define ZR_VM_PARSER_SEMANTIC_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"

typedef TZrUInt32 TZrTypeId;
typedef TZrUInt32 TZrSymbolId;
typedef TZrUInt32 TZrOverloadSetId;
typedef TZrUInt32 TZrLifetimeRegionId;

enum EZrSemanticTypeKind {
    ZR_SEMANTIC_TYPE_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_TYPE_KIND_VALUE,
    ZR_SEMANTIC_TYPE_KIND_REFERENCE,
    ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE,
};

typedef enum EZrSemanticTypeKind EZrSemanticTypeKind;

enum EZrSemanticSymbolKind {
    ZR_SEMANTIC_SYMBOL_KIND_UNKNOWN = 0,
    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
    ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
    ZR_SEMANTIC_SYMBOL_KIND_TYPE,
    ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
    ZR_SEMANTIC_SYMBOL_KIND_FIELD,
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
    TZrBool callsClose;
    TZrBool callsDestructor;
} SZrDeterministicCleanupStep;

typedef struct SZrSemanticContext {
    SZrState *state;
    TZrTypeId nextTypeId;
    TZrSymbolId nextSymbolId;
    TZrOverloadSetId nextOverloadSetId;
    TZrLifetimeRegionId nextLifetimeRegionId;
    SZrArray types;             // SZrSemanticTypeRecord
    SZrArray symbols;           // SZrSemanticSymbolRecord
    SZrArray overloadSets;      // SZrSemanticOverloadSetRecord
    SZrArray cleanupPlan;       // SZrDeterministicCleanupStep
    SZrArray templateSegments;  // SZrTemplateSegment
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

ZR_PARSER_API TZrTypeId ZrParser_Semantic_RegisterInferredType(SZrSemanticContext *context,
                                                       const SZrInferredType *type,
                                                       EZrSemanticTypeKind kind,
                                                       SZrString *name,
                                                       SZrAstNode *astNode);
ZR_PARSER_API TZrTypeId ZrParser_Semantic_RegisterNamedType(SZrSemanticContext *context,
                                                    SZrString *name,
                                                    EZrSemanticTypeKind kind,
                                                    SZrAstNode *astNode);
ZR_PARSER_API TZrSymbolId ZrParser_Semantic_RegisterSymbol(SZrSemanticContext *context,
                                                   SZrString *name,
                                                   EZrSemanticSymbolKind kind,
                                                   TZrTypeId typeId,
                                                   TZrOverloadSetId overloadSetId,
                                                   SZrAstNode *astNode,
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

ZR_PARSER_API SZrHirModule *ZrParser_HirModule_New(SZrState *state,
                                           SZrSemanticContext *context,
                                           SZrAstNode *rootAst);
ZR_PARSER_API void ZrParser_HirModule_Free(SZrState *state, SZrHirModule *module);

#endif // ZR_VM_PARSER_SEMANTIC_H
