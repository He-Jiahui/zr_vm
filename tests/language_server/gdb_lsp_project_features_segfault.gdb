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
