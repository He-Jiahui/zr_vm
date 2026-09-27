# 手工从已加载的已知原生函数快速路径单测采样一参、二参包装器返回后的状态。
# 依赖空白断点表和带符号的 Linux x86-64 目标；断点编号及 ignore 次数依赖用例顺序。
# BUG: commands 内的 finish 会恢复执行，GDB 忽略其后的 printf/continue；两段返回值采样均不会按原意执行。
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
