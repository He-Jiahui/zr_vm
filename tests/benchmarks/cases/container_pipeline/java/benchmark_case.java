final class ContainerPipelineCase {
    static final String NAME = "container_pipeline";
    static final String PASS_BANNER = "BENCH_CONTAINER_PIPELINE_PASS";

    private ContainerPipelineCase() {}

    static long run(int scale) {
        return BenchmarkSupport.containerPipeline(scale);
    }
}
