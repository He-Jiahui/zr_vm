pub const PASS_BANNER: &str = "BENCH_CONTAINER_PIPELINE_PASS";

pub fn run(scale: i64) -> i64 {
    crate::support::container_pipeline(scale)
}
