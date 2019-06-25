;; multiple tables
(module
	(table $t1 0 funcref)
	(table $t2 0 funcref)
)

;; element segments in multiple tables
(module
	(table $t1 1 funcref)
	(table $t2 1 funcref)
	(elem $t1 (i32.const 0) $f)
	(elem $t2 (i32.const 0) $f)
	(func $f)
)


;; non-funcref tables
(module
	(table $t 0 anyref)
)


;; nullref tables
(assert_malformed
	(module quote "(table $t 0 nullref)")
	"expected reference type"
)

;; reference typed values

(module (func (param anyref)))
(module (func (param funcref)))
(module (func (result anyref)  ref.null))
(module (func (result funcref) ref.null))
(module (global anyref  (ref.null)))
(module (global funcref (ref.null)))
(module (global (mut anyref)  (ref.null)))
(module (global (mut funcref) (ref.null)))

;; table.get

(module
	(table $t1 4 funcref)
	(table $t2 3 funcref)

	(elem $t1 (i32.const 0) $0 $1 $2 $3)
	(elem $t2 (i32.const 1) $4 $5)

	(func $0 (result i32) (i32.const 0))
	(func $1 (result i32) (i32.const 1))
	(func $2 (result i32) (i32.const 2))
	(func $3 (result i32) (i32.const 3))
	(func $4 (result i32) (i32.const 4))
	(func $5 (result i32) (i32.const 5))

	(func (export "table.get $t1") (param $index i32) (result anyref)
		(table.get $t1 (local.get $index))
	)
	
	(func (export "table.get $t2") (param $index i32) (result anyref)
		(table.get $t2 (local.get $index))
	)
)

(invoke "table.get $t1" (i32.const 0))
(invoke "table.get $t1" (i32.const 1))
(invoke "table.get $t1" (i32.const 2))
(invoke "table.get $t1" (i32.const 3))
(assert_trap (invoke "table.get $t1" (i32.const 4)) "undefined element")
(assert_trap (invoke "table.get $t1" (i32.const -1)) "undefined element")

(invoke "table.get $t2" (i32.const 0))
(invoke "table.get $t2" (i32.const 1))
(invoke "table.get $t2" (i32.const 2))
(assert_trap (invoke "table.get $t2" (i32.const 3)) "undefined element")
(assert_trap (invoke "table.get $t2" (i32.const -1)) "undefined element")

;; call_indirect with non-zero table index

(module
	(table $t1 4 anyref)
	(table $t2 4 funcref)
	(table $t3 4 funcref)

	(elem $t2 (i32.const 0) $0 $1 $2 $3)
	(elem $t3 (i32.const 0) $4 $5)
	
	(func $0 (result i32) (i32.const 0))
	(func $1 (result i32) (i32.const 1))
	(func $2 (result i32) (i32.const 2))
	(func $3 (result i32) (i32.const 3))
	(func $4 (result i32) (i32.const 4))
	(func $5 (result i32) (i32.const 5))

	(func (export "call_indirect $t2") (param $index i32) (result i32)
		(call_indirect $t2 (result i32) (local.get $index))
	)
	(func (export "call_indirect $t3") (param $index i32) (result i32)
		(call_indirect 2 (result i32) (local.get $index))
	)
)

(assert_return (invoke "call_indirect $t2" (i32.const 0)) (i32.const 0))
(assert_return (invoke "call_indirect $t2" (i32.const 1)) (i32.const 1))
(assert_return (invoke "call_indirect $t2" (i32.const 2)) (i32.const 2))
(assert_return (invoke "call_indirect $t2" (i32.const 3)) (i32.const 3))
(assert_return (invoke "call_indirect $t3" (i32.const 0)) (i32.const 4))
(assert_return (invoke "call_indirect $t3" (i32.const 1)) (i32.const 5))
(assert_trap   (invoke "call_indirect $t3" (i32.const 2)) "uninitialized")
(assert_trap   (invoke "call_indirect $t3" (i32.const 3)) "uninitialized")