---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/comptime_cache.h
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2_lock.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_language_server_extension/schemas/zrp.schema.json
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser
  - zr_vm_parser/src/zr_vm_parser/compiler
  - zr_vm_core/src/zr_vm_core
  - zr_vm_library/src/zr_vm_library/project
tests:
  - tests/parser/test_expression_fragment_parser.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_instruction_execution.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_comptime_runtime_contract.c
  - tests/compileTime/test_comptime_cache_snapshot_cases.h
  - tests/library/test_project_manifest_v2.c
  - tests/cli/test_cli_project_incremental.c
  - tests/cli/test_cli_comptime_cache_incremental_cases.h
references:
  - lua/rust/library/alloc/src/fmt.rs
  - lua/rust/compiler/rustc_parse_format/src/lib.rs
  - lua/rust/compiler/rustc_builtin_macros/src/format.rs
  - lua/rust/compiler/rustc_parse_format/src/tests.rs
doc_type: milestone-detail
status: contract-first
---

# M5: format! and build inputs

This document locks the M5 language/project contract. It is design and test
work only; no production implementation or cache-version change is authorized
until the listed gates pass. The companion macro plan names format and declared
build inputs as M5.

## 1. format! surface

`format!(literal, arguments...)` is a compile-time parsed function-like
intrinsic. Its first argument is a direct string literal. The parser produces a
`ZR_AST_FORMAT_EXPRESSION` containing a compiled template and resolved
argument references. It must not call the host printf formatter in
`zr_vm_core/src/zr_vm_core/string.c`.

Required examples:

~~~zr
format!("hello {}", name)
format!("{0} = {value}", answer, value = answer)
format!("{{literal}} {x}", x)
format!("{value:>width$}", value, width = 8)
format!("{value:.precision$}", value, precision = 3)
format!("{value:.*}", value, 3)
format!("{n:#010x} {n:b} {f:.2e}", n, f)
~~~

The compiler may fold an invocation to a string constant when every argument
and formatter is pure. Otherwise the same template runs through the runtime
engine.

### 1.1 Template grammar

The grammar follows the useful Rust data model in
`lua/rust/compiler/rustc_parse_format/src/lib.rs`, while remaining a ZR
intrinsic:

~~~text
format-call  := "format" "!" "(" string-literal ("," format-arg)* [","] ")"
format-arg   := identifier "=" expression | expression
template     := (literal-text | "{{" | "}}" | placeholder)*
placeholder  := "{" reference [":" spec ] "}"
reference    := "" | decimal-index | identifier
spec         := [fill] align [sign] ["#"] ["0"] [count] [("." precision)] [type]
fill         := one Unicode scalar (only when followed by align)
align        := "<" | "^" | ">"
sign         := "+" | "-"
count        := decimal | decimal "$" | identifier "$"
precision    := decimal | decimal "$" | identifier "$" | "*"
type         := "" | "d" | "b" | "o" | "x" | "X" | "e" | "E" | "f" | "g" | "?"
~~~

`{}` consumes the next positional expression. `{0}` is zero-based and does
not advance that implicit counter. `{name}` resolves an explicit named
argument, or captures lexical identifier `name` exactly once. An explicit
`name = expression` is one source expression, not a second evaluation of the
captured binding. Names are unique. Once a named argument appears, a later
unlabelled positional argument is rejected for deterministic binding.

`{{` and `}}` emit literal braces. Any other unmatched brace, duplicate name,
unknown capture, out-of-range index, malformed spec, unknown type, trailing spec
character, or count with a non-integer value is a compile-time diagnostic. The
literal is parsed before evaluating any argument.

Widths and precisions use Rust-style dollar references. `.*` consumes the next
unlabelled integer argument for precision; a count reference does not advance
the placeholder counter. Width is a minimum width. Precision truncates strings
and controls fractional digits for floating values. `?` requires a debug or
formattable implementation.

### 1.2 Numeric and Unicode semantics

`d`, `b`, `o`, `x`, and `X` are decimal, binary, octal, lower
hexadecimal, and upper hexadecimal integer forms. `e`/`E` are exponential,
`f` fixed, and `g` general floating forms. The default is display. `#`
selects canonical radix prefixes (0b, 0o, 0x), `0` enables sign-aware zero
padding, and `+` requests a sign for non-negative numbers.

Width, padding, and string precision count Unicode scalar values, never UTF-8
bytes. A fill is one scalar and truncation never emits invalid UTF-8. Numeric
rendering is locale-independent and must not use process-global locale state.
Negative dynamic width/precision is a located `format.count_negative` error, not
an unsigned wrap.

## 2. Evaluation and lowering

The parser has three passes, matching the parse/resolve/lower split documented in
`lua/rust/compiler/rustc_builtin_macros/src/format.rs`:

1. Parse the literal into literal pieces and placeholder `FormatSpec` records,
   retaining spans for braces, counts, and type.
2. Parse explicit arguments, resolve named, positional, and captured references,
   and assign one stable slot to each source expression and dynamic count.
3. Emit the template constant and evaluate slots in source order. Runtime reads
   slots only; it never retains or re-evaluates AST expressions.

The exactly-once rule is:

~~~text
for each source argument expression, left to right:
    evaluate once into a temporary slot
for each placeholder, in template order:
    read value/count slots and append rendered text
~~~

A side-effecting `name = call()` runs once even when referenced repeatedly.
A dynamic width/precision expression follows the same rule. One expression
written at two positional sites is evaluated twice because it has two source
occurrences. A capture is one slot per unique identifier.

Add a template constant representation next to the existing template-string
compiler in `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c`.
Do not lower placeholders to repeated `TO_STRING` expression trees; that
instruction remains the empty/default conversion path.

## 3. Runtime engine and IFormattable

The canonical language protocol is:

~~~zr
module zr.format

struct FormatSpec {
    fill: string       // one Unicode scalar
    align: Align
    sign: Sign
    alternate: bool
    zeroPad: bool
    width: int?        // resolved count or null
    precision: int?    // resolved count or null
    type: FormatType
}

interface IFormattable {
    fn format(self, spec: FormatSpec): string
}
~~~

`FormatSpec` is immutable and compiler-created with resolved counts. A
protocol implementation must return a string; otherwise
`format.protocol_return_type` is raised.

Dispatch is fixed:

1. Empty/default `{}` calls `ZrCore_Value_ConvertToString`, preserving the
   current `ZR_META_TO_STRING` lookup and fallback behavior.
2. A non-empty spec on a built-in string or number uses the built-in renderer.
3. A non-empty spec on another value calls
   `zr.format.IFormattable.format(spec)` when its protocol mask advertises
   Formattable.
4. Otherwise fail with `format.unsupported_spec`.

Only a native descriptor marked pure and deterministic may run during comptime
folding. Side-effecting/native-only formatters remain runtime-only.

Use one engine entry point independent of instruction dispatch:

~~~c
typedef struct SZrFormatTemplate SZrFormatTemplate;

TZrBool ZrCore_Format_Render(
        SZrState *state,
        const SZrFormatTemplate *template,
        const SZrTypeValue *argumentSlots,
        TZrSize argumentSlotCount,
        SZrString **outString,
        SZrFileRange errorLocation);
~~~

The template owns UTF-8 literal pieces and placeholder records. Each placeholder
stores its value slot, optional width/precision slots, and resolved flags. The
engine roots borrowed values while appending to one builder; no host varargs API
is part of this boundary.

Append `ZR_PROTOCOL_ID_FORMATTABLE` after the current stable slot source in
`zr_vm_core/include/zr_vm_core/object.h`; do not renumber existing IDs.
Register `IFormattable.format` in `builtin_module.c` and native metadata.
Shared dispatch queries protocol/descriptor data and never compares concrete type
names.

Comptime evaluation needs an explicit `ZR_AST_FORMAT_EXPRESSION` case in
`compile_time_executor.c`. Fold only supported pure values and
pure/deterministic custom formatters. A non-pure refusal falls back to runtime;
malformed syntax, bad counts, and protocol return-type failures are hard errors.

The current `ZRCCV005` snapshot stores fixed-width primitive values, although
`ZrParser_ComptimeCache_MixValue` can mix strings. M5 must either make folded
strings deliberately non-cacheable or preferably add `ZRCCV006` with
variable-length records: digest, value type, payload length, and canonical UTF-8
bytes. V005 remains importable for one release; V006 is the export format,
digest-sorted with integrity verification. Correctness must not depend on cache
persistence, but repeated folded strings should not be permanent process-restart
misses.

## 4. Manifest v2 buildInputs

Add this optional exact-shape object to manifest v2:

~~~json
{
  "manifestVersion": 2,
  "name": "app",
  "version": "1.0.0",
  "kind": "application",
  "source": "src",
  "binary": "bin",
  "entry": "main",
  "buildInputs": {
    "files": ["config/version.txt", "schema/defaults.json"],
    "environment": ["ZR_MODE", "ZR_FEATURE_SET"]
  }
}
~~~

`files` and `environment` are arrays of unique strings. Files are normalized
project-root-relative slash paths; absolute paths, empty components, `..`
escapes, NUL, and paths outside the root are rejected. Environment names are
non-empty and contain neither `=` nor NUL. Parse order is accepted but storage
and writer output are bytewise sorted. Unknown keys inside `buildInputs` are
rejected. Keep the schema and loader in lockstep in
`zr_vm_language_server_extension/schemas/zrp.schema.json` and
`project_manifest_v2.c`.

Project-owned declarations and frozen values:

~~~c
typedef struct SZrLibrary_ProjectBuildInput {
    SZrString *name;       /* canonical path or environment name */
    TZrBool present;       /* file exists / environment is set */
    TZrByte *bytes;        /* owned snapshot; null when absent */
    TZrSize byteCount;
    TZrByte valueDigest[32];
} SZrLibrary_ProjectBuildInput;

struct SZrLibrary_Project {
    /* existing fields */
    SZrLibrary_ProjectBuildInput *buildInputFiles;
    TZrSize buildInputFileCount;
    SZrLibrary_ProjectBuildInput *buildInputEnvironment;
    TZrSize buildInputEnvironmentCount;
    TZrByte manifestDigest[32];
    TZrByte schemaDigest[32];
    TZrByte lockDigest[32];
    TZrByte buildInputDigest[32];
    TZrBool hasBuildInputSnapshot;
};
~~~

The loader parses declarations only. The build driver performs this order before
source parsing/comptime:

~~~text
Project_New -> ReadDependencyLock -> canonical schema/manifest/lock digests
            -> Project_CaptureBuildInputs -> Source_CompileWithComptimeCache
~~~

`ZrLibrary_Project_CaptureBuildInputs` reads each declared file/environment
once, owns bytes until `Project_Free`, and atomically sets the snapshot. A
second capture is an explicit new build. A failed capture leaves the prior
snapshot unchanged. No parser, provider, or evaluator may read the host
filesystem/environment after capture.

Proposed library boundary:

~~~c
ZR_LIBRARY_API TZrBool ZrLibrary_Project_CaptureBuildInputs(
        SZrState *state, SZrLibrary_Project *project,
        TZrChar *errorBuffer, TZrSize errorBufferSize);
ZR_LIBRARY_API TZrBool ZrLibrary_Project_GetBuildInputDigest(
        const SZrLibrary_Project *project, TZrByte outDigest[32]);
ZR_LIBRARY_API TZrBool ZrLibrary_Project_GetManifestSchemaDigest(
        TZrByte outDigest[32]);
ZR_LIBRARY_API const SZrLibrary_ProjectBuildInput *
ZrLibrary_Project_FindBuildInputFile(
        const SZrLibrary_Project *project, const TZrChar *canonicalPath);
ZR_LIBRARY_API const SZrLibrary_ProjectBuildInput *
ZrLibrary_Project_FindBuildInputEnvironment(
        const SZrLibrary_Project *project, const TZrChar *name);
~~~

A declared missing file is `compile.input.file_missing`. Environment lookup is
three-state: undeclared is an error, declared absent returns null, and
declared/present returns a string. A present empty variable returns `""` and
is never confused with null.

## 5. Compile-time input APIs

The compile-only `zr.compile` provider gains:

~~~zr
zr.compile.input.file(path: string): string
zr.compile.input.env(name: string): string?
~~~

Arguments must be compile-time strings and must resolve in the frozen snapshot.
Undeclared input is `compile.input.undeclared`; missing file is
`compile.input.file_missing`; absent environment is the nullable result above.
The API is unavailable at runtime and cannot use a later-phase value. File bytes
must be valid UTF-8 for the string API; a future byte API requires a new contract
hash.

Register roles in `zr_vm_parser/include/zr_vm_parser/compile_tool.h` as
`ZR_PARSER_COMPILE_TOOL_ROLE_INPUT_FILE` and
`ZR_PARSER_COMPILE_TOOL_ROLE_INPUT_ENV`. Both have `PURE_VALUE` effect and
`BUILD_FACTS` minimum phase. Their public contract hash changes with schema,
normalization, or missing-value behavior.

Evaluator-facing functions:

~~~c
TZrBool ZrParser_CompileInput_File(
        SZrCompilerState *cs, const TZrChar *canonicalPath,
        SZrTypeValue *outValue, SZrFileRange location);
TZrBool ZrParser_CompileInput_Environment(
        SZrCompilerState *cs, const TZrChar *name,
        TZrBool *outPresent, SZrTypeValue *outValue, SZrFileRange location);
~~~

Compiler state borrows one project snapshot and records its digest; it never
creates a second mutable host view. Direct parser compilation without a snapshot
may compile code that does not call these APIs, but an input call fails with
`compile.input.snapshot_unavailable`.

## 6. Deterministic digest and cache key

Hash this canonical stream with SHA-256 and domain `zr.build-inputs/v1`.
Integers are unsigned big-endian:

~~~text
domain
u64 fileCount
for files sorted by canonical path:
    u8 kind=0, u64 pathByteLength, pathBytes
    u8 present, u64 byteCount, valueDigest[32]
u64 environmentCount
for environment sorted by exact name bytes:
    u8 kind=1, u64 nameByteLength, nameBytes
    u8 present, u64 byteCount, valueDigest[32]
~~~

The presence bit precedes size/digest, so missing and present-empty differ.
Declaration order, JSON whitespace, path separators, and environment enumeration
order do not affect the result. Missing files fail capture, but retaining the
presence bit makes diagnostics and atomic-failure fixtures unambiguous.

`schemaDigest` is the generated SHA-256 of the canonical manifest-v2 schema
contract (including the `buildInputs` shape); it changes whenever the schema or
normalization rules change. `manifestDigest` is SHA-256 of canonical manifest-v2
writer output prefixed with `zr.project.manifest/v2`. `lockDigest` is SHA-256 of
canonical lock output prefixed with `zr.project.lock/v1`. These digests include
contract versions and dependency/provider identities, but never host input bytes.

Extend `SZrCompilerState` with:

~~~c
TZrByte comptimeManifestDigest[32];
TZrByte comptimeSchemaDigest[32];
TZrByte comptimeLockDigest[32];
TZrByte comptimeBuildInputDigest[32];
TZrBool hasComptimeManifestDigest;
TZrBool hasComptimeSchemaDigest;
TZrBool hasComptimeLockDigest;
TZrBool hasComptimeBuildInputDigest;
~~~

`ZrParser_ComptimeCache_BeginKey` in
`zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c` changes
domain to `zr.comptime.cache/v6` and mixes, in order: schema digest
presence/value, manifest digest presence/value, lock digest presence/value,
build-input digest presence/value,
module/function identity, source digest, compile-tool provider bindings, context,
and source location. Existing provider contract/content hashes stay in the key.
This invalidates entries when schema, lock, declarations, or captured values
change. Input bytes are not embedded in the value-cache snapshot.

`SZrParserSourceComptimeCache.inputSnapshot` remains the serialized value-cache
snapshot. It is not the project input snapshot; capture and digest happen before
`PrepareBuildFactsWithCache` imports V005/V006 entries.

## 7. Diagnostics and atomicity

Stable diagnostic IDs:

~~~text
format.requires_literal
format.unmatched_open_brace
format.unmatched_close_brace
format.invalid_spec
format.unknown_type
format.duplicate_argument
format.unknown_argument
format.argument_index
format.count_type
format.count_negative
format.unsupported_spec
format.protocol_return_type
compile.input.snapshot_unavailable
compile.input.undeclared
compile.input.file_missing
compile.input.file_encoding
compile.input.digest
project.build_inputs.shape
project.build_inputs.path
project.build_inputs.duplicate
project.build_inputs.read
project.build_inputs.digest
~~~

Manifest parsing, capture, digest construction, and cache import/export are
transactional. Malformed declarations, read/allocation failures, digest errors,
or corrupt snapshots leave prior project/cache state untouched.

## 8. Tests and exact touchpoints

### 8.1 Parser/compiler/runtime

`tests/parser/test_expression_fragment_parser.c`: direct-literal enforcement,
implicit/explicit/named/captured resolution, escaped braces, every spec field,
Unicode fill, dynamic counts, spans, and all format diagnostics.

`tests/parser/test_compiler_features.c`: one template constant and one
evaluation slot per source expression/count, including repeated references and
side-effecting positional, named, capture, width, and precision expressions.

`tests/parser/test_instruction_execution.c`: default conversion of null/bool/
numbers/string/custom `ZR_META_TO_STRING`; named/positional/captured and brace
escaping; scalar-count alignment/fill/precision; numeric forms/sign/prefix/zero
pad; dynamic counts; once-only order; IFormattable success, non-string return,
missing protocol, and runtime-only formatter.

### 8.2 Comptime and cache

`tests/compileTime/test_compile_time_execution.c`: pure literal folding,
non-pure formatter remains runtime-only, malformed diagnostics, and folded
string crossing runtime.

`tests/compileTime/test_comptime_runtime_contract.c`: identical snapshots hit;
changed file, environment value, manifest declaration, lock digest, and provider
contract miss; present-empty differs from missing.

`tests/compileTime/test_comptime_cache_snapshot_cases.h`: V006 string records,
canonical ordering, V005 import compatibility, corrupt/truncated variable
records, invalid UTF-8, and atomic replacement. Keep existing V005 assertions
until retirement.

### 8.3 Manifest and CLI

`tests/library/test_project_manifest_v2.c`: buildInputs parse/write roundtrip,
order-independent output, duplicate/absolute/escape/NUL/unknown-key rejection,
schema parity, capture of empty/non-empty/missing file/env values, and failed
recapture preserving the prior snapshot.

`tests/cli/test_cli_project_incremental.c` and
`tests/cli/test_cli_comptime_cache_incremental_cases.h`: compile a project
declaring one file and one environment; mutate file; present-to-empty,
empty-to-missing, and missing-to-present environment transitions; reorder arrays
without changing values and assert a hit; undeclared lookup diagnostic; capture
before first source parse and no reread during imports.

Stress/boundary tests require long Unicode strings, zero/max width, count
overflow/negative, nested source braces, repeated placeholders, GC pressure,
malformed V005/V006 snapshots, and source/runtime parity. Rust evidence is kept
in `lua/rust/compiler/rustc_parse_format/src/tests.rs`,
`lua/rust/tests/ui/macros/format-parse-errors.rs`, and format temporary tests.

## 9. Gates and implementation order

1. Lock AST/template parser and diagnostics.
2. Add slot lowering and runtime renderer; pass parser/compiler/execution tests.
3. Add FormatSpec/IFormattable registry and pure-descriptor checks; pass folding.
4. Add manifest declarations, schema/parser/writer, project capture, input APIs,
   and digest tests.
5. Integrate manifest/lock/input digests into cache keys and finish V006/CLI
   invalidation tests.

Root owns CMake and cross-toolchain execution. No milestone commit is complete
until parser, runtime, comptime, manifest, cache, and CLI gates are green; the
unrelated dirty worktree must remain untouched.
