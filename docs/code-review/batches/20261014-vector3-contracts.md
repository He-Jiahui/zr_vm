---
related_code:
  - zr_vm_lib_math/CMakeLists.txt
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/include/zr_vm_core/memory.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/src/zr_vm_core/value/value.c
  - docs/wiki/03-modules/math-api.md
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/vector3.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector3_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-14 Vector3 全链审查与文档注释任务
  - root: Vector3 六路径 lease、冻结和 r2 验收记录
tests:
  - tests/module/test_module_system.c
  - tests/CMakeLists.txt
  - tests/cmake/compiler-project-targets.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/fixtures/projects/native_numeric_pipeline/src/lin_alg.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
doc_type: workflow-detail
---

# Vector3 契约审查批次（2026-10-14）

本批审查 Vector3 两个公开头、callback 实现和 descriptor registry。源码只增加中文 Doxygen 与局部契约注释；没有改变实现、签名、参数、预处理条件或测试。新叶台账是 Vector3 的唯一 canonical 逐单元记录；已有测试与 fixture 继续使用各自的测试台账单元，不在本表重复登记。

四个源码在当前工作树中的冻结 SHA-256 为：

| 文件 | SHA-256 |
| --- | --- |
| zr_vm_lib_math/include/zr_vm_lib_math/vector3.h | efc878aa7380d812ab63dc8f684f997e1fa970599dd4e685ca9678832729fd79 |
| zr_vm_lib_math/include/zr_vm_lib_math/vector3_registry.h | 50998578f1e8c8a9965401173f66ac9fa5bd1ff4d7c583011f7d9d463be0f3fe |
| zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c | 1f8a17f16bfb586ca1090ae1787817cb6816d0e4cce06d0fc225c38a797d8885 |
| zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3_registry.c | 1b021f7cb13246783973b661894b1d68a668786c04d975a3921a97dca06db22d |

## 单元枚举

[zr_vm_lib_math_vector3.tsv](../coverage/zr_vm_lib_math_vector3.tsv) 使用规范九列，登记 57 个唯一 (file, unit, kind) 单元：

| 文件 | 单元拆分 | 数量 |
| --- | --- | ---: |
| vector3.h | 文件契约、include guard、13 个 callback 声明 | 15 |
| vector3.c | 文件契约、13 个实现、构造字段配置与借用块、构造和 factory 发布缺陷、6 个有限数值溢出块、连续布局与 compare TODO、toString 字符串失败块 | 27 |
| vector3_registry.h | 文件契约、include guard、3 个 getter 声明 | 5 |
| vector3_registry.c | 文件契约、3 个 getter、字段/构造参数/method/meta/type/hint 六项配置 | 10 |
| **合计** |  | **57** |

决策分布为 commented 19、no-comment 2、BUG 31、TODO 5。声明和定义各自独立记录；有独立失败、不变量或跨层数据流的语句块另列。普通局部顺序随所属函数结论，不按花括号数量增加行数。

## 注册、调用与生命周期

zr_vm_lib_math/CMakeLists.txt:4 通过启用的 zr_declare_module(... ON) 将 math module 纳入构建。Vector3 的方法与 meta callback 地址来自 vector3_registry.c 的静态 kMethods、kMeta；module.c:114/126/139 分别在 module descriptor 初始化中收集 function registry、type descriptor 和 type hints。ZrVmLibMath_GetModuleDescriptor 是 module.c:168 的模块级导出 getter，ZrVmLibMath_Register 在 :173-174 提供另一条初始化入口；它们不是 Vector3 registry getter 的别名。descriptor、嵌套表、字符串字面量及 callback 地址均借用当前 provider image，卸载前必须终止这些消费者。

方法和 meta descriptor 的固定 arity 在 callback 前由 native_binding_auto_check_arity 检查（native_binding_dispatch.c:34-55,364）；descriptor 注册本身不执行此次调用检查。实际 callback 表达式位于 native_binding_dispatch_lanes.h:277，dispatcher 调用 wrapper 的入口位于 native_binding_dispatch.c:615。ReadFloat 接受有符号整数、无符号整数和浮点值并转换成 double；缺参由 arity 检查拦截，进入 helper 后的错标签抛 TypeError。ReadObject 接受 object/array，错标签同样走 VM 异常路径。helper 的字段形状失败则是 callback 的普通 false。普通 false 仅当 thread 状态仍为 FINE 时归一化为 null；异常状态走 dispatcher 清理和错误返回路径，不能把所有 false 都解释为 null。

构造器的编译路径由 compile_expression_types.c:994-1003 建立 seed instance，再于 :914-955 取得 __constructor 并发起调用。三个数值实参符合 inline fast lane 的固定 arity、标量和容量准入；该 lane 在 native_binding_dispatch_lanes.c:143-150 浅拷贝 self/参数，不显式 pin self。兼容 owner prototype 的 seed 可由 ZrMath_ResolveConstructTarget 复用，不兼容时 helper 创建目标。两种情形都不能把静态 kFields 与本次栈上 values 混作同一生命周期。

## 已证缺陷与核查范围

**构造 receiver 与 factory 结果可能成功交付不完整字段。** Construct 调用 ZrMath_ConstructFloatObject；MakeVector3 经 zr_math_make_object_with_fields 创建实例后逐字段调用 void ZrMath_WriteFloatField。下层真实调用链为 zr_vm_lib_math/src/zr_vm_lib_math/common.c:167-170 的 ZrMath_WriteFloatField → zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1369-1405 的 ZrLib_Object_SetFieldCString；后者在 :1400-1403 字段名字符串 pin 扩容失败时清理临时 pin 并直接返回。ignored-root registry 处于 capacity - 1 且 receiver/key 尚未登记时，setter 先 pin receiver 占据最后一格，再给字段名 string pin 时需要扩展 ARRAY；zr_vm_core/src/zr_vm_core/gc/gc_object.c:734-768 展示 ignored-root 容量扩展路径，其中 :746 的 RawMalloc 返回 null 会在 :748 返回 false。Construct 的合法 seed 路径和新工厂对象均可进入 setter；构造 helper 仍继续并 finish，factory helper 也仍返回非空 object；callback 只据 object 是否为空决定是否发布，所以一个字段可以保持未写状态。native_binding_metadata.c:1715-1720 的 key/prototype materialization 与 object.c:2173-2181 的 prototype field addition 不会预先把 x/y/z 字符串加入 ignored-root registry。

构造路径的具体合法入口是编译器 seed → __constructor → dispatcher fast lane；fast lane 只浅拷贝 seed，setter 的 receiver pin 仍是本次增长的第一项。MakeVector3 返回的则是刚由 ZrLib_Type_NewInstance 创建的对象，没有已有 receiver pin 约束。两条路径均要求 ignored-root 数组处于上述有效容量状态并且相应分配器请求失败；本批没有故障注入执行信用。Vector3 的 nodeMap 初始容量为 4、阈值为 3/4：HashSet Add 仅在 elementCount + 1 超过 resizeThreshold 时触发扩桶（zr_vm_core/include/zr_vm_core/hash_set.h:498-500），三字段正好到 3，不发生第四字段扩桶；因此本结论只指 ignored-root ARRAY 扩容失败，不继承 Quaternion 的第四字段 HASH_BUCKET 扩容缺陷。

**有限 double 输入的数值结果会溢出。** 这些 BUG 是按完整 callback、double 接受路径和运算式静态证明，没有声明已经运行数值复现：

- Length 与 Normalized： (1e200,0,0) 是 lexer/parser 和 ReadFloat 可接受的有限值；平方和先变为 Inf。长度因此返回 Inf，虽然数学长度 1e200 可表示；归一化随后以 Inf 作除数，将非零 x 分量变成 0。
- Dot： (1e200,1e200,0) · (1e200,-1e200,0) 的中间乘积变成 +Inf、-Inf，累加结果为 NaN，精确实数点积为 0。
- Distance：有限 (1e200,0,0) 到零向量的可表示距离在平方差处溢出为 Inf。
- Lerp：有限 x 端点 1e308 和 -1e308、factor 0.5 使 rhs-lhs 先溢出为 -Inf，结果不是数学中点 0。
- Cross：有限 (0,1e200,1e200) 与自身的 x 分量计算为 Inf-Inf，产生 NaN，而精确叉积为零向量。

Length、LengthSquared、Normalized、Dot 和 MetaCompare 把 ZrMathVector3 地址 cast 为连续 TZrFloat64 输入，再交给 zr_vm_lib_math/src/zr_vm_lib_math/common.c:48 的 ZrMath_Dot；共享 math_common.h 的 TODO 仍要求核实成员填充与跨成员指针运算的 C 对象模型前提。Normalized 的长度平方也经此 helper（vector3.c:51），MetaCompare 的两侧平方长度同样走此 helper（vector3.c:125）。Distance 使用自己的 delta[3]，不适用该布局疑点。MetaCompare 按实际分支在 dl>dr 返回 1、dl<dr 返回 -1、其余返回 0；NaN 或双方平方和均为 Inf 时会走 0。0 不能证明相等；需继续核对 math API compare 约定、真实排序/比较消费者和 Vector3 专属边界断言。ZrCore_Value_CompareDirectly 的对象比较按地址处理（value.c:275），不是该 meta callback 的数值语义证据。

**toString 的短串扩容失败可让 callback 成功交付预置 null。** MetaToString 委托 ZrMath_MakeStringResult；后者在栈 buffer 格式化并调用字符串 setter，然后无条件返回 true。仅当格式结果是未驻留短串且 string HashSet 达到 rehash 阈值时，string_create_short 的 Add 才在 zr_vm_core/include/zr_vm_core/hash_set.h:498-500 触发 HASH_BUCKET rehash；zr_vm_core/src/zr_vm_core/base/hash_set.c:170-181 经 ZrCore_Memory_Allocate 请求新桶（声明见 zr_vm_core/include/zr_vm_core/memory.h:19-22），宿主分配失败后返回 false，zr_vm_core/src/zr_vm_core/string/string.c:802 因此返回 null。ZrLib_Value_SetStringObject 遇 null 不写 result（zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1311-1316）；dispatch 进入 callback 前已把 result 初始化为 null (:362)，而成功 callback 会把 result 继续交给调用者 (:671-674)。该 BUG 不涵盖所有格式输出，也没有分配故障注入执行证据。

## 现有测试与执行证据

已有模块测试 test_native_vector3_constructor_binds_all_numeric_arguments_at_runtime 在 tests/module/test_module_system.c:2974 定义，并在 Unity runner :8121 登记。它编译并执行 init math.Vector3(seed, seed + 1, seed + 2)，随后验证 x/y/z 是浮点并分别接近 2/3/4（容差 0.0001，断言约 :3011-3023）。这证明普通三参数构造绑定，不覆盖 setter 分配失败、数值极值、dot/cross/length、布局或 compare。测试 target 是 zr_vm_module_system_test，由 tests/cmake/compiler-project-targets.cmake:213-224 配置。

手写 fixture native_numeric_pipeline/src/lin_alg.zr 构造两个普通 Vector3，执行 dot、cross 和 cross.length() (:5-14)；callbacks.zr:17 只把 vectorInfo.dot 传给脚本 callback，最终 checksum 由 callbacks.zr:31 组合。suite 在 run_projects_suite.cmake:506-508 运行该 project 的 banner、结果和 probe cases，预期 checksum 为 214。该 aggregate 不断言 Vector3 的 x/y/z 数值，也不消费 crossLength，不能给 dot、cross 或 length 数值正确性单独记执行信用。模块测试和 fixture 已在各自 canonical 台账中登记，本叶表只引用其真实入口和断言证据，没有重复新增测试单元。

Root 对冻结工作树执行的 r2 验证记录在 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/gc-vector3-final-r2-results.json；严格 comment-equivalence 证明为同目录 gc-vector3-final-r2-comment-equivalence.json（SHA-256 3189e5479a6902de00b95e6cb8c7b49171663ffd155ad05b4f60034d67da0534）。证明覆盖的 8 个源码 token stream 保持一致。Root 强制重编四个 Vector3/GC 对象后，zr_vm_module_system_test target 构建 exit 0，耗时 8.438 秒；该次构建有 1 条外部 parser 文件 C4100 warning。module test runner 执行 78 项，76 PASS、2 FAIL、exit 2；Vector3 constructor case PASS。两个失败分别是 test_module_init_summary_cache_growth_keeps_current_source_summary_stable（Expected 8 Was 0）与 test_builtin_wrapper_box_helpers_preserve_runtime_semantics（Expected Non-NULL）；r2 报告记录 78 项逐项与当时 before 结果一致。七个 parser 文件在 r2 before/after 间发生变化，报告标记 before_after_behavior_isolated=false；这不应描述成完全隔离的行为对照。

后续 r3 结果记录在同目录 gc-vector3-final-r3-results.json，严格 token 证明 SHA-256 为 d0e2d1c3cd1e02bb6645b5fc35da7a77a3e36780521dfded24d257ffa80ec69b，8 个最终源码 token stream 仍一致。r3 的 module test target 构建 exit 0，耗时 32.860 秒，无 warning；完整 78 项 runner 为 70 PASS、8 FAIL、exit 8。r2 已有的两项失败仍在，另有六项从 before 的 PASS 变成 FAIL：test_binary_roundtrip_preserves_module_init_callable_metadata（:2702，Expected TRUE Was FALSE）、test_typeof_local_callable_reflection_preserves_callable_shape_in_source_and_binary（:5912，Expected TRUE Was FALSE）、test_typeof_binary_module_reflection_omits_legacy_test_metadata（:6255，Expected Non-NULL）、test_typeof_binary_reflection_restores_advanced_oop_metadata（:7082，Expected Non-NULL）、test_reference_source_and_binary_import_forms_share_logical_identity（:7815，Expected Non-NULL）和 test_reference_binary_module_metadata_roundtrip_fixture（:7931，Expected TRUE Was FALSE）。Vector3 constructor、系统 VM nested-call、native struct registration 和 GC-safe controls 仍通过。r3 前有 9 个 parser 输入文件相对旧 before 发生变化：zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c、zr_vm_parser/src/zr_vm_parser/compiler/compiler_meta_function.c、zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.c、zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.h、zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c、zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_function_assembly.c、zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c、zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c、zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c；因此新增六项失败归因未决，r3 完整套件不能作为 isolated before/after 行为对照，也不能记为全套通过。两个报告的构建/运行均由 Root 等待至进程退出；本批没有运行 Vector3 极值、布局、比较或分配故障注入测试。

## 台账检查

命令：python scripts/code_review_inventory.py check-batch docs/code-review/coverage/zr_vm_lib_math_vector3.tsv

最终对 57 条叶记录运行，退出码为 0，输出 review batch: ... valid。该检查核对九列、允许的 decision、重复键、仓内文件路径和每个 evidence 行非空；caller 方向及语义对应已按具体调用/注册点另行逐项复核。
