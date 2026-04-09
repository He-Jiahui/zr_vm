final class MixedServiceLoopCase {
    static final String NAME = "mixed_service_loop";
    static final String PASS_BANNER = "BENCH_MIXED_SERVICE_LOOP_PASS";

    private MixedServiceLoopCase() {}

    static long run(int scale) {
        return BenchmarkSupport.mixedServiceLoop(scale);
    }
}
