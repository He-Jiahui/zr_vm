# SSA 07.02: constant i64 C11/LLVM text emitter slice

## Scope and trust boundary

This batch emits standalone C11 source and textual LLVM IR only for an AOTIR
module containing one function, one entry block, one `CONSTANT` and one
`RETURN`. The shared gate requires an explicit
`ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64` declaration and rejects `UNKNOWN`; the
emitter further rejects extra instructions, values, effects, capabilities,
memory/GC/deoptimization state, layouts and relocations. It returns bounded
text to a caller-owned buffer; it neither compiles nor registers a VM
artifact. The existing C/LLVM adapter `requireArtifact` path remains
`ARTIFACT_UNAVAILABLE`.

The fixture's `returnTypeToken` and constant `typeToken` are both the hand-made
opaque metadata token `17u`. Token equality does not prove that `17u` denotes
the language's i64 type. Only a trusted producer's explicit `NOARGS_I64`
declaration authorizes this slice to interpret `constant.bits` as i64. The
fixture supplies that declaration to exercise the consumer; it does not
authenticate a real producer. The current `ZrParser_ExecIr_LowerAot` source
path always emits `UNKNOWN`, so it cannot accidentally enter this path.
`ZrCore_AotIr_RequireExecutableAbi` checks ABI/SSA shape but is not a full CFG
dominance proof; this emitter's single block and exact two-instruction shape
make that limitation inapplicable to accepted input.

The emitted function symbol has a standalone no-argument i64 ABI. It is not
the VM's `FZrAotEntryThunk`, which requires `SZrState*`. The AOTIR target
triple/ABI hashes are validated as nonzero schema identities, but they are
not matched against the host compiler by this slice. Compiling the text uses
the selected compiler's host target. No loader, registry, or native coverage
claim follows from these results.

## RED, fixture and failure coverage

`tests/parser/test_ssa_aot_scalar_text.c` was added before the emitter files.
Its first WSL GCC `-std=c11 -Wall -Wextra -Werror -fsyntax-only` run failed
because `backend_aot_ir_scalar_text.h` did not exist. After the initial GREEN,
an added regression for a second, unused result-pool entry failed an assertion
at line 182 under GCC; the shared shape check now rejects extra result and
operand entries and a missing block terminator marker. The final test also
rejects `UNKNOWN`, unsupported effects and constant flags, extra NOP, an
extra block flag, and a null module. Both C and LLVM entry points are tested
with an eight-byte output buffer. Failure clears `output[0]` and resets
`outLength` to zero when the corresponding pointer/capacity exists; diagnostics
retain a status and, where relevant, function/instruction identity.

The positive fixture emits `42`, `-1` and `INT64_MIN`. The test checks the emitted
source text and length, then writes `constant_42.c`, `constant_min.c`,
`constant_neg1.c`, `constant_42.ll`, `constant_min.ll`, `constant_neg1.ll`, and
`runner.c` only when passed an output
directory. The runner calls `zr_aot_scalar_fn_1()` and exits 0 only if the
returned `int64_t` equals the expected value. Thus the generated text is
compiled and called; a string comparison alone is not counted as execution.

## Direct compiler evidence

Commands ran from `E:\Git\zr_vm`; all generated files and executables were
under `D:\tmp\zr_vm\ssa-aot-scalar-text-{gcc,clang,msvc}`. The direct test
sources were the fixture, three new emitter `.c` files, `aot_ir.c`, and
`exec_ir_state_map_storage.c`. The shared include directories were the
backend AOT, parser, core and common include directories. Toolchains were
WSL GCC 11.4.0, WSL Clang 14.0.0 and MSVC x64 19.44.35228.0. The final commands
for Linux were:

```text
wsl.exe --exec bash -lc "cd /mnt/e/Git/zr_vm && gcc -std=c11 -Wall -Wextra -Werror -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include tests/parser/test_ssa_aot_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c zr_vm_core/src/zr_vm_core/aot_ir.c zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c -o /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-gcc/test_ssa_aot_scalar_text && /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-gcc/test_ssa_aot_scalar_text /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-gcc"
wsl.exe --exec bash -lc "cd /mnt/e/Git/zr_vm && clang -std=c11 -Wall -Wextra -Werror -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include tests/parser/test_ssa_aot_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c zr_vm_core/src/zr_vm_core/aot_ir.c zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c -o /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-clang/test_ssa_aot_scalar_text && /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-clang/test_ssa_aot_scalar_text /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-clang"
wsl.exe --exec bash -lc "cd /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-gcc && gcc -std=c11 -Wall -Wextra -Werror constant_42.c runner.c -DEXPECTED=42 -o run_c_42 && ./run_c_42"
wsl.exe --exec bash -lc "cd /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-gcc && gcc -std=c11 -Wall -Wextra -Werror constant_min.c runner.c -DEXPECTED=INT64_MIN -o run_c_min && ./run_c_min"
wsl.exe --exec bash -lc "cd /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-gcc && gcc -std=c11 -Wall -Wextra -Werror constant_neg1.c runner.c -DEXPECTED=-1 -o run_c_neg1 && ./run_c_neg1"
wsl.exe --exec bash -lc "cd /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-clang && clang -Wno-override-module -x ir constant_42.ll -x c runner.c -DEXPECTED=42 -o run_llvm_42 && ./run_llvm_42"
wsl.exe --exec bash -lc "cd /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-clang && clang -Wno-override-module -x ir constant_min.ll -x c runner.c -DEXPECTED=INT64_MIN -o run_llvm_min && ./run_llvm_min"
wsl.exe --exec bash -lc "cd /mnt/d/tmp/zr_vm/ssa-aot-scalar-text-clang && clang -Wno-override-module -x ir constant_neg1.ll -x c runner.c -DEXPECTED=-1 -o run_llvm_neg1 && ./run_llvm_neg1"
```

MSVC x64 used the repository `using-vsdevcmd` wrapper. `/utf-8` avoids the
host ACP 936 warning in repository UTF-8 headers under `/WX`;
`_CRT_SECURE_NO_WARNINGS` matches this test's direct CRT file-writing use.
The final commands were:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS /Izr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot /Izr_vm_parser\include /Izr_vm_core\include /Izr_vm_common\include tests\parser\test_ssa_aot_scalar_text.c zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_ir_scalar_text.c zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_ir_scalar_c.c zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_ir_scalar_llvm.c zr_vm_core\src\zr_vm_core\aot_ir.c zr_vm_core\src\zr_vm_core\exec_ir\exec_ir_state_map_storage.c /FoD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\ /FeD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\test_ssa_aot_scalar_text.exe
D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\test_ssa_aot_scalar_text.exe D:\tmp\zr_vm\ssa-aot-scalar-text-msvc
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS /DEXPECTED=42 D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\constant_42.c D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\runner.c /FoD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\ /FeD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\run_c_42.exe
D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\run_c_42.exe
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS /DEXPECTED=INT64_MIN D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\constant_min.c D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\runner.c /FoD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\ /FeD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\run_c_min.exe
D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\run_c_min.exe
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS /DEXPECTED=-1 D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\constant_neg1.c D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\runner.c /FoD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\ /FeD:\tmp\zr_vm\ssa-aot-scalar-text-msvc\run_c_neg1.exe
D:\tmp\zr_vm\ssa-aot-scalar-text-msvc\run_c_neg1.exe
```

Every final direct test compilation, fixture execution, generated-source
compilation, and generated-function invocation above exited 0. GCC and Clang
both compiled the fixture with `-Werror`; MSVC used `/W4 /WX`. The C source
was invoked under GCC and MSVC for all three values; LLVM IR was invoked under
Clang for all three values. These runs prove the selected standalone functions
return the expected values on those hosts, with the trust boundary above.
The root task independently re-ran the six generated `42`/`INT64_MIN`
executables and the three compiler-built fixture executables; all nine exits
were 0. The added `-1` artifacts were then compiled and invoked by this task.

## Formal test registration and decision

After the artifact-schema task committed its independent CMake hunk, this
slice registered target `zr_vm_ssa_aot_scalar_text_test` and CTest
`ssa_aot_scalar_text` in `tests/cmake/ssa-tests.cmake`. The Windows native
CMake 3.23.0-rc2 configuration and focused low-parallel build used this
dedicated cache:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake -S E:\Git\zr_vm -B D:\tmp\zr_vm\ssa-aot-scalar-cmake-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake --build D:\tmp\zr_vm\ssa-aot-scalar-cmake-msvc --target zr_vm_ssa_aot_scalar_text_test --parallel 2
ctest --test-dir D:\tmp\zr_vm\ssa-aot-scalar-cmake-msvc -C Debug -R ^ssa_aot_scalar_text$ --output-on-failure --no-tests=error
```

Configuration and build both exited 0; the focused CTest passed 1/1 (Test
#245). The root task independently reran the same formal CTest and observed
1/1 pass. CMake reported object-path-length warnings for unrelated
numeric-loop and LSP targets, and MSVC reported the repository's `/W3` being
overridden by the test's `/W4`; neither affected this target. No neighboring
AOTIR targets were built in this dedicated cache: this slice adds new source
files and its own test target without changing their compiled inputs.

The standalone constant-i64 emitter slice is accepted. Current source
`LowerAot` still declares `UNKNOWN`, and the VM artifact gate still returns
`ARTIFACT_UNAVAILABLE`; no production source path reaches the emitter or loads
its text as an artifact. The 07.02 milestone remains open for control/call,
exception/GC/ownership, loader, four-backend differential, and artifact
registration requirements.

## Direct branch extension: RED and fixture

The next independent 07.02 slice extends the same standalone emitter to one
exact two-block control-flow shape: an entry block with only unconditional
`BRANCH`, followed by a block with `CONSTANT → RETURN`. The entry successor and
branch target name the second block; its sole predecessor names the entry
block. This has no conditional choice, loop, call, side effect, or hidden state.
The fixture still supplies the hand-made opaque token `17u` and an explicit
trusted `NOARGS_I64` declaration. The gate does not authenticate that producer
or prove general CFG dominance; the accepted two-block shape fixes the only
path to the definition before the return. Production `LowerAot` still declares
`UNKNOWN`, and the artifact-required adapter remains unavailable.

The test was written before changing the emitter. In
`D:\tmp\zr_vm\ssa-aot-branch-red-gcc`, direct GCC `-std=c11 -Wall -Wextra
-Werror` compilation of the fixture, three scalar emitter sources, `aot_ir.c`
and `exec_ir_state_map_storage.c` succeeded. Running the executable then
reached the new two-block positive assertion at
`tests/parser/test_ssa_aot_scalar_text.c:146` and exited 1: the unextended C
emitter returned a failure instead of `ZR_AOT_IR_OK`. The preceding
`ZrCore_AotIr_ValidateModule` and `ZrCore_AotIr_RequireExecutableAbi`
assertions passed. Thus the RED is the missing emission shape, not an invalid
fixture or failed callable ABI gate.

The new fixture also checks both backends' text and output length for `42` and
`INT64_MIN`, explicit `UNKNOWN` rejection, too-small output buffers, malformed
or extra edges, a missing terminator, effects, and an extra NOP. On each
rejection with a provided buffer and length pointer, it asserts output
clearing. It writes `branch_42.c`, `branch_min.c`, `branch_42.ll`, and
`branch_min.ll` only when given a fixture directory, alongside the existing
`runner.c`. The latter calls `zr_aot_scalar_fn_1()` and checks the returned
`int64_t`; the generated functions must be compiled and invoked for acceptance.

## Direct branch extension: GCC GREEN and generated calls

After adding the exact two-block shape check and C `goto` / LLVM `br label`
rendering, direct GCC compilation and fixture execution under
`D:\tmp\zr_vm\ssa-aot-branch-green-gcc` both exited 0. The command used the
same six direct source files and four include directories as the prior scalar
batch, with `-std=c11 -Wall -Wextra -Werror`:

```text
wsl.exe --exec bash -lc 'cd /mnt/e/Git/zr_vm && gcc -std=c11 -Wall -Wextra -Werror -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include tests/parser/test_ssa_aot_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c zr_vm_core/src/zr_vm_core/aot_ir.c zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c -o /mnt/d/tmp/zr_vm/ssa-aot-branch-green-gcc/test_ssa_aot_scalar_text && /mnt/d/tmp/zr_vm/ssa-aot-branch-green-gcc/test_ssa_aot_scalar_text /mnt/d/tmp/zr_vm/ssa-aot-branch-green-gcc'
```

This GREEN includes all two-block negative assertions and the original
one-block fixture. The generated functions were then compiled and invoked,
using the fixture's `runner.c` to compare the actual returned value:

```text
cd /mnt/d/tmp/zr_vm/ssa-aot-branch-green-gcc
gcc -std=c11 -Wall -Wextra -Werror branch_42.c runner.c -DEXPECTED=42 -o run_c_42 && ./run_c_42
gcc -std=c11 -Wall -Wextra -Werror branch_min.c runner.c -DEXPECTED=INT64_MIN -o run_c_min && ./run_c_min
clang -Wno-override-module -x ir branch_42.ll -x c runner.c -DEXPECTED=42 -o run_llvm_42 && ./run_llvm_42
clang -Wno-override-module -x ir branch_min.ll -x c runner.c -DEXPECTED=INT64_MIN -o run_llvm_min && ./run_llvm_min
```

All four generated-code builds and calls exited 0. This is standalone native
function execution, subject to the trusted ABI declaration and host-target
limits above; it does not establish a production source or artifact path. The
root task independently reran the strict GCC fixture executable and all four
generated call executables; each exited 0.

## Direct branch extension: Clang, MSVC, and formal CTest

The same six-source fixture was directly compiled and run in
`D:\tmp\zr_vm\ssa-aot-branch-clang` with Clang
`-std=c11 -Wall -Wextra -Werror`, and in
`D:\tmp\zr_vm\ssa-aot-branch-msvc` with MSVC x64
`/std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS`. Both compilation
and fixture execution exited 0. These runs exercised the new two-block
positive and negative assertions plus the existing single-block regression.
The Clang-generated `branch_42.ll` and `branch_min.ll` were compiled with
`runner.c` and called, returning the expected 42 and `INT64_MIN`; both calls
exited 0. The MSVC-generated `branch_42.c` and `branch_min.c` were likewise
compiled with `runner.c`, called, and exited 0.

```text
wsl.exe --exec bash -lc 'cd /mnt/e/Git/zr_vm && clang -std=c11 -Wall -Wextra -Werror -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include tests/parser/test_ssa_aot_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c zr_vm_core/src/zr_vm_core/aot_ir.c zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c -o /mnt/d/tmp/zr_vm/ssa-aot-branch-clang/test_ssa_aot_scalar_text && /mnt/d/tmp/zr_vm/ssa-aot-branch-clang/test_ssa_aot_scalar_text /mnt/d/tmp/zr_vm/ssa-aot-branch-clang'
cd /mnt/d/tmp/zr_vm/ssa-aot-branch-clang
clang -Wno-override-module -x ir branch_42.ll -x c runner.c -DEXPECTED=42 -o run_llvm_42 && ./run_llvm_42
clang -Wno-override-module -x ir branch_min.ll -x c runner.c -DEXPECTED=INT64_MIN -o run_llvm_min && ./run_llvm_min
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS /Izr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot /Izr_vm_parser\include /Izr_vm_core\include /Izr_vm_common\include tests\parser\test_ssa_aot_scalar_text.c zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_ir_scalar_text.c zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_ir_scalar_c.c zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_ir_scalar_llvm.c zr_vm_core\src\zr_vm_core\aot_ir.c zr_vm_core\src\zr_vm_core\exec_ir\exec_ir_state_map_storage.c /FoD:\tmp\zr_vm\ssa-aot-branch-msvc\ /FeD:\tmp\zr_vm\ssa-aot-branch-msvc\test_ssa_aot_scalar_text.exe
D:\tmp\zr_vm\ssa-aot-branch-msvc\test_ssa_aot_scalar_text.exe D:\tmp\zr_vm\ssa-aot-branch-msvc
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS /DEXPECTED=42 D:\tmp\zr_vm\ssa-aot-branch-msvc\branch_42.c D:\tmp\zr_vm\ssa-aot-branch-msvc\runner.c /FoD:\tmp\zr_vm\ssa-aot-branch-msvc\ /FeD:\tmp\zr_vm\ssa-aot-branch-msvc\run_c_42.exe
D:\tmp\zr_vm\ssa-aot-branch-msvc\run_c_42.exe
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /D_CRT_SECURE_NO_WARNINGS /DEXPECTED=INT64_MIN D:\tmp\zr_vm\ssa-aot-branch-msvc\branch_min.c D:\tmp\zr_vm\ssa-aot-branch-msvc\runner.c /FoD:\tmp\zr_vm\ssa-aot-branch-msvc\ /FeD:\tmp\zr_vm\ssa-aot-branch-msvc\run_c_min.exe
D:\tmp\zr_vm\ssa-aot-branch-msvc\run_c_min.exe
```

The existing `zr_vm_ssa_aot_scalar_text_test` CMake target was reused without
editing shared CMake files. Windows native CMake configured a dedicated Ninja
Debug cache at `D:\tmp\zr_vm\ssa-aot-branch-cmake-msvc`; its focused target
build completed all eight edges. Direct CTest then passed `ssa_aot_scalar_text`
1/1 (test #245). The root task independently reran this final focused MSVC
CTest and observed 1/1 pass with exit 0:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake -S E:\Git\zr_vm -B D:\tmp\zr_vm\ssa-aot-branch-cmake-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake --build D:\tmp\zr_vm\ssa-aot-branch-cmake-msvc --target zr_vm_ssa_aot_scalar_text_test --parallel 2
ctest --test-dir D:\tmp\zr_vm\ssa-aot-branch-cmake-msvc -C Debug -R '^ssa_aot_scalar_text$' --output-on-failure --no-tests=error
```

CMake emitted path-length warnings for unrelated numeric-loop and LSP
targets, and the focused build reported the repository `/W3` being overridden
by this target's `/W4`. Neither affected this target. The branch extension is
accepted as a standalone text-emission slice. Production source lowering and
artifact registration remain unavailable as described above; 07.02 remains
open for conditional control, calls, exceptional effects, runtime integration,
and the broader differential requirements.
