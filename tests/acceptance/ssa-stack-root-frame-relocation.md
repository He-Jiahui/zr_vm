# AOT Root Frame Bases Across VM Stack Relocation

## Scope

This acceptance slice covers active AOT root-frame bases while the VM stack
allocation moves. The root-frame ABI layout remains unchanged. The linked frame
node must live in host-stable storage until `ZrCore_Gc_AotRootFramePop`; its
`frameBase` may point into the VM stack allocation and is relocated with that
allocation.

Implementation paths:

- `zr_vm_core/src/zr_vm_core/stack.c`
- `zr_vm_core/src/zr_vm_core/gc/gc.c`
- `zr_vm_core/include/zr_vm_core/gc.h`
- `zr_vm_core/include/zr_vm_core/state.h`

Focused test paths:

- `tests/core/test_execution_add_stack_relocation.c`
- `tests/core/test_execution_add_stack_relocation_aot_roots.inc`

## Test Design

`test_stack_relocation_preserves_active_aot_frame_byte_offset_root` creates a
young object reachable only from a VM stack slot beyond the ordinary `stackTop`
scan range, registers that slot through a real `FRAME_BYTE_OFFSET` AOT root map,
registers a second VM-stack frame with another young object, and registers a
third young object through `LOCAL_ADDRESS`. Its allocator forces stack growth
to move the allocation. The fixture injects one allocation failure, verifies
the active root chain and original bases are restored, retries the growth, and
checks both VM `frameBase` values were rebased, the C-local base was preserved,
and the nonnull VM-frame `previous` link was restored.

The test then performs a generational minor collection and verifies that the
two `FRAME_BYTE_OFFSET` objects and the `LOCAL_ADDRESS` object remain registered
in the survivor region. These objects are `ZR_RAW_OBJECT_TYPE_OBJECT`; the
current collector reassigns this type in place during minor collection, so
pointer rewriting is not expected from this fixture. The stack slots sit past
ordinary `stackTop` scanning, making the AOT root maps the only roots for the
first two objects.

`test_aot_root_frame_push_rejects_node_inside_vm_stack_allocation` places the
candidate node at `stackTail`, within the allocator-reserved extra slots. It
checks Push rejects the node without changing the existing chain or depth.
This protects the relocation walk itself from following a chain link that
would become dangling when the allocation moves.

`test_aot_root_frame_push_rejects_duplicate_active_node` verifies that Push
rejects an already-linked node without changing the node, active-chain top, or
depth. This prevents a self-cycle in the chain.

The injected allocation failure is a fixture-specific precondition: the
`test_moving_allocator` returns `NULL` while retaining the original allocation.
This exercises stack-state rollback with the old block still valid. It does not
establish an allocator-wide failure-preservation contract.

The focused executable target is `zr_vm_execution_add_stack_relocation_test`,
a direct-only target in `tests/CMakeLists.txt`; no CTest name is registered for
it. Do not treat a zero-match CTest invocation as evidence.

## RED Evidence Correction

The earlier test-only focused MSVC run in
`D:/tmp/zr_vm/ssa-artifact-v6-msvc` reported `21 Tests 1 Failures 0 Ignored`,
exit code 1, at the original assertion:

> `FRAME_BYTE_OFFSET root must be rewritten after stack relocation and minor GC`

Source review showed this was an invalid RED expectation, not proof of the
stack-root-base defect: minor evacuation deliberately reassigns ordinary
objects in place and does not set a forwarding address for them. The run
therefore cannot establish a root rewrite failure. The current test asserts the
actual stack-base relocation and survivor/root reachability contracts. The isolated old-stack-source run below now proves the direct base assertion
fails with the old implementation.

## Implementation Notes

Stack relocation encodes each active `frameBase` that points inside the full
old allocation as a byte offset. A low-bit tag on `previous` records which
nodes need restoration, guarded by a compile-time alignment assertion on
`SZrAotGcRootFrame`. `LOCAL_ADDRESS` roots keep their original host address.
GC remains stopped from before encoding until all call-info and root-frame
pointers are absolute again. When a failed allocation retains the old block,
all offsets and chain links are restored against that valid old base, and only
then is the prior GC stop state restored. The allocator-wide failure contract
remains an existing open issue in `memory.h` and `stack.c`.

`ZrCore_Gc_AotRootFramePush` rejects a frame node overlapping the logical VM
stack plus allocator extra slots using integer address-difference checks. This
keeps the chain node stable independently of the relocatable root base.
Push also rejects a node already present in the active chain, preserving the
acyclic chain and depth.

## Validation Status

The final MSVC focused validation passed after the fixture changed its old
stack address snapshot to `uintptr_t`, avoiding comparison of a pointer value
after the allocator released that stack block. From the configured x64
VsDevCmd environment, `TEMP` and `TMP` were set to
`D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp`, then these commands ran:

```text
cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_execution_add_stack_relocation_test -j2
D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_execution_add_stack_relocation_test.exe
```

The incremental focused build completed 2/2 Ninja steps with exit code 0
(14.75 seconds). The executable reported 23 tests, 0 failures, 0 ignored, exit
code 0 (0.99 seconds). The preceding four-target MSVC build completed 708/708
steps with exit code 0. This target is direct-only and has no CTest selector.

The implementation and MSVC focused behavior are green. GCC and Clang focused
build/direct runs remain pending. The isolated pre-fix comparison below
confirmed the genuine stack-base migration failure. The earlier pointer-rewrite
failure remains documented only as a corrected false RED.

## Isolated Pre-Fix Stack Comparison

Root built the old stack translation unit from Git blob
`ffb072f29909906e23fe2b8e44a1d61e1826c245` (16,987 bytes), extracted at HEAD
1bd8ef663e960ac61ac6c901a1f0de86fea9dfce. A single direct old-stack object
was linked before the current core archive, using the fresh focused test and
harness objects. No shared source or reusable archive was changed or copied.
The script verified the blob and size. The link requested /VERBOSE:LIB, but
the successful CMake linker wrapper suppressed its output (link.log was
empty); the map confirmed the fixed archive stack.c.obj was not extracted.
The map names
stack_before_fix.obj as provider for ZrCore_Stack_GrowTo and ZrCore_Stack_Grow.

The x64 VsDevCmd run completed in 26.51 seconds. Compile and link exited 0;
the isolated executable exited 1 with exactly 23 tests, 1 failure, 0 ignored.
Its only failure was:

```text
test_stack_relocation_preserves_active_aot_frame_byte_offset_root:FAIL:
successful stack growth must relocate both VM-stack root bases and preserve the C-local base
23 Tests 1 Failures 0 Ignored
```

The other 22 cases passed. The baseline fixture skips collection when rebasing
fails, Pops the host-stable frames and destroys the state before reporting the
assertion, so this comparison does not let GC read a freed old frame base.

TMP, TEMP, and TMPDIR were set to the scratch `tmp` child for compiler,
linker, and test subprocesses. The effective commands were:

```text
Compile pre-fix stack.c:
"E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe" /nologo -DUTF8PROC_STATIC -DZR_DEBUG -DZR_LIBRARY_TYPE_STATIC -Dzr_vm_core -IE:\Git\zr_vm\zr_vm_core\include -IE:\Git\zr_vm\zr_vm_core\src\zr_vm_core -IE:\Git\zr_vm\zr_vm_common\include -IE:\Git\zr_vm\zr_vm_library\include -IE:\Git\zr_vm\zr_vm_core\third_party\zr_xx_hash -IE:\Git\zr_vm\zr_vm_core\third_party\zr_utf8proc /DWIN32 /D_WINDOWS /W3 /MDd /Zi /Ob0 /Od /RTC1 /W4 /wd4819 /wd4103 /experimental:c11atomics /utf-8 -DZR_PLATFORM_WIN_USE_MSVC -D_CRT_SECURE_NO_WARNINGS -DZR_PLATFORM_WIN -DZR_VM_COMPILER_VERSION=\"MSVC-19.44.35228.0\" -DZR_CURRENT_MODULE=\"zr_vm_core\" -std:c11 /showIncludes /FoD:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\stack_before_fix.obj /FdD:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\stack_before_fix.pdb /FS -c D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\stack_before_fix.c

Link baseline executable:
D:\Tools\development\cmake\bin\cmake.exe -E vs_link_exe --intdir=D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\int "--rc=D:\Windows Kits\10\bin\10.0.26100.0\x64\rc.exe" "--mt=D:\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" --manifests -- "E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" /nologo D:\tmp\zr_vm\ssa-artifact-v6-msvc\tests\CMakeFiles\zr_vm_execution_add_stack_relocation_test.dir\core\test_execution_add_stack_relocation.c.obj D:\tmp\zr_vm\ssa-artifact-v6-msvc\tests\CMakeFiles\zr_vm_execution_add_stack_relocation_test.dir\harness\runtime_support.c.obj D:\tmp\zr_vm\ssa-artifact-v6-msvc\tests\CMakeFiles\zr_vm_execution_add_stack_relocation_test.dir\harness\path_support.c.obj D:\tmp\zr_vm\ssa-artifact-v6-msvc\tests\CMakeFiles\zr_vm_execution_add_stack_relocation_test.dir\harness\unity_crash_guard.c.obj D:\tmp\zr_vm\ssa-artifact-v6-msvc\tests\CMakeFiles\zr_vm_execution_add_stack_relocation_test.dir\harness\reference_support.c.obj D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\stack_before_fix.obj /out:D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\zr_vm_execution_add_stack_relocation_oldstack.exe /implib:D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\zr_vm_execution_add_stack_relocation_oldstack.lib /pdb:D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\zr_vm_execution_add_stack_relocation_oldstack.pdb /MAP:D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\zr_vm_execution_add_stack_relocation_oldstack.map /VERBOSE:LIB /version:0.0 /machine:x64 /debug /INCREMENTAL /subsystem:console D:\tmp\zr_vm\ssa-artifact-v6-msvc\lib\zr_unity.lib D:\tmp\zr_vm\ssa-artifact-v6-msvc\lib\zr_vm_core.lib D:\tmp\zr_vm\ssa-artifact-v6-msvc\lib\zr_xx_hash.lib D:\tmp\zr_vm\ssa-artifact-v6-msvc\lib\zr_utf8proc.lib kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib

Run old-stack baseline (expected one RED):
D:\tmp\zr_vm\ssa-stack-root-reloc-oldstack\zr_vm_execution_add_stack_relocation_oldstack.exe

```

The baseline harness returned 0 only after checking that exact expected RED
and the old-object symbol providers. After recording the evidence, root deleted all 17 scratch files (14,547,555
bytes), then the two empty child directories and the scratch root, using
individually checked paths and nonrecursive directory deletion.
Directory.Exists returned false. These commands, hashes, results, and map
evidence remain in this acceptance record.
