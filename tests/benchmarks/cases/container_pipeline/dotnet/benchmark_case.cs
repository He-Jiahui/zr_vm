namespace BenchmarkRunner;

internal static class ContainerPipelineCase
{
    public const string Name = "container_pipeline";
    public const string PassBanner = "BENCH_CONTAINER_PIPELINE_PASS";

    public static long Run(int scale) => BenchmarkSupport.ContainerPipeline(scale);
}
