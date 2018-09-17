(module
  (table $t2 2 anyref)
  (table $t3 3 anyfunc) (elem $t3 (i32.const 1) $dummy)
  (func $dummy)

  (func (export "init") (param $r anyref)
    (table.set $t2 (i32.const 1) (get_local $r))
    (table.set $t3 (i32.const 2) (table.get $t3 (i32.const 1)))
  )

  (func (export "get-anyref") (param $i i32) (result anyref)
    (table.get $t2 (get_local $i))
  )
  (func $f3 (export "get-anyfunc") (param $i i32) (result anyfunc)
    (table.get $t3 (get_local $i))
  )

  (func (export "isnull-anyfunc") (param $i i32) (result i32)
    (ref.isnull (call $f3 (get_local $i)))
  )
)

(invoke "init" (ref.host 1))

(assert_return (invoke "get-anyref" (i32.const 0)) (ref.null))
(assert_return (invoke "get-anyref" (i32.const 1)) (ref.host 1))

(assert_return (invoke "get-anyfunc" (i32.const 0)) (ref.null))
(assert_return (invoke "isnull-anyfunc" (i32.const 1)) (i32.const 0))
(assert_return (invoke "isnull-anyfunc" (i32.const 2)) (i32.const 0))

(assert_trap (invoke "get-anyref" (i32.const 2)) "out of bounds")
(assert_trap (invoke "get-anyfunc" (i32.const 3)) "out of bounds")
(assert_trap (invoke "get-anyref" (i32.const -1)) "out of bounds")
(assert_trap (invoke "get-anyfunc" (i32.const -1)) "out of bounds")
