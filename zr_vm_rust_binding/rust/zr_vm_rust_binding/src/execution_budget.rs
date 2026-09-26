use std::fmt;
use std::ptr::NonNull;
use std::sync::Arc;
use std::time::Duration;

use crate::{
    check_status, module_export_argument_ptrs, string_to_cstring, sys, Error, ProjectSession, Value,
};

#[derive(Debug)]
struct CancellationTokenInner(NonNull<sys::ZrRustBindingCancellationToken>);

// The C token contains only an atomic flag. Arc keeps it alive during every
// read, cancellation request, and call borrowing a CancellationToken.
unsafe impl Send for CancellationTokenInner {}
unsafe impl Sync for CancellationTokenInner {}

impl Drop for CancellationTokenInner {
    fn drop(&mut self) {
        unsafe { sys::ZrRustBinding_CancellationToken_Free(self.0.as_ptr()) };
    }
}

/// A one-shot cancellation request that can be shared with another thread.
#[derive(Clone, Debug)]
pub struct CancellationToken(Arc<CancellationTokenInner>);

impl CancellationToken {
    pub fn new() -> Result<Self, Error> {
        let raw = NonNull::new(unsafe { sys::ZrRustBinding_CancellationToken_New() }).ok_or_else(
            || {
                Error::new(
                    sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                    "failed to allocate cancellation token",
                )
            },
        )?;
        Ok(Self(Arc::new(CancellationTokenInner(raw))))
    }

    pub fn cancel(&self) {
        unsafe { sys::ZrRustBinding_CancellationToken_Cancel(self.raw()) };
    }

    pub fn is_cancelled(&self) -> bool {
        unsafe { sys::ZrRustBinding_CancellationToken_IsCancelled(self.raw()) != 0 }
    }

    fn raw(&self) -> *mut sys::ZrRustBindingCancellationToken {
        self.0.as_ref().0.as_ptr()
    }
}

/// An absolute deadline in the VM's monotonic clock domain.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ExecutionDeadline(u64);

impl ExecutionDeadline {
    pub fn after(duration: Duration) -> Self {
        let now = unsafe { sys::ZrRustBinding_ExecutionNowMicros() };
        let micros = u64::try_from(duration.as_micros()).unwrap_or(u64::MAX);
        Self(now.saturating_add(micros))
    }
}

/// Limits bytecode dispatch and polls around native callbacks. Native callbacks
/// must return cooperatively; termination does not roll back VM or host state.
#[derive(Clone, Debug, Default)]
pub struct CallBudget {
    /// Some(0) admits no bytecode instructions. None leaves instruction count unlimited.
    pub max_instructions: Option<u64>,
    pub deadline: Option<ExecutionDeadline>,
    pub cancellation: Option<CancellationToken>,
    /// Peak requested bytes held by the VM allocator, including its initial heap.
    /// Checked at cooperative boundaries after allocation; a single allocation
    /// or native callback can overshoot. Memory allocated outside the VM's
    /// allocator is excluded, so this is not a process memory ceiling.
    pub max_heap_bytes: Option<u64>,
    pub max_native_calls: Option<u64>,
    /// Cumulative time spent in synchronous GC steps/full collections, including
    /// safepoint waits. A collection completes before this limit is checked.
    pub max_gc_time: Option<Duration>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CallTermination {
    InstructionLimit,
    Deadline,
    Cancelled,
    HeapLimit,
    NativeCallLimit,
    GcTimeLimit,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CallUsage {
    pub executed_instructions: u64,
    pub elapsed: Duration,
    pub peak_heap_bytes: u64,
    pub native_calls: u64,
    pub gc_time: Duration,
}

pub struct CallResult {
    pub value: Value,
    pub usage: CallUsage,
}

#[derive(Debug)]
pub struct CallError {
    pub error: Error,
    pub termination: Option<CallTermination>,
    pub usage: CallUsage,
}

impl From<Error> for CallError {
    fn from(error: Error) -> Self {
        Self {
            error,
            termination: None,
            usage: CallUsage::default(),
        }
    }
}

impl fmt::Display for CallError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&self.error, formatter)
    }
}

impl std::error::Error for CallError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.error)
    }
}

impl ProjectSession {
    pub fn call_module_export_with_budget(
        &mut self,
        module_name: &str,
        export_name: &str,
        arguments: &[Value],
        budget: &CallBudget,
    ) -> Result<CallResult, CallError> {
        let module_name = string_to_cstring(module_name)?;
        let export_name = string_to_cstring(export_name)?;
        let arguments = arguments.iter().map(|value| value.raw).collect::<Vec<_>>();
        let empty_arguments = [std::ptr::null_mut()];
        let argument_ptrs = module_export_argument_ptrs(&arguments, &empty_arguments);
        let options = sys::ZrRustBindingCallBudget {
            maxInstructions: budget.max_instructions.unwrap_or(0),
            deadlineMicros: budget.deadline.map_or(0, |deadline| deadline.0),
            cancelToken: budget
                .cancellation
                .as_ref()
                .map_or(std::ptr::null(), |token| token.raw()),
            hasInstructionLimit: u8::from(budget.max_instructions.is_some()),
            hasDeadline: u8::from(budget.deadline.is_some()),
            maxHeapBytes: budget.max_heap_bytes.unwrap_or(0),
            maxNativeCalls: budget.max_native_calls.unwrap_or(0),
            maxGcMicros: budget.max_gc_time.map_or(0, |time| {
                u64::try_from(time.as_micros()).unwrap_or(u64::MAX)
            }),
            hasHeapLimit: u8::from(budget.max_heap_bytes.is_some()),
            hasNativeCallLimit: u8::from(budget.max_native_calls.is_some()),
            hasGcTimeLimit: u8::from(budget.max_gc_time.is_some()),
        };
        let mut usage = sys::ZrRustBindingCallUsage::default();
        let mut raw = std::ptr::null_mut();
        let status = unsafe {
            sys::ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                self.raw,
                module_name.as_ptr(),
                export_name.as_ptr(),
                argument_ptrs,
                arguments.len(),
                &options,
                &mut usage,
                &mut raw,
            )
        };
        let termination = match usage.termination {
            sys::ZrRustBindingTermination::ZR_RUST_BINDING_TERMINATION_NONE => None,
            sys::ZrRustBindingTermination::ZR_RUST_BINDING_TERMINATION_INSTRUCTION_LIMIT => {
                Some(CallTermination::InstructionLimit)
            }
            sys::ZrRustBindingTermination::ZR_RUST_BINDING_TERMINATION_DEADLINE => {
                Some(CallTermination::Deadline)
            }
            sys::ZrRustBindingTermination::ZR_RUST_BINDING_TERMINATION_CANCELLED => {
                Some(CallTermination::Cancelled)
            }
            sys::ZrRustBindingTermination::ZR_RUST_BINDING_TERMINATION_HEAP_LIMIT => {
                Some(CallTermination::HeapLimit)
            }
            sys::ZrRustBindingTermination::ZR_RUST_BINDING_TERMINATION_NATIVE_CALL_LIMIT => {
                Some(CallTermination::NativeCallLimit)
            }
            sys::ZrRustBindingTermination::ZR_RUST_BINDING_TERMINATION_GC_TIME_LIMIT => {
                Some(CallTermination::GcTimeLimit)
            }
        };
        let usage = CallUsage {
            executed_instructions: usage.executedInstructions,
            elapsed: Duration::from_micros(usage.elapsedMicros),
            peak_heap_bytes: usage.peakHeapBytes,
            native_calls: usage.nativeCalls,
            gc_time: Duration::from_micros(usage.gcMicros),
        };
        check_status(status).map_err(|error| CallError {
            error,
            termination,
            usage,
        })?;
        Ok(CallResult {
            value: Value { raw },
            usage,
        })
    }
}
