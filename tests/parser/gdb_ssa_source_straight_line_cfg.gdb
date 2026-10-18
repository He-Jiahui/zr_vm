set pagination off
set confirm off
set breakpoint pending off
start
break compiler_semantic_cfg_finish
commands
  silent
  printf "source CFG finish: active=%u block=%u instructions=%llu\n", cs->preSemanticIrCfgActive, cs->preSemanticIrCfgBlock, (unsigned long long)cs->preSemanticIr.instructions.length
  continue
end
continue
if $_exitcode != 0
  quit 1
end
quit 0

# @unit gdb_ssa_source_straight_line_cfg.gdb::configure_and_start
# CFG 观察会话启动
#
# ## Summary
# 为源 CFG 完成入口的现场观察建立独立批处理调试会话。先关闭分页和退出确认、禁止未解析断点，再用 start 进入被调试程序的 main 停点；此后才安装 CFG finish 断点，观察窗口不包括 main 之前的调用。
#
# ## Parameters
# 本块没有函数参数。调用者通过 GDB 的 --args 提供待调试可执行文件和原有实参，并通过 -x 选择本脚本；应选用与当前源码、调试字段及 GDB 支持格式匹配的程序。配套目标为 zr_vm_ssa_source_straight_line_cfg_test。
#
# ## Returns
# 没有函数返回值。成功执行后 GDB 保持测试进程停在启动位置，继续解释后续断点配置；启动或符号解析出错时应检查 GDB 自身错误，不能假定末尾的退出码分支已经执行。
#
# ## Constraints and lifecycle
# 这些 set 命令修改当前 GDB 会话状态，宜用于独立批处理会话。脚本不指定可执行路径、不构建程序、不设宿主超时，也不隔离用户 GDB 初始化或其他断点。被调试进程和 GDB 会话由外部调用者启动、监督；本块不取得产品对象所有权。历史文档中的 build/ 路径属于当时示例，实际可执行文件路径由调用者提供。
#
# ## References
# tests/parser/gdb_ssa_source_straight_line_cfg.gdb:3 设置未解析断点策略；tests/parser/gdb_ssa_source_straight_line_cfg.gdb:4 在观察点安装前启动进程。
# tests/cmake/ssa-source-straight-line-tests.cmake:3 定义配套测试目标；tests/cmake/ssa-source-straight-line-tests.cmake:9 注册直接运行程序的 CTest。
# tests/acceptance/ssa-source-straight-line-cfg.md:186 记录历史人工调试使用，不能替代当前执行结果。

# @unit gdb_ssa_source_straight_line_cfg.gdb::cfg_finish_observation
# CFG finish 入口断点回调
#
# ## Summary
# 把一组 GDB commands 绑定到 compiler_semantic_cfg_finish 的入口断点。每次命中时静默读取活动标志、当前 CFG 块和 SemanticIR 指令数量，输出一行现场并继续程序，用于区分源编译阶段是否进入该完成入口。
#
# ## Parameters
# 没有 C 回调参数。表达式中的 cs 是停点栈帧里的被调试函数参数，须非空并可读取 preSemanticIrCfgActive、preSemanticIrCfgBlock 和 preSemanticIr.instructions.length。函数实现自身可以拒绝空 cs，但本打印表达式不执行该保护；调试调用者应使用有效的编译器现场。
#
# ## Returns
# 没有函数返回值。结果通过 GDB 标准输出交付，continue 恢复被调试程序；输出是入口现场，不代表函数返回 TRUE，也不代表 CFG 已提交或验证成功。脚本没有命中次数断言。
#
# ## Constraints and lifecycle
# cs 及其嵌套记录仍属于 inferior，本脚本不转移所有权。字段只在当前停点按有效调试信息读取，继续执行后对象或字段可能变化，不能继续把地址当作同一现场。活动 CFG 的事务调用也可能传入 stagedCompiler；不能把所有命中都解释为原记录已成功更新。故障测试宏重命名得到的 ssa_test_compiler_semantic_cfg_finish 是另一调试符号，本断点不承诺同时覆盖它。脚本不指定多线程调度或对产品状态加锁。
#
# ## References
# tests/parser/gdb_ssa_source_straight_line_cfg.gdb:5 注册断点；tests/parser/gdb_ssa_source_straight_line_cfg.gdb:8 读取三个现场字段；tests/parser/gdb_ssa_source_straight_line_cfg.gdb:9 恢复执行。
# zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg.c:1103 定义目标函数，入口之后才执行空值和活动状态检查。
# zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_finalize.c:730 把 stagedCompiler 传给该入口。
# zr_vm_parser/include/zr_vm_parser/compiler.h:93、zr_vm_parser/include/zr_vm_parser/compiler.h:98、zr_vm_parser/include/zr_vm_parser/compiler.h:104 定义被读取的嵌套函数、活动标志和块字段。

# @unit gdb_ssa_source_straight_line_cfg.gdb::inferior_exit_status
# 恢复执行与退出码映射
#
# ## Summary
# 在观察回调安装后恢复测试程序，并在本次程序退出后依据 $_exitcode 选择 GDB 的结束状态。该层把程序结果交给人工批处理调用者，和前面的 CFG 入口打印分别承担结果门与现场观察职责。
#
# ## Parameters
# 没有函数参数。输入是 GDB 为本次被调试程序退出记录的 $_exitcode；只有该值确实可读且属于本次退出时，数值比较才具有此处约定的含义。
#
# ## Returns
# 非零 inferior 退出码走 quit 1；零退出码走 quit 0。这里只显式区分零与非零，不保留原始非零数值，也不验证断点曾命中、打印内容正确或每个测试分支已执行。
#
# ## Constraints and lifecycle
# 若程序因信号、额外断点或其他原因停住且尚未产生本次有效退出码，本块不定义额外的成功判据，应查看 GDB 自身结果和停止原因。脚本没有宿主超时。quit 结束调试会话；本块不承担测试内部对象回收，也不把旧 acceptance 记录当作当前程序结果。
#
# ## References
# tests/parser/gdb_ssa_source_straight_line_cfg.gdb:11 恢复程序；tests/parser/gdb_ssa_source_straight_line_cfg.gdb:12 检查退出码；tests/parser/gdb_ssa_source_straight_line_cfg.gdb:13 和 tests/parser/gdb_ssa_source_straight_line_cfg.gdb:15 定义两个显式结束状态。
# tests/parser/test_ssa_source_straight_line_cfg.c:902 是配套程序入口；tests/parser/test_ssa_source_straight_line_cfg.c:939 由 UNITY_END 产生原程序结果。
