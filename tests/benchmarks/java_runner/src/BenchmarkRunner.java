import java.util.ArrayList;

public final class BenchmarkRunner {
    private BenchmarkRunner() {}

    public static void main(String[] args) {
        String caseName = null;
        ArrayList<String> runnerArgs = new ArrayList<>();

        for (int index = 0; index < args.length; index++) {
            String argument = args[index];
            switch (argument) {
                case "--case":
                    if (index + 1 >= args.length) {
                        fail("--case requires a benchmark name");
                    }
                    caseName = args[++index];
                    break;
                case "--tier":
                case "--scale":
                    if (index + 1 >= args.length) {
                        fail(argument + " requires a value");
                    }
                    runnerArgs.add(argument);
                    runnerArgs.add(args[++index]);
                    break;
                default:
                    fail("unknown argument: " + argument);
            }
        }

        if (caseName == null || caseName.isEmpty()) {
            fail("--case is required");
        }

        final int scale;
        try {
            scale = BenchmarkSupport.parseScale(runnerArgs.toArray(new String[0]));
        } catch (IllegalArgumentException ex) {
            fail(ex.getMessage());
            return;
        }

        final String passBanner;
        final long checksum;
        switch (caseName) {
            case NumericLoopsCase.NAME:
                passBanner = NumericLoopsCase.PASS_BANNER;
                checksum = NumericLoopsCase.run(scale);
                break;
            case DispatchLoopsCase.NAME:
                passBanner = DispatchLoopsCase.PASS_BANNER;
                checksum = DispatchLoopsCase.run(scale);
                break;
            case ContainerPipelineCase.NAME:
                passBanner = ContainerPipelineCase.PASS_BANNER;
                checksum = ContainerPipelineCase.run(scale);
                break;
            case SortArrayCase.NAME:
                passBanner = SortArrayCase.PASS_BANNER;
                checksum = SortArrayCase.run(scale);
                break;
            case PrimeTrialDivisionCase.NAME:
                passBanner = PrimeTrialDivisionCase.PASS_BANNER;
                checksum = PrimeTrialDivisionCase.run(scale);
                break;
            case MatrixAdd2dCase.NAME:
                passBanner = MatrixAdd2dCase.PASS_BANNER;
                checksum = MatrixAdd2dCase.run(scale);
                break;
            case StringBuildCase.NAME:
                passBanner = StringBuildCase.PASS_BANNER;
                checksum = StringBuildCase.run(scale);
                break;
            case MapObjectAccessCase.NAME:
                passBanner = MapObjectAccessCase.PASS_BANNER;
                checksum = MapObjectAccessCase.run(scale);
                break;
            case FibRecursiveCase.NAME:
                passBanner = FibRecursiveCase.PASS_BANNER;
                checksum = FibRecursiveCase.run(scale);
                break;
            case CallChainPolymorphicCase.NAME:
                passBanner = CallChainPolymorphicCase.PASS_BANNER;
                checksum = CallChainPolymorphicCase.run(scale);
                break;
            case ObjectFieldHotCase.NAME:
                passBanner = ObjectFieldHotCase.PASS_BANNER;
                checksum = ObjectFieldHotCase.run(scale);
                break;
            case ArrayIndexDenseCase.NAME:
                passBanner = ArrayIndexDenseCase.PASS_BANNER;
                checksum = ArrayIndexDenseCase.run(scale);
                break;
            case BranchJumpDenseCase.NAME:
                passBanner = BranchJumpDenseCase.PASS_BANNER;
                checksum = BranchJumpDenseCase.run(scale);
                break;
            case MixedServiceLoopCase.NAME:
                passBanner = MixedServiceLoopCase.PASS_BANNER;
                checksum = MixedServiceLoopCase.run(scale);
                break;
            default:
                fail("unknown benchmark case: " + caseName);
                return;
        }

        System.out.println(passBanner);
        System.out.println(checksum);
    }

    private static void fail(String message) {
        System.err.println(message);
        System.exit(1);
    }
}
