#ifndef ZR_TEST_SSA_RUNTIME_OBJECTS_FAULTS_H
#define ZR_TEST_SSA_RUNTIME_OBJECTS_FAULTS_H
#include <stddef.h>
#include "exec_ir/exec_ir_runtime.h"
ZR_CORE_API void ssa_runtime_objects_faults(TZrUInt32 failObject, TZrBool throwOom,
                                TZrBool collectEachObject, TZrUInt32 failReserve);
ZR_CORE_API TZrUInt32 ssa_runtime_objects_allocation_count(void);
ZR_CORE_API TZrUInt32 ssa_runtime_objects_invalid_mutation_end_count(void);
ZR_CORE_API void ssa_runtime_objects_fail_native_allocation(size_t ordinal);
ZR_CORE_API TZrBool ssa_runtime_objects_native_allocation_failed(void);
ZR_CORE_API void ssa_runtime_objects_fail_remembered_reservation(void);
ZR_CORE_API void ssa_runtime_objects_fail_pause(void);
ZR_CORE_API void ssa_runtime_objects_set_barrier_hook(void (*hook)(SZrState *, void *), void *context);
ZR_CORE_API TZrBool ssa_runtime_objects_materialize(const SZrExecIrObjectMaterializationRequest *request,
                                       SZrExecIrDiagnostic *diagnostic);
#endif

/**
 * @fn void ssa_runtime_objects_faults(TZrUInt32 failObject, TZrBool throwOom, TZrBool collectEachObject, TZrUInt32 failReserve)
 *
 * ## Summary
 * 在一次串行测试场景开始前复位本故障编译实例的计数与开关。
 *
 * ## Parameters
 * @param[in] failObject 要使Object_New尝试失败的1起序号；当前有界测试使用0禁用。
 * @param[in] throwOom 命中对象失败序号时为真则Throw MEMORY_ERROR，否则返回NULL。
 * @param[in] collectEachObject 为真时在对象创建和detached pair-pool预留前都运行真实FullGC。
 * @param[in] failReserve 要使detached pair-pool预留返回FALSE的1起序号；0禁用当前测试序列。
 *
 * ## Returns
 * 无C返回值；覆盖本TU故障配置、清零对象/预留/非法释放计数，并通过native setter复位native序号与命中标志。
 *
 * ## Constraints and lifecycle
 * 还清除remembered/pause失败开关和一次性barrier hook/context；不会释放GC对象、native分配或清理上一次case资源。状态是TU-static而非每state/线程私有，必须在没有正在使用该wrapper的物化/回调时串行重配；concurrency支持只在单次调用前后配置hook。只替换included Objects C的边界，真实GC/对象/哈希实现和linked logical analysis不随之被模拟。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:5；tests/core/ssa_runtime_objects_faults.c:24；tests/core/ssa_runtime_objects_faults.c:25；tests/core/ssa_runtime_objects_faults.c:26；tests/core/ssa_runtime_objects_faults.c:30；tests/core/ssa_runtime_objects_faults.c:31；tests/core/ssa_runtime_objects_faults.c:32；tests/core/ssa_runtime_objects_faults.c:33；tests/core/ssa_runtime_objects_faults.c:34；tests/core/test_ssa_runtime_objects.c:139；tests/core/test_ssa_runtime_objects.c:578。
 */

/**
 * @fn TZrUInt32 ssa_runtime_objects_allocation_count(void)
 *
 * ## Summary
 * 读取自上次faults复位后经故障Object_New入口发生的尝试次数。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return 当前objectCount快照；包括返回NULL或Throw前已计数的尝试，不是成功对象数。
 *
 * ## Constraints and lifecycle
 * 纯读TU-static非原子计数，不清空。须与本wrapper的调用/配置串行或按测试支持的同步协议读取；不统计普通库入口或GC内部其它对象分配，不分配/释放资源。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:7；tests/core/ssa_runtime_objects_faults.c:25；tests/core/ssa_runtime_objects_faults.c:37；tests/core/ssa_runtime_objects_faults.c:68；tests/core/test_ssa_runtime_objects.c:192；tests/core/test_ssa_runtime_objects.c:508。
 */

/**
 * @fn TZrUInt32 ssa_runtime_objects_invalid_mutation_end_count(void)
 *
 * ## Summary
 * 读取检测器捕获的过期mutation释放请求次数。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return 自faults复位以来locked为真而当前mutationDepth为0的请求数。
 *
 * ## Constraints and lifecycle
 * 仅检测test_mutation_end中明确的组合，命中时已抑制真实平台unlock以避免未定义行为；返回0不证明任意锁错误都被覆盖。getter不加锁、不清空、不拥有资源；在materialize同步返回后读取。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:8；tests/core/ssa_runtime_objects_faults.c:26；tests/core/ssa_runtime_objects_faults.c:39；tests/core/ssa_runtime_objects_faults.c:112；tests/core/ssa_runtime_objects_faults.c:115；tests/core/test_ssa_runtime_objects.c:409；tests/core/test_ssa_runtime_objects.c:416。
 */

/**
 * @fn void ssa_runtime_objects_fail_native_allocation(size_t ordinal)
 *
 * ## Summary
 * 为text-included Objects C的直接calloc设置一次故障序号并重开计数窗口。
 *
 * ## Parameters
 * @param[in] ordinal 从1开始的test_calloc尝试序号；当前有界测试以0表示不注入。
 *
 * ## Returns
 * 无C返回值；保存序号、nativeCount置0且nativeFailed置FALSE。
 *
 * ## Constraints and lifecycle
 * 只改变TU-static测试状态，不回收旧内存或影响已完成分配。只控制include后宏展开的直接calloc，当前为五个必需work数组和一个按需ignored快照；linked MaterializeState/Owner/Liveness及真实GC/哈希内部allocator不被此宏覆盖。须在无并发使用时配置，未运行的源码遍历不能报告实际命中次数。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:9；tests/core/ssa_runtime_objects_faults.c:43；tests/core/ssa_runtime_objects_faults.c:44；tests/core/ssa_runtime_objects_faults.c:45；tests/core/ssa_runtime_objects_faults.c:58；tests/core/ssa_runtime_objects_faults.c:130；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:444；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:451；tests/core/test_ssa_runtime_objects.c:579。
 */

/**
 * @fn TZrBool ssa_runtime_objects_native_allocation_failed(void)
 *
 * ## Summary
 * 读取当前native故障窗口是否已命中指定calloc序号。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return nativeFailed粘滞标志；命中过一次后保持TRUE，直到native setter或faults复位。
 *
 * ## Constraints and lifecycle
 * 此getter不清除标志，不证明最终公开API一定失败，也不统计linked allocator。test569用它区分被注入的失败与首次未命中的成功；TU-static非原子状态要求串行控制。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:10；tests/core/ssa_runtime_objects_faults.c:45；tests/core/ssa_runtime_objects_faults.c:48；tests/core/ssa_runtime_objects_faults.c:59；tests/core/test_ssa_runtime_objects.c:581；tests/core/test_ssa_runtime_objects.c:586。
 */

/**
 * @fn void ssa_runtime_objects_fail_remembered_reservation(void)
 *
 * ## Summary
 * 打开物化remembered registry预留的持续失败开关。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；failRemembered置TRUE。
 *
 * ## Constraints and lifecycle
 * 之后test_remembered_reserve直接FALSE且跳过真实预留；不是单次消费，须由faults复位。仅模拟该边界失败，不改变registry资源所有权或证明所有GC分配路径。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:11；tests/core/ssa_runtime_objects_faults.c:30；tests/core/ssa_runtime_objects_faults.c:50；tests/core/ssa_runtime_objects_faults.c:87；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:582；tests/core/test_ssa_runtime_objects.c:248。
 */

/**
 * @fn void ssa_runtime_objects_fail_pause(void)
 *
 * ## Summary
 * 打开物化附着暂停入口的持续失败开关。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；failPause置TRUE。
 *
 * ## Constraints and lifecycle
 * 之后test_pause直接FALSE，不调用真实StopTheWorldBegin也不填写该helper的pause diagnostic；须由faults复位，不会结束已经存在的暂停。仅模拟该边界，非真实超时/全部域故障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:12；tests/core/ssa_runtime_objects_faults.c:31；tests/core/ssa_runtime_objects_faults.c:51；tests/core/ssa_runtime_objects_faults.c:91；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570；tests/core/test_ssa_runtime_objects.c:360。
 */

/**
 * @fn void ssa_runtime_objects_set_barrier_hook(void (*hook)(SZrState *, void *), void *context)
 *
 * ## Summary
 * 为下一次经故障barrier入口的调用借用一个同步回调及上下文。
 *
 * ## Parameters
 * @param[in] hook 可为空的函数指针；非空时在下一次test_barrier先清空注册再调用。
 * @param[in] context 只存储指针值的借用上下文；若hook将写入它，其目标须可写且活到同步回调完成。
 *
 * ## Returns
 * 无C返回值；覆盖当前barrierHook和barrierContext，不调用hook或释放旧context。
 *
 * ## Constraints and lifecycle
 * 不是拥有型注册或线程安全队列；配置/使用须服从串行测试及并发helper的前后同步。test_barrier在hook抛错前已清空hook，但并不自动清空context；set(NULL,NULL)可同时撤销两者。栈context不得在未消费的hook仍可能调用时失效。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:13；tests/core/ssa_runtime_objects_faults.c:53；tests/core/ssa_runtime_objects_faults.c:54；tests/core/ssa_runtime_objects_faults.c:95；tests/core/ssa_runtime_objects_faults.c:96；tests/core/ssa_runtime_objects_faults.c:97；tests/core/test_ssa_runtime_objects.c:347；tests/core/test_ssa_runtime_objects.c:381；tests/core/ssa_runtime_objects_concurrency.c:270；tests/core/ssa_runtime_objects_concurrency.c:274。
 */

/**
 * @fn TZrBool ssa_runtime_objects_materialize(const SZrExecIrObjectMaterializationRequest *request, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明在独立fault TU里由同物理Objects生产正文生成的故障入口，供测试选择边界注入。
 *
 * ## Parameters
 * @param[in] request 可为空的借用请求；空值或无效记录返回FALSE。成功路径要求嵌套function/map/owner witness、源数组和type绑定匹配当前checkpoint与GC/布局契约，checkpoint.target须为空；target记录可写且count不超过capacity，非零capacity的输出槽有效可写并足够容纳aggregate，零capacity时槽数组可为空。
 * @param[out] diagnostic 可为空的诊断输出；非空时须为与request、所有仍访问的输入/输出及runtime元数据不重叠的独立可写槽。
 *
 * ## Returns
 * @return 成功时TRUE并发布aggregate数量及对象槽；验证/准备失败、TryRun返回非FINE状态或候选尚未ready时FALSE，在可用diagnostic中写失败信息。不能由返回值推断linked allocator全受故障控制。
 *
 * ## Constraints and lifecycle
 * 仅替换本fault TU文本include内的八个直接边界，生产算法与原库同物理定义，不能按编译实例重复授信。调用方持有request、数组、function/map及GC域生命周期，records的const不表示嵌套源payload/原型借用或target只读；真实GC可更新已注册根，失败保留旧发布逻辑图但不承诺所有源/堆字节冻结。成功发布不替调用方永久保活；返回后需自行为输出对象建立GC根，不能C free对象。旧target数组本身始终归调用方，wrapper不接管或释放。诊断不可别名，故障配置TU-static要求串行；共享域并发场景需先按caller-root协议保活，再进入GC-aware mutator作用域并保持至新输出根登记。wrapper可为测试而执行FullGC/Throw，生产TryRun与cleanup负责其已取得的候选数组/根/暂停/锁租约；不证明Unity早退会执行case的局部尾部。
 *
 * TryRun只有status!=FINE时才先将旧mutationLocked标志清假；Throw(FINE)可返回FINE，不能外推任意Throw都走该分支。当前故障Throw只用MEMORY_ERROR，未覆盖Throw(FINE)。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.h:14；tests/core/ssa_runtime_objects_faults.c:123；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:673；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:698；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:707；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:725；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:727；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:731；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:734；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_runtime.h:48；tests/core/ssa_runtime_objects_concurrency.c:257；tests/core/ssa_runtime_objects_concurrency.c:259；tests/core/ssa_runtime_objects_concurrency.c:277；tests/core/ssa_runtime_objects_concurrency.c:298；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:708；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:712；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:86。
 */
