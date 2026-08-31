---
related_code:
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - user: 2026-08-30 根据 docs/plans/benchmark/optimize/03-memory-object-gc.md 优化对象 shape 与 member slot
  - docs/plans/benchmark/optimize/03-memory-object-gc.md
tests:
  - tests/core/test_object_shape_transition_cache.c
  - tests/core/test_execution_member_access_fast_paths.c
  - tests/core/test_execution_dispatch_callable_metadata.c
  - tests/acceptance/2026-08-30-object-shape-transition-cache.md
doc_type: module-detail
---

# Object Shape And Member Cache

## Purpose

对象成员访问的缓存必须能区分“哪个 prototype/shape”与“该 shape 的哪一代”。本模块为 prototype 提供进程内稳定 `shapeId` 和单调 `shapeGeneration`，并把这两个值写入 member/PIC slot。这样，字段描述符、继承关系、meta 函数或 prototype 自有成员发生变化时，旧缓存会在下一次访问时 fail closed；普通实例字段值覆盖不会因为值本身变化而制造新的 shape 身份。

## Behavior Model

- 每个 `SZrObjectPrototype` 创建时分配非零 `shapeId`，其生命周期内保持不变。GC rewrite 只转发对象引用，不重新生成该 ID。
- `shapeGeneration` 从 1 开始。添加 descriptor、改变 super prototype、初始化/添加 meta、module prototype accessor 更新，以及 prototype 自有成员写入都会调用 `ZrCore_ObjectPrototype_MarkMutation`。
- mutation 同时推进既有 `memberVersion`，保留旧调用方的版本防线；新 PIC slot 还保存 receiver/owner 的 shape id 与 generation。
- member PIC 的固定容量为 4。插入超过容量时使用已有环形替换策略，不在访问热路径分配内存。
- prototype、descriptor、callable 和 meta slot 命中必须满足 prototype 指针、shape id、shape generation 和既有 member version 均匹配。任一条件不匹配会清空该 cache entry 并回到按名慢路径。
- 精确 receiver object 与精确 pair 身份同时命中时，pair 本身已证明当前实例字段位置；该 lane 可读取/写入同一 pair，并在命中后同步 receiver/owner version、shape id 和 generation。它不放宽 prototype 或 descriptor 缓存的代数检查。
- meta get/set 的缓存写入和读取使用同一 shape 校验；动态 meta/proxy 解析仍走慢路径。

## Profile Metrics

当内存 profile 打开时，member cache 会在总 hit/miss/invalidation 之外区分：

- `member_cache_monomorphic_hit_count`：1 项 PIC 命中；
- `member_cache_polymorphic_hit_count`：2 至 3 项 PIC 命中；
- `member_cache_megamorphic_hit_count`：4 项 PIC 命中；
- `member_cache_meta_fallback_count`：meta get/set 在缓存未命中后进入动态解析。

profile 关闭时这些计数通过现有 `ZR_UNLIKELY` 宏保护，不分配缓存或额外对象。

## Design And Rationale

`memberVersion` 历史上同时承担对象值写入版本和 prototype 结构版本，导致缓存校验粒度偏粗。shape 字段作为明确的身份/代数合同，允许后续把“实例值覆盖”与“结构改变”分离，同时不破坏仍读取 `memberVersion` 的旧路径。当前实现保留 memberVersion 作为额外安全校验，待所有旧消费者迁移后再评估删除。

PIC 容量从 2 提升为固定 4，覆盖小型多态调用站点，同时仍有硬内存上限。没有引入动态 cache 表、shape transition heap graph 或反向 specialization。

## Edge Cases And Constraints

- `shapeId` 是进程内身份，不是持久化或跨进程 ID；模块/跨域对象只依赖 prototype 指针和 generation 合同。
- prototype 的值覆盖也会推进 generation，因为方法/静态成员目标可能改变，即使 key 集合不变。
- 实例值覆盖不改变 prototype shape；精确 receiver pair 快路径直接读取当前 pair 值。若无关的 prototype mutation 推进了 generation，同一 object/pair 身份命中会刷新 slot 元数据，避免把安全的实例字段访问误判为失效。
- slot 由 GC 作为 function 的引用边处理，prototype、receiver object、callable 和 member name 写入时保留 write barrier。
- `object_field_hot` 的确定性 Callgrind 总 Ir 从 `205,647,828` 经 dispatch helper 的 `159,970,049` 降至 `126,716,379`（累计 `-38.38%`）。最终 17 样本墙钟 CV 为 `16.52%`，所以没有宣称 `object_field_hot` 或 `mixed_service_loop` 的吞吐门禁通过。

## Test Coverage

`test_object_shape_transition_cache.c` 覆盖非零且稳定的 shape ID、descriptor/super/meta mutation generation 递增、四项 PIC 填充，以及 mutation 后旧代数不再被接受。member access 回归还覆盖精确 receiver pair 在无关 prototype mutation 后继续命中并刷新 generation，以及 4-slot 环形替换次序。

当前 GCC 11.4 Release、Clang 14 Debug 和 MSVC 19.44 Debug 分别通过 shape `3/3`、member access `102/102`、frame slot `13/13` 和 GC `67/67`。Clang 14 ASan/LeakSanitizer 在 `detect_leaks=1` 下通过同一组测试且没有报告。

## Plan Sources

实现对应 `docs/plans/benchmark/optimize/03-memory-object-gc.md` 的 Task 3，属于 shape/cache 的语义安全基础层。Callgrind 已给出可重复的累计 `-38.38%` 总 Ir 证据，但性能接受条件要求稳定墙钟数据；`object_field_hot` 至少 20% 与 `mixed_service_loop` 至少 10% 均保持开放。

## Open Issues Or Follow-Up

1. 在低噪声、CV 小于 5% 的同 scope 环境重跑 `object_field_hot`，并执行 `mixed_service_loop`，确认 4 项 PIC 的独立收益和 RSS 上限。
2. 增加可运行的 GC move、跨模块对象和动态 meta fallback binary tests；当前源码路径已纳入 shape 校验，但新增测试尚未覆盖完整模块装载。
3. 如果 profile 数据证明 `memberVersion` 已无独立消费者，再单独提交删除旧版本字段的兼容清理。
