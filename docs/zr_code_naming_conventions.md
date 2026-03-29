# Zr VM Code Naming Conventions

This document is the single source of truth for naming across first-party Zr VM code.

## Scope

- Applies to `zr_vm_common`, `zr_vm_core`, `zr_vm_parser`, `zr_vm_library`, `zr_vm_language_server`, `zr_vm_cli`, and `tests`.
- Does not apply to module-local `third_party/`, generated output, or build/cache directories.

## Naming Rules

- Simple type aliases: `TZrXxx`
- Structs: `SZrXxx`
- Enums: `EZrXxx`
- Function-signature aliases: `FZrXxx`
- Public functions: `Zr<Module>_<Object>_<Action>`
- Module entry functions with no clear object: `Zr<Module>_<Action>`
- Static functions: `lower_snake_case`
- Struct fields, locals, parameters: `lowerCamel`
- Global variables: `GZrXxx`
- Constant variables: `CZrXxx`
- Macros: `ZR_XXX_XXX`
- Enum members: `ZR_<ENUM_CONTEXT>_<VALUE>`
- Files and folders: `lower_snake_case`
- Header guards: `ZR_<PATH>_H`

## Module Segments

- `Common`
- `Core`
- `Parser`
- `Library`
- `LanguageServer`
- `Cli`
- `Tests`

## Canonical Base Alias Map

| Old | New |
| --- | --- |
| `TChar` | `TZrChar` |
| `TByte` | `TZrByte` |
| `TBytePtr` | `TZrBytePtr` |
| `TUInt8` | `TZrUInt8` |
| `TInt8` | `TZrInt8` |
| `TUInt16` | `TZrUInt16` |
| `TInt16` | `TZrInt16` |
| `TUInt32` | `TZrUInt32` |
| `TInt32` | `TZrInt32` |
| `TUInt64` | `TZrUInt64` |
| `TInt64` | `TZrInt64` |
| `TFloat32` | `TZrFloat32` |
| `TFloat` | `TZrFloat` |
| `TFloat64` | `TZrFloat64` |
| `TDouble` | `TZrDouble` |
| `TBool` | `TZrBool` |
| `TEnum` | `TZrEnum` |
| `TNativeString` | `TZrNativeString` |

## Public API Mapping Rules

- Old pure-Camel public APIs are replaced in-place.
- Existing segmented names that already follow `Zr<Module>_<...>` remain segmented and are only adjusted when their segments violate the rules.
- Public API names are normalized by:
  - keeping the module segment explicit;
  - using the target object as the middle segment;
  - using the action as the trailing segment;
  - using PascalCase inside each segment.

## Representative API Mappings

| Old | New |
| --- | --- |
| `ZrArrayConstruct` | `ZrCore_Array_Construct` |
| `ZrArrayInit` | `ZrCore_Array_Init` |
| `ZrArrayGet` | `ZrCore_Array_Get` |
| `ZrStateNew` | `ZrCore_State_New` |
| `ZrStateDoRun` | `ZrCore_State_DoRun` |
| `ZrParserParse` | `ZrParser_Parse` |
| `ZrParserFreeAst` | `ZrParser_Ast_Free` |
| `ZrAstNodeArrayNew` | `ZrParser_AstNodeArray_New` |
| `ZrCompilerCompile` | `ZrParser_Compiler_Compile` |
| `ZrCliMain` | `ZrCli_Main` |
| `zr_test_create_state` | `ZrTests_State_Create` |
| `zr_test_destroy_state` | `ZrTests_State_Destroy` |
| `zr_test_execute_function` | `ZrTests_Function_Execute` |
| `zr_test_execute_function_expect_int64` | `ZrTests_Function_ExecuteExpectInt64` |

## Explicit Cleanups Required During Renaming

- Delete empty module stubs based on `ZR_EMPTY_MODULE` and `ZR_EMPTY_FILE` when the translation unit does not carry real logic.
- Replace `kZr...` macros with `ZR_...`.
- Do not keep compatibility wrappers, alias macros, or bridge functions.
- Do not rename `*/third_party/` content.
