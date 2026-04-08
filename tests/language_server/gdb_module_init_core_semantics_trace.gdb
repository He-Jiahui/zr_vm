set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/bin/zr_vm_cli
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/lib
set args /mnt/e/Git/zr_vm/tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp

start

break module_init_prescan_source_summary
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
