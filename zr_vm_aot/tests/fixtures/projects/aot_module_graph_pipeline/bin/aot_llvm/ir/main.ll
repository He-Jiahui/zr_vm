; ZR AOT LLVM Backend
; SemIR overlay + generated exec thunks.
; runtimeContracts:

@zr_aot_module_name = private unnamed_addr constant [5 x i8] c"main\00"
@zr_aot_input_hash = private unnamed_addr constant [17 x i8] c"519d8fc4249f42aa\00"
@zr_aot_runtime_contracts = private constant [1 x ptr] [ptr null]
@zr_aot_embedded_module_blob = private constant [2835 x i8] [
  i8 1,   i8 90,   i8 82,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 14,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 8,   i8 8,   i8 1,   i8 1,   i8 0,   i8 0,   i8 0,
  i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 109,   i8 97,   i8 105,   i8 110,   i8 16,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 53,   i8 49,   i8 57,   i8 100,
  i8 56,   i8 102,   i8 99,   i8 52,   i8 50,   i8 52,   i8 57,   i8 102,   i8 52,   i8 50,   i8 97,   i8 97,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 95,   i8 95,   i8 101,   i8 110,   i8 116,   i8 114,   i8 121,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 13,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 1,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 73,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 2,
  i8 0,   i8 1,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 1,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 2,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 73,   i8 0,   i8 1,   i8 0,   i8 1,   i8 0,   i8 1,   i8 0,   i8 2,
  i8 0,   i8 2,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 2,   i8 0,   i8 5,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 3,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 73,   i8 0,   i8 2,   i8 0,   i8 2,   i8 0,   i8 1,   i8 0,   i8 2,
  i8 0,   i8 3,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 3,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 4,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 73,   i8 0,   i8 3,   i8 0,   i8 3,   i8 0,   i8 1,   i8 0,   i8 2,
  i8 0,   i8 4,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 8,
  i8 0,   i8 4,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 104,   i8 0,   i8 4,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 16,
  i8 0,   i8 5,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 8,
  i8 0,   i8 6,   i8 0,   i8 6,   i8 0,   i8 1,   i8 0,   i8 104,   i8 0,   i8 6,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 16,
  i8 0,   i8 7,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 8,
  i8 0,   i8 8,   i8 0,   i8 8,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 9,   i8 0,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 73,   i8 0,   i8 8,   i8 0,   i8 8,   i8 0,   i8 2,   i8 0,   i8 2,
  i8 0,   i8 9,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 10,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 9,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 9,   i8 0,   i8 9,   i8 0,   i8 3,   i8 0,   i8 8,
  i8 0,   i8 9,   i8 0,   i8 9,   i8 0,   i8 4,   i8 0,   i8 2,   i8 0,   i8 10,   i8 0,   i8 9,   i8 0,   i8 0,   i8 0,   i8 73,
  i8 0,   i8 9,   i8 0,   i8 9,   i8 0,   i8 1,   i8 0,   i8 2,   i8 0,   i8 10,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,
  i8 0,   i8 9,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 9,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,
  i8 0,   i8 9,   i8 0,   i8 9,   i8 0,   i8 3,   i8 0,   i8 8,   i8 0,   i8 9,   i8 0,   i8 9,   i8 0,   i8 4,   i8 0,   i8 0,
  i8 0,   i8 10,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 73,   i8 0,   i8 9,   i8 0,   i8 9,   i8 0,   i8 1,   i8 0,   i8 2,
  i8 0,   i8 10,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 9,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 9,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 75,   i8 0,   i8 1,   i8 0,   i8 9,   i8 0,   i8 0,   i8 0,   i8 7,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 16,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 20,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 24,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 31,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 47,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 14,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 17,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,
  i8 9,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 122,   i8 114,   i8 46,   i8 115,   i8 121,   i8 115,   i8 116,   i8 101,
  i8 109,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 17,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 13,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 103,   i8 114,   i8 97,   i8 112,   i8 104,   i8 95,   i8 115,   i8 116,   i8 97,   i8 103,
  i8 101,   i8 95,   i8 97,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 17,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 13,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 103,   i8 114,   i8 97,   i8 112,   i8 104,   i8 95,   i8 115,   i8 116,   i8 97,   i8 103,   i8 101,   i8 95,
  i8 98,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 17,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 103,   i8 114,   i8 97,   i8 112,   i8 104,   i8 95,   i8 98,   i8 105,   i8 110,   i8 97,   i8 114,   i8 121,   i8 95,   i8 115,
  i8 116,   i8 97,   i8 103,   i8 101,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 30,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 65,   i8 79,   i8 84,   i8 95,   i8 77,   i8 79,   i8 68,   i8 85,   i8 76,   i8 69,   i8 95,   i8 71,   i8 82,   i8 65,   i8 80,   i8 72,
  i8 95,   i8 80,   i8 73,   i8 80,   i8 69,   i8 76,   i8 73,   i8 78,   i8 69,   i8 95,   i8 80,   i8 65,   i8 83,   i8 83,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,
  i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 115,   i8 101,   i8 101,   i8 100,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,
  i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 105,   i8 110,   i8 116,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,
  i8 0,   i8 5,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 101,   i8 114,   i8 103,   i8 101,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,
  i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 98,   i8 105,   i8 110,   i8 97,   i8 114,   i8 121,
  i8 83,   i8 101,   i8 101,   i8 100,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 115,   i8 121,   i8 115,   i8 116,
  i8 101,   i8 109,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 9,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 122,   i8 114,   i8 46,   i8 115,   i8 121,   i8 115,   i8 116,   i8 101,
  i8 109,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 115,   i8 116,   i8 97,   i8 103,   i8 101,   i8 65,   i8 1,   i8 0,   i8 0,   i8 0,   i8 18,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 13,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 103,   i8 114,   i8 97,   i8 112,   i8 104,   i8 95,   i8 115,   i8 116,   i8 97,   i8 103,   i8 101,   i8 95,   i8 97,   i8 18,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 115,   i8 116,   i8 97,   i8 103,   i8 101,   i8 66,   i8 2,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 13,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 103,   i8 114,
  i8 97,   i8 112,   i8 104,   i8 95,   i8 115,   i8 116,   i8 97,   i8 103,   i8 101,   i8 95,   i8 98,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 98,
  i8 105,   i8 110,   i8 97,   i8 114,   i8 121,   i8 83,   i8 116,   i8 97,   i8 103,   i8 101,   i8 3,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 103,   i8 114,   i8 97,   i8 112,   i8 104,   i8 95,   i8 98,   i8 105,   i8 110,   i8 97,   i8 114,   i8 121,   i8 95,   i8 115,   i8 116,   i8 97,
  i8 103,   i8 101,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 108,   i8 101,   i8 102,   i8 116,   i8 5,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 114,   i8 105,   i8 103,   i8 104,   i8 116,   i8 7,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 109,   i8 101,   i8 114,   i8 103,   i8 101,   i8 100,   i8 8,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 105,
  i8 110,   i8 116,   i8 18,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 115,
  i8 101,   i8 101,   i8 100,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 98,   i8 105,   i8 110,   i8 97,   i8 114,   i8 121,   i8 83,   i8 101,   i8 101,
  i8 100,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 5,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 109,   i8 101,   i8 114,   i8 103,   i8 101,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 99,   i8 111,
  i8 110,   i8 115,   i8 111,   i8 108,   i8 101,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 9,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 112,   i8 114,   i8 105,   i8 110,   i8 116,   i8 76,   i8 105,
  i8 110,   i8 101,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 144,   i8 1,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,
  i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 18,   i8 0,   i8 0,   i8 0,   i8 13,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 61,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 116,   i8 101,
  i8 115,   i8 116,   i8 115,   i8 47,   i8 102,   i8 105,   i8 120,   i8 116,   i8 117,   i8 114,   i8 101,   i8 115,   i8 47,   i8 112,   i8 114,   i8 111,
  i8 106,   i8 101,   i8 99,   i8 116,   i8 115,   i8 47,   i8 97,   i8 111,   i8 116,   i8 95,   i8 109,   i8 111,   i8 100,   i8 117,   i8 108,   i8 101,
  i8 95,   i8 103,   i8 114,   i8 97,   i8 112,   i8 104,   i8 95,   i8 112,   i8 105,   i8 112,   i8 101,   i8 108,   i8 105,   i8 110,   i8 101,   i8 47,
  i8 115,   i8 114,   i8 99,   i8 47,   i8 109,   i8 97,   i8 105,   i8 110,   i8 46,   i8 122,   i8 114,   i8 16,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 53,   i8 49,   i8 57,   i8 100,   i8 56,   i8 102,   i8 99,   i8 52,   i8 50,   i8 52,   i8 57,   i8 102,   i8 52,
  i8 50,   i8 97,   i8 97,   i8 47,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 1,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 2,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 3,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 4,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 6,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 7,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 8,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 10,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 11,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 0,   i8 12,   i8 0,   i8 0,   i8 0,   i8 0,
  i8 0,   i8 0,   i8 0
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
  %t2 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 0, i32 0)
  br i1 %t2, label %zr_aot_fn_0_ins_1, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_1:
  %t3 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t3, label %zr_aot_fn_0_ins_1_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_1_body:
  %t4 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 1, i32 1)
  br i1 %t4, label %zr_aot_fn_0_ins_2, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_2:
  %t5 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 2, i32 5)
  br i1 %t5, label %zr_aot_fn_0_ins_2_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_body:
  %t6 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 0, i32 0, i32 1, ptr %direct_call)
  br i1 %t6, label %zr_aot_fn_0_ins_2_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_prepare_ok:
  %t7 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 0, i32 0, i32 1, i32 1)
  br i1 %t7, label %zr_aot_fn_0_ins_2_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_2_finish_ok:
  br label %zr_aot_fn_0_ins_3

zr_aot_fn_0_ins_3:
  %t8 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 3, i32 1)
  br i1 %t8, label %zr_aot_fn_0_ins_3_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_3_body:
  %t9 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 1, i32 2)
  br i1 %t9, label %zr_aot_fn_0_ins_4, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_4:
  %t10 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t10, label %zr_aot_fn_0_ins_4_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_4_body:
  %t11 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 1, i32 3)
  br i1 %t11, label %zr_aot_fn_0_ins_5, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_5:
  %t12 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 5, i32 1)
  br i1 %t12, label %zr_aot_fn_0_ins_5_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_5_body:
  %t13 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 4)
  br i1 %t13, label %zr_aot_fn_0_ins_6, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_6:
  %t14 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 5)
  br i1 %t14, label %zr_aot_fn_0_ins_6_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_6_body:
  %t15 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 1, i32 1, i32 1, ptr %direct_call)
  br i1 %t15, label %zr_aot_fn_0_ins_6_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_6_prepare_ok:
  %t16 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 1, i32 1, i32 1, i32 1)
  br i1 %t16, label %zr_aot_fn_0_ins_6_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_6_finish_ok:
  br label %zr_aot_fn_0_ins_7

zr_aot_fn_0_ins_7:
  %t17 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)
  br i1 %t17, label %zr_aot_fn_0_ins_7_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_7_body:
  %t18 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 2)
  br i1 %t18, label %zr_aot_fn_0_ins_8, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_8:
  %t19 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 8, i32 1)
  br i1 %t19, label %zr_aot_fn_0_ins_8_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_8_body:
  %t20 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 2, i32 5)
  br i1 %t20, label %zr_aot_fn_0_ins_9, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_9:
  %t21 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 9, i32 1)
  br i1 %t21, label %zr_aot_fn_0_ins_9_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_9_body:
  %t22 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 6)
  br i1 %t22, label %zr_aot_fn_0_ins_10, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_10:
  %t23 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 10, i32 5)
  br i1 %t23, label %zr_aot_fn_0_ins_10_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_10_body:
  %t24 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 2, i32 2, i32 1, ptr %direct_call)
  br i1 %t24, label %zr_aot_fn_0_ins_10_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_10_prepare_ok:
  %t25 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 2, i32 2, i32 1, i32 1)
  br i1 %t25, label %zr_aot_fn_0_ins_10_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_10_finish_ok:
  br label %zr_aot_fn_0_ins_11

zr_aot_fn_0_ins_11:
  %t26 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 11, i32 1)
  br i1 %t26, label %zr_aot_fn_0_ins_11_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_11_body:
  %t27 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 2)
  br i1 %t27, label %zr_aot_fn_0_ins_12, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_12:
  %t28 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 12, i32 1)
  br i1 %t28, label %zr_aot_fn_0_ins_12_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_12_body:
  %t29 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 3, i32 7)
  br i1 %t29, label %zr_aot_fn_0_ins_13, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_13:
  %t30 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 13, i32 1)
  br i1 %t30, label %zr_aot_fn_0_ins_13_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_13_body:
  %t31 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 8)
  br i1 %t31, label %zr_aot_fn_0_ins_14, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_14:
  %t32 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 14, i32 5)
  br i1 %t32, label %zr_aot_fn_0_ins_14_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_14_body:
  %t33 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 3, i32 3, i32 1, ptr %direct_call)
  br i1 %t33, label %zr_aot_fn_0_ins_14_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_14_prepare_ok:
  %t34 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 3, i32 3, i32 1, i32 1)
  br i1 %t34, label %zr_aot_fn_0_ins_14_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_14_finish_ok:
  br label %zr_aot_fn_0_ins_15

zr_aot_fn_0_ins_15:
  %t35 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 15, i32 1)
  br i1 %t35, label %zr_aot_fn_0_ins_15_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_15_body:
  %t36 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 4, i32 2)
  br i1 %t36, label %zr_aot_fn_0_ins_16, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_16:
  %t37 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 16, i32 0)
  br i1 %t37, label %zr_aot_fn_0_ins_16_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_16_body:
  %t38 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 4, i32 1)
  br i1 %t38, label %zr_aot_fn_0_ins_17, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_17:
  %t39 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 17, i32 1)
  br i1 %t39, label %zr_aot_fn_0_ins_17_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_17_body:
  %t40 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 4, i32 4, i32 0)
  br i1 %t40, label %zr_aot_fn_0_ins_18, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_18:
  %t41 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 18, i32 5)
  br i1 %t41, label %zr_aot_fn_0_ins_18_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_18_body:
  %t42 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 4, i32 4, i32 0, ptr %direct_call)
  br i1 %t42, label %zr_aot_fn_0_ins_18_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_18_prepare_ok:
  %t43 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 4, i32 4, i32 0, i32 1)
  br i1 %t43, label %zr_aot_fn_0_ins_18_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_18_finish_ok:
  br label %zr_aot_fn_0_ins_19

zr_aot_fn_0_ins_19:
  %t44 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 19, i32 0)
  br i1 %t44, label %zr_aot_fn_0_ins_19_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_19_body:
  %t45 = call i1 @ZrLibrary_AotRuntime_ToInt(ptr %state, ptr %frame, i32 5, i32 4)
  br i1 %t45, label %zr_aot_fn_0_ins_20, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_20:
  %t46 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 20, i32 0)
  br i1 %t46, label %zr_aot_fn_0_ins_20_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_20_body:
  %t47 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 6, i32 3)
  br i1 %t47, label %zr_aot_fn_0_ins_21, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_21:
  %t48 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 21, i32 1)
  br i1 %t48, label %zr_aot_fn_0_ins_21_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_21_body:
  %t49 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 6, i32 6, i32 1)
  br i1 %t49, label %zr_aot_fn_0_ins_22, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_22:
  %t50 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 22, i32 5)
  br i1 %t50, label %zr_aot_fn_0_ins_22_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_22_body:
  %t51 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 6, i32 6, i32 0, ptr %direct_call)
  br i1 %t51, label %zr_aot_fn_0_ins_22_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_22_prepare_ok:
  %t52 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 6, i32 6, i32 0, i32 1)
  br i1 %t52, label %zr_aot_fn_0_ins_22_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_22_finish_ok:
  br label %zr_aot_fn_0_ins_23

zr_aot_fn_0_ins_23:
  %t53 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 23, i32 0)
  br i1 %t53, label %zr_aot_fn_0_ins_23_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_23_body:
  %t54 = call i1 @ZrLibrary_AotRuntime_ToInt(ptr %state, ptr %frame, i32 7, i32 6)
  br i1 %t54, label %zr_aot_fn_0_ins_24, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_24:
  %t55 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 24, i32 0)
  br i1 %t55, label %zr_aot_fn_0_ins_24_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_24_body:
  %t56 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 8, i32 2)
  br i1 %t56, label %zr_aot_fn_0_ins_25, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_25:
  %t57 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 25, i32 1)
  br i1 %t57, label %zr_aot_fn_0_ins_25_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_25_body:
  %t58 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 8, i32 8, i32 2)
  br i1 %t58, label %zr_aot_fn_0_ins_26, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_26:
  %t59 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 26, i32 0)
  br i1 %t59, label %zr_aot_fn_0_ins_26_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_26_body:
  %t60 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 9, i32 5)
  br i1 %t60, label %zr_aot_fn_0_ins_27, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_27:
  %t61 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 27, i32 0)
  br i1 %t61, label %zr_aot_fn_0_ins_27_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_27_body:
  %t62 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 10, i32 7)
  br i1 %t62, label %zr_aot_fn_0_ins_28, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_28:
  %t63 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 28, i32 5)
  br i1 %t63, label %zr_aot_fn_0_ins_28_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_28_body:
  %t64 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 8, i32 8, i32 2, ptr %direct_call)
  br i1 %t64, label %zr_aot_fn_0_ins_28_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_28_prepare_ok:
  %t65 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 8, i32 8, i32 2, i32 1)
  br i1 %t65, label %zr_aot_fn_0_ins_28_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_28_finish_ok:
  br label %zr_aot_fn_0_ins_29

zr_aot_fn_0_ins_29:
  %t66 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 29, i32 1)
  br i1 %t66, label %zr_aot_fn_0_ins_29_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_29_body:
  %t67 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 2)
  br i1 %t67, label %zr_aot_fn_0_ins_30, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_30:
  %t68 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 30, i32 1)
  br i1 %t68, label %zr_aot_fn_0_ins_30_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_30_body:
  %t69 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 2)
  br i1 %t69, label %zr_aot_fn_0_ins_31, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_31:
  %t70 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 31, i32 0)
  br i1 %t70, label %zr_aot_fn_0_ins_31_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_31_body:
  %t71 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t71, label %zr_aot_fn_0_ins_32, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_32:
  %t72 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 32, i32 1)
  br i1 %t72, label %zr_aot_fn_0_ins_32_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_32_body:
  %t73 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 9, i32 3)
  br i1 %t73, label %zr_aot_fn_0_ins_33, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_33:
  %t74 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 33, i32 1)
  br i1 %t74, label %zr_aot_fn_0_ins_33_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_33_body:
  %t75 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 9, i32 4)
  br i1 %t75, label %zr_aot_fn_0_ins_34, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_34:
  %t76 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 34, i32 1)
  br i1 %t76, label %zr_aot_fn_0_ins_34_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_34_body:
  %t77 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 9)
  br i1 %t77, label %zr_aot_fn_0_ins_35, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_35:
  %t78 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 35, i32 5)
  br i1 %t78, label %zr_aot_fn_0_ins_35_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_35_body:
  %t79 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 9, i32 9, i32 1, ptr %direct_call)
  br i1 %t79, label %zr_aot_fn_0_ins_35_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_35_prepare_ok:
  %t80 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 9, i32 9, i32 1, i32 1)
  br i1 %t80, label %zr_aot_fn_0_ins_35_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_35_finish_ok:
  br label %zr_aot_fn_0_ins_36

zr_aot_fn_0_ins_36:
  %t81 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 36, i32 1)
  br i1 %t81, label %zr_aot_fn_0_ins_36_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_36_body:
  %t82 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 2)
  br i1 %t82, label %zr_aot_fn_0_ins_37, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_37:
  %t83 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 37, i32 1)
  br i1 %t83, label %zr_aot_fn_0_ins_37_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_37_body:
  %t84 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 2)
  br i1 %t84, label %zr_aot_fn_0_ins_38, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_38:
  %t85 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 38, i32 0)
  br i1 %t85, label %zr_aot_fn_0_ins_38_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_38_body:
  %t86 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 9, i32 0)
  br i1 %t86, label %zr_aot_fn_0_ins_39, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_39:
  %t87 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 39, i32 1)
  br i1 %t87, label %zr_aot_fn_0_ins_39_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_39_body:
  %t88 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 9, i32 3)
  br i1 %t88, label %zr_aot_fn_0_ins_40, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_40:
  %t89 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 40, i32 1)
  br i1 %t89, label %zr_aot_fn_0_ins_40_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_40_body:
  %t90 = call i1 @ZrLibrary_AotRuntime_GetMember(ptr %state, ptr %frame, i32 9, i32 9, i32 4)
  br i1 %t90, label %zr_aot_fn_0_ins_41, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_41:
  %t91 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 41, i32 0)
  br i1 %t91, label %zr_aot_fn_0_ins_41_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_41_body:
  %t92 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 10, i32 8)
  br i1 %t92, label %zr_aot_fn_0_ins_42, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_42:
  %t93 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 42, i32 5)
  br i1 %t93, label %zr_aot_fn_0_ins_42_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_42_body:
  %t94 = call i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr %state, ptr %frame, i32 9, i32 9, i32 1, ptr %direct_call)
  br i1 %t94, label %zr_aot_fn_0_ins_42_prepare_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_42_prepare_ok:
  %t95 = call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr %state, ptr %frame, ptr %direct_call, i32 9, i32 9, i32 1, i32 1)
  br i1 %t95, label %zr_aot_fn_0_ins_42_finish_ok, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_42_finish_ok:
  br label %zr_aot_fn_0_ins_43

zr_aot_fn_0_ins_43:
  %t96 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 43, i32 1)
  br i1 %t96, label %zr_aot_fn_0_ins_43_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_43_body:
  %t97 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 10, i32 2)
  br i1 %t97, label %zr_aot_fn_0_ins_44, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_44:
  %t98 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 44, i32 1)
  br i1 %t98, label %zr_aot_fn_0_ins_44_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_44_body:
  %t99 = call i1 @ZrLibrary_AotRuntime_CopyConstant(ptr %state, ptr %frame, i32 9, i32 2)
  br i1 %t99, label %zr_aot_fn_0_ins_45, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_45:
  %t100 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 45, i32 0)
  br i1 %t100, label %zr_aot_fn_0_ins_45_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_45_body:
  %t101 = call i1 @ZrLibrary_AotRuntime_CopyStack(ptr %state, ptr %frame, i32 9, i32 8)
  br i1 %t101, label %zr_aot_fn_0_ins_46, label %zr_aot_fn_0_fail

zr_aot_fn_0_ins_46:
  %t102 = call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 46, i32 8)
  br i1 %t102, label %zr_aot_fn_0_ins_46_body, label %zr_aot_fn_0_fail
zr_aot_fn_0_ins_46_body:
  %t103 = call i64 @ZrLibrary_AotRuntime_Return(ptr %state, ptr %frame, i32 9, i1 false)
  ret i64 %t103

zr_aot_fn_0_end_unsupported:
  %t104 = call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 47, i32 0)
  ret i64 %t104

zr_aot_fn_0_fail:
  %t105 = call i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr %state, ptr %frame)
  ret i64 %t105
}

@zr_aot_function_thunks = private constant [1 x ptr] [ptr @zr_aot_fn_0]

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
  i64 2835,
  ptr @zr_aot_function_thunks,
  i32 1,
  ptr @zr_aot_entry
}

; export-symbol: ZrVm_GetAotCompiledModule
; descriptor.moduleName = main
; descriptor.inputKind = 1
; descriptor.inputHash = 519d8fc4249f42aa
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
