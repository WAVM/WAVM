(module
  (func (export "try_with_divide_by_zero") (result i32)
    try (result i32)
      i32.const 0
      i32.const 0
      i32.div_s
      drop
      i32.const 7
    catch_all
      i32.const 8
    end
    )
)

(assert_trap (invoke "try_with_divide_by_zero") "integer divide by zero")
