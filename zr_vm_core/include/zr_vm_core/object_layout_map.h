#ifndef ZR_VM_CORE_OBJECT_LAYOUT_MAP_H
#define ZR_VM_CORE_OBJECT_LAYOUT_MAP_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/object.h"

/** @brief 成员定位结果；未找到和过期 shape 分别供调用方回退而非复用旧偏移。 */
typedef enum EZrObjectLayoutMapStatus {
    ZR_OBJECT_LAYOUT_MAP_OK = 0,
    ZR_OBJECT_LAYOUT_MAP_INVALID_ARGUMENT,
    ZR_OBJECT_LAYOUT_MAP_MEMBER_NOT_FOUND,
    ZR_OBJECT_LAYOUT_MAP_STALE_SHAPE,
    ZR_OBJECT_LAYOUT_MAP_NOT_CLOSED_WORLD,
    ZR_OBJECT_LAYOUT_MAP_PUBLIC_LAYOUT
} EZrObjectLayoutMapStatus;

/** @brief 成功解析时复制出的成员位置快照；逻辑偏移保留公开顺序，物理偏移供内部布局访问。
 * @note 字段是值拷贝，不持有 map 或对象引用；失败时不会写入此结构。 */
typedef struct SZrObjectMemberLocation {
    TZrUInt32 descriptorIndex;
    TZrUInt32 logicalOffset;
    TZrUInt32 physicalOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 flags;
} SZrObjectMemberLocation;

/** @brief 用描述符索引连接逻辑成员与内部物理位置的只读条目。
 * @pre 同一 map 的 descriptorIndex 应唯一，偏移、大小及对齐须由条目生产方验证。 */
typedef struct SZrObjectLayoutMapEntry {
    TZrUInt32 descriptorIndex;
    TZrUInt32 logicalOffset;
    TZrUInt32 physicalOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 flags;
} SZrObjectLayoutMapEntry;

/** @brief 对象布局映射及可见性快照。
 * @note entries 借用调用方存储，不管理分配或 GC 根；shape 与布局变更后调用方须更新映射。
 * shapeId、layoutGeneration 和两个布局哈希目前不由本模块的查询函数校验。 */
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

/** @brief 在已选定的布局映射中按描述符索引解析逻辑/物理位置。
 * @pre 非空 entries 至少含 entryCount 个有效条目，且调用方已将 map 绑定到目标 shape 身份。
 * @return map 或 location 为空时为 INVALID_ARGUMENT；代际不等时为 STALE_SHAPE；无条目或未命中时为 MEMBER_NOT_FOUND。
 * 仅在 OK 时写入 location，其他结果保留其原值。 */
ZR_CORE_API EZrObjectLayoutMapStatus ZrCore_Object_ResolveLayoutMember(
        const SZrObjectLayoutMap *map, TZrUInt32 descriptorIndex,
        TZrUInt64 shapeGeneration, SZrObjectMemberLocation *location);
/** @brief 仅按闭世界、反射、FFI、地址外逸和序列化可见性判断能否尝试内部布局变换。
 * @note 此谓词不检查条目有效性、shape 身份或布局哈希；空 map 返回假。
 * TODO: 接入生产优化前，调用方还须结合上游的 publicLayout 和聚合标志完成变换许可证明。 */
ZR_CORE_API TZrBool ZrCore_Object_LayoutMapCanTransform(const SZrObjectLayoutMap *map);
/** @brief 将布局解析状态映射为静态诊断名称；未知枚举值返回 "unknown"，结果无需释放。 */
ZR_CORE_API const TZrChar *ZrCore_Object_LayoutMap_StatusName(EZrObjectLayoutMapStatus status);

#endif
