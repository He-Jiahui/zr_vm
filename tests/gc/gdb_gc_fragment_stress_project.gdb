# GDB script: debug VM/GC for the gc_fragment_stress project fixture (WSL/Linux gdb).
#
# Prereqs:
# - Build with debug symbols (Debug or RelWithDebInfo): zr_vm_cli + zr_vm_core + zr_vm_lib_system.
# - Current directory when gdb starts must be the repository root (same as ctest `projects`).
#
# Usage (WSL bash, repo root):
#   gdb -q -x ./tests/gc/gdb_gc_fragment_stress_project.gdb
#
# BUG: 先用 -ex 设置 $zr_cli 会被下方 set $zr_cli 覆盖，无法更换 CLI 路径。
# 使用其他构建目录时，请先修改下方默认 CLI 路径再运行本脚本。
#
# MSVC: use WinDbg/cdb with the Windows zr_vm_cli + PDB, or debug the Linux binary in WSL with this script.

set pagination off
set confirm off
set print pretty on
set print elements 256
set breakpoint pending on

# Default CLI path — edit to match your build output directory.
set $zr_cli = "./build-wsl-gcc/bin/zr_vm_cli"

eval "file %s", $zr_cli
set args tests/fixtures/projects/gc_fragment_stress/gc_fragment_stress.zrp
cd .

# BUG: 下方提示仍建议覆盖 $zr_cli，但上方 file 已选定程序，事后改变量不会重选。
printf "Using CLI path (override convenience variable zr_cli if wrong):\n"
printf "%s\n", $zr_cli
printf "Args: tests/fixtures/projects/gc_fragment_stress/gc_fragment_stress.zrp\n"

# --- Crash path: full backtrace on SIGSEGV ---
catch signal SIGSEGV
commands
  silent
  printf "\n===== Caught SIGSEGV =====\n"
  thread apply all bt full
  frame 0
  info registers
  quit 1
end

# --- GC / system.gc entry points (enable only what you need; see `info breakpoints`) ---
break ZrCore_GarbageCollector_GcStep
break ZrCore_GarbageCollector_GcFull
break garbage_collector_single_step
break ZrSystem_Gc_Collect

printf "\nTip: incremental noise is high — `disable 2 3` (GcStep/GcFull) and keep `garbage_collector_single_step` or `ZrSystem_Gc_Collect`.\n"
printf "Tip: `commands` on a breakpoint can `printf` + `bt 20` + `continue` for trace-only.\n"
printf "Tip: under load use `set logging file gc_frag.txt` + `set logging on`.\n\n"
printf "Type `run` to start.\n"
