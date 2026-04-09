final class DispatchLoopsCase {
    static final String NAME = "dispatch_loops";
    static final String PASS_BANNER = "BENCH_DISPATCH_LOOPS_PASS";

    private DispatchLoopsCase() {}

    static long run(int scale) {
        return BenchmarkSupport.dispatchLoops(scale);
    }
}
