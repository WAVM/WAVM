;; Test that NaNs without any significand bits set are rejected

(module quote "(func (f32.const +nan:0x1) drop)")
(assert_malformed (module quote "(func (f32.const +nan:0x0))") "NaN significand must be non-zero")
(assert_malformed (module quote "(func (f32.const -nan:0x0))") "NaN significand must be non-zero")
(assert_malformed (module quote "(func (f64.const +nan:0x0))") "NaN significand must be non-zero")
(assert_malformed (module quote "(func (f64.const -nan:0x0))") "NaN significand must be non-zero")

;; some tests omitted from the reference interpreter test suite because the reference interpreter
;; doesn't correctly implement them.
;; from https://github.com/WebAssembly/spec/issues/421

(module
  (func (export "f32.convert_s_i64") (param $x i64) (result f32) (f32.convert_i64_s (local.get $x)))
  (func (export "f32.convert_u_i64") (param $x i64) (result f32) (f32.convert_i64_u (local.get $x)))
)

(assert_return (invoke "f32.convert_u_i64" (i64.const 0x0020000020000001)) (f32.const 0x1.000002p+53))
(assert_return (invoke "f32.convert_u_i64" (i64.const 0x7fffffbfffffffff)) (f32.const 0x1.fffffep+62))
(assert_return (invoke "f32.convert_u_i64" (i64.const 0x8000008000000001)) (f32.const 0x1.000002p+63))
(assert_return (invoke "f32.convert_u_i64" (i64.const 0xfffffe8000000001)) (f32.const 0x1.fffffep+63))
(assert_return (invoke "f32.convert_s_i64" (i64.const 0x8000004000000001)) (f32.const -0x1.fffffep+62))
(assert_return (invoke "f32.convert_s_i64" (i64.const 0xffdfffffdfffffff)) (f32.const -0x1.000002p+53))
(assert_return (invoke "f32.convert_s_i64" (i64.const 0x0020000020000001)) (f32.const 0x1.000002p+53))
(assert_return (invoke "f32.convert_s_i64" (i64.const 0x7fffff4000000001)) (f32.const 0x1.fffffep+62))

;; Test LLVM IRBuilder constant folding bug that translates:
;;   ICmp(t X, bitcast(float Y to t)) to
;; to:
;;   ICmp(bitcast(t X to float), float Y)
;; If the bug is present and not worked around, these modules should cause undefined behavior.
;; Hopefully, that means an error message and an abort().

(module
	(func (result v128)
		(i32x4.eq (v128.const i32x4 1 2 3 4)
		          (f32x4.convert_i32x4_u (v128.const i32x4 5 6 7 8)))
	)
	
	(func (result i32)
		(i32.eq (i32x4.extract_lane 0 (v128.const i32x4 1 2 3 4))
		        (i32x4.extract_lane 0 (f32x4.convert_i32x4_u (v128.const i32x4 5 6 7 8))))
	)
)

(module
	(func (result v128)
		(i8x16.sub_sat_u (v128.const i32x4 9 10 11 12)
		                 (f32x4.convert_i32x4_u (v128.const i32x4 13 14 15 16)))
	)
)

(module	
	(func (result v128)
		(i16x8.sub_sat_u (v128.const i32x4 17 18 19 20)
		                 (f32x4.convert_i32x4_u (v128.const i32x4 21 22 23 24)))
	)
)

(module
	(func (result i32)	
		(i64.gt_u (i64.const 0)
		          (i64.reinterpret_f64 (f64x2.extract_lane 0 (v128.const i32x4 25 26 27 28))))
	)
)

(module
	(func (result i32)
		(i32.eq (i32.const 0x6c2964f6)
		        (i32.reinterpret_f32 (f32x4.extract_lane 0 (v128.const f32x4 1.0 2.0 3.0 4.0))))
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
						(i32.trunc_f32_s (f32x4.extract_lane 0 (v128.const f32x4 nan 0 0 0)))
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
							(local.get $i)
							(i32.trunc_sat_f32_s (f32x4.extract_lane 0 (v128.const f32x4 nan 0 0 0)))
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
							(local.get $i)
							(i32x4.extract_lane 0
								(i32x4.trunc_sat_f32x4_s (v128.const f32x4 nan 0 0 0))
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
		(f32.eq (f32x4.extract_lane 0 (v128.const f32x4 nan nan nan nan))
				(f32x4.extract_lane 0 (v128.const f32x4 nan nan nan nan)))
	)
	
	(func (export "test-misfold-FCmpNE-self") (result i32)
		(f32.ne (f32x4.extract_lane 0 (v128.const f32x4 nan nan nan nan))
				(f32x4.extract_lane 0 (v128.const f32x4 nan nan nan nan)))
	)
)

(assert_trap (invoke "test-misfold-FCmpUNO-self") "invalid conversion to integer")
(assert_return (invoke "test-misfold-FCmpUNO-self-sat" (i32.const 0)) (i32.const 0))
(assert_return (invoke "test-misfold-FCmpUNO-self-sat" (i32.const 1)) (i32.const 1))
(assert_return (invoke "test-misfold-FCmpUNO-self-simd" (i32.const 0)) (i32.const 0))
(assert_return (invoke "test-misfold-FCmpUNO-self-simd" (i32.const 1)) (i32.const 1))
(assert_return (invoke "test-misfold-FCmpEQ-self") (i32.const 0))
(assert_return (invoke "test-misfold-FCmpNE-self") (i32.const 1))


;; This module hangs X86Isel in LLVM 6.0, but not LLVM 7+
(; (module
   (type $0 (func (param i64 f64 i64 f32 i32)))
   (type $1 (func (param v128)))
   (type $2 (func (param v128 i32 f64 f32 i32)))
   (memory $4  1024 65536 shared)
   (table $3  1024 4294967296 shared funcref)
   (global $5 (mut v128) (v128.const i32x4 0 1 2 3))
   (global $7  (mut i32) (i32.const 1998782039))
   (global $8  (mut i64) (i64.const 2173604684453082686))
   (global $9  i32 (i32.const -2007124476))
   (global $10  i64 (i64.const -2278033257057771354))

   (func $13 (type $2)
      (param $0 v128)
      (param $1 i32)
      (param $2 f64)
      (param $3 f32)
      (param $4 i32)
      (local $5 i64)
      (local $6 i64)
      (local $7 v128)
      (local $8 f32)
      (local $9 f32)
      (local $10 v128)
      v128.const i32x4 1 1 1 1
      i32x4.trunc_sat_f32x4_s
      v128.const i32x4 1 1 1 1
      local.get $1
      local.get $8
      f64.promote/f32
      global.get $5
      f64.const 0x1.8dc8cb3e84082p+523
      f64.ceil
      local.get $7
      local.set $10
      i32.trunc_f64_s
      i8x16.splat
      f32x4.lt
      local.set $10
      local.tee $2
      f64.ceil
      local.get $10
      f32x4.convert_i32x4_s
      local.get $5
      global.get $10
      i64.rotr
      f64.convert_i64_u
      i32.trunc_f64_s
      i32.clz
      i64.load8_u offset=3353026027
      (v128.store (i32.const 10000) (local.get $10))
      local.get $9
      local.set $9
      local.set $6
      global.set $5
      local.set $2
      i32x4.replace_lane 2
      i16x8.le_s
      drop
   )
) ;)

;; some other regression tests for bugs found by fuzz testing

(assert_invalid
	(module binary
		"\00\61\73\6d\01\00\00\00\01\04\01\60\00\00\03\02"
		"\01\00\0a\07\01\05\00\00\0b\1b\6e"
		)
	"error validating binary module: Expected non-empty control stack in select"
	)

(assert_malformed
	(module binary
		"\00\61\73\6d\01\00\00\00\01\84\00\01\60\00\00\03"
		"\84\00\03\00\00\00\0a\aa\00\03\87\80\80\80\00\02"
		"\02\40\0c\00\0b\0b\89\80\80\80\00\00\03\40\41\01"
		"\0d\00\0b\0b\8a\80\80\80\00\00\03\40\41\01\0d\00"
		"\0f\0b\0b"
		)
	"error validating binary module: invalid value type (64)"
	)

(assert_invalid
	(module binary
		"\00\61\73\6d\01\00\00\00\01\0f\01\60\0a\7c\7c\7c"
		"\7c\7c\7c\7c\7c\7c\7c\01\7c\03\02\01\00\0a\31\01"
		"\2f\00\20\00\20\01\20\02\20\03\20\02\20\03\20\04"
		"\20\05\20\06\00\00\00\00\20\09\10\00\00\10\00\0b"
		"\00\0e\04\6e\61\6d\64\01\07\01\01\04\6d\27\69\6e"
		)
	"error validating binary module: Expected non-empty control stack in unreachable"
	)

(assert_malformed
	(module binary
		"\00\61\73\6d\01\00\00\00\01\85\00\01\60\00\01\7f"
		"\03\82\00\01\00\05\84\00\01\29\00\00\0a\ae\00\01"
		"\2c\01\27\2e\03\7f\03\40\3f\00\41\80\80\01\48\04"
		"\40\20\00\28\00\00\04\40\20\04\41\80\87\00\2c\00"
		"\00\6a\03\7f\03\40\2f\00\41\41\2b\0b\0b"
		)
	"error validating binary module: invalid value type (210)"
	)

(assert_malformed
	(module binary
		"\00\61\73\6d\01\00\00\00\01\88\80\80\80\00\01\60"
		"\03\7f\7f\7f\01\7f\03\82\80\80\80\00\01\00\05\84"
		"\80\80\80\00\01\01\10\20\06\86\80\80\80\00\01\7f"
		"\00\41\00\0b\07\88\80\80\80\00\01\04\6d\61\69\6e"
		"\00\00\0a\b0\85\80\80\00\01\ad\05\01\00\00\00\00"
		"\0b\05\43\00\00\00\00\0b\05\43\00\00\00\00\0b\05"
		"\43\00\00\00\00\0b\21\07\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\41\8e\9d\ba\f4\78\41\00\41\00\10\00\b2\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\0b\0b\0b\60\41\00\41\00\10\00"
		"\b2\43\8e\8e\8e\8e\43\8e\00\0e\7d\41\ec\de\01\41"
		"\00\41\00\10\00\1a\41\00\41\8e\8c\8c\ab\07\75\41"
		"\00\0d\00\b2\43\73\74\00\00\60\41\00\41\00\10\00"
		"\b2\43\6f\6d\3a\3a\60\41\00\41\00\10\00\b2\20\07"
		"\a8\04\7d\20\07\a8\04\7d\20\07\a8\04\7d\20\07\a8"
		"\04\7d\20\07\a8\04\7d\20\07\a8\04\7d\02\7f\41\00"
		"\41\e3\00\6a\0b\41\f5\e6\d1\04\74\04\7d\43\00\00"
		"\00\00\05\43\00\00\00\00\0b\05\43\00\00\00\00\0b"
		"\05\43\00\00\00\00\0b\05\43\00\00\00\00\0b\05\43"
		"\00\00\00\00\0b\05\43\00\00\00\00\0b\05\43\00\00"
		"\00\00\0b\21\07\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\41\8e\9d"
		"\ba\f4\78\41\00\41\00\10\00\b2\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\60\41\00\41\00\10\00\b2\43\8e"
		"\8e\8e\8e\43\8e\00\00\00\60\41\00\41\00\10\00\b2"
		"\60\41\00\41\00\10\00\b2\20\00\2f\01\f4\de\b5\d3"
		"\03\20\04\a8\3a\00\e1\c8\81\70\20\07\a8\2e\01\ed"
		"\f4\e8\01\20\07\a8\77\20\07\a8\0d\00\b2\22\0e\a8"
		"\20\07\a8\20\07\a8\10\00\b2\60\41\ba\f4\e8\01\41"
		"\00\10\00\b2\20\07\a8\04\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03"
		"\7d\03\7d\43\6d\6d\6d\6d\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\05\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d\03\7d"
		"\43\6d\6d\8e\8e\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b\0b"
		"\0b\0b\0b\60\0b"
		)
	"error validating binary module: Expected non-empty control stack in else"
	)

(assert_malformed
	(module binary
		"\00\61\73\6d\01\00\00\00\01\88\80\80\80\00\01\60"
		"\03\7f\7f\7f\01\7f\03\82\80\80\80\00\01\00\05\84"
		"\80\80\00\01\2f\10\20\06\86\80\80\80\00\01\7f\00"
		"\41\00\0b\07\88\80\80\80\00\01\04\65\61\69\6e\00"
		"\00\0a\9a\81\80\80\00\01\97\01\0b\04\7f\41\00\05"
		"\41\00\0b\04\7f\41\00\05\41\00\0b\04\7f\41\00\05"
		"\41\00\0b\fb\04\7f\41\00\05\41\00\0b\04\7f\41\00"
		"\05\41\00\0b\04\7f\41\00\05\41\00\0b\04\7f\41\00"
		"\41\00\0b\04\7f\41\00\05\41\00\0b\04\7f\41\00\05"
		"\41\00\0b\04\7f\41\00\05\41\00\ff\ff\ff\ff\0b\04"
		"\7f\41\00\05\41\00\0b\04\7f\41\00\05\41\00\0b\04"
		"\7f\41\00\05\41\00\0b\04\7f\41\00\05\41\00\0b\04"
		"\7f\41\00\05\41\00\0b\04\7f\41\00\05\41\00\0b\04"
		"\7f\41\00\05\41\00\0b\04\7f\41\00\05\41\00\0b\41"
		"\00\0d\00\b7\62\0b"
		)
	"error validating binary module: invalid value type (0)"
	)

(assert_malformed
	(module binary
		"\00\61\73\6d\01\00\00\00\01\04\01\60\00\00\03\02"
		"\01\00"
		)
	"error validating binary module: Serialized module contained function declarations, but no corresponding function definition section"
	)

(assert_malformed
	(module quote
		"\80\61\73\6d\01\00\00\00\02\08\01\01\6d\01\66\03"
		"\7f\00"
		)
	"expected '('"
	)

(assert_malformed
	(module quote
		"\00"
		)
	"expected '('"
	)

;; Test for the bug fixed by https://reviews.llvm.org/D57871
(module
	(func (export "testLLVM-D57871") (param $a v128) (result v128)
		(i16x8.eq (f32x4.convert_i32x4_s (local.get $a))
		          (v128.const i16x8 0 0 0 0 0 0 0 0))
	)
)
(assert_return (invoke "testLLVM-D57871" (v128.const i32x4 0 0 0 0))     (v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1))
(assert_return (invoke "testLLVM-D57871" (v128.const i32x4 1 1 1 1))     (v128.const i16x8 -1 0 -1 0 -1 0 -1 0))


;; Test for the bug fixed by https://reviews.llvm.org/D58049
(module $m (func (export "f")))
(register "m" $m)
(module
	(elem declare $f)
	(func $f (import "m" "f"))
	(func (export "testLLVM-D58049") (param $x i32) (result i32)
		(i32.or
			(i32.shr_u (ref.is_null (ref.func $f)) (local.get $x))
			(i32.shl   (ref.is_null (ref.func $f)) (i32.const 1))
			)
	)
)
(assert_return (invoke "testLLVM-D58049" (i32.const 0)) (i32.const 0))


;; Test for the bug reported here: https://bugs.llvm.org/show_bug.cgi?id=40793
(module
	(type $2 (func (param funcref v128)))
	(memory $4  1024 65536 shared)
	;;(global $11  (mut v128) (v128.const i32x4 0xb3ce8331 0x45c113c5 0xe339424d 0x743a15f1))
	(global $11 (mut i32) (i32.const 0xb3ce))

	(func $15 (type $2)
		(param $0 funcref)
		(param $1 v128)
		v128.const i32x4 0xc6046244 0xbdc49765 0x7fddc966 0x48629ec2
		global.get $11
		i16x8.shr_u
		f64x2.abs
		i16x8.extract_lane_s 5
		i64.load8_u
		br 0
	)
)


;; Test V128 as a loop parameter, in particular that LLVM's "interpreted" V128 types (e.g. f64x2)
;; are correctly coerced to the canonical type the loop parameter PHI expects.
(module
	(func (export "loop") (param $x f64) (result v128)
		local.get $x
		f64x2.splat
		loop $loop (param v128) (result v128)
			f64x2.neg
		end
	)
)
(assert_return (invoke "loop" (f64.const +1)) (v128.const f64x2 -1 -1))
(assert_return (invoke "loop" (f64.const -1)) (v128.const f64x2 +1 +1))

;; Test V128 as an if parameter, in particular that LLVM's "interpreted" V128 types (e.g. f64x2)
;; are correctly coerced to the canonical type the if parameter PHI expects.

(module
	(func (export "if-then") (param $x f64) (param $y i32) (result v128)
		local.get $x
		f64x2.splat
		local.get $y
		if (param v128) (result v128)
			f64x2.neg
		end
	)
)
(assert_return (invoke "if-then" (f64.const +1) (i32.const 0)) (v128.const f64x2 +1 +1))
(assert_return (invoke "if-then" (f64.const +1) (i32.const 1)) (v128.const f64x2 -1 -1))
(assert_return (invoke "if-then" (f64.const -1) (i32.const 0)) (v128.const f64x2 -1 -1))
(assert_return (invoke "if-then" (f64.const -1) (i32.const 1)) (v128.const f64x2 +1 +1))

(module
	(func (export "if-else") (param $x f64) (param $y i32) (result v128)
		local.get $x
		f64x2.splat
		local.get $y
		if (param v128) (result v128)
			f64x2.neg
		else
			f64x2.abs
		end
	)
)
(assert_return (invoke "if-else" (f64.const +1) (i32.const 0)) (v128.const f64x2 +1 +1))
(assert_return (invoke "if-else" (f64.const +1) (i32.const 1)) (v128.const f64x2 -1 -1))
(assert_return (invoke "if-else" (f64.const -1) (i32.const 0)) (v128.const f64x2 +1 +1))
(assert_return (invoke "if-else" (f64.const -1) (i32.const 1)) (v128.const f64x2 +1 +1))

;; Test that this LLVM bug isn't triggered: https://bugs.llvm.org/show_bug.cgi?id=43514
(module
	(memory 1 1 shared)

	(func (param $2 i64)
		loop $loop
			(local.set $2 (i64.rem_s
				(local.get $2)
				(i64.load8_u offset=0 (i32.const 0))))
			(br_if $loop (i32.const 0))
			return
		end ;; $loop
		unreachable
	)
)

;; Test that this LLVM bug isn't triggered: https://bugs.llvm.org/show_bug.cgi?id=43510
(module
	(func $8 (result v128)
		(result v128)
		(local $0 i32)
		(local $1 v128)
		v128.const i32x4 0xadbc5ab3 0x03c8f2fd 0x84d96029 0xf065dbc1
		v128.const i32x4 0x9a8f38a3 0x09c641b2 0xc66a89ad 0x9be68049
		f64x2.max
		local.set $1
		loop $loop
			br $loop
		end ;; $loop
		unreachable
	)
)

;; Test for this issue: https://github.com/WebAssembly/spec/issues/1224
(module
 (func (export "trunc") (result i32)
   (i32.trunc_f64_s
    (f64.const -2147483648.1)
  )
 )
)
(assert_return (invoke "trunc") (i32.const -2147483648))