---
related_code:
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_core/include/zr_vm_core/execution_frame_layout.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_frame_layout.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/test_value_copy_fast_paths.c
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - docs/acceptance/ssa-dead-source-places.md
  - tests/parser/test_ssa_host_primitive_layout.c
  - docs/acceptance/ssa-host-primitive-layout.md
  - tests/parser/test_ssa_primitive_source_frame.c
  - docs/acceptance/ssa-primitive-source-frame.md
doc_type: milestone-detail
status: planned
---

# 04.01 Packed frame 与精确存储布局

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立精确 storage slot count 和 packed frame 表示，让纯标量只保留必要 payload。

**Architecture：** 逻辑 slot、物理 offset/stride、ownership 状态和 root bitmap 分离；同一 frame descriptor 被 ExecBC、AOT、JIT 消费。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2。
- 前置：[01.04 Ownership、挂起与状态恢复映射](../01-execir-ssa/04-state-maps.md)；[01.05 直接解释 Oracle 与无优化后端投影](../01-execir-ssa/05-oracle-projections.md)。
- 交付：统一 frame layout builder、direct scalar mirror 协议与 inline struct byte span。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

### 真实 source 到 primitive frame 的有限前置

真实 literal i64 SCRIPT 的地址值没有 primitive physical layout 合同。
先由[dead-source places](../../../parser-and-semantics/ssa-dead-source-places.md)
证明并压缩 inert PLACE_BASE/address/provenance；其
[验收记录](../../../acceptance/ssa-dead-source-places.md) 记录已完成的 Windows
30/30 focused与8/8消费者有限GREEN；可据此推进独立 frame producer。
不能给残留地址猜测 uintptr 大小，也不能添加伪造 scratch proof。
后续 producer 应按真实 surviving ValueId/TypeId 使用调用方显式 target
layout rows；宿主 sizeof/alignof 只可作为明示 host-only adapter。初批使用
全函数重叠 lifetime、不复用槽，验证 geometry/mapping/hash 后事务附着。
空 state-map header 没有 layoutHash 字段，必须保留真实身份。
此有限前置不关闭 M2，不证明 native artifact retention 或全平台 ABI；SSA47 OPEN。

有限[host primitive layout adapter](../../../parser-and-semantics/ssa-host-primitive-layout.md)
按当前 host 的实际 canonical i64 `sizeof/alignof` 生产独立 layout row；
以31-byte domain加32-byte明确LE字段构成63-byte Stable64输入，排除本地
TypeId、layoutId、context地址、salt/time。它不附frame、不追加module行；
caller检查count+1后实际AppendLayout。当前行为验证见
[独立验收记录](../../../acceptance/ssa-host-primitive-layout.md)：实际source前置2/2
及compaction30/30后，stub的host15例12失败建立行为RED；实际实现GREEN通过
host15/15及compaction30/30，独立source前置2/2，MSVC仅新TU编译通过且有C4127。
既有packed-frame hash不含该row hash，后续AOT必须保留并消费真实layout table。
完整ABI、source provenance、frame/native/artifact retention与SSA47仍OPEN。

后续有限[primitive source frame](../../../parser-and-semantics/ssa-primitive-source-frame.md)
使用显式row geometry与全instruction lifetime的SCALAR请求，不复用槽，参数/
return buffer为0。输入数学storage检查后先CoreVERIFY_ALL、再SEALED/admission；
候选mapping/三counts/geometry及borrowed-view验证后只事务发布frame与
contract.layoutHash，保留empty state-map原pointer/header（没有layoutHash字段）。
接口不独立证明originalsource/context/callable，实际fixture携SemIR.callableTypeId
与同output/constants/layouttable到canonical AOT，NOARGS_I64且runnable=false。
行为门禁状态见[验收记录](../../../acceptance/ssa-primitive-source-frame.md)，
RED已在真实2/2前置及host15/compaction30通过后以18例9失败建立；当前GREEN19例
全通过（新增Core-valid definition0拒绝），host15/compaction30及独立2/2也通过，
新TU MSVC仅编译exit0。原始source/context/callable不由Attach独立认证；storage
preflight后逻辑Core错误保持精确diagnostic。framehash仅现有geometry，非ABI证书；
完整M2、native/retention/SSA47保持OPEN。

现有 function_frame_place、execution_inline_frame 已支持布局快路；测试 test_frame_slot_layout_lookup.c 正被其他任务修改，实施前核对 HEAD，避免覆盖。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/function_frame_place.c` | 逻辑 Place 到实际 slot 的投影 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c` | packed/inline 快路径复用 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/function.h` | frame descriptor 的归属和版本 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c` | 从 live range 与类型布局分配 storage |
| 计划新增 | `zr_vm_core/include/zr_vm_core/execution_frame_layout.h` | 统一逻辑/物理 frame contract |
| 计划新增测试 | `tests/core/test_ssa_frame_layout.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 审计重复表示** 列出 SZrTypeValue、byte span、scalar mirror 当前何处同时写；记录读者并定义同步边界，不能简单删掉仍被 GC/debug 使用的 mirror。

- [ ] **2. 生成布局** 参数 prefix、alignment、固定 stride、inline span、返回区和精确 slot count；生命周期不重叠才复用 storage，地址逃逸时固定位置。

- [ ] **3. 简化纯标量访问** 无 ownership/GC/动态观察的标量使用 payload；边界按需 materialize SZrTypeValue，而非每条算术都回写。

- [ ] **4. 统一 guard 和溢出** frame size/offset/stride 计算检查溢出，layout hash 包含类型/对齐/slot kind；错误 layout 不能 fallback 到猜测 offset。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
Slot = {logicalId, storageKind, byteOffset, byteSize, alignment, rootKind}
Frame = {argumentPrefixBytes, storageBytes, storageSlotCount, returnArea, layoutHash}
allocateSlotsByLiveRanges(pinnedPlacesMustNotMove)
scalarWrite -> payloadOnly
observableBoundary -> materializeRequiredLogicalValues
resumeBoundary -> reloadAndInvalidateStaleMirrors
```

不能把所有值统一写成固定大槽以满足接口；inline struct 按 layout span 传递。未初始化 slot 不得被 GC 或析构扫描。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 零参数/零局部、奇数对齐 inline struct | size/offset 正确 |
| 活跃性复用与地址逃逸 | 只复用安全 slot |
| GC/debug/native 观察标量 frame | materialize 完整且没有脏 mirror |
| 巨型 frame 尺寸溢出 | 分配前报错 |

复用回归入口：`tests/core/test_frame_slot_layout_lookup.c`、`tests/core/test_value_copy_fast_paths.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_frame_layout` 和可执行目标 `zr_vm_ssa_frame_layout_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_frame_layout_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_frame_layout$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 所有 frame access 使用统一 descriptor；复制数量下降有证据且 root/drop/debug 行为不变。

**失败恢复：** 可对未覆盖 storage kind 保留完整值表示；不能绕过 layout hash 检查。

**文档交付：** 新增 docs/core-runtime/packed-frame-layout.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrFrameLayout SZrExecIrFrameLayout;
typedef struct SZrFrameLayoutRequest SZrFrameLayoutRequest;
TZrBool ZrParser_ExecIr_LayoutFrame(const SZrFrameLayoutRequest *request,
    SZrExecIrFrameLayout *layout, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| logical slots/live ranges | SSA liveness/place analysis | slot allocator 区分 address-stable 与可复用 |
| physical offset/root kind | frame layout builder | 所有后端和 observer 读同一 descriptor |
| scalar mirror validity | call/debug/native 边界 | 只同步必要值，失效后不读旧 mirror |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 审计每种 frame 值当前双写点并制作 reader/writer 表；只迁移 descriptor 查询先保持行为。

- [ ] **批次 2：** 实施 live-range slot reuse、alignment 与 scalar payload，逐 storage kind 推进。

- [ ] **批次 3：** 接观察/GC/native materialization，测量 frame bytes 和真实 copy 次数。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange scalar live ranges overlap
assert separate storage; nonoverlapping eligible ranges may reuse
arrange address-taken local
assert byte offset stable for entire borrow lifetime
arrange inline struct with managed fields and odd alignment
assert descriptor root offsets and total aligned size exact
```

### 迁移结束检查

不能残留按旧最大 stack count 扫描全部 frame 的代码。storageSlotCount 与 logical slot count 分别命名，所有 offset 计算用 checked helper。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
