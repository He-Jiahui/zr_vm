---
related_code:
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_write.c
  - zr_vm_core/src/zr_vm_core/object/object_write_internal.h
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/reflection/field/reflection_construction.c
  - zr_vm_core/src/zr_vm_core/reflection/reflection_member_query.c
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/reflection/reflection_object_internal.c
  - zr_vm_core/src/zr_vm_core/reflection/reflection_object_internal.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - tests/core/test_object_write_failures.c
  - tests/cmake/object-write-failure-tests.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_write.c
  - zr_vm_core/src/zr_vm_core/object/object_write_internal.h
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/reflection/field/reflection_construction.c
  - zr_vm_core/src/zr_vm_core/reflection/reflection_member_query.c
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/reflection/reflection_object_internal.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/core/test_object_write_failures.c
  - tests/cmake/object-write-failure-tests.cmake
  - tests/core/test_core_exception_oom.c
  - tests/core/test_gc_root_leases.c
  - tests/core/test_object_shape_transition_cache.c
  - tests/core/test_gc_nested_mutation.c
doc_type: module-detail
---

# 对象存储写入的失败传播

## 范围与 API

旧 `Object_SetValue` 的存储实现会在初始化、临时保活、键规范化、数组存储转换、rehash 或 pair 分配失败后退出，但没有结果。Member、Index 及反射包装往往仍返回 true，Library CString 字段包装也会正常返回。上层工厂因而可能发布字段缺失的对象。

`ZrCore_Object_TrySetValue` 返回实际存储结果，并在失败时保存 `state->threadStatus`。资源普通失败为 MEMORY_ERROR；参数无效为 INVALID；跨域写入拒绝为 RUNTIME_ERROR。受保护工作中实际抛出的状态原样保存。Try 入口在本地捕获这些异常、清理后返回 false，因此调用它的外层 TryRun 可以返回 FINE，同时 bool 结果为 false。成功时写入状态为 FINE。

既有 void `ZrCore_Object_SetValue` 委托 Try，失败后才用保存的状态 Throw。它不再把没有写入当作正常完成，也不把所有失败原因都当作 OOM。`state == NULL` 时无法抛出，仅返回。

Core 的 `TrySetFieldCString` 将字段名创建和字段存储置于同一事务。Library 新增 `ZrLib_Object_TrySetFieldCString` 委托该入口；既有 void CString setter 在其失败清理完成后传播真实错误。Reflection 的 `ObjectSetFieldValue` 直接返回 Core Try 结果，不复制保活逻辑。

Member／CachedDescriptor／StackOperands／NewOwner／字段初始化和 Index 的原有存储 fallback 统一返回 optioned Try 结果。语义拒绝和属性 setter 派发继续由原分支处理。已存在 pair 的其他快速覆盖路径仍保留原实现，未获得本组故障回归信用。

## 事务与根所有权

`object_write.c` 独立拥有参数检查、暂时 root leases、键规范化、初始化、哈希插入、异常边界和结果。`object.c` 仅向它提供窄的内部存储表示准备及 pair 提交接口；缓存及 member-version 细节留在原有所属实现中。新增文件由模块已有递归 `CONFIGURE_DEPENDS` 收集，无需顶层 CMake 修改。

新字段在插入 pair 前先复制到事务 context。普通 struct 的 `Value_Copy` 需要克隆字段，失败可能仅留下 NULL 值而不抛异常；事务同时检查 threadStatus 和非 NULL 输入被清空的结果。复制值取得独立临时 lease 后才允许 HashSet_Add。提交接口对新 pair 只转移已经复制的值，不再次克隆或增加 ownership 引用。复制失败不发布字段，也不增加 elementCount；已经存在的普通 pair 仍使用原有浅覆盖语义。

后续插入失败时，暂存值新增的 shared/weak 等控制块引用在独立保护边界中释放，然后释放包括暂存值在内的全部 leases。control-less direct unique 的 CopyUnique 结果只是调用者 owner 的镜像，取消暂存不 drop 调用者资源。已经转移到 pair 的引用不再由 context 释放。本组 struct/int 故障用例不为复杂 ownership observer 的内部异常赋予回归信用。

`CloneStruct` 在初始桶失败时返回 NULL；字段循环调用 Try setter，失败统一经过 cloneIgnored 清理后返回 NULL，避免新的 void setter Throw 越过显式根清理。`object_hidden_set`、`object_sync_struct_receiver_value`、`construction_set_field`、`query_set_object_field` 和 `query_array_push_object` 同样使用 Try，继续让调用者通过 bool 执行其清理；CString helper 的字段名创建也处于保护边界内。后五处属于同层必要契约迁移，共享本组存储故障覆盖，没有独立 OOM 用例；QueryMembers 正常行为应复用现有 Reflection 集成回归验证。这些超长文件仅作既有调用契约的局部迁移，没有新增职责；写入事务已整体提取至 object_write.c，本轮不继续拆分不相关职责。

TryRun 的调用者持有完整事务 context，回调更新其 lease 和结果字段。C longjmp 返回时，不读取在 setjmp 所在函数内被回调修改的非 volatile 自动变量。目标对象、值、原键、规范化键和新 pair 的复制值分别持有可配对释放的 lease；同对象多次获得也必须分别释放。受 VM 栈保证保活的操作数可保留原 stack lane。

临时 lease 与调用者的显式 ignored root 各自有所有权。释放本次 lease 不撤销调用者原有 root，也不通过 registry 长度截断来猜测所有权。普通字段写入不使用会改变 region／storage 状态的 native pin。

正常回调配对一次 MutationBegin／End；如果内部 Throw 非局部退出，现有 TryRun scope snapshot 恢复回调取得的 mutation 层。回调退出后的暂存值清理和 lease 释放不再次 MutationEnd。清理完成后 bool 返回 false，或由 void 包装向外抛出。

## 防止错误规范化递归

新 setter 清理边界会在 Error 对象字段构造中建立嵌套 TryRun。若每个恢复点都重新允许规范化，持续 OOM 会重复构造 Error。私有 TryRun context 因而单独保存 `inheritedNormalizationSuppression`：入口继承外层本地规范化中或继承抑制中的状态；Begin 同时检查两者，End 只清本层自行取得的标志。普通嵌套 TryRun 仍可独立规范化，Error 构造期间的清理边界则不会重新打开规范化。

必须重跑已有五项 Exception OOM 保护基线，不能仅凭八项 setter 用例推断该公共恢复机制已正确。

## 回归与证据边界

`test_object_write_failures.c` 链接实际静态 Core／Library，Reflection 通过真实私有头调用实际 helper，没有替代被测实现。十项独立 CTest 各有 30 秒超时及 Windows 无对话框诊断。独立 fragment 检查全部七个实际静态链接目标，裁剪配置缺任一目标时跳过，不生成不存在的链接名。

| 用例 | 真实 seam | 成功标准 |
| --- | --- | --- |
| `init_core` | HASH_BUCKET、新块、真实初始桶字节数 | void 写入由 TryRun 观察到 MemoryError |
| `rehash_member`、`rehash_index` | HASH_BUCKET、目标旧桶地址、实际倍增容量字节数 | 外层 TryRun FINE、setter false |
| `rehash_library` | 相同 rehash seam | void CString 包装抛 MemoryError |
| `rehash_reflection` | 相同 rehash seam | 真实 Reflection bool helper false |
| `pair_core`、`pair_library`、`pair_reflection` | HASH_PAIR、新块、sizeof(pair)，持续拒绝至 TryRun 返回 | 可观察失败、无新增临时根、调用者显式根保留 |
| `struct_clone_pair` | 实际含字段普通 struct，持续 HASH_PAIR 新块失败 | CloneStruct 返回 NULL、释放 clone 临时根、保留两个调用者根 |
| `struct_write_bucket` | 向已初始化目标新增 struct 字段，持续拒绝 clone 初始 HASH_BUCKET | void setter 抛 MemoryError、无字段或 elementCount 增量 |

所有用例验证存储计数、可用旧值、解除故障后的同入口重试、最终宿主分配零存活及类别／大小配对。失败前后逐字段比较 CaptureScopes 的 execution/native/mutation 深度及身份，并比较 AOT 根链、深度和 nestedNativeCalls。

八项原始用例在 `support-batch-2-red` 的真实运行中失败；`support-batch-3-red` 中原八项 PASS，新两项 struct 用例真实 FAIL。两项新增用例验证解除故障后的独立 clone 和字段内容，其中 setter 用例另验证已有 pair 保持浅覆盖。最新生产修复及加强断言已在 root 的 support-batch-5-green 中实际重编译，十项全部通过；1929 个输入哈希无变化。本组默认没有开启 concurrent major，因此其 scope 基线相等不证明并发 GC／STW 的锁恢复；实际并发 root lease 和 nested mutation 由各自测试负责。

本组不声称修复多字段工厂原子发布、模块双导出表事务、Reflection `ObjectSetString` 在进入字段 helper 前创建 value 的 root 问题，或所有已存在 pair 快速覆盖的软失败。这些路径需要各自真实 RED 后再扩展修复。
