set pagination off
set confirm off
set print pretty on
handle SIGPIPE nostop noprint pass
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
