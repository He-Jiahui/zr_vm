# Usage: gdb -q -batch -x tests/parser/gdb_ssa_state_map_liveness.gdb
#        --args build/ssa-gcc-debug/bin/zr_vm_ssa_state_map_liveness_test
set pagination off
set confirm off
break UnityFail
commands
silent
print 'ssa_state_map_fixture.h'::diagnostic
bt 5
quit 1
end
run
