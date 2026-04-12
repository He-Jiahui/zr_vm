set pagination off
set confirm off
set breakpoint pending on

file ./build/codex-wsl-gcc-relwithdebinfo-ci-make/bin/zr_vm_cli
set args ./tests/benchmarks/cases/container_pipeline/zr/benchmark_container_pipeline.zrp --execution-mode aot_c --require-aot-path

handle SIGSEGV stop print nopass

run
bt
frame 0
info args
info locals
up
info args
info locals
