# 追踪模块导出记录和对应调用帧槽位，供 import_capture_native 手工诊断。
# TODO: 断点仍指向旧工作树 module/module.c:857；当前模块加载实现位于 module/module_loader.c，复用前须重定位导出收集入口。
set pagination off
set confirm off
file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/import_capture_native/import_capture_native.zrp
break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_core/src/zr_vm_core/module/module.c:857
commands
silent
printf "\nmodule export collection hit\n"
print pathStr
print func->stackSize
print func->exportedVariableLength
if func->exportedVariables != 0
  print func->exportedVariables[0].stackSlot
  print func->exportedVariables[0].accessModifier
  print func->exportedVariables[0].name
  print *(callBase + 1 + func->exportedVariables[0].stackSlot)
end
continue
end
run
quit
