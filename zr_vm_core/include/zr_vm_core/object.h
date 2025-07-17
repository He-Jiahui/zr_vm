//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_OBJECT_H
#define ZR_VM_CORE_OBJECT_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/meta.h"
struct SZrState;

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

ZR_FORCE_INLINE SZrMeta *ZrPrototypeGetMetaRecursively(struct SZrState *state, SZrObjectPrototype *prototype,
                                                       EZrMetaType metaType) {
    ZR_UNUSED_PARAMETER(state);
    while (prototype != ZR_NULL) {
        SZrMeta *meta = prototype->metaTable.metas[metaType];
        if (meta != ZR_NULL) {
            return meta;
        }
        prototype = prototype->superPrototype;
    }
    return ZR_NULL;
}

ZR_FORCE_INLINE SZrMeta *ZrObjectGetMetaRecursively(struct SZrState *state, SZrObject *object, EZrMetaType metaType) {
    ZR_UNUSED_PARAMETER(state);
    SZrObjectPrototype *prototype = object->prototype;
    return ZrPrototypeGetMetaRecursively(state, prototype, metaType);
}

ZR_CORE_API void ZrObjectInit(struct SZrState *state, SZrObject *object);


#endif // ZR_VM_CORE_OBJECT_H
