---
related_code:
  - tests/benchmarks/registry.cmake
implementation_files:
  - tests/benchmarks/registry.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c
  - zr_vm_library/src/zr_vm_library/batch_protocol.c
  - zr_vm_core/include/zr_vm_core/batch_contract.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/library/test_ssa_batch_vectorization.c
doc_type: milestone-detail
status: planned
---

# 09.03 Batch、矩阵与自动向量化

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 以连续数据、别名和循环证明自动生成 batch/vector/matrix 快路，并提供低成本标量实现。

**Architecture：** 先标准库协议与 scalar baseline，再自动 vectorize/fuse 合法循环；成本模型包含 trip count、alignment、tail、bridge 和 code size。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M7。
- 前置：[09.02 严格数值、Fast-math 许可与向量 IR](../09-language-simd/02-numeric-vector-ir.md)；[05.04 Aggregate 标量替换与 AoS/SoA](../05-data-layout/04-aggregate-soa.md)；[02.05 循环优化、PGO 与专门化预算](../02-automatic-optimization/05-loops-specialization.md)。
- 交付：batch/vector/matrix protocol、自动向量化 pass 和代表 workload。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

matrix_add_2d 等 benchmark 可复用；不能靠只新增 SIMD intrinsic API 然后要求业务重写才声称语义优化自动化。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `tests/benchmarks/registry.cmake` | matrix/array/batch 基准登记 |
| 前置计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c` | 由 02.05 提供循环证明，并非当前已有文件 |
| 前置计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c` | 由 05.02 提供连续访问，并非当前已有文件 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c` | 依赖分析和 vector/tail 生成 |
| 计划新增 | `zr_vm_library/src/zr_vm_library/batch_protocol.c` | 通用批量操作 scalar baseline |
| 计划新增 | `zr_vm_core/include/zr_vm_core/batch_contract.h` | shape/stride/effect/alias 描述 |
| 计划新增测试 | `tests/library/test_ssa_batch_vectorization.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义最小通用能力** map/zip/elementwise/matrix stride 访问先实现标量协议；immutable/persistent container 不强制连续，可用显式 materialization，成本计入。

- [ ] **2. 证明依赖** 分析循环携带依赖、读写 alias、bounds、stride/alignment；运行期 disjoint guard 只在可恢复点使用，失败走相同顺序标量循环。

- [ ] **3. 生成向量和 tail** 按 target width 划 lane，余数 loop 或安全 mask，禁止越界读取；strict reduction 不重排，fast-math 必须取得 09.02 许可。

- [ ] **4. 自动成本选择** 小 trip count、重桥接、未知 stride 可能不向量化；remark 给出拒绝原因、估计 bytes、实测 profile，代码版本数受 02.05 预算约束。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
if loopHasNoForbiddenDependence && boundsProven && profitable:
    if aliasUnknownButCheckable: emitDisjointGuard()
    emitVectorBodyWithNumericPolicy()
    emitSafeTailOrMaskedBody()
else: preserveScalarLoop()
LoopFusion only if effectOrderAndExceptionOrderAreEquivalent
MatrixLayout = {rows, columns, rowStride, columnStride, elementLayout}
```

mask 并不总能消除平台实际越界访问，legalizer 必须保证 fault behavior；并行 reduction 的确定性是独立语义选择。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 长度 0/1/lanes-1/lanes+1 | tail 不越界 |
| 输入输出重叠与负/非单位 stride | 正确 guard/fallback |
| 会抛错的 elementwise callback | 保留顺序或不融合 |
| strict 与许可 fast-math 矩阵 | 分别验证数值规则与收益 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_batch_vectorization` 和可执行目标 `zr_vm_ssa_batch_vectorization_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_batch_vectorization_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_batch_vectorization$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 未新增业务性能标注即可识别普通合法循环；scalar/vector 双路径完整，真实收益覆盖 tail/转换成本。

**失败恢复：** 关闭 vector pass 保留库能力和标量语义；不支持平台不阻断构建。

**文档交付：** 新增 docs/library-and-builtins/batch-vector-matrix.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrVectorizeOptions SZrExecIrVectorizeOptions;
TZrBool ZrParser_ExecIr_VectorizeLoops(SZrExecIrFunction *function,
    const SZrExecIrVectorizeOptions *options, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| trip/alias/bounds | loop + range analysis | vector legality 与 profitability 分开 |
| batch shape/stride | 通用库协议 | 非连续 storage 不假装 packed |
| tail/mask plan | legalizer | 任何 lane 的访问都不越界 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现 batch/matrix scalar 协议，固定 shape/stride/error 和 alias 行为。

- [ ] **批次 2：** 加入无跨迭代依赖的 elementwise vectorization，分别测试 alias guard 和 tail。

- [ ] **批次 3：** 再做严格允许的融合/reduction，按 profile 评估转换与 code-size 成本。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange lengths 0,1,W-1,W,W+1 where W is vector width
assert no out-of-range load/store
arrange overlapping input/output with loop-carried dependency
assert scalar path or proven-safe schedule
arrange tiny trip count with costly materialization
assert vectorization may be rejected with cost reason
```

### 迁移结束检查

把手写 intrinsic 封装进标准库不是自动向量化交付的全部；必须有普通循环经语义证明自动 lowering 的测试。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
