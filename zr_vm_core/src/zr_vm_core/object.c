//
// Created by HeJiahui on 2025/6/22.
//
#include "zr_vm_core/object.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include <string.h>

static TZrBool ensure_managed_field_capacity(SZrState *state, SZrObjectPrototype *prototype, TZrUInt32 minimumCapacity) {
    SZrManagedFieldInfo *newFields;
    TZrUInt32 newCapacity;
    TZrSize newSize;

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    if (prototype->managedFieldCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = prototype->managedFieldCapacity > 0 ? prototype->managedFieldCapacity : 4;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2;
    }

    newSize = newCapacity * sizeof(SZrManagedFieldInfo);
    newFields = (SZrManagedFieldInfo *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                 newSize,
                                                                 ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (newFields == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newFields, 0, newSize);
    if (prototype->managedFields != ZR_NULL && prototype->managedFieldCount > 0) {
        memcpy(newFields,
               prototype->managedFields,
               prototype->managedFieldCount * sizeof(SZrManagedFieldInfo));
        ZrCore_Memory_RawFreeWithType(state->global,
                                prototype->managedFields,
                                prototype->managedFieldCapacity * sizeof(SZrManagedFieldInfo),
                                ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }

    prototype->managedFields = newFields;
    prototype->managedFieldCapacity = newCapacity;
    return ZR_TRUE;
}


SZrObject *ZrCore_Object_New(SZrState *state, SZrObjectPrototype *prototype) {
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrObject), ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = prototype;
    object->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
    ZrCore_HashSet_Construct(&object->nodeMap);
    return object;
}

SZrObject *ZrCore_Object_NewCustomized(struct SZrState *state, TZrSize size, EZrObjectInternalType internalType) {
    // 根据 internalType 选择正确的值类型
    EZrValueType valueType = ZR_VALUE_TYPE_OBJECT;
    if (internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        valueType = ZR_VALUE_TYPE_ARRAY;
    }
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, valueType, size, ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = ZR_NULL;
    object->internalType = internalType;
    ZrCore_HashSet_Construct(&object->nodeMap);
    return object;
}

void ZrCore_Object_Init(struct SZrState *state, SZrObject *object) {
    SZrGlobalState *global = state->global;
    // todo:
    ZrCore_HashSet_Init(state, &object->nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);

    // CONSTRUCT OBJECT FROM PROTOTYPE
    SZrMeta *constructor = ZrCore_Object_GetMetaRecursively(state->global, object, ZR_META_CONSTRUCTOR);
    // todo: call constructor
}

TZrBool ZrCore_Object_CompareWithAddress(struct SZrState *state, SZrObject *object1, SZrObject *object2) {
    ZR_UNUSED_PARAMETER(state);
    return object1 == object2;
}


void ZrCore_Object_SetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key, const SZrTypeValue *value) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrCore_Log_Error(state, "attempt to set value with null key");
        return;
    }
    SZrHashSet *nodeMap = &object->nodeMap;
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, nodeMap, key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, nodeMap, key);
    }
    ZrCore_Value_Copy(state, &pair->value, value);
}

const SZrTypeValue *ZrCore_Object_GetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrCore_Log_Error(state, "attempt to get value with null key");
        return ZR_NULL;
    }
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &object->nodeMap, key);
    if (pair != ZR_NULL) {
        return &pair->value;
    }

    if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        SZrObjectPrototype *prototype = (SZrObjectPrototype *)object;
        prototype = prototype->superPrototype;
        while (prototype != ZR_NULL) {
            pair = ZrCore_HashSet_Find(state, &prototype->super.nodeMap, key);
            if (pair != ZR_NULL) {
                return &pair->value;
            }
            prototype = prototype->superPrototype;
        }
        return ZR_NULL;
    }

    SZrObjectPrototype *prototype = object->prototype;
    while (prototype != ZR_NULL) {
        pair = ZrCore_HashSet_Find(state, &prototype->super.nodeMap, key);
        if (pair != ZR_NULL) {
            return &pair->value;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

// 创建基础 ObjectPrototype
SZrObjectPrototype *ZrCore_ObjectPrototype_New(SZrState *state, SZrString *name, EZrObjectPrototypeType type) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 ObjectPrototype 对象
    SZrObject *objectBase = ZrCore_Object_NewCustomized(state, sizeof(SZrObjectPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrObjectPrototype *prototype = (SZrObjectPrototype *)objectBase;
    
    // 初始化哈希集
    ZrCore_HashSet_Init(state, &prototype->super.nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->name = name;
    prototype->type = type;
    prototype->superPrototype = ZR_NULL;
    prototype->managedFields = ZR_NULL;
    prototype->managedFieldCount = 0;
    prototype->managedFieldCapacity = 0;
    
    // 初始化 metaTable
    ZrCore_MetaTable_Construct(&prototype->metaTable);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 创建 StructPrototype
SZrStructPrototype *ZrCore_StructPrototype_New(SZrState *state, SZrString *name) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 StructPrototype 对象
    SZrObject *objectBase = ZrCore_Object_NewCustomized(state, sizeof(SZrStructPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrStructPrototype *prototype = (SZrStructPrototype *)objectBase;
    
    // 初始化哈希集
    ZrCore_HashSet_Init(state, &prototype->super.super.nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->super.name = name;
    prototype->super.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype->super.superPrototype = ZR_NULL;
    prototype->super.managedFields = ZR_NULL;
    prototype->super.managedFieldCount = 0;
    prototype->super.managedFieldCapacity = 0;
    
    // 初始化 metaTable
    ZrCore_MetaTable_Construct(&prototype->super.metaTable);
    
    // 初始化 keyOffsetMap
    ZrCore_HashSet_Construct(&prototype->keyOffsetMap);
    ZrCore_HashSet_Init(state, &prototype->keyOffsetMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 设置继承关系
void ZrCore_ObjectPrototype_SetSuper(SZrState *state, SZrObjectPrototype *prototype, SZrObjectPrototype *superPrototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    prototype->superPrototype = superPrototype;
}

// 初始化元表
void ZrCore_ObjectPrototype_InitMetaTable(SZrState *state, SZrObjectPrototype *prototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    ZrCore_MetaTable_Construct(&prototype->metaTable);
}

// 向 StructPrototype 添加字段
void ZrCore_StructPrototype_AddField(SZrState *state, SZrStructPrototype *prototype, SZrString *fieldName, TZrSize offset) {
    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    
    // 创建键值（字段名）
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 创建值（偏移量）
    SZrTypeValue value;
    ZrCore_Value_InitAsUInt(state, &value, (TZrUInt64)offset);
    
    // 添加到 keyOffsetMap
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &prototype->keyOffsetMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &prototype->keyOffsetMap, &key);
    }
    ZrCore_Value_Copy(state, &pair->value, &value);
}

// 向 Prototype 添加元函数
void ZrCore_ObjectPrototype_AddMeta(SZrState *state, SZrObjectPrototype *prototype, EZrMetaType metaType, SZrFunction *function) {
    if (state == ZR_NULL || prototype == ZR_NULL || function == ZR_NULL) {
        return;
    }
    
    if (metaType >= ZR_META_ENUM_MAX) {
        return;
    }
    
    // 创建 Meta 对象
    SZrGlobalState *global = state->global;
    SZrMeta *meta = (SZrMeta *)ZrCore_Memory_RawMallocWithType(global, sizeof(SZrMeta), ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (meta == ZR_NULL) {
        return;
    }
    
    meta->metaType = metaType;
    meta->function = function;
    
    // 添加到 metaTable
    prototype->metaTable.metas[metaType] = meta;
}

void ZrCore_ObjectPrototype_AddManagedField(SZrState *state,
                                      SZrObjectPrototype *prototype,
                                      SZrString *fieldName,
                                      TZrUInt32 fieldOffset,
                                      TZrUInt32 fieldSize,
                                      TZrUInt32 ownershipQualifier,
                                      TZrBool callsClose,
                                      TZrBool callsDestructor,
                                      TZrUInt32 declarationOrder) {
    SZrManagedFieldInfo *fieldInfo;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    if (!ensure_managed_field_capacity(state, prototype, prototype->managedFieldCount + 1)) {
        return;
    }

    fieldInfo = &prototype->managedFields[prototype->managedFieldCount++];
    fieldInfo->name = fieldName;
    fieldInfo->fieldOffset = fieldOffset;
    fieldInfo->fieldSize = fieldSize;
    fieldInfo->ownershipQualifier = ownershipQualifier;
    fieldInfo->callsClose = callsClose;
    fieldInfo->callsDestructor = callsDestructor;
    fieldInfo->declarationOrder = declarationOrder;
}
