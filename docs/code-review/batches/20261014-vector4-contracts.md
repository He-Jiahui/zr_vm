---
related_code:
  - zr_vm_lib_math/CMakeLists.txt
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/include/zr_vm_lib_math/vector4.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector4_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/include/zr_vm_core/memory.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
  - docs/wiki/03-modules/math-api.md
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/string/string.c
plan_sources:
  - user: 2026-10-14 Vector4 模块逐单元契约审查与注释任务
  - root: Vector4 六路径写入授权与源码冻结消息
tests:
  - tests/fixtures/projects/native_numeric_pipeline/src/lin_alg.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/probe_callbacks.zr
  - tests/module/test_module_system.c
  - tests/library/test_official_provider_convergence.c
  - tests/cmake/run_projects_suite.cmake
  - tests/CMakeLists.txt
doc_type: workflow-detail
---

# Vector4 契约审查批次

## 范围和清单

本批收口 Vector4 四源码、叶台账/批次文档，并由 Root 迁移旧 math 总表对应18行（222→204，其他行原样保留）、更新 Math API 契约。未改实现、参数、API、注册数据或测试。逐单元台账列出所有 callback 声明和定义、registry getter、字段/方法/meta/type/hint 静态表、构造字段名表、include guard 与有独立契约的语句块。

| 文件 | 单元数 |
| --- | ---: |
| zr_vm_lib_math/include/zr_vm_lib_math/vector4.h | 14 |
| zr_vm_lib_math/include/zr_vm_lib_math/vector4_registry.h | 5 |
| zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c | 25 |
| zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c | 9 |
| 合计 | 53 |

总计 40 条 commented、9 条 BUG、2 条 TODO、2 条 no-comment。四源没有本地结构体、除 include guard 外的可调用宏或私有 helper。完整调用/注册入口、定义锚点、契约、失败条件和当前行证据见 [Vector4 叶台账](../coverage/zr_vm_lib_math_vector4.tsv)。

zr_vm_lib_math/CMakeLists.txt:4 调用 zr_declare_module，CommonMacros.cmake:9 对 src 树递归收集实现，因此当前 math module 会编译本批源码；module 聚合器分别读取 GetFunctions、GetType、GetHints。GetType 返回静态 descriptor、字段、方法和 meta 表。kMethods/kMeta 中的 arity 在每次调用派发前由 native_binding_auto_check_arity 检查，随后才调用 callback。ReadFloat 将整数/浮点参数转为 double，ReadObject 接受 object/array；参数标签错误抛 VM 异常，读取字段失败才会走 callback 的普通 false 路径。dispatcher 预置 null result，仅当 VM state 仍为 FINE 时普通 false 才归入 null 结果；异常状态走清理路径。context、result、局部坐标、静态 descriptor 都按同步 callback 或 provider image 边界借用，不跨线程保存。

## 字段写入与发布

ignored-root 表与对象 nodeMap 是两种分开的分配路径。字段 setter 先 pin object，再 pin field key。若 ignored count=capacity-1 且 receiver 与 key 尚未登记，receiver pin 占用最后一个 slot；key pin 需要扩展 ARRAY。若仅该次 RawMallocWithType 失败，setter 以 void 返回并跳过字段写入。ConstructFloatObject 和 MakeVector4 的 helper 不检查 setter 的写入结果，因此对象仍可能被构造并交付。这个 BUG 只在台账所列容量与独立失败条件下成立；本批未注入故障。

空 Vector4 nodeMap 初始容量为 4，HashSet 阈值为 3/4。compiler seed 和 MakeVector4 新实例都从空字段表开始。第四字段 w 插入时触发 HASH_BUCKET 扩容。确认此缺字段 BUG 的条件是 object/key pin、字段字符串创建和 x/y/z 三次插入均成功，并且只有 w 所需的 HASH_BUCKET Memory_Allocate 失败；void setter 不上报错误，构造仍 Finish，MakeVector4 仍返回非空对象，callback 可将其发布。ignored-root ARRAY 失败独立于该 nodeMap 失败。Matrix4x4.mulVector 也经 MakeVector4 创建 Vector4 输出。以上是完整静态可达链；没有分配故障注入信用。

MetaToString 的 HASH_BUCKET 风险发生于短串驻留表。MakeStringResult 通过 Value_SetString 调用 native_binding_support.c:320-324 的 native_binding_create_string，再进入 string.c:958-974 的 CreateTryHitCache；仅当格式文本未驻留、缓存 miss 后进入 Create/short interning、且所需 HASH_BUCKET 扩容的 Memory_Allocate 失败时，短串创建才不抛 VM 异常地返回 null。SetStringObject 留下 dispatcher 预置的 null，MakeStringResult 仍返回成功，随后 dispatcher 将 null 写回。非短串或有剩余 bucket 容量时不推出该失败；本批未做故障注入。

## 数值与未决契约

Length 对有限输入 (1e200,0,0,0) 先平方，平方和变 Inf，尽管真实长度 1e200 可由 double 表示。Normalized 随后按 Inf 范数除坐标，可能将非零输入变成零向量。Dot 对 (1e200,1e200,0,0) 与 (1e200,-1e200,0,0) 先形成 +Inf/-Inf，累加为 NaN，而数学点积是 0。Distance 对 (1e200,0,0,0) 到原点先平方，结果变 Inf，尽管距离仍可表示。Lerp 对有限 x 端点 1e308 与 -1e308 且 factor=0.5，差分先溢出，结果不再是中点 0。台账逐块记录这些静态可达数值路径；没有对应的极值断言。LengthSquared 若平方结果自身超过 double 范围而返回 IEEE Inf 是范围限制，不列为 BUG。

Length、LengthSquared、Normalized、Dot 与 MetaCompare 把 Vector4 struct 地址转为 double 指针并读四项；Distance 使用局部 d[4]。math_common.h 已标注同类布局前提，因此布局保证保留为 TODO。MetaCompare 当前按 dl>dr、dl<dr 的严格比较返回 -1/0/1；NaN 或两个平方长度均为 Inf 时落到 0。具体有限不同幅值例子 (1e200,0,0,0) 与 (2e200,0,0,0) 会因两边平方均为 Inf 而返回 0。实际排序/比较消费者及其 unordered 契约尚未确认，故仍是 TODO；后续入口是确认真实消费者并补 Vector4 非有限输入断言。

## Fixture 与验证边界

native_numeric_pipeline 的 lin_alg.zr 源码构造 Vector4、调用 Matrix4x4.mulVector 并读取 moved.x/y/z；run_projects_suite.cmake:506-508 仅登记 banner、checksum 与 probe 断言。若运行，流程会经过该构造/矩阵静态路径；本批未运行 fixture suite，不能给它执行信用。callbacks 与 probe_callbacks 使用 vectorInfo.dot，但不读取 moved.x/y/z，也不把矩阵输出纳入 checksum；这些断言不验证 Vector4/Matrix4x4 数值、w、长度、归一化、compare、极值或分配失败。

Root final-r1 的实际 products build 针对 zr_vm_module_system_test 和 zr_vm_official_provider_convergence_test，build exit 0，并重建四个已登记 owned objects（包含 Vector4 callback 与 registry 对象）；build、module suite 和 provider suite 均 awaited、未超时。module suite exit 8：78 项中 70 PASS、8 FAIL，全部 78 条 status 与 latest r3 完全一致；provider suite exit 0，9/9 PASS。相关通过项包括 test_system_leaf_modules_expose_new_api_and_owned_types、test_system_native_types_register_complete_struct_fields 和 test_system_gc_safe_controls_report_runtime_stats，但没有 Vector4 数值或矩阵输出断言。
该对照的三个 foreign 输入在 before/current 间变化：zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c、zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c、zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_callable_member.c；因此只报告与 latest r3 逐项一致，不称为隔离的干净 before/after。2950 条保守 pins 在 build 与两组 suite 之间稳定。两条 C4701 来自 Parser compiler_function.c:665-666；另有 21 条其他 Parser 测试对象路径的 CMAKE_OBJECT_PATH_MAX=250 警告，本批 Vector4 源无 warning。
结果文件为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/system-vm-vector4-final-r1-results.json；strict proof 为 system-vm-vector4-final-r1-comment-equivalence.json，SHA-256 a40489f90174ca0269ab4a559f66ac768d47cfe60c0797e8563e4a426e416b33。tmp_files_before/after 为空，未创建源码副本或新测试，net_released_bytes=0。共享 suite 不是 Vector4 数值信用；8 项失败仍保留，不称全绿。本批没有运行 fixture suite、Vector4 数值探针或 allocator fault injection。

## 冻结源码身份

以下 SHA-256 对应 Root 指定冻结源码；之后只允许修改两份新文档。

| 文件 | SHA-256 |
| --- | --- |
| zr_vm_lib_math/include/zr_vm_lib_math/vector4.h | B3ECC72BBA5848DAAF65567B3EB098F55E6F3FC9B1215E5C76C9F1AB678704DE |
| zr_vm_lib_math/include/zr_vm_lib_math/vector4_registry.h | D16A9E36A80540DBA3C0157D5D33D945C9CC022FC99D1559F862265040691A23 |
| zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c | 171A835AE398A060EB63932BDCB988838076B771160A15A0C3402FA054DC1AFC |
| zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c | 0306197CF578AFA9C7A855F4FAD0950619D28839E65086B90937BAD2DC26FACA |
