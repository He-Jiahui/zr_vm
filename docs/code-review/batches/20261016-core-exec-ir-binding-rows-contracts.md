---
doc_type: code-review-batch
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_binding_rows_artifact.c
  - tests/parser/test_ssa_static_binding_facts.c
  - tests/cmake/ssa-tests.cmake
---

# ExecIR binding rows 契约审查

本批完整11物理单元：9个函数（5个static、4个公开定义）和2个对象宏。11份中文Doxygen覆盖24参数、8个非void返回；九列台账为coverage/zr_vm_core_exec_ir_binding_rows.tsv，无未审查或新增BUG/TODO。原15153B/329LF字节和全部程序坐标保留，仅EOF追加。公开头中的4声明、private contract helper及其它编译实例均为依赖0信用。

## 调用与所有权边界

原提案及独审合计353个含原EOL引行、430处文档/台账锚点（189唯一）全部重核；完整程序出现162处为144调用、9定义、4声明、3对象宏展开和2宏定义，其中公开104调用、私有40调用。数组位置rowIndex与指令/行的双向1基关联由validator核，不要求instructionId或segmentIndex单调排序；legacy全零行字段提前成功，不验证指令shape。轻量At只作行表shape及引用范围检查，非空不代表完整语义通过。

Set先核目标及原表，再建立私有候选副本，验证通过后发布typed表、重建指令回指并free旧表；失败在独立诊断及正常不重叠拥有布局前提下保留旧表和引用，不能用它修复损坏原表。rows可借自仍有效旧表区间，但本接口不认证任意alias、extent或allocator来源，写入和释放需独占，无内部锁/并发原子发布。At旧表借用在成功替换释放时失效。Hash按字段和行数组顺序编码，不含capacity/pointer/padding/generation；允许碰撞，最终0映射1不能作唯一身份或完整相等证明。

唯一literal .c文本包含为tests/parser/ssa_eis6_fault_binding_rows.c:12，由其malloc/calloc/realloc/free宏替身构成独立编译实例；CoreModel16TU强制allocator头是另一种编译协作，不能冒称第二个literal include。生成/函数指针/跨语言可检索入口已核，无直接引用也不推断仓外未使用。

## 验证证据

实际原Binding C的BEFORE新跑CoreModel、BindingRowsArtifact、StaticBindingFacts：构建exit0、CTest3/3、直接3程序exit0；Core默认17有限场景含2个bindingrow场景及28/60故障点，64位下两项32位溢出分支显式skip；Artifact普通main输出EIS6 allocation-fault sweep PASS: 10 allocation sites，只有有限fixture和故障点结论；StaticFacts为11个Unity RUN_TEST实际全PASS。Artifact累积require失败和显式早退、Unity断言早退可能绕过局部释放，不能据成功footer扩大失败清理保证。原有unused编译flag与局部index警告记录、不改行为。各233frozen/3585observed起止稳定，当前原Binding C同时在两有限集合中。

已交付Pool61/Allocator43的依赖wholeSHA变化逐一以精确prefix+EOF注释桥接，原行证据不变、依赖0信用；Header156 phiRange TODO独立保留。已知其它Deopt535及SSA dominance失败不由本批推翻。

修改后Clang19 stdin raw lexer两次exit0，2056个非注释token的kind/spelling/StartOfLine/LeadingSpace及原始行列一致，词法输出完整消费；strict证据bbe05892e62fc4ef08ab6d505bebb49eda9f57de8d36ae8867479fd17a3fe12c。AFTER构建、CTest3/3和Core/BindingRowsArtifact/StaticBindingFacts直接运行均exit0（StaticFacts11Unity全PASS、Artifact10故障分配点），三组完整stdout/stderr及退出码与采用的真实BEFORE相等，5条argv及20项cache相同。每次233frozen/3585observed起止稳定，跨运行有限域仅本Binding C11份EOF注释变化。AFTER摘要d5b8ec3bb11ee282fdce9d00718500559d48bd3f82f2f716e0315ce82d91a886为登记E盘证据。实际source/leaf和最终暂存独审继续单独门禁；模块MD在运行后完成，不冒称暂存树运行。有限输入时点域和既有Windows Clang配置不代表完整SDK/实际所有目标闭包/完整程序或配置/原子隔离checkout/stagedtree/全套测试/Linux或默认SDK；无API、程序、断言、构建配置、新测试或源码副本变化。
