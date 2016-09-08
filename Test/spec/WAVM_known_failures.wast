;;
;; From float_exprs.wast
;;

;; Test that x - 0.0 is not folded to x.

(module
  (func (export "f32.no_fold_sub_zero") (param $x f32) (result f32)
    (f32.sub (get_local $x) (f32.const 0.0)))
  (func (export "f64.no_fold_sub_zero") (param $x f64) (result f64)
    (f64.sub (get_local $x) (f64.const 0.0)))
)

;; should be (assert_return (invoke "f32.no_fold_sub_zero" (f32.const nan:0x200000)) (f32.const nan:0x600000))
(assert_return (invoke "f32.no_fold_sub_zero" (f32.const nan:0x200000)) (f32.const nan:0x200000))

;; should be (assert_return (invoke "f64.no_fold_sub_zero" (f64.const nan:0x4000000000000)) (f64.const nan:0xc000000000000))
(assert_return (invoke "f64.no_fold_sub_zero" (f64.const nan:0x4000000000000)) (f64.const nan:0x4000000000000))

;; Test that x*1.0 is not folded to x.
;; See IEEE 754-2008 10.4 "Literal meaning and value-changing optimizations".

(module
  (func (export "f32.no_fold_mul_one") (param $x f32) (result f32)
    (f32.mul (get_local $x) (f32.const 1.0)))
  (func (export "f64.no_fold_mul_one") (param $x f64) (result f64)
    (f64.mul (get_local $x) (f64.const 1.0)))
)

;; should be (assert_return (invoke "f32.no_fold_mul_one" (f32.const nan:0x200000)) (f32.const nan:0x600000))
(assert_return (invoke "f32.no_fold_mul_one" (f32.const nan:0x200000)) (f32.const nan:0x200000))

;; should be (assert_return (invoke "f64.no_fold_mul_one" (f64.const nan:0x4000000000000)) (f64.const nan:0xc000000000000))
(assert_return (invoke "f64.no_fold_mul_one" (f64.const nan:0x4000000000000)) (f64.const nan:0x4000000000000))

;; Test that x/1.0 is not folded to x.

(module
  (func (export "f32.no_fold_div_one") (param $x f32) (result f32)
    (f32.div (get_local $x) (f32.const 1.0)))
  (func (export "f64.no_fold_div_one") (param $x f64) (result f64)
    (f64.div (get_local $x) (f64.const 1.0)))
)

;; should be (assert_return (invoke "f32.no_fold_div_one" (f32.const nan:0x200000)) (f32.const nan:0x600000))
(assert_return (invoke "f32.no_fold_div_one" (f32.const nan:0x200000)) (f32.const nan:0x200000))

;; should be (assert_return (invoke "f64.no_fold_div_one" (f64.const nan:0x4000000000000)) (f64.const nan:0xc000000000000))
(assert_return (invoke "f64.no_fold_div_one" (f64.const nan:0x4000000000000)) (f64.const nan:0x4000000000000))


;; Test that x/-1.0 is not folded to -x.

(module
  (func (export "f32.no_fold_div_neg1") (param $x f32) (result f32)
    (f32.div (get_local $x) (f32.const -1.0)))
  (func (export "f64.no_fold_div_neg1") (param $x f64) (result f64)
    (f64.div (get_local $x) (f64.const -1.0)))
)

;; should be (assert_return (invoke "f32.no_fold_div_neg1" (f32.const nan:0x200000)) (f32.const nan:0x600000))
(assert_return (invoke "f32.no_fold_div_neg1" (f32.const nan:0x200000)) (f32.const -nan:0x200000))

;; should be (assert_return (invoke "f64.no_fold_div_neg1" (f64.const nan:0x4000000000000)) (f64.const nan:0xc000000000000))
(assert_return (invoke "f64.no_fold_div_neg1" (f64.const nan:0x4000000000000)) (f64.const -nan:0x4000000000000))

;; Test that -0.0 - x is not folded to -x.

(module
  (func (export "f32.no_fold_neg0_sub") (param $x f32) (result f32)
    (f32.sub (f32.const -0.0) (get_local $x)))
  (func (export "f64.no_fold_neg0_sub") (param $x f64) (result f64)
    (f64.sub (f64.const -0.0) (get_local $x)))
)

;; should be (assert_return (invoke "f32.no_fold_neg0_sub" (f32.const nan:0x200000)) (f32.const nan:0x600000))
(assert_return (invoke "f32.no_fold_neg0_sub" (f32.const nan:0x200000)) (f32.const -nan:0x200000))

;; should be (assert_return (invoke "f64.no_fold_neg0_sub" (f64.const nan:0x4000000000000)) (f64.const nan:0xc000000000000))
(assert_return (invoke "f64.no_fold_neg0_sub" (f64.const nan:0x4000000000000)) (f64.const -nan:0x4000000000000))

;; Test that -1.0 * x is not folded to -x.

(module
  (func (export "f32.no_fold_neg1_mul") (param $x f32) (result f32)
    (f32.mul (f32.const -1.0) (get_local $x)))
  (func (export "f64.no_fold_neg1_mul") (param $x f64) (result f64)
    (f64.mul (f64.const -1.0) (get_local $x)))
)

;; should be (assert_return (invoke "f32.no_fold_neg1_mul" (f32.const nan:0x200000)) (f32.const nan:0x600000))
(assert_return (invoke "f32.no_fold_neg1_mul" (f32.const nan:0x200000)) (f32.const -nan:0x200000))

;; should be (assert_return (invoke "f64.no_fold_neg1_mul" (f64.const nan:0x4000000000000)) (f64.const nan:0xc000000000000))
(assert_return (invoke "f64.no_fold_neg1_mul" (f64.const nan:0x4000000000000)) (f64.const -nan:0x4000000000000))

;; Test that demote(promote(x)) is not folded to x, and aside from NaN is
;; bit-preserving.

(module
  (func (export "no_fold_promote_demote") (param $x f32) (result f32)
    (f32.demote/f64 (f64.promote/f32 (get_local $x))))
)

;; should be (assert_return (invoke "no_fold_promote_demote" (f32.const nan:0x200000)) (f32.const nan:0x600000))
(assert_return (invoke "no_fold_promote_demote" (f32.const nan:0x200000)) (f32.const nan:0x200000))

;; Test that 0 - (-0 - x) is not optimized to x.
;; https://llvm.org/bugs/show_bug.cgi?id=26746

(module
  (func (export "llvm_pr26746") (param $x f32) (result f32)
    (f32.sub (f32.const 0.0) (f32.sub (f32.const -0.0) (get_local $x)))
  )
)

;; should be (assert_return (invoke "llvm_pr26746" (f32.const -0.0)) (f32.const 0.0))
(assert_return (invoke "llvm_pr26746" (f32.const -0.0)) (f32.const -0.0))

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


;;
;; from call.wast
;;

(;
(module
  (func (param i32 i32))
  (func $arity-nop-first (call 0 (nop) (i32.const 1) (i32.const 2)))
  (func $arity-nop-mid (call 0 (i32.const 1) (nop) (i32.const 2)))
  (func $arity-nop-last (call 0 (i32.const 1) (i32.const 2) (nop)))
);)


;;
;; from call_indirect.wast
;;

(;(module
  (type (func (param i32 i32)))
  (table 0 anyfunc)
  (func $arity-nop-first
    (call_indirect 0 (nop) (i32.const 1) (i32.const 2) (i32.const 0))
  )
  (func $arity-nop-mid
    (call_indirect 0 (i32.const 1) (nop) (i32.const 2) (i32.const 0))
  )
  (func $arity-nop-last
    (call_indirect 0 (i32.const 1) (i32.const 2) (nop) (i32.const 0))
  )
);)


;;
;; from nop.wast
;;

(;

(module
  ;; Auxiliary definitions
  (func $dummy)
  (func $3-ary (param i32 i32 i32) (result i32)
    get_local 0 get_local 1 get_local 2 i32.sub i32.add
  )
  (memory 1)

  (func (export "as-func-first") (result i32)
    (nop) (i32.const 1)
  )
  (func (export "as-func-mid") (result i32)
    (call $dummy) (nop) (i32.const 2)
  )
  (func (export "as-func-last") (result i32)
    (call $dummy) (i32.const 3) (nop)
  )
  (func (export "as-func-everywhere") (result i32)
    (nop) (nop) (call $dummy) (nop) (i32.const 4) (nop) (nop)
  )

  (func (export "as-drop-last") (param i32)
    (get_local 0) (nop) (drop)
  )
  (func (export "as-drop-everywhere") (param i32)
    (nop) (nop) (get_local 0) (nop) (nop) (drop)
  )

  (func (export "as-select-mid1") (param i32) (result i32)
    (get_local 0) (nop) (get_local 0) (get_local 0) (select)
  )
  (func (export "as-select-mid2") (param i32) (result i32)
    (get_local 0) (get_local 0) (nop) (get_local 0) (select)
  )
  (func (export "as-select-last") (param i32) (result i32)
    (get_local 0) (get_local 0) (get_local 0) (nop) (select)
  )
  (func (export "as-select-everywhere") (param i32) (result i32)
    (nop) (get_local 0) (nop) (nop) (get_local 0)
    (nop) (nop) (get_local 0) (nop) (nop) (select)
  )

  (func (export "as-block-first") (result i32)
    (block (nop) (i32.const 2))
  )
  (func (export "as-block-mid") (result i32)
    (block (call $dummy) (nop) (i32.const 2))
  )
  (func (export "as-block-last") (result i32)
    (block (nop) (call $dummy) (i32.const 3) (nop))
  )
  (func (export "as-block-everywhere") (result i32)
    (block (nop) (nop) (call $dummy) (nop) (i32.const 4) (nop) (nop))
  )

  (func (export "as-loop-first") (result i32)
    (loop (nop) (i32.const 2))
  )
  (func (export "as-loop-mid") (result i32)
    (loop (call $dummy) (nop) (i32.const 2))
  )
  (func (export "as-loop-last") (result i32)
    (loop (call $dummy) (i32.const 3) (nop))
  )
  (func (export "as-loop-everywhere") (result i32)
    (loop (nop) (nop) (call $dummy) (nop) (i32.const 4) (nop) (nop))
  )

  (func (export "as-if-condition") (param i32)
    (get_local 0) (nop) (if (then (call $dummy)))
  )
  (func (export "as-if-then") (param i32)
    (if (get_local 0) (nop) (call $dummy))
  )
  (func (export "as-if-else") (param i32)
    (if (get_local 0) (call $dummy) (nop))
  )

  (func (export "as-br-last") (param i32) (result i32)
    (block (get_local 0) (nop) (br 1 0))
  )
  (func (export "as-br-everywhere") (param i32) (result i32)
    (block (nop) (nop) (get_local 0) (nop) (nop) (br 1 0))
  )

  (func (export "as-br_if-mid") (param i32) (result i32)
    (block (get_local 0) (nop) (get_local 0) (br_if 1 0) (i32.const 0))
  )
  (func (export "as-br_if-last") (param i32) (result i32)
    (block (get_local 0) (get_local 0) (nop) (br_if 1 0) (i32.const 0))
  )
  (func (export "as-br_if-everywhere") (param i32) (result i32)
    (block
      (nop) (nop) (get_local 0) (nop) (nop) (get_local 0) (nop) (nop)
      (br_if 1 0)
      (i32.const 0) 
    )
  )

  (func (export "as-br_table-mid") (param i32) (result i32)
    (block (get_local 0) (nop) (get_local 0) (br_table 1 0 0))
  )
  (func (export "as-br_table-last") (param i32) (result i32)
    (block (get_local 0) (get_local 0) (nop) (br_table 1 0 0))
  )
  (func (export "as-br_table-everywhere") (param i32) (result i32)
    (block
      (nop) (nop) (get_local 0) (nop) (nop) (get_local 0) (nop) (nop)
      (br_table 1 0 0)
    )
  )

  (func (export "as-return-last") (param i32) (result i32)
    (get_local 0) (nop) (return)
  )
  (func (export "as-return-everywhere") (param i32) (result i32)
    (nop) (nop) (get_local 0) (nop) (nop) (return)
  )

  (func (export "as-call-mid1") (param i32 i32 i32) (result i32)
    (get_local 0) (nop) (get_local 1) (get_local 2) (call $3-ary)
  )
  (func (export "as-call-mid2") (param i32 i32 i32) (result i32)
    (get_local 0) (get_local 1) (nop) (get_local 2) (call $3-ary)
  )
  (func (export "as-call-last") (param i32 i32 i32) (result i32)
    (get_local 0) (get_local 1) (get_local 2) (nop) (call $3-ary)
  )
  (func (export "as-call-everywhere") (param i32 i32 i32) (result i32)
    (nop) (nop) (get_local 0) (nop) (nop) (get_local 1)
    (nop) (nop) (get_local 2) (nop) (nop) (call $3-ary)
  )

  ;; TODO(stack): call_indirect, *_local, load*, store*

  (func (export "as-unary-last") (param i32) (result i32)
    (get_local 0) (nop) (i32.ctz)
  )
  (func (export "as-unary-everywhere") (param i32) (result i32)
    (nop) (nop) (get_local 0) (nop) (nop) (i32.ctz)
  )

  (func (export "as-binary-mid") (param i32) (result i32)
    (get_local 0) (nop) (get_local 0) (i32.add)
  )
  (func (export "as-binary-last") (param i32) (result i32)
    (get_local 0) (get_local 0) (nop) (i32.add)
  )
  (func (export "as-binary-everywhere") (param i32) (result i32)
    (nop) (get_local 0) (nop) (nop) (get_local 0) (nop) (nop) (i32.add)
  )

  (func (export "as-test-last") (param i32) (result i32)
    (get_local 0) (nop) (i32.eqz)
  )
  (func (export "as-test-everywhere") (param i32) (result i32)
    (nop) (nop) (get_local 0) (nop) (nop) i32.eqz
  )

  (func (export "as-compare-mid") (param i32) (result i32)
    (get_local 0) (nop) (get_local 0) (i32.ne)
  )
  (func (export "as-compare-last") (param i32) (result i32)
    (get_local 0) (get_local 0) (nop) (i32.lt_u)
  )
  (func (export "as-compare-everywhere") (param i32) (result i32)
    (nop) (get_local 0) (nop) (nop) (get_local 0) (nop) (nop) (i32.le_s)
  )

  (func (export "as-grow_memory-last") (param i32) (result i32)
    (get_local 0) (nop) (grow_memory)
  )
  (func (export "as-grow_memory-everywhere") (param i32) (result i32)
    (nop) (nop) (get_local 0) (nop) (nop) (grow_memory)
  )
)

(assert_return (invoke "as-func-first") (i32.const 1))
(assert_return (invoke "as-func-mid") (i32.const 2))
(assert_return (invoke "as-func-last") (i32.const 3))
(assert_return (invoke "as-func-everywhere") (i32.const 4))

(assert_return (invoke "as-drop-last" (i32.const 0)))
(assert_return (invoke "as-drop-everywhere" (i32.const 0)))

(assert_return (invoke "as-select-mid1" (i32.const 3)) (i32.const 3))
(assert_return (invoke "as-select-mid2" (i32.const 3)) (i32.const 3))
(assert_return (invoke "as-select-last" (i32.const 3)) (i32.const 3))
(assert_return (invoke "as-select-everywhere" (i32.const 3)) (i32.const 3))

(assert_return (invoke "as-block-first") (i32.const 2))
(assert_return (invoke "as-block-mid") (i32.const 2))
(assert_return (invoke "as-block-last") (i32.const 3))
(assert_return (invoke "as-block-everywhere") (i32.const 4))

(assert_return (invoke "as-loop-first") (i32.const 2))
(assert_return (invoke "as-loop-mid") (i32.const 2))
(assert_return (invoke "as-loop-last") (i32.const 3))
(assert_return (invoke "as-loop-everywhere") (i32.const 4))

(assert_return (invoke "as-if-condition" (i32.const 0)))
(assert_return (invoke "as-if-condition" (i32.const -1)))
(assert_return (invoke "as-if-then" (i32.const 0)))
(assert_return (invoke "as-if-then" (i32.const 4)))
(assert_return (invoke "as-if-else" (i32.const 0)))
(assert_return (invoke "as-if-else" (i32.const 3)))

(assert_return (invoke "as-br-last" (i32.const 6)) (i32.const 6))
(assert_return (invoke "as-br-everywhere" (i32.const 7)) (i32.const 7))

(assert_return (invoke "as-br_if-mid" (i32.const 5)) (i32.const 5))
(assert_return (invoke "as-br_if-last" (i32.const 6)) (i32.const 6))
(assert_return (invoke "as-br_if-everywhere" (i32.const 7)) (i32.const 7))

(assert_return (invoke "as-br_table-mid" (i32.const 5)) (i32.const 5))
(assert_return (invoke "as-br_table-last" (i32.const 6)) (i32.const 6))
(assert_return (invoke "as-br_table-everywhere" (i32.const 7)) (i32.const 7))

(assert_return (invoke "as-return-last" (i32.const 6)) (i32.const 6))
(assert_return (invoke "as-return-everywhere" (i32.const 7)) (i32.const 7))

(assert_return (invoke "as-call-mid1" (i32.const 3) (i32.const 1) (i32.const 2)) (i32.const 2))
(assert_return (invoke "as-call-mid2" (i32.const 0) (i32.const 3) (i32.const 1)) (i32.const 2))
(assert_return (invoke "as-call-last" (i32.const 10) (i32.const 9) (i32.const -1)) (i32.const 20))
(assert_return (invoke "as-call-everywhere" (i32.const 2) (i32.const 1) (i32.const 5)) (i32.const -2))

(assert_return (invoke "as-unary-last" (i32.const 30)) (i32.const 1))
(assert_return (invoke "as-unary-everywhere" (i32.const 12)) (i32.const 2))

(assert_return (invoke "as-binary-mid" (i32.const 3)) (i32.const 6))
(assert_return (invoke "as-binary-last" (i32.const 3)) (i32.const 6))
(assert_return (invoke "as-binary-everywhere" (i32.const 3)) (i32.const 6))

(assert_return (invoke "as-test-last" (i32.const 0)) (i32.const 1))
(assert_return (invoke "as-test-everywhere" (i32.const 0)) (i32.const 1))

(assert_return (invoke "as-compare-mid" (i32.const 3)) (i32.const 0))
(assert_return (invoke "as-compare-last" (i32.const 3)) (i32.const 0))
(assert_return (invoke "as-compare-everywhere" (i32.const 3)) (i32.const 1))

(assert_return (invoke "as-grow_memory-last" (i32.const 2)) (i32.const 1))
(assert_return (invoke "as-grow_memory-everywhere" (i32.const 12)) (i32.const 3))

(assert_invalid
  (module (func $type-i32 (result i32) (nop)))
  "type mismatch"
)
(assert_invalid
  (module (func $type-i64 (result i64) (nop)))
  "type mismatch"
)
(assert_invalid
  (module (func $type-f32 (result f32) (nop)))
  "type mismatch"
)
(assert_invalid
  (module (func $type-f64 (result f64) (nop)))
  "type mismatch"
)
;)

;;
;; from return.wast
;;

;; should be (module (func $type-value-void-vs-empty (return (nop))))
(assert_invalid (module (func $type-value-void-vs-empty (return (nop)))) "type mismatch")

;; should be (module (func $type-value-num-vs-empty (return (i32.const 0))))
(assert_invalid (module (func $type-value-num-vs-empty (return (i32.const 0)))) "type mismatch")

;;
;; from func.wast
;;

;; should be (module (func $type-return-void-vs-enpty (return (nop))))
(assert_invalid (module (func $type-return-void-vs-enpty (return (nop)))) "type mismatch")

;; should be (module (func $type-return-num-vs-enpty (return (i32.const 0))))
(assert_invalid (module (func $type-return-num-vs-enpty (return (i32.const 0)))) "type mismatch")

;; this case depends on the signature index in call_indirect derived from the sequence of declarations.
(assert_invalid (module
  ;; Auxiliary definition
  (type $sig (func))
  (func $dummy)

  ;; Syntax

  (func)
  (func (export "f"))
  (func $f)
  (func $h (export "g"))

  (func (local))
  (func (local) (local))
  (func (local i32))
  (func (local $x i32))
  (func (local i32 f64 i64))
  (func (local i32) (local f64))
  (func (local i32 f32) (local $x i64) (local) (local i32 f64))

  (func (param))
  (func (param) (param))
  (func (param i32))
  (func (param $x i32))
  (func (param i32 f64 i64))
  (func (param i32) (param f64))
  (func (param i32 f32) (param $x i64) (param) (param i32 f64))

  (func (result i32) (unreachable))

  (func (type $sig))

  (func $complex
    (param i32 f32) (param $x i64) (param) (param i32)
    (result i32)
    (local f32) (local $y i32) (local i64 i32) (local) (local f64 i32)
    (unreachable) (unreachable)
  )
  (func $complex-sig
    (type $sig)
    (local f32) (local $y i32) (local i64 i32) (local) (local f64 i32)
    (unreachable) (unreachable)
  )


  ;; Typing of locals

  (func (export "local-first-i32") (result i32) (local i32 i32) (get_local 0))
  (func (export "local-first-i64") (result i64) (local i64 i64) (get_local 0))
  (func (export "local-first-f32") (result f32) (local f32 f32) (get_local 0))
  (func (export "local-first-f64") (result f64) (local f64 f64) (get_local 0))
  (func (export "local-second-i32") (result i32) (local i32 i32) (get_local 1))
  (func (export "local-second-i64") (result i64) (local i64 i64) (get_local 1))
  (func (export "local-second-f32") (result f32) (local f32 f32) (get_local 1))
  (func (export "local-second-f64") (result f64) (local f64 f64) (get_local 1))
  (func (export "local-mixed") (result f64)
    (local f32) (local $x i32) (local i64 i32) (local) (local f64 i32)
    (drop (f32.neg (get_local 0)))
    (drop (i32.eqz (get_local 1)))
    (drop (i64.eqz (get_local 2)))
    (drop (i32.eqz (get_local 3)))
    (drop (f64.neg (get_local 4)))
    (drop (i32.eqz (get_local 5)))
    (get_local 4)
  )

  ;; Typing of parameters

  (func (export "param-first-i32") (param i32 i32) (result i32) (get_local 0))
  (func (export "param-first-i64") (param i64 i64) (result i64) (get_local 0))
  (func (export "param-first-f32") (param f32 f32) (result f32) (get_local 0))
  (func (export "param-first-f64") (param f64 f64) (result f64) (get_local 0))
  (func (export "param-second-i32") (param i32 i32) (result i32) (get_local 1))
  (func (export "param-second-i64") (param i64 i64) (result i64) (get_local 1))
  (func (export "param-second-f32") (param f32 f32) (result f32) (get_local 1))
  (func (export "param-second-f64") (param f64 f64) (result f64) (get_local 1))
  (func (export "param-mixed") (param f32 i32) (param) (param $x i64) (param i32 f64 i32)
    (result f64)
    (drop (f32.neg (get_local 0)))
    (drop (i32.eqz (get_local 1)))
    (drop (i64.eqz (get_local 2)))
    (drop (i32.eqz (get_local 3)))
    (drop (f64.neg (get_local 4)))
    (drop (i32.eqz (get_local 5)))
    (get_local 4)
  )

  ;; Typing of result

  (func (export "empty"))
  (func (export "value-void") (call $dummy))
  (func (export "value-i32") (result i32) (i32.const 77))
  (func (export "value-i64") (result i64) (i64.const 7777))
  (func (export "value-f32") (result f32) (f32.const 77.7))
  (func (export "value-f64") (result f64) (f64.const 77.77))
  (func (export "value-block-void") (block (call $dummy) (call $dummy)))
  (func (export "value-block-i32") (result i32) (block (call $dummy) (i32.const 77)))

  (func (export "return-empty") (return))
  (func (export "return-i32") (result i32) (return (i32.const 78)))
  (func (export "return-i64") (result i64) (return (i64.const 7878)))
  (func (export "return-f32") (result f32) (return (f32.const 78.7)))
  (func (export "return-f64") (result f64) (return (f64.const 78.78)))
  (func (export "return-block-i32") (result i32)
    (return (block (call $dummy) (i32.const 77)))
  )

  (func (export "break-empty") (br 0))
  (func (export "break-i32") (result i32) (br 0 (i32.const 79)))
  (func (export "break-i64") (result i64) (br 0 (i64.const 7979)))
  (func (export "break-f32") (result f32) (br 0 (f32.const 79.9)))
  (func (export "break-f64") (result f64) (br 0 (f64.const 79.79)))
  (func (export "break-block-i32") (result i32)
    (br 0 (block (call $dummy) (i32.const 77)))
  )

  (func (export "break-br_if-empty") (param i32)
    (br_if 0 (get_local 0))
  )
  (func (export "break-br_if-num") (param i32) (result i32)
    (br_if 0 (i32.const 50) (get_local 0)) (i32.const 51)
  )

  (func (export "break-br_table-empty") (param i32)
    (br_table 0 0 0 (get_local 0))
  )
  (func (export "break-br_table-num") (param i32) (result i32)
    (br_table 0 0 (i32.const 50) (get_local 0)) (i32.const 51)
  )
  (func (export "break-br_table-nested-empty") (param i32)
    (block (br_table 0 1 0 (get_local 0)))
  )
  (func (export "break-br_table-nested-num") (param i32) (result i32)
    (i32.add
      (block (br_table 0 1 0 (i32.const 50) (get_local 0)) (i32.const 51))
      (i32.const 2)
    )
  )

  ;; Default initialization of locals

  (func (export "init-local-i32") (result i32) (local i32) (get_local 0))
  (func (export "init-local-i64") (result i64) (local i64) (get_local 0))
  (func (export "init-local-f32") (result f32) (local f32) (get_local 0))
  (func (export "init-local-f64") (result f64) (local f64) (get_local 0))


  ;; Desugaring of implicit type signature
  (func $empty-sig-1)  ;; should be assigned type $sig
  (func $complex-sig-1 (param f64 i64 f64 i64 f64 i64 f32 i32))
  (func $empty-sig-2)  ;; should be assigned type $sig
  (func $complex-sig-2 (param f64 i64 f64 i64 f64 i64 f32 i32))
  (func $complex-sig-3 (param f64 i64 f64 i64 f64 i64 f32 i32))

  (type $empty-sig-duplicate (func))
  (type $complex-sig-duplicate (func (param f64 i64 f64 i64 f64 i64 f32 i32)))
  (table anyfunc
    (elem
      $complex-sig-3 $empty-sig-2 $complex-sig-1 $complex-sig-3 $empty-sig-1
    )
  )

  (func (export "signature-explicit-reused")
    (call_indirect $sig (i32.const 1))
    (call_indirect $sig (i32.const 4))
  )

  (func (export "signature-implicit-reused")
    ;; The implicit index 16 in this test depends on the function and
    ;; type definitions, and may need adapting if they change.
    (call_indirect 16
      (f64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 0)
    )
    (call_indirect 16
      (f64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 2)
    )
    (call_indirect 16
      (f64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 3)
    )
  )

  (func (export "signature-explicit-duplicate")
    (call_indirect $empty-sig-duplicate (i32.const 1))
  )

  (func (export "signature-implicit-duplicate")
    (call_indirect $complex-sig-duplicate
      (f64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 0)
    )
  )
) "bad signature index")


;;
;; from globals.wast
;;

(module $m (global (export "a") (mut i32) (i32.const 0)))
(register "m" $m)

;; these should be prohibited for now due to importing mutable globals.

(module (import "m" "a" (global (mut i32))))
(module (global (import "m" "a") (mut i32)))
(module (global (mut f32) (f32.const 0)) (export "a" (global 0)))
(module (global (export "a") (mut f32) (f32.const 0)))
