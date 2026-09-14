#ifndef ZR_VM_CORE_OBJECT_LAYOUT_MAP_H
#define ZR_VM_CORE_OBJECT_LAYOUT_MAP_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/object.h"

typedef enum EZrObjectLayoutMapStatus {
    ZR_OBJECT_LAYOUT_MAP_OK = 0,
    ZR_OBJECT_LAYOUT_MAP_INVALID_ARGUMENT,
    ZR_OBJECT_LAYOUT_MAP_MEMBER_NOT_FOUND,
    ZR_OBJECT_LAYOUT_MAP_STALE_SHAPE,
    ZR_OBJECT_LAYOUT_MAP_NOT_CLOSED_WORLD,
    ZR_OBJECT_LAYOUT_MAP_PUBLIC_LAYOUT
} EZrObjectLayoutMapStatus;

typedef struct SZrObjectMemberLocation {
    TZrUInt32 descriptorIndex;
    TZrUInt32 logicalOffset;
    TZrUInt32 physicalOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 flags;
} SZrObjectMemberLocation;

typedef struct SZrObjectLayoutMapEntry {
    TZrUInt32 descriptorIndex;
    TZrUInt32 logicalOffset;
    TZrUInt32 physicalOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 flags;
} SZrObjectLayoutMapEntry;

typedef struct SZrObjectLayoutMap {
    TZrUInt64 shapeId;
    TZrUInt64 shapeGeneration;
    TZrUInt64 layoutGeneration;
    TZrUInt64 logicalLayoutHash;
    TZrUInt64 physicalLayoutHash;
    const SZrObjectLayoutMapEntry *entries;
    TZrUInt32 entryCount;
    TZrBool privateClosedWorld;
    TZrBool reflectionVisible;
    TZrBool ffiVisible;
    TZrBool addressEscapes;
    TZrBool serializationObserved;
} SZrObjectLayoutMap;

ZR_CORE_API EZrObjectLayoutMapStatus ZrCore_Object_ResolveLayoutMember(
        const SZrObjectLayoutMap *map, TZrUInt32 descriptorIndex,
        TZrUInt64 shapeGeneration, SZrObjectMemberLocation *location);
ZR_CORE_API TZrBool ZrCore_Object_LayoutMapCanTransform(const SZrObjectLayoutMap *map);
ZR_CORE_API const TZrChar *ZrCore_Object_LayoutMap_StatusName(EZrObjectLayoutMapStatus status);

#endif
