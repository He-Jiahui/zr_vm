//
// Created by HeJiahui on 2025/6/25.
//

#include "zr_vm_core/meta.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/global.h"

void ZrMetaInit(SZrState *state) {
    SZrGlobalState *global = state->global;
    for (TInt32 i = 0; i < ZR_META_ENUM_MAX; i++) {
        global->metaFunctionName[i] = ZrStringCreateFromNative(state, CZrMetaName[i]);
        ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->metaFunctionName[i]));
    }
}
