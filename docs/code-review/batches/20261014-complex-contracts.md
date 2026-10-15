---
related_code:
  - zr_vm_lib_math/CMakeLists.txt
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/src/zr_vm_core/value/value.c
  - zr_vm_core/src/zr_vm_core/value/meta.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/src/zr_vm_core/execution/execution_memory.c
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - tests/cmake/run_projects_suite.cmake
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/complex.h
  - zr_vm_lib_math/include/zr_vm_lib_math/complex_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/complex-source-leases.json
  - user: 2026-10-14 首方逐单元契约注释任务
tests:
  - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
  - tests/fixtures/projects/import_capture_native/src/ops.zr
  - tests/fixtures/projects/native_chain_calls/src/main.zr
  - tests/fixtures/projects/native_complex_capture_probe/src/main.zr
  - tests/fixtures/projects/native_complex_capture_probe/src/ops.zr
  - tests/cmake/run_projects_suite.cmake
doc_type: workflow-detail
---

# Complex 契约审查批次（2026-10-14）

本批覆盖 Complex 两个公开头、回调实现和静态 descriptor registry。源码变更限于中文 Doxygen 与非显然契约注释；函数签名、可执行 token、预处理条件和运行行为均未改动。Canonical 逐单元结论见新叶台账；旧 zr_vm_lib_math.tsv 的 37 条 Complex 历史记录由 Root 迁移，剩余 124 条 raw 记录的保留由独立 receipt 核验，不把逆向锚点修正重复计为新单元。

原始 4 文件 lease 记录的是修改前状态；以下为本批最终冻结的四个源码 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| zr_vm_lib_math/include/zr_vm_lib_math/complex.h | 85b0ba8b814fa4e2d4a2d1d3a35af87292a968a162b34cc881eb882c9bf2b4b4 |
| zr_vm_lib_math/include/zr_vm_lib_math/complex_registry.h | d77ec0d9136da6d0843271017e2474b971601ff9d9e2831f4e631dc7b291dd04 |
| zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c | 172802751f32e8cd3c573ecd6a4245d8874ba2b2ffcf3098b950d950514ef659 |
| zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c | 15104a28f5ef3279b3d0cbc6b05c3bbf2b02ff947429f09f25e83a2171116000 |

## 单元清点

叶台账记录 61 个唯一 `(file, unit, kind)` 单元。头文件声明与实现分别记账；registry 的静态表按表及具名条目拆开；共享 `common.c` factory 是调用链证据，不计作本批定义。

| 文件 | 单元构成 | 数量 |
| --- | --- | ---: |
| complex.h | 文件契约、include guard、11 个回调声明 | 13 |
| complex_registry.h | 文件契约、include guard、3 个 getter 声明 | 5 |
| complex.c | 文件契约、11 个回调、构造字段名表、7 个独立语义块 | 20 |
| complex_registry.c | 文件契约、3 个 getter、字段表与 2 条目、方法表与 4 条目、meta 表与 7 条目、类型 descriptor、hints 表与 1 条目 | 23 |
| **合计** |  | **61** |

表中的 no-comment 条目只覆盖可由字段名和 descriptor 值直接识别的条目；数组级注释承担静态存储生命周期、显式 arity 不含 receiver、无参 callback 仍负责读取 self，以及空模块函数表不等于没有实例方法等契约。

## 注册、派发与所有权

`module.c:117/129/142` 分别聚合函数 registry、Complex 类型 descriptor 与类型形状元数据。`GetHints` 只把静态形状交给模块聚合及 `native_binding_metadata.c:927-1008` 的 metadata 序列化；本批未找到 parser 或 LSP 直接消费 Complex hint 的证据。`GetType` 返回由当前 provider image 持有的静态 descriptor，内嵌表、字符串和 callback 地址在映像驻留期间借用。

命名方法的函数指针位于 `complex_registry.c:22/24/26/28`，元方法位于 `:32-38`。Binding dispatcher 先按 descriptor arity 检查显式参数，再由方法或元方法 lane 选择回调，并在 `native_binding_dispatch_lanes.h:277` 同步调用；callback 中 `context`、参数视图和 `result` 均为本次调用借用。缺参和错误值标签通过不返回的 VM error API 报告；`ReadComplexObject` 对缺字段或字段类型不合适返回普通 false。对象和字符串结果由 VM 管理，裸对象若跨本次调用保存仍须由宿主建立根。

脚本调用者包括 `native_numeric_pipeline/src/signal.zr:5-7,10` 的构造、normalized、phase 与 magnitude；该 pipeline 最终只把 `signalInfo.magnitude` 纳入 checksum（`callbacks.zr:31`）。`import_capture_native/src/ops.zr:6`、`native_chain_calls/src/main.zr:3` 和 `native_complex_capture_probe/src/main.zr:4`、`ops.zr:5` 也跨模块构造 Complex 并读 magnitude。这里的 `base + magnitude()` 是普通标量加法，不是 Complex MetaAdd。仓内这些脚本没有调用 conjugate、Complex +/−/*、unary − 或 Complex 比较的数值断言。

## 已证问题与未决契约

**大有限值的平方中间量溢出。** Magnitude 与 Normalized 使用 `sqrt(real*real + imag*imag)`；有限 `(1e200, 0)` 的平方变为无穷，Magnitude 因而返回无穷，Normalized 再以无穷为除数而把非零输入缩成零。另有一项已写明的 epsilon 策略：模长不超过 `ZR_MATH_EPSILON`（包括非零近零输入）时有意返回零复数，不把所有成功结果描述为单位模。

**复数乘法可把可表示的实部算成 NaN。** 对合法有限 `lhs = rhs = (1e200, 1e200)`，精确实部为零；当前 `a*c - b*d` 的两个乘积分别溢出成 Inf，随后 `Inf-Inf` 写入 NaN。回调还走新对象 factory，受下面的字段发布路径影响。

**比较可能把不同模长折叠成相等。** `(1e200, 0)` 与 `(2e200, 0)` 的平方模长均溢出到 Inf；两个严格比较都为 false，当前写出 0。NaN / unordered 情况应返回何种 VM tri-state 仍标 TODO，核查入口是 `meta.c:29-31` 的 -1/0/1 定义与实际比较调用方；没有把这项政策疑问升格为已证缺陷。

**新对象字段写入有窄化的 ignored-root ARRAY 软失败路径。** Conjugate、Normalized、MetaAdd/Sub/Mul/Neg 通过 `ZrMath_MakeComplex` 创建新对象，随后只以 helper 是否返回非 NULL 决定是否交付。普通新对象没有 dispatcher 预先 pin 的 receiver；在合法宿主配置下，若 ignored-root 数组已满，且定制 `FZrAllocator` 只拒绝首次 receiver pin 所需的 ARRAY 扩容、其余 setter/工厂步骤均成功，`ZrLib_Object_SetFieldCString` 忽略 pin 失败并返回，void `ZrMath_WriteFloatField` 不能向工厂报告字段未发布，callback 仍可成功交付缺字段对象。证据链为 `common.c:22-35` → `native_binding_dispatch.c:1385,1400-1403` → `gc_object.c:746-750`；allocator contract / 保存与透传入口为 `zr_common_conf.h:179-181`、`global.c:360-374`、`execution_memory.c:60-76`、`memory.h:36-43`。`test_io_allocation_failures.c:129-141,424-434` 展示可一次失败后恢复的宿主 allocator 形式，但该静态测试只筛别的分配种类，且本批未运行；不声称默认 allocator 复现或 Complex 故障注入通过。该结论与两字段 nodeMap 扩桶无关；`Construct` 复用 target 的路径仍单独 TODO，不并入此 BUG。

**短串 cache 扩桶失败会令 toString 成功而结果槽保持预置 NULL。** `MakeStringResult` 在栈缓冲格式化后调用 void string setter 并返回 true；当未驻留短串创建成功而 hash bucket 扩容分配失败，setter 不写结果槽，dispatcher 的常规路径仍交付预置 NULL。该链只证明短串 HASH_BUCKET 失败条件，不声称所有格式结果都会失败。`%g` 输出长度随值变化；本批未把字符串长度描述成固定常数。

## 测试边界（叶台账逐行引用本节）

Root 实际执行了同配置构建与 module/provider suites：构建退出码 0；module 78 例中 70 PASS、8 FAIL（exit 8），provider 9/9 PASS（exit 0），逐例结果与既有 Fs native-r2 完全相同。该结果提供编译、模块加载和 provider 聚合边界证据，不构成 Complex 数值算法覆盖。本批未运行 Complex 项目 fixture、数值回归、比较行为、GC 专项或分配故障注入。native_numeric_pipeline 的项目 runner 在 run_projects_suite.cmake:506-508 注册 banner/result/probe cases；静态阅读只证明脚本调用链和其 checksum 输入，不证明本批实际执行。callbacks.zr:31 只消费 magnitude，没有断言 normalized/phase 的数值，也没有消费复数算术、比较或字符串结果。其他三个跨模块 fixture 在 run_projects_suite.cmake:498,500,504 有 suite 注册点；它们通过 magnitude 及模块调用链，不能作为 Complex 算法专属覆盖。仓内未找到 Complex 专属极值、compare、toString soft-failure 或 allocator 故障注入断言。

## 台账验证

Root 已独立运行 scripts/code_review_inventory.py check-batch docs/code-review/coverage/zr_vm_lib_math_complex.tsv，自然退出 0。当前 61 行 leaf SHA-256 c70636db00d3b15dfe92ff5e5662a7b2eb713955baf544f7e9b42f0404a4334d；结果记录 SHA-256 7995c9c1498d3156b3f457823d0ecf031374b511ff164311fcc06c9f2b01104d。检查九列、唯一键和非空路径锚点，不验证调用方向；GetHints declaration 的 caller 只有 module.c:142，metadata.c:927 仅为聚合后的消费证据。
## 最终验证证据

- 四个源码相对 HEAD 的注释等价证明：E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/complex-final-r1-comment-equivalence.json，SHA-256 e17355f80ff1d9592b3a8949ab7e993b4f50436d903481616ca8ee9e5259cbe4。143/39/1113/406 项 token 与行首、空白属性相同，Clang lexer 自然退出；报告未记录位置敏感宏。
- Root 原生验证：E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/complex-final-native-r1-results.json，SHA-256 c689e859132e1dab97442046e39ced7b4a83cf2d1868442ec57a76c2d3ecbe67；逐例比较 complex-final-native-r1-comparison.json，SHA-256 2d654884b6bbe3466c271d66581965a348effc4738e2f99131605af14b961a50。构建退出码 0，耗时 76.031 秒；module 78 例为 70 PASS / 8 FAIL、exit 8；provider 9/9 PASS、exit 0。module/provider 逐例结果与 Fs native-r2 完全相同。
- 2975 个当前输入 pin 及 Complex 四源、Fs 三源保持稳定。构建包含初始化/未使用选项 warning；外部 SSA typed_metadata 与 quickening 源也在同轮重编译，因此本批不声称全 checkout 隔离归因。临时文件前后无差异，本批未新增或清理产物；Fs 前批 12 个文件、39 B 保持原状。
- 未执行 Complex 数值、项目 fixture、GC、allocator 或故障注入测试；上述 module/provider 结果不作为这些行为的覆盖证据。Root 已按最终 61 行逐项匹配冷读行哈希及精确 caller 修正 receipt，check-batch 自然退出 0。

补充字段可观察性证据：zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:873-891 的 NewCustomized → 设置 prototype → Object_Init 建立空实例；zr_vm_core/src/zr_vm_core/object/object.c:1476-1505 初始化空 nodeMap。native_binding_metadata.c:1214-1241 与 object.c:2296-2336 只建立 memberDescriptors，不给 prototype 安装 real/imag 的 own 值；object.c:1992-2022 的读取先找实例 own 字段，再查 prototype own 值，native helper 的 missing-field 路径在 native_binding_dispatch.c:849-855,858-877 返回普通失败。因此本批第 80 行窄化 pin 失败条件可交付缺少 real own 字段的实例，注册时也没有默认值可回退；不把缺字段说成自动补零。用户后来自行写入 prototype 值属于另一状态。宿主 allocator 的合法性、失败恢复及其静态测试边界仍以第 80 行为准。
