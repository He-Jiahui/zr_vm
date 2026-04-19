set pagination off
set breakpoint pending on
break ZrCore_Object_CallFunctionWithReceiverOneArgumentFast
ignore 1 6
commands 1
silent
printf "direct one-arg wrapper hit\n"
bt 3
finish
printf "direct one-arg ret=%lld threadStatus=%d\n", (long long)$rax, state->threadStatus
continue
end
break ZrCore_Object_CallFunctionWithReceiverTwoArgumentsFast
ignore 2 1
commands 2
silent
printf "direct two-arg wrapper hit\n"
bt 3
finish
printf "direct two-arg ret=%lld threadStatus=%d\n", (long long)$rax, state->threadStatus
continue
end
run
