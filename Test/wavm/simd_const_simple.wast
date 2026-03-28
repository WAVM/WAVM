(module
  (func (export "return_v128") (result v128)
    (v128.const i32x4 0 1 2 3)
  )
)

(assert_return (invoke "return_v128") (v128.const i32x4 0 1 2 3))
