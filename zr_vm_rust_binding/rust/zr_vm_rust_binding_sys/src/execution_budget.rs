use crate::*;

#[repr(C)]
pub struct ZrRustBindingCancellationToken {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ZrRustBindingTermination {
    #[default]
    ZR_RUST_BINDING_TERMINATION_NONE = 0,
    ZR_RUST_BINDING_TERMINATION_INSTRUCTION_LIMIT = 1,
    ZR_RUST_BINDING_TERMINATION_DEADLINE = 2,
    ZR_RUST_BINDING_TERMINATION_CANCELLED = 3,
    ZR_RUST_BINDING_TERMINATION_HEAP_LIMIT = 4,
    ZR_RUST_BINDING_TERMINATION_NATIVE_CALL_LIMIT = 5,
    ZR_RUST_BINDING_TERMINATION_GC_TIME_LIMIT = 6,
}

/// 与 C 的协作式预算布局一致；cancelToken 由调用者保持到导出调用返回。
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingCallBudget {
    pub maxInstructions: TZrUInt64,
    pub deadlineMicros: TZrUInt64,
    pub cancelToken: *const ZrRustBindingCancellationToken,
    pub hasInstructionLimit: TZrBool,
    pub hasDeadline: TZrBool,
    pub maxHeapBytes: TZrUInt64,
    pub maxNativeCalls: TZrUInt64,
    pub maxGcMicros: TZrUInt64,
    pub hasHeapLimit: TZrBool,
    pub hasNativeCallLimit: TZrBool,
    pub hasGcTimeLimit: TZrBool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ZrRustBindingCallUsage {
    pub executedInstructions: TZrUInt64,
    pub elapsedMicros: TZrUInt64,
    pub termination: ZrRustBindingTermination,
    pub peakHeapBytes: TZrUInt64,
    pub nativeCalls: TZrUInt64,
    pub gcMicros: TZrUInt64,
}

extern "C" {
    pub fn ZrRustBinding_ExecutionNowMicros() -> TZrUInt64;
    pub fn ZrRustBinding_CancellationToken_New() -> *mut ZrRustBindingCancellationToken;
    pub fn ZrRustBinding_CancellationToken_Cancel(token: *mut ZrRustBindingCancellationToken);
    pub fn ZrRustBinding_CancellationToken_IsCancelled(
        token: *const ZrRustBindingCancellationToken,
    ) -> TZrBool;
    pub fn ZrRustBinding_CancellationToken_Free(token: *mut ZrRustBindingCancellationToken);
    pub fn ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
        session: *mut ZrRustBindingProjectSession,
        moduleName: *const c_char,
        exportName: *const c_char,
        arguments: *const *mut ZrRustBindingValue,
        argumentCount: TZrSize,
        budget: *const ZrRustBindingCallBudget,
        outUsage: *mut ZrRustBindingCallUsage,
        outResult: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
}
