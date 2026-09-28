# Close proxy opcode and AOT lowering

The lower-layer registration API is exposed to ExecBC as appended opcode 245,
`MARK_CLOSE_PROXY E,A1`. E names the fresh high proxy slot and A1 the existing
dense source. The interpreter, frame-slot scan, generated AOT C and LLVM, and
AOT runtime helper use the same operands. The helper registers the high physical
VALUE slot and keeps the dense source address for mirror cleanup.

The legacy `01ZR` `.zro` writer emits patch 44 for all new files. This is a
global writer gate: even opcode-free files written now require a patch 44
reader. Old opcode numbers through 244 remain fixed. New readers accept patch
43 and reject a future patch 45 before loading instructions. ZRAF schema 6,
EIS2, and AOT ABI 17 remain unchanged because they do not serialize ExecBC
opcode numbers. Generated AOT using the new helper requires a new runtime.

## Focused verification

- Interpreter: 2/2 Unity cases for source/proxy operands, close order, and
  frame-slot scanning.
- AOT helper: 1/1 Unity case with distinct dense and physical source cells and
  a higher physical proxy registration. The callback observes both source
  cells cleared, and the older marker does not close again.
- AOT emitter: 1/1 Unix smoke generates C with
  `MarkCloseProxy(state, &frame, 2, 1)`, LLVM with `i32 2, i32 1`, and links
  the generated C shared library with `--no-undefined`.
- Legacy artifact: 2/2 Unity cases fix IDs 244/245/246, read patch 43, and
  reject 45. A retained patch 43 reader linked against its old core library
  rejected a complete 519-byte patch 44 `.zro` with
  `compiledReaderPatch=43 filePatch=44 status=2 accepted=0`.
- Core proxy 15/15, native closure 3/3, and inline type layout 40/40 remain
  green.

The emitter smoke compiles and links the generated C, while the separate AOT
helper case executes cleanup behavior. Entry execution with a compiled `using`
source belongs to the parser integration slice.

Two adjacent suites remain red outside this opcode path. The AOT scope source
contract still expects the old `slotPointer` spelling, although the committed
runtime already uses `registrationPointer`. The exception suite passes 6/8;
its two finally source cases stop during parsing with `Failed to enter semantic
finally cleanup`, before any proxy instruction is emitted. Neither failure is
counted as opcode validation.
