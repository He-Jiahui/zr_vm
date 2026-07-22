# 04-M7 concurrent major + artifact/AOT/LSP 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
的 `M7 concurrent major + artifact/AOT/LSP`。

## 状态与产出记录

- 完成时间：2026-07-23 00:27 +08:00
- 状态：已完成
- 已完成项目：
  - 已实现domain-local initial snapshot、budgeted concurrent mark、短remark/sweep与按budget
    compact的major state machine；另一个domain不进入当前pause集合。
  - 已用统一domain mutation lock和object write barrier闭合并发mark期间的新边；remark重扫
    roots与barrier-fed work，focused stress未出现漏标。
  - 已发布按domain identity/generation归因的major pause/mark/remark/compact、barrier、
    safepoint及cross-domain prepare/publish/claim/commit/abort、object/byte telemetry。
  - 已把domain transfer optional artifact row投影到canonical consumer，VM/AOT按TypeDef/
    TypeSpec identity消费同一kind/schema/provider/flags，不按名称或文本重建。
  - 已让新source/SemIR/ExecBC/AOT分别使用`OWN_VIEW_SHARED`、`OWN_VIEW_MUT`、
    `OWN_INTO_GC_BOX`、`OWN_RETURN_TO_GC`；legacy borrow/loan/detach仅保留旧artifact reader兼容。
  - 已让source ownership hover和diagnostic消费同一canonical type/ownership/query facts，并补齐
    stable `resource_shared_strong_cycle`消息表注册一致性。
  - 已由ASan/UBSan复审修复minor forwarding对合法空function/prototype/cache/module字段的
    非空安全cast；空入口保持零work，不改变forwarding或prototype-less object合同。
  - 已完成GCC/Clang/MSVC最终矩阵、ASan/UBSan、ThreadSanitizer、文档复审与61-path
    exact audit；提交由本记录对应里程碑commit承载。

## 最终验证结果

- GCC focused：concurrent major 10/10、cross-domain transfer 24/24、compiler integration
  127/127、resource Unique 19/19、owner borrow 6/6、union 69/69、canonical consumer 16/16、
  semantic query 27/27、AOT C ownership 1/1，ownership-opcode与resource-drop ExecBC/AOT
  focused各1/1，全部真实进程`exit 0`。
- 最终同一冻结overlay矩阵：GCC与Clang各26/26进程真实`exit 0`，MSVC共同门禁
  25/25真实`exit 0`；每套22份Unity汇总合计474 tests/0 failures，GCC/Clang额外
  LSP interface进程也为`exit 0`。MSVC该额外target存在既有重复导出链接边界，未计入也
  未宣称GREEN。
- ASan/UBSan在关闭随机地址布局后重放concurrent major、完整GC、cross-domain transfer、
  transfer race与canonical consumer：5/5进程、121 tests/0 failures、sanitizer marker 0；
  ThreadSanitizer重放concurrent major与transfer race：2/2进程、15 tests/0 failures、
  race marker 0。
- 扩大到非M7门禁的owner-borrow sanitizer时，在未由本里程碑修改的
  `execution_member_access.c` cache路径发现另一个空function UBSan问题；本记录不把该额外
  目标记为GREEN，也不越界捆绑修复。其常规GCC/Clang/MSVC owner-borrow 6/6均通过。
- 最终diff check无错误；冻结WSL/MSVC snapshot与工作树61个M7 exact paths逐字节一致，
  LSP路径、三份外部dirty Syntax草案、build/log与根目录生成物均为forbidden=0。

## 当前实现边界

- concurrent major是incremental/mutator-concurrent marker，不引入TaskScheduler或public
  `Send/Sync`，也不把collector worker注册成language mutator。
- compact仍是短domain-local pause，只有显式非零budget才执行；`GcFull`保持同步全收集合同。
- LSP路径不由本里程碑修改；LSP只消费parser/canonical facts，不拼owner前缀、不按诊断文本或
  成员名重建transfer/ownership identity。
- legacy opcode保留numeric ABI reader兼容，但新compiler/writer不得重新发出。
