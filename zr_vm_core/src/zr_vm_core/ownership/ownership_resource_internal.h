#ifndef ZR_VM_CORE_OWNERSHIP_RESOURCE_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_RESOURCE_INTERNAL_H

#include "zr_vm_core/ownership.h"

/**
 * @brief ownership 分流层与 resource 专用实现之间的内部契约。
 * @note direct UNIQUE/LOANED 值不使用共享控制块；domain root 与资源生命周期共同守护对象。
 */

/** @brief 判断对象是否声明 resource 协议，供普通对象与直接 owner 路径分流。 */
TZrBool ZrCore_OwnershipResource_IsObject(const SZrRawObject *object);
/** @brief 识别直接唯一 owner；move、loan、copy 与 transfer 只能按此值类别处理。 */
TZrBool ZrCore_OwnershipResource_IsDirectUniqueValue(const SZrTypeValue *value);
/** @brief 识别不新增根的直接借用，归还前须保持原责任链存活。 */
TZrBool ZrCore_OwnershipResource_IsDirectLoanedValue(const SZrTypeValue *value);
/**
 * @brief 为待交付的 resource 建立唯一直接 owner，并登记 GC domain root。
 * @return 输入无效或登记失败时为 false，且不移交 destination 的所有权。
 * TODO: 核实所有调用都只初始化新建/尚未交付资源；实现未拒绝再次将已消费对象置为 ALIVE。
 */
TZrBool ZrCore_OwnershipResource_InitUnique(struct SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrRawObject *object);
/** @brief 将直接唯一 owner 移至目标槽并消费源槽，保留原 domain root。 */
TZrBool ZrCore_OwnershipResource_MoveUnique(struct SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrTypeValue *source);
/** @brief 将唯一 owner 临时转为 direct loan；源槽清空且不新建根。 */
TZrBool ZrCore_OwnershipResource_LoanUnique(struct SZrState *state,
                                            SZrTypeValue *destination,
                                            SZrTypeValue *source);
/** @brief 借用责任结束后将 direct loan 归还为唯一 owner，并消费 loan 源槽。 */
TZrBool ZrCore_OwnershipResource_ReturnLoan(struct SZrState *state,
                                            SZrTypeValue *destination,
                                            SZrTypeValue *source);
/**
 * @brief 复制 direct resource 句柄供受约束的值槽镜像使用，不创建新的所有权根。
 * TODO: 通用 Value_Copy 上游仍需核实不会因此留下两个可独立释放的 UNIQUE 槽。
 */
void ZrCore_OwnershipResource_CopyUnique(SZrTypeValue *destination,
                                         const SZrTypeValue *source);
/**
 * @brief 完成一次性资源析构并保留失败状态，供最终 owner 先整理控制块再传播异常。
 * @return 回调失败时仍尽量清理字段；root frame 建立失败则不开始 drop；空或非 resource 无操作成功。
 */
EZrThreadStatus ZrCore_OwnershipResource_DropProtected(
        struct SZrState *state, SZrRawObject *object);
/** @brief 无后续控制块清理责任时执行资源 drop，并按 VM 约定重新抛出失败。 */
void ZrCore_OwnershipResource_Drop(struct SZrState *state, SZrRawObject *object);

#endif
