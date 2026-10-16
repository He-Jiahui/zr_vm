#ifndef ZR_VM_CORE_ITERATOR_RUNTIME_H
#define ZR_VM_CORE_ITERATOR_RUNTIME_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrIteratorFrame;

/** @brief READY/YIELDED 属活动期；COMPLETED、FAULTED、CLOSED 标记终态。 */
typedef enum EZrIteratorFrameState {
    ZR_ITERATOR_FRAME_READY = 0,
    ZR_ITERATOR_FRAME_YIELDED,
    ZR_ITERATOR_FRAME_COMPLETED,
    ZR_ITERATOR_FRAME_FAULTED,
    ZR_ITERATOR_FRAME_CLOSED
} EZrIteratorFrameState;

/** @brief MoveNext 同步调用的生产回调，须 Publish 一个值或进入终态。
 * @note userData 由调用方持有，存活期覆盖帧的整个活动期。 */
typedef void (*TZrIteratorFrameProducer)(
        struct SZrState *state,
        struct SZrIteratorFrame *frame,
        TZrPtr userData);
/** @brief 终态时的一次性归还钩子；可能由 Complete、Fault 或 Close 触发。
 * BUG: 回调抛出时已标记终态且不重试，后续清理可能未完成。 */
typedef void (*TZrIteratorFrameCleanup)(
        struct SZrState *state,
        struct SZrIteratorFrame *frame,
        TZrPtr userData);

/** @brief 一次迭代的状态、当前值与 GC root；YIELDED 时的值在下次推进前有效。
 * @note isMoving 阻止 producer 重入；cleanupInvoked 保证正常终态只调用一次 cleanup。 */
typedef struct SZrIteratorFrame {
    EZrIteratorFrameState state;
    SZrTypeValue currentValue;
    SZrGcRootHandle currentRoot;
    TZrIteratorFrameProducer producer;
    TZrIteratorFrameCleanup cleanup;
    TZrPtr userData;
    TZrBool hasCurrentRoot;
    TZrBool isMoving;
    TZrBool cleanupInvoked;
    struct SZrIteratorFrame *nextFree;
} SZrIteratorFrame;

/** @brief 复用已进入终态并归还的帧；freeList 的原生内存属于创建时的 state/global。 */
typedef struct SZrIteratorFramePool {
    SZrIteratorFrame *freeList;
    TZrSize allocationCount;
    TZrSize reuseCount;
} SZrIteratorFramePool;

/** @brief 初始化全新或已归还的帧；不得覆盖仍在活动期的帧，否则会丢失其清理路径。 */
ZR_CORE_API void ZrCore_IteratorFrame_Init(
        struct SZrState *state,
        SZrIteratorFrame *frame,
        TZrIteratorFrameProducer producer,
        TZrPtr userData,
        TZrIteratorFrameCleanup cleanup);
/** @brief 推进 producer 一次；返回真表示现在可通过 Current 读取 yield。
 * BUG: producer 或析构路径异常跳出时，isMoving 不能复位，后续推进被拒。 */
ZR_CORE_API TZrBool ZrCore_IteratorFrame_MoveNext(
        struct SZrState *state,
        SZrIteratorFrame *frame);
/** @brief 取得当前 yield 的借用快照，不转移 frame 对值或 root 的所有权。
 * BUG: 普通 struct 克隆路径可能按输入 root 覆盖输出，返回原对象。 */
ZR_CORE_API TZrBool ZrCore_IteratorFrame_Current(
        struct SZrState *state,
        const SZrIteratorFrame *frame,
        SZrTypeValue *outValue);
/** @brief producer 只在 MoveNext 的 READY 阶段发布一个值；frame 接管当前副本。
 * BUG: 普通 struct 克隆时 root 绑定输入，复制失败仍可能报告 YIELDED。 */
ZR_CORE_API TZrBool ZrCore_IteratorFrame_Publish(
        struct SZrState *state,
        SZrIteratorFrame *frame,
        const SZrTypeValue *value);
/** @brief 正常耗尽时释放当前值并运行一次 cleanup。
 * BUG: 当前值析构抛出会跳过终态写入和 cleanup。 */
ZR_CORE_API void ZrCore_IteratorFrame_Complete(
        struct SZrState *state,
        SZrIteratorFrame *frame);
/** @brief producer 失败或未履约时进入故障终态并清理。
 * BUG: 当前值析构抛出会跳过终态写入和 cleanup。 */
ZR_CORE_API void ZrCore_IteratorFrame_Fault(
        struct SZrState *state,
        SZrIteratorFrame *frame);
/** @brief 消费方提前退出时释放当前值和 producer 私有状态。
 * BUG: 当前资源析构抛出会跳过终态写入及 cleanup。 */
ZR_CORE_API void ZrCore_IteratorFrame_Close(
        struct SZrState *state,
        SZrIteratorFrame *frame);
/** @brief 为调用方持有的池建立空闲表；重初始化前须释放旧空闲链并处理所有活动帧。 */
ZR_CORE_API void ZrCore_IteratorFramePool_Init(
        SZrIteratorFramePool *pool);
/** @brief 复用空闲帧或用 state/global 分配活动帧；返回者须终止并 Release。 */
ZR_CORE_API SZrIteratorFrame *ZrCore_IteratorFramePool_Acquire(
        struct SZrState *state,
        SZrIteratorFramePool *pool,
        TZrIteratorFrameProducer producer,
        TZrPtr userData,
        TZrIteratorFrameCleanup cleanup);
/** @brief 仅检查帧已终止；调用方须使用发放它的 pool 和 state，此入口不验证归属。 */
ZR_CORE_API TZrBool ZrCore_IteratorFramePool_Release(
        struct SZrState *state,
        SZrIteratorFramePool *pool,
        SZrIteratorFrame *frame);
/** @brief 释放池中已归还的帧；活动帧须先 Close/Release，且原分配 global 仍有效。 */
ZR_CORE_API void ZrCore_IteratorFramePool_Free(
        struct SZrState *state,
        SZrIteratorFramePool *pool);

#endif
