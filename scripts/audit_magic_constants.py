#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class AuditRule:
    name: str
    path: str
    pattern: str
    target: str
    layer: str
    meaning: str


@dataclass(frozen=True)
class Exemption:
    scope: str
    reason: str


MIGRATED_RULES: tuple[AuditRule, ...] = (
    AuditRule(
        name="global api cache legacy macros",
        path="zr_vm_core/include/zr_vm_core/global.h",
        pattern=r"ZR_GLOBAL_API_STR_CACHE_N|ZR_GLOBAL_API_STR_CACHE_M",
        target="ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT / ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH",
        layer="common",
        meaning="Global API string cache bucket geometry must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="runtime init-size legacy macros",
        path="zr_vm_core/include/zr_vm_core/global.h",
        pattern=r"ZR_STRING_TABLE_INIT_SIZE_LOG2|ZR_OBJECT_TABLE_INIT_SIZE_LOG2",
        target="ZR_STRING_TABLE_INITIAL_SIZE_LOG2 / ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2",
        layer="common",
        meaning="String/object table initial sizes must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="stack native call legacy macro",
        path="zr_vm_core/include/zr_vm_core/stack.h",
        pattern=r"ZR_STACK_NATIVE_CALL_MIN",
        target="ZR_STACK_NATIVE_CALL_RESERVED_MIN",
        layer="common",
        meaning="Native call stack reserve must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="gc internal legacy macros",
        path="zr_vm_core/src/zr_vm_core/gc_internal.h",
        pattern=r"ZR_WORK_TO_MEM|ZR_GC_SWEEP_MAX|ZR_GC_FIN_MAX|ZR_GC_FINALIZE_COST",
        target="zr_gc_internal_conf.h",
        layer="common",
        meaning="GC budgets and finalize work costs must come from zr_gc_internal_conf.h.",
    ),
    AuditRule(
        name="gc raw duplicate scan limit",
        path="zr_vm_core/src/zr_vm_core/gc_mark.c",
        pattern=r"maxCheckCount\s*=\s*10000|maxIterations\s*=\s*10000",
        target="ZR_GC_GRAY_LIST_DUPLICATE_SCAN_LIMIT / ZR_GC_PROPAGATE_ALL_ITERATION_LIMIT",
        layer="common",
        meaning="GC guard limits must not remain as raw 10000 literals in gc_mark.c.",
    ),
    AuditRule(
        name="gc raw cycle guard limits",
        path="zr_vm_core/src/zr_vm_core/gc_cycle.c",
        pattern=r"maxIterations\s*=\s*10000|maxSweepIterations\s*=\s*10000",
        target="ZR_GC_RUN_UNTIL_STATE_ITERATION_LIMIT / ZR_GC_GENERATIONAL_FULL_SWEEP_ITERATION_LIMIT",
        layer="common",
        meaning="GC cycle guard limits must not remain as raw 10000 literals in gc_cycle.c.",
    ),
    AuditRule(
        name="runtime read-all fallback capacity",
        path="zr_vm_core/src/zr_vm_core/module_loader.c",
        pattern=r"\b4096\b",
        target="ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY",
        layer="common",
        meaning="Module loader fallback capacity must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="core io source patch feature gates",
        path="zr_vm_core/src/zr_vm_core/io.c",
        pattern=r"sourceVersionPatch\s*>=\s*(2|3|4|5|7)\b",
        target="ZR_IO_SOURCE_PATCH_HAS_*",
        layer="common",
        meaning="Binary source-reader feature gates must come from named patch-version constants in zr_io_conf.h.",
    ),
    AuditRule(
        name="parser semir raw deopt none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_semir.c",
        pattern=r"TZrUInt32\s+deoptId\s*=\s*0\s*;",
        target="ZR_RUNTIME_SEMIR_DEOPT_ID_NONE",
        layer="common",
        meaning="SemIR metadata should use the shared no-deopt sentinel instead of a raw 0 literal.",
    ),
    AuditRule(
        name="parser quickening raw deopt none fallback",
        path="zr_vm_parser/src/zr_vm_parser/compiler_quickening.c",
        pattern=r"compiler_quickening_find_deopt_id\s*\([^)]*\)\s*\{[\s\S]*?return\s+0\s*;[\s\S]*?return\s+0\s*;",
        target="ZR_RUNTIME_SEMIR_DEOPT_ID_NONE",
        layer="common",
        meaning="Quickening must reuse the shared no-deopt sentinel when a SemIR instruction has no deopt mapping.",
    ),
    AuditRule(
        name="parser writer source-header widths",
        path="zr_vm_parser/src/zr_vm_parser/writer.c",
        pattern=r"fwrite\(ZR_IO_SOURCE_SIGNATURE,\s*1,\s*4,\s*file\)|opt\s*\[\s*3\s*\]|fwrite\(opt,\s*sizeof\(TZrUInt8\),\s*3,\s*file\)",
        target="ZR_IO_SOURCE_SIGNATURE_LENGTH / ZR_IO_SOURCE_HEADER_OPT_BYTES",
        layer="common",
        meaning="Writer source-header signature and opt-field widths must come from named IO protocol constants in zr_io_conf.h.",
    ),
    AuditRule(
        name="parser import-metadata fallback capacity",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c",
        pattern=r"\b4096\b",
        target="ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY",
        layer="common",
        meaning="Import metadata fallback capacity must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="lexer initial buffer raw literal",
        path="zr_vm_parser/src/zr_vm_parser/lexer.c",
        pattern=r"ZR_LEXER_BUFFER_INIT_SIZE|bufferSize\s*=\s*256\b",
        target="ZR_PARSER_LEXER_BUFFER_INITIAL_SIZE",
        layer="common",
        meaning="Lexer initial buffer size must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser recovery raw literals",
        path="zr_vm_parser/src/zr_vm_parser/parser.c",
        pattern=r"\bMAX_CONSECUTIVE_ERRORS\b|\bMAX_SKIP_TOKENS\b|errorCount\s*>=\s*10\b|skipCount\s*<\s*100\b",
        target="ZR_PARSER_MAX_CONSECUTIVE_ERRORS / ZR_PARSER_MAX_RECOVERY_SKIP_TOKENS",
        layer="common",
        meaning="Parser recovery thresholds must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="compile-time projection depth raw literals",
        path="zr_vm_parser/src/zr_vm_parser/compiler_instruction.c",
        pattern=r"ZR_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH|depth\s*>\s*64\b|visitedCount\s*>=\s*64\b|visitedObjects\s*\[\s*64\s*\]",
        target="ZR_PARSER_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH",
        layer="common",
        meaning="Compile-time runtime-safety depth must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="native runtime abi local definition",
        path="zr_vm_library/include/zr_vm_library/native_binding.h",
        pattern=r"#define\s+ZR_VM_NATIVE_RUNTIME_ABI_VERSION\b",
        target="ZR_VM_NATIVE_RUNTIME_ABI_VERSION",
        layer="common",
        meaning="Native runtime ABI version must live in zr_abi_conf.h.",
    ),
    AuditRule(
        name="native plugin abi local definition",
        path="zr_vm_library/include/zr_vm_library/native_registry.h",
        pattern=r"#define\s+ZR_VM_NATIVE_PLUGIN_ABI_VERSION\b",
        target="ZR_VM_NATIVE_PLUGIN_ABI_VERSION",
        layer="common",
        meaning="Native plugin ABI version must live in zr_abi_conf.h.",
    ),
    AuditRule(
        name="native module info local definition",
        path="zr_vm_core/include/zr_vm_core/module.h",
        pattern=r"#define\s+ZR_NATIVE_MODULE_INFO_VERSION\b",
        target="ZR_NATIVE_MODULE_INFO_VERSION",
        layer="common",
        meaning="Native module metadata version must live in zr_abi_conf.h.",
    ),
    AuditRule(
        name="cli manifest version local definition",
        path="zr_vm_cli/src/zr_vm_cli/project.h",
        pattern=r"#define\s+ZR_CLI_MANIFEST_VERSION\b",
        target="ZR_CLI_MANIFEST_VERSION",
        layer="common",
        meaning="CLI manifest version must live in zr_abi_conf.h.",
    ),
    AuditRule(
        name="library project raw module extensions",
        path="zr_vm_library/src/zr_vm_library/project.c",
        pattern=r"\"\\.zro\"|\"\\.zr\"|length\s*-\s*4\b|length\s*-\s*3\b",
        target="ZR_VM_SOURCE_MODULE_FILE_EXTENSION / ZR_VM_BINARY_MODULE_FILE_EXTENSION",
        layer="common",
        meaning="Library project extension stripping must come from zr_path_conf.h.",
    ),
    AuditRule(
        name="cli project raw module extensions and mkdir mode",
        path="zr_vm_cli/src/zr_vm_cli/project.c",
        pattern=r"\"\\.zro\"|\"\\.zr\"|\"\\.zri\"|\b0755\b",
        target="zr_path_conf.h",
        layer="common",
        meaning="CLI path extensions and POSIX mkdir mode must come from zr_path_conf.h.",
    ),
    AuditRule(
        name="system fs local path max define",
        path="zr_vm_lib_system/src/zr_vm_lib_system/fs.c",
        pattern=r"#define\s+ZR_SYSTEM_FS_PATH_MAX\b|mkdir\(path,\s*0755\)",
        target="ZR_SYSTEM_FS_PATH_MAX / ZR_SYSTEM_FS_DIRECTORY_MODE",
        layer="module",
        meaning="zr.system.fs path/mode constants must come from zr_vm_lib_system/conf.h.",
    ),
    AuditRule(
        name="ffi error buffers in runtime.c",
        path="zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c",
        pattern=r"errorBuffer\s*\[\s*256\s*\]",
        target="ZR_FFI_ERROR_BUFFER_LENGTH",
        layer="module",
        meaning="zr.ffi error buffers must come from zr_vm_lib_ffi/conf.h.",
    ),
    AuditRule(
        name="ffi callback last-error buffer",
        path="zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime_internal.h",
        pattern=r"lastErrorMessage\s*\[\s*256\s*\]",
        target="ZR_FFI_ERROR_BUFFER_LENGTH",
        layer="module",
        meaning="zr.ffi callback error buffers must come from zr_vm_lib_ffi/conf.h.",
    ),
    AuditRule(
        name="ffi support diagnostic message buffer",
        path="zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime_support.c",
        pattern=r"message\s*\[\s*512\s*\]",
        target="ZR_FFI_DIAGNOSTIC_BUFFER_LENGTH",
        layer="module",
        meaning="zr.ffi internal diagnostic formatting buffers must come from zr_vm_lib_ffi/conf.h.",
    ),
    AuditRule(
        name="math local pi/epsilon definitions",
        path="zr_vm_lib_math/include/zr_vm_lib_math/math_common.h",
        pattern=r"\bM_PI\b|#define\s+ZR_MATH_EPSILON\b",
        target="ZR_MATH_PI / ZR_MATH_EPSILON",
        layer="module",
        meaning="zr.math scalar constants must come from zr_vm_lib_math/conf.h.",
    ),
    AuditRule(
        name="math raw constants in module descriptors",
        path="zr_vm_lib_math/src/zr_vm_lib_math/module.c",
        pattern=r"\bM_PI\b|2\.71828182845904523536",
        target="ZR_MATH_PI / ZR_MATH_TAU / ZR_MATH_E",
        layer="module",
        meaning="zr.math exported constant table should reuse module conf constants.",
    ),
    AuditRule(
        name="math raw M_PI in scalar conversion helpers",
        path="zr_vm_lib_math/src/zr_vm_lib_math/scalar.c",
        pattern=r"\bM_PI\b",
        target="ZR_MATH_PI",
        layer="module",
        meaning="zr.math radians/degrees conversion should reuse module conf constants.",
    ),
    AuditRule(
        name="math string formatting buffer",
        path="zr_vm_lib_math/src/zr_vm_lib_math/common.c",
        pattern=r"buffer\s*\[\s*512\s*\]",
        target="ZR_MATH_FORMAT_BUFFER_LENGTH",
        layer="module",
        meaning="zr.math shared string formatting buffers must come from zr_vm_lib_math/conf.h.",
    ),
    AuditRule(
        name="container module capacity and hash mixing constants",
        path="zr_vm_lib_container/src/zr_vm_lib_container/module.c",
        pattern=r"capacity\s*=\s*4;|capacity\s*\*\s*2|16777619ULL|31ULL",
        target="zr_vm_lib_container/conf.h",
        layer="module",
        meaning="zr.container capacity growth and hash mixing must come from zr_vm_lib_container/conf.h.",
    ),
    AuditRule(
        name="library native registry capacities",
        path="zr_vm_library/src/zr_vm_library/native_binding.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&registry->moduleRecords,\s*sizeof\(ZrLibRegisteredModuleRecord\),\s*8\)|"
        r"ZrCore_Array_Init\(state,\s*&registry->bindingEntries,\s*sizeof\(ZrLibBindingEntry\),\s*64\)|"
        r"ZrCore_Array_Init\(state,\s*&registry->pluginHandles,\s*sizeof\(void \*\),\s*4\)",
        target="zr_vm_library/conf.h",
        layer="module",
        meaning="zr.vm.library native registry capacities must come from zr_vm_library/conf.h.",
    ),
    AuditRule(
        name="library native inline argument capacity",
        path="zr_vm_library/src/zr_vm_library/native_binding_dispatch.c",
        pattern=r"inlineArguments\s*\[\s*8\s*\]|argumentCount\s*<=\s*8\b",
        target="ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY",
        layer="module",
        meaning="Native binding inline argument threshold must come from zr_vm_library/conf.h.",
    ),
    AuditRule(
        name="library native registry error buffer",
        path="zr_vm_library/src/zr_vm_library/native_binding_internal.h",
        pattern=r"lastErrorMessage\s*\[\s*512\s*\]",
        target="ZR_LIBRARY_NATIVE_REGISTRY_ERROR_BUFFER_LENGTH",
        layer="module",
        meaning="Native registry last-error buffer must come from zr_vm_library/conf.h.",
    ),
    AuditRule(
        name="cli app error buffer",
        path="zr_vm_cli/src/zr_vm_cli/app.c",
        pattern=r"error\s*\[\s*512\s*\]",
        target="ZR_CLI_ERROR_BUFFER_LENGTH",
        layer="module",
        meaning="CLI command-parse error buffers must come from zr_vm_cli/conf.h.",
    ),
    AuditRule(
        name="cli compiler source-hash, error, and collection capacities",
        path="zr_vm_cli/src/zr_vm_cli/compiler.c",
        pattern=r"sourceHash\s*\[\s*32\s*\]|error\s*\[\s*512\s*\]|"
        r"collection->capacity\s*==\s*0\s*\?\s*8\s*:\s*collection->capacity\s*\*\s*2|"
        r"manifest->capacity\s*==\s*0\s*\?\s*8\s*:\s*manifest->capacity\s*\*\s*2",
        target="zr_vm_cli/conf.h",
        layer="module",
        meaning="CLI compiler hash buffers and collection capacities must come from zr_vm_cli/conf.h.",
    ),
    AuditRule(
        name="cli manifest source-hash buffer",
        path="zr_vm_cli/src/zr_vm_cli/project.h",
        pattern=r"sourceHash\s*\[\s*32\s*\]",
        target="ZR_CLI_SOURCE_HASH_HEX_LENGTH",
        layer="module",
        meaning="CLI manifest source-hash buffers must come from zr_vm_cli/conf.h.",
    ),
    AuditRule(
        name="cli project capacities",
        path="zr_vm_cli/src/zr_vm_cli/project.c",
        pattern=r"manifest->capacity\s*==\s*0\s*\?\s*8\s*:\s*manifest->capacity\s*\*\s*2|"
        r"list->capacity\s*==\s*0\s*\?\s*4\s*:\s*list->capacity\s*\*\s*2",
        target="zr_vm_cli/conf.h",
        layer="module",
        meaning="CLI manifest and string-list capacities must come from zr_vm_cli/conf.h.",
    ),
    AuditRule(
        name="cli repl buffers",
        path="zr_vm_cli/src/zr_vm_cli/repl.c",
        pattern=r"line\s*\[\s*1024\s*\]|bufferCapacity\s*==\s*0\s*\?\s*256\s*:\s*bufferCapacity\s*\*\s*2",
        target="zr_vm_cli/conf.h",
        layer="module",
        meaning="CLI REPL line and accumulation buffers must come from zr_vm_cli/conf.h.",
    ),
    AuditRule(
        name="lsp native generic limits",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c",
        pattern=r"#define\s+ZR_LSP_NATIVE_GENERIC_ARG_MAX\s+8|#define\s+ZR_LSP_NATIVE_GENERIC_TEXT_MAX\s+128",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="LSP native generic limits must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp interface path and array-init literals",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c",
        pattern=r"buffer\[4096\]|ZrCore_HashSet_Init\(state,\s*&context->uriToAnalyzerMap,\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*&context->projectIndexes,\s*sizeof\(SZrLspProjectIndex \*\),\s*2\)|"
        r"ZrCore_Array_Init\(state,\s*[^,]+,\s*sizeof\([^)]+\),\s*8\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="LSP interface buffer sizes, hash-table geometry, and array capacities must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp signature-help location packing literal",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c",
        pattern=r"\*\s*4096\s*\+",
        target="ZR_LSP_SIGNATURE_RANGE_PACK_BASE",
        layer="module",
        meaning="LSP signature-help line/column packing base must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp reference-tracker location packing literal",
        path="zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c",
        pattern=r"\*\s*4096\s*\+",
        target="ZR_LSP_SIGNATURE_RANGE_PACK_BASE",
        layer="module",
        meaning="Reference-tracker line/column span packing must reuse the named LSP range-pack base.",
    ),
    AuditRule(
        name="lsp reference-tracker local invalid span sentinel",
        path="zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c",
        pattern=r"return\s*\(TZrSize\)-1;",
        target="ZR_LSP_RANGE_SPAN_SCORE_INVALID",
        layer="module",
        meaning="Reference span scoring should use a file-local named invalid sentinel instead of raw (TZrSize)-1.",
    ),
    AuditRule(
        name="lsp stdio json-rpc and uri-cache literals",
        path="zr_vm_language_server/stdio/zr_vm_language_server_stdio.c",
        pattern=r"server->uriCache\.capacity\s*==\s*0\s*\?\s*8|-32700|-32600",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="LSP stdio cache geometry and JSON-RPC error codes must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp incremental parser local sizes and hash multiplier",
        path="zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&fileVersion->parserDiagnostics,\s*sizeof\(SZrDiagnostic \*\),\s*4\)|"
        r"ZrCore_HashSet_Init\(state,\s*&parser->uriToFileMap,\s*4\)|hashValue\s*=\s*hashValue\s*\*\s*31",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Incremental parser capacities, hash-table geometry, and hash multiplier must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic analyzer local array capacities",
        path="zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&analyzer->diagnostics,\s*sizeof\(SZrDiagnostic \*\),\s*8\)|"
        r"ZrCore_Array_Init\(state,\s*&analyzer->cache->cachedDiagnostics,\s*sizeof\(SZrDiagnostic \*\),\s*8\)|"
        r"ZrCore_Array_Init\(state,\s*&analyzer->cache->cachedSymbols,\s*sizeof\(SZrSymbol \*\),\s*8\)|"
        r"ZrCore_Array_Init\(state,\s*result,\s*sizeof\(SZrCompletionItem \*\),\s*8\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Semantic analyzer array capacities must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic analyzer support hash multiplier",
        path="zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_support.c",
        pattern=r"hash\s*=\s*hash\s*\*\s*31",
        target="ZR_LSP_HASH_MULTIPLIER",
        layer="module",
        meaning="Semantic analyzer recursive hash mixing must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp symbol table local sizes and hash geometry",
        path="zr_vm_language_server/src/zr_vm_language_server/symbol_table.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&table->scopeStack,\s*sizeof\(SZrSymbolScope \*\),\s*8\)|"
        r"ZrCore_Array_Init\(state,\s*&table->allScopes,\s*sizeof\(SZrSymbolScope \*\),\s*8\)|"
        r"ZrCore_HashSet_Init\(state,\s*&table->nameToSymbolsHashSet,\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*&table->globalScope->symbols,\s*sizeof\(SZrSymbol \*\),\s*16\)|"
        r"ZrCore_Array_Init\(state,\s*&symbol->references,\s*sizeof\(SZrFileRange\),\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*symbolArray,\s*sizeof\(SZrSymbol \*\),\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*result,\s*sizeof\(SZrSymbol \*\),\s*(4|8)\)|"
        r"ZrCore_Array_Init\(state,\s*&newScope->symbols,\s*sizeof\(SZrSymbol \*\),\s*8\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Symbol-table array capacities and hash-table geometry must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp symbol table local position compare tri-state",
        path="zr_vm_language_server/src/zr_vm_language_server/symbol_table.c",
        pattern=r"return\s+-1;|return\s+1;",
        target="ZR_LSP_SYMBOL_POSITION_COMPARE_LESS / ZR_LSP_SYMBOL_POSITION_COMPARE_GREATER",
        layer="module",
        meaning="File-position compare helpers should use file-local named tri-state constants instead of raw -1/1.",
    ),
    AuditRule(
        name="lsp reference tracker local sizes and hash geometry",
        path="zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&tracker->allReferences,\s*sizeof\(SZrReference \*\),\s*16\)|"
        r"ZrCore_HashSet_Init\(state,\s*&tracker->symbolToReferencesMap,\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*refArray,\s*sizeof\(SZrReference \*\),\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*result,\s*sizeof\(SZrReference \*\),\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*result,\s*sizeof\(SZrFileRange\),\s*4\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Reference-tracker array capacities and hash-table geometry must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp project local array capacities",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_project.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&projectIndex->files,\s*sizeof\(SZrLspProjectFileRecord \*\),\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*&bindings,\s*sizeof\(SZrLspImportBinding \*\),\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*result,\s*sizeof\(SZrLspSymbolInformation \*\),\s*8\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Project-index helper array capacities must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp project local hex-digit invalid sentinel",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_project.c",
        pattern=r"return\s+-1;",
        target="ZR_LSP_PROJECT_HEX_DIGIT_INVALID",
        layer="module",
        meaning="Project URI decoding should use a file-local named invalid-hex sentinel instead of raw -1.",
    ),
    AuditRule(
        name="lsp project import helper array capacity",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_project_imports.c",
        pattern=r"ZrCore_Array_Init\(state,\s*result,\s*sizeof\(SZrLspLocation \*\),\s*4\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Project import helper array capacity must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp project feature helper array capacities",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&bindings,\s*sizeof\(SZrLspImportBinding \*\),\s*4\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Project feature helper array capacities must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp project navigation helper array capacities",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_project_navigation.c",
        pattern=r"ZrCore_Array_Init\(state,\s*result,\s*sizeof\(SZrLspLocation \*\),\s*4\)|"
        r"ZrCore_Array_Init\(state,\s*&references,\s*sizeof\(SZrReference \*\),\s*8\)|"
        r"ZrCore_Array_Init\(state,\s*&bindings,\s*sizeof\(SZrLspImportBinding \*\),\s*4\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Project navigation helper array capacities must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic token helper array capacity",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&bindings,\s*sizeof\(SZrLspImportBinding \*\),\s*4\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Semantic token helper array capacity must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp wasm exports array capacities",
        path="zr_vm_language_server/wasm/wasm_exports.cpp",
        pattern=r"ZrCore_Array_Init\(g_wasm_state,\s*[^,]+,\s*sizeof\([^)]+\),\s*8\)",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="WASM LSP export array capacities must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="parser compiler-state local capacities",
        path="zr_vm_parser/src/zr_vm_parser/compiler_state.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&cs->[A-Za-z0-9_]+,\s*sizeof\([^)]+\),\s*(4|8|16|32|64)\)",
        target="ZR_PARSER_INITIAL_CAPACITY_* / ZR_PARSER_INSTRUCTION_INITIAL_CAPACITY",
        layer="common",
        meaning="Compiler-state capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser compile-expression recursive depth literal",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression_types.c",
        pattern=r"depth\s*>\s*32\b",
        target="ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH",
        layer="common",
        meaning="Recursive member lookup depth guards must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser type-inference core buffers and depth literal",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_core.c",
        pattern=r"depth\s*>\s*32\b|sizeBuffer\s*\[\s*32\s*\]|elementTypeBuffer\s*\[\s*128\s*\]|"
        r"errorMessage\s*\[\s*256\s*\]|genericDiagnostic\s*\[\s*256\s*\]|errorMsg\s*\[\s*256\s*\]|"
        r"buffer\s*\[\s*512\s*\]|ZrCore_Array_Init\(state,\s*outArgumentTypeNames,\s*sizeof\(SZrString \*\),\s*2\)",
        target="zr_parser_conf.h",
        layer="common",
        meaning="Type inference recursion guards, buffers, and local capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser class prototype capacities and error buffer",
        path="zr_vm_parser/src/zr_vm_parser/compiler_class.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]|ZrCore_Array_Init\(cs->state,\s*&info\.(inherits|implements|members),\s*sizeof\([^)]+\),\s*(2|4|16)\)",
        target="zr_parser_conf.h",
        layer="common",
        meaning="Class prototype helper capacities and diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser struct formatting buffers and prototype capacities",
        path="zr_vm_parser/src/zr_vm_parser/compiler_struct.c",
        pattern=r"integerBuffer\s*\[\s*64\s*\]|buffer\s*\[\s*1024\s*\]|"
        r"ZrCore_Array_Init\(cs->state,\s*&info\.(inherits|implements|members),\s*sizeof\([^)]+\),\s*(2|4|16)\)",
        target="zr_parser_conf.h",
        layer="common",
        meaning="Struct formatting buffers and prototype capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser interface prototype capacities",
        path="zr_vm_parser/src/zr_vm_parser/compiler_interface.c",
        pattern=r"ZrCore_Array_Init\(cs->state,\s*&info\.(inherits|members),\s*sizeof\([^)]+\),\s*(4|8)\)",
        target="ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Interface prototype capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser extern declaration prototype capacities",
        path="zr_vm_parser/src/zr_vm_parser/compiler_extern_declaration.c",
        pattern=r"ZrCore_Array_Init\(cs->state,\s*&info\.(inherits|implements|members),\s*sizeof\([^)]+\),\s*(2|4|8)\)",
        target="zr_parser_conf.h",
        layer="common",
        meaning="Extern prototype capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser import-metadata prototype capacities",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&info->(inherits|implements|genericParameters|members),\s*sizeof\([^)]+\),\s*(2|4)\)",
        target="ZR_PARSER_INITIAL_CAPACITY_PAIR / ZR_PARSER_INITIAL_CAPACITY_TINY",
        layer="common",
        meaning="Import metadata prototype capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser native type prototype capacities and buffers",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_native.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&info->(inherits|implements|genericParameters|members),\s*sizeof\([^)]+\),\s*(2|8)\)|"
        r"ZrCore_Array_Init\(state,\s*&genericInfo\.constraintTypeNames,\s*sizeof\(SZrString \*\),\s*2\)|"
        r"errorMessage\s*\[\s*256\s*\]|genericDiagnostic\s*\[\s*256\s*\]|nestedBuffer\s*\[\s*128\s*\]",
        target="zr_parser_conf.h",
        layer="common",
        meaning="Native type prototype capacities and diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser type-system environment capacities",
        path="zr_vm_parser/src/zr_vm_parser/type_system.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&env->(variableTypes|functionReturnTypes|typeNames),\s*sizeof\([^)]+\),\s*8\)|"
        r"ZrCore_Array_Init\(state,\s*results,\s*sizeof\(SZrFunctionTypeInfo \*\),\s*4\)",
        target="ZR_PARSER_INITIAL_CAPACITY_SMALL / ZR_PARSER_INITIAL_CAPACITY_TINY",
        layer="common",
        meaning="Type-system environment capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser semantic context capacities",
        path="zr_vm_parser/src/zr_vm_parser/semantic.c",
        pattern=r"ZrCore_Array_Init\(context->state,\s*&context->(types|symbols|overloadSets|cleanupPlan|templateSegments),\s*sizeof\([^)]+\),\s*(4|8)\)|"
        r"ZrCore_Array_Init\(context->state,\s*&record\.members,\s*sizeof\(TZrSymbolId\),\s*4\)",
        target="ZR_PARSER_INITIAL_CAPACITY_SMALL / ZR_PARSER_INITIAL_CAPACITY_TINY",
        layer="common",
        meaning="Semantic context capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser compile-time executor buffers and bindings capacity",
        path="zr_vm_parser/src/zr_vm_parser/compile_time_executor_support.c",
        pattern=r"msg\s*\[\s*256\s*\]|ZrCore_Array_Init\(cs->state,\s*&frame->bindings,\s*sizeof\(SZrCompileTimeBinding\),\s*4\)",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH / ZR_PARSER_INITIAL_CAPACITY_TINY",
        layer="common",
        meaning="Compile-time executor buffers and frame capacities must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="core debug runtime error buffer",
        path="zr_vm_core/src/zr_vm_core/debug.c",
        pattern=r"errorBuffer\s*\[\s*1024\s*\]",
        target="ZR_RUNTIME_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Core runtime error buffers must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="core execution-dispatch runtime buffers",
        path="zr_vm_core/src/zr_vm_core/execution_dispatch.c",
        pattern=r"errorBuffer\s*\[\s*1024\s*\]|message\s*\[\s*256\s*\]",
        target="ZR_RUNTIME_ERROR_BUFFER_LENGTH / ZR_RUNTIME_DIAGNOSTIC_BUFFER_LENGTH",
        layer="common",
        meaning="Dispatch runtime buffers must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="core meta small text buffer",
        path="zr_vm_core/src/zr_vm_core/meta.c",
        pattern=r"buffer\s*\[\s*64\s*\]",
        target="ZR_RUNTIME_SMALL_TEXT_BUFFER_LENGTH",
        layer="common",
        meaning="Core meta fallback text buffers must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="core execution-control meta-call convention",
        path="zr_vm_core/src/zr_vm_core/execution_control.c",
        pattern=r"stableArguments\s*\[\s*2\s*\]|argumentCount\s*>\s*2\b|ReserveScratchSlots\(state,\s*1\s*\+\s*argumentCount\)|"
        r"metaBase\s*\+\s*1\b|metaBase\s*\+\s*2\b|stackTop\.valuePointer\s*=\s*metaBase\s*\+\s*1\s*\+\s*argumentCount",
        target="ZR_META_CALL_*",
        layer="common",
        meaning="Meta-call scratch layout, slot offsets, and max-argument convention must come from zr_meta_conf.h.",
    ),
    AuditRule(
        name="core execution-control exception-handler stack local capacities",
        path="zr_vm_core/src/zr_vm_core/execution_control.c",
        pattern=r"exceptionHandlerStackCapacity\s*>\s*0\s*\?\s*state->exceptionHandlerStackCapacity\s*:\s*8\b|newCapacity\s*\*=\s*2\b",
        target="ZR_EXCEPTION_HANDLER_STACK_INITIAL_CAPACITY / ZR_EXCEPTION_HANDLER_STACK_GROWTH_FACTOR",
        layer="module",
        meaning="Exception-handler stack growth should use file-local named capacity constants instead of raw 8/*2 literals.",
    ),
    AuditRule(
        name="core execution-control cleanup close-count none",
        path="zr_vm_core/src/zr_vm_core/execution_control.c",
        pattern=r"if\s*\(state == ZR_NULL \|\| cleanupCount == 0\)\s*\{\s*return 0;\s*\}|TZrSize\s+closedCount\s*=\s*0\s*;",
        target="ZR_SCOPE_CLEANUP_CLOSED_COUNT_NONE",
        layer="module",
        meaning="Scope cleanup helpers should use a local named zero-close-count sentinel instead of raw 0 returns and initializers.",
    ),
    AuditRule(
        name="core execution-dispatch meta-call arity literals",
        path="zr_vm_core/src/zr_vm_core/execution_dispatch.c",
        pattern=r"^\s*(1|2),\s*$",
        target="ZR_META_CALL_UNARY_ARGUMENT_COUNT / ZR_META_CALL_MAX_ARGUMENTS",
        layer="common",
        meaning="Execution-dispatch meta-call helper invocations must reuse the named unary/binary meta-call arity constants.",
    ),
    AuditRule(
        name="core meta self/other slot literals",
        path="zr_vm_core/src/zr_vm_core/meta.c",
        pattern=r"ZrCore_Stack_GetValue\(base \+ 1\)|ZrCore_Stack_GetValue\(base \+ 2\)|"
        r"ZrCore_Stack_CopyValue\(state,\s*base \+ 1,\s*&stableSelf\)|stackTop\.valuePointer\s*=\s*base \+ 2;",
        target="ZR_META_CALL_SELF_SLOT / ZR_META_CALL_SECOND_ARGUMENT_SLOT / ZR_META_CALL_STACK_TOP",
        layer="common",
        meaning="Builtin meta-method helpers must express self/other slot access through named meta-call ABI macros.",
    ),
    AuditRule(
        name="core value small text buffer",
        path="zr_vm_core/src/zr_vm_core/value.c",
        pattern=r"buffer\s*\[\s*64\s*\]",
        target="ZR_RUNTIME_SMALL_TEXT_BUFFER_LENGTH",
        layer="common",
        meaning="Core value fallback text buffers must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="core reflection formatting buffers",
        path="zr_vm_core/src/zr_vm_core/reflection.c",
        pattern=r"paramName\s*\[\s*32\s*\]|fallbackName\s*\[\s*32\s*\]|typeNameBuffer\s*\[\s*128\s*\]|"
        r"returnTypeBuffer\s*\[\s*128\s*\]|metaName\s*\[\s*128\s*\]|qualified(Name|MemberName)\s*\[\s*512\s*\]|"
        r"entryQualifiedName\s*\[\s*512\s*\]|buffer\s*\[\s*8192\s*\]",
        target="zr_runtime_limits_conf.h",
        layer="common",
        meaning="Reflection formatting buffers must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="core module prototype inherit capacity",
        path="zr_vm_core/src/zr_vm_core/module_prototype.c",
        pattern=r"ZrCore_Array_Init\(state,\s*&protoInfoData\.inheritTypeNames,\s*sizeof\(SZrString \*\),\s*4\)",
        target="ZR_RUNTIME_PROTOTYPE_INHERIT_INITIAL_CAPACITY",
        layer="common",
        meaning="Prototype inherit capacity must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="lsp interface-support recursion and buffers",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c",
        pattern=r"lineStarts\s*\[\s*32\s*\]|lineEnds\s*\[\s*32\s*\]|commentBuffer\s*\[\s*1024\s*\]|"
        r"markdownBuffer\s*\[\s*2048\s*\]|typeBuffer\s*\[\s*(128|256)\s*\]|buffer\s*\[\s*(512|1024)\s*\]|"
        r"baseName\s*\[\s*128\s*\]|memberName\s*\[\s*128\s*\]|receiverNameText\s*\[\s*128\s*\]|"
        r"receiverTypeText\s*\[\s*256\s*\]|specializedType\s*\[\s*256\s*\]|receiverTypeName\s*\[\s*256\s*\]|"
        r"depth\s*>\s*8\b|count\s*<\s*32\b",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="LSP interface helper recursion and buffer limits must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp project feature detail and buffer literals",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c",
        pattern=r"detail\s*\[\s*192\s*\]|typeBuffer\s*\[\s*128\s*\]|buffer\s*\[\s*(256|512)\s*\]",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Project feature detail and text buffers must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic analyzer hover and completion buffers",
        path="zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c",
        pattern=r"typeBuffer\s*\[\s*128\s*\]|signatureBuffer\s*\[\s*512\s*\]|label\s*\[\s*256\s*\]|"
        r"detail\s*\[\s*(160|192)\s*\]|moduleName\s*\[\s*256\s*\]|buffer\s*\[\s*1024\s*\]|"
        r"resolvedTypeBuffer\s*\[\s*128\s*\]",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Semantic analyzer hover/completion buffers must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic analyzer native-module completion depth",
        path="zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c",
        pattern=r"depth\s*>\s*4\b",
        target="ZR_LSP_NATIVE_MODULE_COMPLETION_MAX_DEPTH",
        layer="module",
        meaning="Native module descriptor completion should use a file-local named max-depth guard instead of raw 4.",
    ),
    AuditRule(
        name="lsp semantic-analyzer support recursion depth",
        path="zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_support.c",
        pattern=r"depth\s*>\s*32\b",
        target="ZR_LSP_AST_RECURSION_MAX_DEPTH",
        layer="module",
        meaning="Semantic analyzer AST recursion guards must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp signature-help buffers and recursion depth",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c",
        pattern=r"typeBuffer\s*\[\s*128\s*\]|buffer\s*\[\s*256\s*\]|integerBuffer\s*\[\s*64\s*\]|"
        r"labelBuffer\s*\[\s*512\s*\]|depth\s*>\s*32\b",
        target="zr_vm_language_server/conf.h",
        layer="module",
        meaning="Signature-help buffers and recursion guards must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp signature-help local binding-index sentinel",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c",
        pattern=r"return\s+-1;|bindingIndex\s*>=\s*0",
        target="ZR_LSP_SIGNATURE_BINDING_INDEX_NONE",
        layer="module",
        meaning="Signature-help generic binding helpers should use a local named not-found sentinel instead of raw -1 checks.",
    ),
    AuditRule(
        name="lsp incremental-parser hash buffer",
        path="zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c",
        pattern=r"hashStr\s*\[\s*32\s*\]",
        target="ZR_LSP_SHORT_TEXT_BUFFER_LENGTH",
        layer="module",
        meaning="Incremental-parser hash text buffers must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic-token local capacities",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c",
        pattern=r"ZrCore_Array_Init\(state,\s*(result|&entries),\s*sizeof\([^)]+\),\s*32\)",
        target="ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY",
        layer="module",
        meaning="Semantic-token initial capacities must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic-token local unknown and compare sentinels",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c",
        pattern=r"return\s+left->line\s*<\s*right->line\s*\?\s*-1\s*:\s*1;|"
        r"return\s+left->character\s*<\s*right->character\s*\?\s*-1\s*:\s*1;|"
        r"return\s+left->length\s*<\s*right->length\s*\?\s*-1\s*:\s*1;|"
        r"return\s+left->typeIndex\s*<\s*right->typeIndex\s*\?\s*-1\s*:\s*1;|"
        r"typeIndex\s*>=\s*0|return\s+-1;",
        target="ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN / ZR_LSP_SEMANTIC_TOKEN_COMPARE_*",
        layer="module",
        meaning="Semantic-token helper-local unknown sentinels and qsort compare results should not remain as raw -1/1 values.",
    ),
    AuditRule(
        name="lsp semantic-analyzer typecheck buffers",
        path="zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_typecheck.c",
        pattern=r"buffer\s*\[\s*(128|256)\s*\]",
        target="ZR_LSP_TYPE_BUFFER_LENGTH / ZR_LSP_TEXT_BUFFER_LENGTH",
        layer="module",
        meaning="Semantic-analyzer typecheck buffers must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp semantic-analyzer symbols error buffer",
        path="zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_symbols.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]",
        target="ZR_LSP_TEXT_BUFFER_LENGTH",
        layer="module",
        meaning="Semantic-analyzer symbol diagnostics must come from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="core callsite cache member-entry sentinel",
        path="zr_vm_core/include/zr_vm_core/function.h",
        pattern=r"#define\s+ZR_FUNCTION_CALLSITE_CACHE_MEMBER_ENTRY_NONE\b|0xFFFFFFFFu",
        target="ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE",
        layer="common",
        meaning="PIC cache miss sentinel must come from a shared runtime sentinel/cache conf header.",
    ),
    AuditRule(
        name="parser constant-reference step sentinels",
        path="zr_vm_parser/include/zr_vm_parser/compiler.h",
        pattern=r"ZR_CONSTANT_REF_STEP_PARENT\s*=\s*-1|ZR_CONSTANT_REF_STEP_CONSTANT_POOL\s*=\s*-2|"
        r"ZR_CONSTANT_REF_STEP_MODULE\s*=\s*-3|ZR_CONSTANT_REF_STEP_PROTOTYPE_INDEX\s*=\s*-4|"
        r"ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX\s*=\s*-5",
        target="shared constant-reference contract header",
        layer="common",
        meaning="Compiler constant-reference path step tags must move into one shared contract header.",
    ),
    AuditRule(
        name="core duplicated constant-reference step sentinels",
        path="zr_vm_core/src/zr_vm_core/constant_reference.c",
        pattern=r"#define\s+ZR_CONSTANT_REF_STEP_PARENT\b|#define\s+ZR_CONSTANT_REF_STEP_CONSTANT_POOL\b|"
        r"#define\s+ZR_CONSTANT_REF_STEP_MODULE\b|#define\s+ZR_CONSTANT_REF_STEP_PROTOTYPE_INDEX\b|"
        r"#define\s+ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX\b",
        target="shared constant-reference contract header",
        layer="common",
        meaning="Runtime constant-reference decoding must not duplicate parser step sentinels locally.",
    ),
    AuditRule(
        name="core invalid-pointer low-bound guard in gc_object",
        path="zr_vm_core/src/zr_vm_core/gc_object.c",
        pattern=r"0x1000",
        target="ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND",
        layer="common",
        meaning="Low-address invalid-pointer guard must come from a shared runtime sentinel header.",
    ),
    AuditRule(
        name="core gc-object ignore-registry local capacities",
        path="zr_vm_core/src/zr_vm_core/gc_object.c",
        pattern=r"ignoredObjectCapacity\s*>\s*0\s*\?\s*collector->ignoredObjectCapacity\s*:\s*8\b|newCapacity\s*\*=\s*2\b",
        target="ZR_GC_IGNORE_REGISTRY_INITIAL_CAPACITY / ZR_GC_IGNORE_REGISTRY_GROWTH_FACTOR",
        layer="module",
        meaning="GC ignore-registry growth should use file-local named capacity constants instead of raw 8/*2 literals.",
    ),
    AuditRule(
        name="core invalid-pointer low-bound guard in gc_sweep",
        path="zr_vm_core/src/zr_vm_core/gc_sweep.c",
        pattern=r"0x1000",
        target="ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND",
        layer="common",
        meaning="Low-address invalid-pointer guard must come from a shared runtime sentinel header.",
    ),
    AuditRule(
        name="core invalid-pointer low-bound guard in state teardown",
        path="zr_vm_core/src/zr_vm_core/state.c",
        pattern=r"0x1000",
        target="ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND",
        layer="common",
        meaning="State teardown must not hard-code the low-address invalid-pointer guard.",
    ),
    AuditRule(
        name="core reflection entry hash salt",
        path="zr_vm_core/src/zr_vm_core/reflection.c",
        pattern=r"0xE177u",
        target="ZR_RUNTIME_REFLECTION_ENTRY_HASH_SALT",
        layer="common",
        meaning="Module entry reflection hash salting must come from a named runtime hash constant.",
    ),
    AuditRule(
        name="core reflection member hash-band literals",
        path="zr_vm_core/src/zr_vm_core/reflection.c",
        pattern=r"\+\s*1u\)|\+\s*100u\)",
        target="ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE / ZR_RUNTIME_REFLECTION_METHOD_HASH_BASE",
        layer="common",
        meaning="Reflection member and method hash bands must come from named shared hash constants.",
    ),
    AuditRule(
        name="core gc managed-memory drift divisor",
        path="zr_vm_core/src/zr_vm_core/gc_cycle.c",
        pattern=r"estimatedMemories\s*/\s*10",
        target="ZR_GC_MANAGED_MEMORY_DRIFT_TOLERANCE_DIVISOR",
        layer="common",
        meaning="GC managed-memory drift reconciliation must use a named tolerance divisor from zr_gc_internal_conf.h.",
    ),
    AuditRule(
        name="core gc full-inc debt credit",
        path="zr_vm_core/src/zr_vm_core/gc_cycle.c",
        pattern=r"AddDebtSpace\(global,\s*-2000\)",
        target="ZR_GC_DEBT_CREDIT_BYTES",
        layer="common",
        meaning="GC full-inc cleanup debt credit must come from a named GC-internal budget constant.",
    ),
    AuditRule(
        name="core gc idle debt credit",
        path="zr_vm_core/src/zr_vm_core/gc.c",
        pattern=r"AddDebtSpace\(global,\s*-2000\)",
        target="ZR_GC_DEBT_CREDIT_BYTES",
        layer="common",
        meaning="GC idle-step debt credit must come from a named GC-internal budget constant.",
    ),
    AuditRule(
        name="core native-call error-handler overflow ratio",
        path="zr_vm_core/src/zr_vm_core/function.c",
        pattern=r"ZR_VM_MAX_NATIVE_CALL_STACK\s*\)\s*/\s*10\s*\*\s*11",
        target="ZR_NATIVE_CALL_STACK_ERROR_HANDLER_GUARD_DIVISOR / ZR_NATIVE_CALL_STACK_ERROR_HANDLER_GUARD_MULTIPLIER",
        layer="common",
        meaning="Native stack overflow escalation while handling errors must come from named runtime guard ratio constants.",
    ),
    AuditRule(
        name="core object inline-call argument capacity",
        path="zr_vm_core/src/zr_vm_core/object.c",
        pattern=r"inlineArguments\s*\[\s*8\s*\]|inlineArgumentPinAdded\s*\[\s*8\s*\]|argumentCount\s*<=\s*8\b",
        target="ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY",
        layer="common",
        meaning="Object-call inline scratch argument staging must come from a named runtime capacity limit.",
    ),
    AuditRule(
        name="lsp stdio json-rpc request error codes",
        path="zr_vm_language_server/stdio/stdio_requests.c",
        pattern=r"-32601|-32602",
        target="ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE / ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE",
        layer="module",
        meaning="stdio request dispatch must not hard-code JSON-RPC method-not-found and invalid-params error codes.",
    ),
    AuditRule(
        name="lsp stdio initialize sync-kind and trigger literals",
        path="zr_vm_language_server/stdio/stdio_requests.c",
        pattern=r"ZR_LSP_FIELD_CHANGE,\s*2\b|cJSON_CreateString\(\"\\.\"\)|cJSON_CreateString\(\":\"\)|"
        r"cJSON_CreateString\(\"\(\"\)|cJSON_CreateString\(\",\"\)",
        target="ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL / ZR_LSP_*_TRIGGER_CHARACTER_*",
        layer="module",
        meaning="stdio initialize capabilities must reuse named text-document sync enums and trigger-character constants.",
    ),
    AuditRule(
        name="lsp stdio transport protocol keys and header prefix",
        path="zr_vm_language_server/stdio/stdio_transport.c",
        pattern=r"\"jsonrpc\"|\"2\\.0\"|\"id\"|\"result\"|\"error\"|\"method\"|\"params\"|\"code\"|\"message\"|Content-Length:",
        target="ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX / ZR_LSP_JSON_RPC_FIELD_* / ZR_LSP_JSON_RPC_VERSION",
        layer="module",
        meaning="stdio transport must not reintroduce raw JSON-RPC field names, protocol version, or content-length header prefixes.",
    ),
    AuditRule(
        name="lsp stdio message envelope keys",
        path="zr_vm_language_server/stdio/zr_vm_language_server_stdio.c",
        pattern=r"\"method\"|\"id\"|\"params\"",
        target="ZR_LSP_JSON_RPC_FIELD_METHOD / ZR_LSP_JSON_RPC_FIELD_ID / ZR_LSP_JSON_RPC_FIELD_PARAMS",
        layer="module",
        meaning="stdio request parsing must reuse named JSON-RPC envelope field constants from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp stdio json payload field and markup literals",
        path="zr_vm_language_server/stdio/stdio_json.c",
        pattern=r"\"line\"|\"character\"|\"start\"|\"end\"|\"uri\"|\"range\"|\"name\"|\"kind\"|\"location\"|"
        r"\"containerName\"|\"severity\"|\"source\"|\"zr\"|\"message\"|\"code\"|\"label\"|\"detail\"|"
        r"\"markdown\"|\"value\"|\"documentation\"|\"insertText\"|\"snippet\"|\"insertTextFormat\"|"
        r"\"contents\"|\"parameters\"|\"signatures\"|\"activeSignature\"|\"activeParameter\"",
        target="ZR_LSP_FIELD_* / ZR_LSP_MARKUP_KIND_MARKDOWN / ZR_LSP_INSERT_TEXT_FORMAT_KIND_SNIPPET / ZR_LSP_DIAGNOSTIC_SOURCE_NAME",
        layer="module",
        meaning="stdio JSON serializers and parsers must reuse named LSP payload field, markup-kind, and diagnostic-source constants.",
    ),
    AuditRule(
        name="lsp stdio completion insert-text-format numeric literals",
        path="zr_vm_language_server/stdio/stdio_json.c",
        pattern=r"insertTextFormat\s*=\s*1\b|insertTextFormat\s*=\s*2\b",
        target="ZR_LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT / ZR_LSP_INSERT_TEXT_FORMAT_SNIPPET",
        layer="module",
        meaning="Completion serialization must reuse named LSP insert-text-format protocol enums.",
    ),
    AuditRule(
        name="lsp stdio document field and notification literals",
        path="zr_vm_language_server/stdio/stdio_documents.c",
        pattern=r"\"text\"|\"range\"|\"uri\"|\"version\"|\"diagnostics\"|\"textDocument\"|\"position\"|"
        r"\"textDocument/publishDiagnostics\"",
        target="ZR_LSP_FIELD_* / ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS",
        layer="module",
        meaning="stdio document sync helpers must reuse named LSP document field keys and publishDiagnostics method constants.",
    ),
    AuditRule(
        name="lsp stdio document local hex-digit invalid sentinel",
        path="zr_vm_language_server/stdio/stdio_documents.c",
        pattern=r"return\s+-1;",
        target="ZR_LSP_STDIO_HEX_DIGIT_INVALID",
        layer="module",
        meaning="stdio URI decoding should use a file-local named invalid-hex sentinel instead of raw -1.",
    ),
    AuditRule(
        name="lsp stdio request payload field literals",
        path="zr_vm_language_server/stdio/stdio_requests.c",
        pattern=r"\"range\"|\"newText\"|\"changes\"|\"textDocument\"|\"text\"|\"version\"|\"contentChanges\"|"
        r"\"context\"|\"includeDeclaration\"|\"query\"|\"tokenTypes\"|\"tokenModifiers\"|\"data\"|"
        r"\"placeholder\"|\"newName\"|\"openClose\"|\"change\"|\"includeText\"|\"save\"|"
        r"\"resolveProvider\"|\"triggerCharacters\"|\"prepareProvider\"|\"textDocumentSync\"|"
        r"\"completionProvider\"|\"hoverProvider\"|\"signatureHelpProvider\"|\"definitionProvider\"|"
        r"\"referencesProvider\"|\"renameProvider\"|\"documentSymbolProvider\"|"
        r"\"workspaceSymbolProvider\"|\"documentHighlightProvider\"|\"legend\"|\"full\"|"
        r"\"semanticTokensProvider\"|\"supported\"|\"changeNotifications\"|\"workspaceFolders\"|"
        r"\"workspace\"|\"capabilities\"|\"serverInfo\"|\"name\"",
        target="ZR_LSP_FIELD_* / ZR_LSP_SERVER_NAME / ZR_LSP_SERVER_VERSION",
        layer="module",
        meaning="stdio request handlers must reuse named LSP request/response payload field and server metadata constants.",
    ),
    AuditRule(
        name="lsp stdio request and notification method literals",
        path="zr_vm_language_server/stdio/stdio_requests.c",
        pattern=r"\"initialize\"|\"shutdown\"|\"initialized\"|\"exit\"|\"\$/cancelRequest\"|"
        r"\"textDocument/completion\"|\"textDocument/hover\"|\"textDocument/signatureHelp\"|"
        r"\"textDocument/definition\"|\"textDocument/references\"|\"textDocument/documentSymbol\"|"
        r"\"workspace/symbol\"|\"textDocument/documentHighlight\"|\"textDocument/semanticTokens/full\"|"
        r"\"textDocument/prepareRename\"|\"textDocument/rename\"|\"workspace/didChangeConfiguration\"|"
        r"\"workspace/didChangeWatchedFiles\"|\"workspace/didChangeWorkspaceFolders\"|"
        r"\"textDocument/didOpen\"|\"textDocument/didChange\"|\"textDocument/didClose\"|\"textDocument/didSave\"",
        target="ZR_LSP_METHOD_*",
        layer="module",
        meaning="stdio request and notification dispatch must reuse named LSP method constants from zr_vm_language_server/conf.h.",
    ),
    AuditRule(
        name="lsp completion-item kind raw protocol codes",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c",
        pattern=r"kindValue\s*=\s*(1|2|3|5|6|7|8|9|10|13|21|22)\s*;",
        target="EZrLspCompletionItemKind",
        layer="module",
        meaning="Completion item kind mappings should be inventoried behind named LSP protocol constants or enums.",
    ),
    AuditRule(
        name="lsp completion plain-text format literal",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c",
        pattern=r"ZrCore_String_Create\(\s*state,\s*\"plaintext\"\s*,\s*9\s*\)",
        target="ZR_LSP_INSERT_TEXT_FORMAT_KIND_PLAINTEXT",
        layer="module",
        meaning="Completion-item plain-text insertTextFormat tags should reuse a named LSP format string constant.",
    ),
    AuditRule(
        name="lsp symbol kind raw protocol codes",
        path="zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c",
        pattern=r"return\s+(2|5|6|7|8|10|11|12|13|22|23)\s*;",
        target="EZrLspSymbolKind",
        layer="module",
        meaning="Symbol kind mappings still encode raw LSP protocol values directly in helper logic.",
    ),
    AuditRule(
        name="parser type-inference raw diagnostic and type-name buffers",
        path="zr_vm_parser/src/zr_vm_parser/type_inference.c",
        pattern=r"errorMsg\s*\[\s*(128|256|512)\s*\]|expectedTypeStr\s*\[\s*128\s*\]|actualTypeStr\s*\[\s*128\s*\]|"
        r"elementBuffer\s*\[\s*128\s*\]|nameBuffer\s*\[\s*160\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH / ZR_PARSER_TEXT_BUFFER_LENGTH / ZR_PARSER_TYPE_NAME_BUFFER_LENGTH / ZR_PARSER_DETAIL_BUFFER_LENGTH",
        layer="common",
        meaning="Type-inference diagnostics and array-type formatting helpers must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser type-inference integer range literals",
        path="zr_vm_parser/src/zr_vm_parser/type_inference.c",
        pattern=r"\*minValue\s*=\s*-128;|\*maxValue\s*=\s*127;|\*minValue\s*=\s*-32768;|\*maxValue\s*=\s*32767;|"
        r"\*minValue\s*=\s*-2147483648LL;|\*maxValue\s*=\s*2147483647LL;|\*maxValue\s*=\s*255;|"
        r"\*maxValue\s*=\s*65535;|\*maxValue\s*=\s*4294967295LL;|\*maxValue\s*=\s*18446744073709551615ULL;",
        target="ZR_TYPE_RANGE_*",
        layer="common",
        meaning="Integer literal range bounds must come from named zr_type_conf.h constants rather than raw per-type limits.",
    ),
    AuditRule(
        name="parser-state raw error, snippet, and module-path buffers",
        path="zr_vm_parser/src/zr_vm_parser/parser_state.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]|buffer\s*\[\s*1024\s*\]|snippet\s*\[\s*128\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH / ZR_PARSER_DECLARATION_BUFFER_LENGTH / ZR_PARSER_TYPE_NAME_BUFFER_LENGTH",
        layer="common",
        meaning="Parser-state diagnostics, normalized module-path assembly, and displayed snippets must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser writer-intermediate raw type buffers",
        path="zr_vm_parser/src/zr_vm_parser/writer_intermediate.c",
        pattern=r"(typeBuffer|valueTypeBuffer|paramBuffer|returnTypeBuffer)\s*\[\s*128\s*\]",
        target="ZR_PARSER_TYPE_NAME_BUFFER_LENGTH",
        layer="common",
        meaning="Writer intermediate metadata formatting must reuse parser type-name buffer constants.",
    ),
    AuditRule(
        name="parser extern helper synthetic-name buffer",
        path="zr_vm_parser/src/zr_vm_parser/compiler_extern_helpers.c",
        pattern=r"buffer\s*\[\s*96\s*\]",
        target="ZR_PARSER_GENERATED_NAME_BUFFER_LENGTH",
        layer="common",
        meaning="Compiler extern helper synthetic-name buffers must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser statement helper raw buffers",
        path="zr_vm_parser/src/zr_vm_parser/compile_statement.c",
        pattern=r"buffer\s*\[\s*64\s*\]|errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_GENERATED_NAME_BUFFER_LENGTH / ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Statement lowering helper-name and diagnostic buffers must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser generic-call helper buffers",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_generic_calls.c",
        pattern=r"detailBuffer\s*\[\s*256\s*\]|integerBuffer\s*\[\s*64\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH / ZR_PARSER_INTEGER_BUFFER_LENGTH",
        layer="common",
        meaning="Generic-call diagnostics and const-generic integer formatting must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser passing-mode diagnostic detail buffer",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_passing_modes.c",
        pattern=r"errorBuffer\s*\[\s*160\s*\]",
        target="ZR_PARSER_DETAIL_BUFFER_LENGTH",
        layer="common",
        meaning="Passing-mode diagnostics must use a named parser detail-buffer constant.",
    ),
    AuditRule(
        name="parser closure identifier collection capacity",
        path="zr_vm_parser/src/zr_vm_parser/compiler_closure.c",
        pattern=r"ZrCore_Array_Init\(cs->state,\s*&identifierNames,\s*sizeof\(SZrString \*\),\s*16\)",
        target="ZR_PARSER_INITIAL_CAPACITY_MEDIUM",
        layer="common",
        meaning="Closure external-variable identifier collection should reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser binding parameter-type capacity",
        path="zr_vm_parser/src/zr_vm_parser/compiler_bindings.c",
        pattern=r"ZrCore_Array_Init\(cs->state,\s*&paramTypes,\s*sizeof\(SZrInferredType\),\s*8\)",
        target="ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Binding parameter-type staging should reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser primary-expression helper buffers and capacities",
        path="zr_vm_parser/src/zr_vm_parser/parser_expression_primary.c",
        pattern=r"ZrCore_Array_Init\(ps->state,\s*names,\s*sizeof\(SZrString \*\),\s*4\)|"
        r"ZrParser_AstNodeArray_New\(ps->state,\s*4\)|errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Primary-expression named-argument helpers must use parser capacity tiers and diagnostic buffer constants.",
    ),
    AuditRule(
        name="parser module and block statement capacities",
        path="zr_vm_parser/src/zr_vm_parser/parser.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*16\)",
        target="ZR_PARSER_INITIAL_CAPACITY_MEDIUM",
        layer="common",
        meaning="Parser root/module statement collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser class member collection capacity",
        path="zr_vm_parser/src/zr_vm_parser/parser_class.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*8\)",
        target="ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Class member parsing collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser extern member and declaration capacities",
        path="zr_vm_parser/src/zr_vm_parser/parser_extern.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*(4|8)\)",
        target="ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Extern parser member, declaration, and instruction collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser interface member collection capacity",
        path="zr_vm_parser/src/zr_vm_parser/parser_interface.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*8\)",
        target="ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Interface member parsing collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser literal collection capacities",
        path="zr_vm_parser/src/zr_vm_parser/parser_literals.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*(4|8)\)",
        target="ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Literal parser segment, element, and property collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser statement collection capacities",
        path="zr_vm_parser/src/zr_vm_parser/parser_statements.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*(4|8)\)",
        target="ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Statement parser block and switch-case collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser struct member collection capacity",
        path="zr_vm_parser/src/zr_vm_parser/parser_struct.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*8\)",
        target="ZR_PARSER_INITIAL_CAPACITY_SMALL",
        layer="common",
        meaning="Struct member parsing collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser type collection capacities",
        path="zr_vm_parser/src/zr_vm_parser/parser_types.c",
        pattern=r"ZrParser_AstNodeArray_New\(ps->state,\s*4\)",
        target="ZR_PARSER_INITIAL_CAPACITY_TINY",
        layer="common",
        meaning="Type parser parameter and key collections must reuse parser capacity tiers.",
    ),
    AuditRule(
        name="parser ast default capacity and growth factor",
        path="zr_vm_parser/src/zr_vm_parser/ast.c",
        pattern=r":\s*8\s*;|capacity\s*\*\s*2",
        target="ZR_PARSER_INITIAL_CAPACITY_SMALL / ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR",
        layer="common",
        meaning="AST node-array defaults and growth must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser lexer growth factor and snippet buffer",
        path="zr_vm_parser/src/zr_vm_parser/lexer.c",
        pattern=r"bufferSize\s*\*\s*2|snippet\s*\[\s*128\s*\]",
        target="ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR / ZR_PARSER_TYPE_NAME_BUFFER_LENGTH",
        layer="common",
        meaning="Lexer save-buffer expansion and displayed snippet buffers must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser lexer diagnostic snippet window",
        path="zr_vm_parser/src/zr_vm_parser/lexer.c",
        pattern=r"column\s*>\s*20\b|snippetStart\s*=\s*pos\s*-\s*20\b|displayColumn\s*=\s*21\b|lineEnd\s*-\s*pos\s*<\s*20\b",
        target="ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS / ZR_PARSER_ERROR_SNIPPET_FOCUS_COLUMN",
        layer="common",
        meaning="Lexer diagnostic snippet window geometry must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser-state diagnostic snippet window",
        path="zr_vm_parser/src/zr_vm_parser/parser_state.c",
        pattern=r"column\s*>\s*20\b|snippetStart\s*=\s*pos\s*-\s*20\b|displayColumn\s*=\s*21\b|lineEnd\s*-\s*pos\s*<\s*20\b",
        target="ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS / ZR_PARSER_ERROR_SNIPPET_FOCUS_COLUMN",
        layer="common",
        meaning="Parser-state diagnostic snippet window geometry must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser class-support diagnostics",
        path="zr_vm_parser/src/zr_vm_parser/compiler_class_support.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Class-support diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser generic-semantics diagnostics",
        path="zr_vm_parser/src/zr_vm_parser/compiler_generic_semantics.c",
        pattern=r"errorBuffer\s*\[\s*256\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Generic-semantics diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser compile-expression diagnostics",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Compile-expression diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser compile-expression-call diagnostics",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression_call.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Compile-expression call diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser compile-time executor diagnostics",
        path="zr_vm_parser/src/zr_vm_parser/compile_time_executor.c",
        pattern=r"message\s*\[\s*256\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Compile-time executor diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser declaration diagnostics",
        path="zr_vm_parser/src/zr_vm_parser/parser_declarations.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Parser declaration diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser extern diagnostics",
        path="zr_vm_parser/src/zr_vm_parser/parser_extern.c",
        pattern=r"errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Parser extern diagnostics must come from zr_parser_conf.h.",
    ),
    AuditRule(
        name="core gc shutdown full-collect guard",
        path="zr_vm_core/src/zr_vm_core/gc.c",
        pattern=r"maxIterations\s*=\s*1000\b",
        target="ZR_GC_SHUTDOWN_FULL_COLLECTION_ITERATION_LIMIT",
        layer="common",
        meaning="GC shutdown full-collection iteration guard must come from zr_gc_internal_conf.h.",
    ),
    AuditRule(
        name="core value debug string limits",
        path="zr_vm_core/src/zr_vm_core/value.c",
        pattern=r"#define\s+MAX_DEBUG_BUFFER_SIZE\b|MAX_ELEMENTS_TO_SHOW\s*=\s*10\b|buffer\s*\[\s*2048\s*\]",
        target="ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH / ZR_RUNTIME_DEBUG_COLLECTION_PREVIEW_MAX",
        layer="common",
        meaning="Value debug-string preview limits must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="core object prototype capacity literals",
        path="zr_vm_core/src/zr_vm_core/object.c",
        pattern=r"managedFieldCapacity\s*>\s*0\s*\?\s*prototype->managedFieldCapacity\s*:\s*4|newCapacity\s*\*=\s*2|"
        r"memberDescriptorCapacity\s*>\s*0\s*\?\s*prototype->memberDescriptorCapacity\s*\*\s*2\s*:\s*4",
        target="ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY / ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR",
        layer="common",
        meaning="Runtime object-prototype capacity policy must come from zr_runtime_limits_conf.h.",
    ),
    AuditRule(
        name="cli compiler stable-hash file constants",
        path="zr_vm_cli/src/zr_vm_cli/compiler.c",
        pattern=r"chunk\s*\[\s*4096\s*\]|hash\s*=\s*1469598103934665603ULL|hash\s*\*=\s*1099511628211ULL",
        target="zr_hash_conf.h",
        layer="common",
        meaning="CLI file hashing must reuse the shared stable-hash buffer size and FNV-1a constants.",
    ),
    AuditRule(
        name="cli project stable-hash constants",
        path="zr_vm_cli/src/zr_vm_cli/project.c",
        pattern=r"hash\s*=\s*1469598103934665603ULL|hash\s*\*=\s*1099511628211ULL|\"%016llx\"",
        target="zr_hash_conf.h",
        layer="common",
        meaning="CLI manifest hashing and hex formatting must reuse shared stable-hash constants.",
    ),
    AuditRule(
        name="library aot runtime stable-hash constants",
        path="zr_vm_library/src/zr_vm_library/aot_runtime.c",
        pattern=r"chunk\s*\[\s*4096\s*\]|hash\s*=\s*1469598103934665603ULL|hash\s*\*=\s*1099511628211ULL|\"%016llx\"|sourceHash\s*\[\s*32\s*\]|zroHash\s*\[\s*32\s*\]",
        target="zr_hash_conf.h",
        layer="common",
        meaning="AOT runtime artifact hashing must reuse shared stable-hash constants and hash-buffer sizing.",
    ),
    AuditRule(
        name="lsp stdio header-line buffer",
        path="zr_vm_language_server/stdio/stdio_transport.c",
        pattern=r"headerLine\s*\[\s*1024\s*\]",
        target="ZR_LSP_STDIO_HEADER_BUFFER_LENGTH",
        layer="module",
        meaning="stdio transport must use the named header-line read buffer length.",
    ),
    AuditRule(
        name="lsp stdio semantic-token staging capacity",
        path="zr_vm_language_server/stdio/stdio_requests.c",
        pattern=r"ZrCore_Array_Init\(server->state,\s*&tokens,\s*sizeof\(TZrUInt32\),\s*32\)",
        target="ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY",
        layer="module",
        meaning="stdio request handling must reuse the named semantic-token initial capacity.",
    ),
    AuditRule(
        name="lsp wasm semantic-token staging capacity",
        path="zr_vm_language_server/wasm/wasm_exports.cpp",
        pattern=r"ZrCore_Array_Init\(g_wasm_state,\s*&tokens,\s*sizeof\(TZrUInt32\),\s*32\)",
        target="ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY",
        layer="module",
        meaning="WASM semantic-token export must reuse the named semantic-token initial capacity.",
    ),
    AuditRule(
        name="lsp wasm protocol payload field literals",
        path="zr_vm_language_server/wasm/wasm_exports.cpp",
        pattern=r"\"line\"|\"character\"|\"start\"|\"end\"|\"range\"|\"uri\"|\"name\"|\"kind\"|"
        r"\"containerName\"|\"documentation\"|\"insertText\"|\"insertTextFormat\"|\"contents\"|"
        r"\"value\"|\"location\"|\"severity\"|\"message\"|\"code\"|\"label\"|\"detail\"|\"markdown\"|"
        r"\"placeholder\"",
        target="ZR_LSP_FIELD_* / ZR_LSP_MARKUP_KIND_MARKDOWN",
        layer="module",
        meaning="WASM LSP serializers must reuse the same named protocol field and markup constants as stdio serializers.",
    ),
    AuditRule(
        name="parser bindings raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_bindings.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_MEMBER_ID_NONE",
        layer="common",
        meaning="Compiler binding helpers must reuse the named parser UINT32 none sentinel family from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser expression-support raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression_support.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_MEMBER_ID_NONE / ZR_PARSER_INDEX_NONE",
        layer="common",
        meaning="Expression support helpers must reuse the named parser UINT32 none sentinel family from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser expression-types raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression_types.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_MEMBER_ID_NONE / ZR_PARSER_INDEX_NONE",
        layer="common",
        meaning="Type-oriented expression compilation must reuse the named parser UINT32 none sentinel family from zr_parser_conf.h.",
    ),
    AuditRule(
        name="core function debug-hook no-line sentinel",
        path="zr_vm_core/src/zr_vm_core/function.c",
        pattern=r"ZR_DEBUG_HOOK_EVENT_CALL,\s*\(TZrUInt32\)-1",
        target="ZR_RUNTIME_DEBUG_HOOK_LINE_NONE",
        layer="common",
        meaning="Synthetic debug-hook call events must use the named runtime no-line sentinel.",
    ),
    AuditRule(
        name="core debug-hook return no-line sentinel",
        path="zr_vm_core/src/zr_vm_core/debug.c",
        pattern=r"ZR_DEBUG_HOOK_EVENT_RETURN,\s*\(TZrUInt32\)-1",
        target="ZR_RUNTIME_DEBUG_HOOK_LINE_NONE",
        layer="common",
        meaning="Synthetic debug-hook return events must use the named runtime no-line sentinel.",
    ),
    AuditRule(
        name="core debug trace raw none signal",
        path="zr_vm_core/src/zr_vm_core/debug.c",
        pattern=r"ZrCore_Debug_TraceExecution\s*\([^)]*\)\s*\{[\s\S]*?return\s+0\s*;",
        target="ZR_DEBUG_SIGNAL_NONE",
        layer="common",
        meaning="TraceExecution should return the shared debug-signal none constant instead of a raw 0 fallback.",
    ),
    AuditRule(
        name="core exception invalid thread-status sentinel",
        path="zr_vm_core/src/zr_vm_core/exception.c",
        pattern=r"\(context\)->status\s*=\s*-1;",
        target="ZR_THREAD_STATUS_INVALID",
        layer="common",
        meaning="Native exception fallback must use the shared invalid thread-status sentinel instead of raw -1.",
    ),
    AuditRule(
        name="core exception raw source-line none sentinel",
        path="zr_vm_core/src/zr_vm_core/exception.c",
        pattern=r"ZrCore_Exception_FindSourceLine\s*\([^)]*\)\s*\{[\s\S]*?TZrUInt32 bestLine = 0\s*;[\s\S]*?return 0;|TZrInt64\s+sourceLine\s*=\s*0\s*;",
        target="ZR_EXCEPTION_SOURCE_LINE_NONE",
        layer="module",
        meaning="Exception stack-trace helpers should use a named local no-source-line sentinel instead of raw 0 defaults.",
    ),
    AuditRule(
        name="parser expression-values raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression_values.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_MEMBER_ID_NONE / ZR_PARSER_INDEX_NONE",
        layer="common",
        meaning="Expression value helpers must reuse the named parser UINT32 none sentinel family from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser compile-expression raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_MEMBER_ID_NONE / ZR_PARSER_INDEX_NONE",
        layer="common",
        meaning="Main expression compilation must reuse the named parser UINT32 none sentinel family from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser class-support raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_class_support.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_MEMBER_ID_NONE",
        layer="common",
        meaning="Class-support helpers must reuse the named parser UINT32 none sentinel family from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser closure raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_closure.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_INDEX_NONE",
        layer="common",
        meaning="Closure capture helpers must reuse the named parser UINT32 none sentinel family from zr_parser_conf.h.",
    ),
    AuditRule(
        name="parser extern-declaration raw UINT32 none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_extern_declaration.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_SLOT_NONE",
        layer="common",
        meaning="Extern declaration lowering must reuse the named parser slot-none sentinel instead of raw all-ones values.",
    ),
    AuditRule(
        name="parser function raw slot-none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_function.c",
        pattern=r"functionVarIndex\s*==\s*\(TZrUInt32\)-1",
        target="ZR_PARSER_SLOT_NONE",
        layer="common",
        meaning="Function lowering should test missing local slots through ZR_PARSER_SLOT_NONE.",
    ),
    AuditRule(
        name="parser extern-emit raw member-or-slot none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_extern_emit.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_MEMBER_ID_NONE / ZR_PARSER_SLOT_NONE",
        layer="common",
        meaning="Extern emit helpers must reuse named parser member-id and slot sentinels.",
    ),
    AuditRule(
        name="parser instruction raw member-id none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_instruction.c",
        pattern=r"return\s+\(TZrUInt32\)-1;",
        target="ZR_PARSER_MEMBER_ID_NONE",
        layer="common",
        meaning="Compiler instruction member-entry helpers must return ZR_PARSER_MEMBER_ID_NONE on failure.",
    ),
    AuditRule(
        name="parser locals raw slot-or-index none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_locals.c",
        pattern=r"\(TZrUInt32\)\s*-1|"
        r"if\s*\(cs == ZR_NULL \|\| cs->hasError \|\| name == ZR_NULL\)\s*\{\s*return 0;\s*\}|"
        r"if\s*\(cs == ZR_NULL \|\| name == ZR_NULL\)\s*\{\s*return 0;\s*\}|"
        r"TZrUInt32 allocate_stack_slot\s*\([^)]*\)\s*\{\s*if\s*\(cs == ZR_NULL\)\s*\{\s*return 0;\s*\}",
        target="ZR_PARSER_SLOT_NONE / ZR_PARSER_INDEX_NONE",
        layer="common",
        meaning="Local, closure, and stack-slot helper APIs must reuse the named parser slot/index none sentinels.",
    ),
    AuditRule(
        name="parser locals raw constant-index none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_locals.c",
        pattern=r"compiler_get_cached_null_constant_index\s*\([^)]*\)\s*\{[\s\S]*?return 0;\s*\}|"
        r"generate_function_reference_path_constant\s*\([^)]*\)\s*\{[\s\S]*?return 0;",
        target="ZR_PARSER_INDEX_NONE",
        layer="common",
        meaning="Constant-index helpers in compiler_locals.c must use the shared parser index-none sentinel when they need a real missing/failure boundary.",
    ),
    AuditRule(
        name="parser expression-call raw slot-none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_expression_call.c",
        pattern=r"\(TZrUInt32\)-1",
        target="ZR_PARSER_SLOT_NONE",
        layer="common",
        meaning="Expression-call lowering must reuse the named parser slot-none sentinel for helper failures.",
    ),
    AuditRule(
        name="parser statement raw slot-none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_statement.c",
        pattern=r"existingLocalSlot\s*!=\s*\(TZrUInt32\)-1",
        target="ZR_PARSER_SLOT_NONE",
        layer="common",
        meaning="Statement lowering should test existing locals through ZR_PARSER_SLOT_NONE.",
    ),
    AuditRule(
        name="parser class-support raw signed index-none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_class_support.c",
        pattern=r"return\s+-1;|trackedIndex\s*<\s*0",
        target="ZR_PARSER_I32_NONE",
        layer="common",
        meaning="Class-support helper not-found indexes should use the shared signed parser sentinel instead of raw -1 checks.",
    ),
    AuditRule(
        name="parser generic-semantics raw signed index-none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_generic_semantics.c",
        pattern=r"return\s+-1;|trackedIndex\s*>=\s*0",
        target="ZR_PARSER_I32_NONE",
        layer="common",
        meaning="Generic-semantics helper not-found indexes should use the shared signed parser sentinel instead of raw -1 checks.",
    ),
    AuditRule(
        name="parser generic-call raw signed index-none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_generic_calls.c",
        pattern=r"return\s+-1;|bindingIndex\s*>=\s*0",
        target="ZR_PARSER_I32_NONE",
        layer="common",
        meaning="Generic-call binding lookup should use the shared signed parser sentinel instead of raw -1 checks.",
    ),
    AuditRule(
        name="parser lexer local EOZ sentinel",
        path="zr_vm_parser/src/zr_vm_parser/lexer.c",
        pattern=r"#define\s+ZR_LEXER_EOZ\s+\(-1\)",
        target="ZR_PARSER_LEXER_EOZ",
        layer="common",
        meaning="Lexer end-of-input sentinel must come from zr_parser_conf.h instead of a local raw -1 define.",
    ),
    AuditRule(
        name="parser-state raw EOZ comparison",
        path="zr_vm_parser/src/zr_vm_parser/parser_state.c",
        pattern=r"currentChar\s*==\s*-1",
        target="ZR_PARSER_LEXER_EOZ",
        layer="common",
        meaning="Parser-state token-range helpers must reuse the named lexer end-of-input sentinel.",
    ),
    AuditRule(
        name="parser scope raw label-id none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compiler_scope.c",
        pattern=r"return\s+\(TZrSize\)\s*-1;",
        target="ZR_PARSER_LABEL_ID_NONE",
        layer="common",
        meaning="Label creation failure must return the named parser label-id none sentinel.",
    ),
    AuditRule(
        name="parser flow raw finally-label none sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_statement_flow.c",
        pattern=r"finallyLabelId\s*!=\s*\(TZrSize\)-1|finallyLabelId\s*=\s*hasFinally\s*\?\s*create_label\(cs\)\s*:\s*\(TZrSize\)-1",
        target="ZR_PARSER_LABEL_ID_NONE",
        layer="common",
        meaning="Try/finally lowering must reuse the named parser label-id none sentinel for missing finally labels.",
    ),
    AuditRule(
        name="parser flow raw stack-slot bind sentinel",
        path="zr_vm_parser/src/zr_vm_parser/compile_statement_flow.c",
        pattern=r"bind_existing_stack_slot_as_local_var\s*\([^)]*\)\s*\{[\s\S]*?return\s+0\s*;[\s\S]*?return\s+0\s*;",
        target="ZR_PARSER_SLOT_NONE",
        layer="common",
        meaning="Existing stack-slot local binding helpers must reuse the named parser slot-none sentinel on failure.",
    ),
    AuditRule(
        name="parser type-inference generic-start local sentinel",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_core.c",
        pattern=r"genericStart\s*=\s*\(TZrSize\)-1|genericStart\s*==\s*\(TZrSize\)-1",
        target="ZR_TYPE_INFERENCE_GENERIC_START_NOT_FOUND",
        layer="module",
        meaning="Generic-instance type-name parsing should use a local named start-not-found sentinel instead of raw (TZrSize)-1.",
    ),
    AuditRule(
        name="parser type-inference overload-score local sentinel",
        path="zr_vm_parser/src/zr_vm_parser/type_inference_core.c",
        pattern=r"return\s+-1;|score\s*<\s*0",
        target="ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE",
        layer="module",
        meaning="Overload scoring should use a local named incompatible-score sentinel instead of raw -1 comparisons.",
    ),
    AuditRule(
        name="core meta compare-float local tri-state constants",
        path="zr_vm_core/src/zr_vm_core/meta.c",
        pattern=r"TZrInt64 result = 0;\s*if \(diff > 0\.0\)|result\s*=\s*1;|result\s*=\s*-1;",
        target="ZR_META_COMPARE_RESULT_LESS / ZR_META_COMPARE_RESULT_EQUAL / ZR_META_COMPARE_RESULT_GREATER",
        layer="module",
        meaning="COMPARE float helpers should use local named tri-state constants instead of raw -1/0/1 assignments.",
    ),
    AuditRule(
        name="core closure local zero closed-count sentinel",
        path="zr_vm_core/src/zr_vm_core/closure.c",
        pattern=r"ZrCore_Closure_CloseRegisteredValues\s*\([^)]*\)\s*\{[\s\S]*?TZrSize\s+closedCount\s*=\s*0\s*;[\s\S]*?return\s+0\s*;",
        target="ZR_CLOSURE_CLOSED_COUNT_NONE",
        layer="module",
        meaning="CloseRegisteredValues should use a file-local named zero closed-count sentinel instead of raw 0.",
    ),
    AuditRule(
        name="parser quickening local zero member-flags fallback",
        path="zr_vm_parser/src/zr_vm_parser/compiler_quickening.c",
        pattern=r"compiler_quickening_member_entry_flags\s*\([^)]*\)\s*\{[\s\S]*?return\s+0\s*;",
        target="ZR_COMPILER_QUICKENING_MEMBER_FLAGS_NONE",
        layer="module",
        meaning="Quickening member-entry flag fallback should use a file-local named zero-flags constant instead of raw 0.",
    ),
    AuditRule(
        name="core hash-set local growth factor",
        path="zr_vm_core/include/zr_vm_core/hash_set.h",
        pattern=r"capacity\s*<<\s*1",
        target="ZR_HASH_SET_CAPACITY_GROWTH_FACTOR",
        layer="module",
        meaning="Hash-set growth helpers should use a named local growth factor instead of raw capacity << 1.",
    ),
    AuditRule(
        name="core hash-set local max-load ratio",
        path="zr_vm_core/src/zr_vm_core/hash_set.c",
        pattern=r"newCapacity\s*\*\s*3\s*/\s*4",
        target="ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR",
        layer="module",
        meaning="Hash-set resize thresholds should use named local load-factor ratio constants instead of raw 3/4.",
    ),
    AuditRule(
        name="parser semir local helper capacities and id base",
        path="zr_vm_parser/src/zr_vm_parser/compiler_semir.c",
        pattern=r"typedLocalBindingLength\s*\+\s*1|ownershipStates\s*\[\s*8\s*\]|nextDeoptId\s*=\s*1|nextDeoptId\s*-\s*1",
        target="ZR_SEMIR_TYPE_TABLE_DEFAULT_ENTRY_COUNT / ZR_SEMIR_OWNERSHIP_STATE_TABLE_CAPACITY / ZR_SEMIR_DEOPT_ID_FIRST",
        layer="module",
        meaning="SemIR helper-local default type-entry count, ownership-state table capacity, and first deopt id should not remain as raw literals.",
    ),
    AuditRule(
        name="parser semir local default indexes",
        path="zr_vm_parser/src/zr_vm_parser/compiler_semir.c",
        pattern=r"if\s*\(typeTable == ZR_NULL \|\| ioCount == ZR_NULL \|\| typeRef == ZR_NULL\)\s*\{\s*return 0;\s*\}|"
        r"if\s*\(function == ZR_NULL \|\| typeTable == ZR_NULL \|\| function->typedLocalBindings == ZR_NULL\)\s*\{\s*return 0;\s*\}|"
        r"return 0;\s*\}\s*static TZrUInt32 semir_ensure_ownership_state|"
        r"if\s*\(stateTable == ZR_NULL \|\| ioCount == ZR_NULL\)\s*\{\s*return 0;\s*\}",
        target="ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX / ZR_SEMIR_OWNERSHIP_STATE_INDEX_FIRST",
        layer="module",
        meaning="SemIR helper-local default type and ownership table indexes should use named local constants instead of raw 0 fallbacks.",
    ),
    AuditRule(
        name="backend aot local empty count and root index",
        path="zr_vm_parser/src/zr_vm_parser/backend_aot.c",
        pattern=r"TZrUInt32\s+count\s*=\s*0\s*;|if\s*\(function == ZR_NULL\)\s*\{\s*return 0;\s*\}|entry->flatIndex == 0|functionTable\.count > 0",
        target="ZR_AOT_COUNT_NONE / ZR_AOT_FUNCTION_TREE_ROOT_INDEX",
        layer="module",
        meaning="AOT helper-local empty counts and flattened root-function index should use named local constants instead of raw 0 checks.",
    ),
    AuditRule(
        name="backend aot empty embedded blob byte",
        path="zr_vm_parser/src/zr_vm_parser/backend_aot.c",
        pattern=r"fprintf\(file,\s*\"    0x00\\n\"\);",
        target="ZR_AOT_EMBEDDED_BLOB_EMPTY_BYTE",
        layer="module",
        meaning="AOT C writer should name the empty embedded-blob placeholder byte instead of writing a raw 0x00 literal.",
    ),
    AuditRule(
        name="parser semantic shared id boundaries in semantic.c",
        path="zr_vm_parser/src/zr_vm_parser/semantic.c",
        pattern=r"context->nextTypeId\s*=\s*1;|context->nextSymbolId\s*=\s*1;|context->nextOverloadSetId\s*=\s*1;|"
        r"context->nextLifetimeRegionId\s*=\s*1;|type == ZR_NULL\)\s*\{\s*return 0;|name == ZR_NULL\)\s*\{\s*return 0;|"
        r"context == ZR_NULL\)\s*\{\s*return 0;|overloadSetId\s*==\s*0|symbolId\s*==\s*0",
        target="ZR_SEMANTIC_ID_INVALID / ZR_SEMANTIC_ID_FIRST",
        layer="module",
        meaning="Semantic IDs should use the shared semantic invalid/first boundaries instead of raw 0/1 values.",
    ),
    AuditRule(
        name="parser semantic shared id boundaries in compile_statement.c",
        path="zr_vm_parser/src/zr_vm_parser/compile_statement.c",
        pattern=r"TZrTypeId\s+typeId\s*=\s*0;|if\s*\(cs == ZR_NULL \|\| resource == ZR_NULL \|\| cs->semanticContext == ZR_NULL\)\s*\{\s*return 0;|"
        r"resource->data\.identifier\.name == ZR_NULL\)\s*\{\s*return 0;|TZrLifetimeRegionId\s+regionId\s*=\s*0;|"
        r"TZrSymbolId\s+symbolId\s*=\s*0;|typeId,\s*0,\s*resource,\s*resource->location\)",
        target="ZR_SEMANTIC_ID_INVALID",
        layer="module",
        meaning="Using-statement semantic helpers should reuse the shared semantic invalid-id boundary instead of raw 0.",
    ),
    AuditRule(
        name="parser semantic shared id boundaries in type_system.c",
        path="zr_vm_parser/src/zr_vm_parser/type_system.c",
        pattern=r"typeId,\s*0,\s*ZR_NULL,\s*location\)",
        target="ZR_SEMANTIC_ID_INVALID",
        layer="module",
        meaning="Type-environment semantic registration should reuse the shared semantic invalid-id boundary instead of raw 0.",
    ),
)


BACKLOG_RULES: tuple[AuditRule, ...] = ()


EXEMPTIONS: tuple[Exemption, ...] = (
    Exemption(scope="descriptor tables", reason="Field-name arrays, type descriptors, hint JSON, and help text are data, not conf constants."),
    Exemption(scope="definition-bound values", reason="sizeof(...) expressions, enum values, and schema table counts stay beside the defining type."),
    Exemption(scope="ffi/math definition-bound layouts", reason="zr.ffi descriptor sentinels and zr.math fixed matrix dimensions/counts are bound to descriptor or type layout rather than runtime policy knobs."),
    Exemption(scope="token and semantic-token enum ordinals", reason="Lexer token base values and semantic-token enum ordinals stay with the enum they encode unless promoted into a dedicated protocol header."),
    Exemption(scope="global-state uniqueNumber seeds", reason="Production and test callers may choose fixed or arbitrary hash-seed identity numbers for ZrCore_GlobalState_New(...); these are not config thresholds, ABI codes, or protocol constants."),
    Exemption(scope="wasm bridge envelope/status keys", reason="wasm_exports.cpp keeps success/error/data/updated/closed as the JS-facing bridge response envelope and operation-status payload keys rather than LSP protocol fields."),
    Exemption(scope="local helper layout/schema sentinels", reason="Helper-local values such as native metadata unknown-parameter defaults and hash-seed word counts stay local when they encode schema or buffer layout shape instead of shared policy."),
    Exemption(scope="local helper comparison/index sentinels", reason="Helper-local not-found indexes, overload scoring sentinels, and tri-state compare results stay local when they express algorithm control flow rather than shared parser/runtime policy."),
    Exemption(scope="test and fixture trees", reason="This audit intentionally targets production code and excludes tests, fixtures, build outputs, and third_party."),
)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def collect_matches(rule: AuditRule) -> list[dict[str, object]]:
    path = ROOT / rule.path
    if not path.is_file():
        return [{"path": rule.path, "line": 0, "snippet": "<missing file>"}]

    text = read_text(path)
    matches: list[dict[str, object]] = []
    for match in re.finditer(rule.pattern, text, flags=re.MULTILINE):
        current_line = line_number(text, match.start())
        snippet = text.splitlines()[current_line - 1].strip()
        matches.append({"path": rule.path, "line": current_line, "snippet": snippet})
    return matches


def run_rules(rules: Iterable[AuditRule]) -> list[dict[str, object]]:
    results: list[dict[str, object]] = []
    for rule in rules:
        matches = collect_matches(rule)
        results.append(
            {
                "name": rule.name,
                "path": rule.path,
                "target": rule.target,
                "layer": rule.layer,
                "meaning": rule.meaning,
                "matches": matches,
                "status": "fail" if matches else "pass",
            }
        )
    return results


def print_results(title: str, results: list[dict[str, object]]) -> None:
    print(title)
    for result in results:
        status = result["status"].upper()
        print(f"- {status}: {result['name']}")
        print(f"  target: {result['target']}")
        print(f"  file:   {result['path']}")
        print(f"  note:   {result['meaning']}")
        matches = result["matches"]
        if matches:
            for match in matches:
                print(f"  hit:    {match['path']}:{match['line']} -> {match['snippet']}")
    print()


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit shared and module magic-number consolidation in zr_vm production code.")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON instead of text.")
    args = parser.parse_args()

    migrated_results = run_rules(MIGRATED_RULES)
    backlog_results = run_rules(BACKLOG_RULES)

    migrated_failures = sum(1 for result in migrated_results if result["status"] == "fail")
    backlog_hits = sum(len(result["matches"]) for result in backlog_results)

    payload = {
        "root": str(ROOT),
        "summary": {
            "migrated_checks": len(migrated_results),
            "migrated_failures": migrated_failures,
            "backlog_checks": len(backlog_results),
            "backlog_hits": backlog_hits,
        },
        "migrated": migrated_results,
        "backlog": backlog_results,
        "exemptions": [exemption.__dict__ for exemption in EXEMPTIONS],
    }

    if args.json:
        json.dump(payload, sys.stdout, indent=2, ensure_ascii=False)
        sys.stdout.write("\n")
        return 1 if migrated_failures else 0

    print("Magic Constant Audit")
    print(f"Repository: {ROOT}")
    print(
        "Summary: "
        f"{len(migrated_results)} migrated checks, {migrated_failures} migrated failures, "
        f"{len(backlog_results)} backlog checks, {backlog_hits} backlog hits"
    )
    print()
    print_results("Migrated Checks", migrated_results)
    print_results("Backlog Checks", backlog_results)
    print("Allowed Exemptions")
    for exemption in EXEMPTIONS:
        print(f"- {exemption.scope}: {exemption.reason}")
    print()

    if migrated_failures:
        print("Audit failed: at least one migrated rule regressed.")
        return 1

    print("Audit passed: no migrated rule regressed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
