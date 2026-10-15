#ifndef ZR_VM_PARSER_EXEC_IR_ARRAY_LOWERING_H
#define ZR_VM_PARSER_EXEC_IR_ARRAY_LOWERING_H

#include "zr_vm_core/contiguous_view.h"
#include "zr_vm_parser/conf.h"

ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerArrayIndex(
        const SZrContiguousView *view, TZrInt64 index, TZrSize *offset,
        SZrViewDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool ZrParser_ExecIr_LowerArrayIndex(const SZrContiguousView *view, TZrInt64 index, TZrSize *offset, SZrViewDiagnostic *diagnostic)
 * ## Summary
 * 将 Exec IR 的有符号数组元素下标交给 Core 连续视图接口校验，并在成功时
 * 返回相对 owner 的字节偏移。此适配接口只计算偏移，不返回元素地址，也不
 * 执行元素读写。
 *
 * ## Parameters
 * @param[in] view 借用的连续视图描述符；本函数只在调用期间读取它，不保存其
 *                 指针。空指针或无效描述符导致失败。
 * @param[in] index 以元素为单位的有符号下标，只有 0 <= index < view->length
 *                  才能成功。
 * @param[out] offset 成功时接收 view->byteOffset + index * view->stride 的字节
 *                    偏移。可以为空；为空时返回失败。失败时调用方不得读取该值。
 * @param[out] diagnostic 可选的值型失败诊断；可以为空以丢弃诊断，不转移资源所有权。
 *
 * ## Returns
 * @return 描述符、下标和输出均通过 Core 校验时返回 ZR_TRUE 并写入 offset；否则
 *         返回 ZR_FALSE，并在 diagnostic 非空时提供 Core 诊断类别。
 *
 * ## Constraints and lifecycle
 * view 与 diagnostic 只借用到本次同步调用结束；调用方应保证非空 offset 与
 * diagnostic 输出彼此不重叠，且均不与 view 重叠，以免写输出改变输入元数据。
 * 该接口不解引用或保活 ownerRoot，也不建立 GC root 或 pin。底层校验以
 * generation 0 跳过当前代数比较；描述符中的 PINNED 标记只校验 lifetimeRegion
 * 非零，不证明真实 pin、owner 生命周期、实际 backing 范围或写权限。调用方须在
 * 真实访问前维持 owner 与所需借用/pin，核对当前 generation、存储边界及访问权限；
 * 失败时不能使用 offset。
 *
 * ## References
 * @see ZrCore_View_IndexOffset 执行底层的下标边界与 checked 字节偏移计算。
 * @see tests/core/test_ssa_arrays_slices.c 对 facade 的成功、边界和空输出路径作运行期断言。
 * @see docs/plans/ssa/05-data-layout/02-arrays-slices.md 定义连续视图数组下标计划。
 */
