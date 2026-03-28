(module
	;; Test that random_get with an out-of-bounds pointer returns EFAULT (21).

	(import "wasi_unstable" "proc_exit" (func $proc_exit (param i32)))
	(import "wasi_unstable" "random_get" (func $random_get (param i32 i32) (result i32)))

	(memory (export "memory") 1)

	(func (export "_start")
		;; random_get(65536, 1) — pointer is one byte past end of memory
		i32.const 65536
		i32.const 1
		call $random_get

		;; Expect EFAULT (21)
		i32.const 21
		i32.ne
		if
			i32.const 1
			call $proc_exit
		end

		i32.const 0
		call $proc_exit
	)
)
