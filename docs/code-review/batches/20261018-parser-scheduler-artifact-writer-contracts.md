# Parser scheduler artifact writer 合同

相关代码：

- `zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c`

11单元、1struct+7真body+3顺序block、11Dox，全commented。0旧、11个遗漏单元已补齐。

首条source fact、三类身份、八节载荷；编码先于破坏性打开，普通失败清理非原子；writer.h和LSP只借用，不取定义信用。

实际源码应用见 parser-artifact-family-source-comment-equivalence-r1.json，SHA256 7de541c3ee1dc7cf5159167e80d346cbfbc0551f859871f2ec7d27d2506c0d8b；原程序字节、原行列及同配置的核对见 同一等价收据：原始程序字节和原位置完整保留，EOF仅追加完整注释与空白；前后配置身份相同；未执行编译器词法或AST比较。

关联共享artifact_schema中的scheduler路径；其25注册已含于P的57，不能再次相加。独占E phase工作目录保护scheduler_source_contract.zro及两个早拒候选路径。 修改前 driver 30196：artifact_schema=0；修改后 driver 15916：artifact_schema=0；共享输入端点/argv/原始日志/目录快照见 parser-artifact-family-native-final-acceptance-r1.json，SHA256 1726d54b8d2a0b997de308f746941764ef3909cff177b97a38655b2183c1c8c4；BEFORE/AFTER原始输出、选中依赖和每步独占目录快照均保留于E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。只将实际结果写入本段，不据静态注册数推断PASS，不把direct当CTest验收。

外域LSP-SCHEDULER-SOURCEFACT-UAF-01为静态可达未运行复现，本批不改LSP、不增加Scheduler issue键或修复信用。

最终台账见 [逐单元台账](../coverage/zr_vm_parser_scheduler_artifact_writer.tsv)；inventory仅本批既有键sha/status/note三列按实际交付投影，其余字节保持。共享运行与借用定义不重复计信用。

独立源码/台账复核和暂存收据须按各自门禁登记并通过后，方可串行提交；不授权未来parent或全仓验收。

## 物理源身份与验证限制

- zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c：3fecf56ba96d800ba84268852ff5bf7fe26b0eef06efac32da7fa0b3f2175220

共享验证只含现有6目标、8direct和4只读Ninja查询，未运行CTest/GDB/WSL/POSIX或全仓测试；build原始警告保留。普通失败、OOM和非局部异常仍以各行合同/既有证据限制为准。生命周期OPEN，人工释放0B；复用构建及必要成功、失败原始证据保留，无源码副本。

## 文件末尾格式修订与原生证据版本

暂存检查发现新增注释末尾有一个多余空行。本批只删除 byte[33277] 的 LF，最终文件为 3fecf56ba96d800ba84268852ff5bf7fe26b0eef06efac32da7fa0b3f2175220（33277B、551LF）；全部 11 处 Doxygen、前 551 行及原程序 14162B 均逐字节保持。格式修订和本批索引恢复见 parser-scheduler-artifact-writer-eof-format-erratum-r2.json，SHA256 1f305d0ea4afa9ffb074eccd0e03c92f71fd290a081b4238068e516fcb24ee17。

已有 AFTER-r2 原生结果实际使用 1ab7020a2a629ad44df6520fac09b8020232f601f09a99583e0dbe89a6a6bc0f（33278B、552LF）。最终少一个空行的 raw 文件未单独重新执行原生验证；上述精确字节差量仅衔接这一格式变化，不改写原生结果或原等价收据，也不增加运行或问题信用。
