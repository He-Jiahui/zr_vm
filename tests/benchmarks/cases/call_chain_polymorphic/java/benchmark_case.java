final class CallChainPolymorphicCase {
    static final String NAME = "call_chain_polymorphic";
    static final String PASS_BANNER = "BENCH_CALL_CHAIN_POLYMORPHIC_PASS";

    private CallChainPolymorphicCase() {}

    static long run(int scale) {
        return BenchmarkSupport.callChainPolymorphic(scale);
    }
}
