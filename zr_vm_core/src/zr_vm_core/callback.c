//
// Created by HeJiahui on 2025/6/25.
//
#include "zr_vm_core/callback.h"

#include "zr_vm_core/state.h"

ZR_CALLBACK_IMPLEMENT_NO_PARAM(FZrAfterStateInitialized)

ZR_CALLBACK_IMPLEMENT_NO_PARAM(FZrBeforeStateReleased)

ZR_CALLBACK_IMPLEMENT_ONE_PARAM(FZrAfterThreadInitialized, threadState)

ZR_CALLBACK_IMPLEMENT_ONE_PARAM(FZrBeforeThreadReleased, threadState)
