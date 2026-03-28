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

typedef TUInt32 TZrTypeId;
typedef TUInt32 TZrSymbolId;
typedef TUInt32 TZrOverloadSetId;
typedef TUInt32 TZrLifetimeRegionId;

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
    TBool isInterpolation;
    SZrString *staticText;
    SZrAstNode *expression;
} SZrTemplateSegment;

typedef struct SZrDeterministicCleanupStep {
    EZrDeterministicCleanupKind kind;
    TZrLifetimeRegionId regionId;
    TZrLifetimeRegionId ownerRegionId;
    TZrSymbolId symbolId;
    TInt32 declarationOrder;
    TBool callsClose;
    TBool callsDestructor;
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

ZR_PARSER_API SZrSemanticContext *ZrSemanticContextNew(SZrState *state);
ZR_PARSER_API void ZrSemanticContextFree(SZrSemanticContext *context);
ZR_PARSER_API void ZrSemanticContextReset(SZrSemanticContext *context);

ZR_PARSER_API TZrTypeId ZrSemanticReserveTypeId(SZrSemanticContext *context);
ZR_PARSER_API TZrSymbolId ZrSemanticReserveSymbolId(SZrSemanticContext *context);
ZR_PARSER_API TZrOverloadSetId ZrSemanticReserveOverloadSetId(SZrSemanticContext *context);
ZR_PARSER_API TZrLifetimeRegionId ZrSemanticReserveLifetimeRegionId(SZrSemanticContext *context);

ZR_PARSER_API TZrTypeId ZrSemanticRegisterInferredType(SZrSemanticContext *context,
                                                       const SZrInferredType *type,
                                                       EZrSemanticTypeKind kind,
                                                       SZrString *name,
                                                       SZrAstNode *astNode);
ZR_PARSER_API TZrTypeId ZrSemanticRegisterNamedType(SZrSemanticContext *context,
                                                    SZrString *name,
                                                    EZrSemanticTypeKind kind,
                                                    SZrAstNode *astNode);
ZR_PARSER_API TZrSymbolId ZrSemanticRegisterSymbol(SZrSemanticContext *context,
                                                   SZrString *name,
                                                   EZrSemanticSymbolKind kind,
                                                   TZrTypeId typeId,
                                                   TZrOverloadSetId overloadSetId,
                                                   SZrAstNode *astNode,
                                                   SZrFileRange location);
ZR_PARSER_API TZrOverloadSetId ZrSemanticGetOrCreateOverloadSet(SZrSemanticContext *context,
                                                                SZrString *name);
ZR_PARSER_API TBool ZrSemanticAddOverloadMember(SZrSemanticContext *context,
                                                TZrOverloadSetId overloadSetId,
                                                TZrSymbolId symbolId);

ZR_PARSER_API TBool ZrSemanticAppendCleanupStep(SZrSemanticContext *context,
                                                const SZrDeterministicCleanupStep *step);
ZR_PARSER_API TBool ZrSemanticAppendTemplateSegment(SZrSemanticContext *context,
                                                    const SZrTemplateSegment *segment);

ZR_PARSER_API SZrHirModule *ZrHirModuleNew(SZrState *state,
                                           SZrSemanticContext *context,
                                           SZrAstNode *rootAst);
ZR_PARSER_API void ZrHirModuleFree(SZrState *state, SZrHirModule *module);

#endif // ZR_VM_PARSER_SEMANTIC_H
