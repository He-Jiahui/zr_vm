pub const PASS_BANNER: &str = "BENCH_NUMERIC_LOOPS_PASS";

pub fn run(scale: i64) -> i64 {
    crate::support::numeric_loops(scale)
}
