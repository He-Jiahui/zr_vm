---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c
  - zr_vm_core/src/zr_vm_core/object/object_write.c
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
plan_sources:
  - "user: 全量审查、按需中文注释；保持公开 API 与程序行为。"
  - docs/code-review/comment-standard.md
tests:
  - tests/library/test_math_input_boundaries.c
  - tests/cmake/math-input-boundary-tests.cmake
doc_type: module-detail
---

# Math 共用数值与 VM 转换契约

本批复核 math_common.h 与 common.c 的 118 个既有 canonical 键，新增定义信用为 0。范围包括 39 个公开函数声明、40 个真实 C 函数体（含 1 个内部函数）、6 个结构 typedef、4 个宏、2 个文件单元、18 个有意义块和 9 个静态字段名表。19 个成员声明随其 6 个类型审查；借用入口不重复计为本模块定义。每项定义、当前调用方、入口、约束、测试证据和结论见 [逐项台账](../coverage/zr_vm_lib_math_common.tsv)。

F 为 math-common-scalars-current-contract-review-r1.json（379bc24ac4fa2d72ff659a472fcec83249f0b46ab2675616665e9cdecf13c83b），独立 I 为 math-common-scalars-current-contract-independent-review-r1.json（f70ec6a354f4d6a13156cd2da774183fb48a8d7826d7001a62f1691b49763461）。唯一 typed 更正将参数数量错误的退出 2 与未知 selector 普通路径退出 1 区分；有效 canonical 摘要为 bdf65e62243858ab86167cb8467138fc3227fd2482015c25afbfd14da3fccd71。

## 职责与使用边界

本模块为 Math registry、module 和 native descriptor 注册的业务 callback 提供数值转换、点积、字段读取与发布、结果构造、数值快照、数组、Tensor 和字符串支持。回调通过注册表和原生调用描述符间接进入；有限检索找不到直接调用的 helper 继续保留核查入口。

6 个类型承载数值快照及 Tensor 视图。复制数值不取得 VM 或 GC 所有权，也不替代源对象保活、有效 state 或线程同步。结构体首个 double 成员不自动使后续成员成为 double 数组；Dot 的真实数组输入契约与结构体强转的调用路径分别判断。

浮点转 int64 先检查有限值和 [-2^63, 2^63) 区间，再执行转换；普通 FALSE 保持该接口的输出哨兵。复合读取依次写输出，FALSE 可能保留已完成的分量，不能据单值转换契约推断全部读取具有原子快照或失败清零语义。AVX2、SSE2、NEON 或标量由编译条件选择，没有运行时 CPU 协商；本机同配置运行不证明其他编译分支。

## 借用、发布与生命周期

字段查找可能分配键、创建临时 pin 并物化存储；返回地址是借用。调用方须保持源 VM 对象根和有效 state，后续分配或修改前按具体接口重取字段或持有适当根。数值快照按值复制；兼容 receiver 可复用，但 Finish 仍要求有效 context/state。

void 字段 writer 经 Library 的 TrySet 失败链 Throw；Core 在局部异常边界完成真实根或 lease 收尾后传播状态。字符串创建失败可 Throw MEMORY_ERROR，缓存访问通过 GetNativeString 区分真实长短字符串布局。Throw 可能越过 Math 的后续普通 C 语句，不能把 void 返回解释为静默成功或承诺所有调用路径都会执行尾部清理。

Tensor 正维乘法在相乘前检查商上界。TensorPopulate 在发布前验证长度和全部元素的数值标签，再依序写字段；发布中 setter Throw 不撤销先前字段，复用 receiver 保持已完成修改。新实例由 VM/GC 管理，Math 不自行 free，也不保证异常后立即回收。TryRun 恢复其异常或根边界，不执行被跳过的 C 尾部语句，没有构造事务回滚保证。GetStorage 的类型和转换检查不证明公开字段始终保持 rank、shape、size、data 一致。

## 既有问题与证据边界

本批保留 1 个既有 BUG 家族和 8 个既有 TODO 家族，镜像注释不重复计问题；台账中 4 个 BUG 行属于同一家族。没有行为修复。

| 家族 | 结论及具体核查入口 |
| --- | --- |
| MATH-C11-STRUCT-DOT-OBJECT-BOUNDARY | BUG：Vector3 count3 各后端尾项、Vector2 count2 AVX2/标量、Vector4 count4 SSE2/NEON 后轮及标量，已有从注册调用到 struct 强转和数组访问的静态完整链；未作运行复现。 |
| MATH-INTRINSIC-STRUCT-LOAD-BOUNDARY | TODO：核 Vector2 SSE2/NEON 与 Vector4 AVX2 单次 intrinsic load 的宿主扩展边界，不扩大为已证实 BUG。 |
| MATH-ABS-NEGATIVE-ZERO | TODO：核 Scalar Abs 对负零的公开政策。 |
| MATH-UINT64-SIGNED-POLICY | TODO：核大于 INT64_MAX 的 uint64 转换与 shape/index 接受政策。 |
| MATH-TENSOR-FRACTIONAL-SHAPE | TODO：核截断维数与原 shape 内容的兼容政策。 |
| MATH-TENSOR-FRACTIONAL-INDEX | TODO：核索引截断、stride 可表示范围及 get/set 调用入口。 |
| MATH-TENSOR-MUTABLE-INVARIANT | TODO：核公开字段修改后的存储一致性及 matmul 消费者。 |
| MATH-BOOL-HELPER-ENTRY | TODO：继续核 WriteBoolField 的注册或 callback 用途，不以无直接引用判为死代码。 |
| MATH-ZERO-DATA-HELPER-ENTRY | TODO：核 ZeroData 保留入口、忽略 Push 普通 FALSE 与完整长度要求；尚无完整消费者触发链。 |

按审查时已存在的代码撤回 6 类旧前提：void setter 静默失败、浮点转 int64 无 guard、正维乘法无溢出预检、Populate 不预检全部元素、字符串静默失败或缓存固定短布局、ArraySetValue 错误不可观察。这些是注释事实纠正；不称本轮修复，也不关闭其余数值政策或异常后状态问题。

## 实际变更与验证

实际写入两源的 107 处整注释行替换（头文件 91、C 文件 16）与 4 段 C 文件末尾 Doxygen。52 段文档中 41 段原有文档完善、7 段保留、4 段新增。所有原有非注释标记及字符串的拼写、行号、UTF-8 字节列保持，原 CRLF 保持，逆向还原原始字节精确；该检查没有运行 Clang AST。2052 引用中 38 项 owned 全同、107 项 owned 注释行变化、1346 项仅 owned 偏移变化、560 项外部全同、1 项 Common 仅偏移变化。实际应用收据 math-common-scalars-source-comment-application-r1.json（fc1944d841a4e4512ac3b7ff31e20d013755f5d570ac3dbf9dedd85ea8908384），外部控制器退出 0。

现有目标 zr_vm_math_input_boundaries_test 注册 9 个普通 main selector：int_controls、int_finite_range、int_nonfinite、int_boundaries、array_conversion、shape_product、tensor_data、tensor_controls、storage_conversion。每项 CTest TIMEOUT 为 20 秒；argc 不为 2 返回 2，未知 selector 普通路径返回 1。25 个静态 EXPECT 点不等于实际断言数，没有 Unity/CMocka footer。计数 allocator 不注入故障，VM 用例停止自动 GC。

实际 Windows Clang Debug 修改前、后分别构建 zr_vm_math_input_boundaries_test 并运行上述 9 个现有选择器，20 个构建/测试步骤均退出 0；两个阶段各 4 个只读 Ninja 查询及 14 个直接子进程均已等待结束，各 22 次输入/运行产物守门通过。验收冻结 2167 项输入（647 个当前头文件成员），983 条实际 VALID 对象依赖记录（982 C、1 MASM）及 999 个实际运行产物；测试 stdout/stderr 均为空且真实退出为 0，构建输出按阶段保留。成对验收为 math-common-scalars-native-final-acceptance-r1.json（bb6593e247f4c232a6c4020555ea0b5a246391482c625f3af35f4f4c9b5302b1）。118 行的前 8 列保持独立静态方案，最后一列记录逐项完成或无需新增注释的理由；没有“未审查”单元。

有限输入按实际转移依赖、真实 compiler argv0、完整对象路径、FileAPI 和当前头文件成员冻结。MASM 的真实 VALID 零依赖记录与 C tinydir 的零依赖批准分别核实，手工生成输入没有冒充 Ninja 动态依赖。既有 Common、Object 和 IO 注释变更纳入当前实际基线。

这些结果只覆盖记录的 Windows Clang Debug 配置和 9 个现有选择器。专用 E/tests cwd 与 CTest 默认 B/tests cwd 不同；原测试无文件型输出，未赋予文件系统行为信用。没有 OOM 注入、活跃 GC、并发、全部 callback、全部 SIMD 后端或跨平台运行结论。源状态和各阶段实际退出由外部控制器记录；产物生命周期保持 OPEN，手工释放 0 B。
