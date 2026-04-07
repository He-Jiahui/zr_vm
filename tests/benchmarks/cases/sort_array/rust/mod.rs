pub const PASS_BANNER: &str = "BENCH_SORT_ARRAY_PASS";

pub fn run(scale: i64) -> i64 {
    crate::support::sort_array(scale)
}
