set pagination off
set confirm off
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/import_capture_native/import_capture_native.zrp
run
printf "Hit abort while running import_capture_native\n"
bt 12
frame 7
list
info locals
print functionSlot
print parametersCount
print expectedReturnCount
print instruction
print *opA
print BASE(functionSlot)->value
print callInfo->function
quit
