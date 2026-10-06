---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_constant_consumers.c
  - zr_vm_parser/CMakeLists.txt
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_constant_consumers.c
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
plan_sources:
  - .codex/plans/20261005-ssa-host-aot-target.md
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - tests/parser/test_ssa_aotir_contract.c
  - tests/parser/test_aot_c_frame_setup_contracts.c
  - tests/parser/test_call_binding_aot_projection.c
doc_type: milestone-detail
status: planned
---

# 07.01 共享 AOTIR 与后端 ABI 收敛

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 把 AOT 私有执行描述收敛为共享 ExecIR→AOTIR 契约，切断从 quickened bytecode 反推语义的依赖。

**Architecture：** AOTIR 保留 typed operations、CFG、effect 和逻辑 state maps，只补 ABI/legalization/物理布局；C/LLVM 后端共享它，不再各自判定所有权和调用目标。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 有限宿主 target 前置（有限 Windows gate GREEN）

[Win64 无参数 INT64 target](../../../parser-and-semantics/ssa-host-aot-target.md)
是本计划的有限前置切片：仅从真实 canonical callable 与重算的 host layout
row 产生现有 Core target record，并固定独立的 restricted callable ABI hash。
实际两例 prerequisite 通过后，stub 的三例 feature RED 已提交；r2 三套48例
Unity 全通过，MSVC 单 TU 仅编译检查零警告通过。聚合 driver 退出未观察到，见
[验收记录](../../../acceptance/ssa-host-aot-target.md)与
[测试指南](../../../../tests/acceptance/ssa-host-aot-target.md)。
它不构成 Common 完整平台 ABI、module descriptor binding、真实 frame scalar
emitter、native execution 或 retention 验收。Linux、完整 MSVC、完整 SSA47
及本文件全部未勾选迁移门禁保持 OPEN；不得据此标记本计划完成。

## 依赖与交付范围

- 对应主计划：M4 foundation。本任务是 M1 四后端语义门禁（由 07.02 完成）的前置输入，自身不等待该门禁完成；见 index“执行依赖与阶段门禁说明”对 07.01 的防循环约定。
- 前置：[01.05 直接解释 Oracle 与无优化后端投影](../01-execir-ssa/05-oracle-projections.md)；[04.03 Native、FFI 与跨后端桥接](../04-frame-native/03-native-abi.md)；[04.04 精确根、Frame 重定位与调试观察](../04-frame-native/04-roots-observation.md)。
- 交付：AOTIR 模型、统一 lowering、旧 emitter adapter 迁移清单。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

zr_vm_parser/CMakeLists.txt 已将 zr_vm_aot 下源码编入 parser。backend_aot_exec_ir.h 是现有迁移入口；后端目录名字不能被当成未接入的证据。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h` | 将旧类型适配到共享描述 |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_constant_consumers.c` | 清除对旧指令序列的语义恢复 |
| 现有，修改/复用 | `zr_vm_parser/CMakeLists.txt` | 显式声明后端模块输入和依赖 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/aot_ir.h` | 只读 AOTIR/ABI legalization contract |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c` | 从 SSA 生成 AOTIR |
| 计划新增 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c` | 过渡适配及覆盖诊断 |
| 计划新增测试 | `tests/parser/test_ssa_aotir_contract.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 列 emitter 消费矩阵** 按 scalar/control/call/member/property/array/string/inline/exception/async 标出现读取 ExecBC、常量槽和 runtime helper 的位置；每类替换为 AOTIR 字段。

- [ ] **2. 统一 ABI legalization** typed signature、frame layout、roots/EH/deopt/debug、generic dictionary、native import 一次生成；target triple 仅影响合法表示和 ABI，不影响语言结果。

- [ ] **3. 清理 reverse decoding** 迁移一个操作族就删除该族 quickened opcode 反解；AOTIR 中不能保存供 emitter 猜语义的原 SZrInstruction。

- [ ] **4. 建立无优化基线** 不依赖 M6 高级 passes 即可生成 typed scalar/control/call 的 AOTIR；不支持项显式诊断并进入 coverage 报告。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
AotModule = Legalize(ExecIR, TargetABI, CapabilityContract)
AotFunction = {typedSignature, blocks, frameLayout, gcMap, ehMap,
               cleanupMap, debugMap, deoptMap, imports, patchPolicy}
CEmitter(AotModule)
LLVMEmitter(AotModule)
assert(noSemanticDependencyOnQuickenedExecBC)
unsupportedOperation -> explicitLoweringDiagnostic
```

AOTIR 不能成为第三套语言语义；它是 ExecIR 的 ABI 投影。C11 core 不因 LLVM 后端而全局转成 C++。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 禁用 ExecBC 生成仍输出 AOTIR | 成功 |
| 同 ExecIR 输出 C/LLVM 描述 | signature/root/EH/layout hash 相同 |
| 目标 ABI 对齐不同 | 合法调整物理布局且保持逻辑语义 |
| 不支持 operation | 可定位诊断而非隐含 interpreter |

复用回归入口：`tests/parser/test_aot_c_frame_setup_contracts.c`、`tests/parser/test_call_binding_aot_projection.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_aotir_contract` 和可执行目标 `zr_vm_ssa_aotir_contract_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_aotir_contract_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_aotir_contract$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 所有 emitter 输入字段有唯一生产方；无优化 AOTIR 验证通过，覆盖缺口明确。

**失败恢复：** 未迁移操作族暂用显式 legacy backend mode 对照，不能把混合模式冒充统一后端完成。

**文档交付：** 更新 docs/plans/aot 的关联图并新增 docs/instruction-generation/aotir-contract.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrAotTargetContract SZrAotTargetContract;
TZrBool ZrParser_ExecIr_LegalizeAot(const SZrExecIrModule *input,
    const SZrAotTargetContract *target, SZrAotIrModule *output,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| typed operations/effects | ExecIR | AOTIR 不从 legacy opcode 补语义 |
| ABI legalized frame/calls | 共享 lowering | C/LLVM 只做 emission，不各推 signature |
| coverage/support status | operation lowering | 生成代码与报告使用同一支持表 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 列旧 AOT emitter 的输入来源与反解点，逐个指定 shared field 替代。

- [ ] **批次 2：** 建立无优化 AOTIR 与 target legalization，冻结 frame/native/root/EH/debug ABI。

- [ ] **批次 3：** 先适配一个 scalar/control/call vertical slice，再按族迁移，删除对应 reverse decoder。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange source compilation disables ExecBC entirely
assert AOTIR still has typed calls and control graph
arrange same input lowered to C and LLVM
assert shared metadata hashes match
arrange emitter requests original bytecode PC to infer type
assert architectural dependency check rejects that path
```

### 迁移结束检查

现有 AOT 源码已编进 parser，这里是语义输入迁移，不是重新启动 AOT 项目。避免为方便 include 使 core 依赖 zr_vm_aot 私有目录。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
