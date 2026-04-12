set pagination off
set unwindonsignal on
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-member-slot/bin/zr_vm_execbc_aot_pipeline_test
break /mnt/e/Git/zr_vm/tests/parser/test_execbc_aot_pipeline.c:7431
run
call (int)ZrParser_Writer_WriteIntermediateFile(state, function, "/mnt/e/Git/zr_vm/tmp_nested_member_slot.zri")
quit
