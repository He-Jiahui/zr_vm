pub const PASS_BANNER: &str = "BENCH_DISPATCH_LOOPS_PASS";

pub fn run(scale: i64) -> i64 {
    crate::support::dispatch_loops(scale)
}
