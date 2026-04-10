set -e
ROOT=/mnt/e/Git/zr_vm
BUILD=$ROOT/build/codex-wsl-gcc-debug-current-make/tests
python3 - <<'PY'
from pathlib import Path
import shlex, subprocess
flags = {}
flags_file = Path('/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/tests/CMakeFiles/zr_vm_compiler_features_test.dir/flags.make')
for line in flags_file.read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = v
cmd = ['/usr/bin/gcc']
cmd += shlex.split(flags['C_DEFINES'])
cmd += shlex.split(flags['C_INCLUDES'])
cmd += shlex.split(flags['C_FLAGS'])
cmd += ['-MD', '-MT', 'tests/CMakeFiles/zr_vm_compiler_features_test.dir/parser/test_compiler_features.c.o', '-MF', 'CMakeFiles/zr_vm_compiler_features_test.dir/parser/test_compiler_features.c.o.d', '-o', 'CMakeFiles/zr_vm_compiler_features_test.dir/parser/test_compiler_features.c.o', '-c', '/mnt/e/Git/zr_vm/tests/parser/test_compiler_features.c']
subprocess.run(cmd, check=True, cwd='/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/tests')
PY
cd "$BUILD"
sh CMakeFiles/zr_vm_compiler_features_test.dir/link.txt
cd ..
./bin/zr_vm_compiler_features_test > /tmp/zr_compiler_features_after_forced_rebuild.txt 2>&1 || true
grep -n 'FAIL:' /tmp/zr_compiler_features_after_forced_rebuild.txt || true
tail -n 20 /tmp/zr_compiler_features_after_forced_rebuild.txt