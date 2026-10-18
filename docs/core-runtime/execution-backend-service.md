---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_jit/src/orc_backend.cpp
  - tests/core/test_ssa_backend_service.c
  - tests/core/test_ssa_host_jit_optional.c
  - tests/cmake/ssa-tests.cmake
  - zr_vm_core/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - tests/core/test_ssa_backend_admission.c
  - tests/cmake/ssa-backend-tests.cmake
  - tests/cmake/ssa-backend-admission/CMakeLists.txt
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
plan_sources:
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - "user: 2026-09-14 SSA 10.01 backend service contract implementation"
  - "user: 2026-10-04 caller-first 中文意图注释与当前调用契约核查"
  - "user: 2026-10-18 backend owner admission, module split and precise generation diagnostics"
tests:
  - tests/core/test_ssa_backend_service.c
  - tests/core/test_ssa_host_jit_optional.c
  - tests/acceptance/ssa-backend-service.md
  - tests/core/test_ssa_backend_admission.c
  - docs/testing-and-validation/ssa-backend-admission.md
doc_type: module-detail
---

# Execution Backend Service

## 目的与当前消费边界

此服务把后端注册、编译票据、发布和代码寿命放到同一个 core C 契约中。
宿主提供服务及固定容量数组，决定何时提交、调度、完成和关闭；服务没有
隐藏分配器或工作线程。编译请求复制的是标量身份、契约和 hash 见证，不保存
LLVM 类型、解析器对象或可移动 VM 对象地址。固定容量使接纳失败可显式返回
`CAPACITY`，代码入口只通过运行期租约查询取得，不能持久化为契约身份。

当前已核实的服务调用链来自 `test_ssa_backend_service.c` 的 C mock：构造
descriptor、注册、提交，再显式调用 `ProcessNext`/`Complete`。可选 ORC facade
有可取得的 descriptor，但其 `compile_async` 当前返回后端不可用，不生成机器码；
`test_ssa_host_jit_optional.c` 直接查询 descriptor 的目标回调，不证明把它注册到
本服务的真实宿主链。AOT/ExecBC 回退状态只表达选择结果，服务没有执行回退程序。
不能由名称、构建收集或公开 ABI 推断 CLI、生成 C 或 LLVM 已经消费这些服务入口。

## 文件职责与存储所有权

| 文件 | 契约职责 |
| --- | --- |
| `execution_backend.h` | 公开 C ABI：标量请求、descriptor、票据、代码记录、借用视图与租约 |
| `execution_backend_internal.h` | 服务形状、完整键、查找和锁内回调计数等内部共同前提 |
| `execution_backend.c` | 注册、状态/键 helper、取消、失效、服务初始化/关闭及现有 Resume 转发 |
| `execution_backend_compile.c` | 请求接纳、candidate/回退、队列、ProcessNext/Complete/Publish/QueryTicket |
| `execution_backend_facade.c` | 默认借用指针、默认锁与短 facade；无新增指针保活 |
| `execution_code_handle.c` | 代码/图查询、两类租约、退役收集及未发布结果处置 |
| `execution_contract.h` | 执行 ABI、布局、签名、模块与效果等标量契约 |

`Init` 清零宿主传入的服务和三个数组；数组及 service 存储一直归宿主。
`Register` 复制 descriptor，函数指针与 `userData` 仍是运行期借用。
Register 要求 descriptor/target 的 reserved 为零；请求和结果的 execution
contract reserved0/reserved1 也须为零。其他 service/job 内部保留字段仍不因此
获得公开状态或寿命含义。注册资源销毁由 `destroy` 回调完成；
服务不释放宿主数组。`Deinit` 只在已完成最终关闭时清空服务壳的借用信息，宿主
必须独占该操作并自行释放存储。

默认 facade 借用显式安装的 service。默认锁只保护指针读写，不能保活返回的
service，也不能替宿主协调替换与关闭。仓内尚未发现安装/解绑及这些 facade 的
实际上游；公开 ABI 的仓外使用未知。

## 接纳 owner、版本分域与准确诊断

backend schema 当前为1；请求所带 execution contract 的 schema/logical 当前为
6/1。这是服务接收者的支持政策，不是把 generic `execution_contract.c` 的纯相等
checker 改成 current-only。服务没有逻辑版本协商。ABI 要求非零并匹配实际选中的
registration；layout 同样参与 candidate 匹配。execution/AOT ABI 宏17不是所有
backend 的唯一版本，admission 正例使用23。

Register 保留原 magic/schema/flags/target/operation/必需回调错误在前，全部原验证
通过后才拒绝 descriptor/target reserved。CompileAsync 保留参数、请求
magic/backend schema、flags、immutable 的优先级，再报告 contract schema、
logical 或 reserved 不符。Complete 保留票据/取消/状态及 generation 优先；
错误结果在建立 READY 前进入 FAILED，沿原 owner 的 U→R 处置路径清理。

诊断 expected/actual 给出首个契约差异的真实标量，expectedState/actualState 用于
状态和掩码；source/instruction 与结果 ticket/code identity 保留。完整
generation key 不等时仍报告原 key-generation 载荷；只有完整键相等而
contract.generation 不等时，使用 contract 的 expected/actual。请求1/结果2
现在准确报告1/2，避免把两个相等 key 的1/1写成差异。NULL diagnostic 不改变拒绝
或清理语义。详细优先级、固定槽恢复与实际 RED/GREEN 证据见
[admission 验证页](../testing-and-validation/ssa-backend-admission.md)。

## 编译票据与结果协议

### 四元身份与接纳

域、模块、代次、后端注册身份共同构成 generation key。完整键要求四项非零；
编译请求选择注册项之前允许注册身份为零，选择后写入复制的请求和票据。
失效比较完整四元值，不能按一个 generation 数字跨域退役。

请求声明 immutable input，并带 IR/输入 hash、源位置、所需操作及图位。
当前服务核对这些标量见证及已知位，不读取 IR 本体，也不验证图扫描正确性。
后端仍须证明实际输入稳定、代码 ABI 正确和平台图注册可用。

### 提交与显式调度

```text
FREE -> QUEUED -> COMPILING -> READY -> PUBLISHED
          |          |          |
          +-> FAILED +-> FAILED +-> CANCELLED
          +-> CANCELLED
```

`CompileAsync` 选择匹配的注册项并先复制请求到 `QUEUED` 槽，然后在提交线程中
解锁调用 `queryTarget`。它不直接调用编译回调，但查询回调可以阻塞，所以此入口
不保证帧线程非阻塞。回调计数保护已经进入的目标查询；排队早于查询返回，宿主
若并发调度，必须核对查询失败与 worker 抢先处理的协议，当前 mock 没有覆盖该交错。
查询失败只有仍处于 `QUEUED` 的槽被标为失败。

宿主调用 `ProcessNext` 才把首个排队槽置为 `COMPILING` 并在锁外调用
`compileAsync`。回调 invocation 中的指针借用调用栈，异步后端必须自行复制。
同步成功转交 `Complete`，`PENDING` 表示后续完成；失败的非零代码身份会进入
未发布结果处置。服务不建立 worker，也不替后端排队。

目标/操作不可用时，许可标志可返回 `FALLBACK_AOT` 或 `FALLBACK_EXECBC`；
要求机器码禁止回退，AOT 许可优先于 ExecBC。回退票据是 `FREE` 的选择结果，
没有可供该服务继续查询的有效 service/ticket 身份；诊断仍携带原选择失败原因。

### 完成、查询与发布

`Complete` 仅安装匹配仍在编译的票据、完整键、契约、hash、非零代码身份和尺寸、
所需图位及对应非零 hash 的结果。非 FREE 记录中的重复代码身份也拒绝。
接纳成功复制标量代码元数据到 `READY` 记录；图注册位是后端声明的见证。
失败/取消/过期结果不会发布，符合可路由条件时交给原后端的未发布处置回调。
槽被复用后的过期票据只有仍能匹配活跃注册项，才可找到处置所有者。

`QueryTicket` 返回锁下快照；调用方持有的 ticket.state 不是实时状态。
失败或取消槽可复用，旧票据之后可变成 stale。代码容量失败会使服务内槽失败，
但不更新输入票据的 state，因此应以查询结果为准。
`Publish` 单独发布 READY 代码，同 targetToken 和完整键的旧发布记录退役；
旧租约仍保护其代码，发布本身不等待这些租约结束。

## 借用视图、执行租约与图依赖

`QueryCode` 返回代码元数据、状态和计数的借用快照，不能据此独立保活资源。
`AcquireCode` 对已发布票据取得执行租约并建立带 service/槽/代码/完整键见证的
handle。复制 handle 的字节不会增加计数；不能覆盖尚未释放的租约输出。
已有有效租约可继续查询退役记录；新取得租约只允许已发布记录。
宿主须协调同一 handle 的查询与释放，让执行 lease 覆盖入口使用期。

`AcquireDependencyLease` 在执行租约上另增图依赖；`ReleaseDependencyLease`
只释放图依赖，执行租约仍在。`ReleaseCode` 在该记录还有任何图依赖时拒绝，
包括另一 handle 持有的依赖；成功释放不自动进行退役收集。

`LookupEntry` 复制元数据并在锁外调用后端，入口地址的寿命由执行租约约束，
不是由回调计数永久保活。`QueryMap` 同样借用有效 handle；缺少查询回调时返回
记录中的非零 hash。两者都不证明地址可执行、调用约定正确或图内容具有 GC 安全性。
回调计数保护 descriptor/userData，不是额外代码租约；`QueryMap` 也不自动增加
dependency lease。失败回调可能留下输出值，失败输出不可使用。

`CollectRetired` 只处理两类租约均为零的 RETIRED/RETIRE_FAILED 记录：先置
RECLAIMING 并计数保活，在锁外注销图；注销成功才请求 retire。成功后释放记录
与对应槽；失败留下可重试记录，记住图是否已注销。已有收集数量可在后续失败时
保留。未发布处置的规则不同：即使注销失败也尝试 retire，且不创建可重试代码记录。

## 失效、恢复与关闭

`InvalidateGeneration` 先登记完整键的固定容量墓碑，再取消排队/READY 票据、
退休 READY/PUBLISHED 代码，对编译中票据记取消意图并在锁外请求取消。
墓碑阻止后续同键提交；表满在改变这些状态之前返回容量失败。
取消回调失败或返回 PENDING 不等于资源已经终止；失效成功也不保证回调全部完成。

`ResumeInterpreter` 在锁外借用转发 request 与诊断，缺少回调返回明确诊断。
它不验证恢复内容，也没有单独拒绝 shuttingDown/destroyed 的检查。
当前 mock 只在关闭前串行恢复并转发 sourceId 诊断，不执行真实解释器。

`Shutdown` 停止注册、提交和发布，取消排队、请求编译中取消，并退休代码。
它重读编译槽、目标查询和其他后端回调计数，仍有工作时返回 IN_FLIGHT；
返回 OK 仍可能存在代码租约。
`FinalizeShutdown` 要求已进入关闭、无上述工作，再收集退役代码；有记录尚存
则不能销毁注册项。满足条件后在锁外调用 destroy，清空票据并标记 destroyed。
宿主最后独占 `Deinit`，先解绑默认指针，再释放数组和 service。

## 线程限制与具体 TODO

短自旋锁保护元数据，回调均在服务锁外执行，以允许后端查询状态并避免持锁回调。
回调计数只保护已成功进入计数区间的操作，不能推断所有跨 unlock 的路径已连续保活。

- TODO：从 `Complete` 拒绝结果的 unlock 到
  `zr_execution_backend_dispose_unpublished` 再取得回调计数之间，核对宿主与
  `Unregister`/`FinalizeShutdown` 的调度和 descriptor.userData 寿命交接。
  当前串行 mock 未构成合法并发失败证明，不能把此疑问记成已证实 BUG。
- TODO：在宿主接入 `CompileAsync`/`ProcessNext` 时明确目标查询与 worker 的
  并发顺序，并验证队列已可见但查询尚未返回时的失败后置状态。
- TODO：接入 `ResumeInterpreter` 时核对恢复与最终关闭的互斥或保活协议。
- TODO：补充公开 helper、`Cancel`、`Unregister`、默认 facade 安装/解绑及
  `Deinit` 的实际宿主链；仓内无正调用不表示公开 ABI 无用。

## 当前测试、构建事实与证据范围

`test_ssa_backend_service.c` 是使用 assert 的单线程 C mock，七个场景覆盖显式
排队与同步/PENDING 完成、发布、图/入口查询、两类租约、取消/墓碑失效、过期和
重复结果处置、跨域隔离、恢复诊断及关闭销毁顺序。入口哨兵没有实际执行，恢复
内容没有验证；这些断言不能证明多 worker 调度、真实机器码、真实解释器或全 VM GC。

当前 `tests/cmake/ssa-tests.cmake` 调用共享的
`tests/cmake/ssa-backend-tests.cmake`，注册 service 与 admission 两目标。
每目标显式编入自己的测试 TU，以及 backend.c、backend_compile.c、backend_facade.c、
code_handle.c 四个生产 TU；private header 复用共同 helper。standalone
`tests/cmake/ssa-backend-admission/CMakeLists.txt` 也使用同一列表，
形成10个编译角色与两个 CTest。service 的5个角色显式 /UNDEBUG 或 -UNDEBUG，
生成 argv 中不能被后置 NDEBUG 覆盖；574行 admission 使用始终生效的 CHECK。
core 常规模块仍经 CommonMacros.cmake 收集 src C；可选 host-JIT 仍单独由开关控制。

2026-10-18 Root已实际运行当前有限 GREEN：四阶段0/0/0/0、admission33行全部PASS、
service七场景通过、50输入不变、五个service断言角色有效、进程和handle闭合。
本轮复用前轮构建，仅重编两个compile.c角色并重链两个程序；不能将10条当前
compile_commands称为本轮10次fresh。独立GREEN审查221A4D88已接受该有限结果，详见
[实际阶段、收据和限制](../testing-and-validation/ssa-backend-admission.md)。
这组有限运行结果不代替整仓或真实 provider 验收。

补充2026-10-18实际 MSVC 兼容性检查：六个唯一 C 文件（两个 fixture 及四个生产 TU）
均以 /Zs /W4 /WX /UNDEBUG /showIncludes 自然exit0，50输入未变，输出目录无文件。
独立语法审查394EB90F已接受。实际本地依赖闭包38个头/def，与Ninja逐角色一致；
原50项实时guard保持原义，额外8个头由先前封定且当前匹配的Source41基线离线补足，保护并集58项。
这只是 MSVC 语法检查，不是 MSVC 链接/运行、Linux 或全矩阵验收。


[历史验收记录](../../tests/acceptance/ssa-backend-service.md) 保留当时直接 GCC、
Clang、ASan/UBSan、Memcheck 和 Helgrind 的命令与结果。旧两源命令及 /tmp 路径
只描述当时环境，不是当前可直接重跑的模块列表，也不把单线程证据升级为并发验收。
具体 AOT、ExecBC、JIT provider 的真实执行、平台图/代码所有权、宿主调度与仓外 ABI
仍需各自证据。不能把另一 SZrHostJitCodeHandle/manager 家族或 ownerref 当作本服务消费链。

### 当前有限代码证据

- 注册准入：`execution_backend.c:65`；descriptor复制：`execution_backend.c:210`。
- 请求与candidate：`execution_backend_compile.c:15`、`execution_backend_compile.c:132`；
  入队：`execution_backend_compile.c:226`；目标查询并发TODO：`execution_backend_compile.c:304`。
- 完成/首差异诊断：`execution_backend_compile.c:553`、`execution_backend_compile.c:396`；
  Complete handoff TODO：`execution_backend_compile.c:641`；generation诊断修正：`execution_backend_compile.c:671`。
- 默认指针锁与借用：`execution_backend_facade.c:4`、`execution_backend_facade.c:28`。
- 未入表产物清理：`execution_code_handle.c:59`；入口/图查询：`execution_code_handle.c:419`、
  `execution_code_handle.c:497`；回收：`execution_code_handle.c:588`、`execution_code_handle.c:709`。
- Finalize收集：`execution_backend.c:688`；Resume关闭交错TODO：`execution_backend.c:785`。
- 共享源列表：`tests/cmake/ssa-backend-tests.cmake:14`；
  聚合注册：`tests/cmake/ssa-tests.cmake:986`；admission：`tests/core/test_ssa_backend_admission.c`。
