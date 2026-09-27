# SSA 07.01: explicit shared AOTIR callable ABI

## Scope and trust boundary

Schema version 7 gives each `SZrAotIrFunction` an explicit callable ABI:
`UNKNOWN` (zero/default) or `NOARGS_I64` with a nonzero return type token. The
owned projection, descriptor and backend adapter pass this declaration through
without deriving it from opaque `signatureHash`, `typeToken`, constant bits,
legacy ExecBC, or frame `parameterPrefixBytes`. AOTIR has no parameter-count
field. `NOARGS_I64` therefore asserts a fact supplied by a future trusted
callable-signature producer; the present source `ZrParser_ExecIr_LowerAot`
always leaves it `UNKNOWN`. The new query checks the declaration and the
RETURN value/type shape, but cannot authenticate an absent producer or promise
that C/LLVM can emit an artifact. Both shared artifact emitters still reject
`requireArtifact`. This qualification checks ABI/SSA shape, including one
typed definition across the function before each RETURN. It does not prove
full CFG dominance or emitter support.

## Baseline and test inventory

`tests/parser/test_ssa_aot_callable_abi.c` was written first. A WSL GCC
`-std=c11 -fsyntax-only` compilation failed on the absent callable ABI fields,
enum, core query and backend adapter query. This is the feature's RED check.
The existing repository matrix has unrelated baseline failures documented in
`.codex/skills/zr-vm-dev/SKILL.md`; this acceptance uses the focused ABI and
neighboring AOTIR/adapter targets.

The fixture asserts: an old zero-initialized/descriptor-only ABI still
validates but fails executable ABI qualification with `UNSUPPORTED`; explicit
`NOARGS_I64` with matching `CONSTANT` and `RETURN` value/type passes; changing
the ABI changes the canonical hash; missing or wrong return token, wrong RETURN
arity, missing RETURN, missing or duplicate value definition (including a
later definition in another block), mismatched
producer type, invalid ABI enum, an `UNKNOWN` declaration carrying a return
token, invalid or missing function IDs, and a null output fail closed. The
owned projection copies explicit and unknown declarations into the descriptor,
and the adapter returns the same qualification and diagnostic category.

## Tooling evidence

The CMake target is `zr_vm_ssa_aot_callable_abi_test`, CTest name
`ssa_aot_callable_abi`. It uses the same direct core, projection, lowering and
adapter source list as the neighboring `ssa_aot_projection_descriptor` target,
with the parser/core/common/AOT-adapter include directories. The CMake helper
`zr_vm_apply_common_test_settings` supplies repository test settings. Direct
builds used WSL GCC 11.4, WSL Clang 14.0, and MSVC x64 19.44.35228.0. The
Windows CMake/CTest version was 3.23.0-rc2. Exact direct Linux commands,
run from `E:\Git\zr_vm`, were:

```text
wsl.exe --exec bash -lc "cd /mnt/e/Git/zr_vm && gcc -std=c11 -Wall -Wextra -Werror -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot tests/parser/test_ssa_aot_callable_abi.c zr_vm_core/src/zr_vm_core/aot_ir.c zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c -o /mnt/d/tmp/zr_vm/ssa-aot-abi-gcc/test_ssa_aot_callable_abi && /mnt/d/tmp/zr_vm/ssa-aot-abi-gcc/test_ssa_aot_callable_abi"
wsl.exe --exec bash -lc "cd /mnt/e/Git/zr_vm && clang -std=c11 -Wall -Wextra -Werror -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot tests/parser/test_ssa_aot_callable_abi.c zr_vm_core/src/zr_vm_core/aot_ir.c zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c -o /mnt/d/tmp/zr_vm/ssa-aot-abi-clang/test_ssa_aot_callable_abi && /mnt/d/tmp/zr_vm/ssa-aot-abi-clang/test_ssa_aot_callable_abi"
```

MSVC used the repository's `using-vsdevcmd` wrapper to supply x64 tools:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cl /nologo /std:c11 /utf-8 /W4 /WX /MDd /Izr_vm_parser\include /Izr_vm_core\include /Izr_vm_common\include /Izr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot tests\parser\test_ssa_aot_callable_abi.c zr_vm_core\src\zr_vm_core\aot_ir.c zr_vm_core\src\zr_vm_core\exec_ir\exec_ir_state_map_storage.c zr_vm_parser\src\zr_vm_parser\exec_ir\exec_ir_aot_projection_descriptor.c zr_vm_parser\src\zr_vm_parser\exec_ir\exec_ir_aot_lowering.c zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_ir_adapter.c /FoD:\tmp\zr_vm\ssa-aot-abi-msvc\ /FeD:\tmp\zr_vm\ssa-aot-abi-msvc\test_ssa_aot_callable_abi.exe
D:\tmp\zr_vm\ssa-aot-abi-msvc\test_ssa_aot_callable_abi.exe
```

MSVC `/utf-8` is required because the host's ACP 936 otherwise raises C4819
for existing UTF-8 repository headers under `/WX`. The adjacent
`tests/parser/test_ssa_aotir_state_map.c::test_aotir_owns_state_map` also
asserts that successful `LowerAotWithConstantsAndLayouts` still produces
`UNKNOWN` and a zero return token. Its strict WSL GCC syntax check passed:

```text
wsl.exe --exec bash -lc "cd /mnt/e/Git/zr_vm && gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Izr_vm_parser/include -Izr_vm_core/include -Izr_vm_common/include -Itests/third_party/zr_unity/Unity/src tests/parser/test_ssa_aotir_state_map.c"
```

Formal GCC CMake configuration was started in
`D:\tmp\zr_vm\ssa-aot-abi-gcc-cmake`. It stayed in DrvFS I/O wait for over
13 minutes after the compiler/pthread probes while other D-drive builds were
active, so the configuration process was interrupted. It produced no test
result. After the shared Windows cache was released, native CMake configured
and built the ABI target and neighboring tests in `D:\tmp\zr_vm\ssa-msvc-debug`:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake -S E:\Git\zr_vm -B D:\tmp\zr_vm\ssa-msvc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake --build D:\tmp\zr_vm\ssa-msvc-debug --target zr_vm_ssa_aot_callable_abi_test zr_vm_ssa_aotir_contract_test zr_vm_ssa_aot_projection_descriptor_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_c_llvm_lowering_test zr_vm_ssa_aot_backend_adapters_test --parallel 8
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1 cmake --build D:\tmp\zr_vm\ssa-msvc-debug --target zr_vm_ssa_aot_callable_abi_test --parallel 8
ctest --test-dir D:\tmp\zr_vm\ssa-msvc-debug -C Debug -R "^(ssa_aot_callable_abi|ssa_aotir_contract|ssa_aot_projection_descriptor|ssa_oracle_resume|ssa_c_llvm_lowering|aot_backend_adapters)$" --output-on-failure --no-tests=error
```

## Results and acceptance decision

The cross-block later-definition case initially failed in direct GCC: the
query saw only definitions before the RETURN. The query now scans the whole
function for uniqueness and separately checks that its one definition precedes
the RETURN. After this fix, direct GCC, Clang, and MSVC builds and test
executions all exited 0. Native CMake configuration, the initial six-target
build, the focused rebuild and the final six-target rebuild all exited 0.
The final CTest run passed 6/6: `ssa_oracle_resume`, `ssa_aotir_contract`,
`ssa_aot_projection_descriptor`, `ssa_aot_callable_abi`,
`ssa_c_llvm_lowering`, and `aot_backend_adapters`. The CMake configure reported
three path-length warnings for unrelated numeric-loop/LSP test targets; none
of the six selected targets failed. The explicit callable ABI contract is
accepted as the 07.01 prerequisite. It does not qualify the current source
`LowerAot` path for artifact emission because that path declares `UNKNOWN`.

## File-size boundary

`aot_ir.c` is 1,378 lines after this narrow contract change and was already
over the 1,100-line threshold. It remains one AOTIR validation/hash module;
splitting it here would require a new internal diagnostic boundary and updates
to every direct-source test build of core AOTIR, outside this ABI-only slice.
The smallest coherent later split is callable-ABI validation/qualification in
`aot_ir_callable_abi.c` with a shared internal diagnostic helper, done together
with its build registration. No emitter or other operation family was added to
this large file.

## Remaining work

A trusted resolved callable-signature producer must set `NOARGS_I64` from
source type information before any source path can claim the ABI. C/LLVM
artifact generation for the constant-to-return slice remains separate 07.02
work; this acceptance is solely for its 07.01 ABI prerequisite.
