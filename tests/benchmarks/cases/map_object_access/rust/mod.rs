pub const PASS_BANNER: &str = "BENCH_MAP_OBJECT_ACCESS_PASS";

pub fn run(scale: i64) -> i64 {
    crate::support::map_object_access(scale)
}
