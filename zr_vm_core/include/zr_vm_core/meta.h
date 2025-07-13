//
// Created by HeJiahui on 2025/6/25.
//

#ifndef ZR_VM_CORE_META_H
#define ZR_VM_CORE_META_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrGlobalState;

struct ZR_STRUCT_ALIGN SZrMeta {
    EZrMetaType metaType;
    struct SZrFunction *function;
};

typedef struct SZrMeta SZrMeta;

struct ZR_STRUCT_ALIGN SZrMetaTable {
    SZrMeta *metas[ZR_META_ENUM_MAX];
};

typedef struct SZrMetaTable SZrMetaTable;

ZR_CORE_API void ZrMetaGlobalStaticsInit(struct SZrState *state);

ZR_CORE_API void ZrMetaTableConstruct(SZrMetaTable *table);

#endif // ZR_VM_CORE_META_H
