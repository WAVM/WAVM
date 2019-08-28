(module
	(memory 1)
	(export "main" (func $main))

	(func $store (param $value i32)
		i32.const 65536        ;; func+0
		local.get $value       ;; func+1
		i32.store              ;; func+2
	)

	(func $main (result i32)
		i32.const 0            ;; main+0
		call $store            ;; main+1
		i32.const 0            ;; main+2
	)
)
