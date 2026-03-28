(module
  (func $recurse (export "recurse") (param i32) (result i32)
    (local i32 i32 i32 i32)
    (local.set 1 (i32.add (local.get 0) (i32.const 1)))
    (local.set 2 (i32.add (local.get 0) (i32.const 2)))
    (local.set 3 (i32.add (local.get 0) (i32.const 3)))
    (local.set 4 (i32.add (local.get 0) (i32.const 4)))
    ;; Use all locals after the call so they can't be optimized away
    (i32.add
      (i32.add
        (i32.add
          (call $recurse (local.get 1))
          (local.get 2))
        (local.get 3))
      (local.get 4))
  )
  (func $tail_recurse (export "tail_recurse") (call $tail_recurse))
)

(assert_trap (invoke "recurse" (i32.const 0)) "call stack exhausted")
(assert_trap (invoke "tail_recurse") "call stack exhausted")