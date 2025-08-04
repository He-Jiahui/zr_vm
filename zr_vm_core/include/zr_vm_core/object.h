//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_OBJECT_H
#define ZR_VM_CORE_OBJECT_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/meta.h"
struct SZrState;
struct SZrGlobalState;

struct ZR_STRUCT_ALIGN SZrObjectPrototype {
    SZrRawObject super;
    TZrString name;
    EZrObjectPrototypeType type;
    struct SZrMetaTable metaTable;
    struct SZrObjectPrototype *superPrototype;
};

typedef struct SZrObjectPrototype SZrObjectPrototype;

struct ZR_STRUCT_ALIGN SZrStructPrototype {
    SZrObjectPrototype super;
    SZrHashSet *keyOffsetMap;
};


struct ZR_STRUCT_ALIGN SZrObject {
    SZrRawObject super;

    SZrObjectPrototype *prototype;

    SZrHashSet nodeMap;

    // SZrRawObject *gcList;
};

typedef struct SZrObject SZrObject;


ZR_CORE_API SZrObject *ZrObjectNew(struct SZrState *state, SZrObjectPrototype *prototype);

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

ZR_CORE_API TBool ZrObjectCompare(struct SZrState *state, SZrObject *object1, SZrObject *object2);

ZR_CORE_API void ZrObjectSetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key,
                                  const SZrTypeValue *value);


#endif // ZR_VM_CORE_OBJECT_H
