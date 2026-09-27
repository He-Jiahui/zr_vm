# 由 gdb --args <zr_vm_ffi_probe> 启动；定位 load-failure 的异常来源。
set pagination off
set confirm off
break ZrFfi_LoadLibrary
break zr_ffi_raise_error
run load-failure
continue
bt
quit
