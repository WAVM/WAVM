(module $Mem
  (memory (export "shared") 1 1 shared)
)

(thread $T1 (shared (module $Mem))
  (register "mem" $Mem)
  (module
    (memory (import "mem" "shared") 1 10 shared)
    (func (export "run")
      (i32.store (i32.const 0) (i32.const 42))
    )
  )
  (invoke "run")
)

(thread $T2 (shared (module $Mem))
  (register "mem" $Mem)
  (module
    (memory (import "mem" "shared") 1 1 shared)
    (func (export "run") (result i32)
      (i32.load (i32.const 0))
    )
  )
  (assert_return (invoke "run") (either (i32.const 0) (i32.const 42)))
)

(wait $T1)
(wait $T2)
