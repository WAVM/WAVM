;; multiple tables
(module
	(table $t1 0 anyfunc)
	(table $t2 0 anyfunc)
)

;; element segments in multiple tables
(module
	(table $t1 1 anyfunc)
	(table $t2 1 anyfunc)
	(elem $t1 (i32.const 0) $f)
	(elem $t2 (i32.const 0) $f)
	(func $f)
)


;; non-anyfunc tables
(module
	(table $t 0 anyref)
)

;; elem segments should only be allowed in anyfunc tables
(assert_invalid
	(module
		(table $t 0 anyref)
		(elem $t (i32.const 0) $f)
		(func $f)
	)
	"active elem segments must be in anyfunc tables"
)


;; nullref tables
(assert_malformed
	(module quote "(table $t 0 nullref)")
	"expected reference type"
)

;; reference typed values

(module (func (param anyref)))
(module (func (param anyfunc)))
(module (func (result anyref)  ref.null))
(module (func (result anyfunc) ref.null))
(module (global anyref  (ref.null)))
(module (global anyfunc (ref.null)))
(module (global (mut anyref)  (ref.null)))
(module (global (mut anyfunc) (ref.null)))

;; table.get

(module
	(table $t1 4 anyfunc)
	(table $t2 3 anyfunc)

	(elem $t1 (i32.const 0) $0 $1 $2 $3)
	(elem $t2 (i32.const 1) $4 $5)

	(func $0 (result i32) (i32.const 0))
	(func $1 (result i32) (i32.const 1))
	(func $2 (result i32) (i32.const 2))
	(func $3 (result i32) (i32.const 3))
	(func $4 (result i32) (i32.const 4))
	(func $5 (result i32) (i32.const 5))

	(func (export "table.get $t1") (param $index i32) (result anyref)
		(table.get $t1 (get_local $index))
	)
	
	(func (export "table.get $t2") (param $index i32) (result anyref)
		(table.get $t2 (get_local $index))
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
	(table $t2 4 anyfunc)
	(table $t3 4 anyfunc)

	(elem $t2 (i32.const 0) $0 $1 $2 $3)
	(elem $t3 (i32.const 0) $4 $5)
	
	(func $0 (result i32) (i32.const 0))
	(func $1 (result i32) (i32.const 1))
	(func $2 (result i32) (i32.const 2))
	(func $3 (result i32) (i32.const 3))
	(func $4 (result i32) (i32.const 4))
	(func $5 (result i32) (i32.const 5))

	(func (export "call_indirect $t2") (param $index i32) (result i32)
		(call_indirect $t2 (result i32) (get_local $index))
	)
	(func (export "call_indirect $t3") (param $index i32) (result i32)
		(call_indirect 2 (result i32) (get_local $index))
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