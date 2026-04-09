final class FibRecursiveCase {
    static final String NAME = "fib_recursive";
    static final String PASS_BANNER = "BENCH_FIB_RECURSIVE_PASS";

    private FibRecursiveCase() {}

    static long run(int scale) {
        return BenchmarkSupport.fibRecursive(scale);
    }
}
