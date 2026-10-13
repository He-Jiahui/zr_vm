# JIT 契约审查批次（2026-10-13）

## 范围、审查依据与文件身份

本批关闭 `zr_vm_jit` 四个首方文件：`zr_vm_jit/CMakeLists.txt`、`zr_vm_jit/include/zr_vm_jit/backend.h`、`zr_vm_jit/src/orc_backend.cpp`、`zr_vm_jit/src/jit_state_maps.cpp`。我逐段阅读了实现与失败/释放路径，检索了公开 facade 的直接调用、descriptor 回调绑定及 core dispatch，并核对顶层 CMake 开关与可选测试入口。SSA、parser、core、LSP 仅在确认 JIT 边界和调用契约所需处只读检查。

本次修改前 SHA-256（由 root 登记）：

| 文件 | 修改前 SHA-256 |
| --- | --- |
| `zr_vm_jit/CMakeLists.txt` | `A3F6BD38B36D70513E0A130968D7F08B4097CD2DFDB1636500BE1312E7E35F39` |
| `zr_vm_jit/include/zr_vm_jit/backend.h` | `04EFF5999B62AF4D0B0B44613480B76B49C7CA4BA5F3361278ACCBC2E8071AC9` |
| `zr_vm_jit/src/orc_backend.cpp` | `A2F43930560DB297EBF88E305725549145B07018C6C6D01F2568E054EA8482C0` |
| `zr_vm_jit/src/jit_state_maps.cpp` | `801B8368E6BAE56EE848BEA99A774D2A1182366794765B52D3341E23DF706470` |
| `docs/code-review/coverage/zr_vm_jit.tsv` | `A10A948903CFEFB3989636F546096373BB0B29440874928DC31FB16F082AC194` |

四个源码文件仅增加或改写注释。root 使用修正后的 Clang raw-token 比较核实：header 和两个 C++ 源文件的非注释 token、行首标志一致，CMake 非注释行一致；函数签名、宏定义、表达式、预处理行为、语句与控制流未改变。没有修改 JIT 测试源码。

## 构建与测试入口

顶层 `ZR_VM_ENABLE_HOST_JIT` 默认关闭，启用后才在 `CMakeLists.txt:62-71,110-112` 加入 JIT 子目录。可选 facade 测试在 `tests/cmake/ssa-tests.cmake:1338-1352` 注册，并由 `tests/CMakeLists.txt:722` 纳入；core-only 的 `ssa_host_baseline_jit` fixture 在 `tests/cmake/ssa-tests.cmake:1324-1334` 单独注册。

root 在同一个 MSVC Debug 构建树、实际 checkout 上执行修改前和修改后验证。配置为 Debug、C++17、启用断言且没有 ORC provider。修改前 configure/build/CTest 均退出码 0；optional 测试为 1/1 通过，运行约 40.88 秒。修改后 build 与 CTest 均退出码 0；optional 测试为 1/1 通过，测试用例 40.78 秒，CTest 总耗时 41.09 秒。修改后 build 命令耗时 86.453 秒、CTest 命令记录耗时 82.359 秒。结果 JSON 与 stdout/stderr 日志保存在 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`，包括 `jit-before-r2-results.json`、`jit-after-results.json`、`jit-build-after.stdout.log` 和 `jit-test-after.stdout.log`。

MSVC 修改前日志已在 `orc_backend.cpp` 报 C4819（代码页 936 不能表示 UTF-8 字符）；修改后同类告警还出现在新增中文注释所在的 `(615)` 与 `(1205)`。告警类别在 before 已存在，after 构建/测试通过；源码维持 UTF-8，未加 BOM，也未改编译选项。

可选测试通过 `ZrJit_Host_GetDescriptor` 获取 descriptor，并直接调用 `descriptor.vtable.queryTarget`（`tests/core/test_ssa_host_jit_optional.c:93-105`）；它没有把该 descriptor 注册到 core execution backend service。测试覆盖注册和能力快照、单个缺失 `debugEntryCount` 负例、导入校验、允许 fallback 的有效请求、mapped prepare/publish/acquire、入口地址为零、evict、lease 阻止关闭、release/collect 和最终 shutdown（同文件 `:137-224`）。它没有覆盖 map hash 不一致或 layout hash 仅高 32 位不同的情形。测试中把 `publication.target.platform` 临时设为 WASM 后，Register 仍使用原 HOST options，且已有注册实例，因此该段不能证明 WASM 注册被拒绝（`tests/core/test_ssa_host_jit_optional.c:175-180`）。

## 已闭合的源码及测试单元清单

`docs/code-review/coverage/zr_vm_jit.tsv` 按规范九列记录 137 个当前单元：124 个 JIT 生产源码单元，加上 optional facade 测试的 6 个函数和 7 个重要场景块。每行的 caller/evidence 使用当前 `path:line` 锚点；外部 ABI 无仓内直接调用时明确标记并指回声明/间接入口。旧的 122 行仅用于发现候选单元，没有直接沿用旧结论。

| 文件 | 单元数 | 覆盖内容 |
| --- | ---: | --- |
| `zr_vm_jit/CMakeLists.txt` | 12 | LLVM 探测选项和结果、target/alias、编译特性/定义/include/警告、可选链接、core 目标选择和 install |
| `zr_vm_jit/include/zr_vm_jit/backend.h` | 46 | include guard、17 个公开值/掩码/兼容 alias 宏、状态类型与别名、4 个结构体及 21 个 C ABI 声明 |
| `zr_vm_jit/src/jit_state_maps.cpp` | 5 | 4 个 helper 与导出的图声明校验器 |
| `zr_vm_jit/src/orc_backend.cpp` | 61 | 8 个 callback 前置声明、2 个内部类型、全体 helper 与 20 个 facade 定义、2 个 lambda 谓词和 4 个独立生命周期语句块 |
| `tests/core/test_ssa_host_jit_optional.c` | 13 | 6 个函数及 descriptor、注册/能力、图校验、导入、错误平台、fallback、lease 生命周期 7 个块 |
| **合计** | **137** | |

逐单元的职责、输入/输出约束、所有权/时序、失败状态、调用证据、结论和理由均见对应 TSV 行。新增详细注释集中说明：descriptor 回调如何在锁内重查单例；core manager 对 `records.data()` 的借用期；candidate 完整构造后才发布；Prepare 记录创建与 proof 分配失败回收；Collect 后才删 proof；TryShutdown 在 lease 或未清记录时保留借用数组；Compile 的 fallback 双重门禁。头文件为所有公开类型和 21 个 API 增补 Doxygen 契约，CMake 注明 LLVM package 探测不等于 provider 能力。

### Optional facade 测试的逐单元审查

测试文件只读审查，没有改测试源码；六个函数已有中文合同注释，七个场景块也有紧邻说明，故测试行结论为 `no-comment`。这些结论只表示现有说明覆盖该测试单元，不把断言外推成未构造场景的证据。

| 单元 | 调用方/入口 | 前提、所有权与断言范围 | 结论 |
| --- | --- | --- | --- |
| `test_host_architecture` (`tests/core/test_ssa_host_jit_optional.c:9`) | `test_target:25` | 从编译器宏选 x86-64/AArch64；其他架构返回 NONE；不执行跨架构逻辑。 | 注释充分，无需新增 |
| `test_target` (`:21`) | `test_publication:40`、`main:131` | 返回按值构造的目标；指针宽度取当前进程，little-endian/hash 固定，不做系统探测。 | 注释充分，无需新增 |
| `test_publication` (`:37`) | `main:118,183` | 按值构造固定 publication/map 摘要；元数据不拥有代码，也不证明入口存在。 | 注释充分，无需新增 |
| `test_maps` (`:66`) | `main:117,155` | 每类 count/hash 为一/非零；只测试声明字段，不读取真实图。 | 注释充分，无需新增 |
| `test_descriptor_unavailable` (`:86`) | `main:146` | 先注册后取 descriptor，直接调用 `queryTarget`；不调用编译回调，只验证匹配 target 的不可用状态。 | 注释充分，无需新增 |
| `main` (`:112`) | CTest `tests/cmake/ssa-tests.cmake:1351` | 由 optional 开关控制；用断言跑完整 facade 顺序；fallback 不执行 ExecBC。只含一个 missing-debug-count 负例。 | 分块说明充分，无需新增 |
| descriptor `queryTarget` 直接回调块 (`:92-107`) | `main:146` | 校验 descriptor 后使用其原 target 调 callback，只覆盖匹配 target 时 BACKEND_UNAVAILABLE；未覆盖 mismatch/高 32 位诊断。 | 函数注释说明边界，无需另加块注释 |
| 注册与能力查询块 (`:135-146`) | CTest `ssa-tests.cmake:1351` | 断言 singleton 注册、machine-code=false、QueryInfo 与 descriptor；注册成功不等于可执行能力。 | 紧邻注释充分 |
| 状态图校验块 (`:148-155`) | 同一 CTest 入口 | 完整声明接受，仅把 `debugEntryCount` 置零测试拒绝，再重建输入；无 hash mismatch case。 | 紧邻注释充分 |
| Import manifest 块 (`:157-173,178-179`) | 同一 CTest 入口 | 有效 ID/签名被接受，单独错误签名或 ID 被拒绝，空 manifest/参数也有一例；未覆盖所有格式错误。 | 紧邻注释充分 |
| WASM 注册意图块 (`:175-183`) | 同一 CTest 入口 | 实际只改 `publication.platform`，Register 仍用原 HOST options 且单例已注册；拒绝断言不能证明 WASM 目标校验。 | 原 TODO 保留，测试说明充分 |
| fallback 请求块 (`:185-200`) | 同一 CTest 入口 | 一份有效且允许 fallback 的请求被验证并返回 FALLBACK_EXECBC，输出 record 为空；未执行解释器，也无 hash 错配。 | 紧邻注释充分 |
| record/lease 生命周期块 (`:201-224`) | 同一 CTest 入口 | 使用独立的 `prepared`/`active` 槽，验证 Publish/Acquire/Evict/Collect；lease 使 TryShutdown 返回 ACTIVE_LEASE，Release 后回收再关闭；入口地址为零。 | 紧邻注释充分；不覆盖 lease 槽复用 |

测试 block 的调用方都是 CTest 的 `ssa_host_jit_optional` 命令（`tests/cmake/ssa-tests.cmake:1351`，测试片段由 `tests/CMakeLists.txt:722` 纳入）。core-only baseline fixture 只验证 core manager，不计作 JIT facade 调用或上述测试单元的覆盖证据。

## 调用、所有权和失败路径证据

在仓内检索到的 facade 注册及记录生命周期直接调用位于 `tests/core/test_ssa_host_jit_optional.c:137-224`；`Prepare` 当前没有仓内调用。descriptor 回调由 `fill_descriptor` 在 `orc_backend.cpp:555-562` 绑定，core 的 dispatch 点可在 `zr_vm_core/src/zr_vm_core/execution/execution_backend.c:682,824,1217` 与 `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:94-105,476,569` 追到。仓内可选测试只直接调用 `queryTarget`，没有通过 core service 注册该 JIT descriptor。

`HostImpl` 拥有配置副本、记录向量、proof 向量和 manager wrapper；manager 借用固定的 `records.data()`，singleton 发布发生在 vector 与 manager 初始化成功后（`orc_backend.cpp:120-145,1295-1317`）。全局互斥量保护 singleton、manager 和 proof 的共享访问；持锁取得的裸 `HostImpl *` 不得越过锁保存。`prepare_locked` 先调用 core Prepare，再追加 facade proof；若 proof 分配失败，会尝试 Evict/Collect、清零输出并报 CAPACITY，但忽略清理调用结果，不能据此承诺 rollback 成功（`orc_backend.cpp:999-1045`）。proof 在对应记录非 FREE 时保留，core Collect 后再删除（`orc_backend.cpp:1066-1085,1350-1355,1616-1619`）。

core 公开契约规定 `Code_Prepare` 输出不得带 active lease，`Code_AcquireActive` 不得覆盖已有 lease（`zr_vm_core/include/zr_vm_core/host_baseline_jit.h:359-385`）。facade Prepare/PrepareWithMaps/Compile 会在校验前清零输出，Acquire 成功后把新 lease 复制到调用方槽位（`orc_backend.cpp:1464-1509,1543-1565,1644-1651`）。公开声明现要求使用空的独立输出槽；仓内测试分别使用 `prepared` 和 `active`（`tests/core/test_ssa_host_jit_optional.c:121-123,203-207`）。缺乏合法仓内复用 lease 作为输出的调用证据，因此只保留“调用前提/外部调用需确认”的 TODO，不认定为 API 缺陷。

## 已证实问题与待核入口

- **BUG：queryTarget 的 64 位 layout hash 诊断被截断。** 比较使用完整 `TZrUInt64`（`orc_backend.cpp:656-658`），但 mismatch 分支把两值转为 `TZrUInt32` 并交给参数同为 32 位的 helper（`:588-597,660-664`）；core backend diagnostic 的 `expected`/`actual` 为 64 位（`zr_vm_core/include/zr_vm_core/execution_backend.h:195-207`）。可达路径是成功注册后获取 descriptor，再调用 `queryTarget`，传入仅高 32 位不同的 HOST_JIT target：分支返回 CONTRACT_MISMATCH，但诊断只保留相同低位，丢失区分信息。GetDescriptor 的 callback 绑定为 `:555-562`，仓内测试已证明直接调用入口 `tests/core/test_ssa_host_jit_optional.c:93-105`；Root 独立复核了 Register -> GetDescriptor -> callback 可达路径。当前测试未构造高位差异输入。本批只记录问题，没有加测试或改变行为。
- **TODO：ValidateCompileRequest 与 PrepareWithMaps 的 map hash 检查不同。** validator 验证图字段并只比较 `frameLayoutHash`（`orc_backend.cpp:1199-1252`，特别是 `:1243-1249`）；`prepare_locked` 对照四个 map hash（`:999-1017`）。非零但不同的 map hash 可通过 validator 后进入 fallback。当前公开契约没有证明 fallback 必须拒绝此类差异，故旧台账中的 BUG 降为 TODO。核实 fallback 请求契约后再判断是否统一验证。
- **TODO：Prepare 的 target 与注册 target 是否必须相同。** `prepare_locked` 将 facts 交给 core 校验，但不与 `impl.options.target` 比较（`orc_backend.cpp:999-1021`）；Compile 明确逐项比较请求 target 和注册 target（`:1657-1676`）。须确认 Prepare/Publish 的公开 target 身份约定及未来 provider 的绑定要求，不能仅凭 Compile 的比较推断 Prepare 已违反契约。
- **TODO：core 状态到 facade 状态的映射。** `map_core_status` 保留原始 coreStatus；未显式映射的 ABI_MISMATCH/LAYOUT_MISMATCH 会走 INVALID_STATE 默认分支（`orc_backend.cpp:327-428`）。需要结合 Register 和 validation 的可达输入确认应采用的稳定 facade/ExecIR 诊断类别。
- **TODO：输出句柄生命周期。** wrappers 覆写或清零输出槽，但没有检查旧槽是否代表 live lease（`orc_backend.cpp:1464-1509,1543-1565,1644-1651`）。core 的调用契约禁止这种复用，且仓内调用使用不同槽；先核实 facade 对外前置条件和合法外部调用，再判断是否需要防御策略。
- **TODO：诊断 operation 字段。** `SZrJitHostDiagnostic.operation` 已公开，但实现初始化会清零、未有 JIT 写入（`backend.h:151-162`、`orc_backend.cpp:217-245`）。确认字段是保留位还是应由特定验证入口填充。
- **TODO：ORC 配置门禁。** `ZR_VM_JIT_HAS_ORC` 从 LLVM package 探测设置并作为私有编译定义传入；当前源码不读取该宏，target 也未链接 ORC/JITLink（`zr_vm_jit/CMakeLists.txt:14-28,38-40`）。provider 接入时同时核查实现、链接和能力报告。
- **TODO：便捷 Prepare。** `ZrJit_Host_Prepare` 为四类图合成 count=1，没有仓内 caller（`orc_backend.cpp:1461-1488`）。接入真实 provider 前确认此入口是否仍可参与发布，或应只接受 `PrepareWithMaps` 的实际声明。

旧台账中 `map_core_status` 和 `prepare_locked` 的 TODO 与源码证据一致。queryTarget BUG 保留，因为完整比较、窄诊断字段与公开可达 callback 路径均有当前源码支撑。其他结论没有把静态差异扩大为未证明的 API 缺陷。


## 最终验证凭据与反向引用

Root 对当前冻结源码在同一自有构建树执行 MSVC Debug clean-first 定向重编译和 optional CTest。最新构建退出码为 0（10.266 秒），日志确认两个 JIT C++ 源和 core host 管理器实际重新编译；CTest 退出码为 0，`ssa_host_jit_optional` 1/1 通过（0.55 秒，CTest 总计 0.56 秒）。修改前的同配置 configure/build/test 均退出码为 0，optional CTest 1/1 通过。最新命令、耗时和运行前后源码 pin 记在 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/jit-summary-recompile-after-results.json`（SHA-256 `A9267537A59D4EE96517615BD4DFEE564FA9DDECBCABEBF9EDA78B4EA7192BE5`）；终端输出为同目录 `jit-build-summary-recompile.stdout.log`、`jit-test-summary-recompile.stdout.log`。此前增量构建返回 no-work，因此另行确认最新 header 进入编译；旧报告保持原版本，不能用旧 header pin 代表当前源码。

最终源码与测试固定哈希为：`backend.h` `9b7a072806af518d6860a6d313f60f31c46f571d85ec8dbdd0370b5ee65eaf82`，`orc_backend.cpp` `3463c59087b82924058b7e3267f43e15da1bf568a51cc3dbfc827f9d8efb42d0`，`jit_state_maps.cpp` `e043c0e236a08a82c90c6dfe9a55961ecb8d2c414bd5ea1195dd36ac9dfd3e7c`，`CMakeLists.txt` `3408367e45617825bbf6ec751e97eeeb30e51c93e00f51ac4bc28576ad38b7bd`，未改动的 optional 测试 `76ea51cd4081c42a992c0fc14024f812098732b961a5495726108e6c46ab5bd9`。`jit-summary-comment-equivalence.json`（SHA-256 `112F6909A38BD9C6F245D8C2063104B84BDE9F487829D26AD59FB94296AFCD04`）记录了 header 与两个 C++ 文件的非注释 token 和行首标记相同、CMake 非注释行相同。

最终 MSVC 输出仍有 C4819（当前代码页 936 无法表示中文注释字符）；修改前基线已出现同类警告，定向构建驱动没有传入 `/utf-8`。警告未阻断构建，前后源码非注释 token 等价。

Root 另将 `root_build_ci.tsv` 的 7 个、`zr_vm_core_execution_backend.tsv` 的 70 个、`zr_vm_core_host_baseline_jit.tsv` 的 55 个引用更新到本批 JIT 新锚点。这些是既有台账对 JIT 的反向行号修正，不计为本批新增语义覆盖，也不代表本批重新审查了整张 core/CI 台账。


## 最终逐标签对应

独审后将所有源码 TODO 明确关联到所属台账单元，仍为137行：Diagnostic.operation 的公开意图、StateMapFacts 与 validator 的四图摘要差异，以及 Prepare/PrepareWithMaps/Acquire/Compile 的输出槽前提分别在类型或函数行登记。已有摘要问题没有增加新的 BUG；唯一 BUG 仍为完整可达的 query_target 高位诊断截断。

| 源标签位置 | 逐单元台账关联 | 待核内容 |
| --- | --- | --- |
| backend.h:151 | SZrJitHostDiagnostic | operation 清零但无赋值入口；核 ValidateCompileRequest/Compile 的诊断意图 |
| backend.h:180、:338；orc_backend.cpp:1243 | SZrJitStateMapFacts、ValidateCompileRequest 声明/定义、prepare_locked | fallback 是否要求四图 hash 与 publication 逐项一致 |
| orc_backend.cpp:1470、:1497、:1560、:1649 | Prepare、PrepareWithMaps、Acquire、Compile 的定义 | 外部调用是否遵守独立空槽和 lease 先归还的前提 |
| orc_backend.cpp:318、:989、:1462 | map_core_status、prepare_locked、Prepare | 诊断映射、注册目标身份及合成图摘要各自核查入口 |
| CMakeLists.txt:14 | ZR_VM_JIT_HAS_ORC 变量/编译定义 | provider 实现、链接与能力报告；当前无源码读取宏 |

CMake 配置单元把 `CMakeLists.txt:111` 的真实 add_subdirectory 作为配置入口，变量读取和 target 链接作为消费者；声明、赋值和属性设置留在 evidence。optional fixture 的 PUBLIC usage requirements 消费点是 target_link_libraries :1347，独立重复 include 设置不作消费者。

三个反向台账的数字修正另有 `jit-cross-reference-applied-v1.json`。其中两张 core 台账的 check-batch 退出0；根构建台账仍有22条其他路径的既有失效锚点，`jit-root-build-anchor-baseline.json` 验证修改前后的错误列表相同且无 JIT 锚点错误，不声称根构建台账整表通过。

公开声明与类型文档已逐项核对 Markdown 小节。最后在 27 处原 Doxygen `@brief` 前补显式 `Summary` 标题，原职责正文和全部程序 token 保持；void 的 Returns 按规范说明副作用与失败后状态，不虚构返回值。受影响的 header 数字引用已按插入位置重锚；早于首个插入位置的引用保持原行。
