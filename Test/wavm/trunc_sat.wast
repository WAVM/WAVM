(module
  (import "spectest" "print_f32" (func $spectest_print_f32 (param f32)))
  (import "spectest" "print_f64" (func $spectest_print_f64 (param f64)))
  (import "spectest" "print_i32" (func $spectest_print_i32 (param i32)))
  (import "spectest" "print_i64" (func $spectest_print_i64 (param i64)))

  (table 4 funcref)
  (elem (i32.const 0)
    (; 0 ;) $i32_trunc_s_sat_f32
    (; 1 ;) $emulated_i32_trunc_s_sat_f32
    (; 2 ;) $i32_trunc_u_sat_f32
    (; 3 ;) $emulated_i32_trunc_u_sat_f32
    )

  (type $f32_to_i32 (func (param f32) (result i32)))

  (func $i32_trunc_s_sat_f32 (export "i32.trunc_sat_f32_s") (param $a f32) (result i32) (i32.trunc_sat_f32_s (local.get $a)))
  (func $i32_trunc_u_sat_f32 (export "i32.trunc_sat_f32_u") (param $a f32) (result i32) (i32.trunc_sat_f32_u (local.get $a)))
  (func $i32_trunc_s_sat_f64 (export "i32.trunc_sat_f64_s") (param $a f64) (result i32) (i32.trunc_sat_f64_s (local.get $a)))
  (func $i32_trunc_u_sat_f64 (export "i32.trunc_sat_f64_u") (param $a f64) (result i32) (i32.trunc_sat_f64_u (local.get $a)))
  (func $i64_trunc_s_sat_f32 (export "i64.trunc_sat_f32_s") (param $a f32) (result i64) (i64.trunc_sat_f32_s (local.get $a)))
  (func $i64_trunc_u_sat_f32 (export "i64.trunc_sat_f32_u") (param $a f32) (result i64) (i64.trunc_sat_f32_u (local.get $a)))
  (func $i64_trunc_s_sat_f64 (export "i64.trunc_sat_f64_s") (param $a f64) (result i64) (i64.trunc_sat_f64_s (local.get $a)))
  (func $i64_trunc_u_sat_f64 (export "i64.trunc_sat_f64_u") (param $a f64) (result i64) (i64.trunc_sat_f64_u (local.get $a)))

  (func $emulated_i32_trunc_s_sat_f32 (param $f f32) (result i32)
    ;; return 0 if the input is a NaN
    (br_if 0 (i32.const 0) (f32.ne (local.get $f) (local.get $f)))

    ;; return INT32_MIN if the input is less than it.
    (br_if 0 (i32.const -2147483648) (f32.le (local.get $f) (f32.const -2147483648.0)))

    ;; return INT32_MAX if the input is greater than it.
    (br_if 0 (i32.const 2147483647) (f32.ge (local.get $f) (f32.const 2147483647.0)))

    ;; otherwise, return the result of the non-saturating trunc instruction.
    (return (i32.trunc_f32_s (local.get $f)))
    )
    
  (func $emulated_i32_trunc_u_sat_f32 (param $f f32) (result i32)
    ;; return 0 if the input is a NaN
    (br_if 0 (i32.const 0) (f32.ne (local.get $f) (local.get $f)))

    ;; return UINT32_MIN if the input is less than it.
    (br_if 0 (i32.const 0) (f32.le (local.get $f) (f32.const 0.0)))

    ;; return UINT32_MAX if the input is greater than it.
    (br_if 0 (i32.const 4294967295) (f32.ge (local.get $f) (f32.const 4294967295.0)))

    ;; otherwise, return the result of the non-saturating trunc instruction.
    (return (i32.trunc_f32_u (local.get $f)))
    )

  (func $test_all_f32_to_i32 (param $funcA i32) (param $funcB i32) (result i32)
    ;; loop over all 32-bit values and compare the result of the emulated instruction with the actual instruction.
    (local $i i32)
    loop $loop
        (i32.ne
            (call_indirect (type $f32_to_i32) (f32.reinterpret_i32 (local.get $i)) (local.get $funcA))
            (call_indirect (type $f32_to_i32) (f32.reinterpret_i32 (local.get $i)) (local.get $funcB))
            )
        if
            (call $spectest_print_f32 (f32.reinterpret_i32 (local.get $i)))
            (call $spectest_print_i32 (call_indirect (type $f32_to_i32) (f32.reinterpret_i32 (local.get $i)) (local.get $funcA)))
            (call $spectest_print_i32 (call_indirect (type $f32_to_i32) (f32.reinterpret_i32 (local.get $i)) (local.get $funcB)))
            (return (i32.const 0))
        end
        (local.set $i (i32.add (i32.const 1) (local.get $i)))
        (br_if $loop
            (i32.ne (local.get $i) (i32.const 0)))
    end

    (return (i32.const 1))
    )

  (func (export "test all i32.trunc_sat_f32_s") (result i32)
    (return (call $test_all_f32_to_i32 (i32.const 0) (i32.const 1))))
        
  (func (export "test all i32.trunc_sat_f32_u") (result i32)
    (return (call $test_all_f32_to_i32 (i32.const 2) (i32.const 3))))
)

;; i32.trunc_sat_f32_s

;;(assert_return (invoke "test all i32.trunc_sat_f32_s") (i32.const 1))

(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const 0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const 0x1p-149)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -0x1p-149)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const 1.0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const 0x1.19999ap+0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const 1.5)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -1.0)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -0x1.19999ap+0)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -1.5)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -1.9)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -2.0)) (i32.const -2))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const 2147483520.0)) (i32.const 2147483520))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -2147483648.0)) (i32.const -2147483648))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const 2147483648.0)) (i32.const 2147483647))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -2147483904.0)) (i32.const -2147483648))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const inf)) (i32.const 2147483647))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -inf)) (i32.const -2147483648))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const nan:0x200000)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_s" (f32.const -nan:0x200000)) (i32.const 0))

;; i32.trunc_sat_f32_u

;;(assert_return (invoke "test all i32.trunc_sat_f32_u") (i32.const 1))

(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 0x1p-149)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -0x1p-149)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 1.0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 0x1.19999ap+0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 1.5)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 1.9)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 2.0)) (i32.const 2))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 2147483648)) (i32.const -2147483648)) ;; 0x1.00000p+31 -> 8000 0000
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 4294967040.0)) (i32.const 4294967040))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 4294967295.0)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -0x1.ccccccp-1)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -0x1.fffffep-1)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const 4294967296.0)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -1.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const inf)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -inf)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const nan:0x200000)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f32_u" (f32.const -nan:0x200000)) (i32.const 0))


(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const 0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const 0x0.0000000000001p-1022)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -0x0.0000000000001p-1022)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const 1.0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const 0x1.199999999999ap+0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const 1.5)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -1.0)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -0x1.199999999999ap+0)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -1.5)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -1.9)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -2.0)) (i32.const -2))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const 2147483647.0)) (i32.const 2147483647))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -2147483648.0)) (i32.const -2147483648))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const 2147483648.0)) (i32.const 2147483647))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -2147483649.0)) (i32.const -2147483648))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const inf)) (i32.const 2147483647))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -inf)) (i32.const -2147483648))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const nan:0x4000000000000)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_s" (f64.const -nan:0x4000000000000)) (i32.const 0))

(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -0.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 0x0.0000000000001p-1022)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -0x0.0000000000001p-1022)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 1.0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 0x1.199999999999ap+0)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 1.5)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 1.9)) (i32.const 1))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 2.0)) (i32.const 2))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 2147483648)) (i32.const -2147483648)) ;; 0x1.00000p+31 -> 8000 0000
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 4294967295.0)) (i32.const -1))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -0x1.ccccccccccccdp-1)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -0x1.fffffffffffffp-1)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 1e8)) (i32.const 100000000))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 4294967296.0)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -1.0)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 1e16)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 1e30)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const 9223372036854775808)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const inf)) (i32.const 4294967295))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -inf)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const nan:0x4000000000000)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -nan)) (i32.const 0))
(assert_return (invoke "i32.trunc_sat_f64_u" (f64.const -nan:0x4000000000000)) (i32.const 0))

(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 0x1p-149)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -0x1p-149)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 1.0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 0x1.19999ap+0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 1.5)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -1.0)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -0x1.19999ap+0)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -1.5)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -1.9)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -2.0)) (i64.const -2))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 4294967296)) (i64.const 4294967296)) ;; 0x1.00000p+32 -> 1 0000 0000
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -4294967296)) (i64.const -4294967296)) ;; -0x1.00000p+32 -> ffff ffff 0000 0000
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 9223371487098961920.0)) (i64.const 9223371487098961920))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -9223372036854775808.0)) (i64.const -9223372036854775808))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const 9223372036854775808.0)) (i64.const 9223372036854775807))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -9223373136366403584.0)) (i64.const -9223372036854775808))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const inf)) (i64.const 9223372036854775807))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -inf)) (i64.const -9223372036854775808))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const nan:0x200000)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_s" (f32.const -nan:0x200000)) (i64.const 0))

(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 0x1p-149)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -0x1p-149)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 1.0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 0x1.19999ap+0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 1.5)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 4294967296)) (i64.const 4294967296))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 18446742974197923840.0)) (i64.const -1099511627776))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -0x1.ccccccp-1)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -0x1.fffffep-1)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const 18446744073709551616.0)) (i64.const 18446744073709551615))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -1.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const inf)) (i64.const 18446744073709551615))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -inf)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const nan:0x200000)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f32_u" (f32.const -nan:0x200000)) (i64.const 0))

(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 0x0.0000000000001p-1022)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -0x0.0000000000001p-1022)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 1.0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 0x1.199999999999ap+0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 1.5)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -1.0)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -0x1.199999999999ap+0)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -1.5)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -1.9)) (i64.const -1))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -2.0)) (i64.const -2))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 4294967296)) (i64.const 4294967296)) ;; 0x1.00000p+32 -> 1 0000 0000
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -4294967296)) (i64.const -4294967296)) ;; -0x1.00000p+32 -> ffff ffff 0000 0000
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 9223372036854774784.0)) (i64.const 9223372036854774784))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -9223372036854775808.0)) (i64.const -9223372036854775808))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const 9223372036854775808.0)) (i64.const 9223372036854775807))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -9223372036854777856.0)) (i64.const -9223372036854775808))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const inf)) (i64.const 9223372036854775807))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -inf)) (i64.const -9223372036854775808))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const nan:0x4000000000000)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_s" (f64.const -nan:0x4000000000000)) (i64.const 0))

(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -0.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 0x0.0000000000001p-1022)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -0x0.0000000000001p-1022)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 1.0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 0x1.199999999999ap+0)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 1.5)) (i64.const 1))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 4294967295)) (i64.const 0xffffffff))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 4294967296)) (i64.const 0x100000000))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 18446744073709549568.0)) (i64.const -2048))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -0x1.ccccccccccccdp-1)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -0x1.fffffffffffffp-1)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 1e8)) (i64.const 100000000))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 1e16)) (i64.const 10000000000000000))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 9223372036854775808)) (i64.const -9223372036854775808))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const 18446744073709551616.0)) (i64.const 18446744073709551615))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -1.0)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const inf)) (i64.const 18446744073709551615))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -inf)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const nan:0x4000000000000)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -nan)) (i64.const 0))
(assert_return (invoke "i64.trunc_sat_f64_u" (f64.const -nan:0x4000000000000)) (i64.const 0))
