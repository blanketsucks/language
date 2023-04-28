use std::{env, process::Command};

macro_rules! link {
    ($name:expr) => { println!("cargo:rustc-link-arg={}", $name); };
}

fn run(cmd: &str, args: &[&str]) -> String {
    let output = Command::new(cmd)
        .args(args)
        .output()
        .unwrap_or_else(|_| panic!("Failed to execute {}", cmd))
        .stdout;

    String::from_utf8(output).unwrap()
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let config = env::var("LLVM_CONFIG");
    let mut version = String::new();

    let mut llvm_config = String::new();
    if let Ok(cfg) = config {
        llvm_config = cfg;
        version = run(&llvm_config, &["--version"]);
    } else {
        for cfg in ["llvm-config", "llvm-config-14"] {
            let output = Command::new(cfg).arg("--version").output();
            if let Ok(output) = output {
                llvm_config = cfg.to_string();
                version = String::from_utf8(output.stdout).unwrap();
            }
        }

        if version.is_empty() {
            panic!("LLVM version 14 is required, but llvm-config was not found");
        }
    }

    if !version.trim().starts_with("14") {
        panic!("LLVM version 14 is required, found {}", version);
    }

    let mut build = cc::Build::new();
    build.warnings(false);


    let output = run(&llvm_config, &["--cxxflags"]);
    for flag in output.split_whitespace() {
        build.flag(flag);
    }

    let output = run(&llvm_config, &["--libs", "--system-libs", "--ldflags"]);
    for flag in output.split('\n') {
        if flag.starts_with("-l") || flag.starts_with("-L") {
            build.flag(flag); link!(flag);
        }
    }

    build.cpp(true).file("src/llvm/wrapper.cpp").compile("llvm-wrapper");
    link!("-lllvm-wrapper");
}