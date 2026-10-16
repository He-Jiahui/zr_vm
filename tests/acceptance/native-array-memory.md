# Core 原生数组内存回归

## 范围与修改前基线

修复 Init 字节尺寸溢出与 OOM 状态、Push 增长/字节上界、Push/Append 扩容 OOM 原子性，以及 Free 空 head 状态复位。原 void 入口失败沿 VM MEMORY_ERROR 传播；新增 TryInit/TryReserve/TryPush/TryAppend 供需要在所有权转移前预留容量、清理局部资源并返回失败的宿主调用使用。

MSVC 19.44 实际修改前 CTest：9 项，2 PASS、7 FAIL，其中四项崩溃。原始日志为 reports/review-bugfix-20261016/native-array-msvc-red-r3-2.log。配置或编译失败的两次准备尝试不算行为 RED。

## 测试清单

normal、init_overflow、init_oom、push_oom、append_oom、length_overflow、capacity_limit、free_null_head、append_empty、growth_saturation、try_init_failure、try_push_failure、try_append_failure、try_reserve，共 14 项。OOM 后重试和同分配器/原始大小/类别释放在案例内断言。独立入口和普通 CTest 使用同一份注册。

## 工具与结果

MSVC 19.44 与 ClangCL 19.1.5，Debug，Windows SDK 10.0.26100.0；实际 checkout，所有输出位于 E:/cargo-targets/zr_vm。通过 native-array-run.py 的当前记录核对 configure/build/CTest 命令、源文件 SHA 与进程自然退出状态。

真实显式头依赖重编后的 MSVC 14/14 PASS；ClangCL 14/14 PASS，见两份 `native-array-*-reserve-green-receipt.json`。首次 postedit MSVC 复用了旧目标文件而失败，未计为修复证据。该验证使用替代的异常传播边界，不声称完整运行时 OOM 恢复已验证。

真实 Core 生命周期另有 main_state_oom、import_array_oom、unlaunched_state 三项；修复前分别崩溃、abort 后超时、空栈断言后超时，见 `core-lifecycle-clangcl-red-2.log`。修复后 3/3 PASS，见 `core-lifecycle-clangcl-green-resumed-receipt.json`。独立复核发现栈初始化 OOM 路径，其追加真实分配失败回归见下段。已完成较早数组版本的所需目标构建、代际池 14/14、CLI hello-world smoke；新增启动修复后需重验。

追加 main_stack_oom、worker_stack_oom_retry、stack_size_overflow 后，真实 Core 生命周期共六项。统一修改前记录 `integrated-baseline-red-r3-receipt.json` 与 `integrated-baseline-red-r3-2.log` 显示：新三项均 FAIL，原三项仍 PASS。主/worker 栈 OOM 均在 value.h 的空槽断言失败，尺寸边界案例检测到 Stack_Construct 返回非空且多出一个存活块。夹具误用 SZrStackPointer 的编译失败已修为 TZrStackPointer，该失败不计作行为 RED。

对应修复为：Stack_Construct 在字节乘法前拒绝尺寸溢出，OOM 返回 NULL 而不计算尾地址；state_stack_init 先检查构造结果，主线程通过已有 TryRun 交给 GlobalState_New 清理，worker 的 MutatorLaunch 返回 false 并可重试。NormalizeStatus 在没有 global/基础 Error 原型时避免分配消息和错误对象，保留已有 currentException 的路径。六项在 integrated-stack-registry-lsp-plugin-red 收据中全部GREEN，同轮源码哈希无变化；数组14项、整数幂7项、代际池14项和CLI hello-world也通过。SSA目标现已链接真实Core，缺失Exception符号的链接失败已消除；条件清理行为用例仍超时，正在另行诊断。

## 验收状态

定向数组14项与真实Core生命周期6项通过，上层代际池/CLI通过；模块独立最终审查已ACCEPT，文档已收紧void错误传播可能分配/GC的边界。SSA条件清理行为超时和其他模块剩余BUG不计作已修复。WSL 实测 E_ACCESSDENIED，GCC/Clang Linux 矩阵未执行，不能声称全仓或跨平台通过。
