;; Test LLVM IRBuilder constant folding bug that translates:
;;   ICmp(t X, bitcast(float Y to t)) to
;; to:
;;   ICmp(bitcast(t X to float), float Y)
;; If the bug is present and not worked around, these modules should cause undefined behavior.
;; Hopefully, that means an error message and an abort().

(module
	(func (result v128)
		(i32x4.eq (v128.const i32 1 2 3 4)
		          (f32x4.convert_u/i32x4 (v128.const i32 5 6 7 8)))
	)
	
	(func (result i32)
		(i32.eq (i32x4.extract_lane 0 (v128.const i32 1 2 3 4))
		        (i32x4.extract_lane 0 (f32x4.convert_u/i32x4 (v128.const i32 5 6 7 8))))
	)
)

(module
	(func (result v128)
		(i8x16.sub_saturate_u (v128.const i32 9 10 11 12)
		                      (f32x4.convert_u/i32x4 (v128.const i32 13 14 15 16)))
	)
)

(module	
	(func (result v128)
		(i16x8.sub_saturate_u (v128.const i32 17 18 19 20)
		                      (f32x4.convert_u/i32x4 (v128.const i32 21 22 23 24)))
	)
)

(module
	(func (result i32)	
		(i64.gt_u (i64.const 0)
		          (i64.reinterpret/f64 (f64x2.extract_lane 0 (v128.const i32 25 26 27 28))))
	)
)

(module
	(func (result i32)
		(i32.eq (i32.const 0x6c2964f6)
		        (i32.reinterpret/f32 (f32x4.extract_lane 0 (v128.const f32 1.0 2.0 3.0 4.0))))
	)
)

;; Test LLVM IRBuilder constant folding bug that misfolds FCmpUNO(X, X) to false.

(module
	(memory 1 1 shared)

	(func (export "test-misfold-FCmpUNO-self") (result i32)
		(block $0
			(block $1
				(block $default
					;; If the table index is inferred to be undef, LLVM will branch to an
					;; arbitrary successor of the basic block.
					(br_table
						$0 $1
						$default
						(i32.trunc_s/f32 (f32x4.extract_lane 0 (v128.const f32 nan 0 0 0)))
					)
				)
				(return (i32.const 100))
			)
			(return (i32.const 1))
		)
		(return (i32.const 0))
	)
	
	(func (export "test-misfold-FCmpUNO-self-sat") (param $i i32) (result i32)
		(block $0
			(block $1
				(block $default
					;; If the table index is inferred to be undef, LLVM will branch to an
					;; arbitrary successor of the basic block.
					(br_table
						$0 $1
						$default
						(i32.add
							(get_local $i)
							(i32.trunc_s:sat/f32 (f32x4.extract_lane 0 (v128.const f32 nan 0 0 0)))
						)
					)
				)
				(return (i32.const 100))
			)
			(return (i32.const 1))
		)
		(return (i32.const 0))
	)
	
	(func (export "test-misfold-FCmpUNO-self-simd") (param $i i32) (result i32)
		(block $0
			(block $1
				(block $default
					;; If the table index is inferred to be undef, LLVM will branch to an
					;; arbitrary successor of the basic block.
					(br_table
						$0 $1
						$default
						(i32.add
							(get_local $i)
							(i32x4.extract_lane 0
								(i32x4.trunc_s:sat/f32x4 (v128.const f32 nan 0 0 0))
							)
						)
					)
				)
				(return (i32.const 100))
			)
			(return (i32.const 1))
		)
		(return (i32.const 0))
	)
	
	(func (export "test-misfold-FCmpEQ-self") (result i32)
		(f32.eq (f32x4.extract_lane 0 (v128.const f32 nan nan nan nan))
				(f32x4.extract_lane 0 (v128.const f32 nan nan nan nan)))
	)
	
	(func (export "test-misfold-FCmpNE-self") (result i32)
		(f32.ne (f32x4.extract_lane 0 (v128.const f32 nan nan nan nan))
				(f32x4.extract_lane 0 (v128.const f32 nan nan nan nan)))
	)
)

(assert_trap (invoke "test-misfold-FCmpUNO-self") "invalid conversion to integer")
(assert_return (invoke "test-misfold-FCmpUNO-self-sat" (i32.const 0)) (i32.const 0))
(assert_return (invoke "test-misfold-FCmpUNO-self-sat" (i32.const 1)) (i32.const 1))
(assert_return (invoke "test-misfold-FCmpUNO-self-simd" (i32.const 0)) (i32.const 0))
(assert_return (invoke "test-misfold-FCmpUNO-self-simd" (i32.const 1)) (i32.const 1))
(assert_return (invoke "test-misfold-FCmpEQ-self") (i32.const 0))
(assert_return (invoke "test-misfold-FCmpNE-self") (i32.const 1))