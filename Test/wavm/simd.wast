;; v128 globals

(module $M
  (global (export "a") v128       (v128.const f32x4 0 1 2 3))
  (global (export "b") (mut v128) (v128.const f32x4 4 5 6 7))
)
(register "M" $M)

(module
  (global $a (import "M" "a") v128)
  (global $b (import "M" "b") (mut v128))

  (global $c v128       (global.get $a))
  (global $d v128       (v128.const i32x4 8 9 10 11))
  (global $e (mut v128) (global.get $a))
  (global $f (mut v128) (v128.const i32x4 12 13 14 15))

  (func (export "get-a") (result v128) (global.get $a))
  (func (export "get-b") (result v128) (global.get $b))
  (func (export "get-c") (result v128) (global.get $c))
  (func (export "get-d") (result v128) (global.get $d))
  (func (export "get-e") (result v128) (global.get $e))
  (func (export "get-f") (result v128) (global.get $f))

  (func (export "set-b") (param $value v128) (global.set $b (local.get $value)))
  (func (export "set-e") (param $value v128) (global.set $e (local.get $value)))
  (func (export "set-f") (param $value v128) (global.set $f (local.get $value)))
)

(assert_return (invoke "get-a") (v128.const f32x4 0 1 2 3))
(assert_return (invoke "get-b") (v128.const f32x4 4 5 6 7))
(assert_return (invoke "get-c") (v128.const f32x4 0 1 2 3))
(assert_return (invoke "get-d") (v128.const i32x4 8 9 10 11))
(assert_return (invoke "get-e") (v128.const f32x4 0 1 2 3))
(assert_return (invoke "get-f") (v128.const i32x4 12 13 14 15))

(invoke "set-b" (v128.const f64x2 nan:0x1 nan:0x2))
(assert_return (invoke "get-b") (v128.const f64x2 nan:0x1 nan:0x2))

(invoke "set-e" (v128.const f64x2 -nan:0x3 +inf))
(assert_return (invoke "get-e") (v128.const f64x2 -nan:0x3 +inf))

(invoke "set-f" (v128.const f32x4 -inf +3.14 10.0e30 +nan:0x42))
(assert_return (invoke "get-f") (v128.const f32x4 -inf +3.14 10.0e30 +nan:0x42))

(assert_invalid (module (global v128 (i32.const 0))) "invalid initializer expression")
(assert_invalid (module (global v128 (i64.const 0))) "invalid initializer expression")
(assert_invalid (module (global v128 (f32.const 0))) "invalid initializer expression")
(assert_invalid (module (global v128 (f64.const 0))) "invalid initializer expression")
(assert_invalid (module (global $i32 i32 (i32.const 0)) (global v128 (global.get $i32))) "invalid initializer expression")

(module binary
  "\00asm"
  "\01\00\00\00"       ;; 1 section
  "\06"                ;; global section
  "\16"                ;; 22 bytes
  "\01"                ;; 1 global
  "\7b"                ;; v128
  "\00"                ;; immutable
  "\fd\02"             ;; v128.const
  "\00\01\02\03"       ;; literal bytes 0-3
  "\04\05\06\07"       ;; literal bytes 4-7
  "\08\09\0a\0b"       ;; literal bytes 8-11
  "\0c\0d\0e\0f"       ;; literal bytes 12-15
  "\0b"                ;; end
)

(assert_malformed
  (module binary
    "\00asm"
    "\01\00\00\00"       ;; 1 section
    "\06\86\80\80\80\00" ;; global section
    "\01"                ;; 1 global
    "\7b"                ;; v128
    "\00"                ;; immutable
    "\fd\00"             ;; v128.load
    "\0b"                ;; end
  )
  "invalid initializer expression"
)

;; TODO: v128 parameters

;; TODO: v128 locals

;; TODO: v128 results

;; v128.const

(module
  (func (export "v128.const/i8x16") (result v128) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
  (func (export "v128.const/i16x8") (result v128) (v128.const i16x8 16 17 18 19 20 21 22 23))
  (func (export "v128.const/i32x4") (result v128) (v128.const i32x4 24 25 26 27))
  (func (export "v128.const/i64x2") (result v128) (v128.const i64x2 28 29))
  (func (export "v128.const/f32x4") (result v128) (v128.const f32x4 30.5 31.5 32.5 33.5))
  (func (export "v128.const/f64x2") (result v128) (v128.const f64x2 34.5 35.5))
)

;; v128.load/store

(module
  (memory 1)
  (func (export "v128.load")  (param $address i32)                     (result v128) (v128.load  align=16 (local.get $address)))
  (func (export "v128.store") (param $address i32) (param $value v128)               (v128.store align=16 (local.get $address) (local.get $value)))
)

(assert_invalid (module (memory 1) (func (drop (v128.load  align=32 (i32.const 0))))) "invalid alignment")
(assert_invalid (module (memory 1) (func (drop (v128.store align=32 (i32.const 0))))) "invalid alignment")

;; *.splat

(module
  (func (export "i8x16.splat") (param $a i32) (result v128) (i8x16.splat (local.get $a)))
  (func (export "i16x8.splat") (param $a i32) (result v128) (i16x8.splat (local.get $a)))
  (func (export "i32x4.splat") (param $a i32) (result v128) (i32x4.splat (local.get $a)))
  (func (export "i64x2.splat") (param $a i64) (result v128) (i64x2.splat (local.get $a)))
  (func (export "f32x4.splat") (param $a f32) (result v128) (f32x4.splat (local.get $a)))
  (func (export "f64x2.splat") (param $a f64) (result v128) (f64x2.splat (local.get $a)))
)

;; i*x*.load_splat

(module
  (memory 1)
  (data (i32.const 0) "\00\01\02\03\04\05\06\07\08\09\0a")
  (func (export "v128.load8_splat") (param $address i32) (result v128) (v128.load8_splat offset=0 align=1 (local.get $address)))
)

(assert_return (invoke "v128.load8_splat" (i32.const 0)) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
(assert_return (invoke "v128.load8_splat" (i32.const 1)) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))
(assert_return (invoke "v128.load8_splat" (i32.const 2)) (v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2))
(assert_return (invoke "v128.load8_splat" (i32.const 3)) (v128.const i8x16 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3))

(module
  (memory 1)
  (data (i32.const 0) "\00\01\02\03\04\05\06\07\08\09\0a")
  (func (export "v128.load16_splat") (param $address i32) (result v128) (v128.load16_splat offset=0 align=1 (local.get $address)))
)

(assert_return (invoke "v128.load16_splat" (i32.const 0)) (v128.const i16x8 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100))
(assert_return (invoke "v128.load16_splat" (i32.const 1)) (v128.const i16x8 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201))
(assert_return (invoke "v128.load16_splat" (i32.const 2)) (v128.const i16x8 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302))
(assert_return (invoke "v128.load16_splat" (i32.const 3)) (v128.const i16x8 0x0403 0x0403 0x0403 0x0403 0x0403 0x0403 0x0403 0x0403))

(module
  (memory 1)
  (data (i32.const 0) "\00\01\02\03\04\05\06\07\08\09\0a")
  (func (export "v128.load32_splat") (param $address i32) (result v128) (v128.load32_splat offset=0 align=1 (local.get $address)))
)

(assert_return (invoke "v128.load32_splat" (i32.const 0)) (v128.const i32x4 0x03020100 0x03020100 0x03020100 0x03020100))
(assert_return (invoke "v128.load32_splat" (i32.const 1)) (v128.const i32x4 0x04030201 0x04030201 0x04030201 0x04030201))
(assert_return (invoke "v128.load32_splat" (i32.const 2)) (v128.const i32x4 0x05040302 0x05040302 0x05040302 0x05040302))
(assert_return (invoke "v128.load32_splat" (i32.const 3)) (v128.const i32x4 0x06050403 0x06050403 0x06050403 0x06050403))

(module
  (memory 1)
  (data (i32.const 0) "\00\01\02\03\04\05\06\07\08\09\0a")
  (func (export "v128.load64_splat") (param $address i32) (result v128) (v128.load64_splat offset=0 align=1 (local.get $address)))
)

(assert_return (invoke "v128.load64_splat" (i32.const 0)) (v128.const i64x2 0x0706050403020100 0x0706050403020100))
(assert_return (invoke "v128.load64_splat" (i32.const 1)) (v128.const i64x2 0x0807060504030201 0x0807060504030201))
(assert_return (invoke "v128.load64_splat" (i32.const 2)) (v128.const i64x2 0x0908070605040302 0x0908070605040302))
(assert_return (invoke "v128.load64_splat" (i32.const 3)) (v128.const i64x2 0x0a09080706050403 0x0a09080706050403))

;; *.extract_lane*

(module
  (func (export "i8x16.extract_lane_s/0")  (param $a v128) (result i32) (i8x16.extract_lane_s 0  (local.get $a)))
  (func (export "i8x16.extract_lane_s/1")  (param $a v128) (result i32) (i8x16.extract_lane_s 1  (local.get $a)))
  (func (export "i8x16.extract_lane_s/2")  (param $a v128) (result i32) (i8x16.extract_lane_s 2  (local.get $a)))
  (func (export "i8x16.extract_lane_s/3")  (param $a v128) (result i32) (i8x16.extract_lane_s 3  (local.get $a)))
  (func (export "i8x16.extract_lane_s/4")  (param $a v128) (result i32) (i8x16.extract_lane_s 4  (local.get $a)))
  (func (export "i8x16.extract_lane_s/5")  (param $a v128) (result i32) (i8x16.extract_lane_s 5  (local.get $a)))
  (func (export "i8x16.extract_lane_s/6")  (param $a v128) (result i32) (i8x16.extract_lane_s 6  (local.get $a)))
  (func (export "i8x16.extract_lane_s/7")  (param $a v128) (result i32) (i8x16.extract_lane_s 7  (local.get $a)))
  (func (export "i8x16.extract_lane_s/8")  (param $a v128) (result i32) (i8x16.extract_lane_s 8  (local.get $a)))
  (func (export "i8x16.extract_lane_s/9")  (param $a v128) (result i32) (i8x16.extract_lane_s 9  (local.get $a)))
  (func (export "i8x16.extract_lane_s/10") (param $a v128) (result i32) (i8x16.extract_lane_s 10 (local.get $a)))
  (func (export "i8x16.extract_lane_s/11") (param $a v128) (result i32) (i8x16.extract_lane_s 11 (local.get $a)))
  (func (export "i8x16.extract_lane_s/12") (param $a v128) (result i32) (i8x16.extract_lane_s 12 (local.get $a)))
  (func (export "i8x16.extract_lane_s/13") (param $a v128) (result i32) (i8x16.extract_lane_s 13 (local.get $a)))
  (func (export "i8x16.extract_lane_s/14") (param $a v128) (result i32) (i8x16.extract_lane_s 14 (local.get $a)))
  (func (export "i8x16.extract_lane_s/15") (param $a v128) (result i32) (i8x16.extract_lane_s 15 (local.get $a)))

  (func (export "i8x16.extract_lane_u/0")  (param $a v128) (result i32) (i8x16.extract_lane_u 0  (local.get $a)))
  (func (export "i8x16.extract_lane_u/1")  (param $a v128) (result i32) (i8x16.extract_lane_u 1  (local.get $a)))
  (func (export "i8x16.extract_lane_u/2")  (param $a v128) (result i32) (i8x16.extract_lane_u 2  (local.get $a)))
  (func (export "i8x16.extract_lane_u/3")  (param $a v128) (result i32) (i8x16.extract_lane_u 3  (local.get $a)))
  (func (export "i8x16.extract_lane_u/4")  (param $a v128) (result i32) (i8x16.extract_lane_u 4  (local.get $a)))
  (func (export "i8x16.extract_lane_u/5")  (param $a v128) (result i32) (i8x16.extract_lane_u 5  (local.get $a)))
  (func (export "i8x16.extract_lane_u/6")  (param $a v128) (result i32) (i8x16.extract_lane_u 6  (local.get $a)))
  (func (export "i8x16.extract_lane_u/7")  (param $a v128) (result i32) (i8x16.extract_lane_u 7  (local.get $a)))
  (func (export "i8x16.extract_lane_u/8")  (param $a v128) (result i32) (i8x16.extract_lane_u 8  (local.get $a)))
  (func (export "i8x16.extract_lane_u/9")  (param $a v128) (result i32) (i8x16.extract_lane_u 9  (local.get $a)))
  (func (export "i8x16.extract_lane_u/10") (param $a v128) (result i32) (i8x16.extract_lane_u 10 (local.get $a)))
  (func (export "i8x16.extract_lane_u/11") (param $a v128) (result i32) (i8x16.extract_lane_u 11 (local.get $a)))
  (func (export "i8x16.extract_lane_u/12") (param $a v128) (result i32) (i8x16.extract_lane_u 12 (local.get $a)))
  (func (export "i8x16.extract_lane_u/13") (param $a v128) (result i32) (i8x16.extract_lane_u 13 (local.get $a)))
  (func (export "i8x16.extract_lane_u/14") (param $a v128) (result i32) (i8x16.extract_lane_u 14 (local.get $a)))
  (func (export "i8x16.extract_lane_u/15") (param $a v128) (result i32) (i8x16.extract_lane_u 15 (local.get $a)))

  (func (export "i16x8.extract_lane_s/0")  (param $a v128) (result i32) (i16x8.extract_lane_s 0  (local.get $a)))
  (func (export "i16x8.extract_lane_s/1")  (param $a v128) (result i32) (i16x8.extract_lane_s 1  (local.get $a)))
  (func (export "i16x8.extract_lane_s/2")  (param $a v128) (result i32) (i16x8.extract_lane_s 2  (local.get $a)))
  (func (export "i16x8.extract_lane_s/3")  (param $a v128) (result i32) (i16x8.extract_lane_s 3  (local.get $a)))
  (func (export "i16x8.extract_lane_s/4")  (param $a v128) (result i32) (i16x8.extract_lane_s 4  (local.get $a)))
  (func (export "i16x8.extract_lane_s/5")  (param $a v128) (result i32) (i16x8.extract_lane_s 5  (local.get $a)))
  (func (export "i16x8.extract_lane_s/6")  (param $a v128) (result i32) (i16x8.extract_lane_s 6  (local.get $a)))
  (func (export "i16x8.extract_lane_s/7")  (param $a v128) (result i32) (i16x8.extract_lane_s 7  (local.get $a)))

  (func (export "i16x8.extract_lane_u/0")  (param $a v128) (result i32) (i16x8.extract_lane_u 0  (local.get $a)))
  (func (export "i16x8.extract_lane_u/1")  (param $a v128) (result i32) (i16x8.extract_lane_u 1  (local.get $a)))
  (func (export "i16x8.extract_lane_u/2")  (param $a v128) (result i32) (i16x8.extract_lane_u 2  (local.get $a)))
  (func (export "i16x8.extract_lane_u/3")  (param $a v128) (result i32) (i16x8.extract_lane_u 3  (local.get $a)))
  (func (export "i16x8.extract_lane_u/4")  (param $a v128) (result i32) (i16x8.extract_lane_u 4  (local.get $a)))
  (func (export "i16x8.extract_lane_u/5")  (param $a v128) (result i32) (i16x8.extract_lane_u 5  (local.get $a)))
  (func (export "i16x8.extract_lane_u/6")  (param $a v128) (result i32) (i16x8.extract_lane_u 6  (local.get $a)))
  (func (export "i16x8.extract_lane_u/7")  (param $a v128) (result i32) (i16x8.extract_lane_u 7  (local.get $a)))

  (func (export "i32x4.extract_lane/0")  (param $a v128) (result i32) (i32x4.extract_lane 0  (local.get $a)))
  (func (export "i32x4.extract_lane/1")  (param $a v128) (result i32) (i32x4.extract_lane 1  (local.get $a)))
  (func (export "i32x4.extract_lane/2")  (param $a v128) (result i32) (i32x4.extract_lane 2  (local.get $a)))
  (func (export "i32x4.extract_lane/3")  (param $a v128) (result i32) (i32x4.extract_lane 3  (local.get $a)))

  (func (export "i64x2.extract_lane/0")  (param $a v128) (result i64) (i64x2.extract_lane 0  (local.get $a)))
  (func (export "i64x2.extract_lane/1")  (param $a v128) (result i64) (i64x2.extract_lane 1  (local.get $a)))

  (func (export "f32x4.extract_lane/0")  (param $a v128) (result f32) (f32x4.extract_lane 0  (local.get $a)))
  (func (export "f32x4.extract_lane/1")  (param $a v128) (result f32) (f32x4.extract_lane 1  (local.get $a)))
  (func (export "f32x4.extract_lane/2")  (param $a v128) (result f32) (f32x4.extract_lane 2  (local.get $a)))
  (func (export "f32x4.extract_lane/3")  (param $a v128) (result f32) (f32x4.extract_lane 3  (local.get $a)))

  (func (export "f64x2.extract_lane/0")  (param $a v128) (result f64) (f64x2.extract_lane 0  (local.get $a)))
  (func (export "f64x2.extract_lane/1")  (param $a v128) (result f64) (f64x2.extract_lane 1  (local.get $a)))
)

;; *.replace_lane

(module
  (func (export "i8x16.replace_lane/0")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 0  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/1")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 1  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/2")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 2  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/3")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 3  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/4")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 4  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/5")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 5  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/6")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 6  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/7")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 7  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/8")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 8  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/9")  (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 9  (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/10") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 10 (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/11") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 11 (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/12") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 12 (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/13") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 13 (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/14") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 14 (local.get $a) (local.get $b)))
  (func (export "i8x16.replace_lane/15") (param $a v128) (param $b i32) (result v128) (i8x16.replace_lane 15 (local.get $a) (local.get $b)))

  (func (export "i16x8.replace_lane/0")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 0  (local.get $a) (local.get $b)))
  (func (export "i16x8.replace_lane/1")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 1  (local.get $a) (local.get $b)))
  (func (export "i16x8.replace_lane/2")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 2  (local.get $a) (local.get $b)))
  (func (export "i16x8.replace_lane/3")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 3  (local.get $a) (local.get $b)))
  (func (export "i16x8.replace_lane/4")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 4  (local.get $a) (local.get $b)))
  (func (export "i16x8.replace_lane/5")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 5  (local.get $a) (local.get $b)))
  (func (export "i16x8.replace_lane/6")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 6  (local.get $a) (local.get $b)))
  (func (export "i16x8.replace_lane/7")  (param $a v128) (param $b i32) (result v128) (i16x8.replace_lane 7  (local.get $a) (local.get $b)))

  (func (export "i32x4.replace_lane/0")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 0  (local.get $a) (local.get $b)))
  (func (export "i32x4.replace_lane/1")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 1  (local.get $a) (local.get $b)))
  (func (export "i32x4.replace_lane/2")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 2  (local.get $a) (local.get $b)))
  (func (export "i32x4.replace_lane/3")  (param $a v128) (param $b i32) (result v128) (i32x4.replace_lane 3  (local.get $a) (local.get $b)))

  (func (export "i64x2.replace_lane/0")  (param $a v128) (param $b i64) (result v128) (i64x2.replace_lane 0  (local.get $a) (local.get $b)))
  (func (export "i64x2.replace_lane/1")  (param $a v128) (param $b i64) (result v128) (i64x2.replace_lane 1  (local.get $a) (local.get $b)))

  (func (export "f32x4.replace_lane/0")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 0  (local.get $a) (local.get $b)))
  (func (export "f32x4.replace_lane/1")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 1  (local.get $a) (local.get $b)))
  (func (export "f32x4.replace_lane/2")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 2  (local.get $a) (local.get $b)))
  (func (export "f32x4.replace_lane/3")  (param $a v128) (param $b f32) (result v128) (f32x4.replace_lane 3  (local.get $a) (local.get $b)))

  (func (export "f64x2.replace_lane/0")  (param $a v128) (param $b f64) (result v128) (f64x2.replace_lane 0  (local.get $a) (local.get $b)))
  (func (export "f64x2.replace_lane/1")  (param $a v128) (param $b f64) (result v128) (f64x2.replace_lane 1  (local.get $a) (local.get $b)))
)

;; i8x16.swizzle

(module
	(func (export "i8x16.swizzle") (param $elements v128) (param $indices v128) (result v128) (i8x16.swizzle (local.get $elements) (local.get $indices)))
)

(assert_return
	(invoke "i8x16.swizzle"
		(v128.const i8x16 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115)
		(v128.const i8x16  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0)
		)
	(v128.const i8x16     115 114 113 112 111 110 109 108 107 106 105 104 103 102 101 100))

(assert_return
	(invoke "i8x16.swizzle"
		(v128.const i8x16 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115)
		(v128.const i8x16  -1   1  -2   2  -3   3  -4   4  -5   5  -6   6  -7   7  -8   8)
		)
	(v128.const i8x16       0 101   0 102   0 103   0 104   0 105   0 106   0 107   0 108))

(assert_return
	(invoke "i8x16.swizzle"
		(v128.const i8x16 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115)
		(v128.const i8x16   9  16  10  17  11  18  12  19  13  20  14  21  15  22  16  23)
		)
	(v128.const i8x16     109   0 110   0 111   0 112   0 113   0 114   0 115   0   0   0))

;; i8x16.shuffle

(module
  (func (export "i8x16.shuffle/0123456789abcdef") (param $a v128) (param $b v128) (result v128) (i8x16.shuffle  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 (local.get $a) (local.get $b)))
  (func (export "i8x16.shuffle/ghijklmnopqrstuv") (param $a v128) (param $b v128) (result v128) (i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 (local.get $a) (local.get $b)))
  (func (export "i8x16.shuffle/vutsrqponmlkjihg") (param $a v128) (param $b v128) (result v128) (i8x16.shuffle 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 (local.get $a) (local.get $b)))
  (func (export "i8x16.shuffle/fedcba9876543210") (param $a v128) (param $b v128) (result v128) (i8x16.shuffle 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 (local.get $a) (local.get $b)))
  (func (export "i8x16.shuffle/0000000000000000") (param $a v128) (param $b v128) (result v128) (i8x16.shuffle  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0 (local.get $a) (local.get $b)))
  (func (export "i8x16.shuffle/gggggggggggggggg") (param $a v128) (param $b v128) (result v128) (i8x16.shuffle 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 (local.get $a) (local.get $b)))
  (func (export "i8x16.shuffle/00000000gggggggg") (param $a v128) (param $b v128) (result v128) (i8x16.shuffle  0  0  0  0  0  0  0  0 16 16 16 16 16 16 16 16 (local.get $a) (local.get $b)))
)

;; i*.narrow*

(module
  (func (export "i8x16.narrow_i16x8_s") (param $a v128) (param $b v128) (result v128) (i8x16.narrow_i16x8_s (local.get $a) (local.get $b)))
)

(assert_return (invoke "i8x16.narrow_i16x8_s" (v128.const i16x8 0  100  200  300  400  500  600  700) (v128.const i16x8 0 -100 -200 -300 -400 -500 -600 -700))
                                              (v128.const i8x16 0  100  127  127  127  127  127  127                    0 -100 -128 -128 -128 -128 -128 -128))

(module
  (func (export "i8x16.narrow_i16x8_u") (param $a v128) (param $b v128) (result v128) (i8x16.narrow_i16x8_u (local.get $a) (local.get $b)))
)

(assert_return (invoke "i8x16.narrow_i16x8_u" (v128.const i16x8 0  100  200  300  400  500  600  700) (v128.const i16x8 0 -100 -200 -300 -400 -500 -600 -700))
                                              (v128.const i8x16 0  100  200  255  255  255  255  255                    0    0    0    0    0    0    0    0))

(module
  (func (export "i16x8.narrow_i32x4_s") (param $a v128) (param $b v128) (result v128) (i16x8.narrow_i32x4_s (local.get $a) (local.get $b)))
)

(assert_return (invoke "i16x8.narrow_i32x4_s" (v128.const i32x4 0  15000  25000  35000) (v128.const i32x4 0 -15000 -25000 -35000))
                                              (v128.const i16x8 0  15000  25000  32767                    0 -15000 -25000 -32768))

(module
  (func (export "i16x8.narrow_i32x4_u") (param $a v128) (param $b v128) (result v128) (i16x8.narrow_i32x4_u (local.get $a) (local.get $b)))
)

(assert_return (invoke "i16x8.narrow_i32x4_u" (v128.const i32x4 0  15000  25000  35000) (v128.const i32x4 0 -15000 -25000 -35000))
                                              (v128.const i16x8 0  15000  25000  35000                    0      0      0      0))

;; i*.widen*

(module
  (func (export "i16x8.extend_low_i8x16_s") (param $a v128) (result v128) (i16x8.extend_low_i8x16_s (local.get $a)))
)

(assert_return (invoke "i16x8.extend_low_i8x16_s" (v128.const i8x16 0 +1 -2 +3 -4 +5 -6 +7 -8 +9 -10 +11 -12 +13 -14 +15))
                                                 (v128.const i16x8 0 +1 -2 +3 -4 +5 -6 +7                              ))

(module
  (func (export "i16x8.extend_high_i8x16_s") (param $a v128) (result v128) (i16x8.extend_high_i8x16_s (local.get $a)))
)

(assert_return (invoke "i16x8.extend_high_i8x16_s" (v128.const i8x16 0 +1 -2 +3 -4 +5 -6 +7 -8 +9 -10 +11 -12 +13 -14 +15))
                                                  (v128.const i16x8                        -8 +9 -10 +11 -12 +13 -14 +15))

(module
  (func (export "i16x8.extend_low_i8x16_u") (param $a v128) (result v128) (i16x8.extend_low_i8x16_u (local.get $a)))
)

(assert_return (invoke "i16x8.extend_low_i8x16_u" (v128.const i8x16 0xff 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
                                                 (v128.const i16x8 0xff 1 2 3 4 5 6 7                      ))

(module
  (func (export "i16x8.extend_high_i8x16_u") (param $a v128) (result v128) (i16x8.extend_high_i8x16_u (local.get $a)))
)

(assert_return (invoke "i16x8.extend_high_i8x16_u" (v128.const i8x16 0xff 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
                                                  (v128.const i16x8                    8 9 10 11 12 13 14 15))

(module
  (func (export "i32x4.extend_low_i16x8_s") (param $a v128) (result v128) (i32x4.extend_low_i16x8_s (local.get $a)))
)

(assert_return (invoke "i32x4.extend_low_i16x8_s" (v128.const i16x8 0 +1 -2 +3 -4 +5 -6 +7))
                                                 (v128.const i32x4 0 +1 -2 +3            ))

(module
  (func (export "i32x4.extend_high_i16x8_s") (param $a v128) (result v128) (i32x4.extend_high_i16x8_s (local.get $a)))
)

(assert_return (invoke "i32x4.extend_high_i16x8_s" (v128.const i16x8 0 +1 -2 +3 -4 +5 -6 +7))
                                                  (v128.const i32x4            -4 +5 -6 +7))

(module
  (func (export "i32x4.extend_low_i16x8_u") (param $a v128) (result v128) (i32x4.extend_low_i16x8_u (local.get $a)))
)

(assert_return (invoke "i32x4.extend_low_i16x8_u" (v128.const i16x8 0xff 1 2 3 4 5 6 7))
                                                 (v128.const i32x4 0xff 1 2 3        ))

(module
  (func (export "i32x4.extend_high_i16x8_u") (param $a v128) (result v128) (i32x4.extend_high_i16x8_u (local.get $a)))
)

(assert_return (invoke "i32x4.extend_high_i16x8_u" (v128.const i16x8 0xff 1 2 3 4 5 6 7))
                                                  (v128.const i32x4            4 5 6 7))

;; i*.add

(module
  (func (export "i8x16.add") (param $a v128) (param $b v128) (result v128) (i8x16.add (local.get $a) (local.get $b)))
  (func (export "i16x8.add") (param $a v128) (param $b v128) (result v128) (i16x8.add (local.get $a) (local.get $b)))
  (func (export "i32x4.add") (param $a v128) (param $b v128) (result v128) (i32x4.add (local.get $a) (local.get $b)))
  (func (export "i64x2.add") (param $a v128) (param $b v128) (result v128) (i64x2.add (local.get $a) (local.get $b)))
)

;; i*.sub

(module
  (func (export "i8x16.sub") (param $a v128) (param $b v128) (result v128) (i8x16.sub (local.get $a) (local.get $b)))
  (func (export "i16x8.sub") (param $a v128) (param $b v128) (result v128) (i16x8.sub (local.get $a) (local.get $b)))
  (func (export "i32x4.sub") (param $a v128) (param $b v128) (result v128) (i32x4.sub (local.get $a) (local.get $b)))
  (func (export "i64x2.sub") (param $a v128) (param $b v128) (result v128) (i64x2.sub (local.get $a) (local.get $b)))
)

;; i*.mul

(module
  (func (export "i16x8.mul") (param $a v128) (param $b v128) (result v128) (i16x8.mul (local.get $a) (local.get $b)))
  (func (export "i32x4.mul") (param $a v128) (param $b v128) (result v128) (i32x4.mul (local.get $a) (local.get $b)))
  (func (export "i64x2.mul") (param $a v128) (param $b v128) (result v128) (i64x2.mul (local.get $a) (local.get $b)))
)

;; i*.neg

(module
  (func (export "i8x16.neg") (param $a v128) (result v128) (i8x16.neg (local.get $a)))
  (func (export "i16x8.neg") (param $a v128) (result v128) (i16x8.neg (local.get $a)))
  (func (export "i32x4.neg") (param $a v128) (result v128) (i32x4.neg (local.get $a)))
  (func (export "i64x2.neg") (param $a v128) (result v128) (i64x2.neg (local.get $a)))
)

;; i*.add_sat*

(module
  (func (export "i8x16.add_sat_s") (param $a v128) (param $b v128) (result v128) (i8x16.add_sat_s (local.get $a) (local.get $b)))
  (func (export "i8x16.add_sat_u") (param $a v128) (param $b v128) (result v128) (i8x16.add_sat_u (local.get $a) (local.get $b)))
  (func (export "i16x8.add_sat_s") (param $a v128) (param $b v128) (result v128) (i16x8.add_sat_s (local.get $a) (local.get $b)))
  (func (export "i16x8.add_sat_u") (param $a v128) (param $b v128) (result v128) (i16x8.add_sat_u (local.get $a) (local.get $b)))
)

(assert_return
  (invoke "i8x16.add_sat_s"
    (v128.const i8x16 127 126 125 124 123 122 121 120 119 120 121 122 123 124 125 126)
    (v128.const i8x16 -7 -6 -5 -4 -3 -2 -1 0 +1 +2 +3 +4 +5 +6 +7 +8))
  (v128.const i8x16 120 120 120 120 120 120 120 120 120 122 124 126 127 127 127 127))

;; i*.sub_sat*

(module
  (func (export "i8x16.sub_sat_s") (param $a v128) (param $b v128) (result v128) (i8x16.sub_sat_s (local.get $a) (local.get $b)))
  (func (export "i8x16.sub_sat_u") (param $a v128) (param $b v128) (result v128) (i8x16.sub_sat_u (local.get $a) (local.get $b)))
  (func (export "i16x8.sub_sat_s") (param $a v128) (param $b v128) (result v128) (i16x8.sub_sat_s (local.get $a) (local.get $b)))
  (func (export "i16x8.sub_sat_u") (param $a v128) (param $b v128) (result v128) (i16x8.sub_sat_u (local.get $a) (local.get $b)))
)

;; v128.and/or/xor/not

(module
  (func (export "v128.and") (param $a v128) (param $b v128) (result v128) (v128.and (local.get $a) (local.get $b)))
  (func (export "v128.or")  (param $a v128) (param $b v128) (result v128) (v128.or  (local.get $a) (local.get $b)))
  (func (export "v128.xor") (param $a v128) (param $b v128) (result v128) (v128.xor (local.get $a) (local.get $b)))
  (func (export "v128.not") (param $a v128)                 (result v128) (v128.not (local.get $a)               ))
)

(module (func (export "v128.bitselect") (param $a v128) (param $b v128) (param $c v128) (result v128) (v128.bitselect (local.get $a) (local.get $b) (local.get $c))))

(module
  (func (export "v128.any_true") (param $a v128) (result i32) (v128.any_true (local.get $a)))
)

(module
  (func (export "i8x16.all_true") (param $a v128) (result i32) (i8x16.all_true (local.get $a)))
  (func (export "i16x8.all_true") (param $a v128) (result i32) (i16x8.all_true (local.get $a)))
  (func (export "i32x4.all_true") (param $a v128) (result i32) (i32x4.all_true (local.get $a)))
)

(module
  (func (export "i8x16.eq") (param $a v128) (param $b v128) (result v128) (i8x16.eq (local.get $a) (local.get $b)))
  (func (export "i16x8.eq") (param $a v128) (param $b v128) (result v128) (i16x8.eq (local.get $a) (local.get $b)))
  (func (export "i32x4.eq") (param $a v128) (param $b v128) (result v128) (i32x4.eq (local.get $a) (local.get $b)))
  (func (export "f32x4.eq") (param $a v128) (param $b v128) (result v128) (f32x4.eq (local.get $a) (local.get $b)))
  (func (export "f64x2.eq") (param $a v128) (param $b v128) (result v128) (f64x2.eq (local.get $a) (local.get $b)))
)

(module
  (func (export "i8x16.ne") (param $a v128) (param $b v128) (result v128) (i8x16.ne (local.get $a) (local.get $b)))
  (func (export "i16x8.ne") (param $a v128) (param $b v128) (result v128) (i16x8.ne (local.get $a) (local.get $b)))
  (func (export "i32x4.ne") (param $a v128) (param $b v128) (result v128) (i32x4.ne (local.get $a) (local.get $b)))
  (func (export "f32x4.ne") (param $a v128) (param $b v128) (result v128) (f32x4.ne (local.get $a) (local.get $b)))
  (func (export "f64x2.ne") (param $a v128) (param $b v128) (result v128) (f64x2.ne (local.get $a) (local.get $b)))
)

(module
  (func (export "i8x16.lt_s") (param $a v128) (param $b v128) (result v128) (i8x16.lt_s (local.get $a) (local.get $b)))
  (func (export "i8x16.lt_u") (param $a v128) (param $b v128) (result v128) (i8x16.lt_u (local.get $a) (local.get $b)))
  (func (export "i16x8.lt_s") (param $a v128) (param $b v128) (result v128) (i16x8.lt_s (local.get $a) (local.get $b)))
  (func (export "i16x8.lt_u") (param $a v128) (param $b v128) (result v128) (i16x8.lt_u (local.get $a) (local.get $b)))
  (func (export "i32x4.lt_s") (param $a v128) (param $b v128) (result v128) (i32x4.lt_s (local.get $a) (local.get $b)))
  (func (export "i32x4.lt_u") (param $a v128) (param $b v128) (result v128) (i32x4.lt_u (local.get $a) (local.get $b)))
  (func (export "f32x4.lt")   (param $a v128) (param $b v128) (result v128) (f32x4.lt   (local.get $a) (local.get $b)))
  (func (export "f64x2.lt")   (param $a v128) (param $b v128) (result v128) (f64x2.lt   (local.get $a) (local.get $b)))
)

(module
  (func (export "i8x16.le_s") (param $a v128) (param $b v128) (result v128) (i8x16.le_s (local.get $a) (local.get $b)))
  (func (export "i8x16.le_u") (param $a v128) (param $b v128) (result v128) (i8x16.le_u (local.get $a) (local.get $b)))
  (func (export "i16x8.le_s") (param $a v128) (param $b v128) (result v128) (i16x8.le_s (local.get $a) (local.get $b)))
  (func (export "i16x8.le_u") (param $a v128) (param $b v128) (result v128) (i16x8.le_u (local.get $a) (local.get $b)))
  (func (export "i32x4.le_s") (param $a v128) (param $b v128) (result v128) (i32x4.le_s (local.get $a) (local.get $b)))
  (func (export "i32x4.le_u") (param $a v128) (param $b v128) (result v128) (i32x4.le_u (local.get $a) (local.get $b)))
  (func (export "f32x4.le")   (param $a v128) (param $b v128) (result v128) (f32x4.le   (local.get $a) (local.get $b)))
  (func (export "f64x2.le")   (param $a v128) (param $b v128) (result v128) (f64x2.le   (local.get $a) (local.get $b)))
)

(module
  (func (export "i8x16.gt_s") (param $a v128) (param $b v128) (result v128) (i8x16.gt_s (local.get $a) (local.get $b)))
  (func (export "i8x16.gt_u") (param $a v128) (param $b v128) (result v128) (i8x16.gt_u (local.get $a) (local.get $b)))
  (func (export "i16x8.gt_s") (param $a v128) (param $b v128) (result v128) (i16x8.gt_s (local.get $a) (local.get $b)))
  (func (export "i16x8.gt_u") (param $a v128) (param $b v128) (result v128) (i16x8.gt_u (local.get $a) (local.get $b)))
  (func (export "i32x4.gt_s") (param $a v128) (param $b v128) (result v128) (i32x4.gt_s (local.get $a) (local.get $b)))
  (func (export "i32x4.gt_u") (param $a v128) (param $b v128) (result v128) (i32x4.gt_u (local.get $a) (local.get $b)))
  (func (export "f32x4.gt")   (param $a v128) (param $b v128) (result v128) (f32x4.gt   (local.get $a) (local.get $b)))
  (func (export "f64x2.gt")   (param $a v128) (param $b v128) (result v128) (f64x2.gt   (local.get $a) (local.get $b)))
)

(module
  (func (export "i8x16.ge_s") (param $a v128) (param $b v128) (result v128) (i8x16.ge_s (local.get $a) (local.get $b)))
  (func (export "i8x16.ge_u") (param $a v128) (param $b v128) (result v128) (i8x16.ge_u (local.get $a) (local.get $b)))
  (func (export "i16x8.ge_s") (param $a v128) (param $b v128) (result v128) (i16x8.ge_s (local.get $a) (local.get $b)))
  (func (export "i16x8.ge_u") (param $a v128) (param $b v128) (result v128) (i16x8.ge_u (local.get $a) (local.get $b)))
  (func (export "i32x4.ge_s") (param $a v128) (param $b v128) (result v128) (i32x4.ge_s (local.get $a) (local.get $b)))
  (func (export "i32x4.ge_u") (param $a v128) (param $b v128) (result v128) (i32x4.ge_u (local.get $a) (local.get $b)))
  (func (export "f32x4.ge")   (param $a v128) (param $b v128) (result v128) (f32x4.ge   (local.get $a) (local.get $b)))
  (func (export "f64x2.ge")   (param $a v128) (param $b v128) (result v128) (f64x2.ge   (local.get $a) (local.get $b)))
)

(module
  (func (export "f32x4.min") (param $a v128) (param $b v128) (result v128) (f32x4.min (local.get $a) (local.get $b)))
  (func (export "f64x2.min") (param $a v128) (param $b v128) (result v128) (f64x2.min (local.get $a) (local.get $b)))
  (func (export "f32x4.max") (param $a v128) (param $b v128) (result v128) (f32x4.max (local.get $a) (local.get $b)))
  (func (export "f64x2.max") (param $a v128) (param $b v128) (result v128) (f64x2.max (local.get $a) (local.get $b)))
)

(module
  (func (export "f32x4.add") (param $a v128) (param $b v128) (result v128) (f32x4.add (local.get $a) (local.get $b)))
  (func (export "f64x2.add") (param $a v128) (param $b v128) (result v128) (f64x2.add (local.get $a) (local.get $b)))
  (func (export "f32x4.sub") (param $a v128) (param $b v128) (result v128) (f32x4.sub (local.get $a) (local.get $b)))
  (func (export "f64x2.sub") (param $a v128) (param $b v128) (result v128) (f64x2.sub (local.get $a) (local.get $b)))
  (func (export "f32x4.div") (param $a v128) (param $b v128) (result v128) (f32x4.div (local.get $a) (local.get $b)))
  (func (export "f64x2.div") (param $a v128) (param $b v128) (result v128) (f64x2.div (local.get $a) (local.get $b)))
  (func (export "f32x4.mul") (param $a v128) (param $b v128) (result v128) (f32x4.mul (local.get $a) (local.get $b)))
  (func (export "f64x2.mul") (param $a v128) (param $b v128) (result v128) (f64x2.mul (local.get $a) (local.get $b)))
)

(module
  (func (export "f32x4.neg")  (param $a v128) (result v128) (f32x4.neg  (local.get $a)))
  (func (export "f64x2.neg")  (param $a v128) (result v128) (f64x2.neg  (local.get $a)))
  (func (export "f32x4.abs")  (param $a v128) (result v128) (f32x4.abs  (local.get $a)))
  (func (export "f64x2.abs")  (param $a v128) (result v128) (f64x2.abs  (local.get $a)))
  (func (export "f32x4.sqrt") (param $a v128) (result v128) (f32x4.sqrt (local.get $a)))
  (func (export "f64x2.sqrt") (param $a v128) (result v128) (f64x2.sqrt (local.get $a)))
)

(module
  (func (export "f32x4.convert_i32x4_s") (param $a v128) (result v128) (f32x4.convert_i32x4_s (local.get $a)))
  (func (export "f32x4.convert_i32x4_u") (param $a v128) (result v128) (f32x4.convert_i32x4_u (local.get $a)))
)

;; i32x4.trunc_sat_f32x4_s

(module (func (export "i32x4.trunc_sat_f32x4_s") (param $a f32) (result v128) (i32x4.trunc_sat_f32x4_s (f32x4.splat (local.get $a)))))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const 0.0))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const 1.0))
  (v128.const i32x4 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const 1.9))
  (v128.const i32x4 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const 2.0))
  (v128.const i32x4 2 2 2 2))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -1.0))
  (v128.const i32x4 -1 -1 -1 -1))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -1.9))
  (v128.const i32x4 -1 -1 -1 -1))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -2))
  (v128.const i32x4 -2 -2 -2 -2))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -2147483648.0))
  (v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -2147483648.0))
  (v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -3000000000.0))
  (v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -inf))
  (v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const +inf))
  (v128.const i32x4 2147483647 2147483647 2147483647 2147483647))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const -nan))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const +nan))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_s" (f32.const nan:0x444444))
  (v128.const i32x4 0 0 0 0))

;; i32x4.trunc_sat_f32x4_u

(module (func (export "i32x4.trunc_sat_f32x4_u") (param $a f32) (result v128) (i32x4.trunc_sat_f32x4_u (f32x4.splat (local.get $a)))))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const 0.0))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const 1.0))
  (v128.const i32x4 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const 1.9))
  (v128.const i32x4 1 1 1 1))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const 2.0))
  (v128.const i32x4 2 2 2 2))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const -1.0))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const -2.0))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const -2147483648.0))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const -inf))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const +inf))
  (v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const -nan))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const +nan))
  (v128.const i32x4 0 0 0 0))

(assert_return
  (invoke "i32x4.trunc_sat_f32x4_u" (f32.const nan:0x444444))
  (v128.const i32x4 0 0 0 0))


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
								(i32x4.extract_lane 0 (i32x4.shr_s (local.get $v)
								                                   (i32.const 32)))
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

(assert_return (invoke "test-simd-shift-mask" (v128.const i32x4 0 0 0 0)) (i32.const 0))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32x4 1 0 0 0)) (i32.const 1))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32x4 2 0 0 0)) (i32.const 2))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32x4 3 0 0 0)) (i32.const 3))
(assert_return (invoke "test-simd-shift-mask" (v128.const i32x4 4 0 0 0)) (i32.const 100))

;; Test that misaligned SIMD loads/stores don't trap

(module
	(memory 1 1)
	(func (export "v128.load align=16") (param $address i32) (result v128)
		(v128.load align=16 (local.get $address))
	)
	(func (export "v128.store align=16") (param $address i32) (param $value v128)
		(v128.store align=16 (local.get $address) (local.get $value))
	)
)

(assert_return (invoke "v128.load align=16" (i32.const 0)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load align=16" (i32.const 1)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.store align=16" (i32.const 1) (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)))
(assert_return (invoke "v128.load align=16" (i32.const 0)) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))

;; v128.const format

(module (func (result v128) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)))
(module (func (result v128) (v128.const i16x8 0 1 2 3 4 5 6 7)))
(module (func (result v128) (v128.const i32x4 0 1 2 3)))
(module (func (result v128) (v128.const i64x2 0 1)))
(module (func (result v128) (v128.const i64x2 -1 -2)))

(module (func (result v128) (v128.const i32x4 0xa 0xb 0xc 0xd)))
(module (func (result v128) (v128.const i32x4 0xa 0xb 0xc 0xd)))

(module (func (result v128) (v128.const f32x4 0.0 1.0 2.0 3.0)))
(module (func (result v128) (v128.const f64x2 0.0 1.0)))

(module (func (result v128) (v128.const f32x4 0 1 2 3)))
(module (func (result v128) (v128.const f32x4 0 1 2 -0x1.0p+10)))

(assert_malformed
  (module quote "(func (result v128) (v128.const i32x4 0.0 1.0 2.0 3.0))")
  "expected i32 literal"
)

(assert_malformed
  (module quote "(func (result v128) (v128.const i32 0 1 2 3))")
  "expected 'i8x6', 'i16x8', 'i32x4', 'i64x2', 'f32x4', or 'f64x2'"
)
(assert_malformed
  (module quote "(func (result v128) (v128.const i16x4 0 1 2 3))")
  "expected 'i8x6', 'i16x8', 'i32x4', 'i64x2', 'f32x4', or 'f64x2'"
)
(assert_malformed
  (module quote "(func (result v128) (v128.const f32 0 1 2 3))")
  "expected 'i8x6', 'i16x8', 'i32x4', 'i64x2', 'f32x4', or 'f64x2'"
)
(assert_malformed
  (module quote "(func (result v128) (v128.const 0 1 2 3))")
  "expected 'i8x6', 'i16x8', 'i32x4', 'i64x2', 'f32x4', or 'f64x2'"
)

;; Test the assert_return_canonical_nan_fNxM and assert_return_arithmetic_nan_fNxM commands

(module
  (func (export "f32x4.splat") (param $x f32) (result v128) (f32x4.splat (local.get $x)))
  (func (export "f64x2.splat") (param $x f64) (result v128) (f64x2.splat (local.get $x)))
)

(assert_return (invoke "f32x4.splat" (f32.const +nan:0x400000)) (v128.const f32x4 nan:canonical nan:canonical nan:canonical nan:canonical))
(assert_return (invoke "f32x4.splat" (f32.const +nan:0x400000)) (v128.const f32x4 nan:arithmetic nan:arithmetic nan:arithmetic nan:arithmetic))
(assert_return (invoke "f32x4.splat" (f32.const +nan:0x400001)) (v128.const f32x4 nan:arithmetic nan:arithmetic nan:arithmetic nan:arithmetic))

(assert_return (invoke "f32x4.splat" (f32.const -nan:0x400000)) (v128.const f32x4 nan:canonical nan:canonical nan:canonical nan:canonical))
(assert_return (invoke "f32x4.splat" (f32.const -nan:0x400000)) (v128.const f32x4 nan:arithmetic nan:arithmetic nan:arithmetic nan:arithmetic))
(assert_return (invoke "f32x4.splat" (f32.const -nan:0x400001)) (v128.const f32x4 nan:arithmetic nan:arithmetic nan:arithmetic nan:arithmetic))

(assert_return (invoke "f64x2.splat" (f64.const +nan:0x8000000000000)) (v128.const f64x2 nan:canonical nan:canonical))
(assert_return (invoke "f64x2.splat" (f64.const +nan:0x8000000000000)) (v128.const f64x2 nan:arithmetic nan:arithmetic))
(assert_return (invoke "f64x2.splat" (f64.const +nan:0x8000000000001)) (v128.const f64x2 nan:arithmetic nan:arithmetic))

(assert_return (invoke "f64x2.splat" (f64.const -nan:0x8000000000000)) (v128.const f64x2 nan:canonical nan:canonical))
(assert_return (invoke "f64x2.splat" (f64.const -nan:0x8000000000000)) (v128.const f64x2 nan:arithmetic nan:arithmetic))
(assert_return (invoke "f64x2.splat" (f64.const -nan:0x8000000000001)) (v128.const f64x2 nan:arithmetic nan:arithmetic))

;; Load and extend instructions

(module
  (memory (data "\ff\fe\fd\fc\fb\fa\f9\f8"))
  (func (export "v128.load8x8_s")  (result v128) (v128.load8x8_s  (i32.const 0)))
  (func (export "v128.load8x8_u")  (result v128) (v128.load8x8_u  (i32.const 0)))
  (func (export "v128.load16x4_s") (result v128) (v128.load16x4_s (i32.const 0)))
  (func (export "v128.load16x4_u") (result v128) (v128.load16x4_u (i32.const 0)))
  (func (export "v128.load32x2_s") (result v128) (v128.load32x2_s (i32.const 0)))
  (func (export "v128.load32x2_u") (result v128) (v128.load32x2_u (i32.const 0)))
)

(assert_return (invoke "v128.load8x8_s") (v128.const i16x8 0xffff 0xfffe 0xfffd 0xfffc 0xfffb 0xfffa 0xfff9 0xfff8))
(assert_return (invoke "v128.load8x8_u") (v128.const i16x8 0x00ff 0x00fe 0x00fd 0x00fc 0x00fb 0x00fa 0x00f9 0x00f8))
(assert_return (invoke "v128.load16x4_s") (v128.const i32x4 0xfffffeff 0xfffffcfd 0xfffffafb 0xfffff8f9))
(assert_return (invoke "v128.load16x4_u") (v128.const i32x4 0x0000feff 0x0000fcfd 0x0000fafb 0x0000f8f9))
(assert_return (invoke "v128.load32x2_s") (v128.const i64x2 0xfffffffffcfdfeff 0xfffffffff8f9fafb))
(assert_return (invoke "v128.load32x2_u") (v128.const i64x2 0x00000000fcfdfeff 0x00000000f8f9fafb))

;; v128.andnot

(module
  (func (export "v128.andnot") (param v128 v128) (result v128) (v128.andnot (local.get 0) (local.get 1)))
)

(assert_return (invoke "v128.andnot" (v128.const i32x4 1 2 3 4) (v128.const i32x4 1 1 1 1)) (v128.const i32x4 0 2 2 4))

;; Load interleaved instructions

(module 
  (memory 1)
  (data (i32.const 0)
    "\00\01\02\03\04\05\06\07\08\09\0a\0b\0c\0d\0e\0f"
    "\10\11\12\13\14\15\16\17\18\19\1a\1b\1c\1d\1e\1f"
    "\20\21\22\23\24\25\26\27\28\29\2a\2b\2c\2d\2e\2f"
    "\30\31\32\33\34\35\36\37\38\39\3a\3b\3c\3d\3e\3f")

  (func (export "v8x16.load_interleaved_2")  (param i32) (result v128 v128)           (v8x16.load_interleaved_2 (local.get 0)))
  (func (export "v8x16.load_interleaved_3")  (param i32) (result v128 v128 v128)      (v8x16.load_interleaved_3 (local.get 0)))
  (func (export "v8x16.load_interleaved_4")  (param i32) (result v128 v128 v128 v128) (v8x16.load_interleaved_4 (local.get 0)))
  (func (export "v16x8.load_interleaved_2")  (param i32) (result v128 v128)           (v16x8.load_interleaved_2 (local.get 0)))
  (func (export "v16x8.load_interleaved_3")  (param i32) (result v128 v128 v128)      (v16x8.load_interleaved_3 (local.get 0)))
  (func (export "v16x8.load_interleaved_4")  (param i32) (result v128 v128 v128 v128) (v16x8.load_interleaved_4 (local.get 0)))
  (func (export "v32x4.load_interleaved_2")  (param i32) (result v128 v128)           (v32x4.load_interleaved_2 (local.get 0)))
  (func (export "v32x4.load_interleaved_3")  (param i32) (result v128 v128 v128)      (v32x4.load_interleaved_3 (local.get 0)))
  (func (export "v32x4.load_interleaved_4")  (param i32) (result v128 v128 v128 v128) (v32x4.load_interleaved_4 (local.get 0)))
  (func (export "v64x2.load_interleaved_2")  (param i32) (result v128 v128)           (v64x2.load_interleaved_2 (local.get 0)))
  (func (export "v64x2.load_interleaved_3")  (param i32) (result v128 v128 v128)      (v64x2.load_interleaved_3 (local.get 0)))
  (func (export "v64x2.load_interleaved_4")  (param i32) (result v128 v128 v128 v128) (v64x2.load_interleaved_4 (local.get 0)))
)

(assert_return (invoke "v8x16.load_interleaved_2" (i32.const 0))
  (v128.const i8x16 0 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30)
  (v128.const i8x16 1 3 5 7 9 11 13 15 17 19 21 23 25 27 29 31)
)

(assert_return (invoke "v8x16.load_interleaved_3" (i32.const 0))
  (v128.const i8x16 0 3 6 9  12 15 18 21 24 27 30 33 36 39 42 45)
  (v128.const i8x16 1 4 7 10 13 16 19 22 25 28 31 34 37 40 43 46)
  (v128.const i8x16 2 5 8 11 14 17 20 23 26 29 32 35 38 41 44 47)
)

(assert_return (invoke "v8x16.load_interleaved_4" (i32.const 0))
  (v128.const i8x16 0 4 8  12 16 20 24 28 32 36 40 44 48 52 56 60)
  (v128.const i8x16 1 5 9  13 17 21 25 29 33 37 41 45 49 53 57 61)
  (v128.const i8x16 2 6 10 14 18 22 26 30 34 38 42 46 50 54 58 62)
  (v128.const i8x16 3 7 11 15 19 23 27 31 35 39 43 47 51 55 59 63)
)

(assert_return (invoke "v16x8.load_interleaved_2" (i32.const 0))
  (v128.const i16x8 0x0100 0x0504 0x0908 0x0d0c 0x1110 0x1514 0x1918 0x1d1c)
  (v128.const i16x8 0x0302 0x0706 0x0b0a 0x0f0e 0x1312 0x1716 0x1b1a 0x1f1e)
)

(assert_return (invoke "v16x8.load_interleaved_3" (i32.const 0))
  (v128.const i16x8 0x0100 0x0706 0x0d0c 0x1312 0x1918 0x1f1e 0x2524 0x2b2a)
  (v128.const i16x8 0x0302 0x0908 0x0f0e 0x1514 0x1b1a 0x2120 0x2726 0x2d2c)
  (v128.const i16x8 0x0504 0x0b0a 0x1110 0x1716 0x1d1c 0x2322 0x2928 0x2f2e)
)

(assert_return (invoke "v16x8.load_interleaved_4" (i32.const 0))
  (v128.const i16x8 0x0100 0x0908 0x1110 0x1918 0x2120 0x2928 0x3130 0x3938)
  (v128.const i16x8 0x0302 0x0b0a 0x1312 0x1b1a 0x2322 0x2b2a 0x3332 0x3b3a)
  (v128.const i16x8 0x0504 0x0d0c 0x1514 0x1d1c 0x2524 0x2d2c 0x3534 0x3d3c)
  (v128.const i16x8 0x0706 0x0f0e 0x1716 0x1f1e 0x2726 0x2f2e 0x3736 0x3f3e)
)

(assert_return (invoke "v32x4.load_interleaved_2" (i32.const 0))
  (v128.const i32x4 0x03020100 0x0b0a0908 0x13121110 0x1b1a1918)
  (v128.const i32x4 0x07060504 0x0f0e0d0c 0x17161514 0x1f1e1d1c)
)

(assert_return (invoke "v32x4.load_interleaved_3" (i32.const 0))
  (v128.const i32x4 0x03020100 0x0f0e0d0c 0x1b1a1918 0x27262524)
  (v128.const i32x4 0x07060504 0x13121110 0x1f1e1d1c 0x2b2a2928)
  (v128.const i32x4 0x0b0a0908 0x17161514 0x23222120 0x2f2e2d2c)
)

(assert_return (invoke "v32x4.load_interleaved_4" (i32.const 0))
  (v128.const i32x4 0x03020100 0x13121110 0x23222120 0x33323130)
  (v128.const i32x4 0x07060504 0x17161514 0x27262524 0x37363534)
  (v128.const i32x4 0x0b0a0908 0x1b1a1918 0x2b2a2928 0x3b3a3938)
  (v128.const i32x4 0x0f0e0d0c 0x1f1e1d1c 0x2f2e2d2c 0x3f3e3d3c)
)

(assert_return (invoke "v64x2.load_interleaved_2" (i32.const 0))
  (v128.const i64x2 0x0706050403020100 0x1716151413121110)
  (v128.const i64x2 0x0f0e0d0c0b0a0908 0x1f1e1d1c1b1a1918)
)

(assert_return (invoke "v64x2.load_interleaved_3" (i32.const 0))
  (v128.const i64x2 0x0706050403020100 0x1f1e1d1c1b1a1918)
  (v128.const i64x2 0x0f0e0d0c0b0a0908 0x2726252423222120)
  (v128.const i64x2 0x1716151413121110 0x2f2e2d2c2b2a2928)
)

(assert_return (invoke "v64x2.load_interleaved_4" (i32.const 0))
  (v128.const i64x2 0x0706050403020100 0x2726252423222120)
  (v128.const i64x2 0x0f0e0d0c0b0a0908 0x2f2e2d2c2b2a2928)
  (v128.const i64x2 0x1716151413121110 0x3736353433323130)
  (v128.const i64x2 0x1f1e1d1c1b1a1918 0x3f3e3d3c3b3a3938)
)

(module 
  (memory 1)

  (func (export "v128.load") (param i32) (result v128) (v128.load (local.get 0)))

  (func (export "v8x16.store_interleaved_2") (param i32 v128 v128)           (v8x16.store_interleaved_2 (local.get 0) (local.get 1) (local.get 2)))
  (func (export "v8x16.store_interleaved_3") (param i32 v128 v128 v128)      (v8x16.store_interleaved_3 (local.get 0) (local.get 1) (local.get 2) (local.get 3)))
  (func (export "v8x16.store_interleaved_4") (param i32 v128 v128 v128 v128) (v8x16.store_interleaved_4 (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4)))
  (func (export "v16x8.store_interleaved_2") (param i32 v128 v128)           (v16x8.store_interleaved_2 (local.get 0) (local.get 1) (local.get 2)))
  (func (export "v16x8.store_interleaved_3") (param i32 v128 v128 v128)      (v16x8.store_interleaved_3 (local.get 0) (local.get 1) (local.get 2) (local.get 3)))
  (func (export "v16x8.store_interleaved_4") (param i32 v128 v128 v128 v128) (v16x8.store_interleaved_4 (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4)))
  (func (export "v32x4.store_interleaved_2") (param i32 v128 v128)           (v32x4.store_interleaved_2 (local.get 0) (local.get 1) (local.get 2)))
  (func (export "v32x4.store_interleaved_3") (param i32 v128 v128 v128)      (v32x4.store_interleaved_3 (local.get 0) (local.get 1) (local.get 2) (local.get 3)))
  (func (export "v32x4.store_interleaved_4") (param i32 v128 v128 v128 v128) (v32x4.store_interleaved_4 (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4)))
  (func (export "v64x2.store_interleaved_2") (param i32 v128 v128)           (v64x2.store_interleaved_2 (local.get 0) (local.get 1) (local.get 2)))
  (func (export "v64x2.store_interleaved_3") (param i32 v128 v128 v128)      (v64x2.store_interleaved_3 (local.get 0) (local.get 1) (local.get 2) (local.get 3)))
  (func (export "v64x2.store_interleaved_4") (param i32 v128 v128 v128 v128) (v64x2.store_interleaved_4 (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4)))
)

(assert_return (invoke "v128.load" (i32.const 0 )) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 16)) (v128.const i64x2 0 0))
(assert_return (invoke "v8x16.store_interleaved_2" (i32.const 0)
  (v128.const i8x16  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15)
  (v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31)
))
(assert_return (invoke "v128.load" (i32.const 0 )) (v128.const i8x16 0 16 1 17  2 18  3 19  4 20  5 21  6 22  7 23))
(assert_return (invoke "v128.load" (i32.const 16)) (v128.const i8x16 8 24 9 25 10 26 11 27 12 28 13 29 14 30 15 31))


(assert_return (invoke "v128.load" (i32.const 32)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 48)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 64)) (v128.const i64x2 0 0))
(assert_return (invoke "v8x16.store_interleaved_3" (i32.const 32)
  (v128.const i8x16  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15)
  (v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31)
  (v128.const i8x16 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47)
))
(assert_return (invoke "v128.load" (i32.const 32)) (v128.const i8x16  0 16 32  1 17 33  2 18 34  3 19 35  4 20 36  5))
(assert_return (invoke "v128.load" (i32.const 48)) (v128.const i8x16 21 37  6 22 38  7 23 39  8 24 40  9 25 41 10 26))
(assert_return (invoke "v128.load" (i32.const 64)) (v128.const i8x16 42 11 27 43 12 28 44 13 29 45 14 30 46 15 31 47))

(assert_return (invoke "v128.load" (i32.const 80)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 96)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 112)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 128)) (v128.const i64x2 0 0))
(assert_return (invoke "v8x16.store_interleaved_4" (i32.const 80)
  (v128.const i8x16  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15)
  (v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31)
  (v128.const i8x16 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47)
  (v128.const i8x16 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63)
))
(assert_return (invoke "v128.load" (i32.const 80 )) (v128.const i8x16  0 16 32 48  1 17 33 49  2 18 34 50  3 19 35 51))
(assert_return (invoke "v128.load" (i32.const 96 )) (v128.const i8x16  4 20 36 52  5 21 37 53  6 22 38 54  7 23 39 55))
(assert_return (invoke "v128.load" (i32.const 112)) (v128.const i8x16  8 24 40 56  9 25 41 57 10 26 42 58 11 27 43 59))
(assert_return (invoke "v128.load" (i32.const 128)) (v128.const i8x16 12 28 44 60 13 29 45 61 14 30 46 62 15 31 47 63))

(assert_return (invoke "v128.load" (i32.const 144)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 160)) (v128.const i64x2 0 0))
(assert_return (invoke "v16x8.store_interleaved_2" (i32.const 144)
  (v128.const i16x8  1000 1001 1002 1003 1004 1005 1006 1007)
  (v128.const i16x8  1008 1009 1010 1011 1012 1013 1014 1015)
))
(assert_return (invoke "v128.load" (i32.const 144)) (v128.const i16x8 1000 1008 1001 1009 1002 1010 1003 1011))
(assert_return (invoke "v128.load" (i32.const 160)) (v128.const i16x8 1004 1012 1005 1013 1006 1014 1007 1015))

(assert_return (invoke "v128.load" (i32.const 176)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 192)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 208)) (v128.const i64x2 0 0))
(assert_return (invoke "v16x8.store_interleaved_3" (i32.const 176)
  (v128.const i16x8 1000 1001 1002 1003 1004 1005 1006 1007)
  (v128.const i16x8 1008 1009 1010 1011 1012 1013 1014 1015)
  (v128.const i16x8 1016 1017 1018 1019 1020 1021 1022 1023)
))
(assert_return (invoke "v128.load" (i32.const 176)) (v128.const i16x8 1000 1008 1016 1001 1009 1017 1002 1010))
(assert_return (invoke "v128.load" (i32.const 192)) (v128.const i16x8 1018 1003 1011 1019 1004 1012 1020 1005))
(assert_return (invoke "v128.load" (i32.const 208)) (v128.const i16x8 1013 1021 1006 1014 1022 1007 1015 1023))

(assert_return (invoke "v128.load" (i32.const 224)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 240)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 256)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 272)) (v128.const i64x2 0 0))
(assert_return (invoke "v16x8.store_interleaved_4" (i32.const 224)
  (v128.const i16x8 1000 1001 1002 1003 1004 1005 1006 1007)
  (v128.const i16x8 1008 1009 1010 1011 1012 1013 1014 1015)
  (v128.const i16x8 1016 1017 1018 1019 1020 1021 1022 1023)
  (v128.const i16x8 1024 1025 1026 1027 1028 1029 1030 1031)
))
(assert_return (invoke "v128.load" (i32.const 224)) (v128.const i16x8 1000 1008 1016 1024 1001 1009 1017 1025))
(assert_return (invoke "v128.load" (i32.const 240)) (v128.const i16x8 1002 1010 1018 1026 1003 1011 1019 1027))
(assert_return (invoke "v128.load" (i32.const 256)) (v128.const i16x8 1004 1012 1020 1028 1005 1013 1021 1029))
(assert_return (invoke "v128.load" (i32.const 272)) (v128.const i16x8 1006 1014 1022 1030 1007 1015 1023 1031))


(assert_return (invoke "v128.load" (i32.const 288)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 304)) (v128.const i64x2 0 0))
(assert_return (invoke "v32x4.store_interleaved_2" (i32.const 288)
  (v128.const i32x4 100000 100001 100002 100003)
  (v128.const i32x4 100004 100005 100006 100007)
))
(assert_return (invoke "v128.load" (i32.const 288)) (v128.const i32x4 100000 100004 100001 100005))
(assert_return (invoke "v128.load" (i32.const 304)) (v128.const i32x4 100002 100006 100003 100007))

(assert_return (invoke "v128.load" (i32.const 320)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 336)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 352)) (v128.const i64x2 0 0))
(assert_return (invoke "v32x4.store_interleaved_3" (i32.const 320)
  (v128.const i32x4 100000 100001 100002 100003)
  (v128.const i32x4 100004 100005 100006 100007)
  (v128.const i32x4 100008 100009 100010 100011)
))
(assert_return (invoke "v128.load" (i32.const 320)) (v128.const i32x4 100000 100004 100008 100001))
(assert_return (invoke "v128.load" (i32.const 336)) (v128.const i32x4 100005 100009 100002 100006))
(assert_return (invoke "v128.load" (i32.const 352)) (v128.const i32x4 100010 100003 100007 100011))

(assert_return (invoke "v128.load" (i32.const 368)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 384)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 400)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 416)) (v128.const i64x2 0 0))
(assert_return (invoke "v32x4.store_interleaved_4" (i32.const 368)
  (v128.const i32x4 100000 100001 100002 100003)
  (v128.const i32x4 100004 100005 100006 100007)
  (v128.const i32x4 100008 100009 100010 100011)
  (v128.const i32x4 100012 100013 100014 100015)
))
(assert_return (invoke "v128.load" (i32.const 368)) (v128.const i32x4 100000 100004 100008 100012))
(assert_return (invoke "v128.load" (i32.const 384)) (v128.const i32x4 100001 100005 100009 100013))
(assert_return (invoke "v128.load" (i32.const 400)) (v128.const i32x4 100002 100006 100010 100014))
(assert_return (invoke "v128.load" (i32.const 416)) (v128.const i32x4 100003 100007 100011 100015))

(assert_return (invoke "v128.load" (i32.const 432)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 448)) (v128.const i64x2 0 0))
(assert_return (invoke "v64x2.store_interleaved_2" (i32.const 432)
  (v128.const i64x2 10000000000 10000000001)
  (v128.const i64x2 10000000002 10000000003)
))
(assert_return (invoke "v128.load" (i32.const 432)) (v128.const i64x2 10000000000 10000000002))
(assert_return (invoke "v128.load" (i32.const 448)) (v128.const i64x2 10000000001 10000000003))

(assert_return (invoke "v128.load" (i32.const 464)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 480)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 496)) (v128.const i64x2 0 0))
(assert_return (invoke "v64x2.store_interleaved_3" (i32.const 464)
  (v128.const i64x2 10000000000 10000000001)
  (v128.const i64x2 10000000002 10000000003)
  (v128.const i64x2 10000000004 10000000005)
))
(assert_return (invoke "v128.load" (i32.const 464)) (v128.const i64x2 10000000000 10000000002))
(assert_return (invoke "v128.load" (i32.const 480)) (v128.const i64x2 10000000004 10000000001))
(assert_return (invoke "v128.load" (i32.const 496)) (v128.const i64x2 10000000003 10000000005))

(assert_return (invoke "v128.load" (i32.const 512)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 528)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 544)) (v128.const i64x2 0 0))
(assert_return (invoke "v128.load" (i32.const 560)) (v128.const i64x2 0 0))
(assert_return (invoke "v64x2.store_interleaved_4" (i32.const 512)
  (v128.const i64x2 10000000000 10000000001)
  (v128.const i64x2 10000000002 10000000003)
  (v128.const i64x2 10000000004 10000000005)
  (v128.const i64x2 10000000006 10000000007)
))
(assert_return (invoke "v128.load" (i32.const 512)) (v128.const i64x2 10000000000 10000000002))
(assert_return (invoke "v128.load" (i32.const 528)) (v128.const i64x2 10000000004 10000000006))
(assert_return (invoke "v128.load" (i32.const 544)) (v128.const i64x2 10000000001 10000000003))
(assert_return (invoke "v128.load" (i32.const 560)) (v128.const i64x2 10000000005 10000000007))

;; i8x16.bitmask

(module
  (func (export "i8x16.bitmask") (param v128) (result i32)
    (i8x16.bitmask (local.get 0))
  )
)

(assert_return
  (invoke "i8x16.bitmask"
    (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
  (i32.const 0x0000)
)

(assert_return
  (invoke "i8x16.bitmask"
    (v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80))
  (i32.const 0xffff)
)

(assert_return
  (invoke "i8x16.bitmask"
    (v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f))
  (i32.const 0x0000)
)

(assert_return
  (invoke "i8x16.bitmask"
    (v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff))
  (i32.const 0xffff)
)

(assert_return
  (invoke "i8x16.bitmask"
    (v128.const i8x16 0x80 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
  (i32.const 0x0001)
)

(assert_return
  (invoke "i8x16.bitmask"
    (v128.const i8x16 0x00 0x80 0x00 0x80 0x00 0x80 0x00 0x80 0x00 0x80 0x00 0x80 0x00 0x80 0x00 0x80))
  (i32.const 0x0000aaaa)
)

;; i16x8.bitmask

(module
  (func (export "i16x8.bitmask") (param v128) (result i32)
    (i16x8.bitmask (local.get 0))
  )
)

(assert_return
  (invoke "i16x8.bitmask"
    (v128.const i16x8 0 0 0 0 0 0 0 0))
  (i32.const 0x00)
)

(assert_return
  (invoke "i16x8.bitmask"
    (v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000))
  (i32.const 0xff)
)

(assert_return
  (invoke "i16x8.bitmask"
    (v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff))
  (i32.const 0x00)
)

(assert_return
  (invoke "i16x8.bitmask"
    (v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff))
  (i32.const 0xff)
)

(assert_return
  (invoke "i16x8.bitmask"
    (v128.const i16x8 0x8000 0 0 0 0 0 0 0))
  (i32.const 0x01)
)

(assert_return
  (invoke "i16x8.bitmask"
    (v128.const i16x8 0x0000 0x8000 0x0000 0x8000 0x0000 0x8000 0x0000 0x8000))
  (i32.const 0xaa)
)

;; i32x4.bitmask

(module
  (func (export "i32x4.bitmask") (param v128) (result i32)
    (i32x4.bitmask (local.get 0))
  )
)

(assert_return
  (invoke "i32x4.bitmask"
    (v128.const i32x4 0 0 0 0))
  (i32.const 0x0)
)

(assert_return
  (invoke "i32x4.bitmask"
    (v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000))
  (i32.const 0xf)
)

(assert_return
  (invoke "i32x4.bitmask"
    (v128.const i32x4 0x7fffffff 0x7fffffff 0x7fffffff 0x7fffffff))
  (i32.const 0x0)
)

(assert_return
  (invoke "i32x4.bitmask"
    (v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff))
  (i32.const 0xf)
)

(assert_return
  (invoke "i32x4.bitmask"
    (v128.const i32x4 0x80000000 0 0 0))
  (i32.const 0x1)
)

(assert_return
  (invoke "i32x4.bitmask"
    (v128.const i32x4 0x00000000 0x80000000 0x00000000 0x80000000))
  (i32.const 0xa)
)

;; i8x16 min/max

(module
  (func (export "i8x16.min_s") (param v128 v128) (result v128) (i8x16.min_s (local.get 0) (local.get 1)))
  (func (export "i8x16.min_u") (param v128 v128) (result v128) (i8x16.min_u (local.get 0) (local.get 1)))
  (func (export "i8x16.max_s") (param v128 v128) (result v128) (i8x16.max_s (local.get 0) (local.get 1)))
  (func (export "i8x16.max_u") (param v128 v128) (result v128) (i8x16.max_u (local.get 0) (local.get 1)))
)

(assert_return
  (invoke "i8x16.min_s"
    (v128.const i8x16 -8 -7 -6 -5 -4 -3 -2 -1 +0 +1 +2 +3 +4 +5 +6 +7)
    (v128.const i8x16 +7 +6 +5 +4 +3 +2 +1 +0 -1 -2 -3 -4 -5 -6 -7 -8))
  (v128.const i8x16   -8 -7 -6 -5 -4 -3 -2 -1 -1 -2 -3 -4 -5 -6 -7 -8)
)

(assert_return
  (invoke "i8x16.min_u"
    (v128.const i8x16  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15)
    (v128.const i8x16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0))
  (v128.const i8x16    0  1  2  3  4  5  6  7  7  6  5  4  3  2  1  0)
)

(assert_return
  (invoke "i8x16.max_s"
    (v128.const i8x16 -8 -7 -6 -5 -4 -3 -2 -1 +0 +1 +2 +3 +4 +5 +6 +7)
    (v128.const i8x16 +7 +6 +5 +4 +3 +2 +1 +0 -1 -2 -3 -4 -5 -6 -7 -8))
  (v128.const i8x16   +7 +6 +5 +4 +3 +2 +1 +0 +0 +1 +2 +3 +4 +5 +6 +7)
)

(assert_return
  (invoke "i8x16.max_u"
    (v128.const i8x16  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15)
    (v128.const i8x16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0))
  (v128.const i8x16   15 14 13 12 11 10  9  8  8  9 10 11 12 13 14 15)
)

;; i16x8 min/max

(module
  (func (export "i16x8.min_s") (param v128 v128) (result v128) (i16x8.min_s (local.get 0) (local.get 1)))
  (func (export "i16x8.min_u") (param v128 v128) (result v128) (i16x8.min_u (local.get 0) (local.get 1)))
  (func (export "i16x8.max_s") (param v128 v128) (result v128) (i16x8.max_s (local.get 0) (local.get 1)))
  (func (export "i16x8.max_u") (param v128 v128) (result v128) (i16x8.max_u (local.get 0) (local.get 1)))
)

(assert_return
  (invoke "i16x8.min_s"
    (v128.const i16x8 -4 -3 -2 -1 +0 +1 +2 +3)
    (v128.const i16x8 +3 +2 +1 +0 -1 -2 -3 -4))
  (v128.const i16x8   -4 -3 -2 -1 -1 -2 -3 -4)
)

(assert_return
  (invoke "i16x8.min_u"
    (v128.const i16x8 0 1 2 3 4 5 6 7)
    (v128.const i16x8 7 6 5 4 3 2 1 0))
  (v128.const i16x8   0 1 2 3 3 2 1 0)
)

(assert_return
  (invoke "i16x8.max_s"
    (v128.const i16x8 -4 -3 -2 -1 +0 +1 +2 +3)
    (v128.const i16x8 +3 +2 +1 +0 -1 -2 -3 -4))
  (v128.const i16x8   +3 +2 +1 +0 +0 +1 +2 +3)
)

(assert_return
  (invoke "i16x8.max_u"
    (v128.const i16x8 0 1 2 3 4 5 6 7)
    (v128.const i16x8 7 6 5 4 3 2 1 0))
  (v128.const i16x8   7 6 5 4 4 5 6 7)
)

;; i32x4 min/max

(module
  (func (export "i32x4.min_s") (param v128 v128) (result v128) (i32x4.min_s (local.get 0) (local.get 1)))
  (func (export "i32x4.min_u") (param v128 v128) (result v128) (i32x4.min_u (local.get 0) (local.get 1)))
  (func (export "i32x4.max_s") (param v128 v128) (result v128) (i32x4.max_s (local.get 0) (local.get 1)))
  (func (export "i32x4.max_u") (param v128 v128) (result v128) (i32x4.max_u (local.get 0) (local.get 1)))
)

(assert_return
  (invoke "i32x4.min_s"
    (v128.const i32x4 -2 -1 +0 +1)
    (v128.const i32x4 +1 +0 -1 -2))
  (v128.const i32x4   -2 -1 -1 -2)
)

(assert_return
  (invoke "i32x4.min_u"
    (v128.const i32x4 0 1 2 3)
    (v128.const i32x4 3 2 1 0))
  (v128.const i32x4   0 1 1 0)
)

(assert_return
  (invoke "i32x4.max_s"
    (v128.const i32x4 -2 -1 +0 +1)
    (v128.const i32x4 +1 +0 -1 -2))
  (v128.const i32x4   +1 +0 +0 +1)
)

(assert_return
  (invoke "i32x4.max_u"
    (v128.const i32x4 0 1 2 3)
    (v128.const i32x4 3 2 1 0))
  (v128.const i32x4   3 2 2 3)
)

;; i8x16.avgr_u

(module (func (export "i8x16.avgr_u") (param v128 v128) (result v128) (i8x16.avgr_u (local.get 0) (local.get 1))))

(assert_return
  (invoke "i8x16.avgr_u"
    (v128.const i8x16 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215)
    (v128.const i8x16 216 218 220 222 224 226 228 230 232 234 236 238 240 242 244 246))
  (v128.const i8x16 208 210 211 213 214 216 217 219 220 222 223 225 226 228 229 231)
)

;; i16x8.avgr_u

(module (func (export "i16x8.avgr_u") (param v128 v128) (result v128) (i16x8.avgr_u (local.get 0) (local.get 1))))

(assert_return
  (invoke "i16x8.avgr_u"
    (v128.const i16x8 40000 40001 40002 40003 40004 40005 40006 40007)
    (v128.const i16x8 40016 40018 40020 40022 40024 40026 40028 40030))
  (v128.const i16x8 40008 40010 40011 40013 40014 40016 40017 40019)
)

;; Test for LLVM bug found by fuzzing: https://bugs.llvm.org/show_bug.cgi?id=43750

(module 
  (func (param v128) (result f64)
    local.get 0
    i8x16.bitmask
    i16x8.splat
    f64x2.extract_lane 1
  )
)

;; i8x16.abs

(module (func (export "i8x16.abs") (param v128) (result v128) (i8x16.abs (local.get 0))))

(assert_return
  (invoke "i8x16.abs" (v128.const i8x16 +0 +1 -1 +3 -3 +7 -7 +15 -15 +31 -31 +63 -63 +127 -127 -128))
                      (v128.const i8x16 +0 +1 +1 +3 +3 +7 +7 +15 +15 +31 +31 +63 +63 +127 +127 -128))

;; i16x8.abs

(module (func (export "i16x8.abs") (param v128) (result v128) (i16x8.abs (local.get 0))))

(assert_return
  (invoke "i16x8.abs" (v128.const i16x8 +0 +1 +127 -127 -128 +32767 -32767 -32768))
                      (v128.const i16x8 +0 +1 +127 +127 +128 +32767 +32767 -32768))

;; i32x4.abs

(module (func (export "i32x4.abs") (param v128) (result v128) (i32x4.abs (local.get 0))))

(assert_return
  (invoke "i32x4.abs" (v128.const i32x4 +0 +2147483647 -2147483647 -2147483648))
                      (v128.const i32x4 +0 +2147483647 +2147483647 -2147483648))
