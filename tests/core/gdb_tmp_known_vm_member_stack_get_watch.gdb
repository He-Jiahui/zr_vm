# 原意是在 callable-metadata 单测中监视 KNOWN_VM_CALL 的栈读取 helper 计数变化。
# BUG: 当前 test_execution_dispatch_callable_metadata.c:495 是无捕获函数转发用例中的空行；
# 该用例没有 profileRuntime 局部变量，print 和 watch 无法按原脚本建立有效观察点。
set pagination off
set breakpoint pending on
break tests/core/test_execution_dispatch_callable_metadata.c:495 if callerFunction->memberEntries == 0
run
print profileRuntime
watch profileRuntime->helperCounts[2]
commands
silent
bt 12
continue
end
continue
quit
