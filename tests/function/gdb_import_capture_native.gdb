# 运行原生捕获 fixture 到中止点，检查调用参数和函数槽；frame 7 需按实际回溯确认。
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
