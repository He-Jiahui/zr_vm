# BUG: 目标返回阶段断点的 finish 恢复执行，使本命令块后续 printf/continue 被忽略；预期阶段输出缺失。
# TODO: 脚本绑定旧 WSL 构建目录，仓库内未找到自动调用点；复用前核对路径和断点符号。
set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/bin/zr_vm_cli
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/lib
set args /mnt/e/Git/zr_vm/tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp

start

break module_init_prescan_source_summary
# BUG: 原拟只打印目标模块的预扫描结果，但 finish 恢复执行后本命令块的 printf/continue 被忽略。
commands
silent
set $name = summary && summary->moduleName ? (char *)summary->moduleName->stringDataExtend : 0
if $name && strcmp($name, "core_semantics") == 0
  finish
  printf "\n[module_init_prescan_source_summary] ret=%d state=%d hasPrescan=%d exports=%zu bindings=%zu entryEffects=%zu callableSummaries=%zu\n", (int)$rax, (int)summary->state, (int)summary->hasPrescan, (size_t)summary->exports.length, (size_t)summary->bindings.length, (size_t)summary->entryEffects.length, (size_t)summary->exportedCallableSummaries.length
end
continue
end

break module_init_analyze_source_summary
# BUG: 原拟比较分析后的摘要，但本块的 finish 恢复执行后，后续 printf/continue 被忽略。
commands
silent
set $name = summary && summary->moduleName ? (char *)summary->moduleName->stringDataExtend : 0
if $name && strcmp($name, "core_semantics") == 0
  finish
  printf "\n[module_init_analyze_source_summary] ret=%d state=%d hasAnalysis=%d entryEffects=%zu callableSummaries=%zu error=%s\n", (int)$rax, (int)summary->state, (int)summary->hasAnalysis, (size_t)summary->entryEffects.length, (size_t)summary->exportedCallableSummaries.length, summary->errorMessage
end
continue
end

break module_init_validate_summary
# BUG: 原拟观察校验结果，但本块的 finish 恢复执行后，后续 printf/continue 被忽略。
# TODO: 这些函数返回 unsigned char；修复输出时还需按返回宽度取值并核对目标架构 ABI，不能直接信任整个 $rax。
commands
silent
set $name = summary && summary->moduleName ? (char *)summary->moduleName->stringDataExtend : 0
if $name && strcmp($name, "core_semantics") == 0
  finish
  printf "\n[module_init_validate_summary] ret=%d state=%d error=%s\n", (int)$rax, (int)summary->state, summary->errorMessage
end
continue
end

break module_init_summary_set_error
# 错误路径在写入摘要时留局部调用栈，帮助关联上面三个阶段。
commands
silent
if summary && summary->moduleName
  set $name = (char *)summary->moduleName->stringDataExtend
  if $name && strcmp($name, "core_semantics") == 0
    printf "\n[module_init_summary_set_error] module=%s message=%s\n", $name, message
    bt 6
  end
end
continue
end

continue
quit
