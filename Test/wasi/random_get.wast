(module
	(func $wasi_random_get (import "wasi_unstable" "random_get") (param i32 i32) (result i32))

	(memory (export "memory") 1 1)

	(func (export "_start")
		(drop (call $wasi_random_get (i32.const 0) (i32.const 512)))
	)
)