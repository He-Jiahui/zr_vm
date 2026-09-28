# 手工定位项目特性测试中的原型继承列表崩溃；目标和库路径绑定旧 WSL 调试目录。
# TODO: 仓库内未找到此脚本的自动调用点；重新运行前核对目标二进制和绝对路径。
set pagination off
set confirm off
set breakpoint pending on
handle SIGSEGV stop print nopass
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/bin/zr_vm_language_server_lsp_project_features_test
set env LD_LIBRARY_PATH /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/lib
run
printf "\n=== bt ===\n"
bt
printf "\n=== bt full ===\n"
bt full
printf "\n=== frame 1 locals ===\n"
# 以下打印假定第 1 帧仍有 prototype 局部变量且指针可读。
# TODO: 栈形态或崩溃点变化时先重新确认该前提，再执行字段解引用。
frame 1
printf "inherits.length=%llu elementSize=%llu capacity=%llu head=%p\n", \
       (unsigned long long)prototype->inherits.length, \
       (unsigned long long)prototype->inherits.elementSize, \
       (unsigned long long)prototype->inherits.capacity, \
       prototype->inherits.head
x/8gx prototype->inherits.head
printf "implements.length=%llu elementSize=%llu capacity=%llu head=%p\n", \
       (unsigned long long)prototype->implements.length, \
       (unsigned long long)prototype->implements.elementSize, \
       (unsigned long long)prototype->implements.capacity, \
       prototype->implements.head
x/8gx prototype->implements.head
quit
