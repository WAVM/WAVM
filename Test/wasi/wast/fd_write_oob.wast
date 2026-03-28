(module
	;; Test that fd_write with an out-of-bounds iovec pointer returns EFAULT (21).

	(import "wasi_unstable" "proc_exit" (func $proc_exit (param i32)))
	(import "wasi_unstable" "fd_write" (func $fd_write (param i32 i32 i32 i32) (result i32)))

	(memory (export "memory") 1)

	(func (export "_start")
		;; fd_write(1, 65536, 1, 0) — iovec pointer is past end of memory
		i32.const 1      ;; fd = stdout
		i32.const 65536  ;; iovs address (OOB)
		i32.const 1      ;; iovs count
		i32.const 0      ;; nwritten address (valid)
		call $fd_write

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
