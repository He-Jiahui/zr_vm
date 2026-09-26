---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_core/include/zr_vm_core/function.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_loops_specialization.c
doc_type: milestone-detail
status: planned
---

# 02.05 循环优化、PGO 与专门化预算

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 自动进行循环不变量外提、归纳变量和 strength reduction，并用可靠 profile 约束专门化。

**Architecture：** loop analysis 基于 CFG/dominators 与 effect；PGO 以稳定 IR site key 导入，版本有上限和退避，避免频繁 deopt 和代码膨胀。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6；M3/M7 的 profile 来源。
- 前置：[02.04 跨函数摘要、去虚化与内联](../02-automatic-optimization/04-interprocedural-inlining.md)。
- 交付：loop passes、profile schema/导入验证、专门化成本策略。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

已有 benchmark 与 callsite cache 可以提供命中信息，但持久化 profile 不能包含地址或运行期不稳定 TypeId。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c` | 迁出热度策略，消费专门化决定 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/function.h` | 复用 callsite 计数和 resolved cache 入口 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c` | loop forest、induction、preheader |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c` | 受 effect/异常约束的外提 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c` | 稳定 key、导入拒绝与预算策略 |
| 计划新增测试 | `tests/parser/test_ssa_loops_specialization.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 识别循环结构** 多入口/不可约 CFG 先保守不优化，创建 preheader 时同步 phi/异常边；记录循环 trip count 范围。

- [ ] **2. 实施 LICM/strength reduction** 只外提支配所有使用、操作数不变且不改变陷阱/异常时机的操作；零次迭代循环中的可抛操作不得提前。归纳变换检查整型溢出。

- [ ] **3. 采集与校验 profile** 记录 call target、receiver、branch、numeric type、allocation、boxing、cache、deopt、GC、contention；key 验证 module identity/IR/signature/layout/compiler ABI。

- [ ] **4. 约束专门化** 每站点/函数/module 有版本数和 byte budget；低命中或连续失效后退避，保留基线 typed slot 路径；把拒绝原因交给 remarks。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
profileKey = (moduleIdentity, irHash, signatureHash, layoutHash, compilerABI)
if importedKey != expectedKey: ignoreProfileAndExplain()
hoist(i) only if loopInvariant(i) and memoryStable(i)
             and (nonTrapping(i) or guaranteedOriginalExecution(i))
if site.codeBytes > limit or site.missRate > limit:
    disableSpecializationUntilCooldown()
retainBaselineImplementationForEverySpeculation()
```

热点是收益信息而非语义证明；profile 过期不能影响正确性。成本阈值集中在模块配置，不散落 magic number。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 循环零次、边界溢出与异常 load | 不产生新异常 |
| 高命中到 megamorphic | 版本有界且进入退避 |
| profile hash 任一不同 | 忽略且说明原因 |
| 低 trip count 循环 | 成本模型可保留标量原循环 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_loops_specialization` 和可执行目标 `zr_vm_ssa_loops_specialization_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_loops_specialization_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_loops_specialization$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** loop 差分与失效测试通过；profile 收集开销、code size、编译时长和代表集收益分别报告。

**失败恢复：** 拒绝 profile 或关闭 loop pass 仍能生成合法基线；持久化旧 profile 可忽略而不是硬链接错误。

**文档交付：** 新增 docs/instruction-generation/loop-pgo-specialization.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrProfile SZrExecIrProfile;
typedef struct SZrExecIrLoopInfo SZrExecIrLoopInfo;
TZrBool ZrParser_ExecIr_ImportProfile(SZrExecIrModule *module,
    const SZrExecIrProfile *profile, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_OptimizeLoops(SZrExecIrFunction *function,
    SZrExecIrLoopInfo *loops, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| stable site IDs | canonical source/IR lowering | profile 跨进程匹配，不用代码地址 |
| loop invariance | dominance/alias/effect/exception 分析 | LICM 保留零迭代语义 |
| specialization budget | module profile policy | site/function/module 三层累计限制 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先导入/导出 profile schema，逐字段 mismatch 与缺失信息测试。

- [ ] **批次 2：** 实现 loop forest、preheader 和 LICM，再做 induction/strength reduction；不可约循环明确跳过。

- [ ] **批次 3：** 接热度策略和退避；长期跑频繁 receiver 变化场景，验证代码 cache/版本数不上涨无界。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange zero-trip loop containing division by zero
assert LICM does not introduce exception before loop
arrange profile with same module but different IR hash
assert ignored with PROFILE_STALE
arrange repeated deopt at same site
assert specialization count bounded and cooldown active
```

### 迁移结束检查

任何配置阈值必须在模块 policy 中命名，profile quality 和 guard correctness 独立。无 profile 的编译保留完整合法行为。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

