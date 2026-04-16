#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

use std::ffi::{c_char, c_void};

pub type TZrBool = u8;
pub type TZrSize = usize;
pub type TZrInt64 = i64;
pub type TZrUInt16 = u16;
pub type TZrUInt32 = u32;
pub type TZrUInt64 = u64;
pub type TZrFloat64 = f64;
pub type TZrPtr = *mut c_void;

mod native;
pub use native::*;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ZrRustBindingStatus {
    ZR_RUST_BINDING_STATUS_OK = 0,
    ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT = 1,
    ZR_RUST_BINDING_STATUS_IO_ERROR = 2,
    ZR_RUST_BINDING_STATUS_NOT_FOUND = 3,
    ZR_RUST_BINDING_STATUS_ALREADY_EXISTS = 4,
    ZR_RUST_BINDING_STATUS_BUFFER_TOO_SMALL = 5,
    ZR_RUST_BINDING_STATUS_COMPILE_ERROR = 6,
    ZR_RUST_BINDING_STATUS_RUNTIME_ERROR = 7,
    ZR_RUST_BINDING_STATUS_UNSUPPORTED = 8,
    ZR_RUST_BINDING_STATUS_INTERNAL_ERROR = 9,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ZrRustBindingExecutionMode {
    ZR_RUST_BINDING_EXECUTION_MODE_INTERP = 0,
    ZR_RUST_BINDING_EXECUTION_MODE_BINARY = 1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ZrRustBindingValueKind {
    ZR_RUST_BINDING_VALUE_KIND_NULL = 0,
    ZR_RUST_BINDING_VALUE_KIND_BOOL = 1,
    ZR_RUST_BINDING_VALUE_KIND_INT = 2,
    ZR_RUST_BINDING_VALUE_KIND_FLOAT = 3,
    ZR_RUST_BINDING_VALUE_KIND_STRING = 4,
    ZR_RUST_BINDING_VALUE_KIND_ARRAY = 5,
    ZR_RUST_BINDING_VALUE_KIND_OBJECT = 6,
    ZR_RUST_BINDING_VALUE_KIND_FUNCTION = 7,
    ZR_RUST_BINDING_VALUE_KIND_NATIVE_POINTER = 8,
    ZR_RUST_BINDING_VALUE_KIND_UNKNOWN = 255,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ZrRustBindingOwnershipKind {
    ZR_RUST_BINDING_OWNERSHIP_KIND_NONE = 0,
    ZR_RUST_BINDING_OWNERSHIP_KIND_UNIQUE = 1,
    ZR_RUST_BINDING_OWNERSHIP_KIND_SHARED = 2,
    ZR_RUST_BINDING_OWNERSHIP_KIND_WEAK = 3,
    ZR_RUST_BINDING_OWNERSHIP_KIND_BORROWED = 4,
    ZR_RUST_BINDING_OWNERSHIP_KIND_LOANED = 5,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingErrorInfo {
    pub status: ZrRustBindingStatus,
    pub message: [c_char; 512],
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ZrRustBindingRuntimeOptions {
    pub heapLimitBytes: TZrUInt64,
    pub pauseBudgetUs: TZrUInt64,
    pub remarkBudgetUs: TZrUInt64,
    pub workerCount: TZrUInt32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingScaffoldOptions {
    pub rootPath: *const c_char,
    pub projectName: *const c_char,
    pub overwriteExisting: TZrBool,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ZrRustBindingCompileOptions {
    pub emitIntermediate: TZrBool,
    pub incremental: TZrBool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingRunOptions {
    pub executionMode: ZrRustBindingExecutionMode,
    pub moduleName: *const c_char,
    pub programArgs: *const *const c_char,
    pub programArgCount: TZrSize,
}

#[repr(C)]
pub struct ZrRustBindingRuntime {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ZrRustBindingProjectWorkspace {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ZrRustBindingCompileResult {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ZrRustBindingManifestSnapshot {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ZrRustBindingValue {
    _private: [u8; 0],
}

extern "C" {
    pub fn ZrRustBinding_GetLastErrorInfo(outErrorInfo: *mut ZrRustBindingErrorInfo);

    pub fn ZrRustBinding_Runtime_NewBare(
        options: *const ZrRustBindingRuntimeOptions,
        outRuntime: *mut *mut ZrRustBindingRuntime,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Runtime_NewStandard(
        options: *const ZrRustBindingRuntimeOptions,
        outRuntime: *mut *mut ZrRustBindingRuntime,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Runtime_Free(runtime: *mut ZrRustBindingRuntime) -> ZrRustBindingStatus;

    pub fn ZrRustBinding_Project_Scaffold(
        options: *const ZrRustBindingScaffoldOptions,
        outWorkspace: *mut *mut ZrRustBindingProjectWorkspace,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Project_Open(
        projectPath: *const c_char,
        outWorkspace: *mut *mut ZrRustBindingProjectWorkspace,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectWorkspace_Free(
        workspace: *mut ZrRustBindingProjectWorkspace,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectWorkspace_GetProjectPath(
        workspace: *const ZrRustBindingProjectWorkspace,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectWorkspace_GetProjectRoot(
        workspace: *const ZrRustBindingProjectWorkspace,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectWorkspace_GetManifestPath(
        workspace: *const ZrRustBindingProjectWorkspace,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectWorkspace_GetEntryModule(
        workspace: *const ZrRustBindingProjectWorkspace,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectWorkspace_ResolveArtifacts(
        workspace: *const ZrRustBindingProjectWorkspace,
        moduleName: *const c_char,
        zroBuffer: *mut c_char,
        zroBufferSize: TZrSize,
        zriBuffer: *mut c_char,
        zriBufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectWorkspace_LoadManifest(
        workspace: *const ZrRustBindingProjectWorkspace,
        outManifestSnapshot: *mut *mut ZrRustBindingManifestSnapshot,
    ) -> ZrRustBindingStatus;

    pub fn ZrRustBinding_ManifestSnapshot_GetVersion(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        outVersion: *mut TZrUInt32,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntryCount(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        outEntryCount: *mut TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_FindEntry(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        moduleName: *const c_char,
        outEntryIndex: *mut TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntryModuleName(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        entryIndex: TZrSize,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntrySourceHash(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        entryIndex: TZrSize,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntryZroHash(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        entryIndex: TZrSize,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntryZroPath(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        entryIndex: TZrSize,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntryZriPath(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        entryIndex: TZrSize,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntryImportCount(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        entryIndex: TZrSize,
        outImportCount: *mut TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_GetEntryImport(
        manifestSnapshot: *const ZrRustBindingManifestSnapshot,
        entryIndex: TZrSize,
        importIndex: TZrSize,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_ManifestSnapshot_Free(
        manifestSnapshot: *mut ZrRustBindingManifestSnapshot,
    ) -> ZrRustBindingStatus;

    pub fn ZrRustBinding_Project_Compile(
        runtime: *mut ZrRustBindingRuntime,
        workspace: *const ZrRustBindingProjectWorkspace,
        options: *const ZrRustBindingCompileOptions,
        outCompileResult: *mut *mut ZrRustBindingCompileResult,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_CompileResult_GetCounts(
        compileResult: *const ZrRustBindingCompileResult,
        outCompiledCount: *mut TZrSize,
        outSkippedCount: *mut TZrSize,
        outRemovedCount: *mut TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_CompileResult_Free(
        compileResult: *mut ZrRustBindingCompileResult,
    ) -> ZrRustBindingStatus;

    pub fn ZrRustBinding_Project_Run(
        runtime: *mut ZrRustBindingRuntime,
        workspace: *const ZrRustBindingProjectWorkspace,
        options: *const ZrRustBindingRunOptions,
        outResult: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Project_CallModuleExport(
        runtime: *mut ZrRustBindingRuntime,
        workspace: *const ZrRustBindingProjectWorkspace,
        options: *const ZrRustBindingRunOptions,
        moduleName: *const c_char,
        exportName: *const c_char,
        arguments: *const *mut ZrRustBindingValue,
        argumentCount: TZrSize,
        outResult: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;

    pub fn ZrRustBinding_Value_NewNull(
        outValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_NewBool(
        boolValue: TZrBool,
        outValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_NewInt(
        intValue: TZrInt64,
        outValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_NewFloat(
        floatValue: TZrFloat64,
        outValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_NewString(
        stringValue: *const c_char,
        outValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_NewArray(
        outValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_NewObject(
        outValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;

    pub fn ZrRustBinding_Value_Free(value: *mut ZrRustBindingValue) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_GetKind(value: *const ZrRustBindingValue) -> ZrRustBindingValueKind;
    pub fn ZrRustBinding_Value_GetOwnershipKind(
        value: *const ZrRustBindingValue,
    ) -> ZrRustBindingOwnershipKind;
    pub fn ZrRustBinding_Value_ReadBool(
        value: *const ZrRustBindingValue,
        outBoolValue: *mut TZrBool,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_ReadInt(
        value: *const ZrRustBindingValue,
        outIntValue: *mut TZrInt64,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_ReadFloat(
        value: *const ZrRustBindingValue,
        outFloatValue: *mut TZrFloat64,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_ReadString(
        value: *const ZrRustBindingValue,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_Array_Length(
        value: *const ZrRustBindingValue,
        outLength: *mut TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_Array_Get(
        value: *const ZrRustBindingValue,
        index: TZrSize,
        outElement: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_Array_Push(
        value: *mut ZrRustBindingValue,
        element: *const ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_Object_Get(
        value: *const ZrRustBindingValue,
        fieldName: *const c_char,
        outFieldValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Value_Object_Set(
        value: *mut ZrRustBindingValue,
        fieldName: *const c_char,
        fieldValue: *const ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
}
