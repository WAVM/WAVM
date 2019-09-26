;; Tests for the load_splat instruction

(module
  (memory 1)
  (data (i32.const 0) "\00\01\02\03\04\05\06\07\08\09\0A\0B\0C\0D\0E\0F")

  (func (export "v8x16.load_splat") (param $address i32) (result v128) (v8x16.load_splat (local.get $address)))
  (func (export "v16x8.load_splat") (param $address i32) (result v128) (v16x8.load_splat (local.get $address)))
  (func (export "v32x4.load_splat") (param $address i32) (result v128) (v32x4.load_splat (local.get $address)))
)
(assert_return (invoke "v8x16.load_splat" (i32.const 0)) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
(assert_return (invoke "v8x16.load_splat" (i32.const 1)) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))
(assert_return (invoke "v8x16.load_splat" (i32.const 2)) (v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2))
(assert_return (invoke "v8x16.load_splat" (i32.const 3)) (v128.const i8x16 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3))
(assert_return (invoke "v8x16.load_splat" (i32.const 65535)) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
(assert_return (invoke "v16x8.load_splat" (i32.const 4)) (v128.const i16x8 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504))
(assert_return (invoke "v16x8.load_splat" (i32.const 5)) (v128.const i16x8 0x0605 0x0605 0x0605 0x0605 0x0605 0x0605 0x0605 0x0605))
(assert_return (invoke "v16x8.load_splat" (i32.const 6)) (v128.const i16x8 0x0706 0x0706 0x0706 0x0706 0x0706 0x0706 0x0706 0x0706))
(assert_return (invoke "v16x8.load_splat" (i32.const 7)) (v128.const i16x8 0x0807 0x0807 0x0807 0x0807 0x0807 0x0807 0x0807 0x0807))
(assert_return (invoke "v16x8.load_splat" (i32.const 65534)) (v128.const i16x8 0 0 0 0 0 0 0 0))
(assert_return (invoke "v32x4.load_splat" (i32.const 8)) (v128.const i32x4 0x0B0A0908 0x0B0A0908 0x0B0A0908 0x0B0A0908))
(assert_return (invoke "v32x4.load_splat" (i32.const 9)) (v128.const i32x4 0x0C0B0A09 0x0C0B0A09 0x0C0B0A09 0x0C0B0A09))
(assert_return (invoke "v32x4.load_splat" (i32.const 10)) (v128.const i32x4 0x0D0C0B0A 0x0D0C0B0A 0x0D0C0B0A 0x0D0C0B0A))
(assert_return (invoke "v32x4.load_splat" (i32.const 11)) (v128.const i32x4 0x0E0D0C0B 0x0E0D0C0B 0x0E0D0C0B 0x0E0D0C0B))
(assert_return (invoke "v32x4.load_splat" (i32.const 65532)) (v128.const i32x4 0 0 0 0))


;; Out of bounds memory access
(assert_trap (invoke "v8x16.load_splat" (i32.const -1)) "out of bounds memory access")
(assert_trap (invoke "v16x8.load_splat" (i32.const -1)) "out of bounds memory access")
(assert_trap (invoke "v32x4.load_splat" (i32.const -1)) "out of bounds memory access")
(assert_trap (invoke "v8x16.load_splat" (i32.const 65536)) "out of bounds memory access")
(assert_trap (invoke "v16x8.load_splat" (i32.const 65535)) "out of bounds memory access")
(assert_trap (invoke "v32x4.load_splat" (i32.const 65533)) "out of bounds memory access")


;; Combination

(module (memory 1)
  (data (i32.const 0) "\00\01\02\03\04\05\06\07\08\09\0A")

  (func (export "v8x16.load_splat-in-block") (result v128)
      (block (result v128) (block (result v128) (v8x16.load_splat (i32.const 0))))
  )
  (func (export "v16x8.load_splat-in-block") (result v128)
      (block (result v128) (block (result v128) (v16x8.load_splat (i32.const 1))))
  )
  (func (export "v32x4.load_splat-in-block") (result v128)
      (block (result v128) (block (result v128) (v32x4.load_splat (i32.const 2))))
  )
  (func (export "v8x16.load_splat-as-br-value") (result v128)
    (block (result v128) (br 0 (v8x16.load_splat (i32.const 3))))
  )
  (func (export "v16x8.load_splat-as-br-value") (result v128)
    (block (result v128) (br 0 (v16x8.load_splat (i32.const 4))))
  )
  (func (export "v32x4.load_splat-as-br-value") (result v128)
    (block (result v128) (br 0 (v32x4.load_splat (i32.const 5))))
  )
  (func (export "v8x16.load_splat-extract_lane_s-operand") (result i32)
    (i8x16.extract_lane_s 0 (v8x16.load_splat (i32.const 6)))
  )
  (func (export "v16x8.load_splat-extract_lane_s-operand") (result i32)
    (i8x16.extract_lane_s 0 (v16x8.load_splat (i32.const 7)))
  )
  (func (export "v32x4.load_splat-extract_lane_s-operand") (result i32)
    (i8x16.extract_lane_s 0 (v32x4.load_splat (i32.const 8)))
  )
)
(assert_return (invoke "v8x16.load_splat-in-block") (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
(assert_return (invoke "v16x8.load_splat-in-block") (v128.const i16x8 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201))
(assert_return (invoke "v32x4.load_splat-in-block") (v128.const i32x4 0x05040302 0x05040302 0x05040302 0x05040302))
(assert_return (invoke "v8x16.load_splat-as-br-value") (v128.const i8x16 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3))
(assert_return (invoke "v16x8.load_splat-as-br-value") (v128.const i16x8 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504))
(assert_return (invoke "v32x4.load_splat-as-br-value") (v128.const i32x4 0x08070605 0x08070605 0x08070605 0x08070605))
(assert_return (invoke "v8x16.load_splat-extract_lane_s-operand") (i32.const 6))
(assert_return (invoke "v16x8.load_splat-extract_lane_s-operand") (i32.const 7))
(assert_return (invoke "v32x4.load_splat-extract_lane_s-operand") (i32.const 8))


;; Type check

(assert_invalid (module (memory 0) (func (result v128) (v8x16.load_splat (v128.const i32x4 0 0 0 0)))) "type mismatch")
(assert_invalid (module (memory 0) (func (result v128) (v16x8.load_splat (v128.const i32x4 0 0 0 0)))) "type mismatch")
(assert_invalid (module (memory 0) (func (result v128) (v32x4.load_splat (v128.const i32x4 0 0 0 0)))) "type mismatch")


;; Unknown operator

(assert_malformed (module quote "(memory 1) (func (drop (i8x16.load_splat (i32.const 0))))") "unknown operator")
(assert_malformed (module quote "(memory 1) (func (drop (i16x8.load_splat (i32.const 0))))") "unknown operator")
(assert_malformed (module quote "(memory 1) (func (drop (i32x4.load_splat (i32.const 0))))") "unknown operator")