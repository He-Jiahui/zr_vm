# Clang 构建下记录装饰器编译期导入的失败调用栈。
# TODO: 当前 file/args 指向旧工作区绝对路径；改到本仓库构建和 fixture 后复验。
set pagination off
file /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-clang-debug/bin/zr_vm_cli
set args /mnt/d/Git/Github/zr_vm_mig/zr_vm/tests/fixtures/projects/decorator_compile_time_import/decorator_compile_time_import.zrp
run
bt
quit
