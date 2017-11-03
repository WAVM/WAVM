;; Test that 0 - (-0 - x) is not optimized to x.
;; https://llvm.org/bugs/show_bug.cgi?id=26746

(module
  (func (export "llvm_pr26746") (param $x f32) (result f32)
    (f32.sub (f32.const 0.0) (f32.sub (f32.const -0.0) (get_local $x)))
  )
)

;; Test for improperly reassociating an addition and a conversion.
;; https://llvm.org/bugs/show_bug.cgi?id=27153

(module
  (func (export "llvm_pr27153") (param $x i32) (result f32)
    (f32.add (f32.convert_s/i32 (i32.and (get_local $x) (i32.const 268435455))) (f32.const -8388608.0))
  )
)

;; should be (assert_return (invoke "llvm_pr27153" (i32.const 33554434)) (f32.const 25165824.000000))
(assert_return (invoke "llvm_pr27153" (i32.const 33554434)) (f32.const 0x1.800002p+24))

;; Test that (float)x + (float)y is not optimized to (float)(x + y) when unsafe.
;; https://llvm.org/bugs/show_bug.cgi?id=27036

(module
  (func (export "llvm_pr27036") (param $x i32) (param $y i32) (result f32)
    (f32.add (f32.convert_s/i32 (i32.or (get_local $x) (i32.const -25034805)))
             (f32.convert_s/i32 (i32.and (get_local $y) (i32.const 14942208))))
  )
)

;; should be (assert_return (invoke "llvm_pr27036" (i32.const -25034805) (i32.const 14942208)) (f32.const -0x1.340068p+23))
(assert_return (invoke "llvm_pr27036" (i32.const -25034805) (i32.const 14942208)) (f32.const -0x1.34006ap+23))

