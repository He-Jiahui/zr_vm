mod support;

#[path = "../../cases/numeric_loops/rust/mod.rs"]
mod numeric_loops;
#[path = "../../cases/dispatch_loops/rust/mod.rs"]
mod dispatch_loops;
#[path = "../../cases/container_pipeline/rust/mod.rs"]
mod container_pipeline;
#[path = "../../cases/sort_array/rust/mod.rs"]
mod sort_array;
#[path = "../../cases/prime_trial_division/rust/mod.rs"]
mod prime_trial_division;
#[path = "../../cases/matrix_add_2d/rust/mod.rs"]
mod matrix_add_2d;
#[path = "../../cases/string_build/rust/mod.rs"]
mod string_build;
#[path = "../../cases/map_object_access/rust/mod.rs"]
mod map_object_access;

struct CaseDescriptor {
    name: &'static str,
    pass_banner: &'static str,
    run: fn(i64) -> i64,
}

fn parse_scale(args: &[String]) -> Result<i64, String> {
    let mut tier = "core";
    let mut explicit_scale: Option<i64> = None;
    let mut index = 0;

    while index < args.len() {
        match args[index].as_str() {
            "--tier" => {
                if index + 1 >= args.len() {
                    return Err("--tier requires smoke, core, stress, or profile".to_string());
                }
                tier = args[index + 1].as_str();
                index += 2;
            }
            "--scale" => {
                if index + 1 >= args.len() {
                    return Err("--scale requires a positive integer".to_string());
                }
                let parsed = args[index + 1]
                    .parse::<i64>()
                    .map_err(|_| "--scale requires a positive integer".to_string())?;
                if parsed < 1 {
                    return Err("--scale requires a positive integer".to_string());
                }
                explicit_scale = Some(parsed);
                index += 2;
            }
            arg => {
                return Err(format!("unknown argument: {arg}"));
            }
        }
    }

    if let Some(scale) = explicit_scale {
        return Ok(scale);
    }
    support::scale_from_tier(tier).ok_or_else(|| format!("unsupported tier: {tier}"))
}

fn find_case<'a>(case_name: &str, cases: &'a [CaseDescriptor]) -> Option<&'a CaseDescriptor> {
    cases.iter().find(|descriptor| descriptor.name == case_name)
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let mut case_name: Option<String> = None;
    let mut runner_args: Vec<String> = Vec::new();
    let mut index = 0;

    while index < args.len() {
        match args[index].as_str() {
            "--case" => {
                if index + 1 >= args.len() {
                    eprintln!("--case requires a benchmark name");
                    std::process::exit(1);
                }
                case_name = Some(args[index + 1].clone());
                index += 2;
            }
            "--tier" => {
                if index + 1 >= args.len() {
                    eprintln!("--tier requires smoke, core, stress, or profile");
                    std::process::exit(1);
                }
                runner_args.push(args[index].clone());
                runner_args.push(args[index + 1].clone());
                index += 2;
            }
            "--scale" => {
                if index + 1 >= args.len() {
                    eprintln!("--scale requires a positive integer");
                    std::process::exit(1);
                }
                runner_args.push(args[index].clone());
                runner_args.push(args[index + 1].clone());
                index += 2;
            }
            arg => {
                eprintln!("unknown argument: {arg}");
                std::process::exit(1);
            }
        }
    }

    let cases = [
        CaseDescriptor {
            name: "numeric_loops",
            pass_banner: numeric_loops::PASS_BANNER,
            run: numeric_loops::run,
        },
        CaseDescriptor {
            name: "dispatch_loops",
            pass_banner: dispatch_loops::PASS_BANNER,
            run: dispatch_loops::run,
        },
        CaseDescriptor {
            name: "container_pipeline",
            pass_banner: container_pipeline::PASS_BANNER,
            run: container_pipeline::run,
        },
        CaseDescriptor {
            name: "sort_array",
            pass_banner: sort_array::PASS_BANNER,
            run: sort_array::run,
        },
        CaseDescriptor {
            name: "prime_trial_division",
            pass_banner: prime_trial_division::PASS_BANNER,
            run: prime_trial_division::run,
        },
        CaseDescriptor {
            name: "matrix_add_2d",
            pass_banner: matrix_add_2d::PASS_BANNER,
            run: matrix_add_2d::run,
        },
        CaseDescriptor {
            name: "string_build",
            pass_banner: string_build::PASS_BANNER,
            run: string_build::run,
        },
        CaseDescriptor {
            name: "map_object_access",
            pass_banner: map_object_access::PASS_BANNER,
            run: map_object_access::run,
        },
    ];

    let Some(case_name) = case_name else {
        eprintln!("--case is required");
        std::process::exit(1);
    };
    let scale = match parse_scale(&runner_args) {
        Ok(value) => value,
        Err(error) => {
            eprintln!("{error}");
            std::process::exit(1);
        }
    };
    let Some(descriptor) = find_case(&case_name, &cases) else {
        eprintln!("unknown benchmark case: {case_name}");
        std::process::exit(1);
    };
    let checksum = (descriptor.run)(scale);
    println!("{}", descriptor.pass_banner);
    println!("{checksum}");
}
