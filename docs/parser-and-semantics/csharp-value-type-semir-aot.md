---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_locals.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bitwise.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_logical.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_conversion.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_locals.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bitwise.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_logical.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_conversion.c
plan_sources:
  - user: 2026-06-03 C#-style struct value-type stack layout and IL2CPP-style lowering goal
  - .codex/plans/Struct 变长栈与按布局传参计划.md
  - .codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md
tests:
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_aot_c_source_contracts.c
  - tests/parser/test_typed_numeric_conversion.c
  - tests/parser/test_value_type_runtime.c
  - tests/acceptance/2026-06-04-typed-numeric-conversion.md
  - tests/acceptance/2026-06-05-aot-value-semir-unsupported-fields.md
  - tests/acceptance/2026-06-05-aot-value-semir-value-slot-fields.md
  - tests/acceptance/2026-06-05-aot-value-semir-nested-inline-fields.md
  - tests/acceptance/2026-06-05-aot-value-semir-field-aware-copy.md
  - tests/acceptance/2026-06-05-aot-value-semir-frame-cleanup.md
doc_type: semantic-contract
---

# C#-Style Value-Type SemIR

This contract records the first typed value-place layer used to move `struct` toward C# value-type semantics and IL2CPP-style lowering.

## Scope

`struct` locals that have `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT` frame layout are treated as inline value payloads for SemIR metadata. The interpreter still executes the existing bytecode path, but SemIR now preserves the value-type intent so AOT lowering does not have to infer it from untyped stack mutations.

The initial opcode family is:

- `VALUE_ADDR`: address/place for a frame value slot.
- `FIELD_ADDR`: address/place for a field inside an inline struct receiver.
- `LOAD_VALUE`: typed load from a value place into a destination slot.
- `STORE_VALUE`: typed store into a value place.
- `INIT_VALUE`: reserved for layout-aware zero/default construction.
- `COPY_VALUE`: layout-aware by-value copy between inline struct slots.
- `CALL_TYPED`: typed call metadata for known call sites whose return destination has proven inline struct layout.
- `RETURN_TYPED`: typed return metadata for single-result inline struct return sources.

## Current Lowering

`compiler_semir.c` maps known inline struct bytecode patterns into metadata rows:

- `GET_MEMBER_SLOT` on an inline struct receiver emits `FIELD_ADDR` followed by `LOAD_VALUE`.
- `SET_MEMBER_SLOT` on an inline struct receiver emits `FIELD_ADDR` followed by `STORE_VALUE`.
- `SET_STACK` between inline struct slots emits `COPY_VALUE`; if the source was staged through the immediately preceding `GET_STACK`, the original inline struct source slot is recorded.
- `FUNCTION_CALL` / `KNOWN_VM_CALL` with a result slot that has proven inline struct layout emits `CALL_TYPED`. The destination slot is the return payload slot, `operand0` is the callee slot, and `operand1` is the bytecode argument count.
- Single-result `FUNCTION_RETURN` from a proven inline struct source emits `RETURN_TYPED`. The destination slot and `operand0` both identify the source return payload slot.

`FIELD_ADDR.operand0` is the receiver slot. `FIELD_ADDR.operand1`, `LOAD_VALUE.operand1`, and `STORE_VALUE.operand1` carry the member/cache index used by the source bytecode. `COPY_VALUE.operand0` is the source inline struct slot. Call-result temporaries can recover `typeTableIndex` through matching inline `typeLayoutId` frame-slot layout when there is no direct typed-local binding for the temporary slot.

## AOT Boundary

The AOT ExecIR name table recognizes the new opcodes. These value-place opcodes currently add no runtime contract bits because they are intended to lower into direct frame/field/copy expressions once AotExecIR consumes layout metadata.

The archived AotExecIR frame layout now mirrors inline frame byte metadata:

- `SZrAotExecIrFrameLayout.frameByteSize`
- `SZrAotExecIrFrameLayout.frameByteAlign`
- `SZrAotExecIrFrameLayout.slotLayoutCount`
- `SZrAotExecIrFrameLayout.slotLayouts`

Each `SZrAotExecIrFrameSlotLayout` copies stack slot, byte offset, byte size, byte align, type layout id, slot kind, and parameter marker from `SZrFunctionFrameSlotLayout`. This gives future C/C++ lowering a direct route from `FIELD_ADDR` / `LOAD_VALUE` / `STORE_VALUE` / `COPY_VALUE` to frame byte spans without reverse-engineering layout from flat stack counts.

The archived `aot_c` writer now has a first value-place lowering surface in `backend_aot_c_value_semir.*`. `backend_aot_write_c_value_semir_for_function()` receives `SZrState`, walks each function's AotExecIR SemIR rows, finds frame-slot byte layout with `backend_aot_c_find_frame_slot_layout()`, resolves struct field metadata with `ZrCore_Function_ResolvePrototypeFrameFieldLayout()`, and emits value-place lowering annotations for:

- `FIELD_ADDR` -> `zr_aot_value_field_addr`
- `LOAD_VALUE` -> `zr_aot_value_load`
- `STORE_VALUE` -> `zr_aot_value_store`
- `COPY_VALUE` -> `zr_aot_value_copy`
- `CALL_TYPED` -> `zr_aot_value_call_typed`
- `RETURN_TYPED` -> `zr_aot_value_return_typed`

For equal-sized copy spans it records the current generated-frame ABI shape: `memmove((TZrByte *)frame.slotBase + dstOffset, (const TZrByte *)frame.slotBase + srcOffset, size)`. Field address, field load, and field store annotations now record `base byte offset + field byte offset` expressions after resolving the member/cache index to `SZrFunctionFrameFieldLayout`. Typed call/return annotations record the involved frame layout and type id, and the first POD executable path now consumes the same layout facts.

The first executable M5 steps are now inline `COPY_VALUE` lowering and primitive POD field load/store lowering. `backend_aot_try_write_c_value_semir_for_exec_instruction()` checks the current exec instruction for a matching value SemIR row:

- `COPY_VALUE` on matching inline struct `SET_STACK` / `GET_STACK` sites now emits a generated type-layout gate and skips the old generic stack-copy helper. The generated C resolves `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, typeLayoutId, state)`, uses `ZrCore_TypeLayout_CanRawCopy()` to keep POD struct copies on `memmove`, and uses `ZrCore_TypeLayout_CopyInline()` for non-POD field-aware copies such as structs with embedded `SZrTypeValue` fields.
- `LOAD_VALUE` on matching inline struct `GET_MEMBER_SLOT` sites resolves the field layout, reads the primitive field bytes from `(const TZrByte *)frame.slotBase + baseOffset + fieldOffset`, and writes the scalar result into the destination `SZrTypeValue`.
- `STORE_VALUE` on matching inline struct `SET_MEMBER_SLOT` sites resolves the field layout, reads the scalar source `SZrTypeValue`, converts it to the field primitive type, and copies the primitive bytes into `(TZrByte *)frame.slotBase + baseOffset + fieldOffset`.
- `LOAD_VALUE` / `STORE_VALUE` on matching inline struct fields that are stored as embedded `SZrTypeValue` cells now emit `zr_aot_value_exec_field_value_slot_load` / `zr_aot_value_exec_field_value_slot_store` and use `ZrCore_Value_Copy` directly between the frame byte field and the scalar destination/source slot. This is the first AOT managed/reference field access slice; copy/drop ownership for whole non-POD structs still belongs to later layout-aware copy/drop lowering.
- `LOAD_VALUE` / `STORE_VALUE` on matching nested inline struct fields now emit `zr_aot_value_exec_field_inline_struct_load` / `zr_aot_value_exec_field_inline_struct_store` when the field `typeLayoutId` and byte size match the inline destination/source slot. The generated C uses `memmove` between `frame.slotBase + baseOffset + fieldOffset` and the inline struct slot payload, preserving overlap safety for nested value copies.
- When `LOAD_VALUE` / `STORE_VALUE` resolves an inline struct field but the field is still outside the executable subset, for example a mismatched nested inline struct transfer, the generated C emits `zr_aot_value_unsupported_field_load` / `zr_aot_value_unsupported_field_store`, calls `ZrCore_Debug_RunError(state, "unsupported AOT value SemIR field ...")`, and exits through `ZR_AOT_C_FAIL()`. Unresolved or dynamic field access still falls through to the declared runtime member contract; proven value-type fields do not silently fall back to `GetMemberSlot` / `SetMemberSlot`.
- `CALL_TYPED` on matching static/direct POD struct return sites resolves the caller destination type layout with `ZrCore_Function_ResolvePrototypeFrameTypeLayout`, checks the generated-frame destination byte size, prepares the direct call, invokes the callee thunk, and finishes the call through the existing post-call path.
- `RETURN_TYPED` on matching single inline struct return sites resolves the callee source type layout with `ZrCore_Function_ResolvePrototypeFrameTypeLayout`, checks the inline source byte size, publishes the source slot as `state->stackTop`, and lets `FinishDirectCall`/post-call route the payload through `ZrCore_Function_TryCopyInlineFrameReturnValue`.

Generated AOT C functions now also have a first value-frame cleanup boundary. The generated guard macro uses `ZR_AOT_C_RETURN(expr)` instead of emitting direct `return` statements from normal return, tail return, unsupported-dispatch, and failure paths. Each generated function records `zr_aot_frame_started`, a `zr_aot_return_value`, and `zr_aot_skip_drop_slot`, then exits through `zr_aot_function_exit`. `backend_aot_c_frame_cleanup.*` emits reverse frame-layout cleanup for inline struct slots: it resolves each slot's `SZrTypeLayout` from `frame.function`, skips the typed inline return source slot when that source must remain available for `FinishDirectCall`, and calls `ZrCore_TypeLayout_DropInline` only when the resolved layout has a non-`NONE` drop kind. This does not complete hidden-return ownership or exception-aware partial initialization; it prevents the generated C path from bypassing the existing layout-driven drop contract once non-POD inline value slots are active.

The generated-frame ABI is still mixed during this migration: inline struct payloads are addressed as byte spans from `frame.slotBase`, while scalar temporaries are still addressed as legacy stack value slots with `ZrCore_Stack_GetValue(frame.slotBase + slot)`. Typed call/return lowering is executable only for the first proven POD direct-call subset; large hidden returns, whole non-POD drop/finalization, mismatched nested inline struct field transfer, dynamic calls, and tail-call overlap remain future slices. The current AOT behavior for proven-but-unsupported value fields is an explicit generated failure boundary rather than helper fallback.

Typed scalar arithmetic has a first non-helper AOT lowering slice in `backend_aot_c_lowering_typed_arithmetic.c`. The function-body dispatcher routes non-const `ADD_SIGNED`, `ADD_UNSIGNED`, `SUB_SIGNED`, `SUB_UNSIGNED`, `MUL_SIGNED`, `MUL_UNSIGNED`, `DIV_SIGNED`, `DIV_UNSIGNED`, `ADD_FLOAT`, `SUB_FLOAT`, `MUL_FLOAT`, `DIV_FLOAT`, `NEG_SIGNED`, and `NEG_FLOAT` to emitter helpers instead of printing `ZrLibrary_AotRuntime_*` arithmetic calls. Those emitters load the source `SZrTypeValue` slots, validate the signed/unsigned/float category selected by the opcode, and emit direct C scalar expressions such as `zr_aot_left_scalar + zr_aot_right_scalar`, `-`, `*`, `/`, and `-zr_aot_source_scalar` into `ZR_VALUE_FAST_SET`. Division checks zero before the generated C division, reports `divide by zero`, and exits through `ZR_AOT_C_FAIL()` rather than falling through after an error.

The same lowering module now covers const-specialized signed and unsigned `+`, `-`, `*`, and `/` opcode families. It reads the right-hand operand from the function constant table, formats it as a typed C literal, checks the source slot type at runtime, and emits `zr_aot_left_scalar +|-|*|/ zr_aot_right_literal`. If the constant table entry does not match the opcode's signed/unsigned contract, the generated code raises an explicit unsupported typed arithmetic constant error instead of falling back to a VM arithmetic helper.

Signed and unsigned typed `%` now follow the same direct scalar lowering shape for both register/register and register/constant opcode families. `MOD_SIGNED`, `MOD_UNSIGNED`, `MOD_SIGNED_CONST`, and `MOD_UNSIGNED_CONST` emit `zr_aot_left_scalar % zr_aot_right_scalar` or `zr_aot_left_scalar % zr_aot_right_literal`, validate the selected signed/unsigned operand category, and report `modulo by zero` before the generated C `%` expression. Generic `MOD` and float modulo deliberately stay on their existing runtime helper contracts because they still need separate semantic decisions.

Typed integer bitwise and shift operations have their own AOT lowering module in `backend_aot_c_lowering_typed_bitwise.c`. `BITWISE_NOT`, `BITWISE_AND`, `BITWISE_OR`, `BITWISE_XOR`, `SHIFT_LEFT_INT`, `SHIFT_RIGHT_INT`, `BITWISE_SHIFT_LEFT`, and `BITWISE_SHIFT_RIGHT` now validate integer operands and emit C `~`, `&`, `|`, `^`, `<<`, and `>>` expressions instead of direct `ZrLibrary_AotRuntime_Bitwise*` / `Shift*Int` helper calls. Left shifts are generated through an unsigned intermediate before casting back to `TZrInt64`, and `BITWISE_SHIFT_RIGHT` preserves the existing logical-right-shift contract by using an unsigned left operand. Shift counts are checked as `[0, 63]` before generating the shift expression so undefined C shift counts do not become silent AOT behavior.

The typed numeric comparison families now use the same lowering module. Signed, unsigned, and float `==`, `!=`, `>`, `<`, `>=`, and `<=` opcodes validate their operand category and emit direct C comparison expressions such as `zr_aot_left_scalar == zr_aot_right_scalar` into a boolean destination. This removes the direct `ZrLibrary_AotRuntime_Logical*` helper path for those typed numeric comparison opcodes while leaving generic/string equality, dynamic logical operations, and short-circuit logical operations on their explicit runtime contract paths.

Typed bool logical operations have a separate narrow lowering module in `backend_aot_c_lowering_typed_logical.c`. `LOGICAL_EQUAL_BOOL` and `LOGICAL_NOT_EQUAL_BOOL` validate that both operands are bool values, normalize their stored `nativeBool` payloads, and emit `zr_aot_left_bool == zr_aot_right_bool` or `zr_aot_left_bool != zr_aot_right_bool` into a boolean destination. Statically bool unary `!` now compiles to `LOGICAL_NOT_BOOL`, which validates a bool source and emits `!zr_aot_source_bool`. The function-body dispatcher routes these typed bool opcodes through direct emitters instead of printing `ZrLibrary_AotRuntime_LogicalEqualBool` / `LogicalNotEqualBool` / `LogicalNot` calls for the proven-bool cases.

Typed bool control flow now has its first non-truthiness lowering slice. `compiler_create_jump_if_false_for_condition()` emits `JUMP_IF_BOOL_FALSE` when the condition expression is statically bool, while preserving generic `JUMP_IF` for dynamic truthiness and for existing compare/iterator fusion shapes. The interpreter validates a bool frame slot and branches on `nativeBool` directly. AOT C lowers this opcode in `backend_aot_c_lowering_control.c` by reading the bool slot and emitting `if (!zr_aot_condition_bool) { goto ...; }` instead of calling `ZrLibrary_AotRuntime_IsTruthy`.

Typed numeric conversions now have a first direct scalar lowering slice. Explicit `<float>` casts from statically signed integer sources compile to `TO_FLOAT_SIGNED`, explicit `<float>` casts from statically unsigned integer sources compile to `TO_FLOAT_UNSIGNED`, explicit `<int>` casts from statically float sources compile to `TO_INT_FLOAT`, explicit `<int>` casts from statically unsigned integer sources compile to `TO_INT_UNSIGNED`, explicit `<uint>` casts from statically float sources compile to `TO_UINT_FLOAT`, and explicit `<uint>` casts from statically signed integer sources compile to `TO_UINT_SIGNED`; same-type casts still collapse to the source slot and dynamic/other conversions keep the generic `TO_FLOAT` / `TO_INT` / `TO_UINT` paths. The interpreter reads the source through generated-frame slots and stores direct scalar results into `SZrTypeValue` destinations. AOT C lowers these opcodes in `backend_aot_c_lowering_typed_conversion.c` with `zr_aot_convert_signed_to_float`, `zr_aot_convert_unsigned_to_float`, `zr_aot_convert_float_to_signed`, `zr_aot_convert_unsigned_to_signed`, `zr_aot_convert_float_to_unsigned`, and `zr_aot_convert_signed_to_unsigned` blocks, using `(TZrFloat64)zr_aot_source_scalar`, `(TZrInt64)zr_aot_source_scalar`, `(TZrUInt64)zr_aot_source_scalar`, and an explicit `zr_aot_unsigned_to_signed_limit` formula instead of runtime conversion helpers for these proven numeric cases.

This slice deliberately leaves generic/dynamic arithmetic, generic `NEG`, string concatenation, float modulo, power, dynamic shift/meta operations, bool/string/object conversions, generic/string equality, generic logical-not conversion, and logical `&&`/`||` on their existing helper paths. Those paths still carry VM semantics that need separate metadata or constant-materialization contracts before they can be converted to direct C/C++ expressions without silently changing behavior.

Dynamic, meta, ownership, and iterator SemIR opcodes keep their existing runtime contract behavior. Value-type SemIR is therefore a typed-layout contract first; the interpreter is being moved onto the same layout facts in incremental source-level slices.

## Interpreter Boundary

The active interpreter now has a first source-level local `struct` value-type execution slice for primitive POD fields:

- `ZrCore_Function_ResolvePrototypeFrameFieldLayout()` resolves a struct field name to byte offset, byte size, primitive value kind, nested type id, or embedded `SZrTypeValue` marker from compiled prototype metadata.
- `execution_inline_frame_try_get_member*()` and `execution_inline_frame_try_set_member*()` resolve known inline struct receivers through `SZrFunctionFrameSlotLayout`, then load/store the primitive field directly from the frame byte span.
- `execution_inline_frame_try_copy_stack_slot()` copies inline-struct slots by layout, and can materialize a boxed/source constructor object into an inline destination by writing primitive fields at their layout offsets.
- The interpreter dispatch checks these inline-frame helpers before falling back to object/array/string member dispatch for `GET_MEMBER`, `SET_MEMBER`, `GET_MEMBER_SLOT`, and `SET_MEMBER_SLOT`.
- Quickening preserves by-value local copy intent by forwarding `GET_STACK p -> temp; SET_STACK q <- temp` into a direct inline-struct `SET_STACK q <- p` when both slots share the same inline type layout.
- Source-level by-value struct arguments now preserve inline layout facts for the caller staging slot and the callee parameter slot. When a POD struct parameter is mutated inside the callee, field stores target the callee frame byte span and do not alias the caller's source payload.
- Runtime prototype field-layout resolution can recover the entry function's prototype metadata from the active call stack when compiled child functions no longer retain owner links after quickening/compilation cleanup.
- Source-level POD struct returns now preserve inline layout facts for the caller result slot from resolved call return types. The runtime return path copies the callee inline return payload into the caller inline target span, so mutating a returned value does not alias another inline payload.
- Inline stack slot hint floors keep struct materialization, argument, and return slots from being trimmed and reused as scalar scratch while a layout-aware expression is still active.
- Primary-expression compilation tracks and returns the actual result slot. Normalizing a call expression into a requested target slot therefore copies from the known result slot instead of assuming `stackSlotCount - 1`, which is no longer reliable once inline payload slots are preserved above scalar temporaries.
- Typed bool equality, typed bool logical-not, typed signed/float negation, `JUMP_IF`, and `JUMP_IF_BOOL_FALSE` condition dispatch now read and write through frame metadata slots instead of the old flat stack mirror. This keeps scalar bool and numeric temporaries aligned with the same generated-frame layout used by inline value payloads.
- Typed signed/unsigned-to-float, float-to-signed/unsigned, and signed/unsigned integer cross-cast opcodes read and write through the same generated-frame slot accessors, keeping explicit numeric cast temporaries aligned with the inline-frame ABI. `TO_INT_UNSIGNED` uses an explicit high-bit wrap formula so `uint` values above `INT64_MAX` do not rely on implementation-defined C signed casts.

This slice intentionally stays narrow: it covers source-level local struct construction/materialization, local by-value copy, POD struct by-value parameter isolation, POD struct by-value return isolation, and primitive field get/set. Large/non-trivial hidden returns, managed/reference fields, tail-call overlap stress, and hidden-return/AOT executable body replacement remain later milestones.

## Verification

`tests/parser/test_semir_pipeline.c` compiles a `Point` struct, copies it by value, stores into a field, and loads fields. The `.zri` output must include `FIELD_ADDR`, `STORE_VALUE`, `COPY_VALUE`, and `LOAD_VALUE`.

The same SemIR suite compiles `makePoint(seed: int): Point`, stores its returned value in an inline caller slot, and verifies the `.zri` output includes `CALL_TYPED` and `RETURN_TYPED`. This is covered by `test_struct_value_type_call_and_return_emit_semir_metadata`.

`tests/parser/test_value_type_runtime.c` executes a source-level `Point` struct fixture in the interpreter. It verifies that `var q: Point = p; q.x = 7;` mutates only the copied inline payload and that both `p` and `q` occupy separate frame byte spans with field values read through layout metadata rather than fixed offsets.

The same runtime suite also verifies by-value parameter isolation: `mutate(point: Point)` writes `point.x` inside the callee, returns the mutated callee value, and the caller's original `p.x` remains unchanged.

The runtime suite now also verifies by-value return isolation: `makePoint(seed: int): Point` returns a local inline `Point`, the caller mutates the returned value, and the returned payload remains independent of the source local and other caller inline payloads. This is covered by `test_inline_struct_return_mutation_is_by_value`.

The same test suite also checks the archived AotExecIR source contract: header/source text must expose per-slot inline frame byte layout and release the sidecar. The archive is not registered as an active CMake target in the main build, so compile verification uses a constrained object compile of `backend_aot_exec_ir.c` with the removed archived `SZrAotWriterOptions` type stubbed.

The suite also checks the archived `aot_c` source contract: `backend_aot_c_value_semir.*` must expose the value SemIR lowering entry point, consume `SZrAotExecIrFrameLayout`, recognize `FIELD_ADDR` / `LOAD_VALUE` / `STORE_VALUE` / `COPY_VALUE` / `CALL_TYPED` / `RETURN_TYPED`, resolve field layout, emit `frame.slotBase` byte expressions, expose executable inline-copy, primitive field load/store, and typed call/return helpers, and be wired from `backend_aot_c_function_body.c`. The function-body source must try `backend_aot_try_write_c_value_semir_for_exec_instruction()` before old `GET_STACK` / `SET_STACK` stack-copy lowering, before old `GET_MEMBER_SLOT` / `SET_MEMBER_SLOT` runtime helpers, and before generic direct call/return helpers.

The source-contract assertions now also lock the concrete field lowering shape and typed call/return shape: byte field address, scalar `SZrTypeValue` source/destination slot access, `memcpy` between field bytes and primitive temporaries, `ZR_VALUE_FAST_SET` for load results, direct-call preparation/finish, typed return `state->stackTop`, `ZrCore_Function_ResolvePrototypeFrameTypeLayout`, and the `ZrCore_Function_TryCopyInlineFrameReturnValue` post-call contract. Inline struct copy is locked by `zr_aot_value_exec_inline_copy`, `ZrCore_TypeLayout_CanRawCopy`, and `zr_aot_value_exec_inline_field_copy` / `ZrCore_TypeLayout_CopyInline` for non-POD field-aware copy. The same value SemIR source-contract test now also locks embedded `SZrTypeValue` field load/store with `backend_aot_c_value_field_layout_can_value_slot_exec`, `zr_aot_value_exec_field_value_slot_load`, `zr_aot_value_exec_field_value_slot_store`, and direct `ZrCore_Value_Copy` expressions. Nested inline struct field transfer is locked by `backend_aot_c_value_field_layout_can_inline_struct_exec`, `zr_aot_value_exec_field_inline_struct_load`, `zr_aot_value_exec_field_inline_struct_store`, and `memmove` expressions that copy between the nested field span and the inline struct slot span. Resolved fields that are still unsupported remain locked by `zr_aot_value_unsupported_field_load`, `zr_aot_value_unsupported_field_store`, `fieldLayout.isPrimitivePod`, `fieldLayout.isValueSlot`, `fieldLayout.typeLayoutId`, `ZrCore_Debug_RunError`, and `ZR_AOT_C_FAIL()`.

The AOT source-contract suite now also locks generated value-frame cleanup: `backend_aot_c_frame_cleanup.*` must exist, generated C must include `zr_vm_core/function.h` and `zr_vm_core/type_layout.h`, `ZR_AOT_C_FAIL()` must route through `ZR_AOT_C_RETURN(...)` rather than directly returning, dispatch/default/return paths must use the shared exit, cleanup must emit `zr_aot_value_frame_drop` blocks with `ZrCore_TypeLayout_DropInline`, and typed inline return must set `zr_aot_skip_drop_slot` before `ZR_AOT_C_RETURN(1)`.

`tests/parser/test_aot_c_source_contracts.c` now owns the archived AOT C source-contract assertions that were split out of `tests/parser/test_semir_pipeline.c`. The split keeps SemIR pipeline tests focused on compiler/runtime artifacts while giving AOT C lowering contracts their own growth boundary.

The AOT C source-contract suite now checks that typed scalar signed/unsigned/float arithmetic exposes dedicated emitter declarations, emits source-level C `+`, `-`, `*`, `/`, and unary `-zr_aot_source_scalar` expressions from `backend_aot_c_lowering_typed_arithmetic.c`, routes the corresponding non-const typed arithmetic opcodes from `backend_aot_c_function_body.c`, and no longer prints direct `ZrLibrary_AotRuntime_*` arithmetic helper calls for those opcode families. It also checks signed/unsigned const arithmetic emitter declarations, constant literal formatting, `zr_aot_left_scalar +|-|*|/ zr_aot_right_literal` source expressions, function-body routing for const typed arithmetic opcodes, and absence of direct const arithmetic helper strings for those signed/unsigned opcode families.

The same test now locks typed signed/unsigned `%` lowering: direct emitter declarations, `zr_aot_left_scalar % zr_aot_right_scalar`, `zr_aot_left_scalar % zr_aot_right_literal`, `modulo by zero`, function-body routing for typed modulo opcodes, and absence of direct signed/unsigned modulo helper strings for those opcode families. It also locks typed numeric comparison lowering: signed/unsigned/float comparison emitter declarations, `zr_aot_compare_exec_*` source markers, `==`, `!=`, `>`, `<`, `>=`, and `<=` scalar expressions, function-body routing for typed numeric comparison opcodes, and absence of direct `ZrLibrary_AotRuntime_Logical*` helper strings for those opcode families.

The bitwise source-contract test `test_aot_c_source_lowers_typed_bitwise_to_c_expressions` locks the separate typed bitwise lowering module, emitter declarations, C expression shapes for `~`, `&`, `|`, `^`, signed shift-right, unsigned left/right shift intermediates, shift-count range checks, function-body routing for typed bitwise and shift opcodes, and absence of direct bitwise/typed-shift runtime helper strings for those opcode families.

The bool logical source-contract test `test_aot_c_source_lowers_typed_bool_equality_to_c_expressions` locks the separate typed logical lowering module, bool equality and logical-not emitter declarations, `zr_aot_bool_compare_exec` / `zr_aot_bool_not_exec` source markers, direct `==` / `!=` / `!` bool expressions, function-body routing for `LOGICAL_EQUAL_BOOL` / `LOGICAL_NOT_EQUAL_BOOL` / `LOGICAL_NOT_BOOL`, typed bool false-branch routing for `JUMP_IF_BOOL_FALSE`, `zr_aot_jump_if_bool_false`, and absence of direct bool equality/logical-not runtime helper strings for those proven-bool opcode families.

The numeric conversion source-contract test `test_aot_c_source_lowers_typed_numeric_conversion_to_c_casts` locks the separate typed conversion lowering module, direct emitter declarations for signed/unsigned-to-float, float-to-signed/unsigned, unsigned-to-signed, and signed-to-unsigned conversion helpers, the `zr_aot_convert_signed_to_float` / `zr_aot_convert_unsigned_to_float` / `zr_aot_convert_float_to_signed` / `zr_aot_convert_unsigned_to_signed` / `zr_aot_convert_float_to_unsigned` / `zr_aot_convert_signed_to_unsigned` source markers, direct `(TZrFloat64)`, `(TZrInt64)`, and `(TZrUInt64)` cast expressions, the `zr_aot_unsigned_to_signed_limit` wrap expression, and function-body routing for `TO_FLOAT_SIGNED` / `TO_FLOAT_UNSIGNED` / `TO_INT_FLOAT` / `TO_INT_UNSIGNED` / `TO_UINT_FLOAT` / `TO_UINT_SIGNED`.

`backend_aot_c_value_semir.c` is verified with constrained GCC/Clang syntax checks because the archived AOT backend is not in the active CMake target graph. A standalone syntax check of `backend_aot_c_function_body.c` is still blocked by the archived include tree's missing `SZrAotWriterOptions` type, so its current signal is the source-contract test plus active CLI smoke.

Focused 2026-06-04 validation added `test_aot_c_source_lowers_typed_arithmetic_to_c_expressions` to `tests/parser/test_semir_pipeline.c`. The test first failed on the missing typed arithmetic lowering contract, then passed after `backend_aot_c_lowering_typed_arithmetic.c` was added and the function-body dispatcher was updated. It failed again after const arithmetic expectations were added, then passed after const signed/unsigned lowering moved into the same module. It failed again after typed numeric comparison expectations were added, then passed after signed/unsigned/float comparison lowering moved into the same module. It failed again after signed/unsigned modulo expectations were added, then passed after typed `%` lowering and routing moved into the same module. The typed arithmetic source passes WSL GCC and WSL Clang `-std=c11 -fsyntax-only` checks with active and archived include roots.

Focused 2026-06-04 typed bitwise validation added `test_aot_c_source_lowers_typed_bitwise_to_c_expressions`. It first failed on the missing bitwise lowering contract, then passed after `backend_aot_c_lowering_typed_bitwise.c` was added, old `BITWISE_XOR` fallback lowering was removed from `backend_aot_c_lowering_values.c`, and the function-body dispatcher was routed to direct bitwise emitters. The new bitwise source passes WSL GCC and WSL Clang `-std=c11 -fsyntax-only` checks with active and archived include roots.

Focused 2026-06-04 AOT C source-contract maintenance split the AOT C lowering tests into `tests/parser/test_aot_c_source_contracts.c` and registered `zr_vm_aot_c_source_contracts_test`. The split was validated by rebuilding and running both `zr_vm_semir_pipeline_test` and `zr_vm_aot_c_source_contracts_test`; the SemIR pipeline retained 6 passing tests and the new AOT source-contract target retained the moved 3 passing tests. The same target then added a RED bool equality contract, failed on the missing lowering surface, and passed after `backend_aot_c_lowering_typed_logical.c` plus the bool dispatcher routes were added.

Focused 2026-06-04 typed bool logical-not validation added `tests/parser/test_typed_bool_logical_not.c` and registered `zr_vm_typed_bool_logical_not_test`. The test compiles `!flag` where `flag` is statically bool, verifies that `LOGICAL_NOT_BOOL` is emitted without the generic `LOGICAL_NOT` opcode, executes through `JUMP_IF`, and expects the inverted branch result. This first exposed stale flat-stack reads in the interpreter bool path; `LOGICAL_EQUAL_BOOL`, `LOGICAL_NOT_EQUAL_BOOL`, `LOGICAL_NOT_BOOL`, and `JUMP_IF` now use `FRAME_VALUE_SLOT` consistently for generated-frame execution.

Focused 2026-06-04 typed bool control-flow validation added `tests/parser/test_typed_bool_conditional_jump.c` and registered `zr_vm_typed_bool_conditional_jump_test`. The test compiles a statically bool ternary condition, verifies `JUMP_IF_BOOL_FALSE` is emitted without generic `JUMP_IF`, executes the false branch, and returns the expected value. The same slice routes `JUMP_IF_BOOL_FALSE` through AOT C direct control lowering, AotExecIR conditional-branch recognition, writer output, quickening CFG metadata, and frame-layout slot tracking.

Focused 2026-06-04 typed numeric unary validation added `tests/parser/test_typed_numeric_neg.c` and registered `zr_vm_typed_numeric_neg_test`. The signed case compiles `-i` for a statically `int` source, verifies `NEG_SIGNED` is emitted without generic `NEG`, executes through generated-frame slots, and returns `-7`. The float case compiles `-f` for a statically `float` source, verifies `NEG_FLOAT` is emitted without generic `NEG`, executes the opcode, and returns a float `-2.0`. The AOT C source contract now also requires `backend_aot_write_c_direct_neg_signed`, `backend_aot_write_c_direct_neg_float`, `zr_aot_arith_exec_signed_unary`, `zr_aot_arith_exec_float_unary`, and `-zr_aot_source_scalar`.

Focused 2026-06-04 typed numeric conversion validation added `tests/parser/test_typed_numeric_conversion.c` and registered `zr_vm_typed_numeric_conversion_test`. The signed-to-float case compiles `return <float> i` for a statically `int` source, verifies `TO_FLOAT_SIGNED` is emitted without generic `TO_FLOAT`, executes through generated-frame slots, and returns float `7.0`. The unsigned-to-float case compiles `return <float> u` for a statically `uint` source, verifies `TO_FLOAT_UNSIGNED` is emitted without generic `TO_FLOAT`, executes through generated-frame slots, and returns float `9.0`. The float-to-signed case compiles `return <int> f` for a statically `float` source, verifies `TO_INT_FLOAT` is emitted without generic `TO_INT`, executes through generated-frame slots, and returns integer `2`. The unsigned-to-signed case compiles `return <int> u` for a statically `uint` source, verifies `TO_INT_UNSIGNED` is emitted without generic `TO_INT`, executes through generated-frame slots, and returns integer `17`; a high-bit boundary case materializes `<uint>-1`, converts it back through `TO_INT_UNSIGNED`, and returns `-1` with C# unchecked-style wrap semantics. The float-to-unsigned case compiles `return <uint> f` for a statically `float` source, verifies `TO_UINT_FLOAT` is emitted without generic `TO_UINT`, executes through generated-frame slots, and returns unsigned integer `12`. The signed-to-unsigned case compiles `return <uint> i` for a statically negative `int` source, verifies `TO_UINT_SIGNED` is emitted without generic `TO_UINT`, executes through generated-frame slots, and returns the modulo unsigned payload. The AOT C source contract now also requires `backend_aot_c_lowering_typed_conversion.c`, direct function-body routing, explicit C cast expressions, and the deterministic unsigned-to-signed wrap formula for those six proven numeric conversion forms.

Focused 2026-06-03 validation for the POD return, typed call/return SemIR, first M5 AOT expression-contract slice, executable inline-copy lowering, primitive field load/store lowering, and first typed call/return executable lowering slice passed on WSL GCC and WSL Clang for `zr_vm_semir_pipeline_test`, `zr_vm_instruction_execution_test`, `zr_vm_compiler_integration_test`, and `zr_vm_value_type_runtime_test`. The archived `backend_aot_c_value_semir.c` source also passed GCC and Clang `-fsyntax-only` checks with the required active and archived include roots plus the archived `SZrAotWriterOptions` stub. The same slice also passed an MSVC CLI smoke build and `hello_world.zrp` execution.
