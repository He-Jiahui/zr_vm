---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_backend_service.c
doc_type: milestone-detail
status: planned
---

# 10.01 后端服务、异步编译与代码生命周期

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立统一 execution backend 服务，管理异步编译、代码注册、失效与解释器恢复。

**Architecture：** core 只依赖 C ABI 的 backend vtable；AOT/ExecBC/JIT 通过 owned code handle 和 immutable compile input 接入，编译不阻塞 frame thread。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M7 foundation；后端公共接口。
- 前置：[07.01 共享 AOTIR 与后端 ABI 收敛](../07-aot-backends/01-aotir-contract.md)；[06.05 协程等待、后台编译与帧预算](../06-gc-domain/05-async-frame-budget.md)；[08.03 Generation 发布、旧 Frame 与回收](../08-artifact-hotpatch/03-generation-publication.md)。
- 交付：Register/CompileAsync/InvalidateGeneration/ResumeInterpreter API 与 code handle 生命周期。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

existing AOT ABI 有 entry thunk/methodInfo；JIT 需要新增独立 code lifetime，不能把任意 void* 伪装成永不失效函数指针。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 复用调用/root/exception contract |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/call_binding.h` | 扩展运行期 backend witness，不改磁盘保存原则 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/execution_backend.h` | 00.02 预留接口的正式定义 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_backend.c` | 注册和异步任务状态 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c` | code/map/epoch 引用与释放 |
| 计划新增测试 | `tests/core/test_ssa_backend_service.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义 capability vtable** 后端声明 target、supported operation、compile、lookupEntry、retire、destroy 和 map 查询；unsupported 可观测，不借助异常绕回解释器。

- [ ] **2. 实现 job 状态** Queued→Compiling→Ready/Failed/Cancelled→Published；input 深拷贝或引用冻结 IR，generation/hash 不匹配结果禁止安装。

- [ ] **3. 绑定代码和地图** handle 拥有 machine code、roots/EH/debug/deopt registrations、runtime imports 与 dependency leases；注销 unwind/debug 后才释放代码。

- [ ] **4. 统一恢复与关闭** guard exit 使用 01.04 state maps，shutdown 取消未开始 job、等待 in-flight、退役 code，线程安全销毁后端。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
BackendVTable = {queryTarget, compileAsync, lookupEntry, retire, destroy}
CodeHandle = {target, generation, codeOwner, entryTable, stateMaps, leases}
compileComplete(job):
    if cancelled || staleHashOrGeneration: disposeUnpublishedHandle()
    else: publishAtSafeGenerationBoundary()
InvalidateGeneration -> preventNewEntries -> waitLeases -> unregisterMaps -> freeCode
```

编译线程不直接访问移动中的 VM object；CompileAsync 返回 pending/failure 是正常协议，不能阻塞调用者等待优化代码。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 后台编译时 reload/cancel/shutdown | 不安装过期代码、不泄漏 |
| entry 正在执行时 invalidate | 等 lease 退出后回收 |
| unsupported backend target | 明确 AOT/ExecBC fallback mode |
| deopt 到解释器 | 逻辑状态与效果一致 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_backend_service` 和可执行目标 `zr_vm_ssa_backend_service_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_backend_service_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_backend_service$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** code/map/import 生命周期可证明，后端不可用不破坏 AOT 发布路线；不依赖 JIT 才能运行。

**失败恢复：** 卸载可选后端后保留 ExecBC/AOT，所有执行模式变化上报。

**文档交付：** 新增 docs/core-runtime/execution-backend-service.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecutionBackendDescriptor SZrExecutionBackendDescriptor;
typedef struct SZrExecutionCompileRequest SZrExecutionCompileRequest;
typedef struct SZrExecutionCompileTicket SZrExecutionCompileTicket;
typedef struct SZrExecutionGenerationKey SZrExecutionGenerationKey;
TZrBool ZrCore_ExecutionBackend_Register(const SZrExecutionBackendDescriptor *backend);
TZrBool ZrCore_ExecutionBackend_CompileAsync(const SZrExecutionCompileRequest *request,
    SZrExecutionCompileTicket *ticket);
TZrBool ZrCore_ExecutionBackend_InvalidateGeneration(
    const SZrExecutionGenerationKey *key);
TZrBool ZrCore_ExecutionBackend_ResumeInterpreter(const SZrExecIrResumeRequest *request,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| backend descriptor | 后端注册 | core 只见 C ABI，不依赖 LLVM 对象 |
| compile ticket/job | backend service | 取消与结果发布 CAS 防重复完成 |
| owned code handle/maps | 后端 linker | code、unwind、debug、roots 同生命周期 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 注册 ExecBC/AOT mock backend 测状态机，尚不依赖 JIT。

- [ ] **批次 2：** 接 immutable IR async job、取消/过期结果、queue budget。

- [ ] **批次 3：** 接 code handle/lease/retire 和 state-map resume，验证 shutdown race。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange cancellation wins before compile completion
assert ready code disposed and ticket completed once
arrange invalidate while code lease active
assert no executable page freed
arrange backend unsupported
assert explicit selected fallback mode recorded
```

### 迁移结束检查

CompileAsync 的 callback 不在持 backend/global lock 时调用用户代码。ticket 与 code handle 分离，完成一个编译请求不代表代码已发布。

`SZrExecutionGenerationKey` 包含 domain identity、module identity、generation 和 backend registration identity。失效必须限定到这个命名空间，不能仅用一个 generation 整数误伤另一个模块/域的代码；编译缓存 identity 与运行期 code lifetime key 分开。新增两个域拥有相同 generation 数值时单域失效的测试。

JIT target 在运行期 tagged target 中增加独立 kind，并携带 owned code handle/lease。沿用统一 typed ABI 的 AOT thunk 形状不意味着 JIT code 可按永久 AOT entry 管理。磁盘 CallBinding 仍保存 semantic target token/signature/layout，加载后由后端服务决定当前运行期 target kind；不能因为新增 JIT 分支而把 code handle 写入 persistent contract。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
