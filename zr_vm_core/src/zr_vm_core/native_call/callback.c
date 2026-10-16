//
// Created by HeJiahui on 2025/6/25.
//
#include "zr_vm_core/callback.h"

#include "zr_vm_core/state.h"

/* 每个宏生成 FZrTryFunction 形状的同步桥接函数，供生命周期路径经 TryRun 捕获异常状态。 */
ZR_CALLBACK_IMPLEMENT_NO_PARAM(FZrAfterStateInitialized)

ZR_CALLBACK_IMPLEMENT_NO_PARAM(FZrBeforeStateReleased)

ZR_CALLBACK_IMPLEMENT_ONE_PARAM(FZrAfterThreadInitialized, threadState)

/* GC 当前直接调用此回调字段；桥接函数本身不会替该直接调用建立恢复点。 */
ZR_CALLBACK_IMPLEMENT_ONE_PARAM(FZrBeforeThreadReleased, threadState)
