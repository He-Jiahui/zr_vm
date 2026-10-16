# Native integer power boundary regression

`tests/core/test_native_math_failures.c` exercises the public inline integer-power helpers without VM setup. The standalone CMake project under `tests/cmake/native-math-direct-validation` builds one executable and registers each group as a separate CTest case, so a divide-by-zero in the zero-base cases is distinguishable from assertion failures in the other groups.

The cases cover zero and unit exponents, exact `uint64_t` results, overflow, signed parity, and separate exact/overflow `INT64_MIN` boundaries. The helpers use zero as their overflow sentinel, which is indistinguishable from a valid zero result; tests therefore assert that behavior only where the mathematical result is nonzero or where the existing contract explicitly uses zero.

The regression was reproduced before changing `math.h`: MSVC 19.44 reported a numerical exception for `uint_zero` and failed the representable `2^32` and `2^63` cases plus `INT64_MIN^1`. After the fix, the independent MSVC 19.44 and clang-cl 19.1.5 Debug builds each passed all seven CTest cases. Both build logs show the test source compiling with `math.h` declared as an explicit object dependency.
