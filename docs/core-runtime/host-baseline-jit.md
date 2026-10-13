---
related_code:
  - zr_vm_core/include/zr_vm_core/host_baseline_jit.h
  - zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
  - zr_vm_jit/CMakeLists.txt
  - tests/core/test_ssa_host_baseline_jit.c
  - tests/core/test_ssa_host_jit_optional.c
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/host_baseline_jit.h
  - zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
plan_sources:
  - docs/plans/ssa/10-jit-platforms/02-host-baseline-jit.md
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - "user: 2026-09-13 实现 10.02 host baseline/JIT contract leaf"
  - docs/code-review/batches/20261013-jit-contracts.md
tests:
  - tests/core/test_ssa_host_baseline_jit.c
  - tests/core/test_ssa_host_jit_optional.c
  - tests/acceptance/ssa-host-baseline-jit.md
doc_type: module-detail
status: implemented-subset
---

# Host baseline JIT：声明校验与记录生命周期

## 当前实现范围

core 提供 C ABI 的目标、配置、导入与发布声明校验，以及调用方数组内记录的生命周期管理。它不生成机器码，不分配可执行页，不保存入口地址，不执行 GC 或真实图登记。可选 facade 的 provider、proof 与地址资源属于另外的所有者；当前 facade 注册时把 machineCodeAvailable 设为 false（`zr_vm_jit/src/orc_backend.cpp:1303`），有无 LLVM 构建探测都不能据此获得机器码能力。

目标、身份与 hash 是不含执行地址的标量见证，但整个 lifetime 对象并非无指针：manifest 借用数组，manager 借用记录数组，HostJitCodeHandle 借用记录位置。输入在调用期间保持稳定，可写输出与输入、manager/records 使用独立存储，共享诊断与输出由调用方串行化。公共 C++ 链接包裹只提供 C 链接名（`zr_vm_core/include/zr_vm_core/host_baseline_jit.h:23`）。

## 校验的前提与成功含义

| 声明 | 当前校验 | 成功不代表 |
| --- | --- | --- |
| Target | schema1、HOST、编译 x86-64/AArch64 匹配、pointerSize==sizeof(void*)、LITTLE 声明、execution ABI17、非零 triple/layout hash | 运行时 CPU/端序探测、实际 target/layout hash 重算、provider 可用 |
| Options | schema/已知选项位始终校验；仅 ENABLE 后验证 target 与非零预算 | 禁用配置的 target/预算有效，或预算已分配为代码页 |
| Imports | 完整清单 schema、非零摘要、条目非零 ID/签名、RUNTIME/NATIVE、reserved0、ID唯一；查询还需同签名 | 实际符号绑定、地址解析或 native 权限授予 |
| Publication | MACHINE_CODE/WX、四图注册位、非零图与身份摘要、layout同target、已知操作/capability/effect位 | 真实页权限、状态图内容、平台图登记或代码行为已被验证 |

具体操作见 target 的 ABI 比较 `zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:106`、options 禁用早回 `zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:145`、manifest 唯一身份比较 `zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:186`、publication 注册位比较 `zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:282`。schema 与执行 ABI 分别验证；指针宽度按 sizeof(void*)，不能把 fixture 的固定8写成通用契约。Publication 的 imports 可空，非空时只在本次验证借用，不复制到记录。

## 记录数组与 lease

调用方提供真实容量、对齐且稳定存活的 records 数组。Init 独占初始化并清零数组，manager 不分配或释放它（`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:498`、`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:501`）。正常操作内部配对自旋锁；MSVC 用 InterlockedExchange，其他分支用 __sync。volatile 不能替代互斥，锁不可重入。32位容量检查只防字节跨度溢出，不证明调用方实际分配长度。

完整 shape 在同锁下验证所有槽：FREE 字段全零、非空身份与三个hash非零且身份唯一、PREPARED lease0、nonFREE count一致、最多一条PUBLISHED及active归属（`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:416`、`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:446`）。直接改 count/leaseCount 的 fixture 注入不是普通调用协议。

| 操作 | 资源与状态边界 |
| --- | --- |
| Prepare | 验证 facts 后写身份/三个hash，返回未租用准备引用；不持有 imports/图内容。前置失败不保证清空输出，不能把失败输出当新句柄。 |
| Publish | 只接受本manager的未租用PREPARED引用；旧active退休，新active唯一发布。prepared不被清空，也不自动取得lease或生成地址。 |
| AcquireActive | 为当前PUBLISHED增加一份lease；UINT32_MAX拒绝。输出不得覆盖既有lease，复制句柄不能增加拥有权。 |
| Evict | 按身份退休，包括准备记录；移除active，不减少已有lease。 |
| Resolve | 合法lease可观察RETIRED记录；返回同锁下标量快照，不返回地址或新lease。const manager仍会修改锁，底层实例必须可写。早期失败不保证清空view。 |
| Release | 成功减少这一份lease并清空传入句柄；失败保留句柄，不表示归还成功，也不自动Collect。 |
| CollectRetired | 仅RETIRED&&lease0清零元数据槽；不释放代码页或外部proof。非空计数输出在manager验证前置零。 |
| Deinit | shape有效且active/count都空才解除records借用；void拒绝保留原实例。所有者确认records为空后才可释放数组。 |

实际退休、lease与回收分别在 `zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:643`、`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:691`、`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:845`、`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:884`；Deinit 保留条件在 `zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:518`。core 计数不能被外推为真实 active frame 或代码页寿命的证明。

## 可选 facade 与 SDK 的不同家族

facade 在 global_mutex 内拥有 records vector 与 proof vector，把 records.data 借给 core manager（`zr_vm_jit/src/orc_backend.cpp:1308`）。proof追加失败的补偿调用Evict/Collect并清句柄（`zr_vm_jit/src/orc_backend.cpp:1037`、`zr_vm_jit/src/orc_backend.cpp:1039`）；补偿调用的返回码被忽略，因此不承诺该失败分支已完成回收。正常Collect先清core槽再清proof，Shutdown确认core解除借用后才析构单例。GetDescriptor 复制静态回调且 userData为空，optional测试实际派发queryTarget（`tests/core/test_ssa_host_jit_optional.c:101`）；queryTarget 重新查单例，目标检查通过后仍因机器码不可用拒绝（`zr_vm_jit/src/orc_backend.cpp:666`）。其他回调有不查询单例的快照/no-op 路径，不能承诺关闭后所有回调都返回不可用。Compile只选择fallback或不可用；LookupEntry输出零，不产生机器码入口。descriptor 副本不延长注册寿命，也没有被该测试注册到 core backend service。

ValidateStateMaps 只检查四种登记位、非零 count/hash 和 frameLayoutHash 的声明关系，不读取图内容。ValidateCompileRequest 会核对 publication 与 frameLayoutHash；prepare_locked 另核对四个图摘要。当前 fallback 请求是否也必须满足四摘要一致仍为 TODO，不能把前置校验成功解释为真实图已登记。诊断为空时 ValidateCompileRequest 的图失败统一返回 STATE_MAP_INVALID；独立 ValidateStateMaps 仍返回其具体拒绝码。SZrJitHostDiagnostic.operation 当前只被清零，没有赋值入口，字段语义仍待核查。

Prepare、PrepareWithMaps、Acquire、Compile 的输出必须使用独立空槽，不能覆盖尚须 Release 的 lease。前两个 wrapper 先清空输出再校验，Acquire 成功后复制新 lease，Compile 即使拒绝也会清空输出；这些输出规则继承 core 的 lease 前提，不据无效调用推断生产缺陷。

HostJitCodeHandle 的 record/codeIdentity/leased 与 SDK service 的 SZrExecutionCodeHandle 不互换。后者有 service/slot/generation/dependency 身份协议（`zr_vm_core/include/zr_vm_core/execution_backend.h:295`），不能把它的防复用或地址 lease 保证搬给本 manager。

## 具体 TODO

- 空槽Collect后可复用相同codeIdentity，Host句柄没有独立generation；旧准备引用如何按外部协议失效仍待确认。
- Prepare/Acquire/Release 的失败诊断有解锁后再读字段路径（`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:569`、`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:689`、`zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c:835`）。当前facade外层串行；公开core直接并发 owner/存储约束未闭合，不宣称所有失败诊断无竞态，也未据此升级BUG。
- core OPERATION_UNSUPPORTED/ACTIVE_LEASE 当前无producer，仅名称/上层映射；facade 同名状态属另一枚举，未来或外部生产协议待核。
- StatusName 当前没有仓库正向调用，仓库外公开ABI用途未知；返回借用静态字符串。
- facade缺ABI/LAYOUT专门映射、Prepare是否必须与注册target相同的诊断/使用契约仍需有限确认。旧表BUG没有在本轮得到完整合法路径证明或复现，不继承其信用。四图摘要的前置校验差异、operation 字段、输出空槽和 ORC 探测门禁的逐项核查入口见本次 JIT 台账。
- BUG: query_target 比较完整 64 位 layoutHash 后，通过 32 位 helper 参数填入 service 诊断。Register 成功、GetDescriptor 取得回调、直接传入仅 layoutHash 高 32 位不同的 HOST_JIT target 时，返回 CONTRACT_MISMATCH，但 expected/actual 相同，丢失区别值（`zr_vm_jit/src/orc_backend.cpp:656`、`:660`；`tests/core/test_ssa_host_jit_optional.c:101` 证明直接回调入口）。该结论来自完整可达路径和字段宽度证据，现有测试没有执行这一高位差异用例；本轮只标记。

## 测试、构建与验证范围

baseline fixture由 `tests/cmake/ssa-tests.cmake:1325` 编译本C与测试，CTest在1332登记；optional fixture在host开关下链接JIT目标，在1350登记。core普通模块经CommonMacros递归C glob纳入本源，无需LLVM。

baseline源码覆盖target/选项/导入/发布声明拒绝、重复身份、lease overflow注入、manager count注入、退休后Deinit保留、归还后Collect；optional源码覆盖descriptor直接派发、fallback/零入口和Shutdown顺序。assert须启用。2026-10-13 在实际 checkout 使用 MSVC 19.44 Debug、显式 /UNDEBUG 的定向 driver，修改前 configure/build 和 optional CTest 均退出0，测试1/1通过；最终注释版本复用同一构建目录与配置执行 clean-first 重编译，日志确认实际编译，build/CTest也均退出0，optional测试1/1通过，四个模块源文件、测试及所用core实现的SHA在运行前后保持。三个JIT C/C++文件的Clang raw lexer非注释token及行首标志一致，CMake非注释行一致；最终结果与词法证据分别登记于 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/jit-summary-recompile-after-results.json` 和 `jit-summary-comment-equivalence.json`。本次没有重跑 core-only baseline fixture；其上述项目仅为源码覆盖证据。WSL启动实际返回 E_ACCESSDENIED，不能据 Windows 结果授予 Linux 或两架构验证信用。实际代码页/W^X系统调用、图内容与平台登记、ORC resolver、lowering与性能仍不在本模块已实现信用内。计划与历史验收文件是设计/历史来源，不替代当前运行证据。
