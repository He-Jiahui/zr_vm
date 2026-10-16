#ifndef ZR_VM_CORE_CLOSURE_CLOSE_PROXY_TOKEN_H
#define ZR_VM_CORE_CLOSURE_CLOSE_PROXY_TOKEN_H

#include "zr_vm_core/stack.h"

struct SZrState;

/** @brief 安装关闭链使用的私有 token；身份地址与表示均不属于 artifact ABI。
 * @pre state 与两槽有效，proxySlot 为空且已处于活栈；高位和链头约束由 MarkCloseProxy 检查。
 * @note token 只保存 source 与自身登记槽的栈偏移，不复制或持有 source 的资源值。
 * @return RawObject_New 返回空时为 false；成功把 GC 管理的 NATIVE_DATA 发布到 proxySlot。
 * @note 内存耗尽可由分配层抛出 MEMORY_ERROR，不能只按布尔返回处理。 */
TZrBool ZrCore_ClosureProxyToken_Install(struct SZrState *state,
                                        TZrStackValuePointer proxySlot,
                                        TZrStackValuePointer sourceSlot);
/** @brief 识别当前登记槽的私有 token，返回其 source 栈偏移。
 * @note 同时检查身份、三值表示、自身登记偏移以及 source 的范围和槽对齐；
 * token 移到其他槽后不能据此关闭原 source。
 * @return 不匹配时为 false 且不写 outSourceOffset。 */
TZrBool ZrCore_ClosureProxyToken_GetSourceOffset(struct SZrState *state,
                                                TZrStackValuePointer proxySlot,
                                                TZrMemoryOffset *outSourceOffset);

#endif
