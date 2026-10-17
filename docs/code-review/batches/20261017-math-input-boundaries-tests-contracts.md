---
related_code:
  - tests/library/test_math_input_boundaries.c
  - tests/cmake/math-input-boundary-tests.cmake
  - tests/CMakeLists.txt
implementation_files:
  - tests/library/test_math_input_boundaries.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/library/test_math_input_boundaries.c
  - tests/cmake/math-input-boundary-tests.cmake
doc_type: code-review-batch
---

# Math 输入边界测试：调用与生命周期契约

本批以实际 main/CTest 分发、真实 Library/Core/Math 调用及当前断言复审一个测试文件。共 28 单元：15 函数、1 类型、4 状态、1 宏和 7 个需解释的语句块；块的顺序约束写入所属函数的 Doxygen，并单列于[逐单元台账](../coverage/math_input_boundaries_tests.tsv)。全部 28 行已有定义、调用方、入口/测试证据和契约结论，均为 commented；没有未审单元、新 BUG 或 TODO。新增文件级说明不另算定义信用。

## 入口、职责和调用前提

tests/cmake/math-input-boundary-tests.cmake:6 建立 zr_vm_math_input_boundaries_test，tests/CMakeLists.txt:797 引入；注册的九个 selector 每次独立进程运行，超时均为 20 秒。目标以真实静态 Parser/Core/Library/Math 支持运行，既不替换实现，也不改 EXPECT、参数、CMake 或测试范围。

| selector | 当前断言所覆盖的职责 |
| --- | --- |
| int_controls | 有限截断与整数极值的成功对照，以及 BOOL 标签拒绝。 |
| int_finite_range | 所选有限超界 double 被拒绝，输出哨兵保持。 |
| int_nonfinite | 所选 NaN、正负 Infinity 被拒绝，输出哨兵保持。 |
| int_boundaries | int64 半开接受区间的四个精确 double 常量。 |
| array_conversion | 真实数组的整数读取拒绝，以及无效 shape 总大小。 |
| shape_product | 在生产支持层拒绝 INT64_MAX 与 2 的形状积，不在测试中计算溢出乘法。 |
| tensor_data | STRING、BOOL、NULL 数据标签被 Populate 拒绝。 |
| tensor_controls | 保留小数维度向零截断策略，核字段与原数组身份。 |
| storage_conversion | 恢复 rank 后再变异 size，隔离两个公开字段的整数转换入口。 |

四个纯值 selector 不创建 VM；五个 VM 场景只在 global 初始化成功后运行。GlobalState_New 第三实参 1 是 uniqueNumber。普通 Object 外壳虽调用 Object_Init，helper 不单独保证 nodeMap.isValid，也不代表完整 Tensor 类构造。

## 资源、失败状态与时序

分配账表只拥有账目，实际块由 realloc/free 管理；有效前缀随释放交换删除，索引不稳定。旧指针、尺寸和类别必须共同匹配，拒绝与自然 realloc 失败保留原块。满表在成功分配之后 abort，不是可恢复 OOM；此分配器没有故障注入或并发保护。

EXPECT 失败会继续执行，用 failures 汇总诊断；失败不会自动恢复 VM。数组、字符串和 Object 指针借用同一 VM，局部值不是持久 GC 根。stopGcFlag 只限制当前测试窗口，不保证任意 GC/OOM 保活。SetString 和 void 字段 setter 可以 Throw，main 未对整个场景建立 finally；GlobalState_Free 的收尾保证仅限到达普通控制流。测试不创建文件或目录夹具。

## 注释纯度与实际验证

本次只在原文件 EOF 追加 22 份 Doxygen，含 24 个函数参数、1 个宏参数和 5 份非 void 返回契约。原 8615 字节、208 行及每行原 EOL 保持，追加 47123 字节后为 55738 字节、887 行。Clang 严格原始词法比较确认 1772 个非注释 token 的 kind、spelling、StartOfLine、LeadingSpace 和原行列全部相同；不把原始 lexer 等同于完整预处理或 ABI 证明。

修改前后复用同一 E 盘 clang-cl Debug 配置，分别构建当前测试目标并运行九个原 selector。两次目标构建和各九个原测试进程均实际退出 0，没有超时。 18 条测试输出流的比较为 18 对均逐字节相同，全部为空流；实际退出码、原始双流、超时、PID 和已等待终态保留于任务 first-party-review-01a13463 的 reports。跨阶段 14 个本地静态库及构建图4的 hash、字节数、mtime 保持；仅本测试对象、EXE 和 PDB允许因追加注释后的重建变化。

当前验证绑定 37 份有限输入、4 份实际构建图和 9 个工具。构建图以校验过的同一快照进入运行基准；17 个产物（EXE、PDB、测试对象和 14 个本地静态库）加图4在每次 selector 前及最后一次后实际守门。结果保存这些窗口的端点，未伪称逐次 hash receipt、完整宿主闭包、无 ABA、强制 GC/OOM、完整 Tensor 类或跨平台验收。验证脚本的 R1/R2 在执行前被独审拒绝并保留，实际执行使用已接受的 R3。

## 复核和交付边界

语义提案经过独立复核，修正了把 Object_Init 调用夸大为完整初始化成功的返回承诺；源码实际应用、严格 lexer、前后原生配对再经独立复核。提交前按正常 index、仅本批路径和私有 index 核暂存差异与 cached check，串行提交只含本批注释、台账、清单和导航文档。

本批为 28 个新 canonical key；历史 Root 交付计数由 2844 变为 2872，该数不是全仓去重覆盖率。借用实现和其它会话的文件不授定义信用。构建、缓存及原始证据保留在 E:/cargo-targets/zr_vm；本批无手工清理，释放 0B，产物生命周期仍为 OPEN。
