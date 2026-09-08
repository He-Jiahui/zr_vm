//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_FUNCTION_H
#define ZR_VM_CORE_FUNCTION_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ffi_contract.h"
struct SZrState;
struct SZrCallInfo;
struct SZrTypeValueOnStack;
struct SZrString;
struct SZrObject;
struct SZrObjectPrototype;
struct SZrClosure;
struct SZrTypeLayoutField;
struct SZrTypeLayoutRegistryView;
struct SZrAotCodeRegistration;

typedef enum EZrFunctionMemberEntryKind {
    ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL = 0,
    ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR = 1
} EZrFunctionMemberEntryKind;

typedef struct SZrFunctionMemberEntry {
    struct SZrString *symbol;
    TZrUInt8 entryKind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    TZrUInt32 prototypeIndex;
    TZrUInt32 descriptorIndex;
} SZrFunctionMemberEntry;

#define ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR ((TZrUInt8)0x01)
#define ZR_FUNCTION_MEMBER_ENTRY_FLAG_INITIALIZATION_WRITE ((TZrUInt8)0x02)
#define ZR_FUNCTION_MEMBER_ENTRY_FLAG_PROPERTY_INITIALIZER ((TZrUInt8)0x04)

struct ZR_STRUCT_ALIGN SZrFunctionStackAnchor {
    TZrMemoryOffset offset;
};

typedef struct SZrFunctionStackAnchor SZrFunctionStackAnchor;

struct ZR_STRUCT_ALIGN SZrFunctionClosureVariable {
    struct SZrString *name;
    TZrBool inStack;
    TZrUInt32 index;
    EZrValueType valueType;
    TZrUInt32 scopeDepth;
    TZrUInt32 escapeFlags;
    /* Compiler-only canonical source identity. The legacy closure artifact row stays unchanged. */
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 declarationStartLine;
    TZrUInt32 declarationStartColumn;
    TZrUInt32 declarationEndLine;
    TZrUInt32 declarationEndColumn;
};

typedef struct SZrFunctionClosureVariable SZrFunctionClosureVariable;

struct ZR_STRUCT_ALIGN SZrFunctionLocalVariable {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrMemoryOffset offsetActivate;
    TZrMemoryOffset offsetDead;
    TZrUInt32 scopeDepth;
    TZrUInt32 escapeFlags;
};

typedef struct SZrFunctionLocalVariable SZrFunctionLocalVariable;

#define ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ((TZrUInt32)0xFFFFFFFFu)

typedef enum EZrFunctionFrameSlotKind {
    ZR_FUNCTION_FRAME_SLOT_KIND_VALUE = 0,
    ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT = 1,
    ZR_FUNCTION_FRAME_SLOT_KIND_UNKNOWN = 2
} EZrFunctionFrameSlotKind;

#define ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS ((TZrUInt16)1u)
#define ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS ((TZrUInt16)2u)
#define ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP ((TZrUInt16)4u)
#define ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT ((TZrUInt16)8u)
#define ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS ((TZrUInt16)16u)
/* Derived only after validated layout finalization; never trust serialized input. */
#define ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE ((TZrUInt16)32u)

typedef struct SZrFunctionFrameIndirectAliasBinding {
    TZrUInt32 ownerStackSlot;
    TZrUInt32 ownerByteOffset;
} SZrFunctionFrameIndirectAliasBinding;

typedef struct SZrFunctionFrameBorrowedAliasBinding {
    struct SZrCallInfo *sourceCallInfo;
    SZrFunctionStackAnchor sourceFrameBase;
    TZrUInt32 sourceStackSlot;
    TZrUInt32 reserved0;
} SZrFunctionFrameBorrowedAliasBinding;

typedef struct SZrFunctionFrameSlotLayout {
    TZrUInt32 stackSlot;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 typeLayoutId;
    TZrUInt8 slotKind;
    TZrUInt8 isParameter;
    TZrUInt16 reserved0; /* EZrFunctionFrameSlotFlags */
} SZrFunctionFrameSlotLayout;

typedef struct SZrFunctionFrameFieldLayout {
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 typeLayoutId;
    EZrValueType valueType;
    TZrUInt8 isPrimitivePod;
    TZrUInt8 isValueSlot;
    TZrUInt16 reserved0;
} SZrFunctionFrameFieldLayout;

#define ZR_FUNCTION_FRAME_FIELD_FLAG_IMMUTABLE ((TZrUInt16)0x0001)

struct ZR_STRUCT_ALIGN SZrFunctionExecutionLocationInfo {
    TZrMemoryOffset currentInstructionOffset;
    TZrUInt32 lineInSource;
    TZrUInt32 columnInSourceStart;
    TZrUInt32 lineInSourceEnd;
    TZrUInt32 columnInSourceEnd;
};

typedef struct SZrFunctionExecutionLocationInfo SZrFunctionExecutionLocationInfo;

typedef enum EZrModuleExportKind {
    ZR_MODULE_EXPORT_KIND_VALUE = 0,
    ZR_MODULE_EXPORT_KIND_FUNCTION = 1,
    ZR_MODULE_EXPORT_KIND_TYPE = 2
} EZrModuleExportKind;

typedef enum EZrModuleExportReadiness {
    ZR_MODULE_EXPORT_READY_DECLARATION = 0,
    ZR_MODULE_EXPORT_READY_ENTRY = 1
} EZrModuleExportReadiness;

typedef enum EZrModuleEntryEffectKind {
    ZR_MODULE_ENTRY_EFFECT_IMPORT_REF = 0,
    ZR_MODULE_ENTRY_EFFECT_IMPORT_READ = 1,
    ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL = 2,
    ZR_MODULE_ENTRY_EFFECT_LOCAL_CALL = 3,
    ZR_MODULE_ENTRY_EFFECT_LOCAL_ENTRY_BINDING_READ = 4,
    ZR_MODULE_ENTRY_EFFECT_DYNAMIC_UNKNOWN = 5
} EZrModuleEntryEffectKind;

#define ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ((TZrUInt32)0xFFFFFFFFu)

typedef struct SZrFunctionExportedVariable {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt8 accessModifier;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
    TZrUInt32 callableChildIndex;
} SZrFunctionExportedVariable;

struct ZR_STRUCT_ALIGN SZrFunctionCatchClauseInfo {
    struct SZrString *typeName;
    TZrMemoryOffset targetInstructionOffset;
};

typedef struct SZrFunctionCatchClauseInfo SZrFunctionCatchClauseInfo;

struct ZR_STRUCT_ALIGN SZrFunctionExceptionHandlerInfo {
    TZrMemoryOffset protectedStartInstructionOffset;
    TZrMemoryOffset finallyTargetInstructionOffset;
    TZrMemoryOffset afterFinallyInstructionOffset;
    TZrUInt32 catchClauseStartIndex;
    TZrUInt32 catchClauseCount;
    TZrBool hasFinally;
};

typedef struct SZrFunctionExceptionHandlerInfo SZrFunctionExceptionHandlerInfo;

typedef enum EZrFunctionTypedSymbolKind {
    ZR_FUNCTION_TYPED_SYMBOL_VARIABLE = 1,
    ZR_FUNCTION_TYPED_SYMBOL_FUNCTION = 2
} EZrFunctionTypedSymbolKind;

typedef struct SZrFunctionTypedTypeRef {
    EZrValueType baseType;
    TZrBool isNullable;
    TZrUInt32 ownershipQualifier;
    TZrBool isArray;
    struct SZrString *typeName;
    EZrValueType elementBaseType;
    struct SZrString *elementTypeName;
    EZrStaticCType staticCType;
    TZrUInt32 staticCTypeId;
} SZrFunctionTypedTypeRef;

/*
 * Open callable generic contract carried by typed exports. The rows describe
 * the declaration; individual call facts still carry their own closed TypeId.
 */
typedef struct SZrFunctionTypedGenericParameter {
    struct SZrString *name;
    TZrUInt8 genericKind;
    TZrUInt8 variance;
    TZrUInt8 requiresClass;
    TZrUInt8 requiresStruct;
    TZrUInt8 requiresNew;
    TZrUInt8 requiresOwner;
    TZrUInt16 reserved0;
    TZrUInt32 requiredOwnershipQualifier;
    TZrUInt32 constraintTypeCount;
    struct SZrString **constraintTypeNames;
} SZrFunctionTypedGenericParameter;

enum {
    ZR_FUNCTION_TYPED_LOCAL_ROLE_NONE = 0u,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER = 1u << 0,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE = 1u << 1,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN = 1u << 2,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF = 1u << 3,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF_READONLY = 1u << 4,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF = 1u << 5,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF_READONLY = 1u << 6,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_OUT = 1u << 7,
    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF_READONLY |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF_READONLY |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_OUT
};

typedef struct SZrFunctionTypedLocalBinding {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    SZrFunctionTypedTypeRef type;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 placeId;
    TZrUInt32 declarationStartLine;
    TZrUInt32 declarationStartColumn;
    TZrUInt32 declarationEndLine;
    TZrUInt32 declarationEndColumn;
    TZrUInt32 roleFlags;
} SZrFunctionTypedLocalBinding;

typedef struct SZrFunctionTypedClosureBinding {
    TZrUInt32 captureIndex;
    SZrFunctionTypedTypeRef type;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 declarationStartLine;
    TZrUInt32 declarationStartColumn;
    TZrUInt32 declarationEndLine;
    TZrUInt32 declarationEndColumn;
} SZrFunctionTypedClosureBinding;

typedef struct SZrFunctionSourceRange {
    TZrUInt32 startLine;
    TZrUInt32 startColumn;
    TZrUInt32 endLine;
    TZrUInt32 endColumn;
} SZrFunctionSourceRange;

/*
 * A compiler-owned scheduler call fact. This is intentionally not an artifact
 * row: the artifact writer must still join its canonical TypeId with an exact
 * TypeDef or TypeRef before serializing anything.
 */
typedef struct SZrFunctionArtifactSourceTypeIdentity {
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 signatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 reserved0;
    TZrUInt64 layoutHash;
    TZrUInt64 moduleSignatureHash;
} SZrFunctionArtifactSourceTypeIdentity;

typedef struct SZrFunctionSchedulerSourceFact {
    TZrUInt32 schedulerTypeId;
    TZrUInt32 taskTypeId;
    TZrUInt32 jobTypeId;
    TZrUInt32 schedulerAbiVersion;
    TZrMetadataToken scheduleMemberToken;
    TZrMetadataToken scheduleSignatureToken;
    TZrUInt64 scheduleSignatureHash;
    TZrUInt64 schedulerProtocolMask;
    TZrUInt32 contractRole;
    TZrUInt32 schedulerPolicyMask;
    TZrUInt32 attachedRequirementFlags;
    TZrUInt32 isolatedRequirementFlags;
    TZrUInt32 reserved0;
    TZrUInt64 transportContractHash;
    TZrUInt64 schedulerContractHash;
    SZrFunctionArtifactSourceTypeIdentity schedulerProvider;
    SZrFunctionArtifactSourceTypeIdentity taskProvider;
    SZrFunctionArtifactSourceTypeIdentity jobProvider;
} SZrFunctionSchedulerSourceFact;

typedef struct SZrFunctionTypedExportSymbol {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt8 accessModifier;
    TZrUInt8 symbolKind;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt16 reserved0;
    TZrUInt32 callableChildIndex;
    SZrFunctionTypedTypeRef valueType;
    TZrUInt32 parameterCount;
    SZrFunctionTypedTypeRef *parameterTypes;
    TZrUInt32 genericParameterCount;
    SZrFunctionTypedGenericParameter *genericParameters;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 columnInSourceStart;
    TZrUInt32 lineInSourceEnd;
    TZrUInt32 columnInSourceEnd;
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt64 signatureHash;
} SZrFunctionTypedExportSymbol;

typedef struct SZrFunctionModuleEffect {
    TZrUInt8 kind;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
    struct SZrString *moduleName;
    struct SZrString *assemblyName;
    struct SZrString *symbolName;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 columnInSourceStart;
    TZrUInt32 lineInSourceEnd;
    TZrUInt32 columnInSourceEnd;
    TZrMetadataToken targetMetadataToken;
    TZrMetadataToken targetSignatureToken;
    TZrUInt64 targetSignatureHash;
    TZrUInt64 targetModuleSignatureHash;
    struct SZrString *requestedModuleVersion;
    struct SZrString *minModuleVersionInclusive;
    struct SZrString *maxModuleVersionExclusive;
} SZrFunctionModuleEffect;

typedef struct SZrFunctionCallableSummary {
    struct SZrString *name;
    TZrUInt32 callableChildIndex;
    TZrUInt32 effectCount;
    SZrFunctionModuleEffect *effects;
} SZrFunctionCallableSummary;

typedef struct SZrFunctionTopLevelCallableBinding {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt32 callableChildIndex;
    TZrUInt8 accessModifier;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
} SZrFunctionTopLevelCallableBinding;

typedef struct SZrFunctionMetadataParameter {
    struct SZrString *name;
    SZrFunctionTypedTypeRef type;
    TZrBool hasDefaultValue;
    struct SZrTypeValue defaultValue;
    TZrBool hasDecoratorMetadata;
    struct SZrTypeValue decoratorMetadataValue;
    struct SZrString **decoratorNames;
    TZrUInt32 decoratorCount;
} SZrFunctionMetadataParameter;

typedef enum EZrCompileTimeBindingTargetKind {
    ZR_COMPILE_TIME_BINDING_TARGET_NONE = 0,
    ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION = 1
} EZrCompileTimeBindingTargetKind;

typedef struct SZrFunctionCompileTimePathBinding {
    struct SZrString *path;
    TZrUInt8 targetKind;
    struct SZrString *targetName;
} SZrFunctionCompileTimePathBinding;

typedef struct SZrFunctionCompileTimeVariableInfo {
    struct SZrString *name;
    SZrFunctionTypedTypeRef type;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
    SZrFunctionCompileTimePathBinding *pathBindings;
    TZrUInt32 pathBindingCount;
} SZrFunctionCompileTimeVariableInfo;

typedef struct SZrFunctionCompileTimeFunctionInfo {
    struct SZrString *name;
    SZrFunctionTypedTypeRef returnType;
    TZrUInt32 parameterCount;
    SZrFunctionMetadataParameter *parameters;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
} SZrFunctionCompileTimeFunctionInfo;

typedef enum EZrFunctionEscapeBindingKind {
    ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL = 0,
    ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE = 1,
    ZR_FUNCTION_ESCAPE_BINDING_KIND_RETURN_SLOT = 2,
    ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT = 3,
    ZR_FUNCTION_ESCAPE_BINDING_KIND_GLOBAL_BINDING = 4,
    ZR_FUNCTION_ESCAPE_BINDING_KIND_NATIVE_BINDING = 5
} EZrFunctionEscapeBindingKind;

typedef struct SZrFunctionEscapeBinding {
    struct SZrString *name;
    TZrUInt32 slotOrIndex;
    TZrUInt32 scopeDepth;
    TZrUInt32 escapeFlags;
    TZrUInt8 bindingKind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrFunctionEscapeBinding;

typedef enum EZrSemIrOpcode {
    ZR_SEMIR_OPCODE_NOP = 0,
    ZR_SEMIR_OPCODE_OWN_UNIQUE = 1,
    ZR_SEMIR_OPCODE_OWN_BORROW = 2,
    ZR_SEMIR_OPCODE_OWN_LOAN = 3,
    ZR_SEMIR_OPCODE_OWN_SHARE = 4,
    ZR_SEMIR_OPCODE_OWN_DEGRADE = 5,
    ZR_SEMIR_OPCODE_TYPEOF = 6,
    ZR_SEMIR_OPCODE_DYN_CALL = 7,
    ZR_SEMIR_OPCODE_DYN_TAIL_CALL = 8,
    ZR_SEMIR_OPCODE_META_CALL = 9,
    ZR_SEMIR_OPCODE_META_TAIL_CALL = 10,
    ZR_SEMIR_OPCODE_META_GET = 11,
    ZR_SEMIR_OPCODE_META_SET = 12,
    ZR_SEMIR_OPCODE_DYN_ITER_INIT = 13,
    ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT = 14,
    ZR_SEMIR_OPCODE_OWN_DETACH = 15,
    ZR_SEMIR_OPCODE_OWN_WAKE = 16,
    ZR_SEMIR_OPCODE_OWN_DROP = 17,
    ZR_SEMIR_OPCODE_VALUE_ADDR = 18,
    ZR_SEMIR_OPCODE_FIELD_ADDR = 19,
    ZR_SEMIR_OPCODE_LOAD_VALUE = 20,
    ZR_SEMIR_OPCODE_STORE_VALUE = 21,
    ZR_SEMIR_OPCODE_INIT_VALUE = 22,
    ZR_SEMIR_OPCODE_COPY_VALUE = 23,
    ZR_SEMIR_OPCODE_CALL_TYPED = 24,
    ZR_SEMIR_OPCODE_RETURN_TYPED = 25,
    ZR_SEMIR_OPCODE_OWN_RETURN_LOAN = 26,
    ZR_SEMIR_OPCODE_ADD = 27,
    ZR_SEMIR_OPCODE_SUB = 28,
    ZR_SEMIR_OPCODE_MUL = 29,
    ZR_SEMIR_OPCODE_DIV = 30,
    ZR_SEMIR_OPCODE_MOD = 31,
    ZR_SEMIR_OPCODE_EQ = 32,
    ZR_SEMIR_OPCODE_NE = 33,
    ZR_SEMIR_OPCODE_LT = 34,
    ZR_SEMIR_OPCODE_LE = 35,
    ZR_SEMIR_OPCODE_GT = 36,
    ZR_SEMIR_OPCODE_GE = 37,
    ZR_SEMIR_OPCODE_DYN_ARITHMETIC = 38,
    ZR_SEMIR_OPCODE_DYN_INDEX_GET = 39,
    ZR_SEMIR_OPCODE_DYN_INDEX_SET = 40,
    ZR_SEMIR_OPCODE_BIT_NOT = 41,
    ZR_SEMIR_OPCODE_BIT_AND = 42,
    ZR_SEMIR_OPCODE_BIT_OR = 43,
    ZR_SEMIR_OPCODE_BIT_XOR = 44,
    ZR_SEMIR_OPCODE_SHL = 45,
    ZR_SEMIR_OPCODE_SHR = 46,
    ZR_SEMIR_OPCODE_OWN_VIEW_SHARED = 47,
    ZR_SEMIR_OPCODE_OWN_VIEW_MUT = 48,
    ZR_SEMIR_OPCODE_OWN_INTO_GC_BOX = 49,
    ZR_SEMIR_OPCODE_OWN_RETURN_TO_GC = 50,
    ZR_SEMIR_OPCODE_PROPERTY_REF_GET = 51,
    ZR_SEMIR_OPCODE_DEREFERENCE = 52,
    ZR_SEMIR_OPCODE_PROPERTY_REF_STORE = 53,
    ZR_SEMIR_OPCODE_DYN_CALL_SPREAD = 54,
    ZR_SEMIR_OPCODE_REQUIRE_NON_NULL = 55
} EZrSemIrOpcode;

typedef enum EZrSemIrEffectKind {
    ZR_SEMIR_EFFECT_KIND_NONE = 0,
    ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION = 1,
    ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME = 2
} EZrSemIrEffectKind;

typedef enum EZrSemIrOwnershipState {
    ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC = 0,
    ZR_SEMIR_OWNERSHIP_STATE_UNIQUE = 1,
    ZR_SEMIR_OWNERSHIP_STATE_SHARED = 2,
    ZR_SEMIR_OWNERSHIP_STATE_WEAK = 3,
    ZR_SEMIR_OWNERSHIP_STATE_BORROW_SHARED = 4,
    ZR_SEMIR_OWNERSHIP_STATE_BORROW_MUT = 5
} EZrSemIrOwnershipState;

typedef struct SZrSemIrOwnershipEntry {
    TZrUInt32 state;
} SZrSemIrOwnershipEntry;

typedef struct SZrSemIrEffectEntry {
    TZrUInt32 kind;
    TZrUInt32 instructionIndex;
    TZrUInt32 ownershipInputIndex;
    TZrUInt32 ownershipOutputIndex;
} SZrSemIrEffectEntry;

typedef struct SZrSemIrBlockEntry {
    TZrUInt32 blockId;
    TZrUInt32 firstInstructionIndex;
    TZrUInt32 instructionCount;
} SZrSemIrBlockEntry;

typedef struct SZrSemIrInstruction {
    TZrUInt32 opcode;
    TZrUInt32 execInstructionIndex;
    TZrUInt32 typeTableIndex;
    TZrUInt32 effectTableIndex;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
    TZrUInt32 deoptId;
} SZrSemIrInstruction;

typedef struct SZrSemIrDeoptEntry {
    TZrUInt32 deoptId;
    TZrUInt32 execInstructionIndex;
} SZrSemIrDeoptEntry;

#define ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY ((TZrUInt32)4)

typedef enum EZrFunctionCallSiteCacheKind {
    ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE = 0,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET = 1,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET = 2,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC = 3,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC = 4,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL = 5,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL = 6,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL = 7,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL = 8,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET = 9,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET = 10
} EZrFunctionCallSiteCacheKind;

typedef enum EZrFunctionCallSitePicAccessKind {
    ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_NONE = 0,
    ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD = 1
} EZrFunctionCallSitePicAccessKind;

#define ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET ((TZrUInt16)0x0080u)
typedef enum EZrFunctionCallSitePicHotFieldKind {
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_NONE = 0,
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_ITEMS = 1,
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_LENGTH = 2,
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_CAPACITY = 3,
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_ITERATOR_SOURCE = 4,
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_ITERATOR_CURRENT = 5,
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_ITERATOR_INDEX = 6,
    ZR_FUNCTION_CALLSITE_PIC_HOT_FIELD_ITERATOR_NEXT_NODE = 7
} EZrFunctionCallSitePicHotFieldKind;

typedef struct SZrFunctionCallSitePicSlot {
    struct SZrObjectPrototype *cachedReceiverPrototype;
    struct SZrObjectPrototype *cachedOwnerPrototype;
    struct SZrObject *cachedReceiverObject;
    struct SZrHashKeyValuePair *cachedReceiverPair;
    struct SZrFunction *cachedFunction;
    struct SZrString *cachedMemberName;
    TZrUInt32 cachedReceiverVersion;
    TZrUInt32 cachedOwnerVersion;
    TZrUInt64 cachedReceiverShapeId;
    TZrUInt64 cachedOwnerShapeId;
    TZrUInt64 cachedReceiverShapeGeneration;
    TZrUInt64 cachedOwnerShapeGeneration;
    TZrUInt32 cachedDescriptorIndex;
    TZrUInt8 cachedIsStatic;
    TZrUInt8 cachedAccessKind;
    TZrUInt16 cachedHotFieldKind;
} SZrFunctionCallSitePicSlot;

typedef struct SZrFunctionCallSiteCacheEntry {
    TZrUInt32 kind;
    TZrUInt32 instructionIndex;
    TZrUInt32 memberEntryIndex;
    TZrUInt32 deoptId;
    TZrUInt32 argumentCount;
    TZrUInt32 picSlotCount;
    TZrUInt32 picNextInsertIndex;
    TZrUInt32 reserved2;
    SZrFunctionCallSitePicSlot picSlots[ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY];
    TZrUInt32 runtimeHitCount;
    TZrUInt32 runtimeMissCount;
    SZrCallBinding binding;
    SZrCallBindingLocation bindingLocation;
} SZrFunctionCallSiteCacheEntry;

struct ZR_STRUCT_ALIGN SZrFunction {
    SZrRawObject super;
    TZrUInt16 parameterCount;
    TZrBool hasVariableArguments;
    TZrUInt32 stackSize;
    TZrUInt32 vmEntryClearStackSizePlusOne;
    // function name (函数名由函数自身持有，匿名函数为 ZR_NULL)
    struct SZrString *functionName;
    // length
    TZrUInt32 instructionsLength;
    TZrUInt32 closureValueLength;
    TZrUInt32 constantValueLength;
    TZrUInt32 localVariableLength;
    TZrUInt32 childFunctionLength;
    TZrBool childFunctionGraphIsBorrowed;
    TZrUInt32 executionLocationInfoLength;
    TZrUInt32 catchClauseCount;
    TZrUInt32 exceptionHandlerCount;
    // debug
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
    // instructions
    TZrInstruction *instructionsList;
    // variables
    SZrFunctionClosureVariable *closureValueList;
    SZrTypeValue *constantValueList;
    SZrFunctionLocalVariable *localVariableList;
    struct SZrFunction *childFunctionList;
    struct SZrFunction *ownerFunction;
    // function debug info
    SZrFunctionExecutionLocationInfo *executionLocationInfoList;
    SZrFunctionCatchClauseInfo *catchClauseList;
    SZrFunctionExceptionHandlerInfo *exceptionHandlerList;
    TZrUInt32 *lineInSourceList;
    struct SZrString *sourceCodeList;
    struct SZrString *sourceHash;
    SZrRawObject *gcList;

    // module export info (for script-level functions only)
    // 导出变量信息（用于模块导出）
    SZrFunctionExportedVariable *exportedVariables;   // 导出变量数组
    TZrUInt32 exportedVariableLength;                 // 导出变量数量

    SZrFunctionTypedLocalBinding *typedLocalBindings;
    TZrUInt32 typedLocalBindingLength;
    SZrFunctionTypedClosureBinding *typedClosureBindings;
    TZrUInt32 typedClosureBindingLength;
    SZrFunctionTypedExportSymbol *typedExportedSymbols;
    TZrUInt32 typedExportedSymbolLength;
    SZrFunctionSchedulerSourceFact *schedulerSourceFacts;
    TZrUInt32 schedulerSourceFactLength;
    SZrMetadataTokenRecord *metadataTokenRecords;
    TZrUInt32 metadataTokenRecordLength;
    SZrMetadataTokenRecord *moduleMetadataTokenRecords;
    TZrUInt32 moduleMetadataTokenRecordLength;
    TZrByte *signatureBlobHeap;
    TZrUInt32 signatureBlobHeapLength;
    SZrMetadataStringHeapEntry *metadataStringHeap;
    TZrUInt32 metadataStringHeapLength;
    TZrUInt64 moduleSignatureHash;
    struct SZrString *moduleVersion;
    struct SZrString **staticImports;
    TZrUInt32 staticImportLength;
    SZrFunctionModuleEffect *moduleEntryEffects;
    TZrUInt32 moduleEntryEffectLength;
    SZrFunctionCallableSummary *exportedCallableSummaries;
    TZrUInt32 exportedCallableSummaryLength;
    SZrFunctionTopLevelCallableBinding *topLevelCallableBindings;
    TZrUInt32 topLevelCallableBindingLength;
    SZrFunctionMetadataParameter *parameterMetadata;
    TZrUInt32 parameterMetadataCount;
    TZrBool hasCallableReturnType;
    SZrFunctionTypedTypeRef callableReturnType;
    SZrFunctionCompileTimeVariableInfo *compileTimeVariableInfos;
    TZrUInt32 compileTimeVariableInfoLength;
    SZrFunctionCompileTimeFunctionInfo *compileTimeFunctionInfos;
    TZrUInt32 compileTimeFunctionInfoLength;
    SZrFunctionEscapeBinding *escapeBindings;
    TZrUInt32 escapeBindingLength;
    TZrUInt32 *returnEscapeSlots;
    TZrUInt32 returnEscapeSlotCount;
    TZrBool hasDecoratorMetadata;
    SZrTypeValue decoratorMetadataValue;
    struct SZrString **decoratorNames;
    TZrUInt32 decoratorCount;
    SZrFunctionMemberEntry *memberEntries;
    TZrUInt32 memberEntryLength;

    // prototype数据存储（从常量池迁移）
    TZrByte *prototypeData;                           // prototype 二进制数据（序列化后的 SZrCompiledPrototypeInfo 数组）
    TZrUInt32 prototypeDataLength;                    // prototype 数据长度（字节数）
    TZrUInt32 prototypeCount;                         // prototype 数量
    struct SZrObjectPrototype **prototypeInstances; // 运行时实例化的prototype对象指针数组
    TZrUInt32 prototypeInstancesLength;               // prototype实例数组长度

    SZrFunctionTypedTypeRef *semIrTypeTable;
    TZrUInt32 semIrTypeTableLength;
    SZrSemIrOwnershipEntry *semIrOwnershipTable;
    TZrUInt32 semIrOwnershipTableLength;
    SZrSemIrEffectEntry *semIrEffectTable;
    TZrUInt32 semIrEffectTableLength;
    SZrSemIrBlockEntry *semIrBlockTable;
    TZrUInt32 semIrBlockTableLength;
    SZrSemIrInstruction *semIrInstructions;
    TZrUInt32 semIrInstructionLength;
    SZrSemIrDeoptEntry *semIrDeoptTable;
    TZrUInt32 semIrDeoptTableLength;
    SZrFunctionCallSiteCacheEntry *callSiteCaches;
    TZrUInt32 callSiteCacheLength;
    TZrByte *testManifestData;
    TZrSize testManifestDataLength;
    struct SZrClosure *cachedStatelessClosure;
    // Append-only sidecar: keep existing SZrFunction field offsets stable for
    // native fixtures and copied function graphs that may observe the public ABI.
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    TZrUInt32 frameSlotLayoutLength;
    SZrFunctionFrameSlotLayout *frameSlotLayouts;
    SZrTypeLayout *prototypeFrameTypeLayouts;
    struct SZrTypeLayoutField *prototypeFrameTypeLayoutFields;
    TZrUInt8 *prototypeFrameTypeLayoutStates;
    const SZrTypeLayout **prototypeFrameTypeLayoutRegistry;
    TZrUInt32 prototypeFrameTypeLayoutLength;
    TZrUInt32 prototypeFrameTypeLayoutFieldCount;
    TZrUInt32 prototypeFrameTypeLayoutFieldCapacity;
    struct SZrFunction *prototypeContextFunction;
    const struct SZrAotCodeRegistration *metadataCodeRegistration;
    TZrUInt32 metadataTypeLayoutCount;
    TZrUInt32 metadataGcDescriptorCount;
    SZrMetadataTokenBinding *moduleMetadataBindings;
    TZrUInt32 moduleMetadataBindingLength;
    TZrUInt32 moduleMetadataBindingCapacity;
    SZrNativeImportContract *nativeImportContracts;
    TZrUInt32 nativeImportContractLength;
    /* Immutable derived summary; zero means checked parameter-layout fallback. */
    TZrUInt32 directValueParameterCountPlusOne;
    TZrUInt32 directValueParameterScanLength;
    /* Immutable derived summary; zero means checked frame-drop fallback. */
    TZrUInt32 directValueFrameSlotCountPlusOne;
    /* Immutable derived summary; zero means scan the current instruction stream. */
    TZrUInt32 generatedFrameSlotCountPlusOne;
    TZrUInt64 callBindingGeneration;
    TZrUInt32 *callBindingInstructionMap;
    TZrUInt32 callBindingInstructionMapLength;
};

typedef struct SZrFunction SZrFunction;

typedef const SZrTypeLayout *(*FZrFunctionFrameTypeLayoutResolver)(const SZrFunction *function,
                                                                   TZrUInt32 typeLayoutId,
                                                                   TZrPtr userData);
typedef TZrBool (*FZrFunctionPrototypeFrameFieldVisitor)(struct SZrState *state,
                                                         const SZrFunction *function,
                                                         TZrUInt32 typeLayoutId,
                                                         struct SZrString *fieldName,
                                                         const SZrFunctionFrameFieldLayout *fieldLayout,
                                                         TZrPtr userData);
typedef void (*FZrFunctionFrameGcValueVisitor)(struct SZrState *state,
                                               SZrTypeValue *value,
                                               TZrPtr userData);

// struct ZR_STRUCT_ALIGN SZrFunctionOverload {
//     TZrSize functionOverloadsLength;
//     SZrFunction **functionOverloads;
// };
//
// typedef struct SZrFunctionOverload SZrFunctionOverload;

ZR_CORE_API SZrFunction *ZrCore_Function_New(struct SZrState *state);

ZR_CORE_API void ZrCore_Function_Free(struct SZrState *state, SZrFunction *function);

ZR_CORE_API struct SZrString *ZrCore_Function_GetLocalVariableName(SZrFunction *function, TZrUInt32 index,
                                                             TZrUInt32 programCounter);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CheckStack(struct SZrState *state, TZrSize size,
                                                      TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrCore_Function_CheckNativeStack(struct SZrState *state);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CheckStackAndGc(struct SZrState *state, TZrSize size,
                                                           TZrStackValuePointer stackPointer);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_ReserveScratchSlots(struct SZrState *state,
                                                               TZrSize size,
                                                               TZrStackValuePointer scratchBase);

ZR_CORE_API void ZrCore_Function_Call(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount);

ZR_CORE_API void ZrCore_Function_CallWithoutYield(struct SZrState *state, TZrStackValuePointer stackPointer,
                                            TZrSize resultCount);
ZR_CORE_API void ZrCore_Function_CallWithoutYieldToDestination(struct SZrState *state,
                                                               TZrStackValuePointer stackPointer,
                                                               TZrSize resultCount,
                                                               TZrStackValuePointer returnDestination);

ZR_CORE_API void ZrCore_Function_StackAnchorInit(struct SZrState *state,
                                           TZrStackValuePointer stackPointer,
                                           SZrFunctionStackAnchor *anchor);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_StackAnchorRestore(struct SZrState *state,
                                                                const SZrFunctionStackAnchor *anchor);

ZR_CORE_API void ZrCore_Function_RebindConstantFunctionValuesToChildren(SZrFunction *function);
ZR_CORE_API SZrFunction *ZrCore_Function_ResolveGraphFunctionByFlatIndex(
        struct SZrState *state,
        SZrFunction *rootFunction,
        TZrUInt32 flatIndex);
ZR_CORE_API void ZrCore_Function_ClearChildOwnerLinks(SZrFunction *function);
ZR_CORE_API void ZrCore_Function_DetachOwnedBuffers(SZrFunction *function);

/* Returns canonical capture identity only when the typed sidecar matches a legacy capture slot. */
ZR_CORE_API TZrBool ZrCore_Function_GetClosureCaptureIdentity(
        const SZrFunction *function,
        TZrUInt32 captureIndex,
        const SZrFunctionTypedTypeRef **outType,
        TZrUInt32 *outSymbolId,
        TZrUInt32 *outTypeId,
        SZrFunctionSourceRange *outDeclarationRange);

ZR_CORE_API TZrBool ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(const SZrFunction *function);

ZR_CORE_API TZrUInt32 ZrCore_Function_GetGeneratedFrameSlotCount(const SZrFunction *function);
/*
 * frameSlotLayouts must contain at most one entry for each stackSlot. This
 * metadata contract also applies to hand-built SZrFunction values; compiler
 * output stores the unique canonical entry at frameSlotLayouts[stackSlot].
 */
ZR_CORE_API const SZrFunctionFrameSlotLayout *ZrCore_Function_FindFrameSlotLayout(const SZrFunction *function,
          TZrUInt32 stackSlot);
ZR_CORE_API void ZrCore_Function_FinalizeDirectFrameValueSlots(
        SZrFunction *function);
static ZR_FORCE_INLINE TZrBool ZrCore_Function_IsDirectFrameValueSlotLayout(
        const SZrFunction *function,
        const SZrFunctionFrameSlotLayout *slotLayout) {
    return (TZrBool)(function != ZR_NULL &&
                     slotLayout != ZR_NULL &&
                     function->frameSlotLayouts != ZR_NULL &&
                     slotLayout->stackSlot < function->frameSlotLayoutLength &&
                     &function->frameSlotLayouts[slotLayout->stackSlot] == slotLayout &&
                     (slotLayout->reserved0 &
                      ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE) != 0u);
}
static ZR_FORCE_INLINE TZrBool ZrCore_Function_IsDirectFrameValueSlot(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    if (function == ZR_NULL ||
        function->frameSlotLayouts == ZR_NULL ||
        stackSlot >= function->frameSlotLayoutLength) {
        return ZR_FALSE;
    }
    return ZrCore_Function_IsDirectFrameValueSlotLayout(
            function, &function->frameSlotLayouts[stackSlot]);
}
static ZR_FORCE_INLINE TZrBool ZrCore_Function_HasDirectValueFrameSlotSummary(
        const SZrFunction *function) {
    return (TZrBool)(function != ZR_NULL &&
                     function->frameSlotLayouts != ZR_NULL &&
                     function->frameSlotLayoutLength < UINT32_MAX &&
                     function->directValueFrameSlotCountPlusOne ==
                             function->frameSlotLayoutLength + 1u);
}
static ZR_FORCE_INLINE SZrTypeValue *ZrCore_Function_TryGetDirectFrameValueSlotLayout(
        const SZrFunction *function,
        const SZrFunctionFrameSlotLayout *slotLayout,
        TZrStackValuePointer frameBase) {
    if (frameBase == ZR_NULL ||
        !ZrCore_Function_IsDirectFrameValueSlotLayout(function, slotLayout)) {
        return ZR_NULL;
    }

    return (SZrTypeValue *)((TZrByte *)frameBase + slotLayout->byteOffset);
}
ZR_CORE_API SZrTypeValue *ZrCore_Function_TryGetDirectFrameValueSlot(
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 stackSlot);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_Function_FindMetadataTokenRecord(const SZrFunction *function,
                                                                                  TZrMetadataToken token);
/* schedulerTypeId == 0 selects the first recorded scheduler source fact. */
ZR_CORE_API const SZrFunctionSchedulerSourceFact *ZrCore_Function_FindSchedulerSourceFact(
        const SZrFunction *function,
        TZrUInt32 schedulerTypeId);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_Function_FindMetadataSignatureRecord(const SZrFunction *function,
                                                                                      TZrMetadataToken entityToken);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_Function_FindModuleMetadataTokenRecord(const SZrFunction *function,
                                                                                        TZrMetadataToken token);
ZR_CORE_API const SZrMetadataTokenRecord *ZrCore_Function_FindModuleMetadataSignatureRecord(
        const SZrFunction *function,
        TZrMetadataToken entityToken);
ZR_CORE_API const SZrMetadataTokenBinding *ZrCore_Function_FindModuleMetadataBinding(const SZrFunction *function,
                                                                                     TZrMetadataToken refToken);
ZR_CORE_API SZrMetadataTokenBinding *ZrCore_Function_UpsertModuleMetadataBinding(struct SZrState *state,
                                                                                  SZrFunction *function,
                                                                                  TZrMetadataToken refToken);
ZR_CORE_API TZrBool ZrCore_Function_BindMatchingTypeSpecMetadata(struct SZrState *state,
                                                                  SZrFunction *callerFunction,
                                                                  const SZrFunction *providerFunction);
ZR_CORE_API TZrBool ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(
        struct SZrState *state,
        SZrFunction *callerFunction,
        const SZrFunction *providerFunction,
        SZrMetadataTypeSpecBindStatus *status);
ZR_CORE_API TZrBool ZrCore_Function_BindMatchingTypeRefMetadata(struct SZrState *state,
                                                                 SZrFunction *callerFunction,
                                                                 const SZrFunction *providerFunction);
ZR_CORE_API TZrBool ZrCore_Function_BindMatchingTypeRefMetadataWithStatus(
        struct SZrState *state,
        SZrFunction *callerFunction,
        const SZrFunction *providerFunction,
        SZrMetadataTypeRefBindStatus *status);
ZR_CORE_API const SZrTypeLayout *ZrCore_Function_ResolvePrototypeFrameTypeLayout(const SZrFunction *function,
                                                                                 TZrUInt32 typeLayoutId,
                                                                                 TZrPtr userData);
ZR_CORE_API TZrBool ZrCore_Function_GetPrototypeFrameTypeLayoutRegistry(
        struct SZrState *state,
        const SZrFunction *function,
        TZrUInt32 requiredTypeLayoutId,
        struct SZrTypeLayoutRegistryView *outRegistry);
ZR_CORE_API struct SZrObjectPrototype *ZrCore_Function_ResolvePrototypeFrameStructPrototype(
        struct SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId);
ZR_CORE_API TZrBool ZrCore_Function_ResolvePrototypeFrameFieldLayout(struct SZrState *state,
                                                                     const SZrFunction *function,
                                                                     TZrUInt32 typeLayoutId,
                                                                     struct SZrString *fieldName,
                                                                     SZrFunctionFrameFieldLayout *outFieldLayout);
ZR_CORE_API TZrBool ZrCore_Function_VisitPrototypeFrameFieldLayouts(
        struct SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId,
        FZrFunctionPrototypeFrameFieldVisitor visitor,
        TZrPtr userData);
ZR_CORE_API void ZrCore_Function_FreePrototypeFrameTypeLayoutCache(struct SZrState *state, SZrFunction *function);
ZR_CORE_API TZrSize ZrCore_Function_GetFrameStorageSlotCount(const SZrFunction *function);
ZR_CORE_API TZrBool ZrCore_Function_MakeFrameSlotPlace(struct SZrState *state,
                                                       const SZrFunction *function,
                                                       TZrStackValuePointer frameBase,
                                                       TZrUInt32 stackSlot,
                                                       SZrStackFramePlace *outPlace);
ZR_CORE_API TZrBool ZrCore_Function_ResolveFrameSlotReferenceAnchor(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 stackSlot,
        const SZrFunction **outFunction,
        SZrFunctionStackAnchor *outFrameBase,
        TZrUInt32 *outStackSlot);
ZR_CORE_API TZrBool ZrCore_Function_GetFrameSlotInlineMemberValue(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 receiverStackSlot,
        struct SZrString *memberName,
        struct SZrTypeValue *result);
ZR_CORE_API TZrBool ZrCore_Function_SetFrameSlotInlineMemberValue(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 receiverStackSlot,
        struct SZrString *memberName,
        const struct SZrTypeValue *value);
ZR_CORE_API TZrBool ZrCore_Function_GetFrameSlotInlineMember(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 destinationStackSlot,
        TZrUInt32 receiverStackSlot,
        struct SZrString *memberName);
ZR_CORE_API TZrBool ZrCore_Function_SetFrameSlotInlineMember(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 sourceStackSlot,
        TZrUInt32 receiverStackSlot,
        struct SZrString *memberName);
ZR_CORE_API TZrBool ZrCore_Function_BindFrameSlotInlineArrayElement(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 aliasStackSlot,
        TZrUInt32 arrayStackSlot,
        TZrInt64 elementIndex);
ZR_CORE_API TZrBool ZrCore_Function_BindFrameSlotBorrowedAlias(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 aliasStackSlot,
        struct SZrCallInfo *sourceCallInfo,
        TZrStackValuePointer sourceFrameBase,
        TZrUInt32 sourceStackSlot);
ZR_CORE_API void ZrCore_Function_InitializeFrameLayoutStorage(struct SZrState *state,
                                                              TZrStackValuePointer functionBase,
                                                              const SZrFunction *function,
                                                              TZrSize preservedArgumentCount);
ZR_CORE_API TZrBool ZrCore_Function_FrameStackSlotIntersectsInlineStruct(const SZrFunction *function,
                                                                         TZrStackValuePointer frameBase,
                                                                         TZrStackValuePointer stackSlot);
ZR_CORE_API TZrBool ZrCore_Function_FrameStackSlotIntersectsFrameGcValue(const SZrFunction *function,
                                                                         TZrStackValuePointer frameBase,
                                                                         TZrStackValuePointer stackSlot);
ZR_CORE_API TZrBool ZrCore_Function_CopyFrameSlotInline(struct SZrState *state,
                                                        const SZrTypeLayout *layout,
                                                        const SZrFunction *destinationFunction,
                                                        TZrStackValuePointer destinationFrameBase,
                                                        TZrUInt32 destinationStackSlot,
                                                        const SZrFunction *sourceFunction,
                                                        TZrStackValuePointer sourceFrameBase,
                                                        TZrUInt32 sourceStackSlot);
ZR_CORE_API TZrBool ZrCore_Function_CopyObjectValueToFrameSlotInline(struct SZrState *state,
                                                                     const SZrFunction *destinationFunction,
                                                                     TZrStackValuePointer destinationFrameBase,
                                                                     TZrUInt32 destinationStackSlot,
                                                                     const struct SZrTypeValue *sourceValue);
ZR_CORE_API TZrBool ZrCore_Function_CopyObjectValueToInlineStorage(struct SZrState *state,
                                                                   const SZrFunction *destinationFunction,
                                                                   TZrUInt32 destinationTypeLayoutId,
                                                                   TZrPtr destinationAddress,
                                                                   TZrUInt32 destinationByteSize,
                                                                   const struct SZrTypeValue *sourceValue);
ZR_CORE_API TZrBool ZrCore_Function_InitInlineStorage(struct SZrState *state,
                                                      const SZrFunction *function,
                                                      TZrUInt32 typeLayoutId,
                                                      TZrPtr address,
                                                      TZrUInt32 byteSize);
ZR_CORE_API TZrBool ZrCore_Function_CopyFrameSlotInlineToObjectValue(struct SZrState *state,
                                                                     const SZrFunction *sourceFunction,
                                                                     TZrStackValuePointer sourceFrameBase,
                                                                     TZrUInt32 sourceStackSlot,
                                                                     struct SZrTypeValue *outValue);
ZR_CORE_API TZrBool ZrCore_Function_CopyInlineStorageToObjectValue(struct SZrState *state,
                                                                   const SZrFunction *sourceFunction,
                                                                   TZrUInt32 sourceTypeLayoutId,
                                                                   const void *sourceAddress,
                                                                   TZrUInt32 sourceByteSize,
                                                                   struct SZrTypeValue *outValue);
ZR_CORE_API TZrBool ZrCore_Function_CopyInlineFrameParameters(struct SZrState *state,
                                                              const SZrFunction *calleeFunction,
                                                              TZrStackValuePointer calleeFrameBase,
                                                              const SZrFunction *callerFunction,
                                                              TZrStackValuePointer callerFrameBase,
                                                              TZrUInt32 callerArgumentStartSlot,
                                                              FZrFunctionFrameTypeLayoutResolver resolver,
                                                              TZrPtr resolverUserData);
ZR_CORE_API TZrBool ZrCore_Function_BindAndCopyInlineFrameParametersFromCaller(
        struct SZrState *state,
        const SZrFunction *calleeFunction,
        TZrStackValuePointer calleeFunctionBase,
        TZrStackValuePointer callStackPointer,
        struct SZrCallInfo *callInfo);
ZR_CORE_API TZrBool ZrCore_Function_CopyValueFrameParameters(struct SZrState *state,
                                                             const SZrFunction *calleeFunction,
                                                             TZrStackValuePointer calleeFrameBase,
                                                             TZrStackValuePointer argumentBase,
                                                             TZrSize argumentsCount);
ZR_CORE_API TZrBool ZrCore_Function_CopyValueFrameParametersFromFrame(struct SZrState *state,
                                                                      const SZrFunction *calleeFunction,
                                                                      TZrStackValuePointer calleeFrameBase,
                                                                      const SZrFunction *sourceFunction,
                                                                      TZrStackValuePointer sourceFrameBase,
                                                                      TZrUInt32 sourceArgumentStartSlot,
                                                                      TZrSize argumentsCount);
ZR_CORE_API TZrBool ZrCore_Function_TryCopyInlineFrameReturnValue(struct SZrState *state,
                                                                  const struct SZrCallInfo *callInfo,
                                                                  TZrStackValuePointer returnSource,
                                                                  TZrStackValuePointer returnDestination,
                                                                  FZrFunctionFrameTypeLayoutResolver resolver,
                                                                  TZrPtr resolverUserData);
ZR_CORE_API TZrBool ZrCore_Function_TryCopyObjectReturnValueToInlineDestination(
        struct SZrState *state,
        const struct SZrCallInfo *callInfo,
        TZrStackValuePointer returnDestination,
        const struct SZrTypeValue *returnValue);
ZR_CORE_API TZrBool ZrCore_Function_TryCopyInlineReceiverParameterBack(struct SZrState *state,
                                                                       const struct SZrCallInfo *callInfo);
ZR_CORE_API void ZrCore_Function_TryCopyInlineConstructorReceiverBack(struct SZrState *state,
                                                                      const struct SZrCallInfo *callInfo);
ZR_CORE_API TZrStackValuePointer ZrCore_Function_GetCallInfoFrameStorageTop(struct SZrState *state,
                                                                            const struct SZrCallInfo *callInfo);
ZR_CORE_API void ZrCore_Function_SetCallInfoArgumentSourceFrame(struct SZrCallInfo *callInfo,
                                                                TZrStackValuePointer sourceFrameBase,
                                                                TZrUInt32 sourceStartSlot);
ZR_CORE_API TZrBool ZrCore_Function_VisitInlineFrameGcValues(struct SZrState *state,
                                                             const SZrFunction *function,
                                                             TZrStackValuePointer frameBase,
                                                             FZrFunctionFrameTypeLayoutResolver resolver,
                                                             TZrPtr resolverUserData,
                                                             FZrFunctionFrameGcValueVisitor visitor,
                                                             TZrPtr visitorUserData);
ZR_CORE_API TZrBool ZrCore_Function_VisitFrameGcValues(struct SZrState *state,
                                                       const SZrFunction *function,
                                                       TZrStackValuePointer frameBase,
                                                       FZrFunctionFrameTypeLayoutResolver resolver,
                                                       TZrPtr resolverUserData,
                                                       FZrFunctionFrameGcValueVisitor visitor,
                                                       TZrPtr visitorUserData);
ZR_CORE_API TZrBool ZrCore_Function_DropInlineFrameValues(struct SZrState *state,
                                                           const SZrFunction *function,
                                                           TZrStackValuePointer frameBase,
                                                           FZrFunctionFrameTypeLayoutResolver resolver,
                                                           TZrPtr resolverUserData);
ZR_CORE_API TZrBool ZrCore_Function_GetInlineConstructorInitializedFieldBitmapLayout(
        struct SZrState *state,
        const SZrFunction *function,
        TZrUInt32 *outBitmapByteOffset,
        TZrUInt32 *outInitializedFieldWordCount);
ZR_CORE_API TZrBool ZrCore_Function_GetInlineConstructorInitializedFieldBitmap(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        const TZrUInt64 **outInitializedFieldWords,
        TZrUInt32 *outInitializedFieldWordCount);
ZR_CORE_API TZrBool ZrCore_Function_MarkInlineConstructorFieldInitialized(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        TZrUInt32 receiverStackSlot,
        struct SZrString *memberName);
ZR_CORE_API TZrBool ZrCore_Function_DropInlineFrameValuesOnUnwind(
        struct SZrState *state,
        const SZrFunction *function,
        TZrStackValuePointer frameBase,
        FZrFunctionFrameTypeLayoutResolver resolver,
        TZrPtr resolverUserData);
ZR_CORE_API TZrBool ZrCore_Function_ApplyReturnEscape(struct SZrState *state,
                                                      const SZrFunction *function,
                                                      TZrUInt32 stackSlot,
                                                      const SZrTypeValue *value);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CheckStackAndAnchor(struct SZrState *state,
                                                                TZrSize size,
                                                               TZrStackValuePointer checkPointer,
                                                               TZrStackValuePointer stackPointer,
                                                               SZrFunctionStackAnchor *anchor);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallAndRestore(struct SZrState *state,
                                                          TZrStackValuePointer stackPointer,
                                                          TZrSize resultCount);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallWithoutYieldAndRestore(struct SZrState *state,
                                                                       TZrStackValuePointer stackPointer,
                                                                       TZrSize resultCount);
ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallWithoutYieldAndRestoreWithReturnDestination(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallWithoutYieldKnownValueAndRestore(struct SZrState *state,
                                                                                  TZrStackValuePointer stackPointer,
                                                                                  const struct SZrTypeValue *callableValue,
                                                                                  TZrSize resultCount);
ZR_CORE_API TZrStackValuePointer
ZrCore_Function_CallWithoutYieldKnownVmValueAndRestoreWithReceiverSource(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        struct SZrTypeValue *callableValue,
        TZrSize resultCount,
        TZrStackValuePointer receiverSourceFrameBase,
        TZrUInt32 receiverSourceSlot);

ZR_CORE_API TZrStackValuePointer
ZrCore_Function_CallWithoutYieldKnownValueAndRestoreWithInterpreterGenericContext(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        const struct SZrTypeValue *callableValue,
        TZrSize resultCount,
        const struct SZrTypeValue *interpreterGenericContext);

ZR_CORE_API TZrStackValuePointer
ZrCore_Function_CallWithoutYieldKnownValueAndRestoreWithInterpreterGenericContexts(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        const struct SZrTypeValue *callableValue,
        TZrSize resultCount,
        const struct SZrTypeValue *interpreterGenericContext,
        const struct SZrTypeValue *interpreterGenericMethodContext);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallAndRestoreAnchor(struct SZrState *state,
                                                                const SZrFunctionStackAnchor *anchor,
                                                                TZrSize resultCount);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallWithoutYieldAndRestoreAnchor(struct SZrState *state,
                                                                               const SZrFunctionStackAnchor *anchor,
                                                                               TZrSize resultCount);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallWithoutYieldKnownValueAndRestoreAnchor(
        struct SZrState *state,
        const SZrFunctionStackAnchor *anchor,
        const struct SZrTypeValue *callableValue,
        TZrSize resultCount);

ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCall(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                  TZrSize resultCount, TZrStackValuePointer returnDestination);

ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallKnownValue(struct SZrState *state,
                                                             TZrStackValuePointer stackPointer,
                                                             struct SZrTypeValue *callableValue,
                                                             TZrSize resultCount,
                                                             TZrStackValuePointer returnDestination);

ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallKnownVmValue(struct SZrState *state,
                                                             TZrStackValuePointer stackPointer,
                                                             struct SZrTypeValue *callableValue,
                                                             TZrSize resultCount,
                                                             TZrStackValuePointer returnDestination);
ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallKnownVmValueWithArgumentSource(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        struct SZrTypeValue *callableValue,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination,
        TZrStackValuePointer argumentSourceFrameBase,
        TZrUInt32 argumentSourceStartSlot);
ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallKnownVmValueWithReceiverSource(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        struct SZrTypeValue *callableValue,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination,
        TZrStackValuePointer receiverSourceFrameBase,
        TZrUInt32 receiverSourceSlot);

ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallResolvedVmFunction(struct SZrState *state,
                                                                     TZrStackValuePointer stackPointer,
                                                                     struct SZrFunction *function,
                                                                     TZrSize argumentsCount,
                                                                     TZrSize resultCount,
                                                                     TZrStackValuePointer returnDestination);

/*
 * Hot interpreter call sites already prepare state->stackTop to
 * stackPointer + 1 + argumentsCount before entering precall. This variant
 * preserves the normal resolved-vm semantics while letting those callers skip
 * the redundant stackTop write on the hot path.
 */
ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallPreparedResolvedVmFunction(struct SZrState *state,
                                                                             TZrStackValuePointer stackPointer,
                                                                             struct SZrFunction *function,
                                                                             TZrSize argumentsCount,
                                                                             TZrSize resultCount,
                                                                             TZrStackValuePointer returnDestination);
ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        struct SZrFunction *function,
        TZrSize argumentsCount,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination,
        TZrStackValuePointer argumentSourceFrameBase,
        TZrUInt32 argumentSourceStartSlot);

/*
 * Hot prepared resolved-VM call sites can often prove the exact-args,
 * no-entry-clear, no-debug steady-state shape before entering the generic
 * precall logic. This helper returns ZR_NULL when any colder condition forces
 * fallback to ZrCore_Function_PreCallPreparedResolvedVmFunction.
 */
ZR_CORE_API struct SZrCallInfo *ZrCore_Function_TryPreCallPreparedResolvedVmFunctionExactArgsSteadyState(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        struct SZrFunction *function,
        TZrSize argumentsCount,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination);

ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallResolvedNativeFunction(struct SZrState *state,
                                                                          TZrStackValuePointer stackPointer,
                                                                          FZrNativeFunction nativeFunction,
                                                                          TZrSize resultCount,
                                                                          TZrStackValuePointer returnDestination);

ZR_CORE_API TZrSize ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        TZrStackValuePointer preparedFunctionTop,
        FZrNativeFunction nativeFunction,
        TZrStackValuePointer returnDestination);

ZR_CORE_API TZrBool ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFastRestore(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        TZrStackValuePointer preparedFunctionTop,
        FZrNativeFunction nativeFunction,
        TZrMemoryOffset savedStackTopOffset,
        struct SZrCallInfo *savedCallInfo,
        TZrMemoryOffset originalCallInfoTopOffset,
        TZrBool hasOriginalCallInfoTopAnchor,
        struct SZrTypeValue *result,
        TZrMemoryOffset resultOffset,
        TZrBool hasResultAnchor);

ZR_CORE_API TZrSize ZrCore_Function_CallPreparedResolvedNativeFunction(struct SZrState *state,
                                                                       TZrStackValuePointer stackPointer,
                                                                       TZrStackValuePointer preparedFunctionTop,
                                                                       FZrNativeFunction nativeFunction,
                                                                       TZrSize resultCount,
                                                                       TZrStackValuePointer returnDestination);

ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCallKnownNativeValue(struct SZrState *state,
                                                                 TZrStackValuePointer stackPointer,
                                                                 struct SZrTypeValue *callableValue,
                                                                 TZrSize resultCount,
                                                                 TZrStackValuePointer returnDestination);

ZR_CORE_API TZrBool ZrCore_Function_TryReuseTailVmCall(struct SZrState *state,
                                                   struct SZrCallInfo *callInfo,
                                                   TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrCore_Function_PostCall(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount);

/*
 * Hot interpreter returns commonly resolve to a single result with no debug
 * hook state and plain call status. This variant lets those callers skip the
 * generic multi-result PostCall bookkeeping once they have already proven that
 * shape.
 */
ZR_CORE_API void ZrCore_Function_PostCallPreparedSingleResultFast(struct SZrState *state,
                                                                  struct SZrCallInfo *callInfo,
                                                                  TZrSize resultCount);
#endif // ZR_VM_CORE_FUNCTION_H
