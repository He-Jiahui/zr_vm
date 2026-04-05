; ZR AOT LLVM Backend
; SemIR overlay + generated exec thunks.
; runtimeContracts:

@zr_aot_module_name = private unnamed_addr constant [11 x i8] c"decorators\00"
@zr_aot_input_hash = private unnamed_addr constant [17 x i8] c"85c131e1f4cf947e\00"
@zr_aot_runtime_contracts = private constant [1 x ptr] [ptr null]
@zr_aot_embedded_module_blob = private constant [3088 x i8] [
  i8 1,   i8 90,   i8 82,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 14,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 8,   i8 8,   i8 1,   i8 1,   i8 0,   i8 0,   i8 0,
  i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 100,   i8 101,   i8 99,   i8 111,   i8 114,   i8 97,   i8 116,   i8 111,   i8 114,   i8 115,   i8 16,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 56,   i8 53,   i8 99,   i8 49,   i8 51,   i8 49,   i8 101,   i8 49,   i8 102,   i8 52,   i8 99,   i8 102,   i8 57,   i8 52,
  i8 55,   i8 101,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 95,   i8 95,   i8 101,   i8 110,   i8 116,   i8 114,
  i8 121,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 80,   i8 0,   i8 1,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 2,
  i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 75,   i8 0,   i8 1,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 15,   i8 0,   i8 0,   i8 0,   i8 1,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 109,   i8 97,   i8 114,   i8 107,   i8 70,   i8 117,   i8 110,   i8 99,   i8 116,   i8 105,   i8 111,   i8 110,   i8 9,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 7,   i8 0,
  i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 81,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 2,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 81,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 2,   i8 0,   i8 5,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 1,   i8 0,
  i8 0,   i8 0,   i8 9,   i8 0,   i8 6,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 5,   i8 0,   i8 2,   i8 0,
  i8 0,   i8 0,   i8 2,   i8 0,   i8 6,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 9,   i8 0,   i8 4,   i8 0,   i8 2,   i8 0,
  i8 1,   i8 0,   i8 2,   i8 0,   i8 3,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 4,   i8 0,   i8 2,   i8 0,
  i8 0,   i8 0,   i8 75,   i8 0,   i8 1,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 101,
  i8 116,   i8 97,   i8 100,   i8 97,   i8 116,   i8 97,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 105,   i8 110,   i8 115,   i8 116,   i8 114,   i8 117,   i8 109,   i8 101,   i8 110,   i8 116,   i8 101,   i8 100,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 116,   i8 97,   i8 114,   i8 103,   i8 101,   i8 116,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 70,   i8 117,   i8 110,   i8 99,   i8 116,   i8 105,   i8 111,   i8 110,   i8 18,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 98,   i8 111,   i8 110,   i8 117,   i8 115,   i8 1,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 116,   i8 97,   i8 114,   i8 103,   i8 101,   i8 116,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 70,   i8 117,   i8 110,   i8 99,   i8 116,   i8 105,   i8 111,
  i8 110,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 98,
  i8 111,   i8 110,   i8 117,   i8 115,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 1,   i8 5,   i8 0,   i8 0,   i8 0,   i8 16,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 105,   i8 110,   i8 115,   i8 116,   i8 114,   i8 117,   i8 109,
  i8 101,   i8 110,   i8 116,   i8 101,   i8 100,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 101,   i8 116,   i8 97,   i8 100,   i8 97,   i8 116,
  i8 97,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 71,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 116,   i8 101,   i8 115,
  i8 116,   i8 115,   i8 47,   i8 102,   i8 105,   i8 120,   i8 116,   i8 117,   i8 114,   i8 101,   i8 115,   i8 47,   i8 112,   i8 114,   i8 111,   i8 106,
  i8 101,   i8 99,   i8 116,   i8 115,   i8 47,   i8 100,   i8 101,   i8 99,   i8 111,   i8 114,   i8 97,   i8 116,   i8 111,   i8 114,   i8 95,   i8 99,
  i8 111,   i8 109,   i8 112,   i8 105,   i8 108,   i8 101,   i8 95,   i8 116,   i8 105,   i8 109,   i8 101,   i8 95,   i8 105,   i8 109,   i8 112,   i8 111,
  i8 114,   i8 116,   i8 47,   i8 115,   i8 114,   i8 99,   i8 47,   i8 100,   i8 101,   i8 99,   i8 111,   i8 114,   i8 97,   i8 116,   i8 111,   i8 114,
  i8 115,   i8 46,   i8 122,   i8 114,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 97,   i8 114,   i8 107,   i8 70,   i8 117,   i8 110,   i8 99,
  i8 116,   i8 105,   i8 111,   i8 110,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 97,   i8 114,   i8 107,   i8 70,   i8 117,   i8 110,
  i8 99,   i8 116,   i8 105,   i8 111,   i8 110,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 97,   i8 114,   i8 107,   i8 70,   i8 117,   i8 110,   i8 99,   i8 116,
  i8 105,   i8 111,   i8 110,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 1,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,
  i8 97,   i8 114,   i8 107,   i8 70,   i8 117,   i8 110,   i8 99,   i8 116,   i8 105,   i8 111,   i8 110,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 14,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 68,   i8 101,   i8 99,
  i8 111,   i8 114,   i8 97,   i8 116,   i8 111,   i8 114,   i8 80,   i8 97,   i8 116,   i8 99,   i8 104,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 116,   i8 97,   i8 114,   i8 103,   i8 101,   i8 116,   i8 18,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 70,
  i8 117,   i8 110,   i8 99,   i8 116,   i8 105,   i8 111,   i8 110,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 98,   i8 111,   i8 110,   i8 117,   i8 115,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 5,   i8 0,   i8 0,   i8 0,   i8 16,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 9,   i8 0,
  i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 97,   i8 114,   i8 107,   i8 70,   i8 117,   i8 110,   i8 99,   i8 116,
  i8 105,   i8 111,   i8 110,   i8 9,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 81,
  i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 81,
  i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 5,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 6,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 9,   i8 0,   i8 6,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 5,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 6,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 9,
  i8 0,   i8 4,   i8 0,   i8 2,   i8 0,   i8 1,   i8 0,   i8 2,   i8 0,   i8 3,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 4,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 75,   i8 0,   i8 1,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 109,   i8 101,   i8 116,   i8 97,   i8 100,   i8 97,   i8 116,   i8 97,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 105,   i8 110,   i8 115,   i8 116,   i8 114,   i8 117,   i8 109,   i8 101,   i8 110,
  i8 116,   i8 101,   i8 100,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 116,
  i8 97,   i8 114,   i8 103,   i8 101,   i8 116,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 70,   i8 117,   i8 110,   i8 99,   i8 116,
  i8 105,   i8 111,   i8 110,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 98,   i8 111,   i8 110,   i8 117,   i8 115,   i8 1,   i8 0,   i8 0,   i8 0,
  i8 5,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 116,   i8 97,   i8 114,   i8 103,   i8 101,   i8 116,   i8 18,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 70,   i8 117,
  i8 110,   i8 99,   i8 116,   i8 105,   i8 111,   i8 110,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 98,   i8 111,   i8 110,   i8 117,   i8 115,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 5,   i8 0,   i8 0,   i8 0,   i8 16,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 105,   i8 110,
  i8 115,   i8 116,   i8 114,   i8 117,   i8 109,   i8 101,   i8 110,   i8 116,   i8 101,   i8 100,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 101,
  i8 116,   i8 97,   i8 100,   i8 97,   i8 116,   i8 97,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 71,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 116,   i8 101,   i8 115,   i8 116,   i8 115,   i8 47,   i8 102,   i8 105,   i8 120,   i8 116,   i8 117,   i8 114,   i8 101,   i8 115,
  i8 47,   i8 112,   i8 114,   i8 111,   i8 106,   i8 101,   i8 99,   i8 116,   i8 115,   i8 47,   i8 100,   i8 101,   i8 99,   i8 111,   i8 114,   i8 97,
  i8 116,   i8 111,   i8 114,   i8 95,   i8 99,   i8 111,   i8 109,   i8 112,   i8 105,   i8 108,   i8 101,   i8 95,   i8 116,   i8 105,   i8 109,   i8 101,
  i8 95,   i8 105,   i8 109,   i8 112,   i8 111,   i8 114,   i8 116,   i8 47,   i8 115,   i8 114,   i8 99,   i8 47,   i8 100,   i8 101,   i8 99,   i8 111,
  i8 114,   i8 97,   i8 116,   i8 111,   i8 114,   i8 115,   i8 46,   i8 122,   i8 114,   i8 16,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 56,   i8 53,   i8 99,   i8 49,   i8 51,   i8 49,   i8 101,   i8 49,   i8 102,   i8 52,   i8 99,   i8 102,   i8 57,   i8 52,   i8 55,
  i8 101,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 71,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 116,   i8 101,   i8 115,   i8 116,   i8 115,   i8 47,   i8 102,
  i8 105,   i8 120,   i8 116,   i8 117,   i8 114,   i8 101,   i8 115,   i8 47,   i8 112,   i8 114,   i8 111,   i8 106,   i8 101,   i8 99,   i8 116,   i8 115,
  i8 47,   i8 100,   i8 101,   i8 99,   i8 111,   i8 114,   i8 97,   i8 116,   i8 111,   i8 114,   i8 95,   i8 99,   i8 111,   i8 109,   i8 112,   i8 105,
  i8 108,   i8 101,   i8 95,   i8 116,   i8 105,   i8 109,   i8 101,   i8 95,   i8 105,   i8 109,   i8 112,   i8 111,   i8 114,   i8 116,   i8 47,   i8 115,
  i8 114,   i8 99,   i8 47,   i8 100,   i8 101,   i8 99,   i8 111,   i8 114,   i8 97,   i8 116,   i8 111,   i8 114,   i8 115,   i8 46,   i8 122,   i8 114,
  i8 16,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 56,   i8 53,   i8 99,   i8 49,   i8 51,   i8 49,   i8 101,   i8 49,
  i8 102,   i8 52,   i8 99,   i8 102,   i8 57,   i8 52,   i8 55,   i8 101,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0
]
%ZrAotGeneratedFrame = type { ptr, ptr, ptr, ptr, i32, i32, i32, i32, i32, i1 }
%ZrAotGeneratedDirectCall = type { ptr, ptr, ptr, i32, i32, i32, i32, i32, i1, i1 }
%ZrAotCompiledModule = type { i32, i32, ptr, i32, ptr, ptr, ptr, i64, ptr, i32, ptr }

define internal i64 @zr_aot_fn_0(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 0, ptr %frame)
  br i1 %t0, label %zr_aot_fn_0_ins_0, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t1, label %zr_aot_fn_0_ins_0_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CreateClosure(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t2, label %zr_aot_fn_0_ins_1, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 0)
  br i1 %t3, label %zr_aot_fn_0_ins_1_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 0, i32 1)
  br i1 %t4, label %zr_aot_fn_0_ins_2, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t5, label %zr_aot_fn_0_ins_2_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 1)
  br i1 %t6, label %zr_aot_fn_0_ins_3, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 8)
  br i1 %t7, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t8 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 true)
  ret i64 %t8

zr_aot_fn_0_end_unsupported:
  %t9 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 4, i32 0)
  ret i64 %t9

zr_aot_fn_0_fail:
  %t10 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t10
}

define internal i64 @zr_aot_fn_1(ptr %state) {
entry:
  %frame = alloca %ZrAotGeneratedFrame, align 8
  %direct_call = alloca %ZrAotGeneratedDirectCall, align 8
  %resume_instruction = alloca i32, align 4
  %truthy_value = alloca i8, align 1
  %t0 = call i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr %state, i32 1, ptr %frame)
  br i1 %t0, label %zr_aot_fn_1_ins_0, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_0:
  %t1 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t1, label %zr_aot_fn_1_ins_0_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_0_body:
  %t2 = call i1 @ZrLibrary_AotRuntime_CreateObject(ptr %state, ptr %frame, i32 2)
  br i1 %t2, label %zr_aot_fn_1_ins_1, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t3, label %zr_aot_fn_1_ins_1_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 0)
  br i1 %t4, label %zr_aot_fn_1_ins_2, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 0)
  br i1 %t5, label %zr_aot_fn_1_ins_2_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_CreateObject(ptr %state, ptr %frame, i32 4)
  br i1 %t6, label %zr_aot_fn_1_ins_3, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_3:
  %t7 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t7, label %zr_aot_fn_1_ins_3_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_3_body:
  %t8 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t8, label %zr_aot_fn_1_ins_4, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_4:
  %t9 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 0)
  br i1 %t9, label %zr_aot_fn_1_ins_4_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_4_body:
  %t10 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t10, label %zr_aot_fn_1_ins_5, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_5:
  %t11 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t11, label %zr_aot_fn_1_ins_5_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_5_body:
  %t12 = call i1 @ZrLibrary_AotRuntime_SetMember(ptr %state, ptr %frame, i32 6, i32 4, i32 0)
  br i1 %t12, label %zr_aot_fn_1_ins_6, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_6:
  %t13 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)
  br i1 %t13, label %zr_aot_fn_1_ins_6_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_6_body:
  %t14 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 5, i32 2)
  br i1 %t14, label %zr_aot_fn_1_ins_7, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_7:
  %t15 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t15, label %zr_aot_fn_1_ins_7_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_7_body:
  %t16 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 6, i32 2)
  br i1 %t16, label %zr_aot_fn_1_ins_8, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_8:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t17, label %zr_aot_fn_1_ins_8_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_8_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_SetMember(ptr %state, ptr %frame, i32 4, i32 2, i32 1)
  br i1 %t18, label %zr_aot_fn_1_ins_9, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_9:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t19, label %zr_aot_fn_1_ins_9_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_9_body:
  %t20 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 2)
  br i1 %t20, label %zr_aot_fn_1_ins_10, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_10:
  %t21 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 1)
  br i1 %t21, label %zr_aot_fn_1_ins_10_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_10_body:
  %t22 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 2)
  br i1 %t22, label %zr_aot_fn_1_ins_11, label %zr_aot_fn_1_fail

zr_aot_fn_1_ins_11:
  %t23 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 8)
  br i1 %t23, label %zr_aot_fn_1_ins_11_body, label %zr_aot_fn_1_fail
zr_aot_fn_1_ins_11_body:
  %t24 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 2, i1 false)
  ret i64 %t24

zr_aot_fn_1_end_unsupported:
  %t25 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 12, i32 0)
  ret i64 %t25

zr_aot_fn_1_fail:
  %t26 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t26
}

@zr_aot_function_thunks = private constant [2 x ptr] [ptr @zr_aot_fn_0, ptr @zr_aot_fn_1]

define i64 @zr_aot_entry(ptr %state) {
entry:
  %ret = call i64 @zr_aot_fn_0(ptr %state)
  ret i64 %ret
}

@zr_aot_module = private constant %ZrAotCompiledModule {
  i32 2,
  i32 2,
  ptr @zr_aot_module_name,
  i32 1,
  ptr @zr_aot_input_hash,
  ptr @zr_aot_runtime_contracts,
  ptr @zr_aot_embedded_module_blob,
  i64 3088,
  ptr @zr_aot_function_thunks,
  i32 2,
  ptr @zr_aot_entry
}

; export-symbol: ZrVm_GetAotCompiledModule
; descriptor.moduleName = decorators
; descriptor.inputKind = 1
; descriptor.inputHash = 85c131e1f4cf947e
; descriptor.backendKind = llvm
declare i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CopyConstant(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CreateClosure(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetClosureValue(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_CopyStack(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetGlobal(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_CreateObject(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_CreateArray(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_TypeOf(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToObject(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_ToStruct(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGet(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSet(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGetCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSetCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaGetStaticCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MetaSetStaticCached(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnUsing(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnShare(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnWeak(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnUpgrade(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_OwnRelease(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalEqual(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalNotEqual(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_LogicalLessSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IsTruthy(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_Add(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_AddInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SubInt(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_MulSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_DivSigned(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_Neg(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetMember(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetMember(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_GetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_SetByIndex(ptr, ptr, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IterInit(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IterMoveNext(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_IterCurrent(ptr, ptr, i32, i32)
declare i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr, ptr, i32, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_PrepareMetaCall(ptr, ptr, i32, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall(ptr, ptr, i32, i32, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr, ptr, ptr, i32, i32, i32, i32)
declare i1 @ZrLibrary_AotRuntime_FinishDirectCall(ptr, ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_Try(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_EndTry(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_Throw(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_Catch(ptr, ptr, i32)
declare i1 @ZrLibrary_AotRuntime_EndFinally(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_SetPendingReturn(ptr, ptr, i32, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_SetPendingBreak(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_SetPendingContinue(ptr, ptr, i32, ptr)
declare i1 @ZrLibrary_AotRuntime_ToInt(ptr, ptr, i32, i32)
declare i64 @ZrLibrary_AotRuntime_Return(ptr, ptr, i32, i1)
declare i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr, ptr)
declare i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr, i32, i32, i32)
define ptr @ZrVm_GetAotCompiledModule() {
entry_export:
  ret ptr @zr_aot_module
}
