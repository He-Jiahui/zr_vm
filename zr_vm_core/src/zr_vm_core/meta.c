//
// Created by HeJiahui on 2025/6/25.
//

#include "zr_vm_core/meta.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

void ZrMetaGlobalStaticsInit(SZrState *state) {
    SZrGlobalState *global = state->global;
    for (TEnum i = 0; i < ZR_META_ENUM_MAX; i++) {
        global->metaFunctionName[i] = ZrStringCreateFromNative(state, CZrMetaName[i]);
        ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->metaFunctionName[i]));
    }
}


void ZrMetaTableConstruct(SZrMetaTable *table) {
    for (TEnum i = 0; i < ZR_META_ENUM_MAX; i++) {
        table->metas[i] = ZR_NULL;
    }
}
