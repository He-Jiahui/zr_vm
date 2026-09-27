# 跟踪 runProbe 导出绑定时闭包捕获槽的关闭与后续写入。
set pagination off
set confirm off
set print pretty on
handle SIGPIPE nostop noprint pass
set breakpoint pending on

break module_loader_bind_exported_function
commands
silent
if exported != 0 && exported->name != 0 && strcmp(ZrCore_String_GetNativeString(exported->name), "runProbe") == 0
  printf "runProbe bind: forceRecreate=%d\\n", forceRecreate
  # BUG: GDB 在 breakpoint commands 中执行 next 后忽略余下命令；
  # 命中 runProbe 时下列闭包检查和 watchpoint 均不会设置。
  next
  set $closure = (SZrClosure*)slotValue->value.object
  print $closure->closureValueCount
  print $closure->closureValuesExtend[2]->value.valuePointer
  print &($closure->closureValuesExtend[2]->link.closedValue)
  watch -l $closure->closureValuesExtend[2]->link.closedValue.type
  continue
end
continue
end

run
bt
frame 0
info locals
quit
