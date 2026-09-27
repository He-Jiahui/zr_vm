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

/** @brief 元方法槽记录；metaType 对应原型表下标，function 借用 GC 管理的可调用对象。 */
struct ZR_STRUCT_ALIGN SZrMeta {
    EZrMetaType metaType;
    struct SZrFunction *function;
};

typedef struct SZrMeta SZrMeta;

/** @brief 原型内嵌的元方法索引表；槽位持有原生分配的 SZrMeta。
 * 原型的 GC 标记追踪各槽位的 function，原型销毁时释放 SZrMeta。
 */
struct ZR_STRUCT_ALIGN SZrMetaTable {
    SZrMeta *metas[ZR_META_ENUM_MAX];
};

typedef struct SZrMetaTable SZrMetaTable;

/** @brief 初始化全局元方法名称字符串，并将其固定为永久 GC 对象。
 * @pre state、global 和字符串表已初始化。
 * BUG: 短串驻留表扩容的桶分配失败时，名称串创建可返回空指针；实现未检查便交给
 * 会解引用 object 的 ZrCore_RawObject_MarkAsPermanent，调用期间可因此崩溃。
 */
ZR_CORE_API void ZrCore_Meta_GlobalStaticsInit(struct SZrState *state);

/** @brief 清空元方法槽位，供新原型构造使用。
 * @pre table 非空且不持有待释放的 SZrMeta；本函数不会释放旧槽位。
 */
ZR_CORE_API void ZrCore_MetaTable_Construct(SZrMetaTable *table);

/** @brief 为基础值类型原型注册内建元方法；已占用的槽位保持不变。
 * @pre state 的 global 和目标基础原型已建立；不支持的值类型不注册方法。
 * @note 接口不返回分配状态，调用方无法由此判断是否全部注册成功。
 */
ZR_CORE_API void ZrCore_Meta_InitBuiltinTypeMetaMethods(struct SZrState *state, EZrValueType valueType);

#endif // ZR_VM_CORE_META_H
