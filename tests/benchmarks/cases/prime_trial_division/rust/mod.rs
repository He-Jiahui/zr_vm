pub const PASS_BANNER: &str = "BENCH_PRIME_TRIAL_DIVISION_PASS";

pub fn run(scale: i64) -> i64 {
    crate::support::prime_trial_division(scale)
}
