#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_H

#include "zr_vm_parser/writer.h"

typedef enum EZrAotRuntimeContract {
    ZR_AOT_RUNTIME_CONTRACT_NONE = 0,
    ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF = 1 << 0,
    ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL = 1 << 1,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_BORROW = 1 << 2,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_LOAN = 1 << 3,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE = 1 << 4,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_DEGRADE = 1 << 5,
    ZR_AOT_RUNTIME_CONTRACT_ITER_INIT = 1 << 6,
    ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT = 1 << 7,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_DETACH = 1 << 8,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WAKE = 1 << 9,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_DROP = 1 << 10,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RETURN_LOAN = 1 << 11
} EZrAotRuntimeContract;

typedef enum EZrAotExecIrCallsiteKind {
    ZR_AOT_EXEC_IR_CALLSITE_KIND_NONE = 0,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_STATIC_DIRECT = 1,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_DIRECT_PROBE = 2,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_META = 3,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_GENERIC = 4
} EZrAotExecIrCallsiteKind;

typedef enum EZrAotExecIrTerminatorKind {
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_NONE = 0,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_FALLTHROUGH = 1,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_BRANCH = 2,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_CONDITIONAL_BRANCH = 3,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_RETURN = 4,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_TAIL_RETURN = 5,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_EH_RESUME = 6
} EZrAotExecIrTerminatorKind;

typedef struct SZrAotExecIrInstruction {
    TZrUInt32 functionIndex;
    TZrUInt32 blockIndex;
    TZrUInt32 semIrOpcode;
    TZrUInt32 execInstructionIndex;
    TZrUInt32 typeTableIndex;
    TZrUInt32 effectTableIndex;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
    TZrUInt32 deoptId;
    TZrUInt32 debugLine;
    TZrUInt32 debugLineEnd;
    TZrUInt32 debugColumn;
    TZrUInt32 debugColumnEnd;
    TZrUInt32 callsiteKind;
} SZrAotExecIrInstruction;

typedef struct SZrAotExecIrFrameSlotLayout {
    TZrUInt32 stackSlot;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 typeLayoutId;
    TZrUInt8 slotKind;
    TZrUInt8 isParameter;
    TZrUInt16 reserved0;
} SZrAotExecIrFrameSlotLayout;

typedef enum EZrAotExecIrParameterPassingForm {
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_UNKNOWN = 0,
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE,
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_IN,
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF,
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF_READONLY,
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF,
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF_READONLY,
    ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT,
} EZrAotExecIrParameterPassingForm;

typedef struct SZrAotExecIrParameterLayout {
    TZrUInt32 stackSlot;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 placeId;
    TZrUInt32 roleFlags;
    TZrBool passingFormKnown;
    TZrUInt32 passingForm;
    TZrBool defaultDeclarationKnown;
    TZrBool hasDeclaredDefault;
    SZrFunctionTypedTypeRef type;
} SZrAotExecIrParameterLayout;

static inline TZrBool backend_aot_exec_ir_parameter_passing_form_is_valid(
        const SZrAotExecIrParameterLayout *layout) {
    if (layout == ZR_NULL ||
        (layout->passingFormKnown != ZR_FALSE &&
         layout->passingFormKnown != ZR_TRUE)) {
        return ZR_FALSE;
    }
    if (layout->passingFormKnown == ZR_FALSE) {
        return (TZrBool)(
                layout->passingForm ==
                (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_UNKNOWN);
    }
    return (TZrBool)(
            layout->passingForm >=
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE &&
            layout->passingForm <=
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT);
}

static inline EZrAotParameterPassingMode
backend_aot_exec_ir_parameter_passing_mode(
        const SZrAotExecIrParameterLayout *layout) {
    if (!backend_aot_exec_ir_parameter_passing_form_is_valid(layout) ||
        layout->passingFormKnown != ZR_TRUE) {
        return ZR_AOT_PARAMETER_PASSING_UNKNOWN;
    }
    switch ((EZrAotExecIrParameterPassingForm)layout->passingForm) {
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE:
            return ZR_AOT_PARAMETER_PASSING_VALUE;
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_IN:
            return ZR_AOT_PARAMETER_PASSING_IN;
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF:
            return ZR_AOT_PARAMETER_PASSING_REF;
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF_READONLY:
            return ZR_AOT_PARAMETER_PASSING_REF_READONLY;
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF:
            return ZR_AOT_PARAMETER_PASSING_SCOPED_REF;
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF_READONLY:
            return ZR_AOT_PARAMETER_PASSING_SCOPED_REF_READONLY;
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT:
            return ZR_AOT_PARAMETER_PASSING_OUT;
        case ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_UNKNOWN:
        default:
            return ZR_AOT_PARAMETER_PASSING_UNKNOWN;
    }
}

static inline TZrBool backend_aot_exec_ir_parameter_is_value_passing(
        const SZrAotExecIrParameterLayout *layout) {
    return (TZrBool)(
            backend_aot_exec_ir_parameter_passing_form_is_valid(layout) &&
            layout->passingFormKnown == ZR_TRUE &&
            layout->passingForm ==
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE);
}

static inline TZrBool backend_aot_exec_ir_parameter_default_declaration_is_valid(
        const SZrAotExecIrParameterLayout *layout) {
    if (layout == ZR_NULL ||
        (layout->defaultDeclarationKnown != ZR_FALSE &&
         layout->defaultDeclarationKnown != ZR_TRUE) ||
        (layout->hasDeclaredDefault != ZR_FALSE &&
         layout->hasDeclaredDefault != ZR_TRUE)) {
        return ZR_FALSE;
    }

    return (TZrBool)(!layout->hasDeclaredDefault ||
                     layout->defaultDeclarationKnown);
}

typedef struct SZrAotExecIrFrameLayout {
    TZrUInt32 parameterCount;
    TZrUInt32 stackSlotCount;
    TZrUInt32 generatedFrameSlotCount;
    TZrUInt32 closureValueCount;
    TZrUInt32 localVariableCount;
    TZrUInt32 exportedValueCount;
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    TZrUInt32 parameterLayoutCount;
    SZrAotExecIrParameterLayout *parameterLayouts;
    TZrUInt32 slotLayoutCount;
    SZrAotExecIrFrameSlotLayout *slotLayouts;
} SZrAotExecIrFrameLayout;

typedef struct SZrAotExecIrBasicBlock {
    TZrUInt32 blockId;
    TZrUInt32 firstExecInstructionIndex;
    TZrUInt32 instructionCount;
    TZrUInt32 firstInstructionOffset;
    TZrUInt32 semIrInstructionCount;
    TZrUInt32 terminatorInstructionIndex;
    TZrUInt32 terminatorKind;
    TZrUInt32 successorCount;
    TZrUInt32 successorBlockIndices[2];
} SZrAotExecIrBasicBlock;

typedef struct SZrAotExecIrFunction {
    const SZrFunction *function;
    const SZrFunction *metadataEntryFunction;
    TZrUInt32 flatIndex;
    TZrUInt32 parentFunctionIndex;
    TZrUInt32 runtimeContracts;
    TZrUInt32 firstInstructionOffset;
    TZrUInt32 instructionCount;
    TZrUInt32 execInstructionCount;
    TZrBool callableReturnTypeKnown;
    SZrFunctionTypedTypeRef callableReturnType;
    TZrBool directInlineReturnLayoutKnown;
    TZrUInt32 directInlineReturnTypeLayoutId;
    SZrAotExecIrFrameLayout frameLayout;
    SZrAotExecIrBasicBlock *basicBlocks;
    TZrUInt32 basicBlockCount;
} SZrAotExecIrFunction;

static inline const SZrFunctionTypedTypeRef *backend_aot_exec_ir_callable_return_type(
        const SZrAotExecIrFunction *functionIr) {
    if (functionIr == ZR_NULL || functionIr->callableReturnTypeKnown != ZR_TRUE) {
        return ZR_NULL;
    }
    return &functionIr->callableReturnType;
}

static inline TZrUInt32 backend_aot_exec_ir_direct_inline_return_type_layout_id(
        const SZrAotExecIrFunction *functionIr) {
    if (functionIr == ZR_NULL ||
        functionIr->directInlineReturnLayoutKnown != ZR_TRUE ||
        functionIr->directInlineReturnTypeLayoutId ==
                ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }
    return functionIr->directInlineReturnTypeLayoutId;
}

typedef struct SZrAotExecIrModule {
    SZrAotExecIrInstruction *instructions;
    TZrUInt32 instructionCount;
    TZrUInt32 runtimeContracts;
    SZrAotExecIrFunction *functions;
    TZrUInt32 functionCount;
} SZrAotExecIrModule;

const TZrChar *backend_aot_exec_ir_semir_opcode_name(TZrUInt32 opcode);
const TZrChar *backend_aot_exec_ir_runtime_contract_name(TZrUInt32 contractBit);
TZrUInt32 backend_aot_exec_ir_runtime_contract_count(TZrUInt32 runtimeContracts);
const TZrChar *backend_aot_exec_ir_callsite_kind_name(TZrUInt32 callsiteKind);
const TZrChar *backend_aot_exec_ir_terminator_kind_name(TZrUInt32 terminatorKind);

TZrBool backend_aot_exec_ir_build_module(SZrState *state, SZrFunction *function, SZrAotExecIrModule *outModule);
void backend_aot_exec_ir_release_module(SZrState *state, SZrAotExecIrModule *module);
const SZrAotExecIrFunction *backend_aot_exec_ir_find_function(const SZrAotExecIrModule *module, TZrUInt32 functionIndex);

#endif
