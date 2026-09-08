#ifndef ZR_VM_CORE_CALL_BINDING_H
#define ZR_VM_CORE_CALL_BINDING_H

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_aot_abi.h"

struct SZrFunction;

#define ZR_CALL_BINDING_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_CALL_BINDING_SLOT_NONE ((TZrUInt32)0xffffffffu)
#define ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE ((TZrUInt32)64u)
#define ZR_CALL_BINDING_LOCATION_ENCODED_SIZE ((TZrUInt32)16u)
#define ZR_CALL_BINDING_SECTION_MAGIC ((TZrUInt32)0x444e4243u)
#define ZR_CALL_BINDING_SECTION_ROW_SIZE ((TZrUInt32)84u)

typedef enum EZrCallBindingRelocationKind {
    ZR_CALL_BINDING_RELOCATION_NONE = 0,
    ZR_CALL_BINDING_RELOCATION_CONSTANT = 1,
    ZR_CALL_BINDING_RELOCATION_MODULE = 2,
    ZR_CALL_BINDING_RELOCATION_AOT = 3,
    ZR_CALL_BINDING_RELOCATION_VM_MODULE = 4
} EZrCallBindingRelocationKind;

typedef struct SZrCallBindingLocation {
    TZrUInt32 kind;
    TZrUInt32 targetIndex;
    TZrUInt32 ownerDepth;
    TZrUInt32 flags;
} SZrCallBindingLocation;

typedef enum EZrCallBindingKind {
    ZR_CALL_BINDING_NONE = 0,
    ZR_CALL_BINDING_DIRECT = 1,
    ZR_CALL_BINDING_VIRTUAL = 2,
    ZR_CALL_BINDING_INTERFACE = 3,
    ZR_CALL_BINDING_TYPED_FUNCTION = 4
} EZrCallBindingKind;

typedef enum EZrCallBindingOperation {
    ZR_CALL_BINDING_OPERATION_CALL = 0,
    ZR_CALL_BINDING_OPERATION_GET = 1,
    ZR_CALL_BINDING_OPERATION_SET = 2,
    ZR_CALL_BINDING_OPERATION_META = 3
} EZrCallBindingOperation;

/* The disk contract is encoded field by field. Never serialize SZrCallBinding. */
typedef struct SZrCallBindingContract {
    TZrUInt32 bindingKind;
    TZrMetadataToken targetMetadataToken;
    TZrMetadataToken signatureToken;
    TZrMetadataToken ownerTypeToken;
    TZrUInt64 signatureHash;
    TZrUInt64 moduleSignatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 dispatchSlot;
    TZrUInt64 layoutHash;
    TZrUInt32 operation;
    TZrUInt32 reserved0;
    TZrUInt64 reserved1;
} SZrCallBindingContract;

typedef enum EZrCallBindingTargetKind {
    ZR_CALL_BINDING_TARGET_NONE = 0,
    ZR_CALL_BINDING_TARGET_VM = 1,
    ZR_CALL_BINDING_TARGET_NATIVE = 2,
    ZR_CALL_BINDING_TARGET_AOT = 3
} EZrCallBindingTargetKind;

typedef struct SZrCallBindingTarget {
    TZrUInt32 targetKind;
    TZrUInt32 dispatchSlotCount;
    TZrUInt64 ownerLayoutGeneration;
    TZrUInt64 targetGeneration;
    union {
        struct { struct SZrFunction *function; } vm;
        struct { FZrNativeFunction function; } native;
        struct {
            FZrAotEntryThunk thunk;
            const struct SZrAotMethodInfo *methodInfo;
            FZrAotReflectionInvoker invoker;
        } aot;
    };
    /* Closure/callback context is GC traced, never owned or persisted here. */
    struct SZrRawObject *callableObject;
} SZrCallBindingTarget;

typedef struct SZrCallBinding {
    SZrCallBindingContract contract;
    TZrUInt64 generation;
    SZrCallBindingTarget target;
} SZrCallBinding;

typedef struct SZrCallBindingCandidate {
    SZrCallBindingContract contract;
    TZrUInt64 generation;
    SZrCallBindingTarget target;
} SZrCallBindingCandidate;

typedef enum EZrCallBindingStatus {
    ZR_CALL_BINDING_OK = 0,
    ZR_CALL_BINDING_INVALID_ARGUMENT,
    ZR_CALL_BINDING_MISSING_CONTRACT,
    ZR_CALL_BINDING_INVALID_TOKEN,
    ZR_CALL_BINDING_TARGET_NOT_FOUND,
    ZR_CALL_BINDING_AMBIGUOUS_TARGET,
    ZR_CALL_BINDING_SIGNATURE_MISMATCH,
    ZR_CALL_BINDING_MODULE_MISMATCH,
    ZR_CALL_BINDING_LAYOUT_MISMATCH,
    ZR_CALL_BINDING_INVALID_SLOT,
    ZR_CALL_BINDING_STALE_GENERATION,
    ZR_CALL_BINDING_TARGET_KIND_MISMATCH,
    ZR_CALL_BINDING_INVALID_RELOCATION
} EZrCallBindingStatus;

typedef struct SZrCallBindingDiagnostic {
    EZrCallBindingStatus status;
    TZrMetadataToken targetMetadataToken;
    TZrUInt32 instructionIndex;
    TZrUInt32 candidateIndex;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrCallBindingDiagnostic;

ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_CheckContract(
        const SZrCallBindingContract *contract, SZrCallBindingDiagnostic *diagnostic);
ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_CompareContracts(
        const SZrCallBindingContract *expected, const SZrCallBindingContract *actual,
        SZrCallBindingDiagnostic *diagnostic);
ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_Resolve(
        const SZrCallBindingContract *expected, const SZrCallBindingCandidate *candidates,
        TZrUInt32 count, TZrUInt64 generation, SZrCallBinding *binding,
        SZrCallBindingDiagnostic *diagnostic);
ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_Validate(
        SZrCallBinding *binding, TZrUInt64 generation, SZrCallBindingDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_CallBinding_Invalidate(SZrCallBinding *binding);
/* Invalidate resolved targets for a reloaded function graph. The token map is
 * retained and rebuilt by the next link pass. */
ZR_CORE_API TZrBool ZrCore_CallBinding_AdvanceGeneration(struct SZrFunction *function);
typedef TZrBool (*FZrCallBindingFunctionVisitor)(struct SZrFunction *function, void *context);
ZR_CORE_API TZrBool ZrCore_CallBinding_VisitFunctions(
        struct SZrFunction *root, FZrCallBindingFunctionVisitor visitor, void *context);
ZR_CORE_API const char *ZrCore_CallBinding_StatusName(EZrCallBindingStatus status);
ZR_CORE_API TZrBool ZrCore_CallBinding_EncodeContract(
        const SZrCallBindingContract *contract, TZrByte *bytes, TZrSize length);
ZR_CORE_API TZrBool ZrCore_CallBinding_DecodeContract(
        const TZrByte *bytes, TZrSize length, SZrCallBindingContract *contract);
ZR_CORE_API TZrUInt64 ZrCore_CallBinding_FunctionSignatureHash(const struct SZrFunction *function);
ZR_CORE_API TZrBool ZrCore_CallBinding_LinkFunction(
        struct SZrState *state, struct SZrFunction *function, SZrCallBindingDiagnostic *diagnostic);
ZR_CORE_API TZrUInt64 ZrCore_CallBinding_PrototypeLayoutHash(
        const struct SZrFunction *function, TZrUInt32 prototypeIndex);
ZR_CORE_API struct SZrFunction *ZrCore_CallBinding_PrototypeOwner(struct SZrFunction *function);
ZR_CORE_API TZrBool ZrCore_CallBinding_PrepareMember(
        struct SZrState *state, struct SZrFunction *function, TZrUInt32 cacheIndex,
        const SZrTypeValue *receiver, SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_CallBinding_PrepareKnownCall(
        struct SZrState *state, struct SZrFunction *function, TZrUInt32 instructionIndex,
        SZrTypeValue *callable);
ZR_CORE_API TZrBool ZrCore_CallBinding_TryPrepareKnownCall(
        struct SZrState *state, struct SZrFunction *function, TZrUInt32 instructionIndex,
        SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic);

#endif
