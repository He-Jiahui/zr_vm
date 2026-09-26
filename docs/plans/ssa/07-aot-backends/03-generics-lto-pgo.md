---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_parser/CMakeLists.txt
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_generic_policy.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_generics_lto_pgo.c
doc_type: milestone-detail
status: planned
---

# 07.03 泛型、LTO、PGO 与裁剪

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 用自动泛型策略、LTO/ThinLTO、PGO 和可达性裁剪提升发布性能并控制体积。

**Architecture：** 泛型按 layout/ownership/effect contract 区分可共享 dictionary 与必须实例化；裁剪图从公开入口、capability、reflection、callback 和 patch roots 开始。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M4 发布优化。
- 前置：[07.02 C、LLVM 同源代码生成](../07-aot-backends/02-c-llvm-lowering.md)；[02.04 跨函数摘要、去虚化与内联](../02-automatic-optimization/04-interprocedural-inlining.md)；[02.05 循环优化、PGO 与专门化预算](../02-automatic-optimization/05-loops-specialization.md)；[08.02 Capability manifest 与 Patch 验证](../08-artifact-hotpatch/02-capability-validation.md)。
- 交付：泛型 cost policy、发布链接配置、完整 reachability roots 和 profile 校验。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

AOT 已有 generic sharing 和 metadata registration 支撑，应收敛其 contract，不凭具体类型名判断何时 monomorphize。reachability 已有实现：`backend_aot_reachability.c/.h`（含 `EZrAotReachabilityReason` 的 ROOT_ENTRY/ROOT_EXPORT/MANIFEST/REFLECTION/NATIVE_CALLBACK/NATIVE_IMPORT 等 reason 分类）、`backend_aot_reachability_function_graph.c` 与 `backend_aot_c_type_layout_reachability.c`；本任务在既有 roots/edge reason 之上补齐 patch roots 与 linker/DCE/metadata stripping 的一致性，不新建平行可达图。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | generic dictionary 和 metadata ABI |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h` | 迁移已有 generic 描述 |
| 现有，修改/复用 | `zr_vm_parser/CMakeLists.txt` | 后端构建与链接选项边界 |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c` | 扩展既有可达图 roots/reason：补 patch roots、序列化类型与 capability 可调用入口 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_generic_policy.c` | layout/effect 驱动 sharing/实例化 |
| 计划新增 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_link_profile.c` | LTO/PGO 配置和工具链兼容校验 |
| 计划新增测试 | `tests/parser/test_ssa_generics_lto_pgo.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义共享 key** signature/layout/ownership/effect/target ABI 全部参与 dictionary 或实例化 key；引用类型可共享并不代表不同 inline struct ABI 也能共享。

- [ ] **2. 控制实例化爆炸** profile 热点优先、每模块 code budget、递归泛型深度限制；冷路径 dictionary sharing，remark 解释成本和阻断。

- [ ] **3. 正确构建裁剪 roots** 在既有 `EZrAotReachabilityReason` 分类基础上补齐：序列化类型、AOT capability 可调用入口和热更 future use 入 roots（导出、反射、native callback 已有对应 reason）；manifest 保留的能力不能被 linker 删除。

- [ ] **4. 接入发布优化** dev 不启 LTO；release-lto 使用工具链支持的 LTO/ThinLTO；release-pgo 验证 IR/compiler/target 指纹，不接受旧 profile。compile/link/size 额外成本单列。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
GenericKey = {signature, layoutClass, ownership, effects, targetABI}
if profitableHotSpecialization && withinCodeBudget: monomorphize()
else: dictionaryShareCompatibleRepresentations()
roots = exports + reflection + callbacks + serialization + capabilityEntries
        + patchReachableFunctions
live = transitiveClosure(roots, codeAndMetadataReferences)
stripOnlyUnreachable(live)
```

可 patch target 不可被无条件内联/constant-fold 成过期逻辑；LTO 仍受 patchability contract 约束。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 相同 layout 与不同 ownership 泛型 | 不错误共享 |
| 仅反射/native callback 可达方法 | 不被裁剪 |
| 热更可能调用但基包未调用能力 | 保留入口 |
| 旧 PGO 或 linker 不支持 ThinLTO | 明确降级/拒绝配置，不伪报启用 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_generics_lto_pgo` 和可执行目标 `zr_vm_ssa_generics_lto_pgo_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_generics_lto_pgo_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_generics_lto_pgo$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** native 结果与未经 LTO/PGO 版本一致；code size/compile/link/RSS 与收益一起报告。

**失败恢复：** 独立关闭 LTO、PGO、实例化策略；保守裁剪不会删合法入口，禁止删除元数据解决尺寸超标。

**文档交付：** 新增 docs/instruction-generation/aot-release-optimization.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrAotReachabilityRoots SZrAotReachabilityRoots;
typedef struct SZrAotLinkPlan SZrAotLinkPlan;
TZrBool ZrParser_AotIr_BuildLinkPlan(SZrAotIrModule *module,
    const SZrAotReachabilityRoots *roots, SZrAotLinkPlan *plan,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| generic key | layout/ownership/effect producer | sharing 与 monomorphization 不只看 TypeId |
| link roots | exports/reflection/callback/capability/patch | 裁剪不能删未来合法 patch 入口 |
| PGO/LTO config | effective build profile | 工具链不支持必须可见 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 审计现有 generic sharing，冻结 dictionary ABI 和尺寸预算。

- [ ] **批次 2：** 审计并扩展既有 reachability graph（backend_aot_reachability.c），先 dry-run 报告候选裁剪项并验证所有反射/patch roots。

- [ ] **批次 3：** 加入 LTO/ThinLTO/PGO 构建执行对照，确认 patchability 限制未被跨模块优化绕过。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange method only reachable through base capability manifest
assert retained even if current source never calls it
arrange generic args share layout but differ cleanup contract
assert no unsafe dictionary sharing
arrange LTO sees patchable call target
assert uses entry cell or valid invalidation protocol, no frozen inline body
```

### 迁移结束检查

链接器 section GC、编译器 DCE 与 metadata stripping 使用一致 roots。只统计源码可达函数会漏反射/native callback/热更能力。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

