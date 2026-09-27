#ifndef ZR_VM_CORE_PROPERTY_REFERENCE_H
#define ZR_VM_CORE_PROPERTY_REFERENCE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrFunction;
struct SZrTypeValueOnStack;

/** @brief 为函数成员表中的字段创建可传递的属性引用。
 * @pre function 的成员表索引有效；frameBase 为当前活动帧，base 和 result 是独立的有效值槽。
 * @note 成员描述符与对象基值保存在 GC 对象中；内联帧字段引用仅在来源帧活动期间有效。
 * @return 创建及字段写入成功时返回真；失败时 result 可能已包含部分构造的引用对象。
 */
ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateMember(
        struct SZrState *state,
        struct SZrFunction *function,
        struct SZrTypeValueOnStack *frameBase,
        TZrUInt32 receiverSlot,
        const SZrTypeValue *base,
        TZrUInt32 memberEntryIndex,
        SZrTypeValue *result);

/** @brief 按名字定位原型链上的成员描述符并创建属性引用。
 * @pre base 和 result 是独立的有效值槽，name 在调用期间有效。
 * @return 找到描述符且创建成功时返回真；失败时 result 不保证保持原值。
 */
ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateMemberByName(
        struct SZrState *state,
        const SZrTypeValue *base,
        const TZrChar *name,
        SZrTypeValue *result);

/** @brief 保存基值与索引键，供随后 Load/Store 按索引重新访问。
 * @pre result 与 base、key 分离；base 与 key 可同槽，二者按值写入引用对象。
 * @return 创建成功时返回真；失败时 result 可能已包含部分构造的引用对象。
 */
ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateIndex(
        struct SZrState *state,
        const SZrTypeValue *base,
        const SZrTypeValue *key,
        SZrTypeValue *result);

/** @brief 将活动帧的局部槽编码成可传递的属性引用。
 * @pre frameBase 指向当前活动帧，sourceSlot 属于该函数的有效槽，result 为独立值槽。
 * @note 引用保存相对栈基址的锚点，不延长帧生命周期；帧退出后不得加载或写回。
 * @return 创建成功时返回真；失败时 result 可能已包含部分构造的引用对象。
 */
ZR_CORE_API TZrBool ZrCore_PropertyReference_CreateFrameSlot(
        struct SZrState *state,
        struct SZrFunction *function,
        struct SZrTypeValueOnStack *frameBase,
        TZrUInt32 sourceSlot,
        SZrTypeValue *result);

/** @brief 识别引用对象的基础外形与已知种类。
 * @note 不验证种类专属字段、成员描述符或来源帧仍有效；Load/Store 会再解析。
 */
ZR_CORE_API TZrBool ZrCore_PropertyReference_IsValid(
        struct SZrState *state,
        SZrTypeValue *reference);

/** @brief 从成员、索引或仍活动的帧槽引用读取当前值。
 * @pre 帧引用的来源帧仍活动；result 为独立的有效值槽。
 * @return 定位与读取成功时返回真，引用缺字段或目标不可访问时返回假。
 */
ZR_CORE_API TZrBool ZrCore_PropertyReference_Load(
        struct SZrState *state,
        SZrTypeValue *reference,
        SZrTypeValue *result);

/** @brief 向成员、索引或仍活动的帧槽引用写回值。
 * @pre 帧引用的来源帧仍活动；value 在调用期间有效。
 * @note 帧槽写入还同步需关闭值的物理槽镜像；只读成员描述符拒绝写入。
 * @return 目标定位且写入接口报告成功时返回真。
 */
ZR_CORE_API TZrBool ZrCore_PropertyReference_Store(
        struct SZrState *state,
        SZrTypeValue *reference,
        const SZrTypeValue *value);

#endif /* ZR_VM_CORE_PROPERTY_REFERENCE_H */
