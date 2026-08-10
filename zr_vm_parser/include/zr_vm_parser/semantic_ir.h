#ifndef ZR_VM_PARSER_SEMANTIC_IR_H
#define ZR_VM_PARSER_SEMANTIC_IR_H

#include "zr_vm_core/array.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/place.h"

typedef TZrUInt32 TZrSemanticInstructionId;
typedef TZrUInt32 TZrLoanId;
typedef TZrUInt32 TZrRegionId;
typedef TZrUInt32 TZrCleanupScopeId;

#define ZR_SEMANTIC_INSTRUCTION_ID_INVALID ((TZrSemanticInstructionId)0U)
#define ZR_SEMANTIC_LOAN_ID_INVALID ((TZrLoanId)0U)
#define ZR_SEMANTIC_LOAN_ID_MULTIPLE ((TZrLoanId)0xffffffffU)
#define ZR_SEMANTIC_REGION_ID_INVALID ((TZrRegionId)0U)
#define ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID ((TZrCleanupScopeId)0U)
#define ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID ((TZrUInt32)0U)
#define ZR_SEMANTIC_BOUNDS_FACT_ID_INVALID ((TZrUInt32)0U)

typedef enum EZrSemanticIrOpcode {
    ZR_SEMANTIC_IR_INVALID = 0,
    ZR_SEMANTIC_IR_CONSTANT,
    ZR_SEMANTIC_IR_CONVERT,
    ZR_SEMANTIC_IR_PLACE_BASE,
    ZR_SEMANTIC_IR_PLACE_PROJECT,
    ZR_SEMANTIC_IR_LOAD,
    ZR_SEMANTIC_IR_STORE,
    ZR_SEMANTIC_IR_INITIALIZE,
    ZR_SEMANTIC_IR_MOVE,
    ZR_SEMANTIC_IR_COPY,
    ZR_SEMANTIC_IR_DROP,
    ZR_SEMANTIC_IR_BORROW_SHARED,
    ZR_SEMANTIC_IR_BORROW_MUT,
    ZR_SEMANTIC_IR_RESERVE_BORROW_MUT,
    ZR_SEMANTIC_IR_REBORROW,
    ZR_SEMANTIC_IR_ACTIVATE_LOAN,
    ZR_SEMANTIC_IR_END_LOAN,
    ZR_SEMANTIC_IR_DEREFERENCE,
    ZR_SEMANTIC_IR_CALL_TYPED,
    ZR_SEMANTIC_IR_CALL_VIRTUAL,
    ZR_SEMANTIC_IR_CALL_DYNAMIC,
    ZR_SEMANTIC_IR_CALL_META,
    ZR_SEMANTIC_IR_BRANCH,
    ZR_SEMANTIC_IR_SWITCH,
    ZR_SEMANTIC_IR_RETURN,
    ZR_SEMANTIC_IR_THROW,
    ZR_SEMANTIC_IR_SCOPE_ENTER,
    ZR_SEMANTIC_IR_SCOPE_EXIT,
    ZR_SEMANTIC_IR_CLEANUP,
    ZR_SEMANTIC_IR_VALUE_CONSTRUCT,
    ZR_SEMANTIC_IR_AGGREGATE_CONSTRUCT,
    ZR_SEMANTIC_IR_FIELD_INITIALIZE,
    ZR_SEMANTIC_IR_UNION_CONSTRUCT,
    ZR_SEMANTIC_IR_GC_NEW,
    ZR_SEMANTIC_IR_OWN_CONSTRUCT,
    ZR_SEMANTIC_IR_PROPERTY_GET,
    ZR_SEMANTIC_IR_PROPERTY_SET,
    ZR_SEMANTIC_IR_PROPERTY_REF_GET,
    ZR_SEMANTIC_IR_DESTRUCTURE_EVALUATE,
    ZR_SEMANTIC_IR_SHAPE_VALIDATE,
    ZR_SEMANTIC_IR_DESTRUCTURE_PROJECT,
    ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_ASSIGN,
    ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_BIND,
    ZR_SEMANTIC_IR_DESTRUCTURE_REST,
    ZR_SEMANTIC_IR_YIELD_VALUE,
    ZR_SEMANTIC_IR_YIELD_SUSPEND,
    ZR_SEMANTIC_IR_YIELD_RESUME,
    ZR_SEMANTIC_IR_ITERATOR_COMPLETE,
    ZR_SEMANTIC_IR_ENUM_MAX
} EZrSemanticIrOpcode;

typedef enum EZrSemanticLoanAccess {
    ZR_SEMANTIC_LOAN_SHARED = 0,
    ZR_SEMANTIC_LOAN_MUTABLE
} EZrSemanticLoanAccess;

typedef enum EZrSemanticLoanPhase {
    ZR_SEMANTIC_LOAN_IMMEDIATE = 0,
    ZR_SEMANTIC_LOAN_TWO_PHASE
} EZrSemanticLoanPhase;

typedef enum EZrSemanticOwnershipOperation {
    ZR_SEMANTIC_OWNERSHIP_NONE = 0,
    ZR_SEMANTIC_OWNERSHIP_UNIQUE = 1,
    ZR_SEMANTIC_OWNERSHIP_SHARE = 2,
    ZR_SEMANTIC_OWNERSHIP_DEGRADE = 3,
    ZR_SEMANTIC_OWNERSHIP_WAKE = 4,
    ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX = 5,
    ZR_SEMANTIC_OWNERSHIP_RETURN_TO_GC = 6,
    ZR_SEMANTIC_OWNERSHIP_ENUM_MAX = 7
} EZrSemanticOwnershipOperation;

typedef enum EZrSemanticEscapeState {
    ZR_SEMANTIC_ESCAPE_LOCAL = 0,
    ZR_SEMANTIC_ESCAPE_FUNCTION,
    ZR_SEMANTIC_ESCAPE_CALLER,
    ZR_SEMANTIC_ESCAPE_HEAP_STATIC,
    ZR_SEMANTIC_ESCAPE_UNKNOWN
} EZrSemanticEscapeState;

typedef enum EZrSemanticEscapeKind {
    ZR_SEMANTIC_ESCAPE_KIND_RETURN = 0,
    ZR_SEMANTIC_ESCAPE_KIND_CLOSURE_CAPTURE,
    ZR_SEMANTIC_ESCAPE_KIND_HEAP_STORE,
    ZR_SEMANTIC_ESCAPE_KIND_SUSPENSION,
    ZR_SEMANTIC_ESCAPE_KIND_NATIVE_CAPTURE
} EZrSemanticEscapeKind;

typedef struct SZrSemanticEscapeFact {
    TZrUInt32 escapeFactId;
    EZrSemanticEscapeKind kind;
    TZrRegionId sourceRegionId;
    TZrPlaceId sourcePlaceId;
    EZrSemanticEscapeState sourceEscapeBound;
    EZrSemanticEscapeState targetEscape;
    SZrFileRange originRange;
    SZrFileRange escapeRange;
} SZrSemanticEscapeFact;

typedef enum EZrSemanticContiguousSourceKind {
    ZR_SEMANTIC_CONTIGUOUS_SOURCE_ARRAY = 0,
    ZR_SEMANTIC_CONTIGUOUS_SOURCE_OWNER,
    ZR_SEMANTIC_CONTIGUOUS_SOURCE_NATIVE_PINNED,
    ZR_SEMANTIC_CONTIGUOUS_SOURCE_VIEW
} EZrSemanticContiguousSourceKind;

typedef struct SZrSemanticContiguousViewFact {
    TZrUInt32 factId;
    TZrPlaceId viewPlaceId;
    TZrValueId viewValueId;
    TZrPlaceId sourcePlaceId;
    TZrValueId startValueId;
    TZrValueId lengthValueId;
    TZrRegionId regionId;
    TZrLoanId sourceLoanId;
    EZrSemanticContiguousSourceKind sourceKind;
    TZrBool isReadOnly;
    TZrBool hasKnownStart;
    TZrBool hasKnownLength;
    TZrInt64 knownStart;
    TZrInt64 knownLength;
    SZrFileRange sourceRange;
} SZrSemanticContiguousViewFact;

typedef enum EZrSemanticBoundsProofKind {
    ZR_SEMANTIC_BOUNDS_PROOF_RUNTIME_CHECK = 0,
    ZR_SEMANTIC_BOUNDS_PROOF_CONSTANT_RANGE
} EZrSemanticBoundsProofKind;

typedef struct SZrSemanticBoundsFact {
    TZrUInt32 factId;
    TZrUInt32 contiguousViewFactId;
    TZrPlaceId viewPlaceId;
    TZrValueId indexValueId;
    TZrValueId lengthValueId;
    EZrSemanticBoundsProofKind proofKind;
    TZrBool hasKnownIndex;
    TZrBool hasKnownLength;
    TZrBool lowerBoundProven;
    TZrBool upperBoundProven;
    TZrBool checkElided;
    TZrInt64 knownIndex;
    TZrInt64 knownLength;
    SZrFileRange sourceRange;
} SZrSemanticBoundsFact;

typedef struct SZrSemanticIrValue {
    TZrValueId id;
    TZrTypeId typeId;
    TZrSemanticInstructionId definitionInstructionId;
    SZrFileRange sourceRange;
} SZrSemanticIrValue;

typedef struct SZrSemanticIrLocal {
    TZrSymbolId symbolId;
    TZrPlaceId placeId;
    TZrTypeId typeId;
    TZrBool isParameter;
} SZrSemanticIrLocal;

typedef struct SZrSemanticIrRegion {
    TZrRegionId id;
    TZrRegionId parentId;
    EZrSemanticEscapeState escapeBound;
    SZrFileRange sourceRange;
} SZrSemanticIrRegion;

typedef struct SZrSemanticIrCleanupScope {
    TZrCleanupScopeId id;
    TZrCleanupScopeId parentId;
    TZrUInt32 firstInstructionIndex;
    TZrUInt32 instructionCount;
    SZrFileRange sourceRange;
} SZrSemanticIrCleanupScope;

typedef struct SZrSemanticIrLoanFact {
    TZrLoanId loanId;
    TZrPlaceId sourcePlaceId;
    EZrSemanticLoanAccess access;
    EZrSemanticLoanPhase phase;
    TZrRegionId regionId;
    SZrFileRange originRange;
    SZrFileRange lastUseRange;
    TZrValueId createdByValueId;
} SZrSemanticIrLoanFact;

typedef struct SZrSemanticIrSourceMapEntry {
    TZrSemanticInstructionId instructionId;
    SZrFileRange sourceRange;
} SZrSemanticIrSourceMapEntry;

typedef struct SZrSemanticIrInstruction {
    TZrSemanticInstructionId id;
    EZrSemanticIrOpcode opcode;
    TZrTypeId typeId;
    TZrPlaceId placeId;
    TZrValueId valueId;
    TZrValueId resultValueId;
    TZrValueId auxiliaryValueId;
    TZrSymbolId symbolId;
    TZrSymbolId accessorSymbolId;
    TZrSymbolId constructorId;
    EZrSemanticOwnershipOperation ownershipOperation;
    TZrUInt32 targetBlockId;
    TZrLoanId loanId;
    TZrRegionId regionId;
    TZrCleanupScopeId cleanupScopeId;
    EZrSemanticEscapeState escape;
    TZrUInt32 operandStart;
    TZrUInt32 operandCount;
    SZrFileRange sourceRange;
} SZrSemanticIrInstruction;

typedef struct SZrSemanticIrInstructionSpec {
    EZrSemanticIrOpcode opcode;
    TZrTypeId typeId;
    TZrPlaceId placeId;
    TZrValueId valueId;
    TZrValueId resultValueId;
    TZrValueId auxiliaryValueId;
    TZrSymbolId symbolId;
    TZrSymbolId accessorSymbolId;
    TZrSymbolId constructorId;
    EZrSemanticOwnershipOperation ownershipOperation;
    TZrUInt32 targetBlockId;
    TZrLoanId loanId;
    TZrRegionId regionId;
    TZrCleanupScopeId cleanupScopeId;
    EZrSemanticEscapeState escape;
    const TZrValueId *operands;
    TZrSize operandCount;
    SZrFileRange sourceRange;
} SZrSemanticIrInstructionSpec;

typedef struct SZrSemanticIrFunction {
    SZrState *state;
    TZrSymbolId symbolId;
    TZrTypeId callableTypeId;
    SZrParserPlaceGraph places;
    SZrParserCfg cfg;
    SZrArray locals; /* SZrSemanticIrLocal */
    SZrArray values; /* SZrSemanticIrValue */
    SZrArray instructions; /* SZrSemanticIrInstruction */
    SZrArray valueOperands; /* TZrValueId */
    SZrArray regions; /* SZrSemanticIrRegion */
    SZrArray cleanupScopes; /* SZrSemanticIrCleanupScope */
    SZrArray sourceMap; /* SZrSemanticIrSourceMapEntry */
    SZrArray loanFacts; /* SZrSemanticIrLoanFact */
    SZrArray escapeFacts; /* SZrSemanticEscapeFact */
    SZrArray contiguousViewFacts; /* SZrSemanticContiguousViewFact */
    SZrArray boundsFacts; /* SZrSemanticBoundsFact */
} SZrSemanticIrFunction;

typedef enum EZrSemanticInitializationState {
    ZR_SEMANTIC_INITIALIZATION_UNINITIALIZED = 0,
    ZR_SEMANTIC_INITIALIZATION_INITIALIZED,
    ZR_SEMANTIC_INITIALIZATION_MAYBE_INITIALIZED
} EZrSemanticInitializationState;

typedef enum EZrSemanticAvailabilityState {
    ZR_SEMANTIC_AVAILABILITY_AVAILABLE = 0,
    ZR_SEMANTIC_AVAILABILITY_MOVED,
    ZR_SEMANTIC_AVAILABILITY_MAYBE_MOVED,
    ZR_SEMANTIC_AVAILABILITY_DROPPED
} EZrSemanticAvailabilityState;

typedef struct SZrSemanticBorrowState {
    SZrArray sharedLoanIds; /* TZrLoanId */
    TZrLoanId mutableLoanId;
} SZrSemanticBorrowState;

typedef struct SZrSemanticPlaceFlowState {
    EZrSemanticInitializationState initialization;
    EZrSemanticAvailabilityState availability;
    SZrSemanticBorrowState borrowing;
    EZrSemanticEscapeState escape;
} SZrSemanticPlaceFlowState;

typedef struct SZrSemanticBlockFlowFacts {
    TZrUInt32 blockId;
    TZrBool isReachable;
    TZrBool hasEntryState;
    SZrArray entryStates; /* SZrSemanticPlaceFlowState */
    SZrArray exitStates; /* SZrSemanticPlaceFlowState */
} SZrSemanticBlockFlowFacts;

typedef enum EZrSemanticFlowDiagnosticKind {
    ZR_SEMANTIC_FLOW_UNINITIALIZED = 0,
    ZR_SEMANTIC_FLOW_MAYBE_UNINITIALIZED,
    ZR_SEMANTIC_FLOW_USE_AFTER_MOVE,
    ZR_SEMANTIC_FLOW_MAYBE_MOVED,
    ZR_SEMANTIC_FLOW_USE_AFTER_DROP,
    ZR_SEMANTIC_FLOW_LOAN_CONFLICT,
    ZR_SEMANTIC_FLOW_ESCAPE_VIOLATION
} EZrSemanticFlowDiagnosticKind;

typedef struct SZrSemanticFlowDiagnostic {
    EZrSemanticFlowDiagnosticKind kind;
    TZrUInt32 blockId;
    TZrSemanticInstructionId instructionId;
    TZrPlaceId placeId;
    TZrLoanId relatedLoanId;
    TZrPlaceId relatedPlaceId;
    EZrParserPlaceOverlap overlap;
    SZrFileRange sourceRange;
    SZrFileRange placeDeclarationRange;
    SZrFileRange loanOriginRange;
    SZrFileRange loanLastUseRange;
    TZrUInt32 escapeFactId;
    EZrSemanticEscapeKind escapeKind;
    EZrSemanticEscapeState sourceEscapeBound;
    EZrSemanticEscapeState targetEscape;
    SZrFileRange escapeOriginRange;
    SZrFileRange escapeTargetRange;
} SZrSemanticFlowDiagnostic;

typedef struct SZrSemanticInstructionLoanLiveness {
    TZrSemanticInstructionId instructionId;
    SZrArray liveInLoanIds; /* TZrLoanId */
    SZrArray liveOutLoanIds; /* TZrLoanId */
    SZrArray activeInLoanIds; /* definitely active TZrLoanId */
    SZrArray activeOutLoanIds; /* definitely active TZrLoanId */
} SZrSemanticInstructionLoanLiveness;

typedef struct SZrSemanticLoanRegionFact {
    TZrLoanId loanId;
    TZrLoanId parentLoanId;
    EZrSemanticLoanPhase phase;
    TZrSemanticInstructionId firstLiveInstructionId;
    TZrSemanticInstructionId activationInstructionId;
    TZrSemanticInstructionId lastUseInstructionId;
    SZrFileRange originRange;
    SZrFileRange lastUseRange;
} SZrSemanticLoanRegionFact;

typedef struct SZrSemanticFlowResult {
    SZrState *state;
    TZrSize placeCount;
    TZrSize loanCount;
    SZrArray blockFacts; /* SZrSemanticBlockFlowFacts */
    SZrArray diagnostics; /* SZrSemanticFlowDiagnostic */
    SZrArray loanLiveness; /* SZrSemanticInstructionLoanLiveness */
    SZrArray loanRegions; /* SZrSemanticLoanRegionFact */
} SZrSemanticFlowResult;

ZR_PARSER_API void ZrParser_SemanticIrFunction_Init(
        SZrState *state,
        SZrSemanticIrFunction *function,
        TZrSymbolId symbolId,
        TZrTypeId callableTypeId);
ZR_PARSER_API void ZrParser_SemanticIrFunction_Free(
        SZrState *state,
        SZrSemanticIrFunction *function);
ZR_PARSER_API TZrPlaceId ZrParser_SemanticIr_AddLocal(
        SZrSemanticIrFunction *function,
        TZrSymbolId symbolId,
        const SZrParserPlaceBase *base,
        TZrTypeId typeId,
        SZrFileRange sourceRange,
        TZrBool isParameter);
ZR_PARSER_API TZrValueId ZrParser_SemanticIr_AddValue(
        SZrSemanticIrFunction *function,
        TZrTypeId typeId,
        SZrFileRange sourceRange);
ZR_PARSER_API TZrRegionId ZrParser_SemanticIr_AddRegion(
        SZrSemanticIrFunction *function,
        TZrRegionId parentId,
        EZrSemanticEscapeState escapeBound,
        SZrFileRange sourceRange);
ZR_PARSER_API TZrUInt32 ZrParser_SemanticIr_AddEscapeFact(
        SZrSemanticIrFunction *function,
        EZrSemanticEscapeKind kind,
        TZrRegionId sourceRegionId,
        TZrPlaceId sourcePlaceId,
        EZrSemanticEscapeState targetEscape,
        SZrFileRange escapeRange);
ZR_PARSER_API const SZrSemanticEscapeFact *ZrParser_SemanticIr_EscapeFactAt(
        const SZrSemanticIrFunction *function,
        TZrSize index);
ZR_PARSER_API TZrUInt32 ZrParser_SemanticIr_AddContiguousViewFact(
        SZrSemanticIrFunction *function,
        const SZrSemanticContiguousViewFact *fact);
ZR_PARSER_API const SZrSemanticContiguousViewFact *
ZrParser_SemanticIr_ContiguousViewFactAt(
        const SZrSemanticIrFunction *function,
        TZrSize index);
ZR_PARSER_API const SZrSemanticContiguousViewFact *
ZrParser_SemanticIr_FindContiguousViewFact(
        const SZrSemanticIrFunction *function,
        TZrPlaceId viewPlaceId);
ZR_PARSER_API TZrUInt32 ZrParser_SemanticIr_AddBoundsFact(
        SZrSemanticIrFunction *function,
        const SZrSemanticBoundsFact *fact);
ZR_PARSER_API const SZrSemanticBoundsFact *ZrParser_SemanticIr_BoundsFactAt(
        const SZrSemanticIrFunction *function,
        TZrSize index);
ZR_PARSER_API TZrCleanupScopeId ZrParser_SemanticIr_AddCleanupScope(
        SZrSemanticIrFunction *function,
        TZrCleanupScopeId parentId,
        SZrFileRange sourceRange);
ZR_PARSER_API TZrLoanId ZrParser_SemanticIr_AddLoan(
        SZrSemanticIrFunction *function,
        TZrPlaceId sourcePlaceId,
        EZrSemanticLoanAccess access,
        TZrRegionId regionId,
        SZrFileRange originRange,
        SZrFileRange lastUseRange,
        TZrValueId createdByValueId);
ZR_PARSER_API TZrLoanId ZrParser_SemanticIr_AddLoanEx(
        SZrSemanticIrFunction *function,
        TZrPlaceId sourcePlaceId,
        EZrSemanticLoanAccess access,
        EZrSemanticLoanPhase phase,
        TZrRegionId regionId,
        SZrFileRange originRange,
        SZrFileRange lastUseRange,
        TZrValueId createdByValueId);
ZR_PARSER_API TZrSemanticInstructionId ZrParser_SemanticIr_Emit(
        SZrSemanticIrFunction *function,
        const SZrSemanticIrInstructionSpec *spec);
ZR_PARSER_API TZrBool ZrParser_SemanticIr_Validate(
        const SZrSemanticIrFunction *function);
ZR_PARSER_API const SZrSemanticIrInstruction *ZrParser_SemanticIr_InstructionAt(
        const SZrSemanticIrFunction *function,
        TZrSize index);
ZR_PARSER_API const SZrSemanticIrValue *ZrParser_SemanticIr_Value(
        const SZrSemanticIrFunction *function,
        TZrValueId valueId);
ZR_PARSER_API const SZrSemanticIrLoanFact *ZrParser_SemanticIr_Loan(
        const SZrSemanticIrFunction *function,
        TZrLoanId loanId);
ZR_PARSER_API TZrBool ZrParser_SemanticIr_BindBlockRange(
        const SZrSemanticIrFunction *function,
        SZrParserCfg *cfg,
        TZrUInt32 blockId,
        TZrUInt32 firstInstructionIndex,
        TZrUInt32 instructionCount,
        EZrParserCfgTerminatorKind terminatorKind);
ZR_PARSER_API const TZrChar *ZrParser_SemanticIr_OpcodeName(
        EZrSemanticIrOpcode opcode);
ZR_PARSER_API TZrBool ZrParser_SemanticIr_FormatGolden(
        const SZrSemanticIrFunction *function,
        TZrChar *buffer,
        TZrSize bufferSize);

ZR_PARSER_API void ZrParser_SemanticFlowResult_Init(
        SZrState *state,
        SZrSemanticFlowResult *result);
ZR_PARSER_API void ZrParser_SemanticFlowResult_Free(
        SZrState *state,
        SZrSemanticFlowResult *result);
ZR_PARSER_API TZrBool ZrParser_SemanticFlow_Analyze(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        const SZrParserCfg *cfg,
        SZrSemanticFlowResult *result);
ZR_PARSER_API const SZrSemanticBlockFlowFacts *ZrParser_SemanticFlow_BlockFacts(
        const SZrSemanticFlowResult *result,
        TZrUInt32 blockId);
ZR_PARSER_API const SZrSemanticPlaceFlowState *ZrParser_SemanticFlow_PlaceState(
        const SZrSemanticBlockFlowFacts *facts,
        TZrPlaceId placeId,
        TZrBool entryState);
ZR_PARSER_API TZrBool ZrParser_SemanticFlow_BuildInitializedPlaceBitmap(
        const SZrSemanticBlockFlowFacts *facts,
        const TZrPlaceId *placeIds,
        TZrSize placeCount,
        TZrUInt64 *words,
        TZrSize wordCount);
ZR_PARSER_API TZrBool ZrParser_SemanticFlow_HasDiagnostic(
        const SZrSemanticFlowResult *result,
        EZrSemanticFlowDiagnosticKind kind,
        TZrPlaceId placeId);
ZR_PARSER_API const SZrSemanticFlowDiagnostic *ZrParser_SemanticFlow_EscapeDiagnostic(
        const SZrSemanticFlowResult *result,
        TZrUInt32 escapeFactId);
ZR_PARSER_API TZrBool ZrParser_SemanticFlow_LoanIsLiveAt(
        const SZrSemanticFlowResult *result,
        TZrSemanticInstructionId instructionId,
        TZrLoanId loanId,
        TZrBool beforeInstruction);
ZR_PARSER_API TZrBool ZrParser_SemanticFlow_LoanIsActiveAt(
        const SZrSemanticFlowResult *result,
        TZrSemanticInstructionId instructionId,
        TZrLoanId loanId,
        TZrBool beforeInstruction);
ZR_PARSER_API const SZrSemanticLoanRegionFact *ZrParser_SemanticFlow_LoanRegion(
        const SZrSemanticFlowResult *result,
        TZrLoanId loanId);

#endif /* ZR_VM_PARSER_SEMANTIC_IR_H */
