use std::ffi::{c_char, c_void};
use std::marker::PhantomData;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::slice;
use std::str;

use zr_vm_rust_binding_sys as sys;

use crate::{check_status, Error, NativeCallContext, ValueKind};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NativeArgumentKind {
    Null,
    Bool,
    Int,
    Float,
    String,
    Array,
    Object,
    Function,
    NativePointer,
    Unknown,
}

impl From<sys::ZrRustBindingValueKind> for NativeArgumentKind {
    fn from(value: sys::ZrRustBindingValueKind) -> Self {
        match value {
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_NULL => Self::Null,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_BOOL => Self::Bool,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_INT => Self::Int,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_FLOAT => Self::Float,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_STRING => Self::String,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_ARRAY => Self::Array,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_OBJECT => Self::Object,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_FUNCTION => Self::Function,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_NATIVE_POINTER => {
                Self::NativePointer
            }
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_UNKNOWN => Self::Unknown,
        }
    }
}

impl From<NativeArgumentKind> for ValueKind {
    fn from(value: NativeArgumentKind) -> Self {
        match value {
            NativeArgumentKind::Null => Self::Null,
            NativeArgumentKind::Bool => Self::Bool,
            NativeArgumentKind::Int => Self::Int,
            NativeArgumentKind::Float => Self::Float,
            NativeArgumentKind::String => Self::String,
            NativeArgumentKind::Array => Self::Array,
            NativeArgumentKind::Object => Self::Object,
            NativeArgumentKind::Function => Self::Function,
            NativeArgumentKind::NativePointer => Self::NativePointer,
            NativeArgumentKind::Unknown => Self::Unknown,
        }
    }
}

pub struct NativeArgumentView<'argument> {
    raw: *const sys::ZrRustBindingNativeArgumentView,
    _scope: PhantomData<&'argument NativeCallContext<'argument>>,
}

impl NativeArgumentView<'_> {
    pub fn kind(&self) -> Result<NativeArgumentKind, Error> {
        let mut kind = sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_UNKNOWN;
        check_status(unsafe {
            sys::ZrRustBinding_NativeArgumentView_GetKind(self.raw, &mut kind)
        })?;
        Ok(kind.into())
    }

    pub fn read_bool(&self) -> Result<bool, Error> {
        let mut value = 0;
        check_status(unsafe {
            sys::ZrRustBinding_NativeArgumentView_ReadBool(self.raw, &mut value)
        })?;
        Ok(value != 0)
    }

    pub fn read_int(&self) -> Result<i64, Error> {
        let mut value = 0;
        check_status(unsafe {
            sys::ZrRustBinding_NativeArgumentView_ReadInt(self.raw, &mut value)
        })?;
        Ok(value)
    }

    pub fn read_float(&self) -> Result<f64, Error> {
        let mut value = 0.0;
        check_status(unsafe {
            sys::ZrRustBinding_NativeArgumentView_ReadFloat(self.raw, &mut value)
        })?;
        Ok(value)
    }

    pub fn byte_len(&self) -> Result<usize, Error> {
        let mut length = 0;
        check_status(unsafe {
            sys::ZrRustBinding_NativeArgumentView_ByteArrayLength(self.raw, &mut length)
        })?;
        Ok(length)
    }

    pub fn byte_at(&self, index: usize) -> Result<u8, Error> {
        let mut value = 0;
        check_status(unsafe {
            sys::ZrRustBinding_NativeArgumentView_ByteArrayGet(self.raw, index, &mut value)
        })?;
        Ok(value)
    }

    pub fn with_str<T>(
        &self,
        visitor: impl for<'value> FnOnce(&'value str) -> Result<T, Error>,
    ) -> Result<T, Error> {
        let mut state = NativeStringVisitorState {
            visitor: Some(visitor),
            result: None,
        };
        let status = unsafe {
            sys::ZrRustBinding_NativeArgumentView_WithString(
                self.raw,
                Some(native_string_visitor_trampoline::<_, T>),
                (&mut state as *mut NativeStringVisitorState<_, T>).cast::<c_void>(),
            )
        };
        match state.result {
            Some(result) => result,
            None => {
                check_status(status)?;
                Err(Error::new(
                    sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                    "native string visitor completed without a result",
                ))
            }
        }
    }
}

struct NativeArgumentVisitorState<F, T> {
    visitor: Option<F>,
    result: Option<Result<T, Error>>,
}

unsafe extern "C" fn native_argument_visitor_trampoline<F, T>(
    argument: *const sys::ZrRustBindingNativeArgumentView,
    user_data: *mut c_void,
) -> sys::ZrRustBindingStatus
where
    F: for<'argument> FnOnce(NativeArgumentView<'argument>) -> Result<T, Error>,
{
    if argument.is_null() || user_data.is_null() {
        return sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT;
    }

    let state = &mut *(user_data as *mut NativeArgumentVisitorState<F, T>);
    let Some(visitor) = state.visitor.take() else {
        return sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR;
    };
    match catch_unwind(AssertUnwindSafe(|| {
        visitor(NativeArgumentView {
            raw: argument,
            _scope: PhantomData,
        })
    })) {
        Ok(result) => {
            let status = result
                .as_ref()
                .map(|_| sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_OK)
                .unwrap_or_else(|error| error.status);
            state.result = Some(result);
            status
        }
        Err(_) => {
            state.result = Some(Err(Error::new(
                sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                "native argument visitor panicked",
            )));
            sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR
        }
    }
}

struct NativeStringVisitorState<F, T> {
    visitor: Option<F>,
    result: Option<Result<T, Error>>,
}

unsafe extern "C" fn native_string_visitor_trampoline<F, T>(
    utf8: *const c_char,
    utf8_byte_length: usize,
    user_data: *mut c_void,
) -> sys::ZrRustBindingStatus
where
    F: for<'value> FnOnce(&'value str) -> Result<T, Error>,
{
    if utf8.is_null() || user_data.is_null() {
        return sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT;
    }

    let state = &mut *(user_data as *mut NativeStringVisitorState<F, T>);
    let Some(visitor) = state.visitor.take() else {
        return sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR;
    };
    let result = catch_unwind(AssertUnwindSafe(|| {
        let bytes = slice::from_raw_parts(utf8.cast::<u8>(), utf8_byte_length);
        let value = str::from_utf8(bytes).map_err(|_| {
            Error::new(
                sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                "native argument string is not valid UTF-8",
            )
        })?;
        visitor(value)
    }));
    match result {
        Ok(result) => {
            let status = result
                .as_ref()
                .map(|_| sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_OK)
                .unwrap_or_else(|error| error.status);
            state.result = Some(result);
            status
        }
        Err(_) => {
            state.result = Some(Err(Error::new(
                sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                "native string argument visitor panicked",
            )));
            sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR
        }
    }
}

impl NativeCallContext<'_> {
    pub fn with_argument<T>(
        &self,
        index: usize,
        visitor: impl for<'argument> FnOnce(NativeArgumentView<'argument>) -> Result<T, Error>,
    ) -> Result<T, Error> {
        let mut state = NativeArgumentVisitorState {
            visitor: Some(visitor),
            result: None,
        };
        let status = unsafe {
            sys::ZrRustBinding_NativeCallContext_WithArgument(
                self.raw,
                index,
                Some(native_argument_visitor_trampoline::<_, T>),
                (&mut state as *mut NativeArgumentVisitorState<_, T>).cast::<c_void>(),
            )
        };
        match state.result {
            Some(result) => result,
            None => {
                check_status(status)?;
                Err(Error::new(
                    sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                    "native argument visitor completed without a result",
                ))
            }
        }
    }
}
