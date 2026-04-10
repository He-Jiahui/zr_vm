set -e
ROOT=/mnt/e/Git/zr_vm
BUILD=$ROOT/build/codex-wsl-gcc-debug-current-make/tests
python3 - <<'PY'
from pathlib import Path
import shlex, subprocess
flags = {}
flags_file = Path('/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/tests/CMakeFiles/zr_vm_type_inference_test.dir/flags.make')
for line in flags_file.read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = v
cmd = ['/usr/bin/gcc']
cmd += shlex.split(flags['C_DEFINES'])
cmd += shlex.split(flags['C_INCLUDES'])
cmd += shlex.split(flags['C_FLAGS'])
cmd += ['-MD', '-MT', 'tests/CMakeFiles/zr_vm_type_inference_test.dir/parser/test_type_inference.c.o', '-MF', 'CMakeFiles/zr_vm_type_inference_test.dir/parser/test_type_inference.c.o.d', '-o', 'CMakeFiles/zr_vm_type_inference_test.dir/parser/test_type_inference.c.o', '-c', '/mnt/e/Git/zr_vm/tests/parser/test_type_inference.c']
subprocess.run(cmd, check=True, cwd='/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/tests')
PY
cd "$BUILD"
sh CMakeFiles/zr_vm_type_inference_test.dir/link.txt
cd ..
./bin/zr_vm_type_inference_test > /tmp/zr_type_inference_after_source_interface_fix.txt 2>&1 || true
grep -n 'FAIL:' /tmp/zr_type_inference_after_source_interface_fix.txt || true
grep -n 'test_type_inference_source_interface_members_flow_through_inheritance_chain\|test_type_inference_source_generic_constraint_accepts_source_interface_implementation' /tmp/zr_type_inference_after_source_interface_fix.txt || true
tail -n 20 /tmp/zr_type_inference_after_source_interface_fix.txt