# 追踪 classes_full 调用前的值类型，执行后再检查失败栈；固定 frame 仅适用于原始故障现场。
# BUG: 当前首方源码已无 ZrFunctionPreCall，断点无法命中，后续 bt/局部变量输出不能证明调用路径。
set pagination off
set confirm off
set print pretty on
set breakpoint pending on
handle SIGABRT stop nopass

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/classes_full/classes_full.zrp

break ZrFunctionPreCall
commands
silent
set $callValue = &stackPointer->value
printf "PreCall: type=%d raw=%d isNative=%d ptr=%p\n", $callValue->type, $callValue->value.object ? $callValue->value.object->type : -1, $callValue->isNative, $callValue->value.object
continue
end

run
bt 20
frame 7
info locals
print *opA
print *opB
print *(base + 0)
print *(base + 1)
print *(base + 2)
print *(base + 3)
