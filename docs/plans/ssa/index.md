---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
implementation_files: []
plan_sources:
  - "user: 2026-09-12 SSA 全栈自动优化计划拆分与详细重构指导"
tests:
  - tests/parser/test_call_binding_pipeline.c
  - tests/core/test_call_binding_runtime.c
  - tests/library/test_call_binding_relocation.c
doc_type: category-index
status: planned
---

# ZR VM 全栈自动优化与统一后端计划

## 子计划导航与使用方式

本目录按 **12 个方向、47 个子计划** 拆分。每个方向内序号从 `01` 连续编号，文件格式为 `方向/nn-标题.md`；文档内另用 `方向序号.任务序号`（例如 `03.02`）标识依赖。所有子计划当前均为 **planned**，本文的代码入口核对不代表实现或运行验收已完成。

每份子计划包含：目标、前置依赖、已有/新增文件及职责、逐项重构任务、算法与接口草案、数据生产/消费关系、三批可审查实施步骤、正反向测试与断言、精确 CTest 目标、退出门禁、失败恢复和文档交付。C 接口和 fixture 断言是实施指导，新增类型/API 必须由所标任务实现；不能把草案视作当前已提供的接口。

先读 [00.01 基线、性能指标与真实执行模式](00-measurement-contracts/01-baseline-metrics.md)、[00.02 共享契约、语义边界与版本冻结](00-measurement-contracts/02-contract-freeze.md) 和 [00.03 差分测试支架与里程碑门禁](00-measurement-contracts/03-differential-harness.md)，再按叶子计划的依赖推进。实现代码时先读共享的 [实现架构指导：关键数据结构与函数设计](architecture-design.md)，它给出全计划统一的模块分层、ExecIR 数据模型、opcode 单源生成、verifier/pass/guard/frame/GC/artifact 关键结构与函数契约，以及最小可行实施链；其导读区链接的 guides/ 分册 A–E 进一步提供算法骨架、完整函数草案与常见坑清单。本文与叶子计划冲突时以叶子为准。目录顺序方便阅读，不是严格串行工期；高级优化可在基础 IR 验证可用后独立展开。main-only、自动语义推断、无性能标注负担、无静态字符串回退、无持久化裸地址等约束贯穿全部任务。

### 测量与基础契约（00-measurement-contracts）

- [00.01 基线、性能指标与真实执行模式](00-measurement-contracts/01-baseline-metrics.md)
- [00.02 共享契约、语义边界与版本冻结](00-measurement-contracts/02-contract-freeze.md)
- [00.03 差分测试支架与里程碑门禁](00-measurement-contracts/03-differential-harness.md)

### 统一执行 IR 与 SSA（01-execir-ssa）

- [01.01 ExecIR 数据模型与模块边界](01-execir-ssa/01-core-model.md)
- [01.02 Canonical facts lowering、CFG 与 SSA 构造](01-execir-ssa/02-ssa-construction.md)
- [01.03 Memory、Effect、异常边与验证器](01-execir-ssa/03-effects-verifier.md)
- [01.04 Ownership、挂起与状态恢复映射](01-execir-ssa/04-state-maps.md)
- [01.05 直接解释 Oracle 与无优化后端投影](01-execir-ssa/05-oracle-projections.md)

### 自动语义优化（02-automatic-optimization）

- [02.01 Pass 管理、SCCP、复制传播与 DCE](02-automatic-optimization/01-pass-manager-scalar.md)
- [02.02 别名分析、GVN 与范围证明](02-automatic-optimization/02-gvn-range.md)
- [02.03 逃逸、生命周期与分配消除](02-automatic-optimization/03-escape-ownership.md)
- [02.04 跨函数摘要、去虚化与内联](02-automatic-optimization/04-interprocedural-inlining.md)
- [02.05 循环优化、PGO 与专门化预算](02-automatic-optimization/05-loops-specialization.md)

### 指令执行与静态绑定（03-interpreter-binding）

- [03.01 Dispatch 拆分、状态缓存与冷路径](03-interpreter-binding/01-dispatch-boundaries.md)
- [03.02 成员链与调用 Binding facts 收敛](03-interpreter-binding/02-static-binding-facts.md)
- [03.03 已解析目标、PIC 与 Guard 失效](03-interpreter-binding/03-guarded-caches.md)
- [03.04 自动组合指令与 ExecBC 编码投影](03-interpreter-binding/04-generated-fusion.md)

### Frame、值与 Native 边界（04-frame-native）

- [04.01 Packed frame 与精确存储布局](04-frame-native/01-frame-layout.md)
- [04.02 调用返回、结果转发与尾调用复用](04-frame-native/02-call-return-tail.md)
- [04.03 Native、FFI 与跨后端桥接](04-frame-native/03-native-abi.md)
- [04.04 精确根、Frame 重定位与调试观察](04-frame-native/04-roots-observation.md)

### 对象、容器与数据布局（05-data-layout）

- [05.01 对象 Shape 与内部布局映射](05-data-layout/01-objects-layout-maps.md)
- [05.02 Typed array、连续 Slice 与 Bounds](05-data-layout/02-arrays-slices.md)
- [05.03 Map 与字符串的布局和分配](05-data-layout/03-maps-strings.md)
- [05.04 Aggregate 标量替换与 AoS/SoA](05-data-layout/04-aggregate-soa.md)

### GC、Domain 与调度（06-gc-domain）

- [06.01 Region、TLAB、Remembered set 与 Minor GC](06-gc-domain/01-young-allocation.md)
- [06.02 并发 Major、GC 预算与移动边界](06-gc-domain/02-major-budget.md)
- [06.03 同域多 Worker 与自动 Send/Sync](06-gc-domain/03-domain-sharing.md)
- [06.04 跨域结构化复制与生命周期](06-gc-domain/04-cross-domain-clone.md)
- [06.05 协程等待、后台编译与帧预算](06-gc-domain/05-async-frame-budget.md)

### 统一 AOT 后端（07-aot-backends）

- [07.01 共享 AOTIR 与后端 ABI 收敛](07-aot-backends/01-aotir-contract.md)
- [07.02 C、LLVM 同源代码生成](07-aot-backends/02-c-llvm-lowering.md)
- [07.03 泛型、LTO、PGO 与裁剪](07-aot-backends/03-generics-lto-pgo.md)
- [07.04 AOT Runner、覆盖率与发布门禁](07-aot-backends/04-aot-runner-coverage.md)

### Artifact 与受限热更新（08-artifact-hotpatch）

- [08.01 版本化 Artifact 与无地址 Relocation](08-artifact-hotpatch/01-schema-relocation.md)
  首个 ZRAF v6 / ABI 17 canonical ExecIR 文件子切片见[验收记录](../../../tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md)；完整 08.01 门禁仍待完成。
- [08.02 Capability manifest 与 Patch 验证](08-artifact-hotpatch/02-capability-validation.md)
- [08.03 Generation 发布、旧 Frame 与回收](08-artifact-hotpatch/03-generation-publication.md)
- [08.04 回滚、受限 Patch 与攻击面测试](08-artifact-hotpatch/04-rollback-restricted.md)

### 语言能力与严格 SIMD（09-language-simd）

- [09.01 自动语义摘要与通用能力协议](09-language-simd/01-inferred-protocols.md)
- [09.02 严格数值、Fast-math 许可与向量 IR](09-language-simd/02-numeric-vector-ir.md)
- [09.03 Batch、矩阵与自动向量化](09-language-simd/03-batch-vectorization.md)

### 最小 JIT 与平台（10-jit-platforms）

- [10.01 后端服务、异步编译与代码生命周期](10-jit-platforms/01-backend-service.md)
- [10.02 x86-64 与 AArch64 最小 JIT](10-jit-platforms/02-host-baseline-jit.md)
- [10.03 桌面、Android、iOS 与 WASM 平台验收](10-jit-platforms/03-platform-matrix.md)

### 配置、工具与总体验收（11-tooling-acceptance）

- [11.01 构建 Profile、增量缓存与脚本治理](11-tooling-acceptance/01-build-profiles.md)
- [11.02 优化说明 CLI、JSON 与 LSP](11-tooling-acceptance/02-optimization-remarks.md)
- [11.03 完整覆盖、文档与阶段收口](11-tooling-acceptance/03-release-acceptance.md)

## 执行依赖与阶段门禁说明

| 推进批次 | 任务范围 | 可交付状态与完整验收边界 |
| --- | --- | --- |
| A：建立基线与契约 | 00.01–00.03 | 可以冻结测量/契约；M0 完整 AOT runner 门禁等待 07.04。 |
| B：构建 IR 基础 | 01.01–01.05、03.02 | 先交付模型、SSA/verifier、状态 map、oracle、无优化投影；M1 四后端全语义门禁等待 07.02，不能提前勾选完成。 |
| C：完善运行基础 | 03.01/03.03、04.01–04.04、06.01–06.05 | Frame/root/native/GC/domain 各按前置依赖验收；GC/调度正确性先于快路收益。 |
| D：数据与自动优化 | 02.01–02.05、03.04、05.01–05.04 | pass 输入输出验证、失败原因和表示变换齐备；可以分批测量，不把“无错误”当作性能收益。 |
| E：产物和统一 AOT | 07.01、08.01、07.02、08.02、07.03/07.04 | schema 与 shared ABI 在依赖顺序内落地；补齐 M0/M1/M4 的跨后端门禁。 |
| F：安全发布与语言能力 | 08.03/08.04、09.01–09.03 | generation/回收先于热更；strict vector 先于 fast-math 专门化。 |
| G：可选 JIT 与整体收口 | 10.01–10.03、11.01–11.03 | 配置/remarks 可提前按依赖实现；JIT 不阻塞 AOT 发布；最终验收覆盖原 M0–M7 全范围。 |

上述是依赖阅读地图，精确依赖以每份叶子计划为准。特别地，07.01 不依赖 M1 四后端门禁“已完成”，只依赖 01.05 的基础交付以及 04.03/04.04 的 native/roots 契约输入；避免“M1 等 M4、M4 又等完整 M1”的循环。11.03 才汇总全范围收口。

## 实施边界补充

这些说明细化下文总体需求，避免工程实施产生不同解释。

1. **现状按源码与实测分开。** CallBinding 和三类测试已存在；AOT 后端源码目前由 `zr_vm_parser/CMakeLists.txt` 接入构建。历史验收文件记录过 GCC/Clang focused 通过，也记录 MSVC GC 崩溃与无效 Callgrind 样本；本次仅规划，没有重新运行这些 runtime 测试。
2. **CoreIR 统一指 ExecIR。** 唯一语义源是 typed SSA ExecIR；ExecBC 是解释器投影，AOTIR 是 ABI/codegen 投影。Oracle 复用 core 原语，不创建独立的第三套语言规则。
3. **自动化不放松合法性。** 优化证明不足保留合法原操作并产生 remark；非法 borrow、缺失静态 contract、歧义、signature/layout 不匹配仍编译/链接失败。
4. **隔离 domain 只复制。** 下文 Transfer/structured transfer 指结构化复制事务；materialized handle 必须创建独立且经宿主许可的目标资源，不共享源域 GC 指针或 ownership control。细节见 06.04。
5. **预算不等于硬实时。** 无完整 read barrier/indirection 协议时，半完成 evacuation/引用修复不得恢复 mutator；同步 API 不能透明变 async。不可中断段、超预算原因和 backpressure 必须报告。
6. **Guard miss 分两类。** 合法的推测失败回同 token/slot 的基线操作；stale/缺失/不匹配 contract 返回结构化 link error。静态站点始终无按名字兜底。
7. **热更需要入口及存活协议。** 旧 frame 合法使用所持版本，新入口取新版本；只有无有效 version lease 的失效 witness 才报 stale。可 patch 目标不能被未跟踪的 AOT direct call/内联绕过；回滚创建新 epoch，不能自动撤销外部 I/O。
8. **布局与权限各有边界。** 自动布局不改变公开 ABI、反射、序列化和地址可观察性；effect 摘要不是安全 capability。Patch 所需能力必须全部属于 host/base allowlist，不能把交集裁剪后当作允许执行。
9. **数据说明区分证据。** 软件 PIC miss、硬件 cache miss、静态 locality 估计分别记录。AOT 原生代码、原生 helper 和解释器 fallback 分开统计；90% coverage 与 3% 性能收益是两个门禁。
10. **版本号在实施时冻结。** 本计划基线为 schema 5 / ABI 16；首个持久 ExecIR 子切片已升级为 6 / 17。旧 artifact 明确要求重编译，JIT code/runtime pointer 从不进入 .zro/.zrm。
11. **无变长指令扩围。** 本主线保持现有固定宽度指令并使用 side tables；portable vector IR 不要求同时引入 opcode 前缀改造。平台受限热更是工程能力约束，不是分发审核保证。

## 需求覆盖索引

这是计划覆盖表，不是测试通过表。实施结果由 11.03 的 acceptance manifest 记录；每个子计划的接口、代码与测试证据必须落到同一版本快照。

| 原始需求范围 | 主责子计划 |
| --- | --- |
| 现有基线、persistent/同口径 3%、frame/GC p95/p99、环境指纹 | [00.01](00-measurement-contracts/01-baseline-metrics.md)、[00.03](00-measurement-contracts/03-differential-harness.md)、[07.04](07-aot-backends/04-aot-runner-coverage.md) |
| Canonical facts、typed SSA、CFG/dominators/phi、前后 verifier | [01.01](01-execir-ssa/01-core-model.md)、[01.02](01-execir-ssa/02-ssa-construction.md)、[01.03](01-execir-ssa/03-effects-verifier.md) |
| Memory/effect、异常/cleanup/await、GC/deopt/source map | [01.03](01-execir-ssa/03-effects-verifier.md)、[01.04](01-execir-ssa/04-state-maps.md)、[04.04](04-frame-native/04-roots-observation.md) |
| ExecIR oracle、ExecBC 投影、AOT 同源且不反解 bytecode | [01.05](01-execir-ssa/05-oracle-projections.md)、[07.01](07-aot-backends/01-aotir-contract.md)、[07.02](07-aot-backends/02-c-llvm-lowering.md) |
| 常量传播/SCCP/copy propagation/DCE、CSE/GVN、null/type/shape/bounds | [02.01](02-automatic-optimization/01-pass-manager-scalar.md)、[02.02](02-automatic-optimization/02-gvn-range.md) |
| ownership/escape/lifetime、栈/region 分配、copy/move/release、return forwarding | [02.03](02-automatic-optimization/03-escape-ownership.md)、[04.02](04-frame-native/02-call-return-tail.md)、[05.04](05-data-layout/04-aggregate-soa.md) |
| 自动函数摘要、静态绑定、去虚化/接口槽、guarded inlining | [02.04](02-automatic-optimization/04-interprocedural-inlining.md)、[03.02](03-interpreter-binding/02-static-binding-facts.md)、[03.03](03-interpreter-binding/03-guarded-caches.md)、[09.01](09-language-simd/01-inferred-protocols.md) |
| LICM/induction/strength reduction、PGO、版本/尺寸/命中率退避 | [02.05](02-automatic-optimization/05-loops-specialization.md)、[03.04](03-interpreter-binding/04-generated-fusion.md)、[07.03](07-aot-backends/03-generics-lto-pgo.md) |
| computed-goto/switch、局部状态缓存、hot/cold、固定宽度/side table | [03.01](03-interpreter-binding/01-dispatch-boundaries.md)、[03.04](03-interpreter-binding/04-generated-fusion.md) |
| 静态模块链、对象字段链、getter/setter/meta、virtual/interface/typed function | [03.02](03-interpreter-binding/02-static-binding-facts.md)、[03.03](03-interpreter-binding/03-guarded-caches.md)、[04.03](04-frame-native/03-native-abi.md) |
| 静态失败/歧义/signature/hash 诊断；禁止静态字符串回退 | [00.02](00-measurement-contracts/02-contract-freeze.md)、[03.02](03-interpreter-binding/02-static-binding-facts.md)、[03.03](03-interpreter-binding/03-guarded-caches.md)、[08.01](08-artifact-hotpatch/01-schema-relocation.md) |
| packed frame/参数 prefix/精确 slot/标量 payload/inline span/tail reuse | [04.01](04-frame-native/01-frame-layout.md)、[04.02](04-frame-native/02-call-return-tail.md)、[04.04](04-frame-native/04-roots-observation.md) |
| Native cached lane、marshal/直传、pin/root、异常、thread attach、callback lifetime | [04.03](04-frame-native/03-native-abi.md)、[04.04](04-frame-native/04-roots-observation.md) |
| 对象 shape/MemberId/offset、反射/序列化/FFI/公开布局 bridge | [05.01](05-data-layout/01-objects-layout-maps.md)、[05.04](05-data-layout/04-aggregate-soa.md) |
| typed dense arrays/小数组/capacity/view/slice、bounds proof | [05.02](05-data-layout/02-arrays-slices.md)、[09.01](09-language-simd/01-inferred-protocols.md) |
| map hash/紧凑 buckets、string intern/SSO/builder/rope 候选 | [05.03](05-data-layout/03-maps-strings.md) |
| SROA/unbox/字段重排/AoS-SoA、layout map 与可解释性 | [05.04](05-data-layout/04-aggregate-soa.md)、[11.02](11-tooling-acceptance/02-optimization-remarks.md) |
| region/TLAB/minor evacuation、cards/remembered set、精确 roots | [06.01](06-gc-domain/01-young-allocation.md)、[04.04](04-frame-native/04-roots-observation.md) |
| concurrent major/remark/sweep/compact、slice/debt/不可中断边界 | [06.02](06-gc-domain/02-major-budget.md) |
| 同域多 worker、自动 Send/Sync、同步可变共享 | [06.03](06-gc-domain/03-domain-sharing.md)、[09.01](09-language-simd/01-inferred-protocols.md) |
| 隔离域只复制、Transfer/structured clone、禁止跨域裸 GC 指针 | [06.04](06-gc-domain/04-cross-domain-clone.md) |
| 锁协程等待、后台编译、预热、execution budget/frame-safe | [06.05](06-gc-domain/05-async-frame-budget.md)、[10.01](10-jit-platforms/01-backend-service.md) |
| AOT C/LLVM typed scalar/control/call/member/container/inline/async | [07.01](07-aot-backends/01-aotir-contract.md)、[07.02](07-aot-backends/02-c-llvm-lowering.md) |
| 泛型实例化/dictionary sharing、LTO/ThinLTO/PGO、可达性裁剪 | [07.03](07-aot-backends/03-generics-lto-pgo.md) |
| AOT runner/动态 native coverage ≥90% 目标、fallback/deopt 可见 | [00.01](00-measurement-contracts/01-baseline-metrics.md)、[07.04](07-aot-backends/04-aot-runner-coverage.md) |
| 版本升级、ExecIR/压缩 ExecBC/maps/manifest/profile/target sections | [08.01](08-artifact-hotpatch/01-schema-relocation.md) |
| write/read/copy/AOT roundtrip、无进程地址、资源限额与畸形产物 | [08.01](08-artifact-hotpatch/01-schema-relocation.md)、[00.03](00-measurement-contracts/03-differential-harness.md) |
| 签名信任根、base identity、能力子集、禁止权限/import/ABI 升级 | [08.02](08-artifact-hotpatch/02-capability-validation.md) |
| atomic generation、旧 frame/new call 共存、stale link error、回收 | [08.03](08-artifact-hotpatch/03-generation-publication.md) |
| rollback/幂等/损坏/跨平台 patch、iOS/WASM 受限 profile | [08.04](08-artifact-hotpatch/04-rollback-restricted.md) |
| readonly/borrow/receiver/task effects、immutable/persistent/protocol iteration | [09.01](09-language-simd/01-inferred-protocols.md)、[05.03](05-data-layout/03-maps-strings.md)、[06.03](06-gc-domain/03-domain-sharing.md) |
| strict numeric/确定性、项目包 fast-math、portable vector/scalar fallback | [09.02](09-language-simd/02-numeric-vector-ir.md) |
| batch/vector/matrix、自动 SIMD、alias/tail/masked access | [09.03](09-language-simd/03-batch-vectorization.md) |
| 可选 LLVM ORC/JITLink、x86-64/AArch64、GC/unwind/debug/code lifetime | [10.01](10-jit-platforms/01-backend-service.md)、[10.02](10-jit-platforms/02-host-baseline-jit.md) |
| WSL GCC/Clang、MSVC、Android/iOS/WASM smoke、sanitizer/Valgrind/Helgrind | [00.03](00-measurement-contracts/03-differential-harness.md)、[10.03](10-jit-platforms/03-platform-matrix.md)、[11.03](11-tooling-acceptance/03-release-acceptance.md) |
| 所有公共接口族及项目 profiles、增量编译/缓存/脚本治理 | [00.02](00-measurement-contracts/02-contract-freeze.md)、[11.01](11-tooling-acceptance/01-build-profiles.md)、[10.01](10-jit-platforms/01-backend-service.md) |
| zr explain optimize/JSON/LSP、boxing/bounds/cache/vector/barrier/inlining/AOT/deopt/layout 原因 | [11.02](11-tooling-acceptance/02-optimization-remarks.md) |
| 代表 workload、全范围回归、docs/wiki/API/schema/acceptance、旧路径退出 | [00.01](00-measurement-contracts/01-baseline-metrics.md)、[07.04](07-aot-backends/04-aot-runner-coverage.md)、[11.03](11-tooling-acceptance/03-release-acceptance.md) |

## 本地参考与刻意差异

下表引用的 `../../../lua/...` 路径指向仓库根下的 `lua/` 本地参考目录（被 `.gitignore` 忽略、不随仓库分发）；在没有该目录的克隆中这些链接不可用，仅作实现期对照。

| 参考入口 | 可借鉴内容 | ZR 的约束/差异 |
| --- | --- | --- |
| [Lua lvm.c](../../../lua/src/lvm.c) 的 OP_SELF | receiver 与方法定位/调用紧邻，减少重复操作 | Lua 此路径仍按字符串取成员；ZR 静态站点采用 token/MemberId，不能照搬名字 fallback。 |
| [CPython bytecodes.c](../../../lua/cpython/Python/bytecodes.c) | specialization、guard、cache 失效及宏式指令组合 | ZR guard 受明确静态 contract 约束，stale link error 与动态推测 miss 区分。 |
| [HybridCLR Transform.h](../../../lua/hybridclr/libil2cpp/hybridclr/transform/Transform.h)、[Instruction.h](../../../lua/hybridclr/libil2cpp/hybridclr/interpreter/Instruction.h) | 正交输入转换成解释器专用复杂 IR/指令；生成 opcode 元数据 | 复用两级表示思路，不照搬其指令长度；ZR 本阶段固定宽度，artifact 无进程指针。 |
| [Rust sroa.rs](../../../lua/rust/compiler/rustc_mir_transform/src/sroa.rs) | 对地址可观察、整体逃逸等情况排除 aggregate 展开 | ZR 还需维护 GC roots、inline writeback、deopt 与 native 边界。 |
| [Mono minor copy](../../../lua/mono/mono/sgen/sgen-minor-copy-object.h)、[AOT compiler](../../../lua/mono/mono/mini/aot-compiler.c) | roots/pin/复制协议与 AOT 工程结构参考 | 不据参考存在就宣称本仓库实现完备；GC 跨帧和 ABI 需 ZR 自己的证明/测试。 |

以下保留总体方案、M0–M7 门禁与完整测试需求，作为上述子计划的需求依据。

## Summary

核心结论：不要继续把主要精力放在零散的 C 内联或单条 opcode 微优化上。当前仓库的热点已经证明，真正限制性能的是：

- `SZrTypeValue`、frame、return/call 边界上的重复复制；
- 对象/数组/字符串布局和 GC 屏障；
- 动态调用、属性访问、容器访问；
- AOT 覆盖率不足；
- 解释器与 AOT 语义重复实现；
- 缺乏统一的跨函数数据流和逃逸分析。

采用以下总体路线：

```text
ZR 源码
  -> Canonical Type / Place / Ownership / Effect / CFG
  -> Typed SSA ExecIR
  -> 自动优化与专门化
      ├─> 紧凑 ExecBC + 现有复杂组合指令（解释器）
      ├─> AOTIR -> C / LLVM（发布主路径）
      └─> Host Baseline JIT（可选，x86-64/AArch64）
```

已锁定的设计决策：

| 项目        | 决策                                                         |
| ----------- | ------------------------------------------------------------ |
| 发布主路径  | AOT-first，但解释器、AOT、未来 JIT 共享同一 ExecIR           |
| 解释器      | ExecIR 可直接解释用于调试/语义 oracle；生产解释器使用 ExecIR 生成的紧凑 ExecBC 和组合指令 |
| 优化方式    | 只依赖现有类型、所有权、布局、效果、profile 自动推断；不要求业务代码增加性能标注 |
| IR 内存模型 | 值 SSA + 显式 memory/effect token                            |
| 并行模型    | 一个 domain 可有多个 worker 并共享域堆；Send/Sync 自动证明；隔离 domain 只能复制/结构化转移 |
| 数值语义    | 默认严格 IEEE/整数/异常顺序；项目级配置可许可 fast-math，并生成明确诊断 |
| 数据布局    | 在封闭、无反射/FFI/地址逃逸的范围自动 unbox、标量替换、AoS/SoA；公开 ABI 保持稳定 |
| 热更新      | 只能更新已声明 AOT capability 范围内的 CoreIR/ExecBC；不得新增权限、native import、公开 layout 或 ABI |
| artifact    | 首个持久 ExecIR 子切片使用 schema v6 / AOT ABI 17；旧 ZRAF 明确拒绝并要求重编译，完整 08.01 门禁仍待完成 |
| JIT         | 只实现最小 host baseline JIT；不支持 Android/iOS/WASM 的机器码 JIT |
| 仓库流程    | 遵守 `zr_vm` main-only 政策，不创建 worktree/分支，使用窄提交和独立构建目录隔离 |

## 分层优化方向

### 1. 指令执行层

优先级最高的是降低每条指令的固定成本，而不是继续扩张通用 dispatch 函数。

- 保留 GNU/Clang computed-goto，MSVC 使用优化后的 switch；统一生成 opcode dispatch table。
- 将高频相邻模式自动融合为 superinstruction，例如：
  - `load + typed arithmetic`
  - `compare + branch`
  - `index + load/store`
  - `member lookup + call`
  - `increment + loop branch`
  - `call + return`
- 组合指令由 ExecIR pattern lowering 自动产生，不按具体类型名或成员名手写分支。
- 在 dispatch 顶部缓存：
  - `pc`
  - frame base
  - stack top
  - current domain
  - profile pointer
  - generation
  - 当前 frame layout/shape 信息。
- 只有在 call、GC、异常、debug、suspend、native 边界同步回 VM 状态。
- 将异常、反射、动态 meta、错误构造和慢速 ownership 路径移到冷代码。
- 使用静态/动态分支频率生成 hot/cold layout，避免热路径被错误处理代码污染 I-cache。
- 保持当前固定宽度 `SZrInstruction`；首阶段通过 side table、binding table 和 ExecBC 投影实现，不修改整条变长指令格式。
- 为每个专门化版本设置代码尺寸、命中率和失效次数上限，避免 PIC/superinstruction 爆炸。
- 任何生成的 fast path 必须有统一的 generation、shape、layout、signature guard；失败进入结构化 deopt/通用路径，绝不回退到静态成员字符串查找。

当前应继续复用：

- [zr_instruction_conf.h](/E:/Git/zr_vm/zr_vm_common/include/zr_vm_common/zr_instruction_conf.h)
- [execution_dispatch.c](/E:/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c)
- [call_binding.h](/E:/Git/zr_vm/zr_vm_core/include/zr_vm_core/call_binding.h)

这些文件已经很大，新增职责前必须按 dispatch、call、member、numeric、control、GC/safepoint 等责任拆分，不能继续堆积到单个巨型文件。

### 2. VM 核心层

#### Frame、栈和值表示

- 继续推进 packed direct frame：
  - 固定 stride；
  - 精确 storage slot count；
  - 参数 prefix；
  - direct scalar mirror；
  - return buffer；
  - tail-call frame reuse。
- 对已证明为纯标量的 `int/uint/float/bool` 使用未装箱 payload。
- 对 inline struct 使用 layout-aware byte span，不把它们退化为普通 heap object。
- 只在 ownership、GC、异常、native、debug 或动态类型边界执行完整 `SZrTypeValue` copy。
- 通过 SSA 的 live-range 和 escape facts 自动完成：
  - copy elision；
  - scalar replacement of aggregate；
  - move elision；
  - release batching；
  - return-value forwarding。
- 所有优化必须保留 ownership drop、writeback、异常和 GC root 语义。

#### 对象、成员和容器

- 继续使用 shape/type-version/MemberId/PIC，静态调用站点使用已有 `CallBinding`。
- 对对象：
  - 构造阶段尽早固定 shape；
  - 使用 descriptor index/layout offset；
  - 动态对象才进入 dictionary/prototype 慢路径。
- 对数组：
  - dense numeric array；
  - typed contiguous storage；
  - 小数组内联；
  - capacity 复用；
  - bounds proof 后消除检查。
- 对 map：
  - 缓存 hash；
  - 采用连续 entry 或紧凑 bucket；
  - 对稳定 key 类型生成专门化 lookup；
  - 不在通用路径添加具体类型名特判。
- 对字符串：
  - intern/hash cache；
  - 小字符串优化；
  - builder/rope；
  - 避免循环中的临时字符串和重复 equality。
- 对批量数据：
  - 提供 contiguous view、slice、batch API；
  - 让优化器识别协议和 layout capability，而不是识别具体类名。

#### GC、分配和 frame-safe

- 新生代采用 region + evacuation minor GC。
- 老生代采用并发 mark、分段 sweep、选择性 compact。
- 默认对象可移动；native-visible、pinned、large object 使用 non-moving 区。
- 每个 worker 使用 thread-local allocation buffer/region bump allocation。
- old-to-young 引用使用 card table/remembered set。
- 从 ExecIR/AOTIR 生成精确 GC root map，禁止依赖全堆扫描作为正常路径。
- 将 GC 变成可预算的状态机：
  - `minor mark`
  - `minor evacuate`
  - `concurrent major mark`
  - `remark`
  - `sweep`
  - `compact`
- 每帧按微秒、对象数、字节数或工作单元执行 slice；预算不足时保存状态到下一帧。
- 编译、AOT 生成和 JIT baseline 编译全部进入后台 worker；加载阶段可以主动预热。
- 锁等待必须可转换为协程/Task 等待，不能在游戏帧线程上阻塞 OS 线程。
- 同一 domain 的共享堆只允许经过自动 Send/Sync 证明的值并发访问：
  - immutable/value/Send+Sync 值可直接共享；
  - mutable shared value 必须通过现有 mutex/atomic/ownership contract；
  - borrowed、stack alias、thread-affine native handle 不得跨 worker。
- 不同 domain 之间只允许 `Transfer`、structured clone 或 materialized shared handle，禁止裸 GC 指针跨域。

#### Native/FFI

- 静态 native binding 继续使用 token、signature、layout hash 和 callback ID。
- 已知 native callback 直接进入 cached lane，避免名称解析和重复 marshalling。
- inline struct、contiguous view、typed array 在 ABI 匹配时直接传递；
- 不匹配时走显式 copy/marshal；
- native pin/unpin、GC root、异常策略、thread attach 和 callback lifetime 必须由统一 ABI 描述。
- AOT/解释器共用同一个 native contract，不能由两个后端各自猜测。

### 3. 指令优化与编译器层

现有 [semantic_ir.h](/E:/Git/zr_vm/zr_vm_parser/include/zr_vm_parser/semantic_ir.h) 已有 value/place/loan/escape/property/call 等事实，但需要建立真正的统一执行 IR。

#### Core ExecIR

新增 backend-neutral `ExecIR`，至少包含：

- `SZrExecIrModule`
- `SZrExecIrFunction`
- `SZrExecIrBlock`
- `SZrExecIrInstruction`
- `SZrExecIrValue`
- `SZrExecIrMemoryToken`
- `SZrExecIrEffectToken`
- `SZrExecIrFrameLayout`
- `SZrExecIrGcMap`
- `SZrExecIrDeoptState`
- `SZrExecIrSourceMap`

每条指令必须记录：

- opcode；
- 输入/输出 ValueId；
- TypeId、layout id/hash；
- ownership/nullability；
- memory/effect token；
- 是否可能分配、抛异常、触发 GC 或 suspend；
- source range；
- deopt id；
- CallBinding 或 dispatch slot（如适用）。

副作用至少拆分为：

- stack/frame；
- managed heap；
- module/global；
- native/FFI；
- GC；
- ownership；
- scheduler/task；
- I/O。

只有 token 独立时才允许重排；`store`、`barrier`、`drop`、`throw`、`await`、native call 不能被错误移动。

#### 自动优化 pass 顺序

1. SemIR 到 ExecIR 的 canonical lowering。
2. CFG 清理、dominators、phi、不可达块删除。
3. 常量传播、SCCP、copy propagation、DCE。
4. local/global CSE/GVN，受 memory/effect token 约束。
5. nullability、type、shape、layout 和 bounds analysis。
6. ownership/escape/lifetime analysis。
7. stack/region allocation 与 allocation sinking。
8. 静态调用绑定、devirtualization、interface slot resolution。
9. guarded inlining、tail-call、return forwarding。
10. loop invariant code motion、induction variable、strength reduction。
11. aggregate scalar replacement、unbox、字段访问直接化。
12. typed array/vector/batch lowering。
13. superinstruction 与 ExecBC lowering。
14. AOTIR lowering。
15. 仅在 host JIT profile 下生成 baseline machine code。

所有 pass 都必须在输入和输出后运行 IR verifier；无法证明时保守保留原操作。

#### Profile-guided specialization

profile 记录：

- call target；
- receiver TypeId/shape；
- branch bias；
- numeric/value type；
- allocation site；
- boxing/unboxing；
- cache hit/miss；
- deopt reason；
- GC pause/allocation；
- worker contention。

profile 只有在 module identity、IR hash、signature hash、layout hash 和 compiler ABI 全部匹配时才能导入，否则忽略。

#### 自动优化说明工具

不增加函数级性能标注，但提供：

```text
zr explain optimize <module>
zr explain optimize --json <module>
```

并提供 LSP/IDE 查询。

`SZrOptimizationRemark` 至少包含：

- pass；
- source range；
- success/missed/blocked；
- reason code；
- before/after representation；
- boxing/allocation/cache/deopt 信息；
- profile count；
- 估计成本；
- 是否影响 AOT、ExecBC 或 JIT。

必须能够解释：

- 为什么发生装箱；
- 为什么无法消除 bounds check；
- 为什么发生缓存 miss；
- 为什么不能向量化；
- 为什么保留 write barrier；
- 为什么没有内联；
- 为什么 AOT fallback；
- 为什么产生 deopt；
- 为什么 layout 没有自动重排。

### 4. 语言性质与语言能力

不新增大量人工标注，优先复用现有语言能力：

- `struct`/inline layout；
- `readonly`/`ref readonly`；
- ownership、borrow、loan、escape；
- receiver effect；
- async/task effect；
- `Send`/`Sync`；
- canonical TypeId、TypeLayout、signature hash；
- static/dynamic/meta dispatch 区分。

自动推断规则：

- 私有函数的 purity、receiver mutation、allocation、throw、escape、Send/Sync 从 body 推导；
- 公开 ABI 只发布稳定摘要；
- 泛型按 layout/ownership/effect contract 生成实例化或 dictionary sharing；
- borrowed/ref-like 值不能跨 `await`、worker、closure 或 native lifetime；
- 自动推断失败时保留通用语义并给出 remark，不要求开发者补注解。

数据布局变换规则：

- private/local、无反射、无 FFI、无地址逃逸的范围可自动：
  - unbox；
  - scalar replacement；
  - 字段重排；
  - AoS/SoA；
  - typed contiguous storage；
  - stack/region allocation。
- public type、reflection、serialization、native ABI 和跨模块稳定接口保持逻辑 layout，并通过 bridge 适配内部布局。
- 每次布局变换生成 layout map、调试映射和优化说明。

数值规则：

- 默认严格整数溢出、IEEE 浮点、NaN、±0、异常顺序和确定性；
- 允许在 `zrp`/项目构建配置中开启 fast-math；
- fast-math 是项目/包级许可，不是每个函数都写标注；
- 只有在项目许可、目标平台和 capability manifest 都允许时，才使用 FMA、浮点重排、近似除法或无序归约；
- 默认 SIMD 只做严格等价变换；有序归约不能擅自改变。

后续语言能力应优先增加：

- immutable/persistent value container；
- contiguous view/slice；
- typed array；
- batch/vector/matrix 标准库；
- protocol-based iteration；
- domain-aware task/worker；
- deterministic numeric profile。

这些能力应通过通用 protocol、layout 和 effect contract 表达，不能在共享 runtime 中按具体类型名称分支。

### 5. AOT、JIT 和平台后端

#### AOT

现有 [backend_aot_exec_ir.h](/E:/Git/zr_vm/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h) 需要逐步收敛到共享 ExecIR/AOTIR，不再让 AOT 从 quickened bytecode 反推语义。

C/LLVM 后端共享：

- frame layout；
- typed signature；
- call binding；
- layout map；
- GC root map；
- exception/EH table；
- ownership cleanup；
- debug/observation map；
- deopt map；
- native import contract。

AOT 优化重点：

- typed scalar/control；
- direct call；
- static/member/property；
- inline struct；
- typed array；
- loop lowering；
- escape-based stack/region allocation；
- generic monomorphization；
- LTO/ThinLTO；
- PGO；
- dead metadata/function stripping。

AOT runner 必须独立计时，报告：

- compile time；
- link time；
- run time；
- native coverage；
- fallback count；
- deopt count；
- code size；
- startup；
- RSS；
- GC pause。

不能把 AOT fallback 到解释器后继续标记为 AOT 成功。

#### Baseline JIT

JIT 只作为 ABI 验证后端：

- 输入为共享 ExecIR/AOTIR；
- 首批只支持 x86-64、AArch64 主机；
- 优先复用已有 LLVM toolchain 的 ORC/JITLink 能力；
- 不在第一阶段手写完整机器码 assembler；
- 只覆盖 typed scalar、control、direct call、简单 member/array；
- GC root、unwind、debug map、deopt 和 code lifetime 先于优化；
- JIT code handle、entry pointer、executable memory 只存在运行期；
- JIT code 不进入 `.zro/.zrm`，不进入热更新包；
- JIT 不可用时自动使用 AOT 或 ExecBC，但执行模式必须明确记录。

Android、iOS、WASM 默认使用：

- AOT；
- ExecBC；
- capability-limited hot update；

不依赖运行时生成机器码。

### 6. 热更新、权限与 artifact

[artifact_schema.h](/E:/Git/zr_vm/zr_vm_core/include/zr_vm_core/artifact_schema.h) 与 [zr_aot_abi.h](/E:/Git/zr_vm/zr_vm_common/include/zr_vm_common/zr_aot_abi.h) 已为首个持久 ExecIR 子切片升级至 schema 6 / ABI 17；以下仍是完整 08.01 的交付要求。

新 artifact 保存：

- canonical ExecIR；
- 压缩 ExecBC；
- call-binding rows；
- signature/layout/module hashes；
- GC/EH/deopt/debug maps；
- optimization summary；
- capability manifest；
- optional profile hints；
- target triple/backend contract。

持久化数据只能使用：

- token；
- index；
- offset；
- hash；
- version；
- generation；
- relocation record。

绝不保存：

- VM function 裸地址；
- native callback 地址；
- AOT entry pointer；
- executable memory 地址；
- 进程内对象指针。

热更新 patch 只能：

- 替换已有函数体；
- 替换既有 CoreIR/ExecBC；
- 更新同一公开 signature/layout/call-binding contract；
- 更新 debug/profile metadata；
- 使用已有 AOT capability。

热更新不得：

- 新增 native import；
- 新增 FFI 能力；
- 新增 reflection 权限；
- 新增 scheduler/domain 权限；
- 改变公开 TypeLayout；
- 改变公开 signature；
- 引入 AOT capability manifest 未声明的 effect；
- 通过动态字符串查找绕过静态绑定。

加载流程：

1. 校验 patch 签名和信任根；
2. 校验 base artifact identity；
3. 校验 CoreIR、ExecBC、signature、layout、module、ABI hash；
4. 计算 patch capability 与 AOT capability 的交集；
5. 发现超出范围立即拒绝；
6. 在 generation 边界原子发布；
7. 活跃旧 frame 继续使用旧 generation；
8. 新调用使用新 generation；
9. 旧 binding/cache 失效时返回结构化 link error；
10. 不允许降级成名称查找。

## 公共接口与配置

建议新增以下稳定接口族：

- `ZrParser_ExecIr_Build`
- `ZrParser_ExecIr_Verify`
- `ZrParser_ExecIr_Optimize`
- `ZrParser_ExecIr_LowerExecBc`
- `ZrParser_ExecIr_LowerAot`
- `ZrParser_ExecIr_Read/Write`
- `ZrCore_ExecutionBackend_Register`
- `ZrCore_ExecutionBackend_CompileAsync`
- `ZrCore_ExecutionBackend_InvalidateGeneration`
- `ZrCore_ExecutionBackend_ResumeInterpreter`
- `ZrCore_OptimizationRemarks_Query`
- `ZrCore_HotPatch_Validate`
- `ZrCore_HotPatch_Apply`
- `ZrCore_HotPatch_Rollback`
- `ZrCore_Domain_GetContract`
- `ZrCore_Domain_ShareValue`
- `ZrCore_Domain_TransferValue`
- `ZrCore_Gc_SetBudget`
- `ZrCore_Gc_GetStats`

项目级配置至少提供：

- `dev`
- `interactive`
- `release`
- `release-lto`
- `release-pgo`
- `release-fast-math`
- `frame-safe`
- `wasm`
- `android-aot`
- `ios-aot`
- `host-jit`

## 实施里程碑

### M0：测量和契约冻结

- 建立 persistent same-process benchmark。
- 分离 cold-start、compile、load、steady-state、AOT run。
- 完成 AOT runner、native coverage、deopt/fallback 统计。
- 固定 checksum、环境指纹、CV、Callgrind/hardware counter、RSS、GC pause、frame p95/p99。
- 冻结 ExecIR、artifact、capability、backend ABI 草案。

门禁：

- 所有基准有可复现环境指纹；
- 任何 AOT fallback 都可见；
- 不再使用单次 noisy wall-clock 宣称收益；
- 微优化必须达到同口径至少 3% 改善，或显著降低 frame p99/GC pause。

### M1：统一 ExecIR/SSA 和 verifier

- 将现有 SemIR facts 降低为 typed SSA ExecIR。
- 加入 phi、memory/effect token、exception edge、cleanup、suspend/resume、GC/deopt map。
- 建立 verifier 和 malformed IR negative tests。
- 实现 ExecIR 直接解释器作为语义 oracle。
- 实现 ExecIR→ExecBC 和 ExecIR→AOTIR 的无优化投影。

门禁：

- CoreIR direct interpreter、ExecBC、AOT C、AOT LLVM 结果一致；
- 异常、ownership、GC、async、property、inline struct 全部保持语义；
- 旧 artifact 按新版本明确拒绝。

### M2：Frame、value、layout、GC、domain

- 完成 packed direct frame、typed scalar mirror、return buffer、tail reuse。
- 推进 unboxed scalar 和 inline struct layout。
- 引入 region young generation、remembered set、concurrent major、budgeted GC。
- 建立同域多 worker shared heap 和自动 Send/Sync 检查。
- 建立跨域 Transfer/structured clone。
- 完成 lock-await/coroutine 化等待。

门禁：

- frame p95/p99 在宿主预算内；
- GC slice 不产生未界定长暂停；
- TSan/ASan/LSan/Valgrind/Helgrind 无新增错误；
- 同域共享和跨域复制的 ownership/GC 生命周期正确。

### M3：解释器组合指令和自动 quickening

- 从 ExecIR pattern 自动生成 superinstruction。
- 生成静态 CallBinding、typed call、member/property/accessor/meta/virtual/interface cache。
- 加入 shape/layout/generation guard。
- 将异常和动态路径冷分离。
- 继续优化 dispatch、branch layout、operand packing 和 profile local cache。

门禁：

- 静态站点不产生最终成员名称查找；
- cache miss/deopt 原因可观测；
- `member 102/102`、native fast-path、property、ownership、GC 回归通过；
- 代表集无超过 3% 的非预期回退。

### M4：AOT 统一后端

- C/LLVM 共同消费 AOTIR。
- 完成 typed scalar/control/call/member/array/string/inline struct lowering。
- 完成 generated frame、root map、EH table、deopt map。
- 接入 LTO/ThinLTO、PGO、generic sharing、reachability stripping。
- 建立正式 AOT benchmark registry。

门禁：

- 代表 workload 的 native coverage 目标达到 90% 以上；
- 不支持的能力显式报告，不得伪装成 AOT；
- C/LLVM/ExecBC 结果和异常行为一致；
- AOT compile、link、startup、RSS、code size 单独报告。

### M5：热更新与 capability 安全边界

- 实现签名 patch、base identity、generation、capability intersection。
- 支持 CoreIR/ExecBC patch。
- 禁止新增权限、native import、公开 layout/signature。
- 实现旧 frame/new frame 并存和原子 generation 切换。
- 为 iOS/WASM 生成受限解释器 patch profile。

门禁：

- 未授权 patch 在执行前拒绝；
- artifact/patch 中无裸地址；
- stale generation 返回结构化 link error；
- rollback、重复应用、损坏 patch、跨平台 patch 均有测试。

### M6：自动优化 pass 和可解释性

- 加入常量传播、CSE、DCE、range/null/shape analysis。
- 加入 escape、allocation、ownership、devirtualization、guarded inlining。
- 加入 loop optimization、aggregate scalar replacement。
- 实现 CLI/LSP optimization remarks。
- 生成 boxing/cache/GC/deopt 解释报告。

门禁：

- 每个 pass 有 verifier 和 differential test；
- 自动变换和失败原因均可定位到源代码范围；
- 无需函数级人工标注即可获得主要专门化收益。

### M7：严格 SIMD、batch 和 baseline JIT

- 先实现 portable vector IR 和 scalar fallback。
- 严格数值语义下只做等价向量化。
- fast-math 项目许可下再启用 FMA、无序归约等变换。
- 实现 x86-64/AArch64 host baseline JIT。
- JIT 与 AOT/ExecBC 共用 call frame、GC map、deopt map、capability contract。

门禁：

- JIT 只在主机目标启用；
- JIT 不能破坏 GC、异常、debug、ownership；
- JIT 启动成本、代码缓存、内存占用和 AOT 对比独立报告；
- JIT 不能阻塞 AOT 发布路线。

## 测试计划

### Parser/type inference

覆盖：

- 静态模块链；
- 对象字段链；
- getter/setter；
- meta-method；
- virtual/interface slot；
- typed function value；
- overload ambiguity；
- signature mismatch；
- unknown member；
- missing contract；
- ownership/borrow/escape/effect；
- 自动 Send/Sync；
- layout/boxing/vectorization 诊断。

### IR/compiler

覆盖：

- phi 和 CFG；
- memory/effect token 顺序；
- `load/store/call/alloc/barrier/drop/throw/await`；
- cleanup 和 finally；
- deopt state reconstruction；
- GC root map；
- inline struct；
- generic specialization；
- inlining、CSE、DCE、LICM；
- strict numeric；
- fast-math permission；
- malformed/forged IR。

### Runtime

覆盖：

- VM direct target；
- native callback；
- AOT entry；
- getter/setter；
- meta/virtual/interface；
- CallBinding generation invalidation；
- shape/layout mismatch；
- GC moving/compacting；
- frame relocation；
- exception/cleanup；
- null/empty/overflow；
- inline struct writeback；
- ownership cleanup；
- deep recursion；
- long hot loop；
- repeated deopt/requickening；
- native re-entry；
- GC during native callback；
- worker race；
- same-domain shared heap；
- cross-domain clone/transfer；
- lock wait converted to coroutine；
- frame-safe GC budget。

### Artifact/hot update

覆盖：

- CoreIR/ExecBC write/read/copy/AOT projection；
- token/signature/layout/module hash 一致性；
- schema/ABI mismatch；
- old artifact rejection；
- truncated/corrupt/unknown mandatory section；
- no-process-address byte scan；
- patch signature failure；
- capability escalation；
- stale generation；
- rollback；
- active frame/new generation；
- iOS/WASM restricted patch；
- host JIT code never persisted。

### Performance

代表集至少包含：

- `numeric_loops`
- `dispatch_loops`
- `call_chain_polymorphic`
- `mixed_service_loop`
- `object_field_hot`
- `array_index_dense`
- `matrix_add_2d`
- `map_object_access`
- `string_build`
- `gc_fragment_baseline`
- `gc_fragment_stress`
- native member call
- accessor workload
- async/task workload
- shared-domain worker workload

统一记录：

- instruction count；
- helper count；
- cache hit/miss；
- deopt；
- native coverage；
- allocation；
- GC work；
- pause p50/p95/p99/max；
- frame p50/p95/p99；
- startup；
- RSS；
- code size；
- compile/link time；
- checksum；
- environment fingerprint。

### 平台与工具链

- WSL GCC；
- WSL Clang；
- Windows MSVC；
- x86-64 host JIT；
- AArch64 host JIT；
- Android AOT/ExecBC smoke；
- iOS AOT/受限热更新 smoke；
- WASM ExecBC/AOT smoke；
- ASan、UBSan、LSan、TSan、Valgrind、Helgrind；
- debugger/source map/exception resume；
- `docs/wiki`、模块 API、artifact schema 和 acceptance record 同步维护。

## 主要风险与明确不做的事情

- 不在共享执行路径加入具体类型名、成员名或语法字符串判断。
- 不把静态绑定失败降级为字符串查找。
- 不把 AOT fallback 隐藏在成功报告中。
- 不把裸地址写入 artifact。
- 不默认开启 fast-math。
- 不先做全共享堆无锁并发；同域共享仍受 Send/Sync、ownership 和同步 contract 约束。
- 不在 GC/deopt/root map/debug ABI 尚未稳定前实现完整 optimizing JIT。
- 不为了一个 benchmark 把大量分支塞进巨型 dispatch/helper 文件。
- 不以单次 Callgrind 或不稳定 wall-clock 样本宣称性能收益。
- 不提前修改变长 opcode 格式。
- 不让反射、FFI、公开 ABI 和热更新边界被透明布局变换破坏。

统一 SSA/ExecIR 的工程量会明显大于当前 CallBinding M1，属于长期主线重构；但它是同时获得高性能 AOT、可解释的自动优化、可靠热更新和未来 JIT 的最现实基础。现有 Lua `lvm.c/lgc.c`、CPython `bytecodes.c/specialize.c`、Mono `aot-compiler.c/sgen`、Rust ownership/marker 实现，以及仓库已有 SemIR、CallBinding、GC、AOT ABI 和 benchmark 计划，均支持这条路线。
