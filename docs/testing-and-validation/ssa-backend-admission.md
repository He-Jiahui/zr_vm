---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - tests/core/test_ssa_backend_admission.c
  - tests/core/test_ssa_backend_service.c
  - tests/cmake/ssa-backend-tests.cmake
  - tests/cmake/ssa-backend-admission/CMakeLists.txt
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - "user: 2026-10-18 backend admission owner, diagnostics and fixed-slot recovery"
tests:
  - tests/core/test_ssa_backend_admission.c
  - tests/core/test_ssa_backend_service.c
  - tests/acceptance/ssa-backend-service.md
doc_type: testing-guide
---

# Backend admission：标量接纳与诊断验收

## 范围与 owner

574行 `test_ssa_backend_admission.c` 验证真实 C backend service 的 descriptor、
请求和结果接纳。测试使用固定栈数组、始终生效的 CHECK 和本地元数据回调，
没有 IR、VM 对象、机器码缓冲区或实际代码入口。所有已初始化 fixture 即使断言
失败，也执行 Shutdown → FinalizeShutdown → Deinit。

实现职责和并发限制见[模块说明](../core-runtime/execution-backend-service.md)。
本页只覆盖完整00.02的一组 backend 接纳证据，不代表整叶接口/持久字段字典已完成。

| 版本/字段域 | 当前规则与责任 |
| --- | --- |
| backend schema | descriptor/target/request/result 使用 backend schema **1** |
| execution schema/logical | 请求 admission owner 当前支持 **6 / 1**，没有版本协商 |
| backend ABI | 非零；请求匹配实际选中 registration 的 ABI/layout，正例为 **23** |
| execution/AOT ABI 宏 | 当前 **17**，不能机械作为所有 backend 的唯一 ABI |
| generation key | domain/module/generation/registration 四元身份，不能只按代次数跨域退役 |
| contract.generation | 结果必须与请求契约对应，与完整键的相等性分别校验 |

`execution_contract.c` 保持纯 equality/已知位职责，不把服务的 current-only 政策
推广给所有消费者。descriptor/target 的 reserved 与请求/结果 contract 的
reserved0/reserved1 必须为零；其他 service/job 内部保留字段没有因此取得新准入语义。

原错误优先级保留：

1. Register 原 magic/schema/flags/target/operation/必需回调形状错误在前；
   原验证全部通过后才检查 descriptor、target reserved。
2. CompileAsync 票据参数、请求 magic/backend schema、flags、immutable input 在前，
   然后检查 contract schema/logical/reserved。失败不得 query 或占用作业。
3. Complete 票据/状态/取消与 generation 在前，再检查契约/hash/产物。
   拒绝结果进入 FAILED，不建立 READY；原 owner 清理先 U 撤图、后 R 退休。
4. 完整 key 不等时保留原 key-generation 诊断载荷；完整键相等且独立
   contract.generation 错误时，expected/actual 使用两个 contract generation，
   请求1/结果2须报1/2，不能报两个相等 key 中的1/1。

公开诊断 expected/actual 是64位标量，expectedState/actualState 是32位状态或掩码。
请求错误保留 source/instruction，结果错误还核对 ticket/code identity。
NULL diagnostic 不改变拒绝、FAILED/noREADY 或 U→R 协议。

## 33个 case

原10个 case 名称、顺序与核心语义保留。每个拒绝用独立字段事实验证，检查输入字节
不变、错误状态和清理；不是复制生产的比较算法来生成期望值。

| case | 可观察条件 |
| --- | --- |
| healthy_non17 | schema6/logical1、零 reserved、ABI23匹配注册，PENDING→READY→Publish→U→R→D |
| descriptor_reserved / target_reserved | mismatch expected0/actual1，无注册/query/compile |
| logical_next / logical_max | 非当前非零版本拒绝，准确版本标量，无入队 |
| request_reserved0 / request_reserved1 | expected0/actual1，票据身份0 |
| immutable_priority | contract 多错时 immutable-input-required 优先 |
| result_reserved0 / result_reserved1 | FAILED/noREADY，准确0/1，一次 U→R |
| register_flags_priority | flags+target+operation+reserved 多错先报 flags |
| register_target_priority | target+operation+reserved 多错先报 target |
| register_operation_priority | operation 优先于 reserved |
| request_schema / request_contract_schema | 区分 backend schema 与 execution schema |
| logical_zero | logical0拒绝，expected当前1 |
| request_flags_priority | unknown flags+缺 immutable+contract 多错先报 flags |
| request_null_diag_logical / request_null_diag_reserved | NULL diagnostic，拒绝且无入队 |
| result_schema / result_abi / result_logical | 单版本差异准确 expected/actual |
| result_token / result_signature / result_layout / result_module | 单身份/hash差异，结果输入不变 |
| result_capability / result_effect | 已知标量位相等性差异，不是权限或安全功能测试 |
| result_generation_reserved | key generation与reserved同错，stale-generation优先 |
| result_contract_generation | 完整键仍正确，仅 contract generation2，准确1/2 |
| result_null_diag_reserved | NULL diagnostic 下 FAILED/noREADY/一次 U→R |
| result_contract_priority | 多个契约差异时先报告 schema |
| failure_then_healthy | 两失败填满两个槽，第三健康请求复用终态槽、发布并回收 |

明确期望值包括 ABI23/24、token13/113、signature14/114、layout102/202、
module12/112、capability0/ARITHMETIC、effect0/READ_MEMORY。
FAILED ticket 可暂留供查询，但不得拥有代码或图；最终 Deinit 清空服务壳。
恢复 case 为 query3/compile3/U3/R3/D1；单个拒绝结果为 query1/compile1/U1/R1/D1。

输出协议为 `CASE name=... status=... pass=0|1 query=... compile=... U=... R=... D=...`，
最后 `SUMMARY cases=33 failed=N`。任何 CHECK 失败或总数异常都返回非零。

## 已发生的 RED 与当前 GREEN

本任务证据根：`E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/`。

### 旧10 case：真实漏拒绝 RED

原353行 fixture 在 owner gate 修复前实际 configure/build/list/test 返回0/0/0/8。
healthy_non17 与 immutable_priority 通过，其余8个 reserved/非当前 logical
因旧实现接纳而失败。独审核实 FAIL 原因与清理，接受预期语义 RED。

- [原 RED 独审](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-red-review.json)
- [原 RED 收据](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-red-receipt.json)

收据 SHA256 `0CF2F7B321EA7D5C83EA1FB3CE02DAAD04A5FA633ACFAC3EE68C123C1AE31F8B`；
旧源 RED 不作为当前源 GREEN 信用。

### 扩充33 case：generation诊断 RED 与后置解析失败

拆分/准入修复后，未修 generation 诊断的测试实际32PASS/1FAIL，唯一 FAIL 为
result_contract_generation 的 `diagnostic.actual == actual`。状态仍为
stale-generation，U/R/D 与清理正确，既有 service 七场景也通过。

四个 native 阶段为0/0/0/8，但 helper 后置正则要求点线后至少一个空格；
实际 `............***Failed` 无空格，helper 因解析失败自然 exit1、收据 false。
离线 reconciliation 接受原始流证明的语义 RED，没有重跑 native，没有改写原
false 收据。正则修正允许零空格，同时保留精确 test 名和有限结果状态。

- [原始测试流](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/expanded-red-test.stdout.log)
- [原 false 收据](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-expanded-red-receipt.json)
- [离线 reconciliation](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-expanded-red-reconciliation.json)
- [最小 generation 修正](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-generation-fix.json)

reconciliation SHA256 `5AA53BE536D6D3A891326104FE385A0C301BA4E847F8B28DB7E40B350524AB99`；
原 false 收据 `B4395FED5E5A64F0EC6C5919E0D25BBC66A425B7C9A2C853F6984BA5173EDB75`。
生产修正只选取准确 generation 标量，没有修改状态、锁、清理、计数或测试断言。

### 当前：实际GREEN与独立验收已通过

2026-10-18 Root实际运行当前两项 CTest，四阶段0/0/0/0、33行全部 PASS，
admission/service 均 Passed；50输入不变，五个 service 编译角色的 /UNDEBUG 生效，
processes_remaining0、finalization_errors空。外层自然 exit0、stderr空、handle关闭。
独立 GREEN 审查已接受33案、7场景、精确缓存复用及离线头文件补充；
[审查报告](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-green-review.json)
SHA256 `221A4D88BF309C2F11566A992B054E98B849D5F7229D651AC5E6ABDBE93E483D`。

[GREEN计划](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-green-plan.json)
SHA256 `CBF0823E9A245FD735D58D415D58DA5F202D20C85B460B6609060DDF8AD2A861`；
[实际收据](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-green-receipt.json)
49213B，SHA256 `6CFAE24216BDB56381094FD8D6C427E70CBD398342BDCADEBA9F6DB8CE1751CC`。
[外层证据](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-green-outer.json)
与 green-{configure,build,list,test} 的8个 raw 流保留。

本轮复用 expanded RED 的10角色/2目标构建，仅两个 compile.c 角色重新编译并重链
两个程序。10条 compile_commands 是当前图的分母，不能称为本轮10次 fresh 编译。
本页不据该结果宣称整仓、真实 provider 或并发已经通过。

另有实际 MSVC 语法检查：六唯一C（两fixture、四生产TU）均 /Zs /W4 /WX /UNDEBUG /showIncludes 自然exit0，50输入不变，输出目录无文件。此结果不包含MSVC链接/运行、Linux或full matrix。

[MSVC语法收据](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/msvc-syntax-receipt.json)，9940B，SHA256 `9BEFB8D8282A050578D65F905D145B45048870CA3AB85055B4011B8D50307C7A`。
[独立语法审查](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/msvc-syntax-review.json)
SHA256 `394EB90F40205BDE63D56C9AC1061B71317B7E6D36D12B65A5EEE7D2E8D0C00D`，接受六个唯一C的有限语法结果。

实际 `/showIncludes` 与 Ninja 依赖闭包均为38个本地头/def。原运行前后实时固定的输入仍是50项；
额外8个 service 依赖头由更早已封定的 Source41 MSVC 记录与当前哈希逐个相等，离线补足保护并集58项。
[头文件补充记录](E:/cargo-targets/zr_vm/reports/ssa-contract-freeze-01a0fe2b/backend-header-closure-overlay.json)
SHA256 `A366AD916BDB2D44D4109AAE915C82A3C9952738649B88FA4BA55A651A5F594E`。
该离线基线链不改写历史50项实时检查，也不增加实际编译或测试次数。

## 当前构建路由与限制

`tests/cmake/ssa-backend-tests.cmake` 是两目标共享的显式源列表。每个目标包含
自己的一个测试 TU，加 backend.c、backend_compile.c、backend_facade.c、
code_handle.c 四个生产 TU。职责分别为生命周期/注册、编译状态机、默认 facade、
代码/图/租约/回收。private header 提供共同 helper，无新增 public ABI。

standalone `tests/cmake/ssa-backend-admission/CMakeLists.txt` 同时注册 admission/service，
合计10编译角色、两个 CTest；正常测试聚合复用同一函数。service 必须启用 assert：
/UNDEBUG 或 -UNDEBUG 位于所有 NDEBUG 定义之后。admission CHECK 不受 NDEBUG 影响。

匹配工具链已配置后，精确目标/selector 为：

~~~text
cmake --build E:/cargo-targets/zr_vm/build/ssa-contract-freeze-01a0fe2b/clang --target zr_vm_ssa_backend_admission_test zr_vm_ssa_backend_service_test
ctest --test-dir E:/cargo-targets/zr_vm/build/ssa-contract-freeze-01a0fe2b/clang -V -R "^ssa_backend_(admission|service)$" --no-tests=error
~~~

旧两源直接 GCC/WSL 命令只保留为[历史记录](../../tests/acceptance/ssa-backend-service.md)，
不能用于当前模块集合。admission只直调13个Service/StatusName入口，optional
cancel/lookupEntry/queryMap/resume 为NULL。既有 service 另含受限标量租约、入口/图查询
和诊断转发；0x1234只比较不执行，resume不执行解释器。两者不构成 VM/FFI/网络/
真实机器码或安全功能验证。

Complete handoff、query/worker并发、默认指针寿命和 Resume关闭交错 TODO 全部保留，
串行 mock 证据不关闭这些问题。
