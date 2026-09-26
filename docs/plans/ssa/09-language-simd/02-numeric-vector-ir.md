---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_cli/src/zr_vm_cli/project/project.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
  - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_numeric_vector_ir.c
doc_type: milestone-detail
status: planned
---

# 09.02 严格数值、Fast-math 许可与向量 IR

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 定义严格数值与可移植 vector IR，并把 fast-math 限制为可审计的项目/包许可。

**Architecture：** 数值 profile 随 module/backend/capability contract 传播；默认 scalar/vector 保持相同整数、IEEE、NaN/±0 和异常规则，平台不支持则 scalar fallback。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M7。
- 前置：[09.01 自动语义摘要与通用能力协议](../09-language-simd/01-inferred-protocols.md)；[07.01 共享 AOTIR 与后端 ABI 收敛](../07-aot-backends/01-aotir-contract.md)。
- 交付：numeric contract、向量类型/legalization、项目许可和严格 fallback。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

主索引已选项目/包级 fast-math，无需每函数标注；不能把所有 fast-math 变换当作一个无法解释的 bool。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 数值/profile ABI 标识 |
| 现有，修改/复用 | `zr_vm_cli/src/zr_vm_cli/project/project.c` | 项目级许可读取 |
| 前置计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c` | 由 07.01 提供；本任务接入向量 legalization，并非当前已有文件 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/exec_ir_numeric.h` | 标量/向量 numeric contract |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c` | lane/type/target legalization |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c` | 细粒度 fast-math 许可校验 |
| 计划新增测试 | `tests/parser/test_ssa_numeric_vector_ir.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 冻结严格语义表** 逐操作列 checked/wrapping integer、shift mask、除零、NaN、±0、舍入/异常约定；区分 bitwise deterministic profile 和普通 strict，不能承诺任意平台数学库逐 bit 一致。

- [ ] **2. 定义 vector IR** 固定 lane 元素类型 f32/f64/整型、mask、load/store、arithmetic、select、reduction；表示正交，硬件宽度由 legalization 选择，不提前改 ExecBC 变长格式。

- [ ] **3. 分解 fast-math 权限** reassociate、contract/FMA、approx reciprocal、no-signed-zero、unordered reduction 分别记录；project/package 许可与 target/capability 求交，跨包调用保留各自语义。

- [ ] **4. 实现 scalar fallback** 不支持硬件 vector 或 alignment 不足仍执行同语义标量；strict ordered reduction 保持顺序，不能以 SIMD 名义改变求和结合顺序。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
NumericPolicy = {overflowMode, floatStrictness, determinism,
                 allowReassociate, allowFma, allowApprox, allowUnorderedReduce}
effectivePolicy = projectPermission ∩ packagePermission ∩ capability ∩ target
if strictReduction: preserveSourceOrder()
vectorLegalize(op, lanes) -> supportedTargetVector | equivalentScalarSequence
hash includes numericPolicy and targetContract
```

输入 NaN/±0 的可见语义不能只用 epsilon 比较掩盖；fast-math 报告启用了哪些变换与数值偏差，不声称严格等价。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| NaN/±0/无穷/极小数、整数边界 | 严格路径符合 contract |
| 只有 FMA 许可 | 不能启 unordered reduction |
| 没有 SIMD target | 标量 fallback 正确且模式可见 |
| 跨包 fast-math/strict 混合 | 许可不泄漏到 strict 函数 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_numeric_vector_ir` 和可执行目标 `zr_vm_ssa_numeric_vector_ir_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_numeric_vector_ir_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_numeric_vector_ir$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** numeric profile 进入 artifact/ABI/cache key；严格 vector 与 scalar 差分通过，放宽语义有显式许可和诊断。

**失败恢复：** 禁止的变换保留严格标量，不静默打开编译器全局 -ffast-math。

**文档交付：** 新增 docs/instruction-generation/numeric-vector-contract.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrNumericPolicy SZrNumericPolicy;
typedef struct SZrVectorLegalizationRequest SZrVectorLegalizationRequest;
TZrBool ZrParser_ExecIr_ValidateNumericPolicy(const SZrNumericPolicy *policy,
    SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_LegalizeVector(const SZrVectorLegalizationRequest *request,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| numeric permissions | project/package/host capability | effective policy 取共同允许范围 |
| strict operation semantics | 语言 contract | oracle/C/LLVM/JIT 使用相同规则 |
| vector legalization | target backend | 不支持 lanes/operation 生成等价 scalar |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 建立逐数值操作边界表和 oracle fixture，先明确 deterministic profile 范围。

- [ ] **批次 2：** 定义 vector/mask/reduction 与 target-independent scalar fallback。

- [ ] **批次 3：** 加入细粒度 fast-math flags 和 cache/artifact hash 传播，确保跨包 strict 调用不被污染。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange strict sum with order-sensitive inputs
assert no unordered reassociation
arrange FMA permitted but approximate division forbidden
assert only FMA transformation eligible
arrange no hardware vector support
assert same strict scalar result and explicit fallback representation
```

### 迁移结束检查

strict 不能只靠未设置 -ffast-math；C/LLVM 的 contraction、signed overflow 和 libm 差异都要逐项定义。启用 fast-math 的影响必须可查询。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
