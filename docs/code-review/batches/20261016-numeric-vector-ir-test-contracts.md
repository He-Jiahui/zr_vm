---
doc_type: code-review-batch
related_code:
  - tests/parser/test_ssa_numeric_vector_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c
implementation_files:
  - tests/parser/test_ssa_numeric_vector_ir.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_numeric_vector_ir.c
  - tests/cmake/ssa-tests.cmake
  - tests/CMakeLists.txt
---

# Numeric IR 既有断言测试契约审查

本批仅审查 tests/parser/test_ssa_numeric_vector_ir.c：9个场景、make_request与main，共11个完整函数定义和函数体。11段中文Doxygen含3个参数、2个非void返回；九列表为 coverage/tests_parser_numeric_vector_ir.tsv，11项commented，无未审单元或新增BUG/TODO。Vector测试另9函数仍归active SSA会话，本批没有完成或修改该文件。

原16581B、345LF源码前缀完整保留，只在EOF追加20560B、355LF文档；未改程序token、签名、断言、构建配置或现有测试场景。台账前8列及注释逐项复用已独审20单元提案的Numeric11子集，reason列仅将候选状态更新为实际应用状态。当前11函数并无指针、callback或Unity入口；17处本文件真实静态调用、165处函数/宏调用及91个静态assert表达式分别记录，不将外部同名main/make_request或循环次数算成额外场景。789个EOL行pins与9个借用实现body用于证明契约，借用实现不产生本批信用。

## 断言、失败路径与资源

main固定调用9个场景，没有参数选择、Unity hook或逐例成功输出；只有启用C assert时，夹具构造、检查和正常销毁才按正文发生。assert失败终止进程，可能跳过后续资源释放，不是可恢复的测试框架失败。

测试覆盖策略默认字段、部分非法枚举/位、语义求交、request shape与若干形成的VECTOR/SCALAR_FALLBACK计划；没有执行真实SIMD算术或反向内存访问。严格策略NO_SIGNED_ZERO负例先在非RELAXED有效位门禁拒绝，不能用它证明后续signed-zero分支；hash重复相等不证明无碰撞。Legalize的归约许可TODO已在数值公共契约批次记录，本批沿用其待核边界，不新增BUG或修复行为。

## 构建入口与证据边界

实际注册为tests/cmake/ssa-tests.cmake:1287–1296的zr_vm_ssa_numeric_vector_ir_test / ssa_numeric_vector_ir，经tests/CMakeLists.txt:740纳入顶层产品配置。断言开关检查以实际生成的源编译命令为准。旧20单元静态/独审报告保持不可变，当前17借用依赖只有NumericH精确原16298B前缀+102EOF注释变化；SSA私有接口迁移与Vector归属没有转交给本批。

Numeric103历史AFTER之后，两个SSA CMake检查脚本发生外部变更，因此本批重新运行BEFORE，没有将历史3591观察结果改写为当前全文件接受。复用E盘Windows Clang19.1.5 Debug构建，单目标build、CTest及直接main按实际退出和完整日志比较；30个锁定输入、3592个有限观察输入不构成完整编译依赖闭包、SDK/配置或原子checkout隔离，也不代表逐分支、Linux或默认SDK验证。

严格词法比较通过，原程序 token、flags 与原坐标一致；两次 stdin raw lexer 完整消费输出并退出0。既有 Numeric 单目标 BEFORE/AFTER 构建均exit0、CTest均1/1、直接main均exit0；完整stdout/stderr逐字相同且为空，三个argv、20项cache及实际Numeric源编译命令相同且无NDEBUG。30个锁定输入、3592个有限观察输入在各次运行首尾稳定，前后仅本测试C的11段EOF注释变化。实际source/leaf与stage独审、串行提交仍待完成。
