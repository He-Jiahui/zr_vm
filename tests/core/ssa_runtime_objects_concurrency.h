#ifndef ZR_TEST_SSA_RUNTIME_OBJECTS_CONCURRENCY_H
#define ZR_TEST_SSA_RUNTIME_OBJECTS_CONCURRENCY_H

#include "exec_ir/exec_ir_runtime.h"

/* Keep caller storage rooted while another attached state requests collection
 * at the first attachment barrier. Require collection after materialization
 * returns, and return only after the worker has been joined. */
TZrBool ssa_runtime_objects_competing_collector(
        const SZrExecIrObjectMaterializationRequest *request,
        SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool ssa_runtime_objects_competing_collector(const SZrExecIrObjectMaterializationRequest *request, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 串行 Unity 场景中保持 caller 输入及输出根，安排竞争线程在首个原型附接屏障请求 GC，并在物化返回、输出登记和 worker 回收后报告组合结果。
 *
 * ## Parameters
 * @param[in] request 完整存活到返回的测试请求记录；嵌套 caller state 与可写 values/types/target 存储仍由 caller 拥有，须适合根登记和实际物化。
 * @param[in,out] diagnostic 可为空的独立、已初始化诊断 slot；只有被调物化 API 按其分支写入，本 helper 的前置/晚期失败不统一清零或设置它，失败日志可能读取其既有 code。
 *
 * ## Returns
 * @return 物化成功、hook 观察暂停、worker 完整回收计数恰增1且该观察晚于 API 返回，并且线程/根收尾检查成功时 TRUE；任一条件失败返回 FALSE，部分失控等待直接 abort。FALSE 不保证 target 保持调用前内容。
 *
 * ## Constraints and lifecycle
 * 本函数是当前有效 fixture 的并发验证桥，不是任意请求的完整验证器：外层仅检查部分 NULL/count/capacity，prepare 随后会读取全部 value tag。request 记录可 const 借用，但其 state、values/prototype 指针载荷与 target 输出按运行时契约可修改；输入记录、function/map/池和诊断须满足被调接口的稳定可读及不相互覆盖前提，diagnostic 可空，非空时须是独立可读写 slot。全局 fault hook 不支持并发重配，本 helper 在同域单一测试协议内串行使用。先根化输入再 Enter，worker 附着后保持 inactive；物化 API 的内部暂停/标记锁由其本体释放，helper 不持有或消费其锁 token。API 返回后 release 发布 materializationReturned，再在额外 RUNNING 层下登记新 output 根，随后 Poll 让 worker收集；等待/join 完成才 detach、逆序 Pop/free caller 槽和 Leave。正常清理不表示任意非局部退出都受保护；worker超时及 OS join 错误使用 abort 而非 Unity case longjmp。晚期观察、线程句柄关闭或根平衡失败可在 target 已发布后使返回 FALSE，不回滚新 target；GC 也可更新源 payload。无 hook/无实际 collection 或 collection 早于返回均使组合结果为假。当前 hook 仅覆盖原型 RawObject_Barrier，不覆盖跨 TU 字段 Gc_WriteBarrier，也不证明发生移动 GC。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.h:9；tests/core/ssa_runtime_objects_concurrency.c:235；tests/core/ssa_runtime_objects_concurrency.c:270；tests/core/ssa_runtime_objects_concurrency.c:277；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_concurrency.c:293；tests/core/ssa_runtime_objects_concurrency.c:296；tests/core/test_ssa_runtime_objects.c:441；tests/core/test_ssa_runtime_objects.c:831；tests/core/ssa_runtime_objects_faults.c:128；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:615。
 */
