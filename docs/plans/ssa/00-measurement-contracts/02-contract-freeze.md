---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_common/CommonMacros.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - zr_vm_parser/CMakeLists.txt
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_contract_freeze.c
doc_type: milestone-detail
status: in-progress
---

# 00.02 共享契约、语义边界与版本冻结

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 统一术语、数据所有权、能力安全边界和版本变更，消除跨子计划的歧义。

**Architecture：** ExecIR 是唯一执行语义源；ExecBC 与 AOTIR 是投影，直接 IR interpreter 只作 oracle。共享数据放 core 低层，构造/优化放 parser，后端不反向恢复语义。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M0 契约冻结；M1/M4/M5/M7 共同前置。
- 前置：[00.01 基线、性能指标与真实执行模式](../00-measurement-contracts/01-baseline-metrics.md)。
- 交付：共享契约目录、API 所属层、状态/错误枚举、序列化边界、兼容性规则和能力冻结方式。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

2026-10-15 源码核对：当前顶层工作区契约为 CallBinding schema 1、artifact schema 6、AOT ABI 17，execution contract 的 schema/ABI/logical 为 6/17/1。早期 artifact 5、AOT ABI 16 与 execution 6/17 候选的描述属于历史状态，不能作为当前兼容性结论。各版本域保持独立；canonical 宏所有者、真实生产/消费位置与纯 checker 顺序见[有限契约字典与边界矩阵](../../../testing-and-validation/ssa-contract-freeze.md)。其中 AOT ABI 宏在全仓并非唯一定义：Git 跟踪的 `zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h` 另定义 2u，AOT README 将其标为历史参考；已检查的顶层模块、parser AOTIR 和 Source direct-validation 使用顶层 common include。四页字典现已登记原68个已知路线及2个新增library头路线；D5冻结顶层17生产与nested2参考隔离。仓外/手工消费者不在观察闭包，不能推导全仓宏唯一定义。本次不合并、删除或同步两份头；字段政策已交付，完整运行门禁仍独立。

compiler_semir.c 明确是 ExecBC 之后的兼容投影，而 compiler.h 的 preSemanticIr 位于 ExecBC 之前。当前 `zr_vm_parser/CMakeLists.txt` 通过递归源码发现编译 `zr_vm_parser/src/zr_vm_parser/backend_aot` 下的后端并排除 disabled stub；早期源码位于 `zr_vm_aot` 的描述不再表示当前路径。不能因为根 CMake 没有 add_subdirectory(zr_vm_aot) 就判定 AOT 未接入。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/execution_contract.h` | execution contract、九字段诊断和公开 checker 的唯一声明；版本宏不替代其他版本域 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/runtime/execution_contract.c` | 纯契约比较与状态名；不承担解析、加载或发布 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/call_binding.h` | 复用 contract/target 分离与结构化错误 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/artifact_schema.h` | 登记序列化版本和必选 section |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 冻结 frame/native/code registration ABI |
| 现有历史参考，隔离政策已冻结 | `zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 同名 AOT ABI 宏为 2u；顶层 canonical 为 17u，已知路线按D5隔离；未知仓外使用和历史独立编译验收不在闭包，不在本批改动源码 |
| 现有，修改/复用 | `zr_vm_parser/CMakeLists.txt` | 确认后端源码所有权与低层依赖 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/exec_ir.h` | 低层无 AST 依赖的 IR 定义，细节归 01.01；本叶不另建平行定义 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/execution_backend.h` | 后端状态与运行期 handle 接口，细节归 10.01 |
| 现有测试，覆盖有限 | `tests/parser/test_ssa_contract_freeze.c` | 原 4 组保留，新增 6 组覆盖 logical 诊断九字段、边界、优先级与重试；I19/20矩阵已交付，原运行断言仍有未验证项 |

`tests/cmake/ssa-tests.cmake` 已登记 `zr_vm_ssa_contract_freeze_test` 与 `ssa_contract_freeze`，目标只编译该测试和 `runtime/execution_contract.c`。后续新增文件仍须登记到所属模块 CMake；不得重复登记既有目标。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 建立字段所有权表** 每个 token/hash/id 指定唯一生产方、读者、生命周期和 hash 输入；TypeId 运行期映射与稳定 metadata token 明确区分。

- [ ] **2. 冻结语义术语** CoreIR 统一作为 ExecIR 的旧称，不另设执行语义IR；当前严格数值仍须按源码逐操作补齐基线，09.02未来profile/vector不能替代；优化推断失败只保留合法原语义，不把非法 borrow 或静态缺失 contract 放行。

- [ ] **3. 冻结域和热更规则** 隔离域只复制对象图，不共享 GC 指针或 ownership control；Transfer 是复制事务协议。同域共享必须证明访问和生命周期安全。公开ABI不变；可热更目标跨目标内联必须经过版本入口并具备invalidation/deopt依赖，只有已证明冻结目标才可无条件内联。D1/D2/D4政策已冻结，真实接入归06.03/06.04/08.03等下游，不据文字宣称行为已实现。

- [ ] **4. 冻结版本与接口归属** 当前值、顶层 canonical 所有者及历史同名 AOT 宏冲突已作有限登记；后续变更按所属版本域评审，不能把当前 6/17 再当作待发布候选或直接推导下一版本。D4已冻结磁盘generation/进程epoch分域、唯一Core loader签发非零不复用u64 epoch及下游消费责任；registry/lease/invalidation仍planned，不能从checker比较推导已实现。I19/20逐族区分invalid IR/link/capability/deopt/unsupported/compile pending及不适用项，不另造统一枚举。

本轮纯 checker 顺序为 NULL → schema/ABI → logical → target → generation → signature → layout → module → capabilities → effects，首个失败返回；无解析、hash 重算、分配或发布。schema/ABI 失配继续使用原 schema version pair 与 ABI hash pair 载荷，并优先于同时存在的 logical 失配；仅 logical 不同时，VERSION_MISMATCH 现携带 logical expected/actual、expected functionToken，其余位置及 hash 字段为零。接受集合不变，logical 不强制等于当前宏 1，测试仍接受相等的任意值 11。当前四页已冻结owner支持集合、逐codec reserved规则、已知ABI2隔离和I19/20矩阵；generic checker仍纯等值，不据政策交付推导所有消费者已接入或完整退出门禁通过。

### 有限 logical 诊断 RED/GREEN（2026-10-15 Pacific）

真实测试与 production checker 两个 TU 已由 Windows x64 Clang-cl 19.1.5 和 MSVC 19.44.35229.0 分别 standalone 编译、链接及直接运行。原 4 组保留，新增 6 组验证 logical-only 九字段、0/UINT32_MAX 双向边界、schema/ABI 优先、后续身份失配优先级、NULL diagnostic 与重复失败后成功清空；新增组同时验证输入字节不变。RED7 在原 D7D production 上的两个 run 均精确 exit 1、stdout 空、stderr 为 `FAIL: diagnostic expected version differs\r\n`；最小 D82 logical payload 修正后，GREEN 两个 run 均 exit 0、精确输出 `ssa contract freeze PASS\r\n` 且 stderr 空。两次各 12 个实际阶段自然终态、双流排空、warning 为 0。

RED 收据 SHA256：`5850DC2E7FD275C8859E65EF568A5EC0E997F0030BD89246E3B6B42EFFD4F4B9`；GREEN：`53E4C6111EC3A365E2377AA6F15AA8042FF124B1EE39507C0407C9B52F12DC31`。实际输入 SHA、完整收据及日志路径见[有限 Windows RED/GREEN 证据](../../../testing-and-validation/ssa-contract-freeze.md#windows-standalone-redgreen-evidence)。本次不计 CTest、Linux、UBSan、Source、VM、Oracle、Global、network 或 FFI 信用，也没有完整 dirty-tree digest；00.02 整叶与 release acceptance 均保持 open。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
PersistentContract = {token, signatureHash, layoutHash, moduleHash, logicalVersion}
RuntimeWitness = {loadEpoch, generation, taggedTarget, tracedContext}
ResolvedTarget = VM | Native | AOT | HostJit
StaticLinkFailure -> structured LinkError
ValidSpeculationFailure -> baseline operation using same token/slot
ExplicitDynamicOperation -> existing dynamic semantics
CrossDomain(value) -> validateGraph -> cloneIntoDestination -> publishCopy
EffectSummary != SecurityCapabilitySet
```

GC budget 是可观测的工作预算而非硬实时承诺；没有读屏障/句柄协议时不得在半完成 relocation 中恢复 mutator。同步 API 不能偷偷变 async，协程化只发生在已允许 suspend 的调用边界。fast-math 不默认开启。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 同 token 但 signature/layout 不同 | 链接前拒绝并报告 expected/actual |
| Speculation guard miss 与 stale generation | 分别 deopt 与 link error，均无静态字符串查找 |
| 跨域 borrowed/native thread-affine handle | 复制预检拒绝或用显式 host materializer 复制独立资源 |
| 开启热更同时跨目标内联 | 缺少 invalidation/deopt 依赖时禁止内联 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

既有 CTest 名为 `ssa_contract_freeze`，可执行目标为 `zr_vm_ssa_contract_freeze_test`。后续验证在已初始化的 E 盘外部构建目录执行；下列为 WSL 示例，本次文档更新未配置、编译或运行：

```bash
cmake --build /mnt/e/cargo-targets/zr_vm/build/ssa-gcc-debug --target zr_vm_ssa_contract_freeze_test -j 4
ctest --test-dir /mnt/e/cargo-targets/zr_vm/build/ssa-gcc-debug -R '^ssa_contract_freeze$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 每个主计划接口族和持久化字段具有唯一所有者；边界歧义有明确策略；新增 API 先标 planned，禁止伪称已实现。

**失败恢复：** 契约未稳定前禁止发布新 schema；变更草案联动修改读写端和测试，不能靠接收任意版本解决兼容问题。

**文档交付：** 维护本目录的契约补充及 docs/core-runtime、docs/module-system 对应设计文档；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

历史七字段诊断与布尔 checker 草案已经由真实公开接口取代。`SZrExecIrDiagnostic` 的九字段声明、`SZrExecutionContract` 和返回 `EZrExecutionContractStatus` 的 `ZrCore_ExecutionContract_Check` 均以 [`execution_contract.h`](../../../../zr_vm_core/include/zr_vm_core/execution_contract.h) 为唯一权威；本计划不再复制第二份 C 声明。后续草案使用已有错误、分配器和容器约定，不另建平行框架。

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| Type/Layout/Signature hash | canonical metadata producer | parser/core/backend 不各算不同字段集合 |
| Runtime generation/target | loader/version manager | 只在运行期有效，不从磁盘恢复可信地址 |
| Capability allowlist | host/base manifest | effect 推导只能减少需求，不能增加授权 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 完成全字段字典：类型、宽度、有效域、hash 编码顺序、生产/消费模块；为重复术语建立唯一名称。当前四页已交付I19/20、F/旧IO/runtime与H31/C315有限去重索引和owner；opaque内层grammar、数值基线与原运行验收仍有限制，不自动完成整批。

- [ ] **批次 2：** 用现有 binding/artifact/AOT ABI 对照字段字典，按独立版本域明确兼容性和下一版改动；当前 artifact 6/AOT 17 不等于 execution 6/17 与其格式互通。

- [ ] **批次 3：** 完成兼容矩阵：源码重编、旧 artifact 拒绝、profile 失配忽略、patch contract 失配拒绝。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange identical token but different layoutHash
assert contract check fails before resolving pointer
arrange importedProfile.compilerABI differs
assert profile ignored; compilation remains legal
arrange old artifact schema
assert RECOMPILE_REQUIRED; no partial module published
```

### 迁移结束检查

公共接口归属必须写清：Build/Optimize/Lower/ReadWrite 的实现由 parser 驱动，低层结构/verifier/runtime resolution 在 core；不要让 core include parser AST。上述是责任边界目标，具体读写入口仍须逐项对照真实实现。exec_ir.h/execution_backend.h 已存在，细节分别归 01.01/10.01，本叶不重复创建；四页提供声明分母内接口/字段唯一owner与边界政策；有限H/C与opaque边界不扩称全项目实现闭包，仍按原验收ledger核证。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。


## 后端服务准入的有限交付（2026-10-18 UTC）

后端服务的当前支持集合已固定：execution schema6/logical1，目标ABI由非零注册descriptor负责且请求精确匹配（ABI23为合法正例）；descriptor、target与请求/结果contract的reserved均须零。backend API schema1保持独立，纯checker仍可比较相等的任意logical11。Register旧错误、请求flags/immutable及Complete generation优先级保留；独立contract generation失配现精确报告expected/actual。原单一实现按生命周期、编译协议、默认facade拆成三个TU，共享CMake同时登记四个生产TU。

[准入验证指南](../../../testing-and-validation/ssa-backend-admission.md)记录实际10案RED、扩充33案32PASS/1FAIL及最小修正后的33/33 GREEN、7服务场景与2/2 CTest。GREEN独审221A4D88与6C有限MSVC语法独审394EB90F均接受；旧后置解析失败收据原样保留，由离线记录解释。实际依赖闭包38头/def，50项实时输入与8项先前封定头离线补充分别登记。固定metadata mock不执行真实机器码或VM，原并发寿命TODO保留。

这完成了后端消费者准入这一子任务。四页现已交付声明分母内I/F/H/C owner、磁盘generation/进程epoch政策、已知ABI2隔离及兼容选择。严格数值逐操作表、原运行断言证据和00.01前置未完成；四编号任务/三批次不因政策子项交付自动勾选。

## 四页字典与原退出条件核账（有限文档交付）

- [运行期owner与D1–D4](../../../core-runtime/execution-contract-ownership.md)：actual/planned分开，域复制、策略快照、Gc_GetStats真实含义与process epoch明确。
- [持久字段](../../../module-system/persistent-contract-fields.md)：F117/836路径（824wire+12semantic）、旧IO117函数/976events与15runtime/115paths分别计数；208坐标纠正、35→18 sourcepins及packed45保证保留。
- [I19/20接口与状态](../../../testing-and-validation/ssa-interface-contract-matrix.md)：owner、生产/消费边及缺口；有声明不算有production consumer。
- [兼容与hash域](../../../testing-and-validation/ssa-contract-compatibility.md)：H31/C315有限union，canonical宽度/顺序/域、context-local TypeId限制、codec reserved及已知ABI2隔离；opaque只冻结有界外层合同。

Core语义独审100C1763接受有限政策；初始数据独审32813B7A失败原样保留，FF4067A7修正由current-data独审B5CB0E4D接受。当前四页与精确证据pin由报告封定，不能把历史review pin冒充当前页。

| 原任务/批次 | 可签收的有限子检查 | 保持open |
| --- | --- | --- |
| Task1／批次1 | 声明分母内I/F/H/C owner、字段坐标/hash域；opaque owner明确 | 不扩称全项目hash/未来格式闭包；后续codec按所属叶验收 |
| Task2 | CoreIR旧称、非法borrow/static contract不fallback、effects不授权 | 当前严格数值逐操作源码表缺失；09.02不得代替 |
| Task3 | 独立复制、epoch/leases和热更跨目标inline依赖政策 | 原borrow/thread-affine及inline运行断言尚未由本次metadata证据验证 |
| Task4／批次2 | 版本域owner/准入、状态族、D4签发与D5隔离政策 | registry/provider/lease/invalidation实际接入planned；不伪实现 |
| 批次3 | 源码重编/旧artifact拒绝/profile忽略/patch拒绝政策与actual/planned区分 | 完整caller事务、无部分publication、合法编译与rollback仍需证据 |

[Guide原断言ledger](../../../testing-and-validation/ssa-contract-freeze.md#original-0002-assertion-ledger)逐行保留原四测试要求与三assertion drafts，列测试名、平台、metadata范围、诊断位置、verifier、FI/释放和缺口；N/A不伪记0，未执行不称通过。原logical/checker历史Windows证据与Source42 GREEN33/service7/CTest2、MSVC6syntax保持各自有限范围。

00.01仍是显式前置，当前planned且M0完整收口另依赖07.04。本页status保持in-progress，四编号任务与三批次完整复选框不变。总索引和category导航只提高可发现性，不授本叶/M0/全47完成，也不授新运行权限。
