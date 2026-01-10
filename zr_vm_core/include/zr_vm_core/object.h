//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_OBJECT_H
#define ZR_VM_CORE_OBJECT_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/string.h"
struct SZrState;
struct SZrGlobalState;
struct SZrObjectPrototype;

enum EZrObjectInternalType {
    ZR_OBJECT_INTERNAL_TYPE_OBJECT,
    ZR_OBJECT_INTERNAL_TYPE_STRUCT,
    ZR_OBJECT_INTERNAL_TYPE_ARRAY,
    ZR_OBJECT_INTERNAL_TYPE_MODULE,
    ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE,
    ZR_OBJECT_INTERNAL_TYPE_PROTOTYPE_INFO,  // 用于在常量池中存储prototype信息的对象
    ZR_OBJECT_INTERNAL_TYPE_CUSTOM_EXTENSION_START,
    // user can add custom extension object prototype here
};

typedef enum EZrObjectInternalType EZrObjectInternalType;

struct ZR_STRUCT_ALIGN SZrObject {
    SZrRawObject super;

    struct SZrObjectPrototype *prototype;

    SZrHashSet nodeMap;

    EZrObjectInternalType internalType;

    // SZrRawObject *gcList;
};

typedef struct SZrObject SZrObject;

struct ZR_STRUCT_ALIGN SZrObjectPrototype {
    SZrObject super;
    struct SZrString *name;
    EZrObjectPrototypeType type;
    struct SZrMetaTable metaTable;
    struct SZrObjectPrototype *superPrototype;
};

typedef struct SZrObjectPrototype SZrObjectPrototype;

struct ZR_STRUCT_ALIGN SZrStructPrototype {
    SZrObjectPrototype super;
    SZrHashSet keyOffsetMap;
};
typedef struct SZrStructPrototype SZrStructPrototype;


// pure object should be created by this function
ZR_CORE_API SZrObject *ZrObjectNew(struct SZrState *state, SZrObjectPrototype *prototype);

ZR_CORE_API SZrObject *ZrObjectNewCustomized(struct SZrState *state, TZrSize size, EZrObjectInternalType internalType);

ZR_FORCE_INLINE SZrMeta *ZrPrototypeGetMetaRecursively(struct SZrGlobalState *global, SZrObjectPrototype *prototype,
                                                       EZrMetaType metaType) {
    ZR_UNUSED_PARAMETER(global);
    while (prototype != ZR_NULL) {
        SZrMeta *meta = prototype->metaTable.metas[metaType];
        if (meta != ZR_NULL) {
            return meta;
        }
        prototype = prototype->superPrototype;
    }
    return ZR_NULL;
}

ZR_FORCE_INLINE SZrMeta *ZrObjectGetMetaRecursively(struct SZrGlobalState *global, SZrObject *object,
                                                    EZrMetaType metaType) {
    SZrObjectPrototype *prototype = object->prototype;
    return ZrPrototypeGetMetaRecursively(global, prototype, metaType);
}

ZR_CORE_API void ZrObjectInit(struct SZrState *state, SZrObject *object);

// this function do not call Meta function to compare, only compare address, we use this for hash set to make key
// different
ZR_CORE_API TBool ZrObjectCompareWithAddress(struct SZrState *state, SZrObject *object1, SZrObject *object2);

ZR_CORE_API void ZrObjectSetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key,
                                  const SZrTypeValue *value);

ZR_CORE_API const SZrTypeValue *ZrObjectGetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key);

// Prototype 创建和管理函数
ZR_CORE_API SZrObjectPrototype *ZrObjectPrototypeNew(struct SZrState *state, SZrString *name, EZrObjectPrototypeType type);

ZR_CORE_API SZrStructPrototype *ZrStructPrototypeNew(struct SZrState *state, SZrString *name);

ZR_CORE_API void ZrObjectPrototypeSetSuper(struct SZrState *state, SZrObjectPrototype *prototype, SZrObjectPrototype *superPrototype);

ZR_CORE_API void ZrObjectPrototypeInitMetaTable(struct SZrState *state, SZrObjectPrototype *prototype);

ZR_CORE_API void ZrStructPrototypeAddField(struct SZrState *state, SZrStructPrototype *prototype, SZrString *fieldName, TZrSize offset);

ZR_CORE_API void ZrObjectPrototypeAddMeta(struct SZrState *state, SZrObjectPrototype *prototype, EZrMetaType metaType, struct SZrFunction *function);

#endif // ZR_VM_CORE_OBJECT_H
