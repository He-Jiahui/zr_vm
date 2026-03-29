set pagination off
set confirm off
break ZrFfi_LoadLibrary
break zr_ffi_raise_error
run load-failure
continue
bt
quit
