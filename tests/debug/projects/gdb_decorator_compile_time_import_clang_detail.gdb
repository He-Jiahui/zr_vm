# 在执行期报错入口暂停并采集两层栈帧，定位编译期导入故障来源。
# TODO: file/args 指向旧工作区绝对路径；现行 execution_raise_vm_runtime_error 仍存在，需以本仓库 Clang 构建复验。
set pagination off
file /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-clang-debug/bin/zr_vm_cli
set args /mnt/d/Git/Github/zr_vm_mig/zr_vm/tests/fixtures/projects/decorator_compile_time_import/decorator_compile_time_import.zrp
break execution_raise_vm_runtime_error
run
bt
frame 1
info locals
frame 7
info locals
quit
