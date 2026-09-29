---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/artifact_call_binding_projection.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis4.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis4.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_internal.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_read.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_write.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/artifact_call_binding_projection.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/execbc_verify.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_read.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_write.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
  - "user: 2026-09-28 EIS4 fixed scalar ADD payload"
  - "user: 2026-09-29 EIS5 dynamic counted scalar CFG payload"
  - "user: 2026-09-29 EIS5 BOOL predicate extension"
  - "user: 2026-09-29 EIS5 LT Compare extension"
  - "user: 2026-09-29 EIS5 six Compare modes extension"
  - "user: 2026-09-29 EIS5 scalar SUB extension"
  - "user: 2026-09-29 EIS5 scalar MUL extension"
  - "user: 2026-09-29 EIS5 scalar DIV extension"
  - "user: 2026-09-29 EIS5 two-DIV effect-chain extension"
tests:
  - tests/library/test_ssa_schema_relocation.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/parser/test_call_binding_artifact.c
  - tests/library/test_zrm_container.c
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_bool.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_compare.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_sub.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_mul.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_div.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_div_chain.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_add.inc
  - tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md
  - tests/acceptance/ssa-artifact-v6-eis3-counted-cfg.md
  - tests/acceptance/ssa-artifact-v6-eis4-scalar-add.md
  - tests/acceptance/ssa-artifact-v6-eis5-counted-cfg.md
  - tests/acceptance/ssa-artifact-v6-eis5-bool-predicate.md
  - tests/acceptance/ssa-artifact-v6-eis5-compare-lt.md
  - tests/acceptance/ssa-artifact-v6-eis5-compare-modes.md
  - tests/acceptance/ssa-artifact-v6-eis5-sub.md
  - tests/acceptance/ssa-artifact-v6-eis5-mul.md
  - tests/acceptance/ssa-artifact-v6-eis5-div.md
  - tests/acceptance/ssa-artifact-v6-eis5-div-chain.md
  - tests/acceptance/ssa-artifact-v6-eri1-relocation-boundary.md
doc_type: milestone-detail
status: planned
---

# 08.01 版本化 Artifact 与无地址 Relocation

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 一次升级 artifact schema/ABI，完整保存 ExecIR、ExecBC、binding 和状态映射，保证产物不含进程指针。

**Architecture：** 每个 section 显式长度/版本/必选标志，逐字段编码 token/index/offset/hash/relocation；加载验证完毕后才创建运行期 witness 和目标表。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

**首个持久子切片（2026-09-27）：** ZRAF v6、AOT ABI 17 和 ZRO
`EXEC_IR_BUNDLE` 已用于单函数 i64 CONSTANT→RETURN 的 EIS1 逐字段
writer/reader/Oracle 验证；见
[独立验收记录](../../../../tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md)。
该子切片不完成本计划的 ExecBC、binding、relocation、maps、copy、AOT
projection 或 `ImportByPath` 迁移，以下任务和退出门禁继续有效。

**后续 ERI1 边界子切片：** 在不改变 ZRAF schema 6、AOT ABI 17 或 ERI1
线格式的前提下，原始 reader 对每个目录项采用与 writer 一致的计数/槽宽
规则，全部目录和 hash 通过后才发布借用 view。原始 relocation 的
`codeOffset` 只允许落在 `EXEC_IR` 节内，所有行先验证再触发 resolver；
失败保留行与目标 token，结果数组不部分发布。见
[独立验收记录](../../../../tests/acceptance/ssa-artifact-v6-eri1-relocation-boundary.md)。
这只加固底层封套与解析接口；ZRAF canonical opener 继续拒绝 binding、
relocation、maps 和 ExecBC，不能据此勾选真正跨进程目标解析门禁。

**双块 CFG 持久子切片：** 现有 ZRAF v6/ABI 17/ERI1 v1 保持不变，
`EXEC_IR` payload 新增独立 EIS2 v2 逐字段线格式，保存一个无参数 i64
函数的 ENTRY BRANCH→第二块 CONSTANT→RETURN，以及显式双向边、支配块、
指令和结果/操作数池。原 EIS1 v1 的 412 字节 golden 不变；EIS2 为固定
564 字节。writer 仅在全图 Verify 且属于两种精确形状时选版；loader
临时解码并 Verify 后发布，错误边及错误版本拒绝且不发布图。独立跨进程
Oracle=42 验收见
[canonical ExecIR 验收记录](../../../../tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md)。
此扩展只覆盖一条无条件 CFG 边，不实现通用 CFG、binding、relocation、
maps、ExecBC 或 native AOT 调用，完整 08.01 的退出门禁仍未满足。

**固定标量 ADD 持久子切片：** 在既有 EIS1、EIS2、EIS3 payload 之外增加
独立 EIS4 v4，固定为 i64 常量 20、22 后执行 ADD 并返回，包含三个值、
一个基本块及显式结果/操作数池，精确长度为 716 字节。writer 与 reader
只接受这一完整图形；reader 校验固定 counts、字面量、操作数/结果范围和
空边范围后才发布 verified graph。EIS1/EIS2 golden 与 EIS3 线格式保持不变，
ZRAF v6、AOT ABI 17、ERI1 v1 不变。跨进程 Oracle=42 及畸形字段、重哈希
拒绝见[独立验收记录](../../../../tests/acceptance/ssa-artifact-v6-eis4-scalar-add.md)。
这不是通用算术或 CFG 编码；完整 08.01 的退出门禁仍未满足。

**动态计数 CFG 持久子切片：** EIS5 v5 使用变量长度、显式小端计数和
checked 64-bit 长度计算，支持一个无参数 i64 函数中的 CONSTANT、ADD、
BRANCH、CONDITIONAL_BRANCH、RETURN。块上限为 256，常量/值/指令总数上限
4096，四个 ID 池总数上限 16384，payload 上限 16 MiB。writer 在固定
EIS1–EIS4 形状后选择 EIS5，reader 在按上限和精确长度验证后才分配解码
数组，并在全图 Verify 后发布。五块双分支 fixture 为 1260 字节（该长度
不是格式固定宽度），跨进程 Oracle 分别得到 true=42、false=7。EIS1–EIS4
既有线格式、ZRAF v6、ERI1 v1、AOT ABI 17 保持不变。验收及失败注入见
[EIS5 counted CFG 记录](../../../../tests/acceptance/ssa-artifact-v6-eis5-counted-cfg.md)。
这不完成 ExecBC、maps、binding、relocation、package copy、AOT projection
或 `ImportByPath` 迁移；完整 08.01 的退出门禁仍未满足。
兼容旧 EIS1–EIS4 时，按计数、各块指令范围和操作码序列识别固定格式结构；计数相同但布局不同的合法图仍可使用 EIS5。

**EIS5 BOOL predicate 补充：** EIS5 v5 复用既有 `typeToken` 与 64 位
`bits` 字段，不改变 wire layout 或版本。BOOL 常量只接受 0/1，常量结果
必须保持常量类型一致；条件分支兼容既有 i64 predicate，也支持 BOOL，
ADD 与 RETURN 仍限 i64。跨进程 true=42/false=7、非规范 BOOL bits、类型
错配及 EIS1–E4 相邻回归见
[独立验收记录](../../../../tests/acceptance/ssa-artifact-v6-eis5-bool-predicate.md)。
旧 EIS5 v5 reader 会拒绝 BOOL token；完整 08.01 的剩余门禁仍未满足。

**EIS5 LT Compare 初始子切片：** 初始 Compare 子切片仅接受 LT（`typeToken`
值 1）；两个操作数为 i64，结果为 BOOL。历史 RED/GREEN 记录见
[LT Compare 验收记录](../../../../tests/acceptance/ssa-artifact-v6-eis5-compare-lt.md)。

**EIS5 六种 Compare 模式扩展：** 现有 v5 指令记录中的 `typeToken` 接受
六种 Oracle canonical 模式：EQ=0、LT=1、LE=2、GT=3、GE=4、NE=5。每种
模式都用一组 true 输入和一组 false 输入构图，验证 i64 输入、BOOL 结果，
再由条件分支选择 i64 42 或 7。payload 版本、字段顺序和记录宽度不变；
mode 6、非 i64 输入与非 BOOL 结果会被 writer、直接 reader 和 canonical
opener 拒绝，重哈希测试检查精确偏移及无部分发布。Clang 四项门禁为
`artifact_schema`、`ssa_schema_relocation`、`ssa_exec_ir_artifact_v6_write`
和 `ssa_exec_ir_artifact_v6_roundtrip`，Clang 与独立 MSVC 当前源码门禁均为
4/4 通过。EIS1–EIS4 golden/拒绝路径及已有 EIS5 i64、BOOL predicate、count
collision 和 COMPARE 回归保留。早于 Compare 的 EIS5 v5 reader 不识别
COMPARE opcode，会在指令记录处以 `INVALID_SECTION` 拒绝；此前仅支持 LT
的 v5 reader 则会在模式字段拒绝其他五种模式。版本未变，因此新模式不
具备对旧 reader 的前向兼容性。完整 08.01 的 binding、relocation、ExecBC、
AOT 等退出门禁仍未满足。实现范围与验证记录见
[六模式验收](../../../../tests/acceptance/ssa-artifact-v6-eis5-compare-modes.md)。

**EIS5 i64 SUB 标量子切片：** 在 counted v5 CFG 中增加单一 i64
`SUB` opcode，要求两个 i64 操作数、一个 i64 结果，不改变 header、记录
宽度或 payload 版本。`-11 - 4` 的 Oracle 结果为 `-15`（对应 ADD 会得到
`-7`），避免只靠 opcode 编号证明算术语义。两常量、三值、单块、四指令
fixture 长 716 字节，与 EIS4 固定 ADD 形状长度碰撞；EIS5 仍通过 magic 与
完整 legacy 结构识别路由。直接 codec 与 canonical writer/opener 覆盖
roundtrip；BOOL 输入/结果及当时尚不支持的 MUL 在 writer、reader 和 opener
边界拒绝，reader 不发布部分图。MUL 后续已由独立子切片支持；当前此处的
不支持 opcode 回归改为 ARITHMETIC。现有 EIS1–E4 goldens、EIS5 i64/BOOL
predicate、COMPARE 和计数碰撞回归保持通过。Clang 与独立 MSVC 的四项门禁
均为 4/4 通过。较早的 EIS5 v5 reader 不识别 SUB，会在指令记录处
拒绝；full 08.01 的其余 schema/relocation 退出门禁仍未满足。实现边界和
RED/GREEN 证据见
[EIS5 SUB 验收记录](../../../../tests/acceptance/ssa-artifact-v6-eis5-sub.md)。

**EIS5 i64 MUL 标量子切片：** counted v5 CFG 增加单一 `MUL` opcode，使
两个 i64 操作数产生一个 i64 结果，不改变版本或 wire layout。`6 * -7`
经 VerifyModule 和 Oracle 得到 `-42`，与 ADD、SUB 均不同；单块四指令
fixture 长 716 字节，与 EIS4 固定 ADD 形状长度碰撞。测试覆盖直接 codec
和跨进程 canonical writer/opener roundtrip，以及非 i64 输入/结果和
ARITHMETIC 的 writer、reader、opener 拒绝；reader 失败不发布部分图。Clang 与独立
MSVC 的四项门禁均为 4/4 通过。较早的 EIS5 v5 reader 不识别 MUL，会在
指令记录处拒绝；full 08.01 的退出门禁仍未满足。范围和验证记录见
[EIS5 MUL 验收](../../../../tests/acceptance/ssa-artifact-v6-eis5-mul.md)。

**EIS5 i64 DIV 标量子切片：** counted v5 CFG 增加一个 i64 `DIV` opcode，
两个 i64 操作数产生一个 i64 结果，不改变 header、版本或记录宽度。
VerifyModule 与 Oracle 接受 `84 / -7 = -12`；Oracle 在除数为零或
`INT64_MIN / -1` 时会在 C 除法前拒绝。DIV 必须带 `MAY_THROW` 和固定
effect token pair 1→2；范围只覆盖一个 DIV，不定义通用 effect chain。
直接 codec、canonical writer/opener 和跨进程读取验证 716 字节 EIS4 长度
碰撞；非 i64 输入/结果、缺失 flag、非规范 effect pair 及 ARITHMETIC 都
被 writer、reader 和 opener 拒绝，失败读取不发布部分图。旧 EIS5 writer
在 fixture 已由 VerifyModule/Oracle 接受后，test-only RED 精确失败于
DIV dynamic size query；Clang 与 MSVC 当前源码的 `artifact_schema`、
`ssa_schema_relocation`、write、roundtrip 四项均 4/4 通过。EIS1–EIS4
字节与拒绝路径保持不变；旧 v5 reader 在 opcode 记录处拒绝 DIV。SUB/MUL
因 DIV 现已加入 allowlist，其仍不支持 opcode 回归改用 ARITHMETIC。完整
08.01 的其他 binding、relocation、ExecBC、AOT 门禁仍未满足。实现范围见
[EIS5 DIV 验收](../../../../tests/acceptance/ssa-artifact-v6-eis5-div.md)。

**EIS5 双 DIV effect-chain 子切片（已验证）：** 允许一个函数在同一直线
block 内依次执行最多两条 i64 `DIV`，分别使用 `MAY_THROW` 与 effect token
1→2、2→3；EIS5 v5 header 和记录宽度保持不变，其他指令的 effect 字段仍
必须为零。test-only RED 构图先通过 VerifyModule 和 Oracle，计算
`(84 / -7) / 3 = -4`，随后旧 writer 在第二条 DIV 的 effectIn 偏移 820
返回 `INVALID_SECTION`，命中原有固定 1→2 检查。独立 MSVC 目标构建及
`artifact_schema`、`ssa_schema_relocation`、
`ssa_exec_ir_artifact_v6_write`、`ssa_exec_ir_artifact_v6_roundtrip` 四项
CTest 已通过。Clang 目标构建命令完成 359/359、退出码 0；原 CMake job
经过 VerifyGlobs 和自动重新生成后自然继续完成，未使用备用 harness，空的
scratch 目录已删除。Clang 当前源码四项 CTest 也全部通过；具体命令与耗时
见 [EIS5 双 DIV 验收记录](../../../../tests/acceptance/ssa-artifact-v6-eis5-div-chain.md)。
本子切片不表示完整 08.01 已完成。
旧 reader 若只支持单 DIV 会拒绝第二条 DIV 的 effect pair，早期 EIS5
reader 则在 opcode 处拒绝 DIV。
本叶不扩展跨 block 或任意长度 effect chain，也不关闭完整 08.01。
范围与当前 RED 见
[EIS5 双 DIV 验收](../../../../tests/acceptance/ssa-artifact-v6-eis5-div-chain.md)。

## 依赖与交付范围

- 对应主计划：M1 artifact；M5 基础。
- 前置：[01.04 Ownership、挂起与状态恢复映射](../01-execir-ssa/04-state-maps.md)；[03.03 已解析目标、PIC 与 Guard 失效](../03-interpreter-binding/03-guarded-caches.md)；[00.02 共享契约、语义边界与版本冻结](../00-measurement-contracts/02-contract-freeze.md)；[03.04 自动组合指令与 ExecBC 编码投影](../03-interpreter-binding/04-generated-fusion.md)。
- 交付：.zro/.zrm write/read/copy/AOT projection、旧版本拒绝与结构化加载错误。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

本计划起始基线为 artifact schema 5、AOT ABI 16；首个持久子切片已升为
schema 6、ABI 17。CallBinding source row 84 字节和 canonical row 96 字节是
不同封装，不可直接 memcpy 混用。现有 writer_call_binding 与
artifact_call_binding_projection 已有基础。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/artifact_schema.h` | 分配版本和 section registry |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c` | 写入新 sections |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c` | 复用逐字段 binding 编码 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/artifact_call_binding_projection.c` | 统一 canonical projection |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/module/module_loader.c` | 全量验证后加载 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c` | 运行期 relocation |
| 计划新增 | `zr_vm_core/src/zr_vm_core/artifact_exec_ir.c` | ExecIR/map section codec 与界限检查 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/exec_ir/execbc_verify.c` | 验证 ExecBC-only 的 CFG、typed slots、效果、binding 与 maps；M5 的 ExecBC-only 路径以此为门禁。目录归属说明：core 侧 `exec_ir/` 子目录与 01.01 的 `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c` 共用，只放运行期只读模型与验证；builder/pass 仍在 parser 侧，不在两处各建一套 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c` | 稳定编码投影 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/artifact_exec_ir.h` | 磁盘字段契约 |
| 计划新增测试 | `tests/library/test_ssa_schema_relocation.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 冻结 section 目录** ExecIR、ExecBC、binding、hash/layout、GC/EH/deopt/debug、remarks 摘要、capability、profile hints、target contract；计算现有 section 上限是否足够，修改上限必须有资源边界测试。

- [ ] **2. 定义编码与 hash** 明确 endian、字段宽度、对齐、canonical 顺序、hash 包含/排除项；磁盘 generation 是逻辑版本，runtime epoch 不直接反序列化为有效 guard。

- [ ] **3. 安全读写和压缩** 先检 header/section 范围/重叠/数量/解压限额，再解码并 Verify；压缩 ExecBC 加载后恢复固定宽度，不借机改变指令格式。

- [ ] **4. 后置 relocation** 验证 signature/module/layout/ABI 后依据 token/relocation 解析 VM/native/AOT；失败清理临时 graph，禁止半加载可调用目标。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
write = encodeFields(canonicalContractsAndIR); never memcpy(runtimeStruct)
load(bytes):
    validateHeaderVersionAndLimits()
    validateSectionsNoOverlapAndBoundedDecompression()
    decodeCanonicalRecords(); VerifyExecIRAndMaps()
    validateHashesAndTargetContract()
    resolveTokensToProcessLocalTargets()
    publishOnlyCompleteModule()
oldVersion -> RECOMPILE_REQUIRED
```

native binary 自身合法的链接重定位与 .zro/.zrm 内 runtime 指针序列化不是一回事。裸地址扫描只是回归辅助；真正保证来自 schema 白名单和逐字段编码。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| write/read/copy/AOT projection | token/signature/layout/module/maps 保持 |
| 截断、重叠 section、未知必选项、压缩炸弹 | 执行前拒绝 |
| 注入 sentinel VM/native/AOT pointer 到 witness | 字节中不出现其编码，跨进程重新解析 |
| schema/ABI 旧版本或 target mismatch | 明确重编译/不兼容错误 |

复用回归入口：`tests/parser/test_artifact_schema_source_roundtrip.c`、`tests/parser/test_call_binding_artifact.c`、`tests/library/test_zrm_container.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_schema_relocation` 和可执行目标 `zr_vm_ssa_schema_relocation_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_schema_relocation_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_schema_relocation$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 跨进程 roundtrip 与恶意 artifact 测试通过，runtime target 从不进入 persistent 编码，版本号取实施时最新下一版。

**失败恢复：** 新 reader 不宽松接受旧版本；发布前回退需成套回退 writer/loader/ABI，不产生混合 schema。

**文档交付：** 更新 docs/module-system/artifact-schema.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrReader SZrExecIrReader;
typedef struct SZrExecIrWriter SZrExecIrWriter;
TZrBool ZrParser_ExecIr_Read(SZrExecIrReader *reader,
    SZrExecIrModule *module, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_Write(const SZrExecIrModule *module,
    SZrExecIrWriter *writer, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| section/schema rows | writer canonical encoder | 禁止 memcpy runtime structs |
| decoded untrusted graph | loader staging | 结构/语义/资源上限验证后才 relocation |
| resolved target table | metadata runtime binding | 每进程新建 witness，不接受磁盘地址 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 冻结 byte-level schema、必选 section 与 migration error，添加 golden roundtrip fixture。

- [ ] **批次 2：** 实现 writer/reader/copy/AOT projection，分别验证 84-byte source row 与 96-byte canonical row 的转换。

- [ ] **批次 3：** 做跨进程 relocation、畸形/重叠/压缩超限、sentinel pointer scan 和 schema fuzz。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange runtime witness pointer=P then write artifact twice under different ASLR
assert persistent bytes identical except explicitly variable metadata
arrange overlapping sections or length arithmetic overflow
assert reject before allocation/dereference
arrange decode valid token with missing native registration
assert structured link failure and no published module
```

### 迁移结束检查

artifact pointer-free 的证明是编码字段白名单+独立解码+跨进程 roundtrip，字节扫描只是辅助反例检测。JIT executable pages 和 runtime import pointers 一律不可序列化。

### ExecIR 与 ExecBC 的一致性不能只靠两个独立 hash

同一包同时保存 canonical ExecIR 和 ExecBC 时，合法 hash 只证明各自字节未变，不证明两者语义相同。加载受限 patch 的首版策略是：验证 canonical ExecIR 后，用固定版本的可信 lowering 重新生成执行 ExecBC，将缓存的 ExecBC 视作可丢弃加速数据。要直接采用缓存，必须验证 canonical IR hash、lowering 版本、配置 hash、完整映射，并由可信构建签名覆盖该派生关系；不可信 producer 的两个自报 hash 不能成为证明。

若支持 ExecBC-only patch，则必须增加独立的 bytecode verifier：校验控制流/操作数/slot、所有调用绑定、能力效果、异常/cleanup/GC map 和 branch target，禁止仅验证缺省 ExecIR 后执行另一份未验证代码。该能力未完成前明确拒绝 ExecBC-only patch，不影响完整 ExecIR+ExecBC 包。

本主线的 M5 要求支持 ExecBC patch，因此把上述 verifier 列为必须完成的批次，不能把临时拒绝状态当作最终交付。复用 03.04 的生成 opcode schema 展开组合指令的验证语义，做 typed slot 数据流与 effect/capability 汇总；未知 mandatory opcode 或缺失必要 map 拒绝。这里的展开只用于检查不可信字节码，AOT 后端仍禁止从 quickened bytecode 恢复代码生成语义。

- [ ] 负测：IR 只调用允许目标 A，缓存 ExecBC 被换成目标 B；即使攻击者重算非签名 hash，也不能执行 B。
- [ ] 负测：缓存 ExecBC source/map 与 IR 不对应；重新 lowering 或拒绝，不能套用错误 root/deopt map 执行。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
