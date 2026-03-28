//
// Created by HeJiahui on 2025/6/25.
//

#ifndef ZR_VM_CORE_META_H
#define ZR_VM_CORE_META_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrGlobalState;
struct SZrTypeValue;
struct SZrObject;

struct ZR_STRUCT_ALIGN SZrMeta {
    EZrMetaType metaType;
    struct SZrFunction *function;
};

typedef struct SZrMeta SZrMeta;

struct ZR_STRUCT_ALIGN SZrMetaTable {
    SZrMeta *metas[ZR_META_ENUM_MAX];
};

typedef struct SZrMetaTable SZrMetaTable;

ZR_CORE_API void ZrCore_Meta_GlobalStaticsInit(struct SZrState *state);

ZR_CORE_API void ZrCore_MetaTable_Construct(SZrMetaTable *table);

ZR_CORE_API void ZrCore_Meta_InitBuiltinTypeMetaMethods(struct SZrState *state, EZrValueType valueType);

#endif // ZR_VM_CORE_META_H
