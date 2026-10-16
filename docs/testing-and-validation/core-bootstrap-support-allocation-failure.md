---
related_code:
  - zr_vm_core/src/zr_vm_core/runtime/global.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/string.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/runtime/global.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/string/string.c
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
  - user: 根据会话 01a13463 的已确认 BUG 修复现役代码
tests:
  - tests/core/test_core_bootstrap_support_oom.c
  - tests/cmake/core-bootstrap-support-oom-tests.cmake
doc_type: validation-detail
---

# Core 启动支持对象的 OOM 回滚

GlobalState_New 先创建 global、主 state 和导入数组，再构建 GC 管理器、域和字符串表管理器，最后在 TryRun 中启动主线程。前半段尚无恢复点，构造函数必须检查可失败的宿主分配，不能依赖异常处理兜底。

GC 和 StringTable 的 New 保留现有 void 签名：分配失败后对应 global 字段为空且立即返回；GlobalState_New 检查该字段。GC 管理器失败时只回收已建的数组、未启动 state 和 global。StringTable 管理器失败时 GC/域/profile 已建立，走全局关闭路径；StringTable_Free 接受空表，从而回收已经取得的资源。成功路径的初始化顺序保持原有依赖。

字符串桶在主线程 launch 的 TryRun 内分配。HashSet_Init 初始化后检查 isValid；初始化失败立即 Throw(MEMORY_ERROR)，不再访问空桶、不创建首个短串；已有启动恢复点接收状态并执行全局失败清理。此时 Error 原型和预建 OOM 字符串尚未建立，异常层沿既有 bootstrap 快速路径传递原生状态。

三个独立进程分别拒绝真实 GC 管理器、StringTable 管理器、初始驻留桶的第一次匹配分配。条件同时匹配分配类别与精确尺寸，释放始终允许。每次要求 GlobalState_New 返回 NULL、分配器所有存活块归零、释放的旧尺寸及类别正确；移除注入后必须重新构建并正常释放。夹具不替换生产构造器或异常实现。

稳定 `integrated-support-batch-2-red` 中三项均在配置/编译成功后实际 SegFault，拒绝记录分别为 MANAGER 1280 字节、MANAGER 136 字节及 HASH_BUCKET 32768 字节（本 x64 构建）。尺寸由真实结构和配置计算，不把这些观测数值写为生产常量。修复后 integrated-support-batch-3-red 已实际重编并通过这三项，输入哈希稳定。已有数组/栈六项生命周期回归同时保留；更早通过不能代替这三项新回归。

证据写入 E:/cargo-targets/zr_vm/reports/review-bugfix-20261016，直接编译实际 checkout。WSL 为 E_ACCESSDENIED，不声明 Linux 通过。相关字符串存储失败另见 [String 存储回归](string-storage-allocation-failure.md)，数组和未启动 state 合同见 [原生数组内存契约](../core-runtime/native-array-memory-contract.md)。
