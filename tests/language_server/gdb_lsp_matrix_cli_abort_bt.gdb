# 手工复现 language feature matrix 的 CLI 中止：构建目录和 fixture 路径均为当次 WSL 调试环境。
# TODO: 仓库内未找到此脚本的自动调用点；再次使用前核对目标构建和绝对路径是否仍存在。
set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/bin/zr_vm_cli
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/lib
set args /mnt/e/Git/zr_vm/tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp

handle SIGABRT stop print pass
run
# 中止后列出最内层最多 40 帧，选中第六帧；后续 bt 12 仍从最内层列帧。
# TODO: 固定 frame 6 依赖当次栈深度；新构建中应按首个项目帧重新选择。
bt 40
frame 6
bt 12
quit
