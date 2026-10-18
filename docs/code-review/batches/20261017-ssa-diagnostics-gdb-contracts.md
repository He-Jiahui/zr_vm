# SSA人工GDB诊断脚本合同

本批逐项审查5个单元；0个旧键不重复计数，新增5个键。定义、全部可检索调用、间接入口、现有测试、约束与每项结论记录于[逐单元台账](../coverage/ssa_diagnostics_gdb.tsv)；选中行无未审查项。

审查两份手写GDB脚本的5个自然协议块，仅在EOF追加中文说明，保留全部25条非注释命令及原位置。CFG回调打印的是finish入口现场，不代表完成成功或提交；liveness脚本只捕获经过UnityFail的命名停点，UINT32断言可走另一失败入口，不能声称捕获所有Unity失败。调试状态借用inferior，继续或quit均不延长产品对象寿命；quit也不承诺原测试tearDown已执行。未检索到程序化GDB调用时，保留人工-x/--args入口与历史使用证据，不据此判死代码。

本轮实际运行35case CFG与9case liveness可执行文件；未运行GDB脚本、验证ClangCL PDB的GDB兼容性或POSIX调试。WSL已知环境拒绝保留，不能代替执行证据。

## 验证与边界

复用实际checkout与E:/cargo-targets/zr_vm/build/first-party-review-01a13463/clang-cl-products-debug；ClangCL/Debug、Ninja，配置、工具及选中依赖按真实前后记录核对。共同验证构建六目标，直接运行七步骤；故意失败探针的退出1为必要结果。以下只授予本模块对应步骤的有限证据。

| 现有步骤 | BEFORE实际退出 | AFTER实际退出 | 预期 |
| --- | ---: | ---: | ---: |
| ssa_cfg | 0 | 0 | 0 |
| ssa_liveness | 0 | 0 | 0 |

第一轮真实build-only尝试因外部compile_expression_types.c在构建期间变化而actual1，七测试均未启动，原失败保留，不升级为通过。后续baseline将该源加入strict106并重新运行；七个本任务文件仅有注释变化，原程序文本与位置保持，公开API签名及行为未修改。

实际pair验收：released-contracts-native-final-acceptance-r1.json，SHA256 5d5cff63a88d2315c9510c098632c845063ae9f56e713cc45c31c4dde21827ed；注释等价证明SHA256 003df9bdde1a52b86298fddd4823411089fe308e9921a9214144b36ad4c23765；独审 ssa-diagnostics-gdb-current-contract-independent-review-r1.json，SHA256 4f43f1e32c8ea5a70cbcdeeec9e6b1190c230ffb4db197870dfc1840722c7bbf。完整收据及原始输出在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。全仓清单仍存在外域pending，不据本批声称全仓完成。

## 源身份与提交

- tests/parser/gdb_ssa_source_straight_line_cfg.gdb：956226ed1492229ad801856fd0cdb718604bcf4586532363f3077234d88f2b1b
- tests/parser/gdb_ssa_state_map_liveness.gdb：7c75b1d2486f193ba835305c2ebfe6253374803ddabd0681b1d2d0c51d7e2693

提交前必须核对仅本批差异并通过git diff --cached --check、既有check-batch及独立暂存审查；本批不增加重复实现的测试。其他会话的暂存、源码与混合台账外行保留。构建与必要成功/失败证据保留，生命周期OPEN，manual freed0B。
