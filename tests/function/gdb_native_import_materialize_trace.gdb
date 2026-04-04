set pagination off
set confirm off
set breakpoint pending on
set debuginfod enabled off

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/import_capture_native/import_capture_native.zrp

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:1020
commands
  silent
  printf "\n[native_loader] module=%s\n", nativeModuleName
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:1026
commands
  silent
  printf "[native_loader] descriptor missing for module=%s\n", nativeModuleName
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:963
commands
  silent
  printf "[materialize] start module=%s types=%llu consts=%llu funcs=%llu\n",
         descriptor->moduleName,
         (unsigned long long)descriptor->typeCount,
         (unsigned long long)descriptor->constantCount,
         (unsigned long long)descriptor->functionCount
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:971
commands
  silent
  printf "[materialize] failed: moduleName string allocation for %s\n", descriptor->moduleName
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:979
commands
  silent
  printf "[materialize] failed: type registration module=%s index=%llu type=%s\n",
         descriptor->moduleName,
         (unsigned long long)index,
         descriptor->types[index].name
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:985
commands
  silent
  printf "[materialize] failed: constant registration module=%s index=%llu name=%s\n",
         descriptor->moduleName,
         (unsigned long long)index,
         descriptor->constants[index].name
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:991
commands
  silent
  printf "[materialize] failed: function registration module=%s index=%llu name=%s\n",
         descriptor->moduleName,
         (unsigned long long)index,
         descriptor->functions[index].name
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:998
commands
  silent
  printf "[materialize] failed: module info export module=%s\n", descriptor->moduleName
  continue
end

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_library/src/zr_vm_library/native_binding.c:1003
commands
  silent
  printf "[materialize] success module=%s module=%p\n", descriptor->moduleName, module
  continue
end

run

printf "\n[gdb] program stopped\n"
bt 12
quit
