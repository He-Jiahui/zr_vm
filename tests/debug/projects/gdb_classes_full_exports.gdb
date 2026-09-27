# 供 classes_full 导出异常排查；调用方须以当前 zr_vm_cli 作为 GDB inferior。
# BUG: 当前首方源码已无 ZrModuleAddPubExport，pending 断点不会记录任何导出；需定位现行导出入口并更新脚本。
set pagination off
set breakpoint pending on
set $hit = 0
break ZrModuleAddPubExport
commands
silent
set $hit = $hit + 1
printf "hit=%d name=%s valueType=%d module=%p\n", $hit, (char*)name->stringDataExtend, value->type, module
continue
end
run ./tests/fixtures/projects/classes_full/classes_full.zrp
quit
