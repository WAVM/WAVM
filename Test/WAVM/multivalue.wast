(module
	(func $multi-return-syntax-1 (result i32 i32) unreachable)
	(func $multi-return-syntax-2 (result i32) (result i32) unreachable)

	(func $return2xI32 (export "return2xI32") (result i32 i32)
		i32.const 1
		i32.const 2
	)

	(func $return3xI32 (export "return3xI32") (result i32 i32 i32)
		call $return2xI32
		i32.const 3
	)

	(func $returnI32andF32 (export "returnI32andF32") (result i32 f32)
		i32.const 4
		f32.const 5.0
	)

	(func $block-param (export "block-param") (param $x i32) (result i32)
		get_local $x
		block (param i32) (result i32)
			i32.const 1
			i32.add
		end
	)
	
	(func $loop-param (export "loop-param") (param $x i32) (result i64)
		(local $shouldRepeat i32)
		(set_local $shouldRepeat (i32.const 1))
		get_local $x
		loop $loop (param i32) (result i64)
			i32.const 1
			i32.add
			get_local $shouldRepeat
			(set_local $shouldRepeat (i32.const 0))
			br_if $loop
			i64.extend_u/i32
		end
	)
	
	(func $if-param (export "if-param") (param $condition i32) (result i64)
		get_local $condition
		get_local $condition
		if (param i32)
			i64.extend_u/i32
			i64.const 7
			i64.add
			return
		end
		i64.const 11
		return
	)
	
	(func $if-else-param (export "if-else-param") (param $condition i32) (result i64)
		get_local $condition
		get_local $condition
		if (param i32) (result i64)
			i64.extend_u/i32
			i64.const 13
			i64.add
		else
			i64.extend_u/i32
			i64.const 17
			i64.add
		end
	)
	
	(func $try-param (export "try-param") (param $x i32) (result f64)
		get_local $x
		try (param i32) (result f64)
			f64.convert_u/i32
		catch_all
			f64.const 23.0
		end
	)

	(func $loop-2result (export "loop-2result") (result i64 f64)
		loop $loop (result i64 f64)
			i64.const 1
			f64.const 2.0
		end
	)
)

(assert_return (invoke "return2xI32") (i32.const 1) (i32.const 2))
(assert_return (invoke "return3xI32") (i32.const 1) (i32.const 2) (i32.const 3))
(assert_return (invoke "returnI32andF32") (i32.const 4) (f32.const 5.0))

(assert_return (invoke "block-param" (i32.const 5)) (i32.const 6))
(assert_return (invoke "loop-param" (i32.const 6)) (i64.const 8))
(assert_return (invoke "if-param" (i32.const 0)) (i64.const 11))
(assert_return (invoke "if-param" (i32.const 1)) (i64.const 8))
(assert_return (invoke "if-else-param" (i32.const 0)) (i64.const 17))
(assert_return (invoke "if-else-param" (i32.const 1)) (i64.const 14))
(assert_return (invoke "try-param" (i32.const 19)) (f64.const 19))

(assert_return (invoke "loop-2result") (i64.const 1) (f64.const 2.0))