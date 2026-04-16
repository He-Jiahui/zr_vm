use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::c_char;
use std::path::{Path, PathBuf};
use std::ptr;

use zr_vm_rust_binding_sys as sys;

mod native;
pub use native::*;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ExecutionMode {
    Interp,
    Binary,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ValueKind {
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

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum OwnershipKind {
    None,
    Unique,
    Shared,
    Weak,
    Borrowed,
    Loaned,
}

#[derive(Debug, Clone)]
pub struct Error {
    pub status: sys::ZrRustBindingStatus,
    pub message: String,
}

impl Error {
    pub fn new(status: sys::ZrRustBindingStatus, message: impl Into<String>) -> Self {
        Self {
            status,
            message: message.into(),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:?}: {}", self.status, self.message)
    }
}

impl std::error::Error for Error {}

#[derive(Clone, Copy, Debug, Default)]
pub struct RuntimeOptions {
    pub heap_limit_bytes: u64,
    pub pause_budget_us: u64,
    pub remark_budget_us: u64,
    pub worker_count: u32,
}

#[derive(Clone, Debug, Default)]
pub struct CompileOptions {
    pub emit_intermediate: bool,
    pub incremental: bool,
}

#[derive(Clone, Debug)]
pub struct RunOptions {
    pub execution_mode: ExecutionMode,
    pub module_name: Option<String>,
    pub program_args: Vec<String>,
}

impl Default for RunOptions {
    fn default() -> Self {
        Self {
            execution_mode: ExecutionMode::Interp,
            module_name: None,
            program_args: Vec::new(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CompileResult {
    pub compiled: usize,
    pub skipped: usize,
    pub removed: usize,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ArtifactPaths {
    pub zro: PathBuf,
    pub zri: PathBuf,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ManifestEntry {
    pub module_name: String,
    pub source_hash: String,
    pub zro_hash: String,
    pub zro_path: PathBuf,
    pub zri_path: PathBuf,
    pub imports: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Manifest {
    pub version: u32,
    pub entries: Vec<ManifestEntry>,
}

fn last_error() -> Error {
    let mut error = sys::ZrRustBindingErrorInfo {
        status: sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
        message: [0; 512],
    };
    unsafe { sys::ZrRustBinding_GetLastErrorInfo(&mut error) };
    let message = unsafe { CStr::from_ptr(error.message.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    Error {
        status: error.status,
        message,
    }
}

fn check_status(status: sys::ZrRustBindingStatus) -> Result<(), Error> {
    if status == sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_OK {
        Ok(())
    } else {
        Err(last_error())
    }
}

fn path_to_cstring(path: &Path) -> Result<CString, Error> {
    CString::new(path.to_string_lossy().as_bytes()).map_err(|_| Error {
        status: sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
        message: format!("path contains interior NUL: {}", path.display()),
    })
}

fn string_to_cstring(value: &str) -> Result<CString, Error> {
    CString::new(value).map_err(|_| Error {
        status: sys::ZrRustBindingStatus::ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
        message: "string contains interior NUL".to_string(),
    })
}

fn read_string_with<F>(mut call: F) -> Result<String, Error>
where
    F: FnMut(*mut c_char, usize) -> sys::ZrRustBindingStatus,
{
    let mut buffer = vec![0 as c_char; 4096];
    check_status(call(buffer.as_mut_ptr(), buffer.len()))?;
    Ok(unsafe { CStr::from_ptr(buffer.as_ptr()) }
        .to_string_lossy()
        .into_owned())
}

fn to_sys_runtime_options(options: RuntimeOptions) -> sys::ZrRustBindingRuntimeOptions {
    sys::ZrRustBindingRuntimeOptions {
        heapLimitBytes: options.heap_limit_bytes,
        pauseBudgetUs: options.pause_budget_us,
        remarkBudgetUs: options.remark_budget_us,
        workerCount: options.worker_count,
    }
}

struct RunOptionsOwned {
    sys_options: sys::ZrRustBindingRunOptions,
    _args: Vec<CString>,
    _arg_ptrs: Vec<*const c_char>,
    _module_name: Option<CString>,
}

fn to_sys_run_options(options: &RunOptions) -> Result<RunOptionsOwned, Error> {
    let module_name = match &options.module_name {
        Some(name) => Some(string_to_cstring(name)?),
        None => None,
    };
    let args = options
        .program_args
        .iter()
        .map(|arg| string_to_cstring(arg))
        .collect::<Result<Vec<_>, _>>()?;
    let arg_ptrs = args.iter().map(|arg| arg.as_ptr()).collect::<Vec<_>>();
    let sys_options = sys::ZrRustBindingRunOptions {
        executionMode: match options.execution_mode {
            ExecutionMode::Interp => sys::ZrRustBindingExecutionMode::ZR_RUST_BINDING_EXECUTION_MODE_INTERP,
            ExecutionMode::Binary => sys::ZrRustBindingExecutionMode::ZR_RUST_BINDING_EXECUTION_MODE_BINARY,
        },
        moduleName: module_name.as_ref().map_or(ptr::null(), |value| value.as_ptr()),
        programArgs: if arg_ptrs.is_empty() {
            ptr::null()
        } else {
            arg_ptrs.as_ptr()
        },
        programArgCount: arg_ptrs.len(),
    };
    Ok(RunOptionsOwned {
        sys_options,
        _args: args,
        _arg_ptrs: arg_ptrs,
        _module_name: module_name,
    })
}

pub struct RuntimeBuilder {
    options: RuntimeOptions,
    standard: bool,
}

impl RuntimeBuilder {
    pub fn bare() -> Self {
        Self {
            options: RuntimeOptions::default(),
            standard: false,
        }
    }

    pub fn standard() -> Self {
        Self {
            options: RuntimeOptions::default(),
            standard: true,
        }
    }

    pub fn options(mut self, options: RuntimeOptions) -> Self {
        self.options = options;
        self
    }

    pub fn build(self) -> Result<Runtime, Error> {
        let mut raw = ptr::null_mut();
        let options = to_sys_runtime_options(self.options);
        let status = unsafe {
            if self.standard {
                sys::ZrRustBinding_Runtime_NewStandard(&options, &mut raw)
            } else {
                sys::ZrRustBinding_Runtime_NewBare(&options, &mut raw)
            }
        };
        check_status(status)?;
        Ok(Runtime { raw })
    }
}

pub struct Runtime {
    raw: *mut sys::ZrRustBindingRuntime,
}

impl Drop for Runtime {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                let _ = sys::ZrRustBinding_Runtime_Free(self.raw);
            }
            self.raw = ptr::null_mut();
        }
    }
}

pub struct ProjectWorkspace {
    raw: *mut sys::ZrRustBindingProjectWorkspace,
}

impl ProjectWorkspace {
    pub fn scaffold(root: impl AsRef<Path>, project_name: &str) -> Result<Self, Error> {
        let root = path_to_cstring(root.as_ref())?;
        let project_name = string_to_cstring(project_name)?;
        let options = sys::ZrRustBindingScaffoldOptions {
            rootPath: root.as_ptr(),
            projectName: project_name.as_ptr(),
            overwriteExisting: 1,
        };
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Project_Scaffold(&options, &mut raw) })?;
        Ok(Self { raw })
    }

    pub fn open(project_path: impl AsRef<Path>) -> Result<Self, Error> {
        let project_path = path_to_cstring(project_path.as_ref())?;
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Project_Open(project_path.as_ptr(), &mut raw) })?;
        Ok(Self { raw })
    }

    pub fn project_path(&self) -> Result<PathBuf, Error> {
        Ok(PathBuf::from(read_string_with(|buffer, len| unsafe {
            sys::ZrRustBinding_ProjectWorkspace_GetProjectPath(self.raw, buffer, len)
        })?))
    }

    pub fn project_root(&self) -> Result<PathBuf, Error> {
        Ok(PathBuf::from(read_string_with(|buffer, len| unsafe {
            sys::ZrRustBinding_ProjectWorkspace_GetProjectRoot(self.raw, buffer, len)
        })?))
    }

    pub fn manifest_path(&self) -> Result<PathBuf, Error> {
        Ok(PathBuf::from(read_string_with(|buffer, len| unsafe {
            sys::ZrRustBinding_ProjectWorkspace_GetManifestPath(self.raw, buffer, len)
        })?))
    }

    pub fn entry_module(&self) -> Result<String, Error> {
        read_string_with(|buffer, len| unsafe {
            sys::ZrRustBinding_ProjectWorkspace_GetEntryModule(self.raw, buffer, len)
        })
    }

    pub fn resolve_artifacts(&self, module_name: Option<&str>) -> Result<ArtifactPaths, Error> {
        let module_name = module_name.map(string_to_cstring).transpose()?;
        let mut zro = vec![0 as c_char; 4096];
        let mut zri = vec![0 as c_char; 4096];
        check_status(unsafe {
            sys::ZrRustBinding_ProjectWorkspace_ResolveArtifacts(
                self.raw,
                module_name.as_ref().map_or(ptr::null(), |name| name.as_ptr()),
                zro.as_mut_ptr(),
                zro.len(),
                zri.as_mut_ptr(),
                zri.len(),
            )
        })?;
        Ok(ArtifactPaths {
            zro: PathBuf::from(unsafe { CStr::from_ptr(zro.as_ptr()) }.to_string_lossy().into_owned()),
            zri: PathBuf::from(unsafe { CStr::from_ptr(zri.as_ptr()) }.to_string_lossy().into_owned()),
        })
    }

    pub fn load_manifest(&self) -> Result<Manifest, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_ProjectWorkspace_LoadManifest(self.raw, &mut raw) })?;

        let mut version = 0u32;
        let mut entry_count = 0usize;
        check_status(unsafe { sys::ZrRustBinding_ManifestSnapshot_GetVersion(raw, &mut version) })?;
        check_status(unsafe { sys::ZrRustBinding_ManifestSnapshot_GetEntryCount(raw, &mut entry_count) })?;

        let manifest_result = (|| {
            let mut entries = Vec::with_capacity(entry_count);
            for entry_index in 0..entry_count {
                let module_name = read_string_with(|buffer, len| unsafe {
                    sys::ZrRustBinding_ManifestSnapshot_GetEntryModuleName(raw, entry_index, buffer, len)
                })?;
                let source_hash = read_string_with(|buffer, len| unsafe {
                    sys::ZrRustBinding_ManifestSnapshot_GetEntrySourceHash(raw, entry_index, buffer, len)
                })?;
                let zro_hash = read_string_with(|buffer, len| unsafe {
                    sys::ZrRustBinding_ManifestSnapshot_GetEntryZroHash(raw, entry_index, buffer, len)
                })?;
                let zro_path = PathBuf::from(read_string_with(|buffer, len| unsafe {
                    sys::ZrRustBinding_ManifestSnapshot_GetEntryZroPath(raw, entry_index, buffer, len)
                })?);
                let zri_path = PathBuf::from(read_string_with(|buffer, len| unsafe {
                    sys::ZrRustBinding_ManifestSnapshot_GetEntryZriPath(raw, entry_index, buffer, len)
                })?);
                let mut import_count = 0usize;
                check_status(unsafe {
                    sys::ZrRustBinding_ManifestSnapshot_GetEntryImportCount(raw, entry_index, &mut import_count)
                })?;
                let mut imports = Vec::with_capacity(import_count);
                for import_index in 0..import_count {
                    imports.push(read_string_with(|buffer, len| unsafe {
                        sys::ZrRustBinding_ManifestSnapshot_GetEntryImport(
                            raw,
                            entry_index,
                            import_index,
                            buffer,
                            len,
                        )
                    })?);
                }

                entries.push(ManifestEntry {
                    module_name,
                    source_hash,
                    zro_hash,
                    zro_path,
                    zri_path,
                    imports,
                });
            }

            Ok(Manifest { version, entries })
        })();

        unsafe {
            let _ = sys::ZrRustBinding_ManifestSnapshot_Free(raw);
        }
        manifest_result
    }

    pub fn compile(&self, runtime: &mut Runtime, options: &CompileOptions) -> Result<CompileResult, Error> {
        let options = sys::ZrRustBindingCompileOptions {
            emitIntermediate: options.emit_intermediate as u8,
            incremental: options.incremental as u8,
        };
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Project_Compile(runtime.raw, self.raw, &options, &mut raw) })?;
        let mut compiled = 0;
        let mut skipped = 0;
        let mut removed = 0;
        check_status(unsafe {
            sys::ZrRustBinding_CompileResult_GetCounts(raw, &mut compiled, &mut skipped, &mut removed)
        })?;
        unsafe {
            let _ = sys::ZrRustBinding_CompileResult_Free(raw);
        }
        Ok(CompileResult {
            compiled,
            skipped,
            removed,
        })
    }

    pub fn run(&self, runtime: &mut Runtime, options: &RunOptions) -> Result<Value, Error> {
        let owned = to_sys_run_options(options)?;
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Project_Run(runtime.raw, self.raw, &owned.sys_options, &mut raw) })?;
        Ok(Value { raw })
    }

    pub fn call_module_export(
        &self,
        runtime: &mut Runtime,
        options: &RunOptions,
        module_name: &str,
        export_name: &str,
        arguments: &[Value],
    ) -> Result<Value, Error> {
        let owned = to_sys_run_options(options)?;
        let module_name = string_to_cstring(module_name)?;
        let export_name = string_to_cstring(export_name)?;
        let arg_ptrs = arguments.iter().map(|value| value.raw).collect::<Vec<_>>();
        let mut raw = ptr::null_mut();
        check_status(unsafe {
            sys::ZrRustBinding_Project_CallModuleExport(
                runtime.raw,
                self.raw,
                &owned.sys_options,
                module_name.as_ptr(),
                export_name.as_ptr(),
                if arg_ptrs.is_empty() {
                    ptr::null()
                } else {
                    arg_ptrs.as_ptr()
                },
                arg_ptrs.len(),
                &mut raw,
            )
        })?;
        Ok(Value { raw })
    }
}

impl Drop for ProjectWorkspace {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                let _ = sys::ZrRustBinding_ProjectWorkspace_Free(self.raw);
            }
            self.raw = ptr::null_mut();
        }
    }
}

pub struct Value {
    raw: *mut sys::ZrRustBindingValue,
}

impl Value {
    pub fn new_null() -> Result<Self, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_NewNull(&mut raw) })?;
        Ok(Self { raw })
    }

    pub fn new_bool(value: bool) -> Result<Self, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_NewBool(value as u8, &mut raw) })?;
        Ok(Self { raw })
    }

    pub fn new_int(value: i64) -> Result<Self, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_NewInt(value, &mut raw) })?;
        Ok(Self { raw })
    }

    pub fn new_float(value: f64) -> Result<Self, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_NewFloat(value, &mut raw) })?;
        Ok(Self { raw })
    }

    pub fn new_string(value: &str) -> Result<Self, Error> {
        let value = string_to_cstring(value)?;
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_NewString(value.as_ptr(), &mut raw) })?;
        Ok(Self { raw })
    }

    pub fn new_array() -> Result<Self, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_NewArray(&mut raw) })?;
        Ok(Self { raw })
    }

    pub fn new_object() -> Result<Self, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_NewObject(&mut raw) })?;
        Ok(Self { raw })
    }

    pub fn kind(&self) -> ValueKind {
        match unsafe { sys::ZrRustBinding_Value_GetKind(self.raw) } {
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_NULL => ValueKind::Null,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_BOOL => ValueKind::Bool,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_INT => ValueKind::Int,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_FLOAT => ValueKind::Float,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_STRING => ValueKind::String,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_ARRAY => ValueKind::Array,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_OBJECT => ValueKind::Object,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_FUNCTION => ValueKind::Function,
            sys::ZrRustBindingValueKind::ZR_RUST_BINDING_VALUE_KIND_NATIVE_POINTER => ValueKind::NativePointer,
            _ => ValueKind::Unknown,
        }
    }

    pub fn ownership_kind(&self) -> OwnershipKind {
        match unsafe { sys::ZrRustBinding_Value_GetOwnershipKind(self.raw) } {
            sys::ZrRustBindingOwnershipKind::ZR_RUST_BINDING_OWNERSHIP_KIND_UNIQUE => OwnershipKind::Unique,
            sys::ZrRustBindingOwnershipKind::ZR_RUST_BINDING_OWNERSHIP_KIND_SHARED => OwnershipKind::Shared,
            sys::ZrRustBindingOwnershipKind::ZR_RUST_BINDING_OWNERSHIP_KIND_WEAK => OwnershipKind::Weak,
            sys::ZrRustBindingOwnershipKind::ZR_RUST_BINDING_OWNERSHIP_KIND_BORROWED => OwnershipKind::Borrowed,
            sys::ZrRustBindingOwnershipKind::ZR_RUST_BINDING_OWNERSHIP_KIND_LOANED => OwnershipKind::Loaned,
            _ => OwnershipKind::None,
        }
    }

    pub fn as_bool(&self) -> Result<bool, Error> {
        let mut value = 0u8;
        check_status(unsafe { sys::ZrRustBinding_Value_ReadBool(self.raw, &mut value) })?;
        Ok(value != 0)
    }

    pub fn as_int(&self) -> Result<i64, Error> {
        let mut value = 0i64;
        check_status(unsafe { sys::ZrRustBinding_Value_ReadInt(self.raw, &mut value) })?;
        Ok(value)
    }

    pub fn as_float(&self) -> Result<f64, Error> {
        let mut value = 0.0f64;
        check_status(unsafe { sys::ZrRustBinding_Value_ReadFloat(self.raw, &mut value) })?;
        Ok(value)
    }

    pub fn as_string(&self) -> Result<String, Error> {
        read_string_with(|buffer, len| unsafe { sys::ZrRustBinding_Value_ReadString(self.raw, buffer, len) })
    }

    pub fn array_len(&self) -> Result<usize, Error> {
        let mut length = 0usize;
        check_status(unsafe { sys::ZrRustBinding_Value_Array_Length(self.raw, &mut length) })?;
        Ok(length)
    }

    pub fn array_get(&self, index: usize) -> Result<Value, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_Array_Get(self.raw, index, &mut raw) })?;
        Ok(Value { raw })
    }

    pub fn array_push(&mut self, value: &Value) -> Result<(), Error> {
        check_status(unsafe { sys::ZrRustBinding_Value_Array_Push(self.raw, value.raw) })
    }

    pub fn object_get(&self, field_name: &str) -> Result<Value, Error> {
        let field_name = string_to_cstring(field_name)?;
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_Value_Object_Get(self.raw, field_name.as_ptr(), &mut raw) })?;
        Ok(Value { raw })
    }

    pub fn object_set(&mut self, field_name: &str, value: &Value) -> Result<(), Error> {
        let field_name = string_to_cstring(field_name)?;
        check_status(unsafe { sys::ZrRustBinding_Value_Object_Set(self.raw, field_name.as_ptr(), value.raw) })
    }
}

impl Drop for Value {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                let _ = sys::ZrRustBinding_Value_Free(self.raw);
            }
            self.raw = ptr::null_mut();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn scaffold_compile_and_run_roundtrip() -> Result<(), Box<dyn std::error::Error>> {
        let temp = tempfile::tempdir()?;
        let root = temp.path().join("roundtrip_project");
        let workspace = ProjectWorkspace::scaffold(&root, "roundtrip_project")?;
        let main_source = workspace.project_root()?.join("src").join("main.zr");
        fs::write(&main_source, "return \"rust safe roundtrip\";\n")?;
        assert_eq!(workspace.project_path()?, root.join("roundtrip_project.zrp"));
        assert_eq!(workspace.project_root()?, root);
        assert_eq!(workspace.entry_module()?, "main");
        assert_eq!(workspace.manifest_path()?, root.join("bin").join(".zr_cli_manifest"));

        let mut runtime = RuntimeBuilder::standard().build()?;
        let compile = workspace.compile(
            &mut runtime,
            &CompileOptions {
                emit_intermediate: true,
                incremental: true,
            },
        )?;
        assert!(compile.compiled > 0);
        assert_eq!(compile.removed, 0);

        let artifacts = workspace.resolve_artifacts(None)?;
        assert!(artifacts.zro.exists());
        assert!(artifacts.zri.exists());
        let manifest = workspace.load_manifest()?;
        assert_eq!(manifest.version, 2);
        assert_eq!(manifest.entries.len(), 1);
        assert_eq!(manifest.entries[0].module_name, "main");
        assert_eq!(manifest.entries[0].zro_path, artifacts.zro);
        assert_eq!(manifest.entries[0].zri_path, artifacts.zri);
        assert!(manifest.entries[0].imports.is_empty());

        let interp = workspace.run(
            &mut runtime,
            &RunOptions {
                execution_mode: ExecutionMode::Interp,
                ..RunOptions::default()
            },
        )?;
        assert_eq!(interp.kind(), ValueKind::String);
        assert_eq!(interp.ownership_kind(), OwnershipKind::None);
        assert_eq!(interp.as_string()?, "rust safe roundtrip");

        let binary = workspace.run(
            &mut runtime,
            &RunOptions {
                execution_mode: ExecutionMode::Binary,
                ..RunOptions::default()
            },
        )?;
        assert_eq!(binary.kind(), ValueKind::String);
        assert_eq!(binary.ownership_kind(), OwnershipKind::None);
        assert_eq!(binary.as_string()?, "rust safe roundtrip");
        Ok(())
    }

    #[test]
    fn owned_array_and_object_accessors_roundtrip() -> Result<(), Box<dyn std::error::Error>> {
        let mut array = Value::new_array()?;
        assert_eq!(array.kind(), ValueKind::Array);
        assert_eq!(array.ownership_kind(), OwnershipKind::None);
        array.array_push(&Value::new_int(7)?)?;
        array.array_push(&Value::new_int(11)?)?;
        assert_eq!(array.array_len()?, 2);
        let fetched = array.array_get(1)?;
        assert_eq!(fetched.kind(), ValueKind::Int);
        assert_eq!(fetched.ownership_kind(), OwnershipKind::None);
        assert_eq!(fetched.as_int()?, 11);

        let mut object = Value::new_object()?;
        assert_eq!(object.kind(), ValueKind::Object);
        assert_eq!(object.ownership_kind(), OwnershipKind::None);
        object.object_set("label", &Value::new_string("binding-object")?)?;
        let field = object.object_get("label")?;
        assert_eq!(field.kind(), ValueKind::String);
        assert_eq!(field.ownership_kind(), OwnershipKind::None);
        assert_eq!(field.as_string()?, "binding-object");
        Ok(())
    }

    #[test]
    fn scalar_value_kind_and_ownership_metadata_roundtrip() -> Result<(), Box<dyn std::error::Error>> {
        let null_value = Value::new_null()?;
        assert_eq!(null_value.kind(), ValueKind::Null);
        assert_eq!(null_value.ownership_kind(), OwnershipKind::None);

        let bool_value = Value::new_bool(true)?;
        assert_eq!(bool_value.kind(), ValueKind::Bool);
        assert_eq!(bool_value.ownership_kind(), OwnershipKind::None);
        assert!(bool_value.as_bool()?);

        let int_value = Value::new_int(42)?;
        assert_eq!(int_value.kind(), ValueKind::Int);
        assert_eq!(int_value.ownership_kind(), OwnershipKind::None);
        assert_eq!(int_value.as_int()?, 42);

        let float_value = Value::new_float(3.5)?;
        assert_eq!(float_value.kind(), ValueKind::Float);
        assert_eq!(float_value.ownership_kind(), OwnershipKind::None);
        assert!((float_value.as_float()? - 3.5).abs() < 1e-9);

        let string_value = Value::new_string("owned-metadata")?;
        assert_eq!(string_value.kind(), ValueKind::String);
        assert_eq!(string_value.ownership_kind(), OwnershipKind::None);
        assert_eq!(string_value.as_string()?, "owned-metadata");
        Ok(())
    }

    #[test]
    fn call_module_export_roundtrip() -> Result<(), Box<dyn std::error::Error>> {
        let temp = tempfile::tempdir()?;
        let root = temp.path().join("call_export_project");
        let workspace = ProjectWorkspace::scaffold(&root, "call_export_project")?;
        let mut runtime = RuntimeBuilder::standard().build()?;
        let result = workspace.call_module_export(
            &mut runtime,
            &RunOptions::default(),
            "zr.math",
            "sqrt",
            &[Value::new_float(9.0)?],
        )?;

        assert_eq!(result.kind(), ValueKind::Float);
        assert_eq!(result.ownership_kind(), OwnershipKind::None);
        assert!((result.as_float()? - 3.0).abs() < 1e-9);
        Ok(())
    }
}
