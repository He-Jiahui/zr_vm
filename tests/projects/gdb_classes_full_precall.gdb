set pagination off
set confirm off
set print pretty on
set breakpoint pending on
handle SIGABRT stop nopass

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/projects/classes_full/classes_full.zrp

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
