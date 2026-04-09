final class BranchJumpDenseCase {
    static final String NAME = "branch_jump_dense";
    static final String PASS_BANNER = "BENCH_BRANCH_JUMP_DENSE_PASS";

    private BranchJumpDenseCase() {}

    static long run(int scale) {
        return BenchmarkSupport.branchJumpDense(scale);
    }
}
