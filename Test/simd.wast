;; v128 globals

(module $M
  (global (export "a") v128       (v128.const f32 0 1 2 3))
  (global (export "b") (mut v128) (v128.const f32 4 5 6 7))
)
(register "M" $M)

(module
  (global $a (import "M" "a") v128)
  (global $b (import "M" "b") (mut v128))
  
  (global $c v128       (get_global $a))
  (global $d v128       (v128.const i32 8 9 10 11))
  (global $e (mut v128) (get_global $a))
  (global $f (mut v128) (v128.const i32 12 13 14 15))

  (func (export "get-a") (result v128) (get_global $a))
  (func (export "get-b") (result v128) (get_global $b))
  (func (export "get-c") (result v128) (get_global $c))
  (func (export "get-d") (result v128) (get_global $d))
  (func (export "get-e") (result v128) (get_global $e))
  (func (export "get-f") (result v128) (get_global $f))

  (func (export "set-b") (param $value v128) (set_global $b (get_local $value)))
  (func (export "set-e") (param $value v128) (set_global $e (get_local $value)))
  (func (export "set-f") (param $value v128) (set_global $f (get_local $value)))
)

(assert_return (invoke "get-a") (v128.const f32 0 1 2 3))
(assert_return (invoke "get-b") (v128.const f32 4 5 6 7))
(assert_return (invoke "get-c") (v128.const f32 0 1 2 3))
(assert_return (invoke "get-d") (v128.const i32 8 9 10 11))
(assert_return (invoke "get-e") (v128.const f32 0 1 2 3))
(assert_return (invoke "get-f") (v128.const i32 12 13 14 15))

(invoke "set-b" (v128.const f64 nan:0x1 nan:0x2))
(assert_return (invoke "get-b") (v128.const f64 nan:0x1 nan:0x2))

(invoke "set-e" (v128.const f64 -nan:0x3 +inf))
(assert_return (invoke "get-e") (v128.const f64 -nan:0x3 +inf))

(invoke "set-f" (v128.const f32 -inf +3.14 10.0e30 +nan:0x42))
(assert_return (invoke "get-f") (v128.const f32 -inf +3.14 10.0e30 +nan:0x42))

(assert_invalid (module (global v128 (i32.const 0))) "invalid initializer expression")
(assert_invalid (module (global v128 (i64.const 0))) "invalid initializer expression")
(assert_invalid (module (global v128 (f32.const 0))) "invalid initializer expression")
(assert_invalid (module (global v128 (f64.const 0))) "invalid initializer expression")
(assert_invalid (module (global $i32 i32 (i32.const 0)) (global v128 (get_global $i32))) "invalid initializer expression")

(module binary
  "\00asm"
  "\01\00\00\00"       ;; 1 section
  "\06"                ;; global section
  "\16"                ;; 22 bytes
  "\01"                ;; 1 global
  "\7b"                ;; v128
  "\00"                ;; immutable
  "\fd\00"             ;; v128.const
  "\00\01\02\03"       ;; literal bytes 0-3
  "\04\05\06\07"       ;; literal bytes 4-7
  "\08\09\0a\0b"       ;; literal bytes 8-11
  "\0c\0d\0e\0f"       ;; literal bytes 12-15
  "\0b"                ;; end
)

(assert_invalid
  (module binary
    "\00asm"
    "\01\00\00\00"       ;; 1 section
    "\06\86\80\80\80\00" ;; global section
    "\01"                ;; 1 global
    "\7b"                ;; v128
    "\00"                ;; immutable
    "\fd\01"             ;; v128.load
    "\0b"                ;; end
  )
  "invalid initializer expression"
)

;; TODO: v128 parameters

;; TODO: v128 locals

;; TODO: v128 results

;; v128.const

(module
  (func (export "v128.const/i8x16") (result v128) (v128.const i8 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
  (func (export "v128.const/i16x8") (result v128) (v128.const i16 16 17 18 19 20 21 22 23))
  (func (export "v128.const/i32x4") (result v128) (v128.const i32 24 25 26 27))
  (func (export "v128.const/i64x2") (result v128) (v128.const i64 28 29))
  (func (export "v128.const/f32x4") (result v128) (v128.const f32 30.5 31.5 32.5 33.5))
  (func (export "v128.const/f64x2") (result v128) (v128.const f64 34.5 35.5))
)

;; v128.load/store

(module
  (memory 1)
  (func (export "v128.load")  (param $address i32)                     (result v128) (v128.load  align=16 (get_local $address)))
  (func (export "v128.store") (param $address i32) (param $value v128)               (v128.store align=16 (get_local $address) (get_local $value)))
)

(assert_invalid (module (memory 1) (func (drop (v128.load  align=32 (i32.const 0))))) "invalid alignment")
(assert_invalid (module (memory 1) (func (drop (v128.store align=32 (i32.const 0))))) "invalid alignment")

;; *.splat

(module
  (func (export "i8x16.splat") (param $a i32) (result v128) (i8x16.splat (get_local $a)))
  (func (export "i16x8.splat") (param $a i32) (result v128) (i16x8.splat (get_local $a)))
  (func (export "i32x4.splat") (param $a i32) (result v128) (i32x4.splat (get_local $a)))
  (func (export "i64x2.splat") (param $a i64) (result v128) (i64x2.splat (get_local $a)))
  (func (export "f32x4.splat") (param $a f32) (result v128) (f32x4.splat (get_local $a)))
  (func (export "f64x2.splat") (param $a f64) (result v128) (f64x2.splat (get_local $a)))
)

;; *.extract_lane*

(module
  (func (export "i8x16.extract_lane_s/0")  (param $a v128) (result i32) (i8x16.extract_lane_s 0  (get_local $a)))
  (func (export "i8x16.extract_lane_s/1")  (param $a v128) (result i32) (i8x16.extract_lane_s 1  (get_local $a)))
  (func (export "i8x16.extract_lane_s/2")  (param $a v128) (result i32) (i8x16.extract_lane_s 2  (get_local $a)))
  (func (export "i8x16.extract_lane_s/3")  (param $a v128) (result i32) (i8x16.extract_lane_s 3  (get_local $a)))
  (func (export "i8x16.extract_lane_s/4")  (param $a v128) (result i32) (i8x16.extract_lane_s 4  (get_local $a)))
  (func (export "i8x16.extract_lane_s/5")  (param $a v128) (result i32) (i8x16.extract_lane_s 5  (get_local $a)))
  (func (export "i8x16.extract_lane_s/6")  (param $a v128) (result i32) (i8x16.extract_lane_s 6  (get_local $a)))
  (func (export "i8x16.extract_lane_s/7")  (param $a v128) (result i32) (i8x16.extract_lane_s 7  (get_local $a)))
  (func (export "i8x16.extract_lane_s/8")  (param $a v128) (result i32) (i8x16.extract_lane_s 8  (get_local $a)))
  (func (export "i8x16.extract_lane_s/9")  (param $a v128) (result i32) (i8x16.extract_lane_s 9  (get_local $a)))
  (func (export "i8x16.extract_lane_s/10") (param $a v128) (result i32) (i8x16.extract_lane_s 10 (get_local $a)))
  (func (export "i8x16.extract_lane_s/11") (param $a v128) (result i32) (i8x16.extract_lane_s 11 (get_local $a)))
  (func (export "i8x16.extract_lane_s/12") (param $a v128) (result i32) (i8x16.extract_lane_s 12 (get_local $a)))
  (func (export "i8x16.extract_lane_s/13") (param $a v128) (result i32) (i8x16.extract_lane_s 13 (get_local $a)))
  (func (export "i8x16.extract_lane_s/14") (param $a v128) (result i32) (i8x16.extract_lane_s 14 (get_local $a)))
  (func (export "i8x16.extract_lane_s/15") (param $a v128) (result i32) (i8x16.extract_lane_s 15 (get_local $a)))

  (func (export "i8x16.extract_lane_u/0")  (param $a v128) (result i32) (i8x16.extract_lane_u 0  (get_local $a)))
  (func (export "i8x16.extract_lane_u/1")  (param $a v128) (result i32) (i8x16.extract_lane_u 1  (get_local $a)))
  (func (export "i8x16.extract_lane_u/2")  (param $a v128) (result i32) (i8x16.extract_lane_u 2  (get_local $a)))
  (func (export "i8x16.extract_lane_u/3")  (param $a v128) (result i32) (i8x16.extract_lane_u 3  (get_local $a)))
  (func (export "i8x16.extract_lane_u/4")  (param $a v128) (result i32) (i8x16.extract_lane_u 4  (get_local $a)))
  (func (export "i8x16.extract_lane_u/5")  (param $a v128) (result i32) (i8x16.extract_lane_u 5  (get_local $a)))
  (func (export "i8x16.extract_lane_u/6")  (param $a v128) (result i32) (i8x16.extract_lane_u 6  (get_local $a)))
  (func (export "i8x16.extract_lane_u/7")  (param $a v128) (result i32) (i8x16.extract_lane_u 7  (get_local $a)))
  (func (export "i8x16.extract_lane_u/8")  (param $a v128) (result i32) (i8x16.extract_lane_u 8  (get_local $a)))
  (func (export "i8x16.extract_lane_u/9")  (param $a v128) (result i32) (i8x16.extract_lane_u 9  (get_local $a)))
  (func (export "i8x16.extract_lane_u/10") (param $a v128) (result i32) (i8x16.extract_lane_u 10 (get_local $a)))
  (func (export "i8x16.extract_lane_u/11") (param $a v128) (result i32) (i8x16.extract_lane_u 11 (get_local $a)))
  (func (export "i8x16.extract_lane_u/12") (param $a v128) (result i32) (i8x16.extract_lane_u 12 (get_local $a)))
  (func (export "i8x16.extract_lane_u/13") (param $a v128) (result i32) (i8x16.extract_lane_u 13 (get_local $a)))
  (func (export "i8x16.extract_lane_u/14") (param $a v128) (result i32) (i8x16.extract_lane_u 14 (get_local $a)))
  (func (export "i8x16.extract_lane_u/15") (param $a v128) (result i32) (i8x16.extract_lane_u 15 (get_local $a)))

  (func (export "i16x8.extract_lane_s/0")  (param $a v128) (result i32) (i16x8.extract_lane_s 0  (get_local $a)))
  (func (export "i16x8.extract_lane_s/1")  (param $a v128) (result i32) (i16x8.extract_lane_s 1  (get_local $a)))
  (func (export "i16x8.extract_lane_s/2")  (param $a v128) (result i32) (i16x8.extract_lane_s 2  (get_local $a)))
  (func (export "i16x8.extract_lane_s/3")  (param $a v128) (result i32) (i16x8.extract_lane_s 3  (get_local $a)))
  (func (export "i16x8.extract_lane_s/4")  (param $a v128) (result i32) (i16x8.extract_lane_s 4  (get_local $a)))
  (func (export "i16x8.extract_lane_s/5")  (param $a v128) (result i32) (i16x8.extract_lane_s 5  (get_local $a)))
  (func (export "i16x8.extract_lane_s/6")  (param $a v128) (result i32) (i16x8.extract_lane_s 6  (get_local $a)))
  (func (export "i16x8.extract_lane_s/7")  (param $a v128) (result i32) (i16x8.extract_lane_s 7  (get_local $a)))

  (func (export "i16x8.extract_lane_u/0")  (param $a v128) (result i32) (i16x8.extract_lane_u 0  (get_local $a)))
  (func (export "i16x8.extract_lane_u/1")  (param $a v128) (result i32) (i16x8.extract_lane_u 1  (get_local $a)))
  (func (export "i16x8.extract_lane_u/2")  (param $a v128) (result i32) (i16x8.extract_lane_u 2  (get_local $a)))
  (func (export "i16x8.extract_lane_u/3")  (param $a v128) (result i32) (i16x8.extract_lane_u 3  (get_local $a)))
  (func (export "i16x8.extract_lane_u/4")  (param $a v128) (result i32) (i16x8.extract_lane_u 4  (get_local $a)))
  (func (export "i16x8.extract_lane_u/5")  (param $a v128) (result i32) (i16x8.extract_lane_u 5  (get_local $a)))
  (func (export "i16x8.extract_lane_u/6")  (param $a v128) (result i32) (i16x8.extract_lane_u 6  (get_local $a)))
  (func (export "i16x8.extract_lane_u/7")  (param $a v128) (result i32) (i16x8.extract_lane_u 7  (get_local $a)))

  (func (export "i32x4.extract_lane/0")  (param $a v128) (result i32) (i32x4.extract_lane 0  (get_local $a)))
  (func (export "i32x4.extract_lane/1")  (param $a v128) (result i32) (i32x4.extract_lane 1  (get_local $a)))
  (func (export "i32x4.extract_lane/2")  (param $a v128) (result i32) (i32x4.extract_lane 2  (get_local $a)))
  (func (export "i32x4.extract_lane/3")  (param $a v128) (result i32) (i32x4.extract_lane 3  (get_local $a)))

  (func (export "i64x2.extract_lane/0")  (param $a v128) (result i64) (i64x2.extract_lane 0  (get_local $a)))
  (func (export "i64x2.extract_lane/1")  (param $a v128) (result i64) (i64x2.extract_lane 1  (get_local $a)))

  (func (export "f32x4.extract_lane/0")  (param $a v128) (result f32) (f32x4.extract_lane 0  (get_local $a)))
  (func (export "f32x4.extract_lane/1")  (param $a v128) (result f32) (f32x4.extract_lane 1  (get_local $a)))
  (func (export "f32x4.extract_lane/2")  (param $a v128) (result f32) (f32x4.extract_lane 2  (get_local $a)))
  (func (export "f32x4.extract_lane/3")  (param $a v128) (result f32) (f32x4.extract_lane 3  (get_local $a)))

  (func (export "f64x2.extract_lane/0")  (param $a v128) (result f64) (f64x2.extract_lane 0  (get_local $a)))
  (func (export "f64x2.extract_lane/1")  (param $a v128) (result f64) (f64x2.extract_lane 1  (get_local $a)))
)

;; *.replace_lane

(module
  (func (export "i8x16.replace_lane/0")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 0  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/1")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 1  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/2")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 2  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/3")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 3  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/4")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 4  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/5")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 5  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/6")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 6  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/7")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 7  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/8")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 8  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/9")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 9  (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/10") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 10 (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/11") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 11 (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/12") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 12 (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/13") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 13 (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/14") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 14 (get_local $a) (get_local $b)))
  (func (export "i8x16.replace_lane/15") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 15 (get_local $a) (get_local $b)))

  (func (export "i16x8.replace_lane/0")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 0  (get_local $a) (get_local $b)))
  (func (export "i16x8.replace_lane/1")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 1  (get_local $a) (get_local $b)))
  (func (export "i16x8.replace_lane/2")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 2  (get_local $a) (get_local $b)))
  (func (export "i16x8.replace_lane/3")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 3  (get_local $a) (get_local $b)))
  (func (export "i16x8.replace_lane/4")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 4  (get_local $a) (get_local $b)))
  (func (export "i16x8.replace_lane/5")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 5  (get_local $a) (get_local $b)))
  (func (export "i16x8.replace_lane/6")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 6  (get_local $a) (get_local $b)))
  (func (export "i16x8.replace_lane/7")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 7  (get_local $a) (get_local $b)))

  (func (export "i32x4.replace_lane/0")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 0  (get_local $a) (get_local $b)))
  (func (export "i32x4.replace_lane/1")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 1  (get_local $a) (get_local $b)))
  (func (export "i32x4.replace_lane/2")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 2  (get_local $a) (get_local $b)))
  (func (export "i32x4.replace_lane/3")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 3  (get_local $a) (get_local $b)))

  (func (export "i64x2.replace_lane/0")  (param $a v128) (param $b i64) (result v128) (i64x2.replace_lane 0  (get_local $a) (get_local $b)))
  (func (export "i64x2.replace_lane/1")  (param $a v128) (param $b i64) (result v128) (i64x2.replace_lane 1  (get_local $a) (get_local $b)))

  (func (export "f32x4.replace_lane/0")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 0  (get_local $a) (get_local $b)))
  (func (export "f32x4.replace_lane/1")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 1  (get_local $a) (get_local $b)))
  (func (export "f32x4.replace_lane/2")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 2  (get_local $a) (get_local $b)))
  (func (export "f32x4.replace_lane/3")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 3  (get_local $a) (get_local $b)))

  (func (export "f64x2.replace_lane/0")  (param $a v128) (param $b f64) (result v128) (f64x2.replace_lane 0  (get_local $a) (get_local $b)))
  (func (export "f64x2.replace_lane/1")  (param $a v128) (param $b f64) (result v128) (f64x2.replace_lane 1  (get_local $a) (get_local $b)))
)

;; v8x16.shuffle

(module
  (func (export "v8x16.shuffle/0123456789abcdef") (param $a v128) (param $b v128) (result v128) (v8x16.shuffle ( 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15) (get_local $a) (get_local $b)))
  (func (export "v8x16.shuffle/ghijklmnopqrstuv") (param $a v128) (param $b v128) (result v128) (v8x16.shuffle (16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31) (get_local $a) (get_local $b)))
  (func (export "v8x16.shuffle/vutsrqponmlkjihg") (param $a v128) (param $b v128) (result v128) (v8x16.shuffle (31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16) (get_local $a) (get_local $b)))
  (func (export "v8x16.shuffle/fedcba9876543210") (param $a v128) (param $b v128) (result v128) (v8x16.shuffle (15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0) (get_local $a) (get_local $b)))
  (func (export "v8x16.shuffle/0000000000000000") (param $a v128) (param $b v128) (result v128) (v8x16.shuffle ( 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0) (get_local $a) (get_local $b)))
  (func (export "v8x16.shuffle/gggggggggggggggg") (param $a v128) (param $b v128) (result v128) (v8x16.shuffle (16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16) (get_local $a) (get_local $b)))
  (func (export "v8x16.shuffle/00000000gggggggg") (param $a v128) (param $b v128) (result v128) (v8x16.shuffle ( 0  0  0  0  0  0  0  0 16 16 16 16 16 16 16 16) (get_local $a) (get_local $b)))
)

;; i*.add

(module
  (func (export "i8x16.add") (param $a v128) (param $b v128) (result v128) (i8x16.add (get_local $a) (get_local $b)))
  (func (export "i16x8.add") (param $a v128) (param $b v128) (result v128) (i16x8.add (get_local $a) (get_local $b)))
  (func (export "i32x4.add") (param $a v128) (param $b v128) (result v128) (i32x4.add (get_local $a) (get_local $b)))
  (func (export "i64x2.add") (param $a v128) (param $b v128) (result v128) (i64x2.add (get_local $a) (get_local $b)))
)

;; i*.sub

(module
  (func (export "i8x16.sub") (param $a v128) (param $b v128) (result v128) (i8x16.sub (get_local $a) (get_local $b)))
  (func (export "i16x8.sub") (param $a v128) (param $b v128) (result v128) (i16x8.sub (get_local $a) (get_local $b)))
  (func (export "i32x4.sub") (param $a v128) (param $b v128) (result v128) (i32x4.sub (get_local $a) (get_local $b)))
  (func (export "i64x2.sub") (param $a v128) (param $b v128) (result v128) (i64x2.sub (get_local $a) (get_local $b)))
)

;; i*.mul

(module
  (func (export "i8x16.mul") (param $a v128) (param $b v128) (result v128) (i8x16.mul (get_local $a) (get_local $b)))
  (func (export "i16x8.mul") (param $a v128) (param $b v128) (result v128) (i16x8.mul (get_local $a) (get_local $b)))
  (func (export "i32x4.mul") (param $a v128) (param $b v128) (result v128) (i32x4.mul (get_local $a) (get_local $b)))
)

;; i*.neg

(module
  (func (export "i8x16.neg") (param $a v128) (result v128) (i8x16.neg (get_local $a)))
  (func (export "i16x8.neg") (param $a v128) (result v128) (i16x8.neg (get_local $a)))
  (func (export "i32x4.neg") (param $a v128) (result v128) (i32x4.neg (get_local $a)))
  (func (export "i64x2.neg") (param $a v128) (result v128) (i64x2.neg (get_local $a)))
)

;; i*.add_saturate*

(module
  (func (export "i8x16.add_saturate_s") (param $a v128) (param $b v128) (result v128) (i8x16.add_saturate_s (get_local $a) (get_local $b)))
  (func (export "i8x16.add_saturate_u") (param $a v128) (param $b v128) (result v128) (i8x16.add_saturate_u (get_local $a) (get_local $b)))
  (func (export "i16x8.add_saturate_s") (param $a v128) (param $b v128) (result v128) (i16x8.add_saturate_s (get_local $a) (get_local $b)))
  (func (export "i16x8.add_saturate_u") (param $a v128) (param $b v128) (result v128) (i16x8.add_saturate_u (get_local $a) (get_local $b)))
)

(assert_return
  (invoke "i8x16.add_saturate_s"
    (v128.const i8 127 126 125 124 123 122 121 120 119 120 121 122 123 124 125 126)
    (v128.const i8 -7 -6 -5 -4 -3 -2 -1 0 +1 +2 +3 +4 +5 +6 +7 +8))
  (v128.const i8 120 120 120 120 120 120 120 120 120 122 124 126 127 127 127 127))

;; i*.sub_saturate*

(module
  (func (export "i8x16.sub_saturate_s") (param $a v128) (param $b v128) (result v128) (i8x16.sub_saturate_s (get_local $a) (get_local $b)))
  (func (export "i8x16.sub_saturate_u") (param $a v128) (param $b v128) (result v128) (i8x16.sub_saturate_u (get_local $a) (get_local $b)))
  (func (export "i16x8.sub_saturate_s") (param $a v128) (param $b v128) (result v128) (i16x8.sub_saturate_s (get_local $a) (get_local $b)))
  (func (export "i16x8.sub_saturate_u") (param $a v128) (param $b v128) (result v128) (i16x8.sub_saturate_u (get_local $a) (get_local $b)))
)

;; i*.shl/shr_s/shr_u

(module
  (func (export "i8x16.shl")   (param $a v128) (param $b v128) (result v128) (i8x16.shl   (get_local $a) (get_local $b)))
  (func (export "i16x8.shl")   (param $a v128) (param $b v128) (result v128) (i16x8.shl   (get_local $a) (get_local $b)))
  (func (export "i32x4.shl")   (param $a v128) (param $b v128) (result v128) (i32x4.shl   (get_local $a) (get_local $b)))
  (func (export "i64x2.shl")   (param $a v128) (param $b v128) (result v128) (i64x2.shl   (get_local $a) (get_local $b)))
  (func (export "i8x16.shr_s") (param $a v128) (param $b v128) (result v128) (i8x16.shr_s (get_local $a) (get_local $b)))
  (func (export "i8x16.shr_u") (param $a v128) (param $b v128) (result v128) (i8x16.shr_u (get_local $a) (get_local $b)))
  (func (export "i16x8.shr_s") (param $a v128) (param $b v128) (result v128) (i16x8.shr_s (get_local $a) (get_local $b)))
  (func (export "i16x8.shr_u") (param $a v128) (param $b v128) (result v128) (i16x8.shr_u (get_local $a) (get_local $b)))
  (func (export "i32x4.shr_s") (param $a v128) (param $b v128) (result v128) (i32x4.shr_s (get_local $a) (get_local $b)))
  (func (export "i32x4.shr_u") (param $a v128) (param $b v128) (result v128) (i32x4.shr_u (get_local $a) (get_local $b)))
  (func (export "i64x2.shr_s") (param $a v128) (param $b v128) (result v128) (i64x2.shr_s (get_local $a) (get_local $b)))
  (func (export "i64x2.shr_u") (param $a v128) (param $b v128) (result v128) (i64x2.shr_u (get_local $a) (get_local $b)))
)

;; v128.and/or/xor/not

(module
  (func (export "v128.and") (param $a v128) (param $b v128) (result v128) (v128.and (get_local $a) (get_local $b)))
  (func (export "v128.or")  (param $a v128) (param $b v128) (result v128) (v128.or  (get_local $a) (get_local $b)))
  (func (export "v128.xor") (param $a v128) (param $b v128) (result v128) (v128.xor (get_local $a) (get_local $b)))
  (func (export "v128.not") (param $a v128)                 (result v128) (v128.not (get_local $a)               ))
)

(module (func (export "v128.bitselect") (param $a v128) (param $b v128) (param $c v128) (result v128) (v128.bitselect (get_local $a) (get_local $b) (get_local $c))))

(module
  (func (export "i8x16.any_true") (param $a v128) (result i32) (i8x16.any_true (get_local $a)))
  (func (export "i16x8.any_true") (param $a v128) (result i32) (i16x8.any_true (get_local $a)))
  (func (export "i32x4.any_true") (param $a v128) (result i32) (i32x4.any_true (get_local $a)))
  (func (export "i64x2.any_true") (param $a v128) (result i32) (i64x2.any_true (get_local $a)))
)

(module
  (func (export "i8x16.all_true") (param $a v128) (result i32) (i8x16.all_true (get_local $a)))
  (func (export "i16x8.all_true") (param $a v128) (result i32) (i16x8.all_true (get_local $a)))
  (func (export "i32x4.all_true") (param $a v128) (result i32) (i32x4.all_true (get_local $a)))
  (func (export "i64x2.all_true") (param $a v128) (result i32) (i64x2.all_true (get_local $a)))
)

(module
  (func (export "i8x16.eq") (param $a v128) (param $b v128) (result v128) (i8x16.eq (get_local $a) (get_local $b)))
  (func (export "i16x8.eq") (param $a v128) (param $b v128) (result v128) (i16x8.eq (get_local $a) (get_local $b)))
  (func (export "i32x4.eq") (param $a v128) (param $b v128) (result v128) (i32x4.eq (get_local $a) (get_local $b)))
  (func (export "f32x4.eq") (param $a v128) (param $b v128) (result v128) (f32x4.eq (get_local $a) (get_local $b)))
  (func (export "f64x2.eq") (param $a v128) (param $b v128) (result v128) (f64x2.eq (get_local $a) (get_local $b)))
)

(module
  (func (export "i8x16.ne") (param $a v128) (param $b v128) (result v128) (i8x16.ne (get_local $a) (get_local $b)))
  (func (export "i16x8.ne") (param $a v128) (param $b v128) (result v128) (i16x8.ne (get_local $a) (get_local $b)))
  (func (export "i32x4.ne") (param $a v128) (param $b v128) (result v128) (i32x4.ne (get_local $a) (get_local $b)))
  (func (export "f32x4.ne") (param $a v128) (param $b v128) (result v128) (f32x4.ne (get_local $a) (get_local $b)))
  (func (export "f64x2.ne") (param $a v128) (param $b v128) (result v128) (f64x2.ne (get_local $a) (get_local $b)))
)

(module
  (func (export "i8x16.lt_s") (param $a v128) (param $b v128) (result v128) (i8x16.lt_s (get_local $a) (get_local $b)))
  (func (export "i8x16.lt_u") (param $a v128) (param $b v128) (result v128) (i8x16.lt_u (get_local $a) (get_local $b)))
  (func (export "i16x8.lt_s") (param $a v128) (param $b v128) (result v128) (i16x8.lt_s (get_local $a) (get_local $b)))
  (func (export "i16x8.lt_u") (param $a v128) (param $b v128) (result v128) (i16x8.lt_u (get_local $a) (get_local $b)))
  (func (export "i32x4.lt_s") (param $a v128) (param $b v128) (result v128) (i32x4.lt_s (get_local $a) (get_local $b)))
  (func (export "i32x4.lt_u") (param $a v128) (param $b v128) (result v128) (i32x4.lt_u (get_local $a) (get_local $b)))
  (func (export "f32x4.lt")   (param $a v128) (param $b v128) (result v128) (f32x4.lt   (get_local $a) (get_local $b)))
  (func (export "f64x2.lt")   (param $a v128) (param $b v128) (result v128) (f64x2.lt   (get_local $a) (get_local $b)))
)

(module
  (func (export "i8x16.le_s") (param $a v128) (param $b v128) (result v128) (i8x16.le_s (get_local $a) (get_local $b)))
  (func (export "i8x16.le_u") (param $a v128) (param $b v128) (result v128) (i8x16.le_u (get_local $a) (get_local $b)))
  (func (export "i16x8.le_s") (param $a v128) (param $b v128) (result v128) (i16x8.le_s (get_local $a) (get_local $b)))
  (func (export "i16x8.le_u") (param $a v128) (param $b v128) (result v128) (i16x8.le_u (get_local $a) (get_local $b)))
  (func (export "i32x4.le_s") (param $a v128) (param $b v128) (result v128) (i32x4.le_s (get_local $a) (get_local $b)))
  (func (export "i32x4.le_u") (param $a v128) (param $b v128) (result v128) (i32x4.le_u (get_local $a) (get_local $b)))
  (func (export "f32x4.le")   (param $a v128) (param $b v128) (result v128) (f32x4.le   (get_local $a) (get_local $b)))
  (func (export "f64x2.le")   (param $a v128) (param $b v128) (result v128) (f64x2.le   (get_local $a) (get_local $b)))
)

(module
  (func (export "i8x16.gt_s") (param $a v128) (param $b v128) (result v128) (i8x16.gt_s (get_local $a) (get_local $b)))
  (func (export "i8x16.gt_u") (param $a v128) (param $b v128) (result v128) (i8x16.gt_u (get_local $a) (get_local $b)))
  (func (export "i16x8.gt_s") (param $a v128) (param $b v128) (result v128) (i16x8.gt_s (get_local $a) (get_local $b)))
  (func (export "i16x8.gt_u") (param $a v128) (param $b v128) (result v128) (i16x8.gt_u (get_local $a) (get_local $b)))
  (func (export "i32x4.gt_s") (param $a v128) (param $b v128) (result v128) (i32x4.gt_s (get_local $a) (get_local $b)))
  (func (export "i32x4.gt_u") (param $a v128) (param $b v128) (result v128) (i32x4.gt_u (get_local $a) (get_local $b)))
  (func (export "f32x4.gt")   (param $a v128) (param $b v128) (result v128) (f32x4.gt   (get_local $a) (get_local $b)))
  (func (export "f64x2.gt")   (param $a v128) (param $b v128) (result v128) (f64x2.gt   (get_local $a) (get_local $b)))
)

(module
  (func (export "i8x16.ge_s") (param $a v128) (param $b v128) (result v128) (i8x16.ge_s (get_local $a) (get_local $b)))
  (func (export "i8x16.ge_u") (param $a v128) (param $b v128) (result v128) (i8x16.ge_u (get_local $a) (get_local $b)))
  (func (export "i16x8.ge_s") (param $a v128) (param $b v128) (result v128) (i16x8.ge_s (get_local $a) (get_local $b)))
  (func (export "i16x8.ge_u") (param $a v128) (param $b v128) (result v128) (i16x8.ge_u (get_local $a) (get_local $b)))
  (func (export "i32x4.ge_s") (param $a v128) (param $b v128) (result v128) (i32x4.ge_s (get_local $a) (get_local $b)))
  (func (export "i32x4.ge_u") (param $a v128) (param $b v128) (result v128) (i32x4.ge_u (get_local $a) (get_local $b)))
  (func (export "f32x4.ge")   (param $a v128) (param $b v128) (result v128) (f32x4.ge   (get_local $a) (get_local $b)))
  (func (export "f64x2.ge")   (param $a v128) (param $b v128) (result v128) (f64x2.ge   (get_local $a) (get_local $b)))
)

(module
  (func (export "f32x4.min") (param $a v128) (param $b v128) (result v128) (f32x4.min (get_local $a) (get_local $b)))
  (func (export "f64x2.min") (param $a v128) (param $b v128) (result v128) (f64x2.min (get_local $a) (get_local $b)))
  (func (export "f32x4.max") (param $a v128) (param $b v128) (result v128) (f32x4.max (get_local $a) (get_local $b)))
  (func (export "f64x2.max") (param $a v128) (param $b v128) (result v128) (f64x2.max (get_local $a) (get_local $b)))
)

(module
  (func (export "f32x4.add") (param $a v128) (param $b v128) (result v128) (f32x4.add (get_local $a) (get_local $b)))
  (func (export "f64x2.add") (param $a v128) (param $b v128) (result v128) (f64x2.add (get_local $a) (get_local $b)))
  (func (export "f32x4.sub") (param $a v128) (param $b v128) (result v128) (f32x4.sub (get_local $a) (get_local $b)))
  (func (export "f64x2.sub") (param $a v128) (param $b v128) (result v128) (f64x2.sub (get_local $a) (get_local $b)))
  (func (export "f32x4.div") (param $a v128) (param $b v128) (result v128) (f32x4.div (get_local $a) (get_local $b)))
  (func (export "f64x2.div") (param $a v128) (param $b v128) (result v128) (f64x2.div (get_local $a) (get_local $b)))
  (func (export "f32x4.mul") (param $a v128) (param $b v128) (result v128) (f32x4.mul (get_local $a) (get_local $b)))
  (func (export "f64x2.mul") (param $a v128) (param $b v128) (result v128) (f64x2.mul (get_local $a) (get_local $b)))
)

(module
  (func (export "f32x4.neg")  (param $a v128) (result v128) (f32x4.neg  (get_local $a)))
  (func (export "f64x2.neg")  (param $a v128) (result v128) (f64x2.neg  (get_local $a)))
  (func (export "f32x4.abs")  (param $a v128) (result v128) (f32x4.abs  (get_local $a)))
  (func (export "f64x2.abs")  (param $a v128) (result v128) (f64x2.abs  (get_local $a)))
  (func (export "f32x4.sqrt") (param $a v128) (result v128) (f32x4.sqrt (get_local $a)))
  (func (export "f64x2.sqrt") (param $a v128) (result v128) (f64x2.sqrt (get_local $a)))
)

(module
  (func (export "f32x4.convert_s/i32x4") (param $a v128) (result v128) (f32x4.convert_s/i32x4 (get_local $a)))
  (func (export "f32x4.convert_u/i32x4") (param $a v128) (result v128) (f32x4.convert_u/i32x4 (get_local $a)))
  (func (export "f64x2.convert_s/i64x2") (param $a v128) (result v128) (f64x2.convert_s/i64x2 (get_local $a)))
  (func (export "f64x2.convert_u/i64x2") (param $a v128) (result v128) (f64x2.convert_u/i64x2 (get_local $a)))
)

;; i32x4.trunc_s:sat/f32x4

(module (func (export "i32x4.trunc_s:sat/f32x4") (param $a f32) (result v128) (i32x4.trunc_s:sat/f32x4 (f32x4.splat (get_local $a)))))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const 0.0))
  (v128.const i32 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const 1.0))
  (v128.const i32 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const 1.9))
  (v128.const i32 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const 2.0))
  (v128.const i32 2 2 2 2))
  
(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const -1.0))
  (v128.const i32 -1 -1 -1 -1))
  
(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const -1.9))
  (v128.const i32 -1 -1 -1 -1))
  
(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const -2))
  (v128.const i32 -2 -2 -2 -2))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const -2147483648.0))
  (v128.const i32 -2147483648 -2147483648 -2147483648 -2147483648))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const -2147483648.0))
  (v128.const i32 -2147483648 -2147483648 -2147483648 -2147483648))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const -3000000000.0))
  (v128.const i32 -2147483648 -2147483648 -2147483648 -2147483648))
  
(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const -inf))
  (v128.const i32 -2147483648 -2147483648 -2147483648 -2147483648))
  
(; not yet working correctly
(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const +inf))
  (v128.const i32 2147483647 2147483647 2147483647 2147483647))

(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const nan))
  (v128.const i32 0 0 0 0))
;)
  
;; i32x4.trunc_u:sat/f32x4

(module (func (export "i32x4.trunc_u:sat/f32x4") (param $a f32) (result v128) (i32x4.trunc_u:sat/f32x4 (f32x4.splat (get_local $a)))))

(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const 0.0))
  (v128.const i32 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const 1.0))
  (v128.const i32 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const 1.9))
  (v128.const i32 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const 2.0))
  (v128.const i32 2 2 2 2))
  
(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const -1.0))
  (v128.const i32 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const -2.0))
  (v128.const i32 0 0 0 0))
  
(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const -2147483648.0))
  (v128.const i32 0 0 0 0))
  
(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const -inf))
  (v128.const i32 0 0 0 0))
  
(; not yet working correctly
(assert_return
  (invoke "i32x4.trunc_s:sat/f32x4" (f32.const +inf))
  (v128.const i32 0xffffffff 0xffffffff 0xffffffff 0xffffffff))

(assert_return
  (invoke "i32x4.trunc_u:sat/f32x4" (f32.const nan))
  (v128.const i32 0 0 0 0))
;)

;; i64x2.trunc_s:sat/f64x2

(module (func (export "i64x2.trunc_s:sat/f64x2") (param $a f64) (result v128) (i64x2.trunc_s:sat/f64x2 (f64x2.splat (get_local $a)))))

;; i64x2.trunc_u:sat/f64x2

(module (func (export "i64x2.trunc_u:sat/f64x2") (param $a f64) (result v128) (i64x2.trunc_u:sat/f64x2 (f64x2.splat (get_local $a)))))

;; Test that LLVM undef isn't introduced by SIMD shifts greater than the scalar width.

(module
	(memory 1 1 shared)
	(func (export "test-simd-shift-mask") (param $v v128) (result i32)
		(block $0
			(block $1
				(block $2
					(block $3
						(block $default
							;; If the table index is inferred to be undef, LLVM will branch to an
							;; arbitrary successor of the basic block.
							(br_table
								$0 $1 $2 $3
								$default
								(i32x4.extract_lane 0 (i32x4.shr_s (get_local $v)
								                                   (v128.const i32 32 32 32 32)))
							)
						)
						(return (i32.const 100))
					)
					(return (i32.const 3))
				)
				(return (i32.const 2))
			)
			(return (i32.const 1))
		)
		(return (i32.const 0))
	)
)

(assert_return (invoke "test-simd-shift-mask" (v128.const i32 0 0 0 0)) (i32.const 0))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32 1 0 0 0)) (i32.const 1))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32 2 0 0 0)) (i32.const 2))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32 3 0 0 0)) (i32.const 3))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32 4 0 0 0)) (i32.const 100))

;; Test that misaligned SIMD loads/stores don't trap

(module
	(memory 1 1)
	(func (export "v128.load align=16") (param $address i32) (result v128)
		(v128.load align=16 (get_local $address))
	)
	(func (export "v128.store align=16") (param $address i32) (param $value v128)
		(v128.store align=16 (get_local $address) (get_local $value))
	)
)

(assert_return (invoke "v128.load align=16" (i32.const 0)) (v128.const i64 0 0))
(assert_return (invoke "v128.load align=16" (i32.const 1)) (v128.const i64 0 0))
(assert_return (invoke "v128.store align=16" (i32.const 1) (v128.const i8 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)))
(assert_return (invoke "v128.load align=16" (i32.const 0)) (v128.const i8 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))