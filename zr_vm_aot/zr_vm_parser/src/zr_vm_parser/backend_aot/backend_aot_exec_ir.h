#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_H

#include "zr_vm_parser/writer.h"

/** @brief 旧 AOT emitter 为 SemIR 指令要求的运行时辅助能力位集。 */
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

/** @brief 调用点可静态识别的分派类别，供 emitter 选择调用路径。 */
typedef enum EZrAotExecIrCallsiteKind {
    ZR_AOT_EXEC_IR_CALLSITE_KIND_NONE = 0,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_STATIC_DIRECT = 1,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_DIRECT_PROBE = 2,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_META = 3,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_GENERIC = 4
} EZrAotExecIrCallsiteKind;

/** @brief 旧执行字节码块的离开方式；EH_RESUME 包含异常控制转移。 */
typedef enum EZrAotExecIrTerminatorKind {
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_NONE = 0,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_FALLTHROUGH = 1,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_BRANCH = 2,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_CONDITIONAL_BRANCH = 3,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_RETURN = 4,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_TAIL_RETURN = 5,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_EH_RESUME = 6
} EZrAotExecIrTerminatorKind;

/** @brief 将 SemIR 指令与执行字节码、类型/效果表和源码跨度关联的投影。 */
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

/** @brief 原始帧槽的 ABI 投影；reserved0 保留别名标志但剔除运行时派生的 DIRECT_VALUE。 */
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

/** @brief 参数传递形式；UNKNOWN 仅表示上游未提供可证明的形式。 */
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

/** @brief 参数身份、类型及默认声明的证据；Known 位区分未知与明确为否。 */
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

/** @brief 校验 passingFormKnown 与 passingForm 的一致性。 */
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

/** @brief 将已证实的 ExecIR 参数形式映射为 emitter 的传参模式。 */
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

/** @brief 仅在形式明确且为 VALUE 时允许按值路径。 */
static inline TZrBool backend_aot_exec_ir_parameter_is_value_passing(
        const SZrAotExecIrParameterLayout *layout) {
    return (TZrBool)(
            backend_aot_exec_ir_parameter_passing_form_is_valid(layout) &&
            layout->passingFormKnown == ZR_TRUE &&
            layout->passingForm ==
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE);
}

/** @brief 拒绝没有默认声明证据却声称存在默认值的布局。 */
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

/** @brief 函数帧与参数 ABI 的拥有者；两个数组由 build_frame_layout 分配。 */
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

/** @brief 执行指令区间及其 SemIR 投影范围、终结符和最多两个后继。 */
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

/** @brief 单函数旧 AOT 投影；function 指针借用原函数图，frameLayout/basicBlocks 归模块持有。 */
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

/** @brief 仅返回已知的可调用返回类型；指针生命周期受函数投影约束。 */
static inline const SZrFunctionTypedTypeRef *backend_aot_exec_ir_callable_return_type(
        const SZrAotExecIrFunction *functionIr) {
    if (functionIr == ZR_NULL || functionIr->callableReturnTypeKnown != ZR_TRUE) {
        return ZR_NULL;
    }
    return &functionIr->callableReturnType;
}

/** @brief 仅返回经所有 RETURN_TYPED 路径证明的直接内联返回布局 ID。 */
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

/** @brief 旧 AOT emitter 使用的模块投影及其拥有的指令/函数数组。 */
typedef struct SZrAotExecIrModule {
    SZrAotExecIrInstruction *instructions;
    TZrUInt32 instructionCount;
    TZrUInt32 runtimeContracts;
    SZrAotExecIrFunction *functions;
    TZrUInt32 functionCount;
} SZrAotExecIrModule;

/** @brief 将 SemIR opcode 转为清单可读名称；未知值归为 NOP。 */
const TZrChar *backend_aot_exec_ir_semir_opcode_name(TZrUInt32 opcode);
/** @brief 将单个运行时契约位转为清单名称。 */
const TZrChar *backend_aot_exec_ir_runtime_contract_name(TZrUInt32 contractBit);
/** @brief 计数旧 AOT 已定义的运行时契约位。 */
TZrUInt32 backend_aot_exec_ir_runtime_contract_count(TZrUInt32 runtimeContracts);
/** @brief 将调用点分类值转为清单名称。 */
const TZrChar *backend_aot_exec_ir_callsite_kind_name(TZrUInt32 callsiteKind);
/** @brief 将终结符分类值转为清单名称。 */
const TZrChar *backend_aot_exec_ir_terminator_kind_name(TZrUInt32 terminatorKind);

/** @brief 展平函数图并投影旧 ExecIR，供 C/LLVM emitter 共用。
 *  @note 成功结果由 release_module 释放；该投影仍借用原 SZrFunction。 */
TZrBool backend_aot_exec_ir_build_module(SZrState *state, SZrFunction *function, SZrAotExecIrModule *outModule);
/** @brief 释放模块投影的数组、函数帧及基本块。 */
void backend_aot_exec_ir_release_module(SZrState *state, SZrAotExecIrModule *module);
/** @brief 按保留的扁平索引查询函数投影；返回借用指针。 */
const SZrAotExecIrFunction *backend_aot_exec_ir_find_function(const SZrAotExecIrModule *module, TZrUInt32 functionIndex);

#endif
