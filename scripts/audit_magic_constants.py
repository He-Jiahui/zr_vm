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
        name="lsp stdio json-rpc request error codes",
        path="zr_vm_language_server/stdio/stdio_requests.c",
        pattern=r"-32601|-32602",
        target="ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE / ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE",
        layer="module",
        meaning="stdio request dispatch must not hard-code JSON-RPC method-not-found and invalid-params error codes.",
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
        pattern=r"ZrCore_Array_Init\(ps->state,\s*names,\s*sizeof\(SZrString \*\),\s*4\)|errorMsg\s*\[\s*256\s*\]",
        target="ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_ERROR_BUFFER_LENGTH",
        layer="common",
        meaning="Primary-expression named-argument helpers must use parser capacity tiers and diagnostic buffer constants.",
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
        name="core object prototype capacity literals",
        path="zr_vm_core/src/zr_vm_core/object.c",
        pattern=r"managedFieldCapacity\s*>\s*0\s*\?\s*prototype->managedFieldCapacity\s*:\s*4|newCapacity\s*\*=\s*2|"
        r"memberDescriptorCapacity\s*>\s*0\s*\?\s*prototype->memberDescriptorCapacity\s*\*\s*2\s*:\s*4",
        target="ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY / ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR",
        layer="common",
        meaning="Runtime object-prototype capacity policy must come from zr_runtime_limits_conf.h.",
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
)


BACKLOG_RULES: tuple[AuditRule, ...] = ()


EXEMPTIONS: tuple[Exemption, ...] = (
    Exemption(scope="descriptor tables", reason="Field-name arrays, type descriptors, hint JSON, and help text are data, not conf constants."),
    Exemption(scope="definition-bound values", reason="sizeof(...) expressions, enum values, and schema table counts stay beside the defining type."),
    Exemption(scope="ffi/math definition-bound layouts", reason="zr.ffi descriptor sentinels and zr.math fixed matrix dimensions/counts are bound to descriptor or type layout rather than runtime policy knobs."),
    Exemption(scope="token and semantic-token enum ordinals", reason="Lexer token base values and semantic-token enum ordinals stay with the enum they encode unless promoted into a dedicated protocol header."),
    Exemption(scope="global-state uniqueNumber seeds", reason="Production and test callers may choose fixed or arbitrary hash-seed identity numbers for ZrCore_GlobalState_New(...); these are not config thresholds, ABI codes, or protocol constants."),
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
