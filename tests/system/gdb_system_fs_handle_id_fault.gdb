set pagination off
set confirm off
set breakpoint pending on

file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug/bin/zr_vm_system_fs_test

break zr_ffi_try_lower_handle_id_wrapper
commands
silent
printf "\n-- lower_handle_id entry --\n"
print value != 0
print wrapperObject
print type != 0
print type->kind
 next
 print targetTypeName
continue
end

break zr_ffi_try_read_handle_id_field
commands
silent
printf "\n-- read_handle_id_field entry --\n"
print wrapperObject
print closed
print zr_ffi_find_field_raw(state, wrapperObject, "closed")
print zr_ffi_find_field_raw(state, wrapperObject, "__zr_ffi_handleId")
print zr_ffi_find_field_raw(state, wrapperObject, "handleId")
next
next
print fieldValue
if fieldValue != 0
    print *fieldValue
end
continue
end

break zr_ffi_build_scalar_argument
commands
silent
printf "\n-- build_scalar_argument entry --\n"
print value
print type != 0
if type != 0
    print type->kind
end
continue
end

break zr_ffi_raise_error
commands
silent
printf "\n-- ffi raise error --\n"
bt 5
continue
end

break ZrCore_Exception_Throw
commands
silent
printf "\n-- core exception throw --\n"
bt 6
continue
end

run
bt
quit
