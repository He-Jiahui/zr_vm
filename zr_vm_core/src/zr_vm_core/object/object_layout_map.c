#include "zr_vm_core/object_layout_map.h"

EZrObjectLayoutMapStatus ZrCore_Object_ResolveLayoutMember(
        const SZrObjectLayoutMap *map, TZrUInt32 descriptorIndex,
        TZrUInt64 shapeGeneration, SZrObjectMemberLocation *location) {
    TZrUInt32 i;
    if (map == ZR_NULL || location == ZR_NULL) return ZR_OBJECT_LAYOUT_MAP_INVALID_ARGUMENT;
    if (shapeGeneration != map->shapeGeneration) return ZR_OBJECT_LAYOUT_MAP_STALE_SHAPE;
    if (map->entries == ZR_NULL) return ZR_OBJECT_LAYOUT_MAP_MEMBER_NOT_FOUND;
    for (i = 0u; i < map->entryCount; ++i) {
        const SZrObjectLayoutMapEntry *entry = &map->entries[i];
        if (entry->descriptorIndex == descriptorIndex) {
            location->descriptorIndex = entry->descriptorIndex;
            location->logicalOffset = entry->logicalOffset;
            location->physicalOffset = entry->physicalOffset;
            location->byteSize = entry->byteSize;
            location->byteAlign = entry->byteAlign;
            location->flags = entry->flags;
            return ZR_OBJECT_LAYOUT_MAP_OK;
        }
    }
    return ZR_OBJECT_LAYOUT_MAP_MEMBER_NOT_FOUND;
}

TZrBool ZrCore_Object_LayoutMapCanTransform(const SZrObjectLayoutMap *map) {
    return (TZrBool)(map != ZR_NULL && map->privateClosedWorld &&
                     !map->reflectionVisible && !map->ffiVisible &&
                     !map->addressEscapes && !map->serializationObserved);
}

const TZrChar *ZrCore_Object_LayoutMap_StatusName(EZrObjectLayoutMapStatus status) {
    switch (status) {
        case ZR_OBJECT_LAYOUT_MAP_OK: return "ok";
        case ZR_OBJECT_LAYOUT_MAP_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_OBJECT_LAYOUT_MAP_MEMBER_NOT_FOUND: return "member-not-found";
        case ZR_OBJECT_LAYOUT_MAP_STALE_SHAPE: return "stale-shape";
        case ZR_OBJECT_LAYOUT_MAP_NOT_CLOSED_WORLD: return "not-closed-world";
        case ZR_OBJECT_LAYOUT_MAP_PUBLIC_LAYOUT: return "public-layout";
        default: return "unknown";
    }
}
