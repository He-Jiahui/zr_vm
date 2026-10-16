---
doc_type: code-review-batch
related_code:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_compare_metadata_guards.c
implementation_files:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_compare_metadata_guards.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_compare_metadata_guards.c
---

# ExecIR 模型、value 与比较元数据测试契约审查

本批三份人工测试源码77单元：Core22、Value16、Compare39；56完整函数体、4条件接口声明、5类型、8全局状态/名称表、3可调用宏和1条件文本包含块逐项核对。75份中文Doxygen含56参数标签及17非void返回标签，Compare的空setUp/tearDown仅满足链接且已有说明，台账保留2项无需新增注释结论。63个比较参数化场景从属于函数，不另增规范键。逐项台账为coverage/zr_vm_core_exec_ir_model_verifier_tests.tsv；1149条含原EOL的引行已核，宏/条件编译/allocator替换和真实构建入口一并追踪。

## 职责、入口与失败边界

Core和Value均为普通C main，expect失败打印后exit，不继续局部Free。Core默认14个本文件场景加3个条件文本包含的allocator场景；另有5个独立CLI选择入口，默认运行不代表全部选择器已执行。CloneFunction和CloneModule的逐分配故障循环实际输出28及60个PASS；32位size_t溢出场景在当前64位平台显式跳过。Value的13个固定测试调用只认成功footer与可达断言，不作Unity逐例输出或未到达验证分支的证明。

Compare虽然经CMake Unity辅助入口链接，其main不调用RUN_TEST或UnityDefaultTestRun；由20个emission、20个stored、2个opaque和21个VM场景自行计数。EXPECT记录错误后继续，prerequisite返回布尔并让所属分支走done清理；空hook不建立Unity/longjmp失败语义。fixture持有state/context/semantic/module/projection，按实际初始化标记和root状态逆序清理；输入与投影的字节快照仅为测试期借用/副本，不能扩张为全部池或所有失败路径的生产保证。各复合断言只能证明实际检查的合取结果，不从FALSE反推唯一子条件；前置append或Parser拒绝不算后续Core verifier分支覆盖。

## 原有注释更正

Core原614行声称“第二临时函数的value pool分配后失败”，实际在本场景前置分配成功且无注入故障时，第二源函数缺少其instructionCount声称存在的instruction池，clone shape预检先拒绝；随后释放候选module此前函数和已初始化当前槽。仅替换这一完整注释行并保持缩进及换行数，所有断言和程序语句原样；原始定义/body证据保留，修改后该函数raw span变化按注释更正单列。Core617/619–621/622–624仅证明实际旧目标数量、token73和已检查字段，不宣称完整旧目标逐字节或全路径无泄漏。不存在新增运行BUG或TODO。

## 验证证据

实际原始三源BEFORE构建通过，CTest3/3通过，三个普通程序均exit0；233 frozen与3585 conservative observed输入起止稳定。Compare原始日志实际63 CASE PASS、346 PRECONDITION PASS，0失败；Core28/60故障点及invalid-input汇总通过，Value成功footer可见。摘要4cba987738bbd399353688eb1a8deb7e61911e383e3c53e5cc95464bf2c76218为登记E盘入口。旧独审R2的reused_failure_and_test_limits[0]把Compare错误描述为Unity，保留原报告并在该基线摘要明确勘误；75份提案Doxygen本来准确，未为修正报告措辞改代码。

修改后Clang19 stdin raw lexer六次exit0，12,857个非注释token的kind/spelling/StartOfLine/LeadingSpace及原始行列完全一致，原始词法输出逐字节消费；strict证据66d328aaf560ee5373cdc32b7b076fdad2ff5b93d26814eb8b850ddaec3a6801。AFTER构建、CTest3/3及core/value/compare直接运行均通过，三组完整stdout/stderr与退出码同BEFORE逐字节相等，Compare仍63场景346前提通过；5条argv及20项cache相同。每次233 frozen/3585 observed起止稳定，跨运行在该有限域仅自有三源注释变化，不推断完整程序/配置或SDK闭包及原子隔离。AFTER摘要00be2e13ac23fd4762915121e7341e166f0bcbde78abb0321b18bc5d3554bd70为已登记E盘证据。实际source/leaf与最终暂存独审随后作为独立门禁，模块MD在运行后完成，不冒称暂存树执行。Core既有注释替换必须采用与原始源码匹配的Git before作词法基线，不能声称未变原字节前缀；Compare原CRLF保留而Git blob按既有规则规范为LF，证据区分两者。已知其它Deopt535与SSA dominance失败不由本批推翻，不外推全仓、Linux、默认SDK、原子checkout、隔离行为、完整SDK输入冻结或暂存树执行。

本批仅75份文档注释、一处经完整调用链证实的旧注释更正、77行台账、模块记录及inventory三行审核列，不改实现、公开签名、断言或构建配置。无重复实现的新测试，无源码副本；复用登记E盘build/cache，释放0B，生命周期开放，必要证据保留。
