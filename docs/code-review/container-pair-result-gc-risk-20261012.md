---
related_code:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership.c
  - zr_vm_core/src/zr_vm_core/object/object.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object.c
plan_sources:
  - user: 全仓首方代码调用链审查与宏观契约注释任务
tests: []
doc_type: review-detail
---

# Pair 构造结果的 GC 根与结构复制

## Summary

**BUG: Pair 原生构造结果在结构复制前未登记 GC 根。** 公开零参构造可创建只由 native dispatcher 的 C 局部结果持有的 GC Pair。首次复制到 VM 栈时，Core 按 internal STRUCT 克隆它；首次 clone 分配若触发成功的完整回收，源 Pair 可以被释放。分配重试成功后继续读取 source，可能形成 UAF 或损坏的原型/成员值。

这是完整可达调用路径上的条件性静态结论。作者 pair_result_clone_r200 与独立审查者 pair_result_independent_r201 分别阅读当前源码；Root 核公开调用、实际 lane、native 边界、结果复制及 OOM 入口。本阶段没有运行复现、故障注入、构建或测试。

## Parameters and trigger conditions

以下条件须同时成立：

1. 有效且已初始化的 VM state；容器模块已注册，Pair 原型已物化。
2. 宿主从原型取出 __constructor 闭包，经公开 ZrLib_CallValue 传空 receiver、空 arguments、argumentCount=0 和有效结果槽。
3. Pair 目标及两个 null 分量初始化成功，新对象尚未进入 VM 栈，也没有其它显式根保活它。
4. CloneStruct 首次对象分配失败；collector 非 immediate，完整 major GC 的暂停、扫描与回收成功。
5. 回收后的分配重试成功，克隆继续读取原 source。

Pair descriptor 范围为 0..2，callback 另外只接受 0 或 2。本证明使用合法零参，单参负面行为不参与缺陷条件。

## Call chain and intent

| 阶段 | 调用目的与实际边界 |
| --- | --- |
| 公开构造闭包 | ZrLib_Type_FindPrototype 可查询已物化原型；metadata 将构造闭包挂为 __constructor。CallValue 的零参校验允许空 receiver/arguments。 |
| 创建目标 | 空 receiver 令 Self=null；resolver 按 owner 原型创建 internal STRUCT，设置 null 分量后以 OBJECT 外部标签交付结果。 |
| 实际派发 lane | flags=0 且 min/max 不同，Pair 未进入 stack-root 或固定 arity fast lane，而走 inline-pinned。此 lane 只保护已有 Self/参数，零 Self/零参数不产生 pin 项，callback 结果不在保护集合。 |
| 首次结果入栈 | dispatcher 的 result 是 C 局部值；finish_result 恢复锚点后通过 Value_Copy 写闭包结果槽，复制前没有登记 source 根。 |
| STRUCT 克隆 | ownership 将 GC OBJECT/internal STRUCT 送入 CloneStruct；克隆先分配对象，后读 source->prototype/nodeMap；后续 ignore 保护的是 clone。 |
| OOM 及回收 | RawObject_New 首次对象分配进入 GcMalloc。首次 NULL 调 GcFull 再重试；成功 major GC 扫显式根及 VM 栈，未标记的 INITED 旧代源对象可被 sweep。重试成功后代码继续使用原 source。 |

完整路径与当前行号见 References 和同一构造单元的台账。原 22 项源码哈希在任务报告 container-pair-result-gc-static-review-r202.json；新增公开 object.h 的哈希及本次文档修订另记于 container-pair-result-gc-written-review-r204.json（23 项依赖）。

## Constraints and lifecycle

Pair 的 OBJECT 外部标签与 internal STRUCT 协作成立，Core Copy 使用结构克隆语义，值复制不承诺共享身份；本问题的生存期缺口位于首次发布复制之前。

对象链表与 domain membership 管理分配对象，可达根由独立 root-slot 表和 VM 栈提供。它们不扫描 dispatcher 的 C 局部结果；实例指向 prototype 也不会反向保活实例。

NativeEnter/NativeLeave 管理 safepoint 状态，没有为结果登记 root；callback 返回后才执行结果复制。CallValue 更晚从 VM 栈向宿主输出槽的复制无法保护之前的 C 局部 source。

## TODO and remaining boundaries

- TODO: 在后续修复任务中确定首次入栈前的结果保活责任，沿 native_binding_dispatch_finish_result、ZrCore_Value_Copy、ZrCore_Object_CloneStruct 核接口协作，并对本公开零参入口注入准确的首次 clone 分配失败。
- TODO: 重试仍失败会抛 MEMORY_ERROR；最终宿主返回及异常恢复需另核 panic/受保护调用边界，本 BUG 使用重试成功路径。
- TODO: 单参负面场景、字段初始化失败后的构造部分状态及其它回调的临时结果保护继续分别审查。

没有移动 GC、所有 collector 模式、所有构造器或整模块运行验收信用；现有普通 Pair 测试未触发本 OOM 条件。

## Delivery state

当前容器源码有另一会话的目录迁移差异。本阶段交付此报告和既有 zr_container_pair_constructor 的台账更新，源码零写入、行为未改。同一单元身份保持，没有新增单元。历史详细审查计数为 236/357、剩余 121；该计数不能代表当前整份台账或模块验收通过，文件仍 pending。

374 行全量台账的 check-batch 已以 exit 1 结束，失败包括迁移后遗留的旧 evidence 路径与 path-only common 宏行。本次构造单元已完成当前调用链语义复核；单行机械检查及其实际终态另记于后续审查报告。其余历史记录仍需路径迁移和语义复核。

## References

- zr_vm_library/include/zr_vm_library/native_binding.h:657、:671；zr_vm_core/include/zr_vm_core/object.h:303：公开原型查询、对象字段读取与同步调用。宿主可用字符串键从 prototype 的 super 对象借用已安装的 __constructor；这些是公开接口声明证据，不冒充仓内 Pair 专属调用点。
- zr_vm_library/include/zr_vm_library/native_binding.h:255、:264：元方法 arity 与 dispatchFlags 字段，Pair 的静态初始化未指定的后续 flags 为零。
- zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1407、:1429、:1437：构造 callable 的注册、__constructor 条件与实际字段安装。
- zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1809、:1817、:1879：公开零参和 receiver 处理；:284、:362：C 局部结果；:395、:404：inline-pinned 和结果发布；:249、:262：首次 Value_Copy；:1931：更晚宿主输出复制。
- zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:499：零 raw argument 的 Self=null。
- zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:332、:547、:557：固定 arity、stack-root 与 inline-pinned 的实际判定；:274、:278：native 边界。
- zr_vm_core/src/zr_vm_core/object/object_call.c:475、:476：直接分派缓存要求 stack-root flag 与固定参数计数；Pair 的 flags=0、min/max 不同不满足该路径。
- zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:367、:387、:405：Self/参数保护及 callback；没有保护输出值的循环。
- zr_vm_lib_container/src/zr_vm_lib_container/module.c:1891、:1907、:5559、:5576、:5582、:7608、:7733：目标解析、零参构造、结果及 STRUCT descriptor。
- zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:873、:889：STRUCT 实例初始化。
- zr_vm_core/src/zr_vm_core/ownership/ownership.c:193、:207：Core 复制选择 CloneStruct。
- zr_vm_core/src/zr_vm_core/object/object.c:1613、:1625、:1629、:1634、:1642：clone 分配、保护与后续源读取。
- zr_vm_core/src/zr_vm_core/base/memory.c:16、:17、:18、:42、:44：首次 NULL、完整回收及分配重试。
- zr_vm_core/src/zr_vm_core/gc/gc.c:455、:766、:795：默认模式及 full major；gc_cycle.c:2903、:3140：重置和 generation 翻转；gc_mark.c:471、:1303：显式根/VM栈；gc_internal.h:42、gc_sweep.c:27、:34：未达判断及实际释放。
- zr_vm_core/src/zr_vm_core/gc/gc_object.c:1069、gc_domain.c:265、:512、gc_domain_mutator.c:630、:696：对象/domain管理、root-slot 与 native 进入/退出边界。
- docs/code-review/comment-standard.md：完整可达静态链可标 BUG，静态证明与运行验证分别记录。
