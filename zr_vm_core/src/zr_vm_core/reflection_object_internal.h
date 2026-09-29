#ifndef ZR_VM_REFLECTION_OBJECT_INTERNAL_H
#define ZR_VM_REFLECTION_OBJECT_INTERNAL_H

#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

/** @brief 在 GC ignored registry 中临时保护一个裸对象。
 * @pre state、object 和 addedByCaller 的生命周期覆盖后续可能触发 GC 的操作。
 * @return 成功登记或对象已经登记时为 true；参数无效或 registry 分配失败时为 false。
 * @note addedByCaller 只记录本次是否新增登记；调用方必须把同一标志传给 ObjectUnpinRaw。
 * BUG: registry 的查询、插入和索引写入没有跨 mutator 的原子保护，多个 RUNNING mutator 同时 pin/unpin 同一 global 时保护可能丢失。
 */
TZrBool ZrCore_Reflection_ObjectPinRaw(
        SZrState *state,
        SZrRawObject *object,
        TZrBool *addedByCaller);
/** @brief 撤销本调用方通过 ObjectPinRaw 新增的裸对象保护。
 * @pre global、object 必须属于同一个 GC global；addedByCaller 必须来自对应 pin 调用。
 * @note 只撤销 addedByCaller 为 true 的登记，不负责释放对象或替代 VM stack root。
 * BUG: ignored registry 的 swap-delete 与并发撤销没有同步，调用方不能把本接口当作线程安全的 root 引用计数。
 */
void ZrCore_Reflection_ObjectUnpinRaw(
        SZrGlobalState *global,
        SZrRawObject *object,
        TZrBool addedByCaller);
/** @brief 对 GC 管理的 TypeValue 所指对象执行 ObjectPinRaw，非 GC 值无需登记。
 * @pre GC 值必须由 state 所属 global 管理；addedByCaller 用于配对释放。
 * @return 值为空、非 GC 值或成功登记时为 true。
 * TODO: state 为空时实现会先于 GC 值检查返回 true 且不 pin；当前调用者均先检查 state，需确认此空值语义是否应拒绝 GC 值。
 * BUG: GC 值会委托无锁 ObjectPinRaw，同一 global 的多个 RUNNING mutator 并发 pin 仍可能丢失 root 登记。
 */
TZrBool ZrCore_Reflection_ObjectPinValue(
        SZrState *state,
        const SZrTypeValue *value,
        TZrBool *addedByCaller);
/** @brief 撤销 ObjectPinValue 对 GC 值新增的 registry 登记。
 * @pre addedByCaller 必须来自同一值的 ObjectPinValue 调用。
 * @note 非 GC 值和未新增登记的值不会执行 registry 操作。
 * BUG: GC 值会委托无锁 ObjectUnpinRaw，并发撤销可能破坏 ignored registry 索引。
 */
void ZrCore_Reflection_ObjectUnpinValue(
        SZrGlobalState *global,
        const SZrTypeValue *value,
        TZrBool addedByCaller);
/** @brief 按 UTF-8/NUL 字符串创建 GC 管理的反射字段名或字段值。
 * @pre state 和 text 非空；返回对象在交给可能分配或 GC 的操作前必须建立 stack root 或临时 pin。
 * @return 新建或字符串表命中的字符串；参数无效或分配失败时为 NULL。
 */
SZrString *ZrCore_Reflection_ObjectMakeString(
        SZrState *state,
        const TZrChar *text);
/** @brief 创建字符串键并把 TypeValue 写入对象字段。
 * @pre object、fieldName 和 value 非空，且所有 GC 值属于 state 的 GC domain。
 * @return 参数/pin 前置步骤成功时为 true。
 * @note 本函数只在写入前临时 pin object、key 和 value；返回后 field value 由对象存储持有，调用方仍须保护其返回前的临时对象。
 * BUG: 底层 Object_SetValue 是 void 且可能因 domain 校验、rehash 或 pair 分配失败而不写入，本函数仍无条件返回 true；异常分配 longjmp 还会跳过下面的 unpin 清理。
 */
TZrBool ZrCore_Reflection_ObjectSetFieldValue(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        const SZrTypeValue *value);
/** @brief 创建字符串 TypeValue 并委托 ObjectSetFieldValue 写入字段。
 * @pre state、object、fieldName 和 value 非空；object 须在字符串分配期间保持 rooted/stable。
 * @return 字符串创建及字段写入 helper 报告成功时为 true。
 * @note 底层 setter 的失败/异常语义与 ObjectSetFieldValue 相同。
 * BUG: 本函数先创建 value 字符串，再进入会 pin object 的委托函数；未被调用方 root 的对象可能在分配重试 GC 中被回收。
 */
TZrBool ZrCore_Reflection_ObjectSetString(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        const TZrChar *value);
/** @brief 创建布尔 TypeValue 并委托 ObjectSetFieldValue 写入字段。
 * @pre object、fieldName 非空且 state 属于目标 GC domain。
 * @return 委托 helper 报告成功时为 true。
 */
TZrBool ZrCore_Reflection_ObjectSetBool(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrBool value);
/** @brief 以给定值类型把对象值或 null 写入字段。
 * @pre 非空 value 必须是 valueType 对应的 GC 对象，且和 object 属于同一 GC domain。
 * @return 委托 helper 报告成功时为 true。
 */
TZrBool ZrCore_Reflection_ObjectSetObject(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        SZrObject *value,
        EZrValueType valueType);
/** @brief 创建字符串键并查找对象字段，返回对象存储中的借用 TypeValue 指针。
 * @pre state、object 和 fieldName 非空；receiver 在创建 key 字符串期间须保持 rooted/stable。
 * @return 找到的字段值地址；不存在、键创建失败或底层查找失败时为 NULL。
 * @note 返回指针只在对象未改写/销毁且相关 GC root 仍有效期间可读，调用方不得把它当作独立 root。
 * TODO: 非短 fieldName 创建的字符串没有在 Object_GetValue 前 pin；当前调用者使用反射对象，若以后支持 raw array receiver，materialization 可分配并触发 GC，使临时 key 失效。
 */
const SZrTypeValue *ZrCore_Reflection_ObjectGetFieldValue(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName);

#endif
