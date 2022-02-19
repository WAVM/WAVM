(module $A
  (exception_type $a (export "a") i32)
  (exception_type $b (export "b") i32)
  (exception_type $c (export "c") i32 f64)
  (exception_type $d (export "d"))

  (type $i32_to_void_sig (func (param i32)))
  (func $throw_a (export "throw_a") (type $i32_to_void_sig) (param i32) (throw $a (local.get 0)))
  (func $throw_b (export "throw_b") (type $i32_to_void_sig) (param i32) (throw $b (local.get 0)))
  (func $throw_c (export "throw_c") (param i32 f64) (throw $c (local.get 0) (local.get 1)))
  (func $throw_d (export "throw_d") (throw $d))
  (func $no_throw (export "no_throw") (type $i32_to_void_sig) (param i32))
  (func $divide_by_zero (export "divide_by_zero") (type $i32_to_void_sig) (param i32)
    local.get 0
    i32.const 0
    i32.div_s
    drop
    )


  (table funcref (elem $no_throw $divide_by_zero $throw_a $throw_b))

  (func (export "try_without_throw") (result i32)
    try (result i32)
      i32.const 5
    catch_all
      i32.const 6
    end
    )
  
  (func (export "try_with_divide_by_zero") (result i32)
    try (result i32)
      i32.const 0
      i32.const 0
      i32.div_s
      drop
      i32.const 7
    catch_all
      i32.const 8
    end
    )

  (func (export "catch_throw") (result i32)
    try (result i32)
      i32.const 9
      throw $a
    catch $a
    end
    )

  (func (export "catch_call_throw") (result i32)
    try (result i32)
      i32.const 9
      call $throw_a
      i32.const 10
    catch $a
    end
    )

  (func (export "catch_with_different_throw") (result i32)
    try (result i32)
      i32.const 11
      throw $a
    catch $b
    end
    )

    (func (export "catch_all") (param $thunk i32) (result i32)
      try (result i32)
        (call_indirect (type $i32_to_void_sig) (i32.const 13) (local.get $thunk))
        i32.const 14
      catch_all
        i32.const 15
      end
      )

  (func (export "try_inside_catch") (result i32)
      try (result i32)
        i32.const 16
        throw $a
      catch_all
        try (result i32)
          i32.const 18
          throw $b
        catch $b
        end
      end
      )

  (func (export "catch_rethrow") (result i32)
      try (result i32)
        i32.const 20
        throw $a
      catch $a
        try
          i32.const 22
          throw $b
        catch $b
          rethrow 1
        end
      end
      )

  (func (export "catch_all_rethrow") (result i32)
      try (result i32)
        i32.const 23
        throw $a
      catch_all
        try
          i32.const 25
          throw $b
        catch $b
          rethrow 1
        end
        i32.const 26
      end
      )

  (func (export "throw_from_catch") (result i32)
    try (result i32)
      i32.const 27
      throw $a
    catch $a
      throw $b
    end
    )
)

(assert_throws (invoke "throw_a" (i32.const 1)) $A "a" (i32.const 1))
(assert_throws (invoke "throw_b" (i32.const 2)) $A "b" (i32.const 2))
(assert_throws (invoke "throw_c" (i32.const 3) (f64.const 4.0)) $A "c" (i32.const 3) (f64.const 4.0))
(assert_throws (invoke "throw_d") $A "d")

(assert_return (invoke "try_without_throw") (i32.const 5))
(assert_trap (invoke "try_with_divide_by_zero") "integer divide by zero")
(assert_return (invoke "catch_throw")  (i32.const 9))
(assert_return (invoke "catch_call_throw")  (i32.const 9))
(assert_throws (invoke "catch_with_different_throw") $A "a" (i32.const 11))

(assert_return (invoke "catch_all" (i32.const 0)) (i32.const 14))
(assert_trap (invoke "catch_all" (i32.const 1)) "integer divide by zero")
(assert_return (invoke "catch_all" (i32.const 2)) (i32.const 15))
(assert_return (invoke "catch_all" (i32.const 3)) (i32.const 15))

(assert_return (invoke "try_inside_catch") (i32.const 18))
(assert_throws (invoke "catch_rethrow") $A "a" (i32.const 20))
(assert_throws (invoke "catch_all_rethrow") $A "a" (i32.const 23))

(assert_throws (invoke "throw_from_catch") $A "b" (i32.const 27))

;; todo:
;; throw inside of function vs directly in try
;; throw in catch
;; throw in catch in try (same function)
;; exception type imported into other module
;; named/indexed rethrows
;; rethrow outside of catch
;; try with multiple catches with the same exception types
;; try with multiple catch_alls
;; try with catch after catch_all
;; exception arguments

(register "A" $A)

(module
  (exception_type $a (import "A" "a") i32)
  (exception_type $b (import "A" "b") i32)
  (exception_type $c (import "A" "c") i32 f64)
  (exception_type $d (import "A" "d"))
)

(assert_unlinkable (module (exception_type (import "A" "a") i64)) "import type doesn't match")
(assert_unlinkable (module (exception_type (import "A" "c") i32 i64)) "import type doesn't match")
