;;
;; From float_exprs.wast
;;

;; test that various operations with no effect on non-NaN floats have the correct effect when operating on a NaN.
(module
  (func (export "no_fold_promote_demote") (param $x i32) (result i32)
    (i32.and (i32.reinterpret/f32 (f32.demote/f64 (f64.promote/f32 (f32.reinterpret/i32 (get_local $x)))))
             (i32.const 0x7fc00000)))
)
(assert_return (invoke "no_fold_promote_demote" (i32.const 0x7fa00000)) (i32.const 0x7f800000))
