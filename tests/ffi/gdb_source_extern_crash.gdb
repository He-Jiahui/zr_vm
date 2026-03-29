set pagination off
set print pretty on
set breakpoint pending on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_ffi_test

break test_zr_ffi_source_extern_can_bind_and_call_symbol
break ZrCore_Exception_Throw
break zr_ffi_raise_error
break zr_ffi_symbol_invoke_array

run

commands 2
  silent
  printf "\n== ZrCore_Exception_Throw ==\n"
  printf "state=%p errorCode=%d recover=%p\n", state, errorCode, state ? state->exceptionRecoverPoint : 0
  if state && state->exceptionRecoverPoint
    printf "recover->previous=%p recover->status=%d\n", state->exceptionRecoverPoint->previous, state->exceptionRecoverPoint->status
  end
  bt 12
  continue
end

commands 3
  silent
  printf "\n== zr_ffi_raise_error ==\n"
  bt 10
  continue
end

commands 4
  silent
  printf "\n== zr_ffi_symbol_invoke_array ==\n"
  printf "state=%p selfObject=%p symbolData=%p argumentsArray=%p\n", state, selfObject, symbolData, argumentsArray
  bt 12
  continue
end
