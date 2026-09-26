---
related_code:
  - CMakeLists.txt
  - zr_vm_common/include/zr_vm_common/zr_thread_conf.h
  - tests/CMakeLists.txt
implementation_files:
  - CMakeLists.txt
  - zr_vm_common/include/zr_vm_common/zr_thread_conf.h
  - tests/CMakeLists.txt
  - tests/cmake/ssa-platform-matrix.cmake
  - tests/acceptance/ssa-platform-matrix.md
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_platform_matrix.c
doc_type: milestone-detail
status: planned
---

# 10.03 桌面、Android、iOS 与 WASM 平台验收

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 落实桌面和 Android/iOS/WASM 的实际构建运行矩阵，防止 host 假设渗入可移植内核。

**Architecture：** 平台适配只承载线程、计时、执行内存、原子、unwind 和工具链差异；公共 IR/ABI/权限规则跨平台一致。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M7 平台；M1–M5 跨平台回归。
- 前置：[07.04 AOT Runner、覆盖率与发布门禁](../07-aot-backends/04-aot-runner-coverage.md)；[08.04 回滚、受限 Patch 与攻击面测试](../08-artifact-hotpatch/04-rollback-restricted.md)；[10.02 x86-64 与 AArch64 最小 JIT](../10-jit-platforms/02-host-baseline-jit.md)。
- 交付：平台能力矩阵、AOT/ExecBC smoke、host JIT 两架构验证和 unavailable 记录。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

根 CMake 已有 Windows/Darwin/Unix 分支；部分功能依赖平台 provider，必须验证真实设备/运行时，交叉编译通过不等于运行通过。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `CMakeLists.txt` | 检查功能开关和目标能力 |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_thread_conf.h` | 线程/原子适配 |
| 现有，修改/复用 | `tests/CMakeLists.txt` | 平台测试清单 |
| 计划新增 | `tests/cmake/ssa-platform-matrix.cmake` | 能力驱动测试注册 |
| 计划新增 | `tests/acceptance/ssa-platform-matrix.md` | 每个平台命令/设备/结果记录 |
| 计划新增测试 | `tests/core/test_ssa_platform_matrix.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 冻结 capability matrix** 列 ExecBC/AOT-C/AOT-LLVM/JIT、thread/concurrent GC、PMU、unwind、debug 支持；单线程 WASM 不宣称通过多 worker 测试。

- [ ] **2. 测试真实 ABI** pointer width、endianness、alignment、int/float、struct return、native callbacks；生成 C ABI 静态断言并跨平台 artifact 拒绝错误 target。

- [ ] **3. 执行平台 smoke** 桌面 GCC/Clang/MSVC、host 两架构 JIT；Android AOT/ExecBC、iOS AOT/受限 patch、WASM AOT/ExecBC 各运行同源 fixture。

- [ ] **4. 报告缺口和门禁** 工具链/设备缺失标 unavailable；跨编译、模拟器、真机分开记录；受限 patch 工程验收不替代平台分发政策判断。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
PlatformResult = {targetTriple, deviceOrRuntime, compiler, backend,
                  compiled, executed, semanticPassed, unsupportedFeatures}
require compiled && executed && semanticPassed for runtimeAcceptance
if noThreads: skipWithExplicitCapabilityReason(concurrentTests)
if mobileOrWasm: assert(machineCodeJitDisabled)
rejectArtifactWithIncompatibleTargetOrNumericContract()
```

通用 kernel 不按 OS/具体类型堆叠热点分支；平台差异在 backend/adapters。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 32/64 位或不同 alignment artifact | 不兼容明确拒绝 |
| WASM 无线程/PMU | 标 unsupported，非通过 |
| iOS patch 含新增 native import | 拒绝 |
| MSVC switch 与 GCC computed goto | 结果/异常/source map 一致 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_platform_matrix` 和可执行目标 `zr_vm_ssa_platform_matrix_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_platform_matrix_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_platform_matrix$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 索引列出的平台逐项有执行证据或明确未完成状态，主线可移植性不能由单 host 测试替代。

**失败恢复：** 某平台暂不可用不伪报支持；关闭可选 backend 不影响已验收的公共内核。

**文档交付：** 维护 docs/testing-and-validation/ssa-platform-validation.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrSsaPlatformCapability SZrSsaPlatformCapability;
typedef struct SZrSsaPlatformObservation SZrSsaPlatformObservation;
TZrBool ZrTests_Ssa_CheckPlatform(const SZrSsaPlatformCapability *declared,
    const SZrSsaPlatformObservation *observed);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| declared capabilities | target profile | CTest 按真实功能注册，缺失明确说明 |
| build/run evidence | 对应 device/runtime runner | 交叉编译和实际运行分开 |
| target ABI | generated assertions + loader | 跨平台 artifact 不匹配早拒绝 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 生成平台 capability 表与运行 fixture 目录，避免只列工具链名称。

- [ ] **批次 2：** 完成 desktop/host 两架构 ABI 与 backend 对照，分别记录 switch/computed-goto。

- [ ] **批次 3：** 完成 Android/iOS/WASM 的真实 runtime smoke 和受限 patch，设备缺失保留开放项。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange artifact built for incompatible pointer width/layout
assert load rejected
arrange WASM runtime without threads
assert concurrent test unavailable, not passed
arrange mobile profile attempts JIT executable allocation
assert feature unavailable and no machine-code path executed
```

### 迁移结束检查

验收表至少标 compiled/executed/passed 三列。iOS 受限热更 profile 的工程验证不能写成平台审核必然通过。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

