---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_guarded_caches.c
  - tests/core/test_call_binding_runtime.c
  - tests/library/test_call_binding_relocation.c
  - tests/core/test_object_call_known_native_fast_path.c
doc_type: milestone-detail
status: planned
---

# 03.03 已解析目标、PIC 与 Guard 失效

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 复用已有 callsite cache 保存 VM/native/AOT resolved target，并对失效采取明确的语义路径。

**Architecture：** persistent contract 与 runtime witness 分离；loader 提前解析直接目标，receiver/shape/slot guard 只做必要校验。PIC 有界，closure context 受 GC tracing。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M3；与 M5 generation 协同。
- 前置：[03.02 成员链与调用 Binding facts 收敛](../03-interpreter-binding/02-static-binding-facts.md)；[01.04 Ownership、挂起与状态恢复映射](../01-execir-ssa/04-state-maps.md)。
- 交付：统一 guard/result 协议、cache 生命周期和冷错误路径。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

function.h 的 SZrFunctionCallSiteCacheEntry 和 call_binding.h 的 tagged target 已存在；禁止建立第二套按类型名或成员名选择目标的 cache。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/function.h` | 扩展/复用现有 cache 统计与 guard |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/call_binding_link.c` | 加载解析，保留精确错误 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/call_binding_member.c` | 按 receiver slot 校验 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c` | VM/native/AOT token 映射 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c` | 统一 generation/type/shape/layout/signature 判定 |
| 计划新增测试 | `tests/core/test_ssa_guarded_caches.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 区分稳定和动态字段** contract 只存 token/hash/slot；witness 保存 targetKind/pointer/generation/context，GC 遍历 callableObject，模块卸载前清除所有外部引用。

- [ ] **2. 预链接直接目标** loader 验证签名和布局后一次解析；native cached lane 复用已注册 callback，不重复 marshal 元数据或按名字解析。

- [ ] **3. 定义失败分类** 合法 shape 多态或 profile 假设失败回到已验证 token/slot 基线；stale generation、contract/hash 错直接结构化 link error。

- [ ] **4. 限制 PIC 和观测开销** 复用现有容量上限，记录 miss reason/命中/失效；megamorphic 转保守 slot 分派，计数可采样但不能影响 guard 正确性。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
if binding.generation != activeGeneration: return LINK_STALE
if signatureHash != expected or layoutContractInvalid: return LINK_CONTRACT
if receiverGuardMatches: return call(binding.target)
if receiverIsValidForDeclaredSlot:
    return genericSlotDispatch(binding.contract.dispatchSlot)
return RECEIVER_TYPE_ERROR
# never resolve a statically bound member by string
```

旧 frame 的 activeGeneration 来自其 version record，而非无条件使用最新全局 generation；M5 由 08.03 定义共存。target 自身布局 generation 和调用者 generation 不能混为一项。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| VM/native/AOT 与 typed closure | 目标正确、context GC 可达 |
| GC 移动 receiver/context | cache 无悬空引用 |
| 相同名称但不同 hash | 拒绝，不能误绑定 |
| 频繁 shape 变化与 reload | PIC 有界；miss/deopt/link error 分类准确 |

复用回归入口：`tests/core/test_call_binding_runtime.c`、`tests/library/test_call_binding_relocation.c`、`tests/core/test_object_call_known_native_fast_path.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_guarded_caches` 和可执行目标 `zr_vm_ssa_guarded_caches_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_guarded_caches_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_guarded_caches$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 静态站点无字符串回退，全部 target kind 和 guard failure 测试通过，模块/GC 生命周期可验证。

**失败恢复：** 可关闭 PIC 专门化改用已验证 slot 路径；不能关闭 signature/generation 检查。

**文档交付：** 更新 docs/core-runtime/call-binding.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecutionBindingGuardInput SZrExecutionBindingGuardInput;
typedef struct SZrExecutionBindingGuardResult SZrExecutionBindingGuardResult;
TZrBool ZrCore_Execution_CheckBindingGuard(
    const SZrExecutionBindingGuardInput *input,
    SZrExecutionBindingGuardResult *result);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| contract/witness | loader link 与 callsite cache | 快路径只比较 guard 并取已解析 target |
| receiver slot target | 合法多态 runtime dispatch | 不得按名称补找 |
| closure/context root | GC tracing | witness 不拥有裸可移动对象的未追踪别名 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 统一 guard result 枚举，将 stale/hash/type miss/shape speculation miss 分开并做负测。

- [ ] **批次 2：** 接 KNOWN_VM/KNOWN_NATIVE/META/accessor 到同一 witness，不更换其 receiver/ownership 语义。

- [ ] **批次 3：** 加入 PIC 预算和统计，跑 GC 移动、module reload、typed closure/native callback 矩阵。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange valid old frame with pinned version V1 while active=V2
assert V1-owned binding remains valid for that frame
arrange expired external binding without valid lease
assert STALE_GENERATION, nameLookupCount=0
arrange shape miss but declared interface slot valid
assert slot dispatch succeeds, no link-by-name
```

### 迁移结束检查

清理所有把 module active generation 无条件套到旧 frame 的快捷判断。cache reset 必须清除 target/context/guard 但保留合法 persistent contract。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

