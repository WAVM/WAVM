(module

	(import "wasi_unstable" "proc_exit" (func $wasi_proc_exit (param i32)))

	(memory (export "memory") 1)

	(func $store (param $value i32)
		i32.const 65536        ;; func+0
		local.get $value       ;; func+1
		i32.store              ;; func+2
	)

	(func $main (export "_start")
		i32.const 0            ;; main+0
		call $store            ;; main+1
		i32.const 0            ;; main+2
		call $wasi_proc_exit   ;; main+3
	)
)
