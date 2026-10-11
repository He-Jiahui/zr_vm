/**
 * @file
 * ## Summary
 * 公开 zr.thread 的宿主调度策略、隔离复制预算、停止请求和最近成功 worker 域诊断。
 * 配置写入同一 GlobalState 的隐藏根字段，由之后的 ThreadScheduler 构造路径快照使用。
 *
 * ## Constraints and lifecycle
 * 宿主在目标 global 的主 VM 线程协调调用；现有实例不追溯后续策略/预算更改。
 * 停止标记成功写入时才拒收新隔离任务；停止接口故障化已登记 runtime 的待启动队列，不等待 worker 退出。
 * 对在途隔离 worker，宿主须继续泵送主域完成消息，并保持 global/mainThreadState/分配器到 teardown 结束。
 * 返回的诊断身份是普通值，不拥有域或证明域仍活动。
 *
 * ## References
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h:8、zr_vm_lib_thread/src/zr_vm_lib_thread/module.c:3、tests/thread/test_thread_runtime.c:13 和 tests/parser/test_artifact_schema_source_roundtrip.c:13 直接包含本头；
 * zr_vm_cli/src/zr_vm_cli/project/project.c:371 通过门面 Register 接入 provider，tests/thread/test_thread_runtime.c:711 和 tests/thread/test_thread_runtime.c:944 选择策略及停止请求。
 */

#ifndef ZR_VM_THREAD_RUNTIME_H
#define ZR_VM_THREAD_RUNTIME_H

#include "zr_vm_lib_thread/conf.h"
#include "zr_vm_core/gc_domain.h"

/**
 * ## Summary
 * 以不引入 GlobalState 完整布局的方式公开宿主策略、预算、停止和诊断接口的借用关系。
 * 这是现有核心类型的前置声明，不另建 VM 域或承诺稳定的对象布局。
 *
 * ## Invariants and lifecycle
 * 宿主保持实际 GlobalState/mainThreadState/根对象有效；四个带 global 的接口均不接管或释放它。
 * 停止请求不替代在途 worker 的完成和 teardown；异步消费者结束前不能销毁其上下文。
 *
 * ## References
 * zr_vm_core/include/zr_vm_core/global.h 定义完整类型；zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:302 和 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:393 检查主线程前提。
 */
struct SZrGlobalState;

/**
 * ## Summary
 * 由宿主选择新 ThreadScheduler 实例的 worker 执行域；没有对应的脚本配置切换入口。
 * 构造器快照该值，提交时使用实例自己的后端字段，使宿主后续配置不改变已有 scheduler。
 *
 * ## Value model
 * ATTACHED_DOMAIN 在 caller GcDomain 执行；ISOLATED_DOMAIN 创建独立 GlobalState/GcDomain，跨域复制受 quota 限制。
 * 两个普通枚举值不持有线程、域或资源；宿主 setter 只接受两个具名值。
 *
 * ## Invariants and lifecycle
 * 配置阶段须早于目标 scheduler 构造；此枚举不是 worker 当前运行/退出状态，也不是域所有权。
 *
 * ## References
 * tests/thread/test_thread_runtime.c:711 选择 isolated；zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1215 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1227 快照，
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1259 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1266 选择提交后端。
 */
typedef enum EZrVmThreadSchedulerExecutionPolicy {
    /**
     * worker 附着 caller 的 GcDomain；新 scheduler 构造时固定该执行后端。
     */
    ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ATTACHED_DOMAIN = 0,
    /**
     * worker 建立独立 GlobalState/GcDomain，捕获与结果须遵守跨域复制预算。
     */
    ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN = 1
} EZrVmThreadSchedulerExecutionPolicy;

/**
 * ## Summary
 * 借出 zr.thread 的静态模块描述符，供宿主登记、插件符号和元数据核对使用同一契约。
 * 查询无需 VM，不创建线程、worker 或 scheduler，也不执行 Register 的 registry 附着动作。
 *
 * ## Returns
 * @return 非 NULL const 静态描述符及其嵌套表/字符串/回调的借用指针；不得修改或释放。
 *
 * ## Constraints and lifecycle
 * provider 镜像必须保持加载到最后一次描述符与回调消费结束；结果不取得 VM/域所有权。
 *
 * ## References
 * zr_vm_lib_thread/src/zr_vm_lib_thread/module.c:6 门面转发、zr_vm_lib_thread/src/zr_vm_lib_thread/module.c:18 登记、zr_vm_lib_thread/src/zr_vm_lib_thread/module.c:22 插件入口均直接调用；
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1591 返回静态表。
 */
ZR_VM_THREAD_API const ZrLibModuleDescriptor *ZrVmThread_Runtime_GetModuleDescriptor(void);

/**
 * ## Summary
 * 设置同一 global 之后构造的 ThreadScheduler 的 attached/isolated worker 后端。
 * 已有 scheduler 持有构造时策略，后续设置不追溯到它们。
 *
 * ## Parameters
 * @param[in,out] global 宿主拥有、mainThreadState 和 zr 根对象已建立的有效全局状态；仅借用。
 * @param[in] policy 仅两个具名执行策略；其它数值不接受。
 *
 * ## Returns
 * @return global/主线程/根对象无效或 policy 非法时为 ZR_FALSE；否则写入尝试后为 ZR_TRUE。
 *         true 不能证明配置已保存：当前底层 void setter 无成功反馈，调用者仍须处理下述失败边界。
 *
 * ## Constraints and lifecycle
 * 在 global 的主 VM 线程、目标 scheduler 构造前调用；同步由宿主协调，不假定可并发或重入。
 * BUG: 合法根对象尚未 ignore、ignore 表需扩容而分配失败，或字段键创建/pin/存储失败时，
 * void 写入可直接返回，上层仍 true；后续新实例使用旧配置或默认 attached。永久根不等于已经 ignore。
 * 这是完整静态可达失败链，尚未做故障注入；不改变已有 scheduler 或 global 所有权。
 *
 * ## References
 * tests/thread/test_thread_runtime.c:711 和 tests/thread/test_thread_runtime.c:740 直接调用；zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:312 写配置，zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1215 消费；
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385、zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1393 和 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1400 是写入失败入口；
 * zr_vm_core/src/zr_vm_core/gc/gc.c:696、zr_vm_core/src/zr_vm_core/gc/gc_object.c:746 和 zr_vm_core/src/zr_vm_core/object/object.c:1863 是合法扩容/存储分配失败路径。
 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
        struct SZrGlobalState *global,
        EZrVmThreadSchedulerExecutionPolicy policy);
/**
 * ## Summary
 * 为之后构造的 isolated scheduler 配置捕获与结果的跨域复制预算，使每次传输受对象、字节和深度上限约束。
 * 已有实例继续使用其 quota 快照；此接口不切换执行策略或创建 worker。
 *
 * ## Parameters
 * @param[in,out] global 宿主拥有且主线程/根对象已建立的有效 GlobalState，调用期间借用。
 * @param[in] maxObjects 复制允许的最大对象数，必须非零。
 * @param[in] maxBytes 复制允许的最大字节数，必须非零。
 * @param[in] maxDepth 结构化复制允许的最大递归深度，必须非零。
 *
 * ## Returns
 * @return global/主线程/根无效或任一预算为零时为 ZR_FALSE；否则三次写入尝试后为 ZR_TRUE。
 *         当前写入不是事务，true 不保证三项均已保存。
 *
 * ## Constraints and lifecycle
 * 在目标 global 的主 VM 线程、isolated scheduler 构造前设置；数值按值传递，不转移资源。
 * BUG: void 字段写入在对象/key pin或分配/存储失败时无法反馈，后续构造可读取旧值或新旧混合预算，接口仍 true。
 * 失败条件与 SetSchedulerExecutionPolicy 的静态写入链相同；尚未做故障注入。
 *
 * ## References
 * tests/thread/test_thread_runtime.c:890 是直接宿主设置；zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:338 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:340 是三次写入；
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:372 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:377 读取配置，zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1228 构造时快照；
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:823 在提交时借用实例预算；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1393 说明即使字段已存在仍会创建临时键。
 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_SetIsolatedTransferQuota(
        struct SZrGlobalState *global,
        TZrUInt32 maxObjects,
        TZrUInt64 maxBytes,
        TZrUInt32 maxDepth);
/**
 * ## Summary
 * 停止标记成功写入时，请求同一 global 的隔离 scheduler 拒绝后续新任务；本次还故障化已登记 runtime 的待启动队列。
 * 已启动隔离 worker 的结果/故障仍须回到 caller 域完成，停止请求不等待其退出。
 *
 * ## Parameters
 * @param[in,out] global 宿主拥有且 mainThreadState/根对象有效的全局状态；不接管或释放。
 *
 * ## Returns
 * @return global/主线程/根无效时为 ZR_FALSE；否则写停止标记并处理当前 pending 队列后为 ZR_TRUE。
 *         true 不表示 worker 已退出、scheduler 已释放或 global 已可销毁，也不保证底层标记写入成功。
 *
 * ## Constraints and lifecycle
 * 主 VM 线程协调调用；不 join、不等待已启动 worker。宿主继续泵送完成消息，worker teardown 结束前保持上下文有效。
 * BUG: 合法停止标记缺失或为 false，void setter 因 ignore扩容/key/pin/存储分配失败直接返回时，接口仍 true。
 * shutdown_all 仅故障化已有 pending 队列，不另设 runtime 拒收位；后续资源恢复后，公开 schedule 读旧 false 仍可接单/启动新域。
 * 该条件沿 SetSchedulerExecutionPolicy 的完整静态写入链可达；尚未做故障注入。
 * TODO: runtime-head 的 void 登记写失败可能使 pending 遍历遗漏实例；沿 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:786 和 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:676 核查登记失败后的队列处理。
 * TODO: 若宿主需要安全销毁屏障，须明确等待 worker teardown 的可调用契约；核查 worker 完成确认和 GlobalState_Free 路径。
 *
 * ## References
 * tests/thread/test_thread_runtime.c:107 和 tests/thread/test_thread_runtime.c:944 直接请求停止；zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:359 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:361 是标记/清队列/成功返回；
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:681 仅处理旧 pending，zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:829 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:839 检查新提交标记；
 * tests/thread/test_thread_runtime.c:928 至 tests/thread/test_thread_runtime.c:947 是停止后新建 scheduler 的宿主路径；构造器 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1215 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1240 后，经公开 schedule 的 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1259 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1264 选择后端；
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:902 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:912 排队/派发，zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:460 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:466 是 worker teardown 的实际上下文释放。
 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_ShutdownIsolatedSchedulers(
        struct SZrGlobalState *global);
/**
 * ## Summary
 * 按值读取最近一次成功回投并完成的 isolated worker 域身份，让宿主比较 caller/worker 所属域。
 * 它提供诊断记录，不取得域对象、GC 根或 worker 管理权限。
 *
 * ## Parameters
 * @param[in] global 借用宿主全局状态，mainThreadState 和 zr 根对象须有效。
 * @param[out] outDomain 宿主拥有的完整输出槽；非 NULL 时先清零，NULL 正常返回 ZR_FALSE。
 *
 * ## Returns
 * @return 根记录的 id/generation 均非零且 generation 可表示为 UInt32 时为 ZR_TRUE，交付普通值副本。
 *         global/主线程/根/输出无效或没有有效记录时为 ZR_FALSE，存在的输出保持零值。
 *
 * ## Constraints and lifecycle
 * 主 VM 线程在完成消息泵送后读取；身份的 id/generation 应作为一对消费，不证明域仍活动，不延长其寿命。
 * 记录仅在成功 Claim/Commit/Complete 后更新，fault 不更新；shutdown 和新 scheduler 不清记录，可能仍读到较早成功值。
 * TODO: 记录的两次 void 字段写入非事务；核查部分失败与未来 generation 变化的成对读取/清除语义。
 * 当前每个新 worker GcDomain 的 generation 为 1，因此不能据“双写”断言现行身份已经错配。
 *
 * ## References
 * tests/thread/test_thread_runtime.c:718 直接读取；zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:401 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:410 校验并复制；
 * zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:565 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:586 的成功回投调用记录，zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:287 至 zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:291 是两个字段写入；
 * zr_vm_core/src/zr_vm_core/gc/gc_domain.c:182 和 zr_vm_core/src/zr_vm_core/gc/gc_domain.c:183 建立新域身份。
 */
ZR_VM_THREAD_API TZrBool ZrVmThread_Runtime_GetLastSchedulerWorkerDomain(
        const struct SZrGlobalState *global,
        SZrGcDomainIdentity *outDomain);

#endif // ZR_VM_THREAD_RUNTIME_H
