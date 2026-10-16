---
doc_type: code-review-batch
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_ssa_compare_metadata_guards.c
  - tests/parser/exec_ir_clone_test_allocator_cases.inc
  - tests/parser/exec_ir_clone_test_allocator.c
  - tests/parser/exec_ir_clone_test_allocator.h
---

# ExecIR 动态池与拥有生命周期契约审查

本批完整61单元：55函数（12 static、43公开定义）、4宏、1静态opcode表和1个schema include/undef注册块。所有定义及55完整函数体、3913条含原EOL引行已核；61份中文Doxygen覆盖175参数和44非void返回标签，全部61项已注释、没有未审查或新增BUG/TODO。相关测试与公开Header156为调用与合同依赖，不重复计入本批。台账为coverage/zr_vm_core_exec_ir_pool_lifecycle.tsv。

## 使用、发布与生命周期

池reserve可能迁移存储并使旧借用指针失效，容量不是逻辑count；空追加按itemCount=0仍请求reserve并发布空range，不声称必定新分配。AppendInstruction先reserve再核部分结果约束，失败可能已扩大容量，不能承诺所有FALSE保持完整目标字节不变。Init/Free要求真实可读或可释放存储；形状预检不认证任意地址。调用期源保持稳定、目标变更及释放由上层串行。

CloneFunction在候选函数内复制普通池逻辑count，目标capacity由增长策略决定；bindingRows按源capacity请求且实际可向上取整，只复制源count行。CloneModule先初始化并计入当前候选槽再克隆，因此统一失败清理涵盖此前函数和当前部分槽；成功最后替换旧目标。sealed、generation与stateMap由实际上下层入口维护，不将原始追加接口扩张成自动重建保证。两个literal exec_ir.c文本包含与allocator宏替身是独立编译实例，生成schema项也不当成新增物理函数。

## 证据与边界

接管预检发现上一测试77运行后另一份Parser源码改变，在任何登记或写入前终止旧基线采用；随后在当前工作区实际重跑三目标，原始本文件51293B/1147LF、SHA7f5bd0c576723714461459fd9447cc1c121d89c89e35c8e9be9300f4fca644ac仍未修改。新BEFORE构建0、CTest3/3和直接程序3/3 exit0，Core28/60故障点PASS，当前64位平台显式跳过32位size_t溢出两例。Compare为63自定义场景/346前提的普通C main，Value仅13固定调用与成功footer，不记为Unity逐例运行。采用该新运行前再次核233 frozen和3585 observed全匹配；基线摘要只采用确已完成的新运行，不将前次并发差异视为本批注释效果。41个静态test profiles仍仅证明实际到达的断言，关联helper不代表独立分支全覆盖。

前批三份测试仅注释变化导致全文件依赖SHA更新；3913引行中仅Core614旧英文时序误述更正，按前批完整shape预检/候选回滚证据接受同一行的新哈希，合同正文不变且不重复计信用。Header的phiRange域差异TODO不在本C新增。已知其它Deopt535与SSA dominance失败继续保留。

修改后Clang19 stdin raw lexer两次exit0，6625个非注释token的kind/spelling/StartOfLine/LeadingSpace及原始行列一致，词法输出完整消费；strict证据43d5bc7581e4cc4b97036fb3d4816d2b2cf0a9d3b646585a2aaed446753d5f7b。AFTER构建、CTest3/3和core/value/compare直接运行均exit0，三组完整stdout/stderr及退出码与采用的真实BEFORE相等，5条argv及20项cache相同。每次233frozen/3585observed起止稳定，跨运行有限域仅本C61份EOF注释变化。AFTER摘要41b273c7469ace22c51defb1c8d56baac5b1a0b00274344ba07b66cbfd2ae96d为登记E盘证据。实际source/leaf和最终暂存独审继续单独门禁；模块MD在运行后完成，不冒称暂存树运行。有限233/3585时点域与现有Windows Clang配置不代表完整程序/配置/SDK闭包、原子checkout、隔离行为、stagedtree运行、全套测试、Linux或默认SDK。无API/程序语句/断言/构建配置修改，也无新增测试或源码副本。
