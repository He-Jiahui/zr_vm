---
doc_type: code-review-batch
related_code:
  - tests/parser/exec_ir_clone_test_allocator.c
  - tests/parser/exec_ir_clone_test_allocator.h
  - tests/parser/exec_ir_clone_test_allocator_cases.inc
implementation_files:
  - tests/parser/exec_ir_clone_test_allocator.c
  - tests/parser/exec_ir_clone_test_allocator.h
  - tests/parser/exec_ir_clone_test_allocator_cases.inc
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_compare_metadata_guards.c
  - tests/cmake/ssa-tests.cmake
---

# ExecIR 克隆分配器测试支持契约审查

本批完整43单元：C17、H9、cases.inc17，合计23真实函数体、4接口声明、2类型、4跟踪状态、9宏（含1guard）和1个native allocator重映射边界块。42份中文Doxygen含43参数及9非void返回标签，普通guard在43行台账中记录无需新增注释，没有未审查或新增BUG/TODO。台账为coverage/tests_parser_exec_ir_clone_allocator_support.tsv；956含原EOL引行和全部可检索程序调用/宏/强制头及文本包含入口均核。

## 入口、故障与资源

C先#undef四个allocator映射，再调用真正的libc malloc/calloc/realloc/free及内部跟踪节点分配，避免包装器递归。H的四接口经CoreModel目标16个TU强制包含，由四个对象宏在各编译单元词法替换；静态出现14malloc/45calloc/1realloc/148free不是动态分配次数。两个exec_ir.c literal文本包含使用自己的rename与故障替身，不能当作第二份同名物理公开定义。inc由CoreModel翻译单元条件包含、普通main调用其中场景，12个helper/场景与构建开关、调用顺序共同组成入口。

故障ordinal与live登记属于本支持C的单个实例；Reset、FailAt、查询及所有包装分配/释放须串行，无内部原子或锁。Reset不代替释放live业务块；live节点登记维护与原始allocator来源需配对，不能对任意地址或不匹配实例保证可释放性。realloc成功后旧地址借用失效，应使用返回值；正size失败保留保证不扩张到size0。普通expect失败通过exit终止整个程序，不先走当前函数末尾Reset/Free，也没有Unity长跳转语义。

容量字节溢出断言只在SIZE_MAX==UINT32_MAX分支执行，其他size_t构型只打印前提提示；32位路径需要在相应平台取得单独执行证据。当前Core默认14本文件场景+3包含allocator场景，5个另行CLI选择入口不算默认全部已跑；28/60故障循环只表示现有有限fixture，不证明全输入或全部分配顺序。R2仅去掉三处长期Doxygen内的“本轮验收”措辞，保留size0限制和平台分支事实；16文字镜像及9派生字段逐项核，原R1和其REVISE报告原样保留。

## 验证证据

本批采用刚完成Pool61的真实AFTER作为本支持三源原始BEFORE：当时C/H/inc仍是原始字节；三目标构建0、CTest3/3、三个普通程序exit0，Core28/60故障点及invalid-input通过、64位平台两条32位溢出分支显式跳过。Compare为63自定义case和346前提的普通main，Value仅13固定调用及成功footer，不冒称Unity逐例。采用前重新核233frozen/3585observed全匹配，采用不新增一轮运行。

已交付测试77的三源EOF注释和Core614旧时序注释更正，以及Pool61 EOF注释，都作为明确的依赖桥接，源坐标和调用位置保留、依赖0信用。原43提案956引行保留，当前614批准注释的新行哈希单列；已知其它Deopt535及SSA dominance失败不由本批推翻。

修改后Clang19 stdin raw lexer六次exit0，3992个非注释token的kind/spelling/StartOfLine/LeadingSpace及原始行列一致，词法输出完整消费；strict证据81a5ca79fbeb0ba9456e0d34909b7d089cb7d81dada5810295bb90e598b6b329。AFTER构建、CTest3/3和core/value/compare直接运行均exit0，三组完整stdout/stderr及退出码与采用的真实BEFORE相等，5条argv及20项cache相同。每次233frozen/3585observed起止稳定，跨运行3585observed有限域仅本三源42份EOF注释变化，233frozen中仅本头文件变化（C/inc不在冻结子集）。AFTER摘要7f2b03fce144649baa166dbfb9a099e843459d26f77147933b1be4f166c46810为登记E盘证据。实际source/leaf和最终暂存独审继续单独门禁；模块MD在运行后完成，不冒称暂存树运行。有限233/3585时点域与既有Windows Clang配置不代表完整SDK/实际依赖闭包/完整程序或配置/原子或隔离checkout/stagedtree/全套测试/Linux或默认SDK。无API、程序语句、断言、构建配置、新测试或源码副本变化。
