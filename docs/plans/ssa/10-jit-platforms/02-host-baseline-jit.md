---
related_code:
  - CMakeLists.txt
  - zr_vm_parser/CMakeLists.txt
implementation_files:
  - CMakeLists.txt
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_jit/CMakeLists.txt
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_host_baseline_jit.c
doc_type: milestone-detail
status: planned
---

# 10.02 x86-64 与 AArch64 最小 JIT

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 实现仅 x86-64/AArch64 host 的最小 JIT，以验证共享 ABI、GC、异常和代码生命周期。

**Architecture：** 可选 LLVM ORC/JITLink 适配层消费 AOTIR；LLVM/C++ 依赖隔离为可选模块，core 维持 C11。首版优先完整安全边界，覆盖少量 typed 操作。

**Tech Stack：** C11、CMake、Unity/CTest；C11 core、可选 C++/LLVM ORC/JITLink、host ABI。

## 依赖与交付范围

- 对应主计划：M7。
- 前置：[10.01 后端服务、异步编译与代码生命周期](../10-jit-platforms/01-backend-service.md)；[07.02 C、LLVM 同源代码生成](../07-aot-backends/02-c-llvm-lowering.md)；[09.02 严格数值、Fast-math 许可与向量 IR](../09-language-simd/02-numeric-vector-ir.md)。
- 交付：host JIT 注册、typed scalar/control/direct/simple member/array 支持和性能独立报告。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

仓库根默认只启 C；引入 ORC 必须局部 enable CXX/可选目标，不能让未启 JIT 的构建强制依赖 LLVM。ORC 本身不保证 baseline 级编译延迟，需要测量。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `CMakeLists.txt` | 新增可选 host JIT 构建开关 |
| 现有，修改/复用 | `zr_vm_parser/CMakeLists.txt` | 共享 AOTIR 输入，避免反解 ExecBC |
| 计划新增 | `zr_vm_jit/CMakeLists.txt` | 隔离 C++/LLVM 工具链 |
| 计划新增 | `zr_vm_jit/include/zr_vm_jit/backend.h` | 对 core 暴露 C ABI |
| 计划新增 | `zr_vm_jit/src/orc_backend.cpp` | ORC/JITLink、symbol allowlist 与 code lifetime |
| 计划新增 | `zr_vm_jit/src/jit_state_maps.cpp` | root/unwind/debug/deopt 注册 |
| 计划新增测试 | `tests/core/test_ssa_host_baseline_jit.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 固定支持矩阵** host x86-64/AArch64 独立验证；Android/iOS/WASM 配置拒绝启机器码 JIT。LLVM 版本在实施环境固定，不写死未经验证的 API 版本。

- [ ] **2. 隔离导入解析** ORC symbol resolver 只允许 manifest 内 runtime/native symbols，不能搜索任意宿主地址；entry 类型经过统一签名与 ABI 校验。

- [ ] **3. 先做地图与生命周期** executable memory 使用平台 W^X 协议；stack maps/unwind/debug 注册后才发布 entry，退休先撤入口等待 lease 再注销和释放。

- [ ] **4. 实现最小 lowering** 只接 typed scalar/control/direct/simple member/array；其他显式 unsupported 或可见 ExecBC continuation。单独报告 compile latency、cold start、warm throughput、cache/RSS。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
if target not in allowedHostTargets: disableJitAtConfiguration()
compile(AOTIR):
    verifyContractsAndAllowedImports()
    emitBaselineLLVMWithoutExpensiveOptimizationPipeline()
    linkWithRestrictedResolver()
    registerRootsUnwindDebugDeopt()
    publishOwnedCodeHandle()
JitAddress never enters artifact or patch payload
```

首版不做完整 optimizing JIT/OSR 产品化；有 guard exit 就必须能恢复完整 state，不完整则不启该专门化。LLVM 使用的 C++ 异常不能穿越 core C ABI。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 两个 host 架构 typed ABI | 参数/返回/inline layout 一致 |
| JIT 内 GC/native callback/throw | root/unwind/cleanup 正确 |
| 非法 symbol import 与非 host target | 拒绝 |
| code cache eviction 有活跃 frame | 等待 lease，不释放执行中的页 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_host_baseline_jit` 和可执行目标 `zr_vm_ssa_host_baseline_jit_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_host_baseline_jit_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_host_baseline_jit$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 安全/语义矩阵先于速度；JIT 可选且可完全禁用，成本与 AOT 独立对比。

**失败恢复：** JIT 编译失败保持 AOT/ExecBC 并记录原因；不能安装部分生成代码。

**文档交付：** 新增 docs/core-runtime/host-baseline-jit.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
/* Public JIT facade remains C ABI even though the implementation is C++. */
typedef struct SZrHostJitOptions SZrHostJitOptions;
TZrBool ZrJit_Host_Register(const SZrHostJitOptions *options,
    SZrExecIrDiagnostic *diagnostic);
void ZrJit_Host_Shutdown(void);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| allowed imports | base capability/runtime ABI | ORC resolver 禁任意 host symbol lookup |
| object/code/maps | LLVM/JITLink 适配层 | 完成所有 registrations 后才可进入 |
| host platform contract | 构建目标 | 移动/WASM 不创建 executable memory |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 可选 C++/LLVM target 与核心 C ABI，禁用 JIT 的构建不受影响。

- [ ] **批次 2：** 先做 scalar/direct call、symbol allowlist、W^X 和 code lifetime。

- [ ] **批次 3：** 接两个架构的 root/unwind/deopt/debug，运行 GC/exception/re-entry 和 code-cache eviction。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange disabled JIT build without LLVM installed
assert ordinary VM/AOT build still succeeds
arrange JIT module requests unlisted host symbol
assert link rejected before entry published
arrange active code evicted from cache
assert retired until all frame/callback leases released
```

### 迁移结束检查

ORC 的优化等级和首次编译耗时必须实际测量；不能因为使用 ORC 就称为低延迟 baseline。平台异常展开与 stack map 不完整的目标不标支持。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

