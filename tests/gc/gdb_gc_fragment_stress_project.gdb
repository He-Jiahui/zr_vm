# GDB script: debug VM/GC for the gc_fragment_stress project fixture (WSL/Linux gdb).
#
# Prereqs:
# - Build with debug symbols (Debug or RelWithDebInfo): zr_vm_cli + zr_vm_core + zr_vm_lib_system.
# - Current directory when gdb starts must be the repository root (same as ctest `projects`).
#
# Usage (WSL bash, repo root):
#   gdb -q -x ./tests/gc/gdb_gc_fragment_stress_project.gdb
#
# Override CLI path (if your build dir differs):
#   gdb -q -ex 'set $zr_cli="/mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli"' -x ./tests/gc/gdb_gc_fragment_stress_project.gdb
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
