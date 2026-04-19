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
