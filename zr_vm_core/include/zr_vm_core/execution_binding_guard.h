#ifndef ZR_VM_CORE_EXECUTION_BINDING_GUARD_H
#define ZR_VM_CORE_EXECUTION_BINDING_GUARD_H

#include "zr_vm_core/call_binding.h"

struct SZrFunctionCallSiteCacheEntry;
struct SZrObjectPrototype;

/** @brief 将绑定契约、代际和接收者见证的检查结果分开，供调用方选择后续路由。
 * @note SLOT_FALLBACK 仅准许重新按槽查找，仍计一次 miss；OK 不执行目标调用。 */
typedef enum EZrExecutionBindingGuardResult {
    ZR_EXECUTION_BINDING_GUARD_OK = 0,
    ZR_EXECUTION_BINDING_GUARD_STALE_GENERATION,
    ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH,
    ZR_EXECUTION_BINDING_GUARD_RECEIVER_TYPE_MISMATCH,
    ZR_EXECUTION_BINDING_GUARD_SHAPE_MISS,
    ZR_EXECUTION_BINDING_GUARD_SLOT_FALLBACK,
    ZR_EXECUTION_BINDING_GUARD_INVALID_SLOT,
    ZR_EXECUTION_BINDING_GUARD_TARGET_MISSING
} EZrExecutionBindingGuardResult;

/** @brief 一次同步检查所需的借用绑定、可选计数缓存和接收者见证。
 * @note 指针目标由调用方保活，检查期间不得并发修改；非零 expected 字段才参与比较。
 * receiverShapeId/Generation 为待比对见证，零表示不检查该项。缓存仅改命中/未命中计数。 */
typedef struct SZrExecutionBindingGuardInput {
    const SZrCallBinding *binding;
    struct SZrFunctionCallSiteCacheEntry *cacheEntry;
    TZrUInt64 activeGeneration;
    TZrUInt64 expectedModuleSignatureHash;
    TZrUInt64 expectedSignatureHash;
    TZrUInt32 expectedLayoutVersion;
    TZrUInt64 expectedLayoutHash;
    const struct SZrObjectPrototype *receiverPrototype;
    TZrUInt64 receiverShapeId;
    TZrUInt64 receiverShapeGeneration;
    TZrBool allowSlotFallback;
} SZrExecutionBindingGuardInput;

/** @brief 返回失败类别、底层契约状态与适用的数值见证；不持有对象。
 * @note expected/actual 随失败分支解释：所属绑定代际记录 binding/input，
 * 目标代际记录封存值/当前值；shape 失败记录当前 shapeId/输入 shapeId，
 * 因此仅 generation 不匹配时两者可能相等，layoutVersion 失败也仍记录 hash。 */
typedef struct SZrExecutionBindingGuardDiagnostic {
    EZrExecutionBindingGuardResult result;
    EZrCallBindingStatus bindingStatus;
    TZrUInt32 targetKind;
    TZrUInt32 dispatchSlot;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrExecutionBindingGuardDiagnostic;

/** @brief 在采用缓存目标前分类检查契约、所属代际、VM 目标代际及接收者见证。
 * @pre 非空指针所引用的绑定、目标函数、原型和缓存须在整个同步检查期间有效。
 * @note diagnostic 可空；函数不分配、不派发、不释放对象，也不自动失效目标。
 * 失败（含槽回退）增加饱和 miss，OK 增加饱和 hit；计数不是原子更新。
 * native/AOT 分支只检查入口指针是否齐备，不替代完整 CallBinding_Validate 的闭包代际检查。 */
ZR_CORE_API EZrExecutionBindingGuardResult ZrCore_Execution_CheckBindingGuard(
        const SZrExecutionBindingGuardInput *input,
        SZrExecutionBindingGuardDiagnostic *diagnostic);

/** @brief 清除 PIC 槽、运行时目标、代际及计数，保留重新链接所需的契约和坐标。
 * @note kind、instructionIndex、memberEntryIndex、deoptId、argumentCount 也保留；
 * 空指针无操作。只撤销借用见证，不释放被引用对象；调用方须独占缓存写入。 */
ZR_CORE_API void ZrCore_Execution_ResetBindingCache(
        struct SZrFunctionCallSiteCacheEntry *cacheEntry);

/** @brief 返回诊断展示使用的静态短名称；未知枚举值返回 unknown，调用方不得释放。 */
ZR_CORE_API const char *ZrCore_Execution_BindingGuardResultName(
        EZrExecutionBindingGuardResult result);

#endif
