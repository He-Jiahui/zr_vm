use std::env;
use std::ffi::OsStr;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

// sys crate 只链接 C ABI；有外部库目录时复用它，否则在 Cargo 构建期编译完整 native 目标。
fn main() {
    println!("cargo:rerun-if-env-changed=ZR_VM_RUST_BINDING_LIB_DIR");

    if let Some(lib_dir) = env::var_os("ZR_VM_RUST_BINDING_LIB_DIR") {
        assert!(
            !lib_dir.is_empty() && Path::new(&lib_dir).is_dir(),
            "ZR_VM_RUST_BINDING_LIB_DIR must name an existing import library directory"
        );
        link_search(Path::new(&lib_dir));
    } else {
        build_native();
    }
    println!("cargo:rustc-link-lib=dylib=zr_vm_rust_binding");
}

// 独立 cargo 构建依赖完整源码树和本机 CMake；交叉编译必须由调用者提供目标平台库目录。
fn build_native() {
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
    run(
        Command::new(&cmake)
            .current_dir(&output)
            .arg("-S")
            .arg(source)
            .arg("-B")
            .arg(&output)
            .arg(format!("-DCMAKE_BUILD_TYPE={profile}"))
            .args([
                "-DBUILD_SHARED_LIB=ON",
                "-DBUILD_STATIC_LIB=OFF",
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
                "zr_vm_rust_binding_shared",
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
