use std::env;
use std::ffi::OsStr;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const LINK_KIND_ENV: &str = "ZR_VM_RUST_BINDING_LINK_KIND";
const STATIC_LIBRARIES_ENV: &str = "ZR_VM_RUST_BINDING_STATIC_LINK_LIBRARIES";
const STATIC_SYSTEM_LIBRARIES_ENV: &str = "ZR_VM_RUST_BINDING_STATIC_SYSTEM_LIBRARIES";

// sys crate 只链接 C ABI；有外部库目录时复用它，否则在 Cargo 构建期编译完整 native 目标。
// CMake supplies the link kind and, for static packages, the native dependency
// closure.  Keeping that choice explicit prevents a static SDK from silently
// falling back to the shared library name.
fn main() {
    validate_output_environment();
    println!("cargo:rerun-if-env-changed=ZR_VM_RUST_BINDING_LIB_DIR");
    println!("cargo:rerun-if-env-changed={LINK_KIND_ENV}");
    println!("cargo:rerun-if-env-changed={STATIC_LIBRARIES_ENV}");
    println!("cargo:rerun-if-env-changed={STATIC_SYSTEM_LIBRARIES_ENV}");

    let link_kind = requested_link_kind();
    let has_external_library_dir = env::var_os("ZR_VM_RUST_BINDING_LIB_DIR").is_some();

    if let Some(lib_dir) = env::var_os("ZR_VM_RUST_BINDING_LIB_DIR") {
        let lib_dir_path = Path::new(&lib_dir);
        assert!(
            !lib_dir.is_empty() && lib_dir_path.is_dir(),
            "ZR_VM_RUST_BINDING_LIB_DIR must name an existing import library directory"
        );
        validate_approved_path(lib_dir_path, "ZR_VM_RUST_BINDING_LIB_DIR");
        link_search(lib_dir_path);
    } else {
        build_native(link_kind);
    }

    emit_link_contract(link_kind, has_external_library_dir);
}

fn requested_link_kind() -> &'static str {
    match env::var(LINK_KIND_ENV).as_deref() {
        Ok("static") => "static",
        Ok("dylib") | Err(_) => "dylib",
        Ok(other) => panic!(
            "{LINK_KIND_ENV} must be exactly `dylib` or `static` (got `{other}`)"
        ),
    }
}

fn emit_link_contract(link_kind: &str, has_external_library_dir: bool) {
    if link_kind == "dylib" {
        println!("cargo:rustc-link-lib=dylib=zr_vm_rust_binding");
        return;
    }

    if has_external_library_dir {
        assert!(
            env::var_os(STATIC_LIBRARIES_ENV).is_some(),
            "{STATIC_LIBRARIES_ENV} is required when {LINK_KIND_ENV}=static and an external SDK is used"
        );
        assert!(
            env::var_os(STATIC_SYSTEM_LIBRARIES_ENV).is_some(),
            "{STATIC_SYSTEM_LIBRARIES_ENV} is required when {LINK_KIND_ENV}=static and an external SDK is used"
        );
    }

    println!("cargo:rustc-link-lib=static=zr_vm_rust_binding");
    let libraries = configured_static_libraries();
    for library in libraries {
        println!("cargo:rustc-link-lib=static={library}");
    }

    let system_libraries = configured_static_system_libraries();
    for library in system_libraries {
        // System dependencies are supplied by the platform SDK; the native
        // CMake target has already selected their platform link names.
        println!("cargo:rustc-link-lib={library}");
    }
}

fn native_link_contract() -> Result<(Vec<String>, Vec<String>), String> {
    let out_dir = env::var_os("OUT_DIR")
        .ok_or_else(|| "OUT_DIR is unavailable for the native static link contract".to_owned())?;
    let path = PathBuf::from(out_dir)
        .join("zr_vm-native")
        .join("zr_vm_rust_binding-static-link-contract.txt");
    let contents = fs::read_to_string(&path)
        .map_err(|error| format!("cannot read {}: {error}", path.display()))?;
    let mut lines = contents.lines();
    let library_line = lines
        .next()
        .filter(|line| !line.trim().is_empty())
        .ok_or_else(|| format!("{} has no static archive list", path.display()))?;
    let system_line = lines
        .next()
        .filter(|line| !line.trim().is_empty())
        .ok_or_else(|| format!("{} has no system library list", path.display()))?;
    let libraries = parse_csv(STATIC_LIBRARIES_ENV, library_line);
    let system_libraries = parse_csv(STATIC_SYSTEM_LIBRARIES_ENV, system_line);
    Ok((libraries, system_libraries))
}

fn configured_static_libraries() -> Vec<String> {
    if let Ok(value) = env::var(STATIC_LIBRARIES_ENV) {
        return parse_csv(STATIC_LIBRARIES_ENV, &value);
    }
    native_link_contract()
        .unwrap_or_else(|error| panic!("static native build has no generated dependency contract: {error}"))
        .0
}

fn configured_static_system_libraries() -> Vec<String> {
    if let Ok(value) = env::var(STATIC_SYSTEM_LIBRARIES_ENV) {
        return parse_csv(STATIC_SYSTEM_LIBRARIES_ENV, &value);
    }
    native_link_contract()
        .unwrap_or_else(|error| panic!("static native build has no generated system dependency contract: {error}"))
        .1
}

fn parse_csv(variable: &str, value: &str) -> Vec<String> {
    let values = value
        .split(',')
        .map(str::trim)
        .map(str::to_owned)
        .filter(|value| !value.is_empty())
        .collect::<Vec<_>>();
    assert!(!values.is_empty(), "{variable} must contain at least one library");
    assert!(
        values.iter().all(|value| {
            !value.contains(';')
                && !value.contains('=')
                && value.chars().all(|character| !character.is_whitespace())
        }),
        "{variable} must be a comma-separated list of linker names"
    );
    values
}

// 独立 cargo 构建依赖完整源码树和本机 CMake；交叉编译必须由调用者提供目标平台库目录。
fn build_native(link_kind: &str) {
    assert_eq!(
        env::var_os("HOST"),
        env::var_os("TARGET"),
        "cross compilation requires ZR_VM_RUST_BINDING_LIB_DIR for the target libraries"
    );
    let manifest = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    // Cargo supplies an absolute directory. Canonicalizing it adds Windows's
    // verbatim prefix, whose question mark prevents CMake's source globs matching.
    let source = manifest
        .ancestors()
        .nth(3)
        .expect("binding crate must be nested inside the native source tree");
    assert!(
        source.join("CMakeLists.txt").is_file(),
        "native source build requires the complete zr_vm source tree"
    );
    watch_sources(source);

    let output = PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("zr_vm-native");
    fs::create_dir_all(&output).expect("create native build output directory");
    let profile = if env::var("PROFILE").as_deref() == Ok("debug") {
        "Debug"
    } else {
        "Release"
    };
    for variable in [
        "CMAKE",
        "CMAKE_GENERATOR",
        "CMAKE_GENERATOR_PLATFORM",
        "CMAKE_GENERATOR_TOOLSET",
    ] {
        println!("cargo:rerun-if-env-changed={variable}");
    }
    let cmake = env::var_os("CMAKE").unwrap_or_else(|| "cmake".into());
    let (build_shared, build_static, native_target) = if link_kind == "static" {
        ("OFF", "ON", "zr_vm_rust_binding_static")
    } else {
        ("ON", "OFF", "zr_vm_rust_binding_shared")
    };
    run(
        Command::new(&cmake)
            .current_dir(&output)
            .arg("-S")
            .arg(source)
            .arg("-B")
            .arg(&output)
            .arg(format!("-DCMAKE_BUILD_TYPE={profile}"))
            .arg(format!("-DBUILD_SHARED_LIB={build_shared}"))
            .arg(format!("-DBUILD_STATIC_LIB={build_static}"))
            .args([
                "-DBUILD_RUST_BINDING=ON",
                "-DBUILD_TESTS=OFF",
                "-DBUILD_CLI=OFF",
                "-DBUILD_LANGUAGE_SERVER=OFF",
                "-DBUILD_LANGUAGE_SERVER_EXTENSION=OFF",
            ]),
        "configure native ZrVM libraries",
    );
    // The Cargo build script occupies one managed build slot. Keep its native
    // subprocess serial rather than starting a second independent worker pool.
    run(
        Command::new(&cmake)
            .current_dir(&output)
            .arg("--build")
            .arg(&output)
            .args([
                "--config",
                profile,
                "--target",
                native_target,
                "--parallel",
                "1",
            ]),
        "build native ZrVM libraries",
    );

    // CMake single- and multi-config generators use different output layouts.
    // Paths under OUT_DIR also reach the loader for Cargo test/run processes.
    for directory in ["lib", "bin"] {
        for path in [output.join(directory).join(profile), output.join(directory)] {
            if path.is_dir() {
                link_search(&path);
            }
        }
    }
}

fn validate_output_environment() {
    let target_dir = env::var_os("CARGO_TARGET_DIR")
        .expect("CARGO_TARGET_DIR must be set below D:/cargo-targets, E:/cargo-targets, or F:/cargo-targets");
    validate_approved_path(Path::new(&target_dir), "CARGO_TARGET_DIR");
    for variable in ["CARGO_HOME", "SCCACHE_DIR", "TMP", "TEMP", "TMPDIR"] {
        if let Some(value) = env::var_os(variable) {
            validate_approved_path(Path::new(&value), variable);
        }
    }
    if let Some(out_dir) = env::var_os("OUT_DIR") {
        validate_approved_path(Path::new(&out_dir), "OUT_DIR");
    }
}

fn validate_approved_path(path: &Path, variable: &str) {
    assert!(
        path.is_absolute(),
        "{variable} must be an absolute path below D:/cargo-targets, E:/cargo-targets, or F:/cargo-targets"
    );
    let normalized = normalize_path(path);
    let approved = ["d:/cargo-targets", "e:/cargo-targets", "f:/cargo-targets"];
    assert!(
        approved.iter().any(|root| {
            normalized == *root
                || normalized
                    .strip_prefix(root)
                    .is_some_and(|suffix| suffix.starts_with('/'))
        }),
        "{variable} must remain below a direct approved drive-root cargo-targets directory: {}",
        path.display()
    );
    let canonical = fs::canonicalize(path).unwrap_or_else(|error| {
        panic!("{variable} must exist before the sys build starts: {path:?}: {error}")
    });
    assert_eq!(
        normalize_path(&canonical),
        normalized,
        "{variable} must not use a symlink, junction, reparse point, or path alias"
    );
}

fn normalize_path(path: &Path) -> String {
    let mut value = path.to_string_lossy().replace('\\', "/").to_lowercase();
    if let Some(stripped) = value.strip_prefix("//?/") {
        value = stripped.to_owned();
    }
    while value.ends_with('/') && value.len() > 3 {
        value.pop();
    }
    value
}

fn watch_sources(source: &Path) {
    println!(
        "cargo:rerun-if-changed={}",
        source.join("CMakeLists.txt").display()
    );
    for entry in fs::read_dir(source).expect("read native source modules") {
        let entry = entry.expect("read native source module");
        if entry
            .file_type()
            .expect("read native source module type")
            .is_dir()
            && entry.file_name().to_string_lossy().starts_with("zr_vm_")
        {
            println!("cargo:rerun-if-changed={}", entry.path().display());
        }
    }
}

fn link_search(path: &Path) {
    println!("cargo:rustc-link-search=native={}", path.display());
}

fn run(command: &mut Command, operation: &str) {
    let program: &OsStr = command.get_program();
    eprintln!("{operation}: {command:?}");
    let description = program.to_string_lossy().into_owned();
    let status = command
        .status()
        .unwrap_or_else(|error| panic!("failed to {operation} with {description}: {error}"));
    assert!(status.success(), "failed to {operation}: {status}");
}
