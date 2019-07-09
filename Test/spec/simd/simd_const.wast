;; v128.const normal parameter (e.g. (i8x16, i16x8 i32x4, f32x4))

(module (func (v128.const i8x16  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF) drop))
(module (func (v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80) drop))
(module (func (v128.const i8x16  255  255  255  255  255  255  255  255  255  255  255  255  255  255  255  255) drop))
(module (func (v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128) drop))
(module (func (v128.const i16x8  0xFFFF  0xFFFF  0xFFFF  0xFFFF  0xFFFF  0xFFFF  0xFFFF  0xFFFF) drop))
(module (func (v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000) drop))
(module (func (v128.const i16x8  65535  65535  65535  65535  65535  65535  65535  65535) drop))
(module (func (v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768) drop))
(module (func (v128.const i32x4  0xffffffff  0xffffffff  0xffffffff  0xffffffff) drop))
(module (func (v128.const i32x4 -0x80000000 -0x80000000 -0x80000000 -0x80000000) drop))
(module (func (v128.const i32x4  4294967295  4294967295  4294967295  4294967295) drop))
(module (func (v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648) drop))
(module (func (v128.const f32x4  0x1p127  0x1p127  0x1p127  0x1p127) drop))
(module (func (v128.const f32x4 -0x1p127 -0x1p127 -0x1p127 -0x1p127) drop))
(module (func (v128.const f32x4  1e38  1e38  1e38  1e38) drop))
(module (func (v128.const f32x4 -1e38 -1e38 -1e38 -1e38) drop))
(module (func (v128.const f32x4  340282356779733623858607532500980858880 340282356779733623858607532500980858880
                                 340282356779733623858607532500980858880 340282356779733623858607532500980858880) drop))
(module (func (v128.const f32x4 -340282356779733623858607532500980858880 -340282356779733623858607532500980858880
                                -340282356779733623858607532500980858880 -340282356779733623858607532500980858880) drop))
(module (func (v128.const f32x4 nan:0x1 nan:0x1 nan:0x1 nan:0x1) drop))
(module (func (v128.const f32x4 nan:0x7f_ffff nan:0x7f_ffff nan:0x7f_ffff nan:0x7f_ffff) drop))

;; Non-splat cases

(module (func (v128.const i8x16  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF  0xFF
                                -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80) drop))
(module (func (v128.const i8x16  0xFF  0xFF  0xFF  0xFF   255   255   255   255
                                -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80) drop))
(module (func (v128.const i8x16  0xFF  0xFF  0xFF  0xFF   255   255   255   255
                                -0x80 -0x80 -0x80 -0x80  -128  -128  -128  -128) drop))
(module (func (v128.const i16x8 0xFF 0xFF  0xFF  0xFF -0x8000 -0x8000 -0x8000 -0x8000) drop))
(module (func (v128.const i16x8 0xFF 0xFF 65535 65535 -0x8000 -0x8000 -0x8000 -0x8000) drop))
(module (func (v128.const i16x8 0xFF 0xFF 65535 65535 -0x8000 -0x8000  -32768  -32768) drop))
(module (func (v128.const i32x4 0xffffffff 0xffffffff -0x80000000 -0x80000000) drop))
(module (func (v128.const i32x4 0xffffffff 4294967295 -0x80000000 -0x80000000) drop))
(module (func (v128.const i32x4 0xffffffff 4294967295 -0x80000000 -2147483648) drop))


;; Constant out of range (int literal is too large)

(module (memory 1))
(assert_malformed
  (module quote "(func (v128.const i8x16 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i8x16 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i8x16 256 256 256 256 256 256 256 256 256 256 256 256 256 256 256 256) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i8x16 -129 -129 -129 -129 -129 -129 -129 -129 -129 -129 -129 -129 -129 -129 -129 -129) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i16x8 0x10000 0x10000 0x10000 0x10000 0x10000 0x10000 0x10000 0x10000) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i16x8 -0x8001 -0x8001 -0x8001 -0x8001 -0x8001 -0x8001 -0x8001 -0x8001) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i16x8 65536 65536 65536 65536 65536 65536 65536 65536) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i16x8 -32769 -32769 -32769 -32769 -32769 -32769 -32769 -32769) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i32x4  0x100000000  0x100000000  0x100000000  0x100000000) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i32x4 -0x80000001 -0x80000001 -0x80000001 -0x80000001) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i32x4  4294967296  4294967296  4294967296  4294967296) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const i32x4 -2147483649 -2147483649 -2147483649 -2147483649) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const f32x4  0x1p128  0x1p128  0x1p128  0x1p128) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const f32x4 -0x1p128 -0x1p128 -0x1p128 -0x1p128) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const f32x4  1e39  1e39  1e39  1e39) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const f32x4 -1e39 -1e39 -1e39 -1e39) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const f32x4  340282356779733661637539395458142568448 340282356779733661637539395458142568448"
                "                         340282356779733661637539395458142568448 340282356779733661637539395458142568448) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const f32x4 -340282356779733661637539395458142568448 -340282356779733661637539395458142568448"
                "                        -340282356779733661637539395458142568448 -340282356779733661637539395458142568448) drop)")
  "constant out of range"
)
(assert_malformed
  (module quote "(func (v128.const f32x4 nan:1 nan:1 nan:1 nan:1) drop)")
  "unknown operator"
)

(assert_malformed
  (module quote "(func (v128.const f32x4 nan:0x0 nan:0x0 nan:0x0 nan:0x0) drop)")
  "constant out of range"
)

(assert_malformed
  (module quote "(func (v128.const f32x4 nan:0x80_0000 nan:0x80_0000 nan:0x80_0000 nan:0x80_0000) drop)")
  "constant out of range"
)


;; Rounding behaviour

;; f32x4, small exponent
(module (func (export "f") (result v128) (v128.const f32x4 +0x1.00000100000000000p-50 +0x1.00000100000000000p-50 +0x1.00000100000000000p-50 +0x1.00000100000000000p-50)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000000p-50 +0x1.000000p-50 +0x1.000000p-50 +0x1.000000p-50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x1.00000100000000000p-50 -0x1.00000100000000000p-50 -0x1.00000100000000000p-50 -0x1.00000100000000000p-50)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000000p-50 -0x1.000000p-50 -0x1.000000p-50 -0x1.000000p-50))
(module (func (export "f") (result v128) (v128.const f32x4 +0x1.00000500000000001p-50 +0x1.00000500000000001p-50 +0x1.00000500000000001p-50 +0x1.00000500000000001p-50)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000006p-50 +0x1.000006p-50 +0x1.000006p-50 +0x1.000006p-50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x1.00000500000000001p-50 -0x1.00000500000000001p-50 -0x1.00000500000000001p-50 -0x1.00000500000000001p-50)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000006p-50 -0x1.000006p-50 -0x1.000006p-50 -0x1.000006p-50))

(module (func (export "f") (result v128) (v128.const f32x4 +0x4000.004000000p-64 +0x4000.004000000p-64 +0x4000.004000000p-64 +0x4000.004000000p-64)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000000p-50 +0x1.000000p-50 +0x1.000000p-50 +0x1.000000p-50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x4000.004000000p-64 -0x4000.004000000p-64 -0x4000.004000000p-64 -0x4000.004000000p-64)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000000p-50 -0x1.000000p-50 -0x1.000000p-50 -0x1.000000p-50))
(module (func (export "f") (result v128) (v128.const f32x4 +0x4000.014000001p-64 +0x4000.014000001p-64 +0x4000.014000001p-64 +0x4000.014000001p-64)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000006p-50 +0x1.000006p-50 +0x1.000006p-50 +0x1.000006p-50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x4000.014000001p-64 -0x4000.014000001p-64 -0x4000.014000001p-64 -0x4000.014000001p-64)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000006p-50 -0x1.000006p-50 -0x1.000006p-50 -0x1.000006p-50))

(module (func (export "f") (result v128) (v128.const f32x4 +8.8817847263968443573e-16 +8.8817847263968443573e-16 +8.8817847263968443573e-16 +8.8817847263968443573e-16)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000000p-50 +0x1.000000p-50 +0x1.000000p-50 +0x1.000000p-50))
(module (func (export "f") (result v128) (v128.const f32x4 -8.8817847263968443573e-16 -8.8817847263968443573e-16 -8.8817847263968443573e-16 -8.8817847263968443573e-16)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000000p-50 -0x1.000000p-50 -0x1.000000p-50 -0x1.000000p-50))
(module (func (export "f") (result v128) (v128.const f32x4 +8.8817857851880284253e-16 +8.8817857851880284253e-16 +8.8817857851880284253e-16 +8.8817857851880284253e-16)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000004p-50 +0x1.000004p-50 +0x1.000004p-50 +0x1.000004p-50))
(module (func (export "f") (result v128) (v128.const f32x4 -8.8817857851880284253e-16 -8.8817857851880284253e-16 -8.8817857851880284253e-16 -8.8817857851880284253e-16)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000004p-50 -0x1.000004p-50 -0x1.000004p-50 -0x1.000004p-50))

;; f32x4, large exponent
(module (func (export "f") (result v128) (v128.const f32x4 +0x1.00000100000000000p+50 +0x1.00000100000000000p+50 +0x1.00000100000000000p+50 +0x1.00000100000000000p+50)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000000p+50 +0x1.000000p+50 +0x1.000000p+50 +0x1.000000p+50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x1.00000100000000000p+50 -0x1.00000100000000000p+50 -0x1.00000100000000000p+50 -0x1.00000100000000000p+50)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000000p+50 -0x1.000000p+50 -0x1.000000p+50 -0x1.000000p+50))
(module (func (export "f") (result v128) (v128.const f32x4 +0x1.00000500000000001p+50 +0x1.00000500000000001p+50 +0x1.00000500000000001p+50 +0x1.00000500000000001p+50)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000006p+50 +0x1.000006p+50 +0x1.000006p+50 +0x1.000006p+50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x1.00000500000000001p+50 -0x1.00000500000000001p+50 -0x1.00000500000000001p+50 -0x1.00000500000000001p+50)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000006p+50 -0x1.000006p+50 -0x1.000006p+50 -0x1.000006p+50))

(module (func (export "f") (result v128) (v128.const f32x4 +0x4000004000000 +0x4000004000000 +0x4000004000000 +0x4000004000000)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000000p+50 +0x1.000000p+50 +0x1.000000p+50 +0x1.000000p+50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x4000004000000 -0x4000004000000 -0x4000004000000 -0x4000004000000)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000000p+50 -0x1.000000p+50 -0x1.000000p+50 -0x1.000000p+50))
(module (func (export "f") (result v128) (v128.const f32x4 +0x400000c000000 +0x400000c000000 +0x400000c000000 +0x400000c000000)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000004p+50 +0x1.000004p+50 +0x1.000004p+50 +0x1.000004p+50))
(module (func (export "f") (result v128) (v128.const f32x4 -0x400000c000000 -0x400000c000000 -0x400000c000000 -0x400000c000000)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000004p+50 -0x1.000004p+50 -0x1.000004p+50 -0x1.000004p+50))

(module (func (export "f") (result v128) (v128.const f32x4 +1125899973951488 +1125899973951488 +1125899973951488 +1125899973951488)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000000p+50 +0x1.000000p+50 +0x1.000000p+50 +0x1.000000p+50))
(module (func (export "f") (result v128) (v128.const f32x4 -1125899973951488 -1125899973951488 -1125899973951488 -1125899973951488)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000000p+50 -0x1.000000p+50 -0x1.000000p+50 -0x1.000000p+50))
(module (func (export "f") (result v128) (v128.const f32x4 +1125900108169216 +1125900108169216 +1125900108169216 +1125900108169216)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.000004p+50 +0x1.000004p+50 +0x1.000004p+50 +0x1.000004p+50))
(module (func (export "f") (result v128) (v128.const f32x4 -1125900108169216 -1125900108169216 -1125900108169216 -1125900108169216)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.000004p+50 -0x1.000004p+50 -0x1.000004p+50 -0x1.000004p+50))

;; f32x4, subnormal
(module (func (export "f") (result v128) (v128.const f32x4 +0x0.00000100000000000p-126 +0x0.00000100000000000p-126 +0x0.00000100000000000p-126 +0x0.00000100000000000p-126)))
(assert_return (invoke "f") (v128.const f32x4 +0x0.000000p-126 +0x0.000000p-126 +0x0.000000p-126 +0x0.000000p-126))
(module (func (export "f") (result v128) (v128.const f32x4 -0x0.00000100000000000p-126 -0x0.00000100000000000p-126 -0x0.00000100000000000p-126 -0x0.00000100000000000p-126)))
(assert_return (invoke "f") (v128.const f32x4 -0x0.000000p-126 -0x0.000000p-126 -0x0.000000p-126 -0x0.000000p-126))
(module (func (export "f") (result v128) (v128.const f32x4 +0x0.00000500000000001p-126 +0x0.00000500000000001p-126 +0x0.00000500000000001p-126 +0x0.00000500000000001p-126)))
(assert_return (invoke "f") (v128.const f32x4 +0x0.000006p-126 +0x0.000006p-126 +0x0.000006p-126 +0x0.000006p-126))
(module (func (export "f") (result v128) (v128.const f32x4 -0x0.00000500000000001p-126 -0x0.00000500000000001p-126 -0x0.00000500000000001p-126 -0x0.00000500000000001p-126)))
(assert_return (invoke "f") (v128.const f32x4 -0x0.000006p-126 -0x0.000006p-126 -0x0.000006p-126 -0x0.000006p-126))

;; f32x4, round down at limit to infinity
(module (func (export "f") (result v128) (v128.const f32x4 +0x1.fffffe8p127 +0x1.fffffe8p127 +0x1.fffffe8p127 +0x1.fffffe8p127)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.fffffep127 +0x1.fffffep127 +0x1.fffffep127 +0x1.fffffep127))
(module (func (export "f") (result v128) (v128.const f32x4 -0x1.fffffe8p127 -0x1.fffffe8p127 -0x1.fffffe8p127 -0x1.fffffe8p127)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.fffffep127 -0x1.fffffep127 -0x1.fffffep127 -0x1.fffffep127))
(module (func (export "f") (result v128) (v128.const f32x4 +0x1.fffffefffffffffffp127 +0x1.fffffefffffffffffp127 +0x1.fffffefffffffffffp127 +0x1.fffffefffffffffffp127)))
(assert_return (invoke "f") (v128.const f32x4 +0x1.fffffep127 +0x1.fffffep127 +0x1.fffffep127 +0x1.fffffep127))
(module (func (export "f") (result v128) (v128.const f32x4 -0x1.fffffefffffffffffp127 -0x1.fffffefffffffffffp127 -0x1.fffffefffffffffffp127 -0x1.fffffefffffffffffp127)))
(assert_return (invoke "f") (v128.const f32x4 -0x1.fffffep127 -0x1.fffffep127 -0x1.fffffep127 -0x1.fffffep127))


;; As parameters of control constructs

(module (memory 1)
  (func (export "as-br-retval") (result v128)
    (block (result v128) (br 0 (v128.const i32x4 0x03020100 0x07060504 0x0b0a0908 0x0f0e0d0c)))
  )
  (func (export "as-br_if-retval") (result v128)
    (block (result v128)
      (br_if 0 (v128.const i32x4 0 1 2 3) (i32.const 1))
    )
  )
  (func (export "as-return-retval") (result v128)
    (return (v128.const i32x4 0 1 2 3))
  )
  (func (export "as-if-then-retval") (result v128)
    (if (result v128) (i32.const 1)
      (then (v128.const i32x4 0 1 2 3)) (else (v128.const i32x4 3 2 1 0))
    )
  )
  (func (export "as-if-else-retval") (result v128)
    (if (result v128) (i32.const 0)
      (then (v128.const i32x4 0 1 2 3)) (else (v128.const i32x4 3 2 1 0))
    )
  )
  (func $f (param v128 v128 v128) (result v128) (v128.const i32x4 0 1 2 3))
  (func (export "as-call-param") (result v128)
    (call $f (v128.const i32x4 0 1 2 3) (v128.const i32x4 0 1 2 3) (v128.const i32x4 0 1 2 3))
  )
  (type $sig (func (param v128 v128 v128) (result v128)))
  (table funcref (elem $f))
  (func (export "as-call_indirect-param") (result v128)
    (call_indirect (type $sig)
      (v128.const i32x4 0 1 2 3) (v128.const i32x4 0 1 2 3) (v128.const i32x4 0 1 2 3) (i32.const 0)
    )
  )
  (func (export "as-block-retval") (result v128)
    (block (result v128) (v128.const i32x4 0 1 2 3))
  )
  (func (export "as-loop-retval") (result v128)
    (loop (result v128) (v128.const i32x4 0 1 2 3))
  )
  (func (export "as-drop-operand")
    (drop (v128.const i32x4 0 1 2 3))
  )
)

(assert_return (invoke "as-br-retval") (v128.const i32x4 0x03020100 0x07060504 0x0b0a0908 0x0f0e0d0c))
(assert_return (invoke "as-br_if-retval") (v128.const i32x4 0 1 2 3))
(assert_return (invoke "as-return-retval") (v128.const i32x4 0 1 2 3))
(assert_return (invoke "as-if-then-retval") (v128.const i32x4 0 1 2 3))
(assert_return (invoke "as-if-else-retval") (v128.const i32x4 3 2 1 0))
(assert_return (invoke "as-call-param") (v128.const i32x4 0 1 2 3))
(assert_return (invoke "as-call_indirect-param") (v128.const i32x4 0 1 2 3))
(assert_return (invoke "as-block-retval") (v128.const i32x4 0 1 2 3))
(assert_return (invoke "as-loop-retval") (v128.const i32x4 0 1 2 3))
(assert_return (invoke "as-drop-operand"))


;; v128 locals

(module (memory 1)
  (func (export "as-local.set/get-value_0_0") (param $0 v128) (result v128)
    (local v128 v128 v128 v128)
    (local.set 0 (local.get $0))
    (local.get 0)
  )
  (func (export "as-local.set/get-value_0_1") (param $0 v128) (result v128)
    (local v128 v128 v128 v128)
    (local.set 0 (local.get $0))
    (local.set 1 (local.get 0))
    (local.set 2 (local.get 1))
    (local.set 3 (local.get 2))
    (local.get 0)
  )
  (func (export "as-local.set/get-value_3_0") (param $0 v128) (result v128)
    (local v128 v128 v128 v128)
    (local.set 0 (local.get $0))
    (local.set 1 (local.get 0))
    (local.set 2 (local.get 1))
    (local.set 3 (local.get 2))
    (local.get 3)
  )
  (func (export "as-local.tee-value") (result v128)
    (local v128)
    (local.tee 0 (v128.const i32x4 0 1 2 3))
  )
)

(assert_return (invoke "as-local.set/get-value_0_0" (v128.const i32x4 0 0 0 0)) (v128.const i32x4 0 0 0 0))
(assert_return (invoke "as-local.set/get-value_0_1" (v128.const i32x4 1 1 1 1)) (v128.const i32x4 1 1 1 1))
(assert_return (invoke "as-local.set/get-value_3_0" (v128.const i32x4 2 2 2 2)) (v128.const i32x4 2 2 2 2))
(assert_return (invoke "as-local.tee-value") (v128.const i32x4 0 1 2 3))


;; v128 globals

(module (memory 1)
  (global $g0 (mut v128) (v128.const i32x4 0 1 2 3))
  (global $g1 (mut v128) (v128.const i32x4 4 5 6 7))
  (global $g2 (mut v128) (v128.const i32x4 8 9 10 11))
  (global $g3 (mut v128) (v128.const i32x4 12 13 14 15))
  (global $g4 (mut v128) (v128.const i32x4 16 17 18 19))

  (func $set_g0 (export "as-global.set_value_$g0") (param $0 v128)
    (global.set $g0 (local.get $0))
  )
  (func $set_g1_g2 (export "as-global.set_value_$g1_$g2") (param $0 v128) (param $1 v128)
    (global.set $g1 (local.get $0))
    (global.set $g2 (local.get $1))
  )
  (func $set_g0_g1_g2_g3 (export "as-global.set_value_$g0_$g1_$g2_$g3") (param $0 v128) (param $1 v128) (param $2 v128) (param $3 v128)
    (call $set_g0 (local.get $0))
    (call $set_g1_g2 (local.get $1) (local.get $2))
    (global.set $g3 (local.get $3))
  )
  (func (export "global.get_g0") (result v128)
    (global.get $g0)
  )
  (func (export "global.get_g1") (result v128)
    (global.get $g1)
  )
  (func (export "global.get_g2") (result v128)
    (global.get $g2)
  )
  (func (export "global.get_g3") (result v128)
    (global.get $g3)
  )
)

(assert_return (invoke "as-global.set_value_$g0_$g1_$g2_$g3" (v128.const i32x4 1 1 1 1)
                                                             (v128.const i32x4 2 2 2 2)
                                                             (v128.const i32x4 3 3 3 3)
                                                             (v128.const i32x4 4 4 4 4)))
(assert_return (invoke "global.get_g0") (v128.const i32x4 1 1 1 1))
(assert_return (invoke "global.get_g1") (v128.const i32x4 2 2 2 2))
(assert_return (invoke "global.get_g2") (v128.const i32x4 3 3 3 3))
(assert_return (invoke "global.get_g3") (v128.const i32x4 4 4 4 4))


;; Test integer literal parsing.

(module
  (func (export "i32x4.test") (result v128) (return (v128.const i32x4 0x0bAdD00D 0x0bAdD00D 0x0bAdD00D 0x0bAdD00D)))
  (func (export "i32x4.smax") (result v128) (return (v128.const i32x4 0x7fffffff 0x7fffffff 0x7fffffff 0x7fffffff)))
  (func (export "i32x4.neg_smax") (result v128) (return (v128.const i32x4 -0x7fffffff -0x7fffffff -0x7fffffff -0x7fffffff)))
  (func (export "i32x4.inc_smin") (result v128) (return (i32x4.add (v128.const i32x4 -0x80000000 -0x80000000 -0x80000000 -0x80000000) (v128.const i32x4 1 1 1 1))))
  (func (export "i32x4.neg_zero") (result v128) (return (v128.const i32x4 -0x0 -0x0 -0x0 -0x0)))
  (func (export "i32x4.not_octal") (result v128) (return (v128.const i32x4 010 010 010 010)))
  (func (export "i32x4.plus_sign") (result v128) (return (v128.const i32x4 +42 +42 +42 +42)))

  (func (export "i32x4-dec-sep1") (result v128) (v128.const i32x4 1_000_000 1_000_000 1_000_000 1_000_000))
  (func (export "i32x4-dec-sep2") (result v128) (v128.const i32x4 1_0_0_0 1_0_0_0 1_0_0_0 1_0_0_0))
  (func (export "i32x4-hex-sep1") (result v128) (v128.const i32x4 0xa_0f_00_99 0xa_0f_00_99 0xa_0f_00_99 0xa_0f_00_99))
  (func (export "i32x4-hex-sep2") (result v128) (v128.const i32x4 0x1_a_A_0_f 0x1_a_A_0_f 0x1_a_A_0_f 0x1_a_A_0_f))
)

(assert_return (invoke "i32x4.test") (v128.const i32x4 195940365 195940365 195940365 195940365))
(assert_return (invoke "i32x4.smax") (v128.const i32x4 2147483647 2147483647 2147483647 2147483647))
(assert_return (invoke "i32x4.neg_smax") (v128.const i32x4 -2147483647 -2147483647 -2147483647 -2147483647))
(assert_return (invoke "i32x4.inc_smin") (v128.const i32x4 -2147483647 -2147483647 -2147483647 -2147483647))
(assert_return (invoke "i32x4.neg_zero") (v128.const i32x4 0 0 0 0))
(assert_return (invoke "i32x4.not_octal") (v128.const i32x4 10 10 10 10))
(assert_return (invoke "i32x4.plus_sign") (v128.const i32x4 42 42 42 42))

(assert_return (invoke "i32x4-dec-sep1") (v128.const i32x4 1000000 1000000 1000000 1000000))
(assert_return (invoke "i32x4-dec-sep2") (v128.const i32x4 1000 1000 1000 1000))
(assert_return (invoke "i32x4-hex-sep1") (v128.const i32x4 0xa0f0099 0xa0f0099 0xa0f0099 0xa0f0099))
(assert_return (invoke "i32x4-hex-sep2") (v128.const i32x4 0x1aa0f 0x1aa0f 0x1aa0f 0x1aa0f))

(assert_malformed
  (module quote "(global v128 (v128.const i32x4 _100 _100 _100 _100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 +_100 +_100 +_100 +_100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 -_100 -_100 -_100 -_100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 99_ 99_ 99_ 99_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 1__000 1__000 1__000 1__000))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 _0x100 _0x100 _0x100 _0x100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 0_x100 0_x100 0_x100 0_x100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 0x_100 0x_100 0x_100 0x_100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 0x00_ 0x00_ 0x00_ 0x00_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const i32x4 0xff__ffff 0xff__ffff 0xff__ffff 0xff__ffff))")
  "unknown operator"
)


;; Test floating-point literal parsing.

(module
  (func (export "f32-dec-sep1") (result v128) (v128.const f32x4 1_000_000 1_000_000 1_000_000 1_000_000))
  (func (export "f32-dec-sep2") (result v128) (v128.const f32x4 1_0_0_0 1_0_0_0 1_0_0_0 1_0_0_0))
  (func (export "f32-dec-sep3") (result v128) (v128.const f32x4 100_3.141_592 100_3.141_592 100_3.141_592 100_3.141_592))
  (func (export "f32-dec-sep4") (result v128) (v128.const f32x4 99e+1_3 99e+1_3 99e+1_3 99e+1_3))
  (func (export "f32-dec-sep5") (result v128) (v128.const f32x4 122_000.11_3_54E0_2_3 122_000.11_3_54E0_2_3 122_000.11_3_54E0_2_3 122_000.11_3_54E0_2_3))
  (func (export "f32-hex-sep1") (result v128) (v128.const f32x4 0xa_0f_00_99 0xa_0f_00_99 0xa_0f_00_99 0xa_0f_00_99))
  (func (export "f32-hex-sep2") (result v128) (v128.const f32x4 0x1_a_A_0_f 0x1_a_A_0_f 0x1_a_A_0_f 0x1_a_A_0_f))
  (func (export "f32-hex-sep3") (result v128) (v128.const f32x4 0xa0_ff.f141_a59a 0xa0_ff.f141_a59a 0xa0_ff.f141_a59a 0xa0_ff.f141_a59a))
  (func (export "f32-hex-sep4") (result v128) (v128.const f32x4 0xf0P+1_3 0xf0P+1_3 0xf0P+1_3 0xf0P+1_3))
  (func (export "f32-hex-sep5") (result v128) (v128.const f32x4 0x2a_f00a.1f_3_eep2_3 0x2a_f00a.1f_3_eep2_3 0x2a_f00a.1f_3_eep2_3 0x2a_f00a.1f_3_eep2_3))
)

(assert_return (invoke "f32-dec-sep1") (v128.const f32x4 1000000 1000000 1000000 1000000))
(assert_return (invoke "f32-dec-sep2") (v128.const f32x4 1000 1000 1000 1000))
(assert_return (invoke "f32-dec-sep3") (v128.const f32x4 1003.141592 1003.141592 1003.141592 1003.141592))
(assert_return (invoke "f32-dec-sep4") (v128.const f32x4 99e+13 99e+13 99e+13 99e+13))
(assert_return (invoke "f32-dec-sep5") (v128.const f32x4 122000.11354e23 122000.11354e23 122000.11354e23 122000.11354e23))
(assert_return (invoke "f32-hex-sep1") (v128.const f32x4 0xa0f0099 0xa0f0099 0xa0f0099 0xa0f0099))
(assert_return (invoke "f32-hex-sep2") (v128.const f32x4 0x1aa0f 0x1aa0f 0x1aa0f 0x1aa0f))
(assert_return (invoke "f32-hex-sep3") (v128.const f32x4 0xa0ff.f141a59a 0xa0ff.f141a59a 0xa0ff.f141a59a 0xa0ff.f141a59a))
(assert_return (invoke "f32-hex-sep4") (v128.const f32x4 0xf0P+13 0xf0P+13 0xf0P+13 0xf0P+13))
(assert_return (invoke "f32-hex-sep5") (v128.const f32x4 0x2af00a.1f3eep23 0x2af00a.1f3eep23 0x2af00a.1f3eep23 0x2af00a.1f3eep23))

(assert_malformed
  (module quote "(global v128 (v128.const f32x4 _100 _100 _100 _100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 +_100 +_100 +_100 +_100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 -_100 -_100 -_100 -_100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 99_ 99_ 99_ 99_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1__000 1__000 1__000 1__000))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 _1.0 _1.0 _1.0 _1.0))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1.0_ 1.0_ 1.0_ 1.0_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1_.0 1_.0 1_.0 1_.0))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1._0 1._0 1._0 1._0))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 _1e1 _1e1 _1e1 _1e1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1e1_ 1e1_ 1e1_ 1e1_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1_e1 1_e1 1_e1 1_e1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1e_1 1e_1 1e_1 1e_1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 _1.0e1 _1.0e1 _1.0e1 _1.0e1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1.0e1_ 1.0e1_ 1.0e1_ 1.0e1_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1.0_e1 1.0_e1 1.0_e1 1.0_e1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1.0e_1 1.0e_1 1.0e_1 1.0e_1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1.0e+_1 1.0e+_1 1.0e+_1 1.0e+_1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 1.0e_+1 1.0e_+1 1.0e_+1 1.0e_+1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 _0x100 _0x100 _0x100 _0x100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0_x100 0_x100 0_x100 0_x100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x_100 0x_100 0x_100 0x_100))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x00_ 0x00_ 0x00_ 0x00_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0xff__ffff 0xff__ffff 0xff__ffff 0xff__ffff))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x_1.0 0x_1.0 0x_1.0 0x_1.0))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1.0_ 0x1.0_ 0x1.0_ 0x1.0_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1_.0 0x1_.0 0x1_.0 0x1_.0))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1._0 0x1._0 0x1._0 0x1._0))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x_1p1 0x_1p1 0x_1p1 0x_1p1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1p1_ 0x1p1_ 0x1p1_ 0x1p1_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1_p1 0x1_p1 0x1_p1 0x1_p1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1p_1 0x1p_1 0x1p_1 0x1p_1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x_1.0p1 0x_1.0p1 0x_1.0p1 0x_1.0p1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1.0p1_ 0x1.0p1_ 0x1.0p1_ 0x1.0p1_))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1.0_p1 0x1.0_p1 0x1.0_p1 0x1.0_p1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1.0p_1 0x1.0p_1 0x1.0p_1 0x1.0p_1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1.0p+_1 0x1.0p+_1 0x1.0p+_1 0x1.0p+_1))")
  "unknown operator"
)
(assert_malformed
  (module quote "(global v128 (v128.const f32x4 0x1.0p_+1 0x1.0p_+1 0x1.0p_+1 0x1.0p_+1))")
  "unknown operator"
)


;; Test parsing a integer from binary

(module binary
  ;; (func (export "4294967249") (result v128) (v128.const i32x4 4294967249 4294967249 4294967249 4294967249))

  "\00\61\73\6d\01\00\00\00\01\05\01\60\00\01\7b\03"
  "\02\01\00\07\0e\01\0a\34\32\39\34\39\36\37\32\34"
  "\39\00\00\0a\16\01\14\00\fd\02\d1\ff\ff\ff\d1\ff"
  "\ff\ff\d1\ff\ff\ff\d1\ff\ff\ff\0b\00\2c\04\6e\61"
  "\6d\65\00\01\00\01\03\01\00\00\02\03\01\00\00\03"
  "\03\01\00\00\04\01\00\05\01\00\06\01\00\07\01\00"
  "\08\01\00\09\01\00\0a\01\00\00\a8\14\17\77\61\76"
  "\6d\2e\70\72\65\63\6f\6d\70\69\6c\65\64\5f\6f\62"
  "\6a\65\63\74\7f\45\4c\46\02\01\01\00\00\00\00\00"
  "\00\00\00\00\01\00\3e\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\90\05\00\00"
  "\00\00\00\00\00\00\00\00\40\00\00\00\00\00\40\00"
  "\12\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\55\48\89\e5\48\89\f8\48\b9\00\00\00"
  "\00\00\00\00\00\c5\f8\28\01\5d\c3\00\00\00\00\00"
  "\00\00\00\00\d1\ff\ff\ff\d1\ff\ff\ff\d1\ff\ff\ff"
  "\d1\ff\ff\ff\57\41\56\4d\00\75\6e\6b\6e\6f\77\6e"
  "\00\66\75\6e\63\74\69\6f\6e\44\65\66\30\00\01\11"
  "\01\25\0e\13\05\03\0e\10\17\1b\0e\e1\7f\19\11\01"
  "\12\06\00\00\02\2e\00\11\01\12\06\40\18\6e\0e\03"
  "\0e\3f\19\e1\7f\19\00\00\00\3e\00\00\00\04\00\00"
  "\00\00\00\08\01\00\00\00\00\ff\ff\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\17"
  "\00\00\00\02\00\00\00\00\00\00\00\00\17\00\00\00"
  "\01\56\00\00\00\00\00\00\00\00\00\00\4c\00\00\00"
  "\05\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\01\00\00\00\01\00\00\00\07\00\00\00\08\00\00\00"
  "\4c\4c\56\4d\30\37\30\30\00\00\00\00\01\00\00\00"
  "\aa\f6\05\41\00\00\00\00\00\00\00\00\2e\2e\03\13"
  "\00\00\00\2e\2a\00\00\00\00\00\00\00\14\00\00\00"
  "\00\00\00\00\01\7a\52\00\01\78\10\01\1c\0c\07\08"
  "\90\01\00\00\24\00\00\00\1c\00\00\00\00\00\00\00"
  "\00\00\00\00\17\00\00\00\00\00\00\00\00\41\0e\10"
  "\86\02\43\0d\06\00\00\00\00\00\00\00\39\00\00\00"
  "\04\00\1f\00\00\00\01\01\01\fb\0e\0d\00\01\01\01"
  "\01\00\00\00\01\00\00\01\00\75\6e\6b\6e\6f\77\6e"
  "\00\00\00\00\00\00\09\02\00\00\00\00\00\00\00\00"
  "\11\0a\08\13\02\06\00\01\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\e9\00\00\00\04\00\f1\ff\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\03\00\02\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\03\00\04\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\03\00\05\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\03\00\06\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\03\00\07\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\03\00\0f\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\87\00\00\00"
  "\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\d5\00\00\00\12\00\02\00\20\00\00\00"
  "\00\00\00\00\17\00\00\00\00\00\00\00\bc\00\00\00"
  "\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\e2\00\00\00\10\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\08\00\00\00"
  "\00\00\00\00\01\00\00\00\0a\00\00\00\00\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\01\00\00\00"
  "\08\00\00\00\ff\ff\ff\ff\ff\ff\ff\ff\18\00\00\00"
  "\00\00\00\00\01\00\00\00\0b\00\00\00\00\00\00\00"
  "\00\00\00\00\29\00\00\00\00\00\00\00\01\00\00\00"
  "\03\00\00\00\00\00\00\00\00\00\00\00\06\00\00\00"
  "\00\00\00\00\0a\00\00\00\05\00\00\00\00\00\00\00"
  "\00\00\00\00\0c\00\00\00\00\00\00\00\0a\00\00\00"
  "\04\00\00\00\00\00\00\00\00\00\00\00\12\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\05\00\00\00"
  "\00\00\00\00\16\00\00\00\00\00\00\00\0a\00\00\00"
  "\07\00\00\00\00\00\00\00\00\00\00\00\1a\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\05\00\00\00"
  "\00\00\00\00\1e\00\00\00\00\00\00\00\01\00\00\00"
  "\02\00\00\00\20\00\00\00\00\00\00\00\2b\00\00\00"
  "\00\00\00\00\01\00\00\00\02\00\00\00\20\00\00\00"
  "\00\00\00\00\39\00\00\00\00\00\00\00\0a\00\00\00"
  "\04\00\00\00\0d\00\00\00\00\00\00\00\3d\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\0d\00\00\00"
  "\00\00\00\00\2c\00\00\00\00\00\00\00\0a\00\00\00"
  "\06\00\00\00\00\00\00\00\00\00\00\00\38\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\0d\00\00\00"
  "\00\00\00\00\20\00\00\00\00\00\00\00\18\00\00\00"
  "\02\00\00\00\20\00\00\00\00\00\00\00\2c\00\00\00"
  "\00\00\00\00\01\00\00\00\02\00\00\00\20\00\00\00"
  "\00\00\00\00\00\2e\64\65\62\75\67\5f\61\62\62\72"
  "\65\76\00\2e\72\65\6c\61\2e\74\65\78\74\00\2e\72"
  "\65\6c\61\2e\64\65\62\75\67\5f\6e\61\6d\65\73\00"
  "\2e\64\65\62\75\67\5f\73\74\72\00\2e\64\65\62\75"
  "\67\5f\6d\61\63\69\6e\66\6f\00\2e\72\65\6c\61\2e"
  "\64\65\62\75\67\5f\69\6e\66\6f\00\2e\6e\6f\74\65"
  "\2e\47\4e\55\2d\73\74\61\63\6b\00\2e\72\65\6c\61"
  "\2e\64\65\62\75\67\5f\6c\69\6e\65\00\2e\72\65\6c"
  "\61\2e\65\68\5f\66\72\61\6d\65\00\62\69\61\73\65"
  "\64\4d\6f\64\75\6c\65\49\6e\73\74\61\6e\63\65\49"
  "\64\00\2e\73\74\72\74\61\62\00\2e\73\79\6d\74\61"
  "\62\00\2e\72\6f\64\61\74\61\2e\63\73\74\31\36\00"
  "\66\75\6e\63\74\69\6f\6e\44\65\66\4d\75\74\61\62"
  "\6c\65\44\61\74\61\73\30\00\66\75\6e\63\74\69\6f"
  "\6e\44\65\66\30\00\74\79\70\65\49\64\30\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\9e\00\00\00\03\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\a0\04\00\00"
  "\00\00\00\00\ea\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\14\00\00\00\01\00\00\00\06\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\40\00\00\00"
  "\00\00\00\00\37\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\0f\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\08\03\00\00"
  "\00\00\00\00\60\00\00\00\00\00\00\00\11\00\00\00"
  "\02\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\ae\00\00\00\01\00\00\00\12\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\80\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\10\00\00\00"
  "\00\00\00\00\2c\00\00\00\01\00\00\00\30\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\90\00\00\00"
  "\00\00\00\00\1a\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\01\00\00\00"
  "\00\00\00\00\01\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\aa\00\00\00"
  "\00\00\00\00\2b\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\4b\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\d5\00\00\00"
  "\00\00\00\00\42\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\46\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\68\03\00\00"
  "\00\00\00\00\d8\00\00\00\00\00\00\00\11\00\00\00"
  "\07\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\37\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\17\01\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\1f\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\18\01\00\00"
  "\00\00\00\00\50\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\04\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\1a\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\40\04\00\00"
  "\00\00\00\00\30\00\00\00\00\00\00\00\11\00\00\00"
  "\0a\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\57\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\68\01\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\7d\00\00\00\01\00\00\70\02\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\68\01\00\00"
  "\00\00\00\00\40\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\08\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\78\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\70\04\00\00"
  "\00\00\00\00\18\00\00\00\00\00\00\00\11\00\00\00"
  "\0d\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\6c\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\a8\01\00\00"
  "\00\00\00\00\3d\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\67\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\88\04\00\00"
  "\00\00\00\00\18\00\00\00\00\00\00\00\11\00\00\00"
  "\0f\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\a6\00\00\00\02\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\e8\01\00\00"
  "\00\00\00\00\20\01\00\00\00\00\00\00\01\00\00\00"
  "\08\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00"
)
(assert_return (invoke "4294967249") (v128.const i32x4 4294967249 4294967249 4294967249 4294967249))


;; Test parsing a float from binary

(module binary
  ;; (func (export "4294967249") (result v128) (v128.const f32x4 4294967249 4294967249 4294967249 4294967249))

  "\00\61\73\6d\01\00\00\00\01\05\01\60\00\01\7b\03"
  "\02\01\00\07\0e\01\0a\34\32\39\34\39\36\37\32\34"
  "\39\00\00\0a\16\01\14\00\fd\02\00\00\80\4f\00\00"
  "\80\4f\00\00\80\4f\00\00\80\4f\0b\00\2c\04\6e\61"
  "\6d\65\00\01\00\01\03\01\00\00\02\03\01\00\00\03"
  "\03\01\00\00\04\01\00\05\01\00\06\01\00\07\01\00"
  "\08\01\00\09\01\00\0a\01\00\00\a8\14\17\77\61\76"
  "\6d\2e\70\72\65\63\6f\6d\70\69\6c\65\64\5f\6f\62"
  "\6a\65\63\74\7f\45\4c\46\02\01\01\00\00\00\00\00"
  "\00\00\00\00\01\00\3e\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\90\05\00\00"
  "\00\00\00\00\00\00\00\00\40\00\00\00\00\00\40\00"
  "\12\00\01\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\55\48\89\e5\48\89\f8\48\b9\00\00\00"
  "\00\00\00\00\00\c5\f8\28\01\5d\c3\00\00\00\00\00"
  "\00\00\00\00\00\00\80\4f\00\00\80\4f\00\00\80\4f"
  "\00\00\80\4f\57\41\56\4d\00\75\6e\6b\6e\6f\77\6e"
  "\00\66\75\6e\63\74\69\6f\6e\44\65\66\30\00\01\11"
  "\01\25\0e\13\05\03\0e\10\17\1b\0e\e1\7f\19\11\01"
  "\12\06\00\00\02\2e\00\11\01\12\06\40\18\6e\0e\03"
  "\0e\3f\19\e1\7f\19\00\00\00\3e\00\00\00\04\00\00"
  "\00\00\00\08\01\00\00\00\00\ff\ff\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\17"
  "\00\00\00\02\00\00\00\00\00\00\00\00\17\00\00\00"
  "\01\56\00\00\00\00\00\00\00\00\00\00\4c\00\00\00"
  "\05\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\01\00\00\00\01\00\00\00\07\00\00\00\08\00\00\00"
  "\4c\4c\56\4d\30\37\30\30\00\00\00\00\01\00\00\00"
  "\aa\f6\05\41\00\00\00\00\00\00\00\00\2e\2e\03\13"
  "\00\00\00\2e\2a\00\00\00\00\00\00\00\14\00\00\00"
  "\00\00\00\00\01\7a\52\00\01\78\10\01\1c\0c\07\08"
  "\90\01\00\00\24\00\00\00\1c\00\00\00\00\00\00\00"
  "\00\00\00\00\17\00\00\00\00\00\00\00\00\41\0e\10"
  "\86\02\43\0d\06\00\00\00\00\00\00\00\39\00\00\00"
  "\04\00\1f\00\00\00\01\01\01\fb\0e\0d\00\01\01\01"
  "\01\00\00\00\01\00\00\01\00\75\6e\6b\6e\6f\77\6e"
  "\00\00\00\00\00\00\09\02\00\00\00\00\00\00\00\00"
  "\11\0a\08\13\02\06\00\01\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\e9\00\00\00\04\00\f1\ff\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\03\00\02\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\03\00\04\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\03\00\05\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\03\00\06\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\03\00\07\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\03\00\0f\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\87\00\00\00"
  "\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\d5\00\00\00\12\00\02\00\20\00\00\00"
  "\00\00\00\00\17\00\00\00\00\00\00\00\bc\00\00\00"
  "\10\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\e2\00\00\00\10\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\08\00\00\00"
  "\00\00\00\00\01\00\00\00\0a\00\00\00\00\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\01\00\00\00"
  "\08\00\00\00\ff\ff\ff\ff\ff\ff\ff\ff\18\00\00\00"
  "\00\00\00\00\01\00\00\00\0b\00\00\00\00\00\00\00"
  "\00\00\00\00\29\00\00\00\00\00\00\00\01\00\00\00"
  "\03\00\00\00\00\00\00\00\00\00\00\00\06\00\00\00"
  "\00\00\00\00\0a\00\00\00\05\00\00\00\00\00\00\00"
  "\00\00\00\00\0c\00\00\00\00\00\00\00\0a\00\00\00"
  "\04\00\00\00\00\00\00\00\00\00\00\00\12\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\05\00\00\00"
  "\00\00\00\00\16\00\00\00\00\00\00\00\0a\00\00\00"
  "\07\00\00\00\00\00\00\00\00\00\00\00\1a\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\05\00\00\00"
  "\00\00\00\00\1e\00\00\00\00\00\00\00\01\00\00\00"
  "\02\00\00\00\20\00\00\00\00\00\00\00\2b\00\00\00"
  "\00\00\00\00\01\00\00\00\02\00\00\00\20\00\00\00"
  "\00\00\00\00\39\00\00\00\00\00\00\00\0a\00\00\00"
  "\04\00\00\00\0d\00\00\00\00\00\00\00\3d\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\0d\00\00\00"
  "\00\00\00\00\2c\00\00\00\00\00\00\00\0a\00\00\00"
  "\06\00\00\00\00\00\00\00\00\00\00\00\38\00\00\00"
  "\00\00\00\00\0a\00\00\00\04\00\00\00\0d\00\00\00"
  "\00\00\00\00\20\00\00\00\00\00\00\00\18\00\00\00"
  "\02\00\00\00\20\00\00\00\00\00\00\00\2c\00\00\00"
  "\00\00\00\00\01\00\00\00\02\00\00\00\20\00\00\00"
  "\00\00\00\00\00\2e\64\65\62\75\67\5f\61\62\62\72"
  "\65\76\00\2e\72\65\6c\61\2e\74\65\78\74\00\2e\72"
  "\65\6c\61\2e\64\65\62\75\67\5f\6e\61\6d\65\73\00"
  "\2e\64\65\62\75\67\5f\73\74\72\00\2e\64\65\62\75"
  "\67\5f\6d\61\63\69\6e\66\6f\00\2e\72\65\6c\61\2e"
  "\64\65\62\75\67\5f\69\6e\66\6f\00\2e\6e\6f\74\65"
  "\2e\47\4e\55\2d\73\74\61\63\6b\00\2e\72\65\6c\61"
  "\2e\64\65\62\75\67\5f\6c\69\6e\65\00\2e\72\65\6c"
  "\61\2e\65\68\5f\66\72\61\6d\65\00\62\69\61\73\65"
  "\64\4d\6f\64\75\6c\65\49\6e\73\74\61\6e\63\65\49"
  "\64\00\2e\73\74\72\74\61\62\00\2e\73\79\6d\74\61"
  "\62\00\2e\72\6f\64\61\74\61\2e\63\73\74\31\36\00"
  "\66\75\6e\63\74\69\6f\6e\44\65\66\4d\75\74\61\62"
  "\6c\65\44\61\74\61\73\30\00\66\75\6e\63\74\69\6f"
  "\6e\44\65\66\30\00\74\79\70\65\49\64\30\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\9e\00\00\00\03\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\a0\04\00\00"
  "\00\00\00\00\ea\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\14\00\00\00\01\00\00\00\06\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\40\00\00\00"
  "\00\00\00\00\37\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\0f\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\08\03\00\00"
  "\00\00\00\00\60\00\00\00\00\00\00\00\11\00\00\00"
  "\02\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\ae\00\00\00\01\00\00\00\12\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\80\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\10\00\00\00\00\00\00\00\10\00\00\00"
  "\00\00\00\00\2c\00\00\00\01\00\00\00\30\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\90\00\00\00"
  "\00\00\00\00\1a\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\01\00\00\00"
  "\00\00\00\00\01\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\aa\00\00\00"
  "\00\00\00\00\2b\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\4b\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\d5\00\00\00"
  "\00\00\00\00\42\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\46\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\68\03\00\00"
  "\00\00\00\00\d8\00\00\00\00\00\00\00\11\00\00\00"
  "\07\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\37\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\17\01\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\1f\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\18\01\00\00"
  "\00\00\00\00\50\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\04\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\1a\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\40\04\00\00"
  "\00\00\00\00\30\00\00\00\00\00\00\00\11\00\00\00"
  "\0a\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\57\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\68\01\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\7d\00\00\00\01\00\00\70\02\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\68\01\00\00"
  "\00\00\00\00\40\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\08\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\78\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\70\04\00\00"
  "\00\00\00\00\18\00\00\00\00\00\00\00\11\00\00\00"
  "\0d\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\6c\00\00\00\01\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\a8\01\00\00"
  "\00\00\00\00\3d\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\01\00\00\00\00\00\00\00\00\00\00\00"
  "\00\00\00\00\67\00\00\00\04\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\88\04\00\00"
  "\00\00\00\00\18\00\00\00\00\00\00\00\11\00\00\00"
  "\0f\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00\a6\00\00\00\02\00\00\00\00\00\00\00"
  "\00\00\00\00\00\00\00\00\00\00\00\00\e8\01\00\00"
  "\00\00\00\00\20\01\00\00\00\00\00\00\01\00\00\00"
  "\08\00\00\00\08\00\00\00\00\00\00\00\18\00\00\00"
  "\00\00\00\00"
)
(assert_return (invoke "4294967249") (v128.const f32x4 4294967249 4294967249 4294967249 4294967249))
