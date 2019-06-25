;; v128.load operater with normal argument (e.g. (i8x16, i16x8 i32x4))

(module
  (memory 1)
  (func (export "v128.load") (param $address v128) (result v128)
    (local v128)
    (v128.store (i32.const 0) (local.get $address))
    (v128.load (i32.const 0))
  )
)

(assert_return (invoke "v128.load" (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
(assert_return (invoke "v128.load" (v128.const i16x8 0 1 2 3 4 5 6 7)) (v128.const i16x8 0 1 2 3 4 5 6 7))
(assert_return (invoke "v128.load" (v128.const i32x4 0 1 2 3)) (v128.const i32x4 0 1 2 3))


;; v128.load operater as the argument of other SIMD instructions

(module (memory 1)
  (data (i32.const 0) "\00\01\02\03\04\05\06\07\08\09\10\11\12\13\14\15\00\01\02\03")
  (func (export "as-i8x16_extract_lane_s-value/0") (result i32)
    (i8x16.extract_lane_s 0 (v128.load (i32.const 0)))
  )
  (func (export "as-i8x16.eq-operand") (result v128) (i8x16.eq (v128.load offset=0 (i32.const 0)) (v128.load offset=16 (i32.const 0))))
  (func (export "as-v128.not-operand") (param $0 v128) (result v128)
    (local v128)
    (v128.store (i32.const 0) (local.get $0))
    (v128.not (v128.load (i32.const 0)))
  )
  (func (export "as-v128.bitselect-operand") (param $0 v128) (param $1 v128) (param $2 v128) (result v128)
    (local v128 v128 v128)
    (v128.store            (i32.const 0) (local.get $0))
    (v128.store  offset=16 (i32.const 1) (local.get $1))
    (v128.store  offset=32 (i32.const 2) (local.get $2))
    (v128.bitselect (v128.load (i32.const 0)) (v128.load offset=16 (i32.const 1)) (v128.load offset=32 (i32.const 2)))
  )
  (func (export "as-i8x16.all_true-operand") (param $0 v128) (result i32)
    (local v128)
    (v128.store (i32.const 0) (local.get $0))
    (i8x16.all_true (v128.load (i32.const 0)))
  )
  (func (export "as-add/sub/sul-operand") (param $a v128) (param $b v128) (result v128)
    (v128.store offset=0  (i32.const 0) (local.get $a))
    (v128.store offset=15 (i32.const 1) (local.get $b))
    (v128.store (i32.const 0) (i8x16.add (v128.load (i32.const 0)) (v128.load offset=15 (i32.const 1))))      ;; 2 2 2 2 + 3 3 3 3 = 5 5 5 5
    (v128.store (i32.const 0) (i8x16.sub (v128.load (i32.const 0)) (v128.load offset=15 (i32.const 1))))      ;; 5 5 5 5 + 3 3 3 3 = 2 2 2 2
    (v128.store (i32.const 0) (i8x16.mul (v128.load (i32.const 0)) (v128.load offset=15 (i32.const 1))))      ;; 2 2 2 2 * 3 3 3 3 = 6 6 6 6
    (v128.load (i32.const 0))                                                                                 ;; 6 6 6 6
  )
  (func (export "as-i8x16.shl-operand") (param $0 v128) (param $1 i32) (result v128)
    (local v128 i32)
    (v128.store (i32.const 0) (local.get $0))
    (i8x16.shl (v128.load (i32.const 0)) (local.get $1))
  )
  (func (export "as-f32x4.mul-operand") (param $0 v128) (param $1 v128) (result v128)
    (local v128 v128)
    (v128.store (i32.const 0)           (local.get $0))
    (v128.store offset=16 (i32.const 1) (local.get $1))
    (f32x4.mul (v128.load (i32.const 0))  (v128.load offset=16 (i32.const 1)))
  )
  (func (export "as-f32x4.abs-operand") (param $0 v128) (result v128)
    (local v128)
    (v128.store (i32.const 0)           (local.get $0))
    (f32x4.abs (v128.load (i32.const 0)))
  )
  (func (export "as-f32x4.min-operand") (param $0 v128) (param $1 v128) (result v128)
    (local v128 v128)
    (v128.store (i32.const 0)          (local.get $0))
    (v128.store offset=16 (i32.const 1)(local.get $1))
    (f32x4.min (v128.load (i32.const 0)) (v128.load offset=16 (i32.const 1)))
  )
  (func (export "as-i32x4.trunc_sat_f32x4_s-operand") (param $0 v128) (result v128)
    (local v128)
    (v128.store (i32.const 0) (local.get $0))
    (i32x4.trunc_sat_f32x4_s (v128.load (i32.const 0)))
  )
  (func (export "as-f32x4.convert_i32x4_u-operand") (param $0 v128) (result v128)
    (local v128)
    (v128.store (i32.const 0) (local.get $0))
    (f32x4.convert_i32x4_u (v128.load (i32.const 0)))
  )
  (func (export "as-v8x16.shuffle1-operand") (param $0 v128) (param $1 v128) (result v128)
    (local v128 v128)
    (v128.store (i32.const 0) (local.get $0))
    (v128.store offset=16 (i32.const 1) (local.get $1))
    (v8x16.shuffle1 (v128.load (i32.const 0)) (v128.load offset=16 (i32.const 1)))
  )
  (func (export "as-br-value") (result v128)
    (local v128)
    (v128.store (i32.const 0) (v128.const i32x4 0 1 2 3))
    (block (result v128) (br 0 (v128.load (i32.const 0))))
  )
)

(assert_return (invoke "as-i8x16_extract_lane_s-value/0")  (i32.const 0x00))
(assert_return (invoke "as-i8x16.eq-operand") (v128.const i32x4 0xffffffff 0x00000000 0x00000000 0x00000000))
(assert_return (invoke "as-v128.not-operand" (v128.const i32x4 0 -1 0 -1)) (v128.const i32x4 -1 0 -1 0))
(assert_return (invoke "as-v128.bitselect-operand"
    (v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA)
    (v128.const i32x4 0xBBBBBBBB 0xBBBBBBBB 0xBBBBBBBB 0xBBBBBBBB)
    (v128.const i32x4 0xF0F0F0F0 0xFFFFFFFF 0x00000000 0xFF00FF00)
  )
  (v128.const i32x4 0xABABABAB 0xAAAAAAAA 0xBBBBBBBB 0xAABBAABB)
)
(assert_return (invoke "as-i8x16.all_true-operand" (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)) (i32.const 0))
(assert_return (invoke "as-add/sub/sul-operand" (v128.const i32x4 2 2 2 2) (v128.const i32x4 3 3 3 3)) (v128.const i32x4 6 6 6 6))
(assert_return (invoke "as-i8x16.shl-operand"
  (v128.const i8x16 0 1 2 4 8 16 32 64 -128 3 6 12 24 48 96 -64) (i32.const 1))
  (v128.const i8x16 0 2 4 8 16 32 64 -128 0 6 12 24 48 96 -64 -128)
)
(assert_return
  (invoke "as-f32x4.mul-operand"
    (v128.const f32x4 nan -nan inf 42)
    (v128.const f32x4 42 inf inf 2)
  )
  (v128.const f32x4 nan -nan inf 84)
)
(assert_return (invoke "as-f32x4.abs-operand" (v128.const f32x4 -0 nan -inf 5)) (v128.const f32x4 0 nan inf 5))
(assert_return (invoke "as-f32x4.min-operand" (v128.const f32x4 -0 0 nan 5) (v128.const f32x4 0 -0 5 nan)) (v128.const f32x4 -0 -0 nan nan))
(assert_return (invoke "as-i32x4.trunc_sat_f32x4_s-operand" (v128.const f32x4 42 nan inf -inf)) (v128.const i32x4 42 0 2147483647 -2147483648))
(assert_return (invoke "as-f32x4.convert_i32x4_u-operand" (v128.const i32x4 0 -1 2147483647 -2147483648))
  (v128.const f32x4 0 4294967296 2147483648 2147483648)
)
(assert_return
  (invoke "as-v8x16.shuffle1-operand"
    (v128.const i8x16 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115)
    (v128.const i8x16  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0)
  )
  (v128.const i8x16     115 114 113 112 111 110 109 108 107 106 105 104 103 102 101 100)
)
(assert_return (invoke "as-br-value") (v128.const i32x4 0 1 2 3))


;; Unknown operator(e.g. v128.load8, v128.load16, v128.load32)

(assert_malformed
  (module quote
    "(memory 1)"
    "(func (local v128) (drop (v128.load8 (i32.const 0))))"
  )
  "unknown operator"
)
(assert_malformed
  (module quote
    "(memory 1)"
    "(func (local v128) (drop (v128.load16 (i32.const 0))))"
  )
  "unknown operator"
)
(assert_malformed
  (module quote
    "(memory 1)"
    "(func (local v128) (drop (v128.load32 (i32.const 0))))"
  )
  "unknown operator"
)


;; Type mismatched (e.g. v128.load(f32.const 0), type address empty)

(assert_invalid
  (module (memory 1) (func (local v128) (drop (v128.load (f32.const 0)))))
  "type mismatch"
)
(assert_invalid
  (module (memory 1) (func (local v128) (block (br_if 0 (v128.load (i32.const 0))))))
  "type mismatch"
)
(assert_invalid
  (module (memory 1) (func (local v128) (v128.load (i32.const 0))))
  "type mismatch"
)


;; Type address empty

(assert_invalid
  (module (memory 1) (func (drop (v128.load (local.get 2)))))
  "unknown local 2"
)
