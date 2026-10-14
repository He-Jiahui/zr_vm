//
// zr.system.gc native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_GC_H
#define ZR_VM_LIB_SYSTEM_GC_H

#include "zr_vm_lib_system/conf.h"

/**
 * @file gc.h
 * ## Shared constraints
 * 这些 callback 操作或观察调用 State 所属 global 的共享 collector。宿主必须协调同一 global
 * 上的 GC 控制调用、显式 collection 与其他 collector 更新，避免未串行化的并发修改；本 API
 * 不在不同操作之间提供事务或通用锁。单个入口的参数、结果和执行边界见下方声明。
 */

/**
 * ## Summary
 * 将调用者所属 global 的 collector 切换到 generational 模式并清除停止标志；
 * 该控制属于共享 GC，不属于某个 State 或脚本对象。
 *
 * ## Parameters
 * @param[in] context 由 native dispatcher 为当前调用提供；其 State、global 与 collector
 *                    必须在回调期间有效。
 * @param[out] result 可写的 VM 返回值槽；成功时写入 null，槽由调用方持有。
 *
 * ## Returns
 * @return 成功修改 collector 并写入 null 时返回真；缺少 context、result 或 collector
 *         时返回假。经 descriptor 调用的零参数元数由 dispatcher 在进入回调前检查。
 *
 * ## Constraints and lifecycle
 * collector 由 global 持有，其他 State 也观察到本次模式/停止标志变化；本函数不启动
 * 一轮回收，也不提供与其他 mutator 控制操作的事务或线程同步。
 *
 * ## References
 * `ZrSystem_GcRegistry_GetModule` 注册此回调；实现写入
 * `SZrGarbageCollector.gcMode` 与 `stopGcFlag`。
 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Enable(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 在调用者所属 global 的 collector 上设置 stop flag，供部分自动回收循环作为停止请求。
 *
 * ## Parameters
 * @param[in] context 由 native dispatcher 提供；调用期间 State、global 和 collector 有效。
 * @param[out] result 可写的 VM 返回值槽；成功时写入 null，槽由调用方持有。
 *
 * ## Returns
 * @return 成功设置 stop flag 并写入 null 时返回真；无效 context、result 或 collector 时返回假。
 *
 * ## Constraints and lifecycle
 * 本函数只设置 `stopGcFlag`，不恢复此前的 `gcMode`，也不回滚已经进入的 GC step；Core
 * 只有部分增量循环检查此标志，因此不能据此保证所有自动 generational 路径都停止。
 * TODO: 核对 `gc.c` 的 generational step、`CheckGc` 与普通 minor 路径如何共同解释此 flag。
 * 显式 `collect` 仍可推进 GC。操作影响同一 global 下的所有 State。
 *
 * ## References
 * 配套启用接口为 `ZrSystem_Gc_Enable`；调度行为由
 * `ZrCore_GarbageCollector_GcStep` 与 `ZrCore_GarbageCollector_GcFull` 执行。
 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Disable(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将可选 kind 映射为 Core GC 请求；省略 kind 时执行同步 full 路径。
 *
 * ## Parameters
 * @param[in] context 由 dispatcher 提供，包含零个或一个参数；kind 接受 `minor`、`major`
 *                    或 `full` 字符串。零参数等同于 `full`。
 * @param[out] result 可写的 VM 返回值槽；回调结束时写入 null，槽由调用方持有。
 *
 * ## Returns
 * @return Core 调度调用完成后返回真并交付 null；minor/major 只安排请求并推进一次
 *         `GcStep`，full 的 stop-the-world 超时可使 Core 提前返回而本回调仍返回真。
 *         未知 kind 会使 callback 返回假；通用 dispatcher 在无 pending exception 时将
 *         结果归一为 null。错误类型由 `ReadString` 抛出调用异常。
 *
 * ## Constraints and lifecycle
 * `minor` 和 `major` 的回调返回不表示完整周期已完成；full 可能等待其他 mutator，且
 * Core 的 void 接口不向本回调报告 stop-the-world 超时。kind 按字节精确匹配是公开
 * 约束；实现当前以 `strcmp` 检查，合法 VM 字符串中的内嵌 NUL 会使尾随字节不可见，
 * 详见实现中的 `BUG:` 记录。
 *
 * ## References
 * 文本映射在 `system_gc_read_collection_kind`；调度与执行在
 * `system_gc_run_collection`、`ZrCore_GarbageCollector_ScheduleCollection`、
 * `ZrCore_GarbageCollector_GcStep` 和 `ZrCore_GarbageCollector_GcFull`。
 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Collect(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 更新调用者所属 global 的托管堆阈值；Core 在后续安全点据此安排回收。
 *
 * ## Parameters
 * @param[in] context dispatcher 提供的单参数调用；索引 0 的参数应是非负且可由 `int64` 表示的
 *                    整数 bytes 值，0 清除阈值。float/double 经共享 ReadInt 窄化，不属于本
 *                    API 保证的输入；NaN、无穷和 int64 范围外浮点值触发共享入口已记录的 UB。
 * @param[out] result 可写的 VM 返回值槽；接受参数时写入 null，槽由调用方持有。
 *
 * ## Returns
 * @return `ReadInt` 转换结果非负且 Core 配置更新时返回真；转换结果为负时 callback 返回假，
 *         dispatcher 在无 pending exception 时归一为 null；不支持的类型由 `ReadInt` 抛异常。
 *
 * ## Constraints and lifecycle
 * 阈值是 global 共享配置，不会在本回调内立即执行回收。descriptor 的 `int` 是工具可见
 * 的类型提示；共享 `ReadInt` 会接受 float/double 并向零窄化，因此 `-0.5` 会转为 0 而不
 * 被拒绝，但这不是本 API 的小数契约。该共享入口对 NaN、无穷和 int64 范围外浮点值存在
 * 已记录 UB；UINT64 超过 INT64_MAX 再转 signed 的结果由实现定义。传给 Core 前还会转换为
 * `TZrMemoryOffset`；`TODO:` 核对 32 位目标可表示范围与超界策略。
 *
 * ## References
 * 参数在 `system_gc_read_non_negative_int_argument` 读取，再交给
 * `ZrCore_GarbageCollector_SetHeapLimitBytes`；descriptor 位于 `gc_registry.c`。
 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetHeapLimit(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将一个非负配置值同时写入 global collector 的 pause 与 remark budget。
 *
 * ## Parameters
 * @param[in] context dispatcher 提供的单参数调用；`context[0]` 应是非负且可由 `int64` 表示的
 *                    整数 microseconds；浮点窄化及共享 ReadInt 的边界见约束。
 * @param[out] result 可写的 VM 返回值槽；接受参数时写入 null，槽由调用方持有。
 *
 * ## Returns
 * @return `ReadInt` 转换结果非负且更新 Core 配置时返回真；转换结果为负时返回假并由 dispatcher
 *         在无 pending exception 时归一为 null；不支持的类型由 `ReadInt` 抛出异常。
 *
 * ## Constraints and lifecycle
 * 该值分别作为两个 Core 配置写入；0 仍表示配置为零，而非 generational 的增量 `GcStep`
 * 循环会把零折算为至少一个工作单位；generational 模式不保证所有阶段都受此值约束。它不是硬墙钟上限：初始 major pause 是否受限仍待核，remark 的同步尾段
 * 明确不由此值限制。UINT64 超过 INT64_MAX 经共享 ReadInt 转 signed 的结果由实现定义；
 * Core 还将 pause 值窄化为 `TZrSize`，32 位边界待核。该配置作用于
 * global 的共享 collector。
 *
 * ## References
 * 输入经 `system_gc_read_non_negative_int_argument` 读取，再传给
 * `ZrCore_GarbageCollector_SetPauseBudgetUs`；pause、remark 的实际消费见 Core GC step。
 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetBudget(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将 Core GC 统计复制到 VM 管理的 `SystemGcStats` 对象，避免向脚本暴露 collector 指针。
 *
 * ## Parameters
 * @param[in] context dispatcher 提供的零参数调用；其 State/global/collector 在回调期间有效。
 * @param[out] result 可写的 VM 返回值槽；成功时持有返回对象的 VM 值，调用方不释放该对象。
 *
 * ## Returns
 * @return 成功构造统计对象并将其放入 result 时返回真；无 collector 或对象构造失败时
 *         返回假，Core/native 分配也可能通过 VM 异常退出。
 *
 * ## Constraints and lifecycle
 * Core snapshot 在创建 VM 对象前读取；随后创建对象、键和字段可能分配并触发 GC，故本次
 * 查询本身会改变后续可观测状态。`enabled` 再单独投影 `!stopGcFlag`，仅表示该 flag
 * 状态，不表示 `gcMode` 或 worker 正在运行，因此各字段不是同一原子时点。Core 明示 snapshot 无通用
 * 线程安全保证，调用方须与同 global 的 GC 更新协调；多 State 的实际同步入口仍待核。
 * 对象的初始 nodeMap 初始化失败后，字段 setter 会重试 void `Object_Init`；若重试仍失败，字段可被静默跳过。
 * 42 个字段经 void setter 投影；pin 的 ignored-registry 扩容或对象动态字段桶 Rehash 失败也
 * 可能静默跳过字段而仍返回对象，详见实现中的 `BUG:` 记录。当前 descriptor 的 `enabled`
 * 英文提示比实际 `!stopGcFlag` 投影更宽；本批保留既有元数据字串并在审查文档记录差异。
 *
 * ## References
 * 构造路径为 `system_gc_make_stats_object`、`ZrCore_GarbageCollector_GetStatsSnapshot`、
 * `g_gc_stats_fields`；Core 要求和仓内并发入口列于本批审查文档。
 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_GetStats(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_GC_H
