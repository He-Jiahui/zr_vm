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
