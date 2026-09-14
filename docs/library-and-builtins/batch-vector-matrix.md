# Batch and matrix scalar protocol

The batch protocol provides the semantic baseline used by future automatic
vectorization. `SZrBatchBuffer` describes a byte span, element width, and a
signed stride; the logical element zero is always `data`, including for a
negative stride. Shape multiplication and every element offset are checked for
overflow and bounds errors before a callback runs.

`ZrLibrary_Batch_Map` and `ZrLibrary_Batch_Zip` execute callbacks in increasing
logical index order. A callback failure stops immediately and reports the
failing index, so exception/error ordering is preserved. Exact in-place map is
permitted when input and output have identical layout. Partial overlap and any
zip output overlap are rejected with `ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE`; a
caller can then use the scalar fallback or materialize a disjoint buffer.

`ZrLibrary_Batch_MatrixMap2D` computes `rows * columns` with checked
overflow, then delegates to the same ordered zip semantics. Non-unit and
negative strides are valid when the supplied byte span proves every access is
inside the allocation. No vector width, alignment, mask, or fast-math promise
is made by this protocol; those are separate legality/cost decisions for the
ExecIR vector pass.

The focused fixture `tests/library/test_ssa_batch_vectorization.c` covers
zero/one/tail lengths, callback failure ordering, partial overlap rejection,
negative stride, matrix shape overflow, and matrix zip results. The test is
intentionally standalone so it can be registered by `tests/cmake/ssa-tests.cmake`
as `ssa_batch_vectorization` without coupling this contract to parser 09.02.
