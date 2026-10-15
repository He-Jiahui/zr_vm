---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/tensor.h
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - CMakeLists.txt
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/conversion.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_core/include/zr_vm_core/object.h
  - tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr
  - tests/parser/test_parser.c
tests: []
doc_type: workflow-detail
---

# Math common 辅助层契约审查

## 范围与源码快照

本批覆盖 Math 公共 helper 头文件和唯一实现文件。头文件为公共回调模块提供数值快照、字段读写、构造结果、数组转换和 Tensor 存储操作；实现文件由 Math 静态库编译，并由各类型 callback 复用。这里只记录注释、调用契约和静态证明，不改变实现行为。

审查时冻结的源码：

- zr_vm_lib_math/include/zr_vm_lib_math/math_common.h：SHA256 AF305F2CB005753FC4F0E8BFE59FCD72CE826D0D95809B822BFC38D25FBD0089，931 行。
- zr_vm_lib_math/src/zr_vm_lib_math/common.c：SHA256 8022B685D7F198821C5288CB7A18F0EC04A7BCDD6A7E1468C7F3C6E5805E00F3，546 行；相对原 520 行只在文件末尾追加 private helper 文档，原 C token 行号未移动。

头文件声明 39 个 helper，另定义六种临时快照：Vector2 的 x/y、Vector3 的 x/y/z、Vector4 的 x/y/z/w、Quaternion 的 x/y/z/w、Complex 的 real/imag，以及 TensorStorage 的 shape/data/rank/size。实现文件包含相同 39 个导出函数、一个共享对象工厂、七个对象字段名表和两个矩阵字段读取表。所有函数声明/定义、九个局部字段表、SIMD 选择、布局边界及关键失败块都逐项列入本批台账。

## 调用链与状态边界

Math module 在 module.c 汇总各类型 registry，native descriptor 再将 callback 连接到 VM 派发。公共 helper 在同步 callback 中访问当前 state、receiver、数组或结果槽；局部快照不转移 VM 对象所有权，也不能保存到 callback 之后。Read/convert helper 对普通字段/标签不匹配返回 false；调用方应保留已有 VM error 状态。构造对象工厂返回 VM 管理对象或空指针；返回非空不代表所有 void 字段写入都成功。

Tensor constructor 先复制输入数组，再用 TensorPopulate 计算并发布 shape、data、rank 和 size。其现有 numeric callback 将数组元素读为浮点数；Populate 只验证 data 长度，未逐项验证标签。公开数组仍可由脚本修改，TensorGetStorage 只重读字段标签，不重建这些字段之间的一致性。

## 已证实问题与待核契约

- IntFromValue 将浮点值直接转换为 int64。已闭合的用户路径为 Tensor shape/index 数组输入有限 1e300，经 ArrayReadInt 进入 cast，构成 C 未定义行为；GetStorage 还在 common.c:415 直接转换 rank/size 两个公开字段（注册表未声明只读），该实际消费者纳入契约，但脚本可写范围与缓存一致性政策仍单列 TODO。不把所有非有限脚本构造都视为已逐入口证明；uint64 超过 INT64_MAX 的转换是实现定义。
- TensorTotalSize 对正维度做有符号乘法而未检查溢出。两个各自可表示的正维度（例如 3037000500 和 3037000500）即可在构造路径触发 signed overflow。
- TensorPopulate 违反 tensor.h:12 对元素为数值的公开预期，接受 Tensor([1], ["x"]) 这类长度匹配但含非数值项的数据；sum/mean 在读取失败时跳过该项并交付数值，其他若干运算以零初始化后忽略读取状态。另一个独立发布 BUG 是：新 Tensor 的空 map 容量4/阈值3，依次写 shape、__data、rank 后，第四个 size 插入请求 HASH_BUCKET 扩容；若仅该次桶分配返回 NULL、其余创建/pin/写入/回调步骤成功，void setter 不能传播失败，Populate 仍 true，构造或工厂可交付缺 size、随后 GetStorage 无法读取的实例。此链为完整静态证明，未故障注入。
- 构造 helper 与七个静态对象工厂都共享 void 字段 setter。新构造 receiver 或运算结果对象的 map 初始容量/阈值下，第四、七或第十三次字段插入可请求桶扩容；若该次桶分配返回 NULL 且其余创建、pin、后续操作成功，setter 不传播状态，ConstructFloatObject 仍交付 receiver，静态工厂仍返回对象。此批没有分配故障注入，不把静态可达链说成运行时复现。
- MakeStringResult 写入短串缓存时，桶扩容软失败可使预置 null 结果槽保持 null，而 callback/helper 仍报告成功。长格式结果长度随 %g 值变化；其较长缓存比较是否使用短串 inline-byte 布局仍是具体 TODO，不声称所有输出都短。
- Tensor 浮点 shape/index 当前经 IntFromValue 截去小数。此为已记录实现行为；是否应改成只接受整数仍待核，不把截断本身列为已证 BUG。TensorStorage 的公开字段可被调用方修改，后续操作是否应拒绝不一致缓存也待核。
- 根 CMakeLists.txt:17、18、19 设定 C11 并关闭扩展。按 WG14 N1570 §6.7.2.1p15 与 §6.5.6p7–8（[C11 草案](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)），结构体转 double* 对应首成员标量对象，不能把其他成员作为同一数组：Vector3 count=3 在 scalar/AVX2/SSE2/NEON 尾循环读下标2；Vector2 count=2 在 AVX2 尾循环和纯scalar读下标1；Vector4 count=4 在 SSE2/NEON 第二轮求 lhs+2/rhs+2，纯scalar也越界。各自合法 callback 调用与非法 C 指针操作已闭合，均作窄条件 BUG。Vector2 SSE2/NEON 与 Vector4 AVX2 的单次 intrinsic load 范围保留 TODO；没有当前默认构建已运行这些边界输入或覆盖各后端的断言。

## 测试与验证边界

本批未运行 Tensor、数值极值、分配故障注入或项目 fixture。仓内可检索到 parser 对 Tensor 构造语法的测试，以及 native_numeric_pipeline 的 tensor_pipeline.zr 样例；它们不执行本批的缺陷输入或断言相关数值。pipeline 的聚合证据不能代替单个 helper 行为断言。

Root 最近一次实际 checkout 原生验证为 support-common-final-native-r2：build exit0/5.047秒，module70/78且同既有8FAIL，provider9/9，Fs7/8且同既有stream_modes C1035失败；所有 case 行与各自最近基线精确一致。该次使用本批最终修订前的 Math C B44D9464/h B930317E；后续只改注释，最终严格 token/坐标证明和原生输入桥另记，不能将旧源码执行冒充最终字节执行。

实际原生运行期间3216 pins与13 frozen输入稳定，所有子进程自然退出。修改前完整构建曾因并发 Parser 缺少 ZR_INSTRUCTION_USE_RET_OPERAND_EXTRA 定义失败；当前两个相关外部源已变化并解除阻塞，不能将成功单独归因于本批注释。本批没有新增数值极值、UB、OOM、GC、并发、动态插件或 Linux 执行信用。

## 注释范围结论

当前源码只补契约注释和问题标签，没有改变转换、溢出、发布、字符串缓存或 Dot 行为。最终台账118个唯一单元：39声明、40定义、6类型、4宏、2文件、18个真正独立语句块及9个局部字段表；删除两个重复整个函数的 unused-block 伪单元，并补一个独立 Tensor 字段发布块。定义、全调用、失败状态、所有权与测试边界见 coverage/zr_vm_lib_math_common.tsv。

Root 对 40 个函数名和 6 个类型名执行全仓 rg 候选检索，不按扩展名限制候选，并排除 lua、third_party、.git、.claude 副本与 .codex；C/H 按注释和字符串字面量剔除、保留行号后区分定义/声明/调用。共 367 处实际调用、358 个逐符号唯一坐标，40 个函数各有唯一实现；未见函数指针或字符串映射引用。非 C/H 候选均为说明文档/台账；头文件有 11 处直接 include。所有函数和类型 caller 列已据此补全，不把仓内无引用推成仓外不可达。完整元数据证据为 math-common-full-caller-evidence-audit-r2.json（SHA256 26caa91f06e45cc18d4e6e54c9001f59cd2ebfe2f1b4d4db190dc5ea59497d3a）。

最终严格证明 math-common-final-r6-comment-equivalence.json（SHA256 4a8fbd2c3c96f560703aa93f12c1e574d93b1f94cf6556bc9642adc264afb4d5）确认4290个非注释token及其词法标志完全一致，所有原C程序token保留物理行号。实际native-r2到最终源码的同token/坐标桥为 math-common-final-source-native-token-bridge.json（SHA256 b8e844d4ad79d0e0ca534040501b7b618f278d35afa3260ffefa5c70b6a4c9f7）；它保留原生使用的旧源码哈希，不声称最终注释字节另跑了suite。最终118行台账格式和非空数字锚点检查exit0；语义真实性由Root和独立reviewer逐项复核。

本批反向覆盖全部30个将结构体地址传入 Dot 的公开声明/定义（Vector2/3/4 各五函数）和两个既有独立布局块，共32条既有单元，保留其原数值、factory 与排序策略结论，不新增审查数量。Distance 使用显式数组，未误列结构体转换缺陷。Vector2/3/4 六个源码文件中的旧布局 TODO 与旧 common.h 锚点由 Root 独占接手，安排在本批提交后立即完成依赖注释批次；本批对其现有源码注释不声称已改写。
