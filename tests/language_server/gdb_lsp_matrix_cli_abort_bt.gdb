set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/bin/zr_vm_cli
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/lib
set args /mnt/e/Git/zr_vm/tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp

handle SIGABRT stop print pass
run
bt 40
frame 6
bt 12
quit
