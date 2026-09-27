# 旧的捕获槽写入观察脚本；由调用者提供带符号的模块测试可执行文件。
set pagination off
set confirm off
set print pretty on
handle SIGPIPE nostop noprint pass
# BUG: 当前 2235 行是前一场景的 destroy_test_state(state)，并非预安装闭包场景；
# 首次命中会观察错场景，需重选目标场景中取得 runExport 后的断点。
break /mnt/e/Git/zr_vm/tests/module/test_module_system.c:2235
run
set $closure = (SZrClosure*)runExport->value.object
print $closure->closureValueCount
print &($closure->closureValuesExtend[2]->link.closedValue)
print $closure->closureValuesExtend[2]->link.closedValue
watch -l $closure->closureValuesExtend[2]->link.closedValue.type
continue
bt
frame 0
info locals
print $closure->closureValuesExtend[2]->link.closedValue
quit
