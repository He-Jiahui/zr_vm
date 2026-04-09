final class NumericLoopsCase {
    static final String NAME = "numeric_loops";
    static final String PASS_BANNER = "BENCH_NUMERIC_LOOPS_PASS";

    private NumericLoopsCase() {}

    static long run(int scale) {
        return BenchmarkSupport.numericLoops(scale);
    }
}
