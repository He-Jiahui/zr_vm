#ifndef ZR_VM_PARSER_BACKEND_AOT_CALLABLE_PROVENANCE_H
#define ZR_VM_PARSER_BACKEND_AOT_CALLABLE_PROVENANCE_H

#include "backend_aot_function_table.h"

/** @brief 为 C/LLVM 函数体生成期间的栈槽建立可调用函数来源表。
 *  @return 调用方持有的索引数组；零槽位或分配失败时返回空指针。
 *  @note 初始值均为 ZR_AOT_INVALID_FUNCTION_INDEX。 */
TZrUInt32 *backend_aot_allocate_callable_slot_function_indices(SZrState *state, const SZrFunction *function);
/** @brief 按生成帧槽数释放来源表；允许空指针。 */
void backend_aot_release_callable_slot_function_indices(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrUInt32 *slotFunctionIndices);
/** @brief 查询栈槽的静态函数索引；槽位无效或来源未知时返回无效索引。 */
TZrUInt32 backend_aot_get_callable_slot_function_index(const TZrUInt32 *slotFunctionIndices,
                                                       const SZrFunction *function,
                                                       TZrUInt32 slotIndex);
/** @brief 在已分配来源表中记录栈槽的静态函数索引；无效槽位不写入。 */
void backend_aot_set_callable_slot_function_index(TZrUInt32 *slotFunctionIndices,
                                                  const SZrFunction *function,
                                                  TZrUInt32 slotIndex,
                                                  TZrUInt32 functionIndex);
/** @brief 在指定执行指令之前反向追溯栈槽的可调用函数来源。
 *  @pre instructionLimit 不超过 function->instructionsLength，且非空范围有指令数组。
 *  @return 无法静态证明唯一来源时返回无效索引。 */
TZrUInt32 backend_aot_resolve_callable_slot_function_index_before_instruction(const SZrAotFunctionTable *table,
                                                                              SZrState *state,
                                                                              const SZrFunction *function,
                                                                              TZrUInt32 instructionLimit,
                                                                              TZrUInt32 slotIndex,
                                                                              TZrUInt32 recursionDepth);

#endif
