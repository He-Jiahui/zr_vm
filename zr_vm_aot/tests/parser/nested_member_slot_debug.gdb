set pagination off
set unwindonsignal on
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-member-slot/bin/zr_vm_execbc_aot_pipeline_test
# BUG: 断点指向不存在的根 tests/parser，且 7431 已漂移到测试源码字符串；无法在预期上下文停下，后续 state/function 表达式失去前提。
break /mnt/e/Git/zr_vm/tests/parser/test_execbc_aot_pipeline.c:7431
run
call (int)ZrParser_Writer_WriteIntermediateFile(state, function, "/mnt/e/Git/zr_vm/tmp_nested_member_slot.zri")
quit
