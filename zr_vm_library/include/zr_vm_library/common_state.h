//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_COMMON_STATE_H
#define ZR_VM_LIBRARY_COMMON_STATE_H
#include "zr_vm_library/conf.h"

/** @brief 给 CLI 和简易宿主提供从 .zrp 配置建立 global 的默认路径。
 *  默认分配器使用系统堆；构造器把 Project 与源码加载器挂在 global 上，
 *  Free 负责反向释放这些资源。宿主须先结束所有仍使用该 global 的 state 和 worker。
 */

struct ZR_STRUCT_ALIGN SZrLibrary_CommonState {
    SZrGlobalState *globalState;
};

typedef struct SZrLibrary_CommonState SZrLibrary_CommonState;

ZR_LIBRARY_API TZrPtr ZrLibrary_CommonState_BuiltinAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
                                                             TZrSize newSize, TZrInt64 flag);

/** @brief 读取配置并建立供 CLI、测试或嵌入式宿主使用的 global。
 *  @return 成功时返回持有 Project 和源码加载器的 global；配置或 Project 加载失败返回 NULL。构造时尝试挂接 native registry，挂接失败目前不影响返回值。
 *  @note 返回值必须由 CommonGlobalState_Free 释放，且不可早于所有使用它的线程。
 *  @note GlobalState_New 分配失败时当前实现可能在返回前解引用 NULL，见实现中的 BUG 标记。
 *  @note TODO: Attach 可因分配失败返回 false；需明确构造成功是否应保证 registry 就绪，或由后续调用重试挂接。
 */
ZR_LIBRARY_API SZrGlobalState *ZrLibrary_CommonState_CommonGlobalState_New(TZrNativeString configFilePath);

ZR_LIBRARY_API void ZrLibrary_CommonState_CommonGlobalState_Free(SZrGlobalState *globalState);

#endif // ZR_VM_LIBRARY_COMMON_STATE_H
