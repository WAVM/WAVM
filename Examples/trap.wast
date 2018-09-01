(module
  (memory 1)
  (export "main" (func $main))

  (func $func
	(i32.store (i32.const -1) (i32.const -1))
	)

  (func $main (result i32)
    (call $func)
	i32.const 0
  )
)
