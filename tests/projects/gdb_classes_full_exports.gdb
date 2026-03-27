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
run ./tests/projects/classes_full/classes_full.zrp
quit
