from pathlib import Path
import shlex, subprocess
root = Path('/mnt/e/Git/zr_vm')
build = root / 'build/codex-wsl-gcc-debug-current-make/zr_vm_parser'
flags = {}
flags_file = build / 'CMakeFiles/zr_vm_parser_shared.dir/flags.make'
for line in flags_file.read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = v
base = ['/usr/bin/gcc'] + shlex.split(flags['C_DEFINES']) + shlex.split(flags['C_INCLUDES']) + shlex.split(flags['C_FLAGS'])
compile_specs = [
    ('type_inference_core.c', 'zr_vm_parser/CMakeFiles/zr_vm_parser_shared.dir/src/zr_vm_parser/type_inference_core.c.o'),
    ('type_inference_native.c', 'zr_vm_parser/CMakeFiles/zr_vm_parser_shared.dir/src/zr_vm_parser/type_inference_native.c.o'),
]
for src_name, mt in compile_specs:
    obj_rel = mt.replace('zr_vm_parser/', '')
    dep_rel = obj_rel + '.d'
    src = f'/mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/{src_name}'
    cmd = base + ['-MD', '-MT', mt, '-MF', dep_rel, '-o', obj_rel, '-c', src]
    subprocess.run(cmd, check=True, cwd=str(build))
link_cmd = (build / 'CMakeFiles/zr_vm_parser_shared.dir/link.txt').read_text().strip()
subprocess.run(shlex.split(link_cmd), check=True, cwd=str(build))
PY = None
