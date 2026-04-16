use std::ffi::c_char;

use super::{
    TZrBool, TZrFloat64, TZrInt64, TZrPtr, TZrSize, TZrUInt16, TZrUInt32, TZrUInt64,
    ZrRustBindingRuntime, ZrRustBindingStatus, ZrRustBindingValue,
};

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ZrRustBindingNativeConstantKind {
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_NULL = 0,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_BOOL = 1,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_INT = 2,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_FLOAT = 3,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_STRING = 4,
    ZR_RUST_BINDING_NATIVE_CONSTANT_KIND_ARRAY = 5,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ZrRustBindingPrototypeType {
    ZR_RUST_BINDING_PROTOTYPE_TYPE_INVALID = 0,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_MODULE = 1,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_CLASS = 2,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_INTERFACE = 3,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_STRUCT = 4,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_ENUM = 5,
    ZR_RUST_BINDING_PROTOTYPE_TYPE_NATIVE = 6,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ZrRustBindingMetaMethodType {
    ZR_RUST_BINDING_META_METHOD_CONSTRUCTOR = 0,
    ZR_RUST_BINDING_META_METHOD_DESTRUCTOR = 1,
    ZR_RUST_BINDING_META_METHOD_ADD = 2,
    ZR_RUST_BINDING_META_METHOD_SUB = 3,
    ZR_RUST_BINDING_META_METHOD_MUL = 4,
    ZR_RUST_BINDING_META_METHOD_DIV = 5,
    ZR_RUST_BINDING_META_METHOD_MOD = 6,
    ZR_RUST_BINDING_META_METHOD_POW = 7,
    ZR_RUST_BINDING_META_METHOD_NEG = 8,
    ZR_RUST_BINDING_META_METHOD_COMPARE = 9,
    ZR_RUST_BINDING_META_METHOD_TO_BOOL = 10,
    ZR_RUST_BINDING_META_METHOD_TO_STRING = 11,
    ZR_RUST_BINDING_META_METHOD_TO_INT = 12,
    ZR_RUST_BINDING_META_METHOD_TO_UINT = 13,
    ZR_RUST_BINDING_META_METHOD_TO_FLOAT = 14,
    ZR_RUST_BINDING_META_METHOD_CALL = 15,
    ZR_RUST_BINDING_META_METHOD_GETTER = 16,
    ZR_RUST_BINDING_META_METHOD_SETTER = 17,
    ZR_RUST_BINDING_META_METHOD_SHIFT_LEFT = 18,
    ZR_RUST_BINDING_META_METHOD_SHIFT_RIGHT = 19,
    ZR_RUST_BINDING_META_METHOD_BIT_AND = 20,
    ZR_RUST_BINDING_META_METHOD_BIT_OR = 21,
    ZR_RUST_BINDING_META_METHOD_BIT_XOR = 22,
    ZR_RUST_BINDING_META_METHOD_BIT_NOT = 23,
    ZR_RUST_BINDING_META_METHOD_GET_ITEM = 24,
    ZR_RUST_BINDING_META_METHOD_SET_ITEM = 25,
    ZR_RUST_BINDING_META_METHOD_CLOSE = 26,
    ZR_RUST_BINDING_META_METHOD_DECORATE = 27,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeTypeHintDescriptor {
    pub symbolName: *const c_char,
    pub symbolKind: *const c_char,
    pub signature: *const c_char,
    pub documentation: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeFieldDescriptor {
    pub name: *const c_char,
    pub typeName: *const c_char,
    pub documentation: *const c_char,
    pub contractRole: TZrUInt32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeParameterDescriptor {
    pub name: *const c_char,
    pub typeName: *const c_char,
    pub documentation: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeGenericParameterDescriptor {
    pub name: *const c_char,
    pub documentation: *const c_char,
    pub constraintTypeNames: *const *const c_char,
    pub constraintTypeCount: TZrSize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeEnumMemberDescriptor {
    pub name: *const c_char,
    pub kind: ZrRustBindingNativeConstantKind,
    pub intValue: TZrInt64,
    pub floatValue: TZrFloat64,
    pub stringValue: *const c_char,
    pub boolValue: TZrBool,
    pub documentation: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeConstantDescriptor {
    pub name: *const c_char,
    pub kind: ZrRustBindingNativeConstantKind,
    pub intValue: TZrInt64,
    pub floatValue: TZrFloat64,
    pub stringValue: *const c_char,
    pub boolValue: TZrBool,
    pub documentation: *const c_char,
    pub typeName: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeModuleLinkDescriptor {
    pub name: *const c_char,
    pub moduleName: *const c_char,
    pub documentation: *const c_char,
}

#[repr(C)]
pub struct ZrRustBindingNativeCallContext {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ZrRustBindingNativeModuleBuilder {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ZrRustBindingNativeModule {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ZrRustBindingRuntimeNativeModuleRegistration {
    _private: [u8; 0],
}

pub type FZrRustBindingNativeCallback = Option<
    unsafe extern "C" fn(
        context: *mut ZrRustBindingNativeCallContext,
        userData: TZrPtr,
        outResult: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus,
>;

pub type FZrRustBindingDestroyCallback =
    Option<unsafe extern "C" fn(userData: TZrPtr)>;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeFunctionDescriptor {
    pub name: *const c_char,
    pub minArgumentCount: TZrUInt16,
    pub maxArgumentCount: TZrUInt16,
    pub callback: FZrRustBindingNativeCallback,
    pub userData: TZrPtr,
    pub destroyUserData: FZrRustBindingDestroyCallback,
    pub returnTypeName: *const c_char,
    pub documentation: *const c_char,
    pub parameters: *const ZrRustBindingNativeParameterDescriptor,
    pub parameterCount: TZrSize,
    pub genericParameters: *const ZrRustBindingNativeGenericParameterDescriptor,
    pub genericParameterCount: TZrSize,
    pub contractRole: TZrUInt32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeMethodDescriptor {
    pub name: *const c_char,
    pub minArgumentCount: TZrUInt16,
    pub maxArgumentCount: TZrUInt16,
    pub callback: FZrRustBindingNativeCallback,
    pub userData: TZrPtr,
    pub destroyUserData: FZrRustBindingDestroyCallback,
    pub returnTypeName: *const c_char,
    pub documentation: *const c_char,
    pub isStatic: TZrBool,
    pub parameters: *const ZrRustBindingNativeParameterDescriptor,
    pub parameterCount: TZrSize,
    pub contractRole: TZrUInt32,
    pub genericParameters: *const ZrRustBindingNativeGenericParameterDescriptor,
    pub genericParameterCount: TZrSize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeMetaMethodDescriptor {
    pub metaType: ZrRustBindingMetaMethodType,
    pub minArgumentCount: TZrUInt16,
    pub maxArgumentCount: TZrUInt16,
    pub callback: FZrRustBindingNativeCallback,
    pub userData: TZrPtr,
    pub destroyUserData: FZrRustBindingDestroyCallback,
    pub returnTypeName: *const c_char,
    pub documentation: *const c_char,
    pub parameters: *const ZrRustBindingNativeParameterDescriptor,
    pub parameterCount: TZrSize,
    pub genericParameters: *const ZrRustBindingNativeGenericParameterDescriptor,
    pub genericParameterCount: TZrSize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ZrRustBindingNativeTypeDescriptor {
    pub name: *const c_char,
    pub prototypeType: ZrRustBindingPrototypeType,
    pub fields: *const ZrRustBindingNativeFieldDescriptor,
    pub fieldCount: TZrSize,
    pub methods: *const ZrRustBindingNativeMethodDescriptor,
    pub methodCount: TZrSize,
    pub metaMethods: *const ZrRustBindingNativeMetaMethodDescriptor,
    pub metaMethodCount: TZrSize,
    pub documentation: *const c_char,
    pub extendsTypeName: *const c_char,
    pub implementsTypeNames: *const *const c_char,
    pub implementsTypeCount: TZrSize,
    pub enumMembers: *const ZrRustBindingNativeEnumMemberDescriptor,
    pub enumMemberCount: TZrSize,
    pub enumValueTypeName: *const c_char,
    pub allowValueConstruction: TZrBool,
    pub allowBoxedConstruction: TZrBool,
    pub constructorSignature: *const c_char,
    pub genericParameters: *const ZrRustBindingNativeGenericParameterDescriptor,
    pub genericParameterCount: TZrSize,
    pub protocolMask: TZrUInt64,
    pub ffiLoweringKind: *const c_char,
    pub ffiViewTypeName: *const c_char,
    pub ffiUnderlyingTypeName: *const c_char,
    pub ffiOwnerMode: *const c_char,
    pub ffiReleaseHook: *const c_char,
}

extern "C" {
    pub fn ZrRustBinding_NativeModuleBuilder_New(
        moduleName: *const c_char,
        outBuilder: *mut *mut ZrRustBindingNativeModuleBuilder,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_SetDocumentation(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        documentation: *const c_char,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_SetModuleVersion(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        moduleVersion: *const c_char,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_SetTypeHintsJson(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        typeHintsJson: *const c_char,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_SetRuntimeRequirements(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        minRuntimeAbi: TZrUInt32,
        requiredCapabilities: TZrUInt64,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_AddTypeHint(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        descriptor: *const ZrRustBindingNativeTypeHintDescriptor,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_AddModuleLink(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        descriptor: *const ZrRustBindingNativeModuleLinkDescriptor,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_AddConstant(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        descriptor: *const ZrRustBindingNativeConstantDescriptor,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_AddFunction(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        descriptor: *const ZrRustBindingNativeFunctionDescriptor,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_AddType(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        descriptor: *const ZrRustBindingNativeTypeDescriptor,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_Build(
        builder: *mut ZrRustBindingNativeModuleBuilder,
        outModule: *mut *mut ZrRustBindingNativeModule,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModuleBuilder_Free(
        builder: *mut ZrRustBindingNativeModuleBuilder,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeModule_Free(
        module: *mut ZrRustBindingNativeModule,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_Runtime_RegisterNativeModule(
        runtime: *mut ZrRustBindingRuntime,
        module: *mut ZrRustBindingNativeModule,
        outRegistration: *mut *mut ZrRustBindingRuntimeNativeModuleRegistration,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_RuntimeNativeModuleRegistration_Free(
        registration: *mut ZrRustBindingRuntimeNativeModuleRegistration,
    ) -> ZrRustBindingStatus;

    pub fn ZrRustBinding_NativeCallContext_GetModuleName(
        context: *const ZrRustBindingNativeCallContext,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeCallContext_GetTypeName(
        context: *const ZrRustBindingNativeCallContext,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeCallContext_GetCallableName(
        context: *const ZrRustBindingNativeCallContext,
        buffer: *mut c_char,
        bufferSize: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeCallContext_GetArgumentCount(
        context: *const ZrRustBindingNativeCallContext,
        outArgumentCount: *mut TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeCallContext_CheckArity(
        context: *const ZrRustBindingNativeCallContext,
        minArgumentCount: TZrSize,
        maxArgumentCount: TZrSize,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeCallContext_GetArgument(
        context: *const ZrRustBindingNativeCallContext,
        index: TZrSize,
        outArgumentValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
    pub fn ZrRustBinding_NativeCallContext_GetSelf(
        context: *const ZrRustBindingNativeCallContext,
        outSelfValue: *mut *mut ZrRustBindingValue,
    ) -> ZrRustBindingStatus;
}
