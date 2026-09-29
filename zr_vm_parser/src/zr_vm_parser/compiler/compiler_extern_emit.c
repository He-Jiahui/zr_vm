//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

/*
 * 外部 FFI 声明使用这些辅助函数编译模块导入、成员查找与函数调用指令；
 * 导入/调用 helper 将结果归入调用方预分配的持久槽，成员查找 helper 写入调用方指定的槽。
 * 调用方须为成员查找提供有效 objectSlot；helper 以 false 传回校验或发射失败，
 * 调用层据此结束当前声明块，可诊断失败由相应发射或诊断路径设置 hasError。
 */
TZrBool extern_compiler_emit_get_member_to_slot(SZrCompilerState *cs,
                                                       TZrUInt32 destSlot,
                                                       TZrUInt32 objectSlot,
                                                       SZrString *memberName) {
    TZrUInt32 memberId;

    if (cs == ZR_NULL || memberName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    memberId = compiler_get_or_add_member_entry(cs, memberName);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        /* module/method callers 会把索引分配失败报告为编译错误，再中止 extern 声明。 */
        return ZR_FALSE;
    }

    /*
     * BUG: 栈槽由 u32 分配，当前编译路径没有 UINT16_MAX 上限；extern 块前的持久
     * 局部和本函数的临时槽都从 stackSlotCount 取号。任一被编码的目标槽或对象槽
     * 首次大于 UINT16_MAX 时，下面的显式窄化只保留低 16 位（slot & 0xffff），
     * GET_MEMBER 因而可能覆盖或读取更早的局部槽；之后读取该局部值可观察到污染。
     * 例如脚本先有 65,536 个持久局部，再编译 extern 声明分配 hidden ffi/library
     * 局部和临时槽，就会让后续 GET_MEMBER 操作数跨过该边界；这不假定其目标槽
     * 恰等于 65,536。create_instruction_2 的 operandExtra 和 operand1 字段都是 u16，
     * 函数 stackSize 则保留为 u32；超过 65,536 个槽时优化器也会跳过密集重映射。
     * BUG: memberId 是按 u32 长度增长的 memberEntries 索引，随后窄化到 u16。
     * 全局名赋值会向同一 currentFunction 表登记符号；compile_if 会生成条件跳转并
     * 编译 then 分支，因此可在运行时为 false 的动态分支里调整不同全局名数量，
     * 使后续 extern 查找前的表长为 65,536。下一索引 65,536 编码为 0；若先以一个
     * 外部模块未导出的名称占据 entry0，运行时 GET_MEMBER 会按该名称查属性并报
     * missing-member，或在该名称有不同值时读错属性。表的 u32 扩容见
     * compiler_instruction.c:311-346，分支编译见 compile_statement.c:3792-3800，
     * 全局名登记见 compile_expression.c:2021-2026，运行时解名见
     * execution_dispatch.c:8675、8687-91。
     */
    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                          (TZrUInt16)destSlot,
                                          (TZrUInt16)objectSlot,
                                          (TZrUInt16)memberId));
    return ZR_TRUE;
}

/*
 * 外部导入表达式会留下一个临时结果槽；此处复制到调用方预留的局部槽，再回收
 * 临时栈尾。调用方持有 localSlot，返回的布尔值表示导入指令是否成功发射；false
 * 会沿 compile_extern_block_declaration 的检查提前退出当前 extern block。
 * BUG: localSlot 可超过 UINT16_MAX；SET_STACK 的 operandExtra 是 u16，目标槽窄化
 * 后会别名较早的局部槽。达到这一门槛的脚本仍以 u32 stackSize 编译通过。
 */
TZrBool extern_compiler_emit_import_module_to_local(SZrCompilerState *cs,
                                                           SZrString *moduleName,
                                                           TZrUInt32 localSlot,
                                                           SZrFileRange location) {
    TZrUInt32 importSlot;

    if (cs == ZR_NULL || moduleName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    importSlot = ZrParser_Compiler_EmitImportModuleExpression(cs, moduleName, location);
    if (importSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }

    /* 将导入表达式的临时结果移入声明预先分配的持久局部槽。 */
    if (importSlot != localSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)importSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

/*
 * 先从模块槽读取函数，再把参数常量依次放在函数槽之后供 FUNCTION_CALL 使用；
 * 调用结果先落回 functionSlot，之后复制到 localSlot 并裁掉临时参数槽。当前
 * extern 声明调用此 helper 时传入一个 loadLibrary 参数。
 * BUG: moduleSlot、functionSlot、参数临时槽和 localSlot 都是 u32 槽；GET_MEMBER、
 * FUNCTION_CALL、SET_STACK 及 emit_constant_to_slot 写入的槽字段为 u16。被编码的
 * 槽号超过 UINT16_MAX 时会落到低 16 位槽，可能读取或改写已有局部值。
 */
TZrBool extern_compiler_emit_module_function_call_to_local(SZrCompilerState *cs,
                                                                  TZrUInt32 moduleSlot,
                                                                  SZrString *functionName,
                                                                  const SZrTypeValue *argumentValues,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 localSlot,
                                                                  SZrFileRange location) {
    TZrUInt32 functionSlot;

    if (cs == ZR_NULL || functionName == ZR_NULL || (argumentCount > 0 && argumentValues == ZR_NULL) || cs->hasError) {
        return ZR_FALSE;
    }

    functionSlot = allocate_stack_slot(cs);
    if (!extern_compiler_emit_get_member_to_slot(cs, functionSlot, moduleSlot, functionName)) {
        ZrParser_Compiler_Error(cs, "failed to resolve extern ffi module function", location);
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < argumentCount; index++) {
        TZrUInt32 argumentSlot = allocate_stack_slot(cs);
        emit_constant_to_slot(cs, argumentSlot, &argumentValues[index]);
    }

    /* FUNCTION_CALL 的参数窗口从 functionSlot 后连续读取；结果覆盖函数临时槽。 */
    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)argumentCount));
    if (functionSlot != localSlot) {
        /* BUG: localSlot 超过 UINT16_MAX 时 SET_STACK 会写入同低 16 位的旧槽。 */
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)functionSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

/*
 * 外部库方法调用按实例调用约定把 receiver 复制为第一个实参，再接上声明参数；
 * 调用方须传入有效 receiverSlot，本 helper 不检查该槽是否属于当前栈帧。
 * FUNCTION_CALL 因此接收 argumentCount + 1，结果归入调用方的 localSlot；false 会
 * 传回 declaration caller，由它提前退出当前 extern block。
 * BUG: receiverSlot、functionSlot、selfSlot、参数临时槽和 localSlot 都是 u32 栈槽；
 * GET_MEMBER、FUNCTION_CALL、SET_STACK 及 emit_constant_to_slot 写入的槽字段为 u16。
 * 被编码的槽号超过 UINT16_MAX 时可能与较早的局部槽重合并污染其值。
 */
TZrBool extern_compiler_emit_method_call_to_local(SZrCompilerState *cs,
                                                         TZrUInt32 receiverSlot,
                                                         SZrString *methodName,
                                                         const SZrTypeValue *argumentValues,
                                                         TZrUInt32 argumentCount,
                                                         TZrUInt32 localSlot,
                                                         SZrFileRange location) {
    TZrUInt32 functionSlot;
    TZrUInt32 selfSlot;

    if (cs == ZR_NULL || methodName == ZR_NULL || (argumentCount > 0 && argumentValues == ZR_NULL) || cs->hasError) {
        return ZR_FALSE;
    }

    functionSlot = allocate_stack_slot(cs);
    if (!extern_compiler_emit_get_member_to_slot(cs, functionSlot, receiverSlot, methodName)) {
        ZrParser_Compiler_Error(cs, "failed to resolve extern ffi receiver method", location);
        return ZR_FALSE;
    }

    /* BUG: selfSlot 窄化到 SET_STACK 的 u16 目标字段后可能覆盖较早的局部槽。 */
    selfSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                          (TZrUInt16)selfSlot,
                                          (TZrInt32)receiverSlot));
    for (TZrUInt32 index = 0; index < argumentCount; index++) {
        TZrUInt32 argumentSlot = allocate_stack_slot(cs);
        emit_constant_to_slot(cs, argumentSlot, &argumentValues[index]);
    }

    /* 方法 receiver 已计入参数数；返回值先写到 functionSlot 临时槽。 */
    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)(argumentCount + 1)));
    if (functionSlot != localSlot) {
        /* BUG: localSlot 超过 UINT16_MAX 时 SET_STACK 会写入同低 16 位的旧槽。 */
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)functionSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

// 进入新作用域
