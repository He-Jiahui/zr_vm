---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/tensor.h
  - zr_vm_lib_math/include/zr_vm_lib_math/tensor_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/include/zr_vm_core/memory.h
  - tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/probe_callbacks.zr
  - tests/cmake/lsp-cli-smoke-registration.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/module/test_module_system.c
  - tests/language_server/test_lsp_interface.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_lib_math/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/tensor.h
  - zr_vm_lib_math/include/zr_vm_lib_math/tensor_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
plan_sources:
  - user: 2026-09-26 全仓首方代码调用链审查与注释任务
  - user: 2026-10-05 文档级声明与类型注释及提交者要求
tests:
  - tests/cmake/lsp-cli-smoke-registration.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/module/test_module_system.c
  - tests/language_server/test_lsp_interface.c
doc_type: workflow-detail
---

# Math Tensor 原生接口契约审查

## 审查范围与冻结源码

本批只覆盖 Tensor 公开回调头、registry 头、回调实现及 descriptor 实现。当前冻结源码 SHA256：

- tensor.h：95FF63EB9B5555728960A5E0465E779F9202B65602D6C00950CE53C3D23085E7
- tensor_registry.h：0E5A38728D13D42E2D0F8781CCAAA3ED91B25DA1CE80A2C17C32A61F0649BC14
- tensor.c：4413AFC0BEF327187FCAE8BE2B4C0ED1F8B8DC764AA1CFAF832F4A61EF62911F
- tensor_registry.c：5F6101C807F09864B4BC2275EE6F51A81427C8511374E0EFAB3F85544376E29F

本批逐项核对四文件的文件说明、guard、15 个 callback 声明/定义、两个私有 helper、三个 getter、3 个字段描述、13 个方法描述、2 个元方法描述、类型描述与 constructor signature、单条 type hint，以及独立失败块。叶台账共有 70 个唯一审查单元，粒度和每条的调用/测试边界见 docs/code-review/coverage/zr_vm_lib_math_tensor.tsv。

## 调用入口与数据生命周期

zr_vm_lib_math/CMakeLists.txt:4 通过 zr_declare_module 建立 Math 模块目标；src/zr_vm_lib_math/module.c:120,132,145 分别聚合 Tensor 的模块函数、类型和 hints getter。Tensor 没有模块级函数，实例方法和 constructor/toString 通过 GetType 返回的静态 descriptor 表绑定；native registry / metadata 读取 fields、methods、metaMethods，native_binding_dispatch.c:615 间接调用注册的 callback。registry 表只在 provider 映像驻留期间有效；参数 context 与 callback 结果槽只在本次同步调用期间有效，结果对象由 VM 管理。

构造入口读两个数组，复制外层容器后交给共享 TensorPopulate 发布 shape、data、rank、size。输入数组容器与新容器不同，但数组内对象值不递归复制。公开 shape/rank/size descriptor 与隐藏的 __data 存储字段分开；后续 TensorGetStorage 重新读取字段，shape/data 的元素一致性和 numeric data 校验属于公共 helper 契约，不在本批重复命名同一缺陷。

## 已证实的模块缺陷

**数组复制忽略 pin 失败。** tensor.c:7–12 的 copy helper 忽略 ZrLib_Array_PushValue 的 false 并照常返回副本。可达链为：ZrLib_Array_New 创建数组对象但不预 pin（native_binding_dispatch.c:1356）；PushValue 先 pin 数组，pin 失败即返回 false（同文件 :1515）；满容量时 ZrCore_GarbageCollector_IgnoreObject 请求扩容（gc.c:696），扩容用 RawMallocWithType(..., ZR_MEMORY_NATIVE_TYPE_ARRAY)，NULL 时只返回 false（gc_object.c:746，memory.h:37），不改变 VM threadStatus。单次仅该 ignored-root ARRAY 扩容分配失败、其他创建/pin/字段操作成功时，false 保持普通回调状态，helper 继续循环并返回短副本；其他错误状态或数组对象创建失败不属于这条静默路径。copy helper 的全部直接调用位置为 tensor.c:24、tensor.c:25、tensor.c:34、tensor.c:45、tensor.c:121、tensor.c:130、tensor.c:135。

合法例子是构造 shape [1,2]、data 两个数值：复制 shape 的首项 1 因该次数组 pin 失败被跳过，后项 2 在一次性故障结束后追加成功；TensorPopulate 只按 shape 乘积与 data 长度比对，随后将 rank 记录为 1，因此构造仍可成功交付 shape [2] 的 Tensor。对 ToArray，同一 helper 也可能直接交付短 data 副本。此链使用 GC ignored-root 的 ARRAY 原生分配，不是对象字段 HASH_BUCKET 扩桶；未运行 fault injection。

**Transpose2D 与 Matmul 输出 shape 可能丢维。** 两个 callback 都新建 shape/data 数组并忽略 shape PushValue 返回值（tensor.c:96、tensor.c:109）。仅当输出首维为 1、该次 shape 数组 pin 触发的 ignored-root ARRAY 扩容分配单独返回 NULL，后续第二维和 data 追加均成功时，shape 会少首维而总元素积保持不变；TensorMake 的长度校验不能区分 rank=2 与压缩后的 rank=1，故回调可能成功交付形状错误的结果。Transpose2D 的输出首维等于输入原 cols，因此触发条件是 cols=1；Matmul 的输出首维等于左矩阵行数，因此触发条件是左行数=1。若失败来自带 VM 错误状态的其他写入路径，dispatcher 会保留错误并清理，不能套用此普通 false/继续路径。此处没有故障注入。

**构造签名 metadata 与实际输入不符。** tensor_registry.c 的 type descriptor 声明 Tensor(shape: array, fillValue: float)，而 kMeta.constructor 把第二个参数交给 ReadArray，且 native_numeric_pipeline 的合法调用传 data 数组。metadata exporter 将 descriptor 的 constructorSignature 写入 ModuleInfo；kMeta 参数类型数组为空。本批只确认导出的签名文本错误，不声称运行时 dispatcher 会因该文本拒绝调用。

数据元素标签问题已在 zr_vm_lib_math/src/zr_vm_lib_math/common.c:424–451 的 ZrMath_TensorPopulate.numeric-data-block 记录：Populate 只检查 data 长度；Sum/Mean 跳过 ArrayReadFloat 失败项，Transpose2D/Matmul/二元运算/MulScalar 的局部零初值保留并继续。各受影响 Tensor 单元仍按各自入口标记 BUG；仅在批次统计根因时引用共享 numeric-data 根因，不重复计数。Fill/Set 的底层写入状态仍分别保留 ZrMath_ArraySetValue.status-block 的 TODO。

## 测试边界

仓内唯一含原生 Tensor 方法调用的人工项目 fixture 是 tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr:5–12：构造两个 Tensor，调用 matmul、add、mulScalar、sum、mean、toArray。其 arr 字段在 callbacks.zr:31 和 probe_callbacks.zr:28 的 checksum 中未读取；只有 sum 和 mean 参与聚合 checksum，且 CMake 的 native_numeric_pipeline_result 只核验组合输出 214 及报告字符串，未单独断言各回调数值。banner 与 probe 另检查输出标记/执行进度，不构成故障或数值专项断言。项目 suite 注册为 CTest projects（tests/cmake/lsp-cli-smoke-registration.cmake:268–278；三个 pipeline case 在 tests/cmake/run_projects_suite.cmake:506–508）；runner 会复制 fixtures、重写输出路径并删除/重建 generated 副本中的报告/字节码，测试不能被当作只读源码探针。

其余匹配项不执行 native callback：tests/parser/test_parser.c:741–756 仅解析 new math.Tensor；tests/parser/test_type_inference.c:4277–4303 检查推断出的 Tensor 类型名；tests/module/test_module_system.c:2175–2238 与 :2266–2270 两处同名 tensor_pipeline 都是恒定返回 11 的 source stub；tests/language_server/test_lsp_interface.c:1124–1162 的源码字符串只供 parse/定位测试，注释明确说明本测试不直接执行。当前没有本单元的专用数值、非数值数据、rank 缩短或分配故障断言。本 producer 未运行 project suite、parser/module/LSP tests、数值探针或分配故障注入；Root 最终运行的现有 module suite 另见文末，它不构成 Tensor callback 数值执行证据。

## 修改与验证范围

源码修改只增加/修订中文注释和 Doxygen；没有改函数签名、表达式、分支、参数、返回值或运行时 API。前一版 proof `math-tensor-final-r3-comment-equivalence.json`（SHA256 `4966c683a1e91019e43b9f2d2a3ab58bb0d67b8f590e3f2b8741bcc4b20af8a7`）覆盖旧头文件（54FF…/ABCA…），不作为当前源码证据。当前冻结四源的 r4 strict proof 已由 Root 确认通过：`math-tensor-final-r4-comment-equivalence.json`（SHA256 `1137b5f8063036d46cbfcffa17282497e2413503feb5c1b85068a258cba73d05`），4 文件 3204 tokens、8 个 lexer 全部通过；Root 已完成最终 native 验证，实际结果见文末；本 producer 未运行构建、测试或故障注入。本节测试边界用于叶台账逐单元引用，不移动本节。


## 逐单元结论与证据补充

coverage leaf 有 70 个唯一审查键，decision 统计为 35 BUG、2 TODO、21 no-comment、12 commented。这个数字统计的是单元结论；共享根因按既有 common canonical 项归并，不因此把受影响 callback 单元改成无缺陷。故障类按已证路径区分为：数值/形状转换与运算边界、非数值 data 的错误计算、ARRAY ignored-root 扩容软失败造成的短副本/短输出 shape、短字符串缓存软失败、以及 constructorSignature 导出失真；尚未证实的细节继续留在对应 TODO。

Root 的完整调用扫描记录 `math-tensor-full-caller-evidence-audit-r1.json`（SHA256 `D9794550C628EA77CBC330495FD99E170BB7D397E5C3A1109E23CA9E0BDBE22B`）：20 个函数、18 个声明、2 个聚合 getter 宏实参、20 个定义、12 个直接调用和 15 个 descriptor callback 指针候选。该扫描用于补齐可检索入口，不表示这些入口都由运行测试执行。

越界浮点输入并非只能由损坏的宿主对象构造：parser lexer 在 `zr_vm_parser/src/zr_vm_parser/syntax/lexer.c:163–188` 接受指数形式并用 `strtod` 产生 FLOAT；注册的 Construct/Reshape/Get/Set 会把合法数组实参交给 `ArrayReadInt`，其 `IntFromValue` 路径没有对 FLOAT 到 int64 转换做范围检查（`zr_vm_lib_math/src/zr_vm_lib_math/common.c:355–378`）。例如有限的 `1e300` 可触发该转换边界，随后结果可能未定义；这是静态合法路径分析，未运行此输入或 sanitizer。维度乘积的 signed overflow 另由 common `positive shape product` 单元记录。Get/Set 的小数索引向零截断语义仍是独立 TODO，不并入该 BUG。

`GetType` 的 constructorSignature 缺陷限于导出 metadata：`zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c:3420,3497` 读取/保存该文本；`zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c:1575–1599` 只检查 prototype 是否为无参构造形式，不解析 signature 中参数名或类型。因此不能据此声称第二参数被静态推断为 float，也不能声称 dispatcher 以该文本作为运行时类型门禁。header 的 References 使用 parser 源文件 basename，leaf 与本段给出完整仓库路径。

模块 stub 的第二处测试源码位于 `tests/module/test_module_system.c:2266–2270`，和 :2175–2238 一样只返回常量 11，不调用 Tensor native callback。除此之外本批没有新增测试或 fixture；本批最终 r4 proof 与 native 结果见文末实际验证记录。


## 局部 VM 数组跨完整回收的待核窗口

另有一项有界生命周期 TODO，已附到 leaf 中 Construct、Clone、Reshape、Transpose2D、Matmul、Add、Sub、MulScalar 的声明/定义记录，以及 `zr_math_tensor_copy_array`、`zr_math_tensor_binary` 两个 helper 记录；没有因此增加或重分类 70 个审查键，也不把该疑点升级为 BUG。`ToArray` 不在此窗口内：copy helper 返回后 callback 直接将数组写入 result，没有后续 VM 分配。

风险入口是 `ZrLib_Array_New`（`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1356–1364`）→`ZrCore_Object_NewCustomized`（`zr_vm_core/src/zr_vm_core/object/object.c:1482–1506`）→`ZrCore_RawObject_New`（`zr_vm_core/src/zr_vm_core/gc/gc_object.c:1069–1105`）→`Memory_GcMalloc`（`zr_vm_core/src/zr_vm_core/base/memory.c:39–47`）。仅当第一次 RawMalloc 失败、global collector 已初始化且非 immediate-GC 时，才会经 `base/memory.c:13–18` 执行 `GcFull` 后重试。GC roots 扫描活动 VM stack slots 与 call-info frame slots（`zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1310,1358`），并经 `:1317` 标记已登记的 AOT root frames；它不扫描任意 Tensor C local。`Array_PushValue`（`native_binding_dispatch.c:1503–1534`）只在单次调用中 pin/unpin 数组和值，不提供 callback 后续分配期间的持久 root；不能据此说每次 append 都会触发 GC。

具体待核窗口是：Construct `tensor.c:24–27` 中 shapeCopy 跨 dataCopy 的 Array_New；Populate 先插入 shape 字段（`common.c:435`），`ZrLib_Object_SetFieldCString` 会 pin tensor 与当前字段值，并在建键串/写对象字段期间分配（`native_binding_dispatch.c:1385,1388,1393,1408`），因此 dataCopy 跨 shape 字段插入窗口，直到随后写入 `__data`（`common.c:436`）才发布。Clone `:31–37` 与 Reshape `:40–48` 的两个 copy helper 实参求值顺序未规定，一个新数组可能跨另一个复制及 `TensorMake` 中 `Type_NewInstance` 的分配；Transpose2D `:90–99`、Matmul `:101–112` 的 shape 跨第二个 Array_New，之后 shape/data 又跨 TensorMake；Add/Sub 进入 `zr_math_tensor_binary` `:114–122`，以及 MulScalar `:125–131` 的局部 data 跨复制 shape 和 TensorMake。共同后续分配点是 `common.c:454–462` 的 TensorMake/Type_NewInstance，再由 Populate `common.c:424–451` 写字段。下一步应在支持的 GC/rooting 协议下逐分配点核对局部数组是否被显式 root 并保持到发布；本批未执行 GC 压力或故障注入，不能据此断言数组已被回收、移动或造成可观察错误。


## 最终验证记录

Root 对冻结四源执行 r4 strict comment-equivalence proof，3204 tokens 与 8 个 lexer 均通过，proof 文件 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/math-tensor-final-r4-comment-equivalence.json` SHA256 `1137b5f8063036d46cbfcffa17282497e2413503feb5c1b85068a258cba73d05`。最终 native 结果文件 `tensor-stream-final-native-r1-results.json`（SHA256 `b2a77e8e1f11d5b5a676b88dfcce8eff919d8af5730f9ed84b54b99d542b6764`）记录 build exit 0；module suite 70/78（8 个失败），provider 9/9，通过；Fs suite 7/8，唯一 C1035 `stream_modes` 失败。比较文件 `tensor-stream-final-native-r1-comparison.json` SHA256 `9a7da0dc226e726aba5cbf20d27f03e1ecea2c713b8675872fac1dadd8c52917` 记录 case lines 与稳定参考结果相同；但原修改前全量 build 被外部 Parser 缺宏阻断，故不能声称有隔离的源码前后归因。3216 输入 pins 与 28 个冻结 source pins 在该轮前后稳定。无 Tensor 数值/非法输入、OOM/GC 故障注入信用；这些 suites 未新增覆盖，不表示 8 个既有 module 失败已解决。


本节 GC 生命周期路径涉及 `zr_vm_core/src/zr_vm_core/base/memory.c`、`zr_vm_core/src/zr_vm_core/gc/gc_mark.c`、`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c`；越界浮点文本入口另见 `zr_vm_parser/src/zr_vm_parser/syntax/lexer.c`。这些只读实现用于解释待核窗口，不在 Tensor 本批源码写入范围内。


## Root 规范台账迁移与提交验收

旧 zr_vm_lib_math.tsv 的23个 Tensor 键已逐项映射至70单元专用台账：20个函数/helper、两条旧公开文件接口和组合 descriptor 状态保留定义对应关系；原10条BUG和copy helper的TODO没有降级，旧表其余7个配置/构建单元原字节保留。声明与定义分别记录；35个BUG结论是受影响单元数，不等于独立缺陷数。共享BUG/TODO引用common台账，不重复登记根因。

完整调用报告 math-tensor-full-caller-evidence-audit-r1.json（D9794550C628EA77CBC330495FD99E170BB7D397E5C3A1109E23CA9E0BDBE22B）与最终r4源码由 math-tensor-final-caller-source-bridge-r1.json（4A2015B2613335CF7C16978044049E1A2A957DC358BDF8664572454788DCBE38）连接：四文件3204个程序token/词法flags及原token到最终行映射与r3一致，两个C文件原始字节不变；不把旧头文件原始字节说成当前版本。getter聚合宏实参在module.c:120/145，实际调用在宏体:92/98，GetType直接调用:132。Math CMake:4经CommonMacros.cmake:6–11扫描首方C源建立目标；descriptor回调由native lane/dispatch间接到达，注册本身不计执行信用。

Root补齐related_code的GC、Parser和构建依赖，并将leaf内batch行号按新增frontmatter8行映射；测试范围段原:76变为:84，仍描述静态fixture及未执行project suite。Wiki2同步浅拷贝、实际shape索引长度检查、写入状态、metadata和root待核边界。提交范围为4源码、专用leaf/batch、旧Math表、Wiki2及inventory的4源码行，共10路径；精确stage、三次cached check和独立签收记录见任务报告目录的 math-tensor-index-r1-receipt.json、math-tensor-root-staged-r1-audit.json、math-tensor-independent-staged-review.json。

本批自有lexer/build/test子进程均已终态。实际释放0 B；复用build/cache/index和必要失败证据保留，Fs runtime当前32文件/104 B。此前自动清理审批被策略拒绝，未重试或绕过，清理状态未关闭。没有新增重复实现的测试或源码副本，没有改变API签名或程序行为。
