---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_internal.h
plan_sources:
  - "user: 全量调用链审查、按需中文注释、问题只标记，按功能模块提交。"
  - docs/code-review/comment-standard.md
  - docs/cli-and-tooling/zr-debugger-v1-launch-workflow.md
tests:
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_evaluation_effect_policy_cases.h
doc_type: review-batch
---

# Debug 兼容诊断与表达式跳读契约

本批把原台账45个完整三列键迁移到当前eval物理路径，不新增定义信用。范围为22个函数体、20个内部声明、2个结构与1个头文件保护宏；结果36 commented、8 BUG、1 no-comment。每个单元核对直接调用、回调入口、失败状态及资源有效期；27份有限完整输入和464处证据的范围以冻结审查记录为准。原叶只删除已针定的45行，其余742行和表头逐字节保留。

24段中文Doxygen补齐诊断缓冲区、parser借用字段、同步回调、短路跳读和普通返回恢复的契约。两源仅在EOF增加完整注释；原375行字节、行号、UTF-8列及EOL保持。跳读仍可能分配字符串；回调FALSE可推进cursor或部分输出；普通返回时的字段恢复不延伸为nonlocal Throw、GC保活、线程安全或全部状态回滚保证。

8个BUG结论和5段Doxygen共用DED-BUG-001一类静态缺陷：有效暂停态使用允许legacy fallback的Evaluate入口，对失败表达式传入非NULL、容量0且未NUL终止的错误区时，setter不写入，而右操作数诊断细化仍可能strcmp越过该缓冲区。完整调用路径已核对，未运行该故障组合；本批只标记，不改行为。其它明确禁用legacy fallback的入口不据此扩散缺陷结论。

关联模块文档按13项精确文字操作修正当前路径及formal-first/fallback契约，保留历史执行事实。允许legacy fallback且formal parser失败或未处理时才进入兼容解析；prepare或parse的普通失败不证明canonical facts齐备。原说明中无证据的能力、恢复或所有权推论已收窄。

现有同一FileAPI目标修改前后构建均退出0；直接运行相同56项Unity入口，各55PASS、1FAIL、0IGNORE，全部逐项结果、stdout和stderr一致。物理失败是tests/debug/test_debug_evaluation_effect_policy_cases.h:484的policy.hasCanonicalFacts；主翻译单元的Unity文件标签不等于断言物理文件。getter效应位断言及getter执行未到达，根因未唯一确定，不增设BUG家族。

配对报告debug-eval-diagnostics-current-native-pair-r1.json，SHA256 943a6afea97886eba86f34e0e8f12cd550ed018978aeb2f31da7645e7791c059；原始前后结果97e3862aeca798d3d185b9cb3eac46d0303c33e3ff3d6d6c77a881d1529b27ff、62fcceecab779fa599fce87da5f257057c68dc07f91d6e6e7481b4047d81be97。配对记录保留三处Root补证文件名勘误，实际存在路径已重读；勘误不改变测试结果或根因未定边界。

执行采用当前checkout及现有VS/Clang构建，直接EXE运行不称为CTest；120秒只是任务runner上限。有限普通测试不代表45项或零容量故障全覆盖，不宣称全仓通过。inventory仅本批两个源文件的sha/status/note变更，49行86格外来差异原样保留；全局check修改前后仍报告同一既有stale，具体原始输出和退出码另记checks。

新叶check-batch只检查九列格式、完整键与非空物理行锚；不承担调用语义或清单哈希证明。7处约束中的换行仅改为空格，45行证据去掉括号角色标签以适配该现有校验器；路径、行号、顺序、问题家族及合同内容保持，原文完整保留在冻结F。
