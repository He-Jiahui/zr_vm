use std::ffi::{c_char, c_void, CStr, CString};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;

use zr_vm_rust_binding_sys as sys;

use crate::{check_status, read_string_with, string_to_cstring, Error, Runtime, Value};

type NativeCallback = dyn Fn(&NativeCallContext) -> Result<Value, Error> + Send + Sync + 'static;

#[derive(Clone, Debug, PartialEq)]
pub enum ConstantValue {
    Null,
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
}

impl From<bool> for ConstantValue {
    fn from(value: bool) -> Self {
        Self::Bool(value)
    }
}

impl From<i64> for ConstantValue {
    fn from(value: i64) -> Self {
        Self::Int(value)
    }
}

impl From<f64> for ConstantValue {
    fn from(value: f64) -> Self {
        Self::Float(value)
    }
}

impl From<String> for ConstantValue {
    fn from(value: String) -> Self {
        Self::String(value)
    }
}

impl From<&str> for ConstantValue {
    fn from(value: &str) -> Self {
        Self::String(value.to_string())
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PrototypeType {
    Invalid,
    Module,
    Class,
    Interface,
    Struct,
    Enum,
    Native,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MetaMethodType {
    Constructor,
    Destructor,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Pow,
    Neg,
    Compare,
    ToBool,
    ToString,
    ToInt,
    ToUint,
    ToFloat,
    Call,
    Getter,
    Setter,
    ShiftLeft,
    ShiftRight,
    BitAnd,
    BitOr,
    BitXor,
    BitNot,
    GetItem,
    SetItem,
    Close,
    Decorate,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TypeHintDescriptor {
    pub symbol_name: String,
    pub symbol_kind: String,
    pub signature: String,
    pub documentation: Option<String>,
}

impl TypeHintDescriptor {
    pub fn new(symbol_name: &str, symbol_kind: &str, signature: &str) -> Self {
        Self {
            symbol_name: symbol_name.to_string(),
            symbol_kind: symbol_kind.to_string(),
            signature: signature.to_string(),
            documentation: None,
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ModuleLinkDescriptor {
    pub name: String,
    pub module_name: String,
    pub documentation: Option<String>,
}

impl ModuleLinkDescriptor {
    pub fn new(name: &str, module_name: &str) -> Self {
        Self {
            name: name.to_string(),
            module_name: module_name.to_string(),
            documentation: None,
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FieldDescriptor {
    pub name: String,
    pub type_name: String,
    pub documentation: Option<String>,
    pub contract_role: u32,
}

impl FieldDescriptor {
    pub fn new(name: &str, type_name: &str) -> Self {
        Self {
            name: name.to_string(),
            type_name: type_name.to_string(),
            documentation: None,
            contract_role: 0,
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }

    pub fn contract_role(mut self, contract_role: u32) -> Self {
        self.contract_role = contract_role;
        self
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ParameterDescriptor {
    pub name: String,
    pub type_name: String,
    pub documentation: Option<String>,
}

impl ParameterDescriptor {
    pub fn new(name: &str, type_name: &str) -> Self {
        Self {
            name: name.to_string(),
            type_name: type_name.to_string(),
            documentation: None,
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct GenericParameterDescriptor {
    pub name: String,
    pub documentation: Option<String>,
    pub constraint_type_names: Vec<String>,
}

impl GenericParameterDescriptor {
    pub fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            documentation: None,
            constraint_type_names: Vec::new(),
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }

    pub fn constraint(mut self, type_name: &str) -> Self {
        self.constraint_type_names.push(type_name.to_string());
        self
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct EnumMemberDescriptor {
    pub name: String,
    pub value: ConstantValue,
    pub documentation: Option<String>,
}

impl EnumMemberDescriptor {
    pub fn new(name: &str, value: impl Into<ConstantValue>) -> Self {
        Self {
            name: name.to_string(),
            value: value.into(),
            documentation: None,
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct ConstantDescriptor {
    pub name: String,
    pub value: ConstantValue,
    pub documentation: Option<String>,
    pub type_name: Option<String>,
}

impl ConstantDescriptor {
    pub fn new(name: &str, value: impl Into<ConstantValue>) -> Self {
        Self {
            name: name.to_string(),
            value: value.into(),
            documentation: None,
            type_name: None,
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }

    pub fn type_name(mut self, type_name: &str) -> Self {
        self.type_name = Some(type_name.to_string());
        self
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct FfiTypeMetadata {
    pub lowering_kind: Option<String>,
    pub view_type_name: Option<String>,
    pub underlying_type_name: Option<String>,
    pub owner_mode: Option<String>,
    pub release_hook: Option<String>,
}

pub struct FunctionBuilder {
    name: String,
    min_argument_count: u16,
    max_argument_count: u16,
    callback: Box<NativeCallback>,
    return_type_name: Option<String>,
    documentation: Option<String>,
    parameters: Vec<ParameterDescriptor>,
    generic_parameters: Vec<GenericParameterDescriptor>,
    contract_role: u32,
}

impl FunctionBuilder {
    pub fn new<F>(name: &str, min_argument_count: u16, max_argument_count: u16, callback: F) -> Self
    where
        F: Fn(&NativeCallContext) -> Result<Value, Error> + Send + Sync + 'static,
    {
        Self {
            name: name.to_string(),
            min_argument_count,
            max_argument_count,
            callback: Box::new(callback),
            return_type_name: None,
            documentation: None,
            parameters: Vec::new(),
            generic_parameters: Vec::new(),
            contract_role: 0,
        }
    }

    pub fn return_type(mut self, return_type_name: &str) -> Self {
        self.return_type_name = Some(return_type_name.to_string());
        self
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }

    pub fn parameter(mut self, name: &str, type_name: &str, documentation: &str) -> Self {
        self.parameters.push(ParameterDescriptor::new(name, type_name).documentation(documentation));
        self
    }

    pub fn add_parameter(mut self, parameter: ParameterDescriptor) -> Self {
        self.parameters.push(parameter);
        self
    }

    pub fn generic_parameter(mut self, parameter: GenericParameterDescriptor) -> Self {
        self.generic_parameters.push(parameter);
        self
    }

    pub fn contract_role(mut self, contract_role: u32) -> Self {
        self.contract_role = contract_role;
        self
    }
}

pub struct MetaMethodBuilder {
    meta_type: MetaMethodType,
    min_argument_count: u16,
    max_argument_count: u16,
    callback: Box<NativeCallback>,
    return_type_name: Option<String>,
    documentation: Option<String>,
    parameters: Vec<ParameterDescriptor>,
    generic_parameters: Vec<GenericParameterDescriptor>,
}

impl MetaMethodBuilder {
    pub fn new<F>(meta_type: MetaMethodType, min_argument_count: u16, max_argument_count: u16, callback: F) -> Self
    where
        F: Fn(&NativeCallContext) -> Result<Value, Error> + Send + Sync + 'static,
    {
        Self {
            meta_type,
            min_argument_count,
            max_argument_count,
            callback: Box::new(callback),
            return_type_name: None,
            documentation: None,
            parameters: Vec::new(),
            generic_parameters: Vec::new(),
        }
    }

    pub fn return_type(mut self, return_type_name: &str) -> Self {
        self.return_type_name = Some(return_type_name.to_string());
        self
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }

    pub fn parameter(mut self, name: &str, type_name: &str, documentation: &str) -> Self {
        self.parameters.push(ParameterDescriptor::new(name, type_name).documentation(documentation));
        self
    }

    pub fn add_parameter(mut self, parameter: ParameterDescriptor) -> Self {
        self.parameters.push(parameter);
        self
    }

    pub fn generic_parameter(mut self, parameter: GenericParameterDescriptor) -> Self {
        self.generic_parameters.push(parameter);
        self
    }
}

struct MethodSpec {
    function: FunctionBuilder,
    is_static: bool,
}

pub struct TypeBuilder {
    name: String,
    prototype_type: PrototypeType,
    fields: Vec<FieldDescriptor>,
    methods: Vec<MethodSpec>,
    meta_methods: Vec<MetaMethodBuilder>,
    documentation: Option<String>,
    extends_type_name: Option<String>,
    implements_type_names: Vec<String>,
    enum_members: Vec<EnumMemberDescriptor>,
    enum_value_type_name: Option<String>,
    allow_value_construction: bool,
    allow_boxed_construction: bool,
    constructor_signature: Option<String>,
    generic_parameters: Vec<GenericParameterDescriptor>,
    protocol_mask: u64,
    ffi_metadata: FfiTypeMetadata,
}

impl TypeBuilder {
    pub fn new(name: &str, prototype_type: PrototypeType) -> Self {
        Self {
            name: name.to_string(),
            prototype_type,
            fields: Vec::new(),
            methods: Vec::new(),
            meta_methods: Vec::new(),
            documentation: None,
            extends_type_name: None,
            implements_type_names: Vec::new(),
            enum_members: Vec::new(),
            enum_value_type_name: None,
            allow_value_construction: false,
            allow_boxed_construction: false,
            constructor_signature: None,
            generic_parameters: Vec::new(),
            protocol_mask: 0,
            ffi_metadata: FfiTypeMetadata::default(),
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }

    pub fn extends(mut self, type_name: &str) -> Self {
        self.extends_type_name = Some(type_name.to_string());
        self
    }

    pub fn implements(mut self, type_name: &str) -> Self {
        self.implements_type_names.push(type_name.to_string());
        self
    }

    pub fn add_field(mut self, field: FieldDescriptor) -> Self {
        self.fields.push(field);
        self
    }

    pub fn field(mut self, name: &str, type_name: &str, documentation: &str, contract_role: u32) -> Self {
        self.fields.push(
            FieldDescriptor::new(name, type_name)
                .documentation(documentation)
                .contract_role(contract_role),
        );
        self
    }

    pub fn add_method(mut self, method: FunctionBuilder) -> Self {
        self.methods.push(MethodSpec {
            function: method,
            is_static: false,
        });
        self
    }

    pub fn add_static_method(mut self, method: FunctionBuilder) -> Self {
        self.methods.push(MethodSpec {
            function: method,
            is_static: true,
        });
        self
    }

    pub fn add_meta_method(mut self, method: MetaMethodBuilder) -> Self {
        self.meta_methods.push(method);
        self
    }

    pub fn add_enum_member(mut self, member: EnumMemberDescriptor) -> Self {
        self.enum_members.push(member);
        self
    }

    pub fn enum_value_type(mut self, type_name: &str) -> Self {
        self.enum_value_type_name = Some(type_name.to_string());
        self
    }

    pub fn allow_value_construction(mut self, allow: bool) -> Self {
        self.allow_value_construction = allow;
        self
    }

    pub fn allow_boxed_construction(mut self, allow: bool) -> Self {
        self.allow_boxed_construction = allow;
        self
    }

    pub fn constructor_signature(mut self, signature: &str) -> Self {
        self.constructor_signature = Some(signature.to_string());
        self
    }

    pub fn generic_parameter(mut self, parameter: GenericParameterDescriptor) -> Self {
        self.generic_parameters.push(parameter);
        self
    }

    pub fn protocol_mask(mut self, protocol_mask: u64) -> Self {
        self.protocol_mask = protocol_mask;
        self
    }

    pub fn ffi_metadata(mut self, ffi_metadata: FfiTypeMetadata) -> Self {
        self.ffi_metadata = ffi_metadata;
        self
    }
}

pub struct ModuleBuilder {
    name: String,
    documentation: Option<String>,
    module_version: Option<String>,
    type_hints_json: Option<String>,
    min_runtime_abi: u32,
    required_capabilities: u64,
    type_hints: Vec<TypeHintDescriptor>,
    module_links: Vec<ModuleLinkDescriptor>,
    constants: Vec<ConstantDescriptor>,
    functions: Vec<FunctionBuilder>,
    types: Vec<TypeBuilder>,
}

impl ModuleBuilder {
    pub fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            documentation: None,
            module_version: None,
            type_hints_json: None,
            min_runtime_abi: 0,
            required_capabilities: 0,
            type_hints: Vec::new(),
            module_links: Vec::new(),
            constants: Vec::new(),
            functions: Vec::new(),
            types: Vec::new(),
        }
    }

    pub fn documentation(mut self, documentation: &str) -> Self {
        self.documentation = Some(documentation.to_string());
        self
    }

    pub fn module_version(mut self, module_version: &str) -> Self {
        self.module_version = Some(module_version.to_string());
        self
    }

    pub fn type_hints_json(mut self, type_hints_json: &str) -> Self {
        self.type_hints_json = Some(type_hints_json.to_string());
        self
    }

    pub fn runtime_requirements(mut self, min_runtime_abi: u32, required_capabilities: u64) -> Self {
        self.min_runtime_abi = min_runtime_abi;
        self.required_capabilities = required_capabilities;
        self
    }

    pub fn add_type_hint(mut self, descriptor: TypeHintDescriptor) -> Self {
        self.type_hints.push(descriptor);
        self
    }

    pub fn add_module_link(mut self, descriptor: ModuleLinkDescriptor) -> Self {
        self.module_links.push(descriptor);
        self
    }

    pub fn add_constant(mut self, descriptor: ConstantDescriptor) -> Self {
        self.constants.push(descriptor);
        self
    }

    pub fn int_constant(mut self, name: &str, value: i64, type_name: &str, documentation: &str) -> Self {
        self.constants.push(
            ConstantDescriptor::new(name, value)
                .type_name(type_name)
                .documentation(documentation),
        );
        self
    }

    pub fn add_function(mut self, descriptor: FunctionBuilder) -> Self {
        self.functions.push(descriptor);
        self
    }

    pub fn add_type(mut self, descriptor: TypeBuilder) -> Self {
        self.types.push(descriptor);
        self
    }

    pub fn build(self) -> Result<NativeModule, Error> {
        let raw_builder = RawModuleBuilder::new(&self.name)?;

        if let Some(documentation) = &self.documentation {
            let documentation = string_to_cstring(documentation)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_SetDocumentation(raw_builder.raw, documentation.as_ptr())
            })?;
        }
        if let Some(module_version) = &self.module_version {
            let module_version = string_to_cstring(module_version)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_SetModuleVersion(raw_builder.raw, module_version.as_ptr())
            })?;
        }
        if let Some(type_hints_json) = &self.type_hints_json {
            let type_hints_json = string_to_cstring(type_hints_json)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_SetTypeHintsJson(raw_builder.raw, type_hints_json.as_ptr())
            })?;
        }
        if self.min_runtime_abi != 0 || self.required_capabilities != 0 {
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_SetRuntimeRequirements(
                    raw_builder.raw,
                    self.min_runtime_abi,
                    self.required_capabilities,
                )
            })?;
        }

        for type_hint in &self.type_hints {
            let serialized = SerializedTypeHintDescriptor::new(type_hint)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_AddTypeHint(raw_builder.raw, &serialized.raw)
            })?;
        }
        for module_link in &self.module_links {
            let serialized = SerializedModuleLinkDescriptor::new(module_link)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_AddModuleLink(raw_builder.raw, &serialized.raw)
            })?;
        }
        for constant in &self.constants {
            let serialized = SerializedConstantDescriptor::new(constant)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_AddConstant(raw_builder.raw, &serialized.raw)
            })?;
        }
        for function in self.functions {
            let mut serialized = SerializedFunctionDescriptor::new(function)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_AddFunction(raw_builder.raw, &serialized.raw)
            })?;
            serialized.transfer();
        }
        for type_descriptor in self.types {
            let mut serialized = SerializedTypeDescriptor::new(type_descriptor)?;
            check_status(unsafe {
                sys::ZrRustBinding_NativeModuleBuilder_AddType(raw_builder.raw, &serialized.raw)
            })?;
            serialized.transfer();
        }

        let mut raw_module = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_NativeModuleBuilder_Build(raw_builder.raw, &mut raw_module) })?;
        Ok(NativeModule { raw: raw_module })
    }
}

pub struct NativeModule {
    raw: *mut sys::ZrRustBindingNativeModule,
}

impl Drop for NativeModule {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                let _ = sys::ZrRustBinding_NativeModule_Free(self.raw);
            }
            self.raw = ptr::null_mut();
        }
    }
}

pub struct NativeModuleRegistration {
    raw: *mut sys::ZrRustBindingRuntimeNativeModuleRegistration,
    _module: NativeModule,
}

impl Drop for NativeModuleRegistration {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                let _ = sys::ZrRustBinding_RuntimeNativeModuleRegistration_Free(self.raw);
            }
            self.raw = ptr::null_mut();
        }
    }
}

pub struct NativeCallContext {
    raw: *mut sys::ZrRustBindingNativeCallContext,
}

impl NativeCallContext {
    pub fn module_name(&self) -> Result<String, Error> {
        read_string_with(|buffer, len| unsafe {
            sys::ZrRustBinding_NativeCallContext_GetModuleName(self.raw, buffer, len)
        })
    }

    pub fn type_name(&self) -> Result<Option<String>, Error> {
        read_optional_string_with(|buffer, len| unsafe {
            sys::ZrRustBinding_NativeCallContext_GetTypeName(self.raw, buffer, len)
        })
    }

    pub fn callable_name(&self) -> Result<String, Error> {
        read_string_with(|buffer, len| unsafe {
            sys::ZrRustBinding_NativeCallContext_GetCallableName(self.raw, buffer, len)
        })
    }

    pub fn argument_count(&self) -> Result<usize, Error> {
        let mut count = 0usize;
        check_status(unsafe { sys::ZrRustBinding_NativeCallContext_GetArgumentCount(self.raw, &mut count) })?;
        Ok(count)
    }

    pub fn check_arity(&self, min_argument_count: usize, max_argument_count: usize) -> Result<(), Error> {
        check_status(unsafe {
            sys::ZrRustBinding_NativeCallContext_CheckArity(
                self.raw,
                min_argument_count,
                max_argument_count,
            )
        })
    }

    pub fn argument(&self, index: usize) -> Result<Value, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_NativeCallContext_GetArgument(self.raw, index, &mut raw) })?;
        Ok(Value { raw })
    }

    pub fn self_value(&self) -> Result<Option<Value>, Error> {
        let mut raw = ptr::null_mut();
        let status = unsafe { sys::ZrRustBinding_NativeCallContext_GetSelf(self.raw, &mut raw) };
        if status == sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_NOT_FOUND {
            return Ok(None);
        }
        check_status(status)?;
        Ok(Some(Value { raw }))
    }
}

impl Runtime {
    pub fn register_native_module(&mut self, module: NativeModule) -> Result<NativeModuleRegistration, Error> {
        let mut module = module;
        let mut raw = ptr::null_mut();
        check_status(unsafe {
            sys::ZrRustBinding_Runtime_RegisterNativeModule(self.raw, module.raw, &mut raw)
        })?;
        let module_raw = module.raw;
        module.raw = ptr::null_mut();
        Ok(NativeModuleRegistration {
            raw,
            _module: NativeModule { raw: module_raw },
        })
    }
}

struct RawModuleBuilder {
    raw: *mut sys::ZrRustBindingNativeModuleBuilder,
}

impl RawModuleBuilder {
    fn new(module_name: &str) -> Result<Self, Error> {
        let module_name = string_to_cstring(module_name)?;
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_NativeModuleBuilder_New(module_name.as_ptr(), &mut raw) })?;
        Ok(Self { raw })
    }
}

impl Drop for RawModuleBuilder {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                let _ = sys::ZrRustBinding_NativeModuleBuilder_Free(self.raw);
            }
            self.raw = ptr::null_mut();
        }
    }
}

struct SerializedCStringArray {
    _values: Vec<CString>,
    ptrs: Vec<*const c_char>,
}

impl SerializedCStringArray {
    fn new(values: &[String]) -> Result<Self, Error> {
        let c_values = values
            .iter()
            .map(|value| string_to_cstring(value))
            .collect::<Result<Vec<_>, _>>()?;
        let ptrs = c_values.iter().map(|value| value.as_ptr()).collect::<Vec<_>>();
        Ok(Self {
            _values: c_values,
            ptrs,
        })
    }

    fn as_ptr(&self) -> *const *const c_char {
        if self.ptrs.is_empty() {
            ptr::null()
        } else {
            self.ptrs.as_ptr()
        }
    }

    fn len(&self) -> usize {
        self.ptrs.len()
    }
}

struct SerializedTypeHintDescriptor {
    _symbol_name: CString,
    _symbol_kind: CString,
    _signature: CString,
    _documentation: Option<CString>,
    raw: sys::ZrRustBindingNativeTypeHintDescriptor,
}

impl SerializedTypeHintDescriptor {
    fn new(descriptor: &TypeHintDescriptor) -> Result<Self, Error> {
        let symbol_name = string_to_cstring(&descriptor.symbol_name)?;
        let symbol_kind = string_to_cstring(&descriptor.symbol_kind)?;
        let signature = string_to_cstring(&descriptor.signature)?;
        let documentation = descriptor
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let raw = sys::ZrRustBindingNativeTypeHintDescriptor {
            symbolName: symbol_name.as_ptr(),
            symbolKind: symbol_kind.as_ptr(),
            signature: signature.as_ptr(),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
        };
        Ok(Self {
            _symbol_name: symbol_name,
            _symbol_kind: symbol_kind,
            _signature: signature,
            _documentation: documentation,
            raw,
        })
    }
}

struct SerializedModuleLinkDescriptor {
    _name: CString,
    _module_name: CString,
    _documentation: Option<CString>,
    raw: sys::ZrRustBindingNativeModuleLinkDescriptor,
}

impl SerializedModuleLinkDescriptor {
    fn new(descriptor: &ModuleLinkDescriptor) -> Result<Self, Error> {
        let name = string_to_cstring(&descriptor.name)?;
        let module_name = string_to_cstring(&descriptor.module_name)?;
        let documentation = descriptor
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let raw = sys::ZrRustBindingNativeModuleLinkDescriptor {
            name: name.as_ptr(),
            moduleName: module_name.as_ptr(),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
        };
        Ok(Self {
            _name: name,
            _module_name: module_name,
            _documentation: documentation,
            raw,
        })
    }
}

struct SerializedConstantDescriptor {
    _name: CString,
    _string_value: Option<CString>,
    _documentation: Option<CString>,
    _type_name: Option<CString>,
    raw: sys::ZrRustBindingNativeConstantDescriptor,
}

impl SerializedConstantDescriptor {
    fn new(descriptor: &ConstantDescriptor) -> Result<Self, Error> {
        let name = string_to_cstring(&descriptor.name)?;
        let documentation = descriptor
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let type_name = descriptor
            .type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let string_value = match &descriptor.value {
            ConstantValue::String(value) => Some(string_to_cstring(value)?),
            _ => None,
        };

        let (kind, int_value, float_value, bool_value) = match &descriptor.value {
            ConstantValue::Null => (sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_NULL, 0, 0.0, 0),
            ConstantValue::Bool(value) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_BOOL,
                0,
                0.0,
                u8::from(*value),
            ),
            ConstantValue::Int(value) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_INT,
                *value,
                0.0,
                0,
            ),
            ConstantValue::Float(value) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_FLOAT,
                0,
                *value,
                0,
            ),
            ConstantValue::String(_) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_STRING,
                0,
                0.0,
                0,
            ),
        };

        let raw = sys::ZrRustBindingNativeConstantDescriptor {
            name: name.as_ptr(),
            kind,
            intValue: int_value,
            floatValue: float_value,
            stringValue: string_value.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            boolValue: bool_value,
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            typeName: type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
        };

        Ok(Self {
            _name: name,
            _string_value: string_value,
            _documentation: documentation,
            _type_name: type_name,
            raw,
        })
    }
}

struct SerializedFieldDescriptor {
    _name: CString,
    _type_name: CString,
    _documentation: Option<CString>,
    raw: sys::ZrRustBindingNativeFieldDescriptor,
}

impl SerializedFieldDescriptor {
    fn new(descriptor: &FieldDescriptor) -> Result<Self, Error> {
        let name = string_to_cstring(&descriptor.name)?;
        let type_name = string_to_cstring(&descriptor.type_name)?;
        let documentation = descriptor
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let raw = sys::ZrRustBindingNativeFieldDescriptor {
            name: name.as_ptr(),
            typeName: type_name.as_ptr(),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            contractRole: descriptor.contract_role,
        };
        Ok(Self {
            _name: name,
            _type_name: type_name,
            _documentation: documentation,
            raw,
        })
    }
}

struct SerializedParameterDescriptor {
    _name: CString,
    _type_name: CString,
    _documentation: Option<CString>,
    raw: sys::ZrRustBindingNativeParameterDescriptor,
}

impl SerializedParameterDescriptor {
    fn new(descriptor: &ParameterDescriptor) -> Result<Self, Error> {
        let name = string_to_cstring(&descriptor.name)?;
        let type_name = string_to_cstring(&descriptor.type_name)?;
        let documentation = descriptor
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let raw = sys::ZrRustBindingNativeParameterDescriptor {
            name: name.as_ptr(),
            typeName: type_name.as_ptr(),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
        };
        Ok(Self {
            _name: name,
            _type_name: type_name,
            _documentation: documentation,
            raw,
        })
    }
}

struct SerializedGenericParameterDescriptor {
    _name: CString,
    _documentation: Option<CString>,
    _constraint_type_names: SerializedCStringArray,
    raw: sys::ZrRustBindingNativeGenericParameterDescriptor,
}

impl SerializedGenericParameterDescriptor {
    fn new(descriptor: &GenericParameterDescriptor) -> Result<Self, Error> {
        let name = string_to_cstring(&descriptor.name)?;
        let documentation = descriptor
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let constraint_type_names = SerializedCStringArray::new(&descriptor.constraint_type_names)?;
        let raw = sys::ZrRustBindingNativeGenericParameterDescriptor {
            name: name.as_ptr(),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            constraintTypeNames: constraint_type_names.as_ptr(),
            constraintTypeCount: constraint_type_names.len(),
        };
        Ok(Self {
            _name: name,
            _documentation: documentation,
            _constraint_type_names: constraint_type_names,
            raw,
        })
    }
}

struct SerializedEnumMemberDescriptor {
    _name: CString,
    _string_value: Option<CString>,
    _documentation: Option<CString>,
    raw: sys::ZrRustBindingNativeEnumMemberDescriptor,
}

impl SerializedEnumMemberDescriptor {
    fn new(descriptor: &EnumMemberDescriptor) -> Result<Self, Error> {
        let name = string_to_cstring(&descriptor.name)?;
        let documentation = descriptor
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let string_value = match &descriptor.value {
            ConstantValue::String(value) => Some(string_to_cstring(value)?),
            _ => None,
        };
        let (kind, int_value, float_value, bool_value) = match &descriptor.value {
            ConstantValue::Null => (sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_NULL, 0, 0.0, 0),
            ConstantValue::Bool(value) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_BOOL,
                0,
                0.0,
                u8::from(*value),
            ),
            ConstantValue::Int(value) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_INT,
                *value,
                0.0,
                0,
            ),
            ConstantValue::Float(value) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_FLOAT,
                0,
                *value,
                0,
            ),
            ConstantValue::String(_) => (
                sys::ZrRustBindingNativeConstantKind::ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_STRING,
                0,
                0.0,
                0,
            ),
        };
        let raw = sys::ZrRustBindingNativeEnumMemberDescriptor {
            name: name.as_ptr(),
            kind,
            intValue: int_value,
            floatValue: float_value,
            stringValue: string_value.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            boolValue: bool_value,
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
        };
        Ok(Self {
            _name: name,
            _string_value: string_value,
            _documentation: documentation,
            raw,
        })
    }
}

struct CallbackUserData {
    callback: Box<NativeCallback>,
}

struct PendingCallbackUserData {
    pointer: *mut CallbackUserData,
    transferred: bool,
}

impl PendingCallbackUserData {
    fn new(callback: Box<NativeCallback>) -> Self {
        Self {
            pointer: Box::into_raw(Box::new(CallbackUserData { callback })),
            transferred: false,
        }
    }

    fn user_data(&self) -> *mut c_void {
        self.pointer.cast()
    }

    fn transfer(&mut self) {
        self.transferred = true;
    }
}

impl Drop for PendingCallbackUserData {
    fn drop(&mut self) {
        if !self.transferred && !self.pointer.is_null() {
            unsafe {
                drop(Box::from_raw(self.pointer));
            }
        }
    }
}

struct SerializedFunctionDescriptor {
    _name: CString,
    _return_type_name: Option<CString>,
    _documentation: Option<CString>,
    _parameters: Vec<SerializedParameterDescriptor>,
    _raw_parameters: Vec<sys::ZrRustBindingNativeParameterDescriptor>,
    _generic_parameters: Vec<SerializedGenericParameterDescriptor>,
    _raw_generic_parameters: Vec<sys::ZrRustBindingNativeGenericParameterDescriptor>,
    callback_user_data: PendingCallbackUserData,
    raw: sys::ZrRustBindingNativeFunctionDescriptor,
}

impl SerializedFunctionDescriptor {
    fn new(function: FunctionBuilder) -> Result<Self, Error> {
        let name = string_to_cstring(&function.name)?;
        let return_type_name = function
            .return_type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let documentation = function
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let parameters = function
            .parameters
            .iter()
            .map(SerializedParameterDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_parameters = parameters.iter().map(|value| value.raw).collect::<Vec<_>>();
        let generic_parameters = function
            .generic_parameters
            .iter()
            .map(SerializedGenericParameterDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_generic_parameters = generic_parameters.iter().map(|value| value.raw).collect::<Vec<_>>();
        let callback_user_data = PendingCallbackUserData::new(function.callback);
        let raw = sys::ZrRustBindingNativeFunctionDescriptor {
            name: name.as_ptr(),
            minArgumentCount: function.min_argument_count,
            maxArgumentCount: function.max_argument_count,
            callback: Some(native_callback_trampoline),
            userData: callback_user_data.user_data(),
            destroyUserData: Some(native_callback_destroy_trampoline),
            returnTypeName: return_type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            parameters: if raw_parameters.is_empty() {
                ptr::null()
            } else {
                raw_parameters.as_ptr()
            },
            parameterCount: raw_parameters.len(),
            genericParameters: if raw_generic_parameters.is_empty() {
                ptr::null()
            } else {
                raw_generic_parameters.as_ptr()
            },
            genericParameterCount: raw_generic_parameters.len(),
            contractRole: function.contract_role,
        };
        Ok(Self {
            _name: name,
            _return_type_name: return_type_name,
            _documentation: documentation,
            _parameters: parameters,
            _raw_parameters: raw_parameters,
            _generic_parameters: generic_parameters,
            _raw_generic_parameters: raw_generic_parameters,
            callback_user_data,
            raw,
        })
    }

    fn transfer(&mut self) {
        self.callback_user_data.transfer();
    }
}

struct SerializedMethodDescriptor {
    _name: CString,
    _return_type_name: Option<CString>,
    _documentation: Option<CString>,
    _parameters: Vec<SerializedParameterDescriptor>,
    _raw_parameters: Vec<sys::ZrRustBindingNativeParameterDescriptor>,
    _generic_parameters: Vec<SerializedGenericParameterDescriptor>,
    _raw_generic_parameters: Vec<sys::ZrRustBindingNativeGenericParameterDescriptor>,
    callback_user_data: PendingCallbackUserData,
    raw: sys::ZrRustBindingNativeMethodDescriptor,
}

impl SerializedMethodDescriptor {
    fn new(method: MethodSpec) -> Result<Self, Error> {
        let name = string_to_cstring(&method.function.name)?;
        let return_type_name = method
            .function
            .return_type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let documentation = method
            .function
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let parameters = method
            .function
            .parameters
            .iter()
            .map(SerializedParameterDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_parameters = parameters.iter().map(|value| value.raw).collect::<Vec<_>>();
        let generic_parameters = method
            .function
            .generic_parameters
            .iter()
            .map(SerializedGenericParameterDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_generic_parameters = generic_parameters.iter().map(|value| value.raw).collect::<Vec<_>>();
        let callback_user_data = PendingCallbackUserData::new(method.function.callback);
        let raw = sys::ZrRustBindingNativeMethodDescriptor {
            name: name.as_ptr(),
            minArgumentCount: method.function.min_argument_count,
            maxArgumentCount: method.function.max_argument_count,
            callback: Some(native_callback_trampoline),
            userData: callback_user_data.user_data(),
            destroyUserData: Some(native_callback_destroy_trampoline),
            returnTypeName: return_type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            isStatic: u8::from(method.is_static),
            parameters: if raw_parameters.is_empty() {
                ptr::null()
            } else {
                raw_parameters.as_ptr()
            },
            parameterCount: raw_parameters.len(),
            contractRole: method.function.contract_role,
            genericParameters: if raw_generic_parameters.is_empty() {
                ptr::null()
            } else {
                raw_generic_parameters.as_ptr()
            },
            genericParameterCount: raw_generic_parameters.len(),
        };
        Ok(Self {
            _name: name,
            _return_type_name: return_type_name,
            _documentation: documentation,
            _parameters: parameters,
            _raw_parameters: raw_parameters,
            _generic_parameters: generic_parameters,
            _raw_generic_parameters: raw_generic_parameters,
            callback_user_data,
            raw,
        })
    }

    fn transfer(&mut self) {
        self.callback_user_data.transfer();
    }
}

struct SerializedMetaMethodDescriptor {
    _return_type_name: Option<CString>,
    _documentation: Option<CString>,
    _parameters: Vec<SerializedParameterDescriptor>,
    _raw_parameters: Vec<sys::ZrRustBindingNativeParameterDescriptor>,
    _generic_parameters: Vec<SerializedGenericParameterDescriptor>,
    _raw_generic_parameters: Vec<sys::ZrRustBindingNativeGenericParameterDescriptor>,
    callback_user_data: PendingCallbackUserData,
    raw: sys::ZrRustBindingNativeMetaMethodDescriptor,
}

impl SerializedMetaMethodDescriptor {
    fn new(method: MetaMethodBuilder) -> Result<Self, Error> {
        let return_type_name = method
            .return_type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let documentation = method
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let parameters = method
            .parameters
            .iter()
            .map(SerializedParameterDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_parameters = parameters.iter().map(|value| value.raw).collect::<Vec<_>>();
        let generic_parameters = method
            .generic_parameters
            .iter()
            .map(SerializedGenericParameterDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_generic_parameters = generic_parameters.iter().map(|value| value.raw).collect::<Vec<_>>();
        let callback_user_data = PendingCallbackUserData::new(method.callback);
        let raw = sys::ZrRustBindingNativeMetaMethodDescriptor {
            metaType: map_meta_method_type(method.meta_type),
            minArgumentCount: method.min_argument_count,
            maxArgumentCount: method.max_argument_count,
            callback: Some(native_callback_trampoline),
            userData: callback_user_data.user_data(),
            destroyUserData: Some(native_callback_destroy_trampoline),
            returnTypeName: return_type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            parameters: if raw_parameters.is_empty() {
                ptr::null()
            } else {
                raw_parameters.as_ptr()
            },
            parameterCount: raw_parameters.len(),
            genericParameters: if raw_generic_parameters.is_empty() {
                ptr::null()
            } else {
                raw_generic_parameters.as_ptr()
            },
            genericParameterCount: raw_generic_parameters.len(),
        };
        Ok(Self {
            _return_type_name: return_type_name,
            _documentation: documentation,
            _parameters: parameters,
            _raw_parameters: raw_parameters,
            _generic_parameters: generic_parameters,
            _raw_generic_parameters: raw_generic_parameters,
            callback_user_data,
            raw,
        })
    }

    fn transfer(&mut self) {
        self.callback_user_data.transfer();
    }
}

struct SerializedTypeDescriptor {
    _name: CString,
    _documentation: Option<CString>,
    _extends_type_name: Option<CString>,
    _implements_type_names: SerializedCStringArray,
    _enum_value_type_name: Option<CString>,
    _constructor_signature: Option<CString>,
    _ffi_lowering_kind: Option<CString>,
    _ffi_view_type_name: Option<CString>,
    _ffi_underlying_type_name: Option<CString>,
    _ffi_owner_mode: Option<CString>,
    _ffi_release_hook: Option<CString>,
    _fields: Vec<SerializedFieldDescriptor>,
    _raw_fields: Vec<sys::ZrRustBindingNativeFieldDescriptor>,
    methods: Vec<SerializedMethodDescriptor>,
    _raw_methods: Vec<sys::ZrRustBindingNativeMethodDescriptor>,
    meta_methods: Vec<SerializedMetaMethodDescriptor>,
    _raw_meta_methods: Vec<sys::ZrRustBindingNativeMetaMethodDescriptor>,
    _enum_members: Vec<SerializedEnumMemberDescriptor>,
    _raw_enum_members: Vec<sys::ZrRustBindingNativeEnumMemberDescriptor>,
    _generic_parameters: Vec<SerializedGenericParameterDescriptor>,
    _raw_generic_parameters: Vec<sys::ZrRustBindingNativeGenericParameterDescriptor>,
    raw: sys::ZrRustBindingNativeTypeDescriptor,
}

impl SerializedTypeDescriptor {
    fn new(type_builder: TypeBuilder) -> Result<Self, Error> {
        let name = string_to_cstring(&type_builder.name)?;
        let documentation = type_builder
            .documentation
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let extends_type_name = type_builder
            .extends_type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let implements_type_names = SerializedCStringArray::new(&type_builder.implements_type_names)?;
        let enum_value_type_name = type_builder
            .enum_value_type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let constructor_signature = type_builder
            .constructor_signature
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let ffi_lowering_kind = type_builder
            .ffi_metadata
            .lowering_kind
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let ffi_view_type_name = type_builder
            .ffi_metadata
            .view_type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let ffi_underlying_type_name = type_builder
            .ffi_metadata
            .underlying_type_name
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let ffi_owner_mode = type_builder
            .ffi_metadata
            .owner_mode
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let ffi_release_hook = type_builder
            .ffi_metadata
            .release_hook
            .as_ref()
            .map(|value| string_to_cstring(value))
            .transpose()?;
        let fields = type_builder
            .fields
            .iter()
            .map(SerializedFieldDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_fields = fields.iter().map(|value| value.raw).collect::<Vec<_>>();
        let methods = type_builder
            .methods
            .into_iter()
            .map(SerializedMethodDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_methods = methods.iter().map(|value| value.raw).collect::<Vec<_>>();
        let meta_methods = type_builder
            .meta_methods
            .into_iter()
            .map(SerializedMetaMethodDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_meta_methods = meta_methods.iter().map(|value| value.raw).collect::<Vec<_>>();
        let enum_members = type_builder
            .enum_members
            .iter()
            .map(SerializedEnumMemberDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_enum_members = enum_members.iter().map(|value| value.raw).collect::<Vec<_>>();
        let generic_parameters = type_builder
            .generic_parameters
            .iter()
            .map(SerializedGenericParameterDescriptor::new)
            .collect::<Result<Vec<_>, _>>()?;
        let raw_generic_parameters = generic_parameters.iter().map(|value| value.raw).collect::<Vec<_>>();
        let raw = sys::ZrRustBindingNativeTypeDescriptor {
            name: name.as_ptr(),
            prototypeType: map_prototype_type(type_builder.prototype_type),
            fields: if raw_fields.is_empty() { ptr::null() } else { raw_fields.as_ptr() },
            fieldCount: raw_fields.len(),
            methods: if raw_methods.is_empty() { ptr::null() } else { raw_methods.as_ptr() },
            methodCount: raw_methods.len(),
            metaMethods: if raw_meta_methods.is_empty() { ptr::null() } else { raw_meta_methods.as_ptr() },
            metaMethodCount: raw_meta_methods.len(),
            documentation: documentation.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            extendsTypeName: extends_type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            implementsTypeNames: implements_type_names.as_ptr(),
            implementsTypeCount: implements_type_names.len(),
            enumMembers: if raw_enum_members.is_empty() { ptr::null() } else { raw_enum_members.as_ptr() },
            enumMemberCount: raw_enum_members.len(),
            enumValueTypeName: enum_value_type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            allowValueConstruction: u8::from(type_builder.allow_value_construction),
            allowBoxedConstruction: u8::from(type_builder.allow_boxed_construction),
            constructorSignature: constructor_signature.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            genericParameters: if raw_generic_parameters.is_empty() { ptr::null() } else { raw_generic_parameters.as_ptr() },
            genericParameterCount: raw_generic_parameters.len(),
            protocolMask: type_builder.protocol_mask,
            ffiLoweringKind: ffi_lowering_kind.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            ffiViewTypeName: ffi_view_type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            ffiUnderlyingTypeName: ffi_underlying_type_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            ffiOwnerMode: ffi_owner_mode.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
            ffiReleaseHook: ffi_release_hook.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
        };
        Ok(Self {
            _name: name,
            _documentation: documentation,
            _extends_type_name: extends_type_name,
            _implements_type_names: implements_type_names,
            _enum_value_type_name: enum_value_type_name,
            _constructor_signature: constructor_signature,
            _ffi_lowering_kind: ffi_lowering_kind,
            _ffi_view_type_name: ffi_view_type_name,
            _ffi_underlying_type_name: ffi_underlying_type_name,
            _ffi_owner_mode: ffi_owner_mode,
            _ffi_release_hook: ffi_release_hook,
            _fields: fields,
            _raw_fields: raw_fields,
            methods,
            _raw_methods: raw_methods,
            meta_methods,
            _raw_meta_methods: raw_meta_methods,
            _enum_members: enum_members,
            _raw_enum_members: raw_enum_members,
            _generic_parameters: generic_parameters,
            _raw_generic_parameters: raw_generic_parameters,
            raw,
        })
    }

    fn transfer(&mut self) {
        for method in &mut self.methods {
            method.transfer();
        }
        for meta_method in &mut self.meta_methods {
            meta_method.transfer();
        }
    }
}

fn read_optional_string_with<F>(mut call: F) -> Result<Option<String>, Error>
where
    F: FnMut(*mut c_char, usize) -> sys::ZrRustBindingStatus,
{
    let mut buffer = vec![0 as c_char; 4096];
    let status = call(buffer.as_mut_ptr(), buffer.len());
    if status == sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_NOT_FOUND {
        return Ok(None);
    }
    check_status(status)?;
    Ok(Some(
        unsafe { CStr::from_ptr(buffer.as_ptr()) }
            .to_string_lossy()
            .into_owned(),
    ))
}

fn map_prototype_type(value: PrototypeType) -> sys::ZrRustBindingPrototypeType {
    match value {
        PrototypeType::Invalid => sys::ZrRustBindingPrototypeType::ZR_RUST_BINDING_PROTOTYPE_TYPE_INVALID,
        PrototypeType::Module => sys::ZrRustBindingPrototypeType::ZR_RUST_BINDING_PROTOTYPE_TYPE_MODULE,
        PrototypeType::Class => sys::ZrRustBindingPrototypeType::ZR_RUST_BINDING_PROTOTYPE_TYPE_CLASS,
        PrototypeType::Interface => sys::ZrRustBindingPrototypeType::ZR_RUST_BINDING_PROTOTYPE_TYPE_INTERFACE,
        PrototypeType::Struct => sys::ZrRustBindingPrototypeType::ZR_RUST_BINDING_PROTOTYPE_TYPE_STRUCT,
        PrototypeType::Enum => sys::ZrRustBindingPrototypeType::ZR_RUST_BINDING_PROTOTYPE_TYPE_ENUM,
        PrototypeType::Native => sys::ZrRustBindingPrototypeType::ZR_RUST_BINDING_PROTOTYPE_TYPE_NATIVE,
    }
}

fn map_meta_method_type(value: MetaMethodType) -> sys::ZrRustBindingMetaMethodType {
    match value {
        MetaMethodType::Constructor => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_CONSTRUCTOR,
        MetaMethodType::Destructor => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_DESTRUCTOR,
        MetaMethodType::Add => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_ADD,
        MetaMethodType::Sub => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_SUB,
        MetaMethodType::Mul => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_MUL,
        MetaMethodType::Div => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_DIV,
        MetaMethodType::Mod => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_MOD,
        MetaMethodType::Pow => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_POW,
        MetaMethodType::Neg => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_NEG,
        MetaMethodType::Compare => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_COMPARE,
        MetaMethodType::ToBool => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_TO_BOOL,
        MetaMethodType::ToString => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_TO_STRING,
        MetaMethodType::ToInt => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_TO_INT,
        MetaMethodType::ToUint => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_TO_UINT,
        MetaMethodType::ToFloat => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_TO_FLOAT,
        MetaMethodType::Call => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_CALL,
        MetaMethodType::Getter => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_GETTER,
        MetaMethodType::Setter => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_SETTER,
        MetaMethodType::ShiftLeft => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_SHIFT_LEFT,
        MetaMethodType::ShiftRight => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_SHIFT_RIGHT,
        MetaMethodType::BitAnd => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_BIT_AND,
        MetaMethodType::BitOr => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_BIT_OR,
        MetaMethodType::BitXor => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_BIT_XOR,
        MetaMethodType::BitNot => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_BIT_NOT,
        MetaMethodType::GetItem => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_GET_ITEM,
        MetaMethodType::SetItem => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_SET_ITEM,
        MetaMethodType::Close => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_CLOSE,
        MetaMethodType::Decorate => sys::ZrRustBindingMetaMethodType::ZR_RUST_BINDING_META_METHOD_DECORATE,
    }
}

unsafe extern "C" fn native_callback_trampoline(
    context: *mut sys::ZrRustBindingNativeCallContext,
    user_data: *mut c_void,
    out_result: *mut *mut sys::ZrRustBindingValue,
) -> sys::ZrRustBindingStatus {
    if user_data.is_null() || out_result.is_null() {
        return sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT;
    }

    *out_result = ptr::null_mut();
    let callback = &*(user_data as *const CallbackUserData);
    let context = NativeCallContext { raw: context };
    match catch_unwind(AssertUnwindSafe(|| (callback.callback)(&context))) {
        Ok(Ok(mut value)) => {
            *out_result = value.raw;
            value.raw = ptr::null_mut();
            sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_OK
        }
        Ok(Err(error)) => error.status,
        Err(_) => sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
    }
}

unsafe extern "C" fn native_callback_destroy_trampoline(user_data: *mut c_void) {
    if !user_data.is_null() {
        drop(Box::from_raw(user_data as *mut CallbackUserData));
    }
}
