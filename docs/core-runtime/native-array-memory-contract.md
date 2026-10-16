---
related_code:
  - zr_vm_core/include/zr_vm_core/array.h
  - zr_vm_core/include/zr_vm_core/memory.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - tests/core/test_native_array_failures.c
  - tests/cmake/native-array-tests.cmake
  - zr_vm_core/src/zr_vm_core/runtime/global.c
  - zr_vm_core/src/zr_vm_core/runtime/state.c
  - zr_vm_core/src/zr_vm_core/runtime/stack.c
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - tests/core/test_core_allocation_lifecycle.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/array.h
  - zr_vm_core/src/zr_vm_core/runtime/global.c
  - zr_vm_core/src/zr_vm_core/runtime/state.c
  - zr_vm_core/src/zr_vm_core/runtime/stack.c
  - zr_vm_core/src/zr_vm_core/exception/exception.c
plan_sources:
  - user: 2026-10-16 根据会话 01a13463 的 BUG 注释修复现役代码
  - docs/code-review/coverage/zr_vm_core_array_api.tsv
tests:
  - tests/core/test_native_array_failures.c
  - tests/cmake/native-array-direct-validation/CMakeLists.txt
  - tests/core/test_core_allocation_lifecycle.c
  - tests/cmake/core-allocation-lifecycle-tests.cmake
  - tests/acceptance/native-array-memory.md
doc_type: module-detail
---

# 原生数组的内存契约

`SZrArray` 是由 VM 宿主分配器持有的定长元素字节数组，不是托管的 guest Array。Init、Push、Append 只复制存储字节；元素内的指针、引用和析构仍由调用方负责。Get 与 Pop 返回的内部借用在扩容、对应槽覆写或 Free 后失效。

## 状态与失败

Construct 产生 head=NULL、elementSize/length/capacity=0、isValid=false。Init 只用于尚未持有缓冲区的对象；零容量请求规范化为一个元素，元素宽度必须非零。只有完整缓冲区分配成功后才发布 head、宽度、容量和有效标志。

Init 的容量乘元素宽度先以除法检查可表示性。Push 的长度增一和 Append 的总长度都按 `SIZE_MAX / elementSize` 检查，超过存储上界时不进入分配器，也不访问源区。增长比例拆成整数部分和余数部分计算，避免先做 `capacity * percent` 造成中间乘积回绕；增长超出字节上界时夹到最大可表示元素数。

扩容先把分配器结果保存在局部变量，成功后一起更新 head 和 capacity，最后复制新元素并增加 length。OOM 保留原指针、长度、容量与元素；调用方捕获错误后可以重试或 Free。失败的宿主 realloc 必须保留旧块，这与 VM 其他可重试的原生容器相同。

TryInit、TryReserve、TryPush、TryAppend 返回成功标志。尺寸不可表示和 OOM 都返回 false，不抛异常；TryInit 留下 Construct 空状态，其余入口保留原数组。TryReserve 只预留至少指定容量，不改变长度，便于调用方在转移元素所有权前完成可能失败的分配。启动和宿主构造函数使用这些接口清理本地资源并兑现 NULL/false 失败约定。

Init、Push、Append 仍为 void，包裹相应 Try 入口并沿 `ZrCore_Exception_Throw(state, MEMORY_ERROR)` 传播失败。数组分配自身不执行 GC 或 GC 重试；Try 接口全程不触发 GC。void 包装器失败后的 Throw 遵从异常层契约，异常规范化可能分配或触发 GC。需恢复的调用应在相同线程的 TryRun 动态范围内；跨跳转的调用方临时资源仍应按 VM 异常契约清理。

GlobalState_New 在 GC、字符串表和主线程 launch 之前检查主 state 与导入数组的分配结果；失败只回收已经构造的资源并返回 NULL。State_Free 检查实际栈指针，未 launch 的 state 不计算空栈边界或释放未申请的栈。

Stack_Construct 也按槽宽检查字节尺寸，尺寸不可表示时不进入分配器；OOM 与尺寸失败均将栈基址保持为空并返回 NULL，不计算空指针的尾地址。state_stack_init 只有在完整栈分配成功后才建立栈顶、边界和基础调用帧。主线程 launch 失败沿 GlobalState_New 已有的 TryRun 返回 MEMORY_ERROR，由全局失败清理回收已建资源；MutatorLaunch 的布尔接口直接返回 false，保留无栈、无入口帧状态，允许关闭故障注入后重试。

主栈分配发生在 StringTable_Init 与 GlobalState_InitRegistry 之前，此时还没有预建内存错误消息或基础 Error 原型。NormalizeStatus 保留已有 currentException 的快速路径；没有 global 或基础 Error 原型时不创建查找字符串和错误对象，仅让 Throw 传递原生状态并恢复 TryRun scopes。正常初始化先在 registry 建立基础 Error，再允许 afterStateInitialized 等宿主路径注册或使用自定义特化错误；运行时的 Error 身份判断本身也以该基础原型为前提。

Free 只释放有效数组的现有缓冲区，并将所有字段复位；head 已经为空时也复位，重复 Free 安全。数组内对象不会被逐项释放。

## 源区与测试边界

Set 使用不重叠字节复制。Push/Append 的源在整个调用期间保持可读，可能扩容时不得从目标数组借用；本接口不支持自追加。零长度 Append 不访问 elements，也不申请空间。

回归通过实际 header、真实 malloc/强制搬移的 realloc 分配器，以及在异常符号边界的 setjmp/longjmp 捕获，核查尺寸拒绝、初始化 OOM、两种扩容 OOM、失败后重试、正常复制与清理。容量极限用合成元数据验证提前拒绝和 reserve 的饱和申请尺寸，不声称实际申请 SIZE_MAX 大小存储。真实 Core 的分配失败回归另外链接生产 Core，跟踪主 state/导入数组构造失败与未启动 state 回收的存活块、尺寸和类别；替代异常边界不授予这些覆盖。

当前报告位于 `E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/`。修改前九项为两项通过、七项失败（含四个崩溃）；增加 Try、预留容量与增长饱和案例后，MSVC 与 ClangCL 定向各 14 项通过。真实 Core 的 main_state_oom、import_array_oom、unlaunched_state 三项修复后均通过；随后独立复核发现的栈初始化 OOM 路径另列于下述追加回归。首次配置路径和源码编码配置错误未执行测试；首次 postedit Ninja 未重编头文件，保留为无效构建证据，随后增加显式 OBJECT_DEPENDS，真实重编通过。WSL 在本会话权限下为 E_ACCESSDENIED，没有 Linux 通过信用。

追加的真实 Core main_stack_oom、worker_stack_oom_retry、stack_size_overflow 三项已在 `integrated-baseline-red-r3-2.log` 形成行为 RED：前两项在空栈槽初始化断言失败，后一项观察到字节乘积回绕后多出一个存活块。该轮原生命周期三项、数组 14 项和整数幂 7 项仍通过。新增栈处理和无 Error 基础设施时的规范化检查已在 integrated-stack-registry-lsp-plugin-red 真实重编后使生命周期六项全部通过，同轮数组14项、整数幂7项、代际池14项和CLI hello-world也通过，输入哈希无变化。SSA独立目标补Core依赖后链接成功，但条件清理行为测试超时，另行排查，不计作本轮全通过；测试夹具类型名修正导致的早期编译失败不算行为证据。
