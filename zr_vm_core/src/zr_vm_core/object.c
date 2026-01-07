//
// Created by HeJiahui on 2025/6/22.
//
#include "zr_vm_core/object.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"


SZrObject *ZrObjectNew(SZrState *state, SZrObjectPrototype *prototype) {
    SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrObject), ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = prototype;
    object->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
    ZrHashSetConstruct(&object->nodeMap);
    return object;
}

SZrObject *ZrObjectNewCustomized(struct SZrState *state, TZrSize size, EZrObjectInternalType internalType) {
    // 根据 internalType 选择正确的值类型
    EZrValueType valueType = ZR_VALUE_TYPE_OBJECT;
    if (internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        valueType = ZR_VALUE_TYPE_ARRAY;
    }
    SZrRawObject *rawObject = ZrRawObjectNew(state, valueType, size, ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = ZR_NULL;
    object->internalType = internalType;
    ZrHashSetConstruct(&object->nodeMap);
    return object;
}

void ZrObjectInit(struct SZrState *state, SZrObject *object) {
    SZrGlobalState *global = state->global;
    // todo:
    ZrHashSetInit(state, &object->nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);

    // CONSTRUCT OBJECT FROM PROTOTYPE
    SZrMeta *constructor = ZrObjectGetMetaRecursively(state->global, object, ZR_META_CONSTRUCTOR);
    // todo: call constructor
}

TBool ZrObjectCompareWithAddress(struct SZrState *state, SZrObject *object1, SZrObject *object2) {
    ZR_UNUSED_PARAMETER(state);
    return object1 == object2;
}


void ZrObjectSetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key, const SZrTypeValue *value) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrLogError(state, "attempt to set value with null key");
        return;
    }
    SZrHashSet *nodeMap = &object->nodeMap;
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, nodeMap, key);
    if (pair == ZR_NULL) {
        pair = ZrHashSetAdd(state, nodeMap, key);
    }
    ZrValueCopy(state, &pair->value, value);
}

const SZrTypeValue *ZrObjectGetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrLogError(state, "attempt to get value with null key");
        return ZR_NULL;
    }
    SZrHashSet *nodeMap = &object->nodeMap;
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, nodeMap, key);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    return &pair->value;
}

// 创建基础 ObjectPrototype
SZrObjectPrototype *ZrObjectPrototypeNew(SZrState *state, SZrString *name, EZrObjectPrototypeType type) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 ObjectPrototype 对象
    SZrObject *objectBase = ZrObjectNewCustomized(state, sizeof(SZrObjectPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrObjectPrototype *prototype = (SZrObjectPrototype *)objectBase;
    
    // 初始化哈希集
    ZrHashSetInit(state, &prototype->super.nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->name = name;
    prototype->type = type;
    prototype->superPrototype = ZR_NULL;
    
    // 初始化 metaTable
    ZrMetaTableConstruct(&prototype->metaTable);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 创建 StructPrototype
SZrStructPrototype *ZrStructPrototypeNew(SZrState *state, SZrString *name) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 StructPrototype 对象
    SZrObject *objectBase = ZrObjectNewCustomized(state, sizeof(SZrStructPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrStructPrototype *prototype = (SZrStructPrototype *)objectBase;
    
    // 初始化哈希集
    ZrHashSetInit(state, &prototype->super.super.nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->super.name = name;
    prototype->super.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype->super.superPrototype = ZR_NULL;
    
    // 初始化 metaTable
    ZrMetaTableConstruct(&prototype->super.metaTable);
    
    // 初始化 keyOffsetMap
    ZrHashSetConstruct(&prototype->keyOffsetMap);
    ZrHashSetInit(state, &prototype->keyOffsetMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 设置继承关系
void ZrObjectPrototypeSetSuper(SZrState *state, SZrObjectPrototype *prototype, SZrObjectPrototype *superPrototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    prototype->superPrototype = superPrototype;
}

// 初始化元表
void ZrObjectPrototypeInitMetaTable(SZrState *state, SZrObjectPrototype *prototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    ZrMetaTableConstruct(&prototype->metaTable);
}

// 向 StructPrototype 添加字段
void ZrStructPrototypeAddField(SZrState *state, SZrStructPrototype *prototype, SZrString *fieldName, TZrSize offset) {
    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    
    // 创建键值（字段名）
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 创建值（偏移量）
    SZrTypeValue value;
    ZrValueInitAsUInt(state, &value, (TUInt64)offset);
    
    // 添加到 keyOffsetMap
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, &prototype->keyOffsetMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrHashSetAdd(state, &prototype->keyOffsetMap, &key);
    }
    ZrValueCopy(state, &pair->value, &value);
}

// 向 Prototype 添加元函数
void ZrObjectPrototypeAddMeta(SZrState *state, SZrObjectPrototype *prototype, EZrMetaType metaType, SZrFunction *function) {
    if (state == ZR_NULL || prototype == ZR_NULL || function == ZR_NULL) {
        return;
    }
    
    if (metaType >= ZR_META_ENUM_MAX) {
        return;
    }
    
    // 创建 Meta 对象
    SZrGlobalState *global = state->global;
    SZrMeta *meta = (SZrMeta *)ZrMemoryRawMallocWithType(global, sizeof(SZrMeta), ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (meta == ZR_NULL) {
        return;
    }
    
    meta->metaType = metaType;
    meta->function = function;
    
    // 添加到 metaTable
    prototype->metaTable.metas[metaType] = meta;
}
