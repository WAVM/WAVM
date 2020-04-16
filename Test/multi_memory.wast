;; Test memory section structure

(module (memory 0 0) (memory 0 0))
(module (memory 0 1) (memory 0 1))
(module (memory 1 256) (memory 1 256))
(module (memory 0 65536) (memory 0 65536))
(module (memory 0 0 shared) (memory 0 0 shared))
(module (memory 1 2 shared) (memory 1 2 shared))

(module (memory (import "spectest" "memory") 1 2) (memory 0 0))

(module (memory 1) (memory 1) (memory 1) (memory 1) (memory 1) (memory 1) (memory 1))

;; Binary encoding

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1
	"\05\05\02"                          ;; memory section: 5 bytes, 2 entries
	"\00\01"                             ;;   (memory 1)
	"\00\01"                             ;;   (memory 1)
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0
	
	"\05\05\02"                          ;; memory section: 5 bytes, 2 entries
	"\00\01"                             ;;   (memory 1)
	"\00\01"                             ;;   (memory 1)

	"\0a\0c\01"                          ;; Code section
	"\0a\00"                             ;; function 0: 10 bytes, 0 local sets
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\36\42\00\01"                       ;; i32.store 1 align=4 offset=0
	"\0b"                                ;; end
)

;; Load/store
(module
  (memory $a 1)
  (memory $b 2)

  (func (export "a.store32") (param $address i32) (param $value i32)
    (i32.store $a offset=0 align=4 (local.get $address) (local.get $value))
  )
  (func (export "b.store32") (param $address i32) (param $value i32)
    (i32.store $b offset=0 align=4 (local.get $address) (local.get $value))
  )
  (func (export "a.load32") (param $address i32) (result i32)
    (i32.load $a offset=0 align=4 (local.get $address))
  )
  (func (export "b.load32") (param $address i32) (result i32)
    (i32.load $b offset=0 align=4 (local.get $address))
  )
)

(assert_return (invoke "a.load32" (i32.const 0)) (i32.const 0))
(assert_return (invoke "a.store32" (i32.const 0) (i32.const 0x01020304)))
(assert_return (invoke "a.load32" (i32.const 0)) (i32.const 0x01020304))

(assert_return (invoke "b.load32" (i32.const 0)) (i32.const 0))
(assert_return (invoke "b.store32" (i32.const 0) (i32.const 0x05060708)))
(assert_return (invoke "b.load32" (i32.const 0)) (i32.const 0x05060708))
(assert_return (invoke "a.load32" (i32.const 0)) (i32.const 0x01020304))

(assert_trap (invoke "a.load32" (i32.const 0x10000)) "out of bounds memory access")
(assert_return (invoke "b.load32" (i32.const 0x10000)) (i32.const 0))
(assert_trap (invoke "b.load32" (i32.const 0x20000)) "out of bounds memory access")

;; Atomic load/store
(module
  (memory $a 1 1 shared)
  (memory $b 2 2 shared)

  (func (export "a.atomic.store32") (param $address i32) (param $value i32)
    (i32.atomic.store $a offset=0 align=4 (local.get $address) (local.get $value))
  )
  (func (export "b.atomic.store32") (param $address i32) (param $value i32)
    (i32.atomic.store $b offset=0 align=4 (local.get $address) (local.get $value))
  )
  (func (export "a.atomic.load32") (param $address i32) (result i32)
    (i32.atomic.load $a offset=0 align=4 (local.get $address))
  )
  (func (export "b.atomic.load32") (param $address i32) (result i32)
    (i32.atomic.load $b offset=0 align=4 (local.get $address))
  )
)

(assert_return (invoke "a.atomic.load32" (i32.const 0)) (i32.const 0))
(assert_return (invoke "a.atomic.store32" (i32.const 0) (i32.const 0x01020304)))
(assert_return (invoke "a.atomic.load32" (i32.const 0)) (i32.const 0x01020304))

(assert_return (invoke "b.atomic.load32" (i32.const 0)) (i32.const 0))
(assert_return (invoke "b.atomic.store32" (i32.const 0) (i32.const 0x05060708)))
(assert_return (invoke "b.atomic.load32" (i32.const 0)) (i32.const 0x05060708))
(assert_return (invoke "a.atomic.load32" (i32.const 0)) (i32.const 0x01020304))

(assert_trap (invoke "a.atomic.load32" (i32.const 0x10000)) "out of bounds memory access")
(assert_return (invoke "b.atomic.load32" (i32.const 0x10000)) (i32.const 0))
(assert_trap (invoke "b.atomic.load32" (i32.const 0x20000)) "out of bounds memory access")


;; Active data segments
(module
  (memory $a 1)
  (memory $b 1)
  (data (memory $a) (i32.const 0) "ABC\a7D") (data (memory $a) (i32.const 20) "WASM")
  (data (memory $b) (i32.const 0) "WASM") (data (memory $b) (i32.const 20) "ABC\a7D")

  (func (export "a.load8_u") (param $address i32) (result i32)
    (i32.load8_u $a (local.get $address))
  )
  (func (export "b.load8_u") (param $address i32) (result i32)
    (i32.load8_u $b (local.get $address))
  )
)

(assert_return (invoke "a.load8_u" (i32.const 0)) (i32.const 65))
(assert_return (invoke "a.load8_u" (i32.const 1)) (i32.const 66))
(assert_return (invoke "a.load8_u" (i32.const 2)) (i32.const 67))
(assert_return (invoke "a.load8_u" (i32.const 3)) (i32.const 167))
(assert_return (invoke "a.load8_u" (i32.const 4)) (i32.const 68))
(assert_return (invoke "a.load8_u" (i32.const 5)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 6)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 7)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 16)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 17)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 18)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 19)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 20)) (i32.const 87))
(assert_return (invoke "a.load8_u" (i32.const 21)) (i32.const 65))
(assert_return (invoke "a.load8_u" (i32.const 22)) (i32.const 83))
(assert_return (invoke "a.load8_u" (i32.const 23)) (i32.const 77))
(assert_return (invoke "a.load8_u" (i32.const 24)) (i32.const 0))

(assert_return (invoke "b.load8_u" (i32.const 0)) (i32.const 87))
(assert_return (invoke "b.load8_u" (i32.const 1)) (i32.const 65))
(assert_return (invoke "b.load8_u" (i32.const 2)) (i32.const 83))
(assert_return (invoke "b.load8_u" (i32.const 3)) (i32.const 77))
(assert_return (invoke "b.load8_u" (i32.const 4)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 5)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 6)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 7)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 16)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 17)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 18)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 19)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 20)) (i32.const 65))
(assert_return (invoke "b.load8_u" (i32.const 21)) (i32.const 66))
(assert_return (invoke "b.load8_u" (i32.const 22)) (i32.const 67))
(assert_return (invoke "b.load8_u" (i32.const 23)) (i32.const 167))
(assert_return (invoke "b.load8_u" (i32.const 24)) (i32.const 68))

;; memory.size/memory.grow with multiple memories.
(module
    (memory $a 0)
    (memory $b 0)

    (func (export "a.load_at_zero") (result i32) (i32.load $a (i32.const 0)))
    (func (export "a.store_at_zero") (i32.store $a (i32.const 0) (i32.const 2)))

    (func (export "a.load_at_page_size") (result i32) (i32.load $a (i32.const 0x10000)))
    (func (export "a.store_at_page_size") (i32.store $a (i32.const 0x10000) (i32.const 3)))

    (func (export "a.grow") (param $sz i32) (result i32) (memory.grow $a (local.get $sz)))
    (func (export "a.size") (result i32) (memory.size $a))

    (func (export "b.load_at_zero") (result i32) (i32.load $b (i32.const 0)))
    (func (export "b.store_at_zero") (i32.store $b (i32.const 0) (i32.const 2)))

    (func (export "b.load_at_page_size") (result i32) (i32.load $b (i32.const 0x10000)))
    (func (export "b.store_at_page_size") (i32.store $b (i32.const 0x10000) (i32.const 3)))

    (func (export "b.grow") (param $sz i32) (result i32) (memory.grow $b (local.get $sz)))
    (func (export "b.size") (result i32) (memory.size $b))
)

(assert_return (invoke "a.size") (i32.const 0))
(assert_trap (invoke "a.store_at_zero") "out of bounds memory access")
(assert_trap (invoke "a.load_at_zero") "out of bounds memory access")
(assert_trap (invoke "a.store_at_page_size") "out of bounds memory access")
(assert_trap (invoke "a.load_at_page_size") "out of bounds memory access")
(assert_return (invoke "a.grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "a.size") (i32.const 1))
(assert_return (invoke "a.load_at_zero") (i32.const 0))
(assert_return (invoke "a.store_at_zero"))
(assert_return (invoke "a.load_at_zero") (i32.const 2))
(assert_trap (invoke "a.store_at_page_size") "out of bounds memory access")
(assert_trap (invoke "a.load_at_page_size") "out of bounds memory access")
(assert_return (invoke "a.grow" (i32.const 4)) (i32.const 1))
(assert_return (invoke "a.size") (i32.const 5))
(assert_return (invoke "a.load_at_zero") (i32.const 2))
(assert_return (invoke "a.store_at_zero"))
(assert_return (invoke "a.load_at_zero") (i32.const 2))
(assert_return (invoke "a.load_at_page_size") (i32.const 0))
(assert_return (invoke "a.store_at_page_size"))
(assert_return (invoke "a.load_at_page_size") (i32.const 3))

(assert_return (invoke "b.size") (i32.const 0))
(assert_trap (invoke "b.store_at_zero") "out of bounds memory access")
(assert_trap (invoke "b.load_at_zero") "out of bounds memory access")
(assert_trap (invoke "b.store_at_page_size") "out of bounds memory access")
(assert_trap (invoke "b.load_at_page_size") "out of bounds memory access")
(assert_return (invoke "b.grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "b.size") (i32.const 1))
(assert_return (invoke "b.load_at_zero") (i32.const 0))
(assert_return (invoke "b.store_at_zero"))
(assert_return (invoke "b.load_at_zero") (i32.const 2))
(assert_trap (invoke "b.store_at_page_size") "out of bounds memory access")
(assert_trap (invoke "b.load_at_page_size") "out of bounds memory access")
(assert_return (invoke "b.grow" (i32.const 4)) (i32.const 1))
(assert_return (invoke "b.size") (i32.const 5))
(assert_return (invoke "b.load_at_zero") (i32.const 2))
(assert_return (invoke "b.store_at_zero"))
(assert_return (invoke "b.load_at_zero") (i32.const 2))
(assert_return (invoke "b.load_at_page_size") (i32.const 0))
(assert_return (invoke "b.store_at_page_size"))
(assert_return (invoke "b.load_at_page_size") (i32.const 3))


(module
  (memory $a 0)
  (memory $b 0)
  (func (export "a.grow") (param i32) (result i32) (memory.grow $a (local.get 0)))
  (func (export "b.grow") (param i32) (result i32) (memory.grow $b (local.get 0)))
)

(assert_return (invoke "a.grow" (i32.const 0)) (i32.const 0))
(assert_return (invoke "a.grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "a.grow" (i32.const 0)) (i32.const 1))
(assert_return (invoke "a.grow" (i32.const 2)) (i32.const 1))
(assert_return (invoke "a.grow" (i32.const 800)) (i32.const 3))
(assert_return (invoke "a.grow" (i32.const 0x10000)) (i32.const -1))
(assert_return (invoke "a.grow" (i32.const 64736)) (i32.const -1))
(assert_return (invoke "a.grow" (i32.const 1)) (i32.const 803))

(assert_return (invoke "b.grow" (i32.const 0)) (i32.const 0))
(assert_return (invoke "b.grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "b.grow" (i32.const 0)) (i32.const 1))
(assert_return (invoke "b.grow" (i32.const 2)) (i32.const 1))
(assert_return (invoke "b.grow" (i32.const 800)) (i32.const 3))
(assert_return (invoke "b.grow" (i32.const 0x10000)) (i32.const -1))
(assert_return (invoke "b.grow" (i32.const 64736)) (i32.const -1))
(assert_return (invoke "b.grow" (i32.const 1)) (i32.const 803))

(module
  (memory $a 0 10)
  (memory $b 0 10)
  (func (export "a.grow") (param i32) (result i32) (memory.grow $a (local.get 0)))
  (func (export "b.grow") (param i32) (result i32) (memory.grow $b (local.get 0)))
)

(assert_return (invoke "a.grow" (i32.const 0)) (i32.const 0))
(assert_return (invoke "a.grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "a.grow" (i32.const 1)) (i32.const 1))
(assert_return (invoke "a.grow" (i32.const 2)) (i32.const 2))
(assert_return (invoke "a.grow" (i32.const 6)) (i32.const 4))
(assert_return (invoke "a.grow" (i32.const 0)) (i32.const 10))
(assert_return (invoke "a.grow" (i32.const 1)) (i32.const -1))
(assert_return (invoke "a.grow" (i32.const 0x10000)) (i32.const -1))

(assert_return (invoke "b.grow" (i32.const 0)) (i32.const 0))
(assert_return (invoke "b.grow" (i32.const 1)) (i32.const 0))
(assert_return (invoke "b.grow" (i32.const 1)) (i32.const 1))
(assert_return (invoke "b.grow" (i32.const 2)) (i32.const 2))
(assert_return (invoke "b.grow" (i32.const 6)) (i32.const 4))
(assert_return (invoke "b.grow" (i32.const 0)) (i32.const 10))
(assert_return (invoke "b.grow" (i32.const 1)) (i32.const -1))
(assert_return (invoke "b.grow" (i32.const 0x10000)) (i32.const -1))

;; memory.fill
(module
  (memory $a 1)
  (memory $b 1)

  (func (export "a.fill") (param i32 i32 i32)
    (memory.fill $a
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "b.fill") (param i32 i32 i32)
    (memory.fill $b
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "a.load8_u") (param i32) (result i32)
    (i32.load8_u $a (local.get 0)))

  (func (export "b.load8_u") (param i32) (result i32)
    (i32.load8_u $b (local.get 0)))
)

;; Basic fill test.
(invoke "a.fill" (i32.const 1) (i32.const 0xff) (i32.const 3))
(assert_return (invoke "a.load8_u" (i32.const 0)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 1)) (i32.const 0xff))
(assert_return (invoke "a.load8_u" (i32.const 2)) (i32.const 0xff))
(assert_return (invoke "a.load8_u" (i32.const 3)) (i32.const 0xff))
(assert_return (invoke "a.load8_u" (i32.const 4)) (i32.const 0))

;; Fill value is stored as a byte.
(invoke "a.fill" (i32.const 0) (i32.const 0xbbaa) (i32.const 2))
(assert_return (invoke "a.load8_u" (i32.const 0)) (i32.const 0xaa))
(assert_return (invoke "a.load8_u" (i32.const 1)) (i32.const 0xaa))

;; Fill all of memory
(invoke "a.fill" (i32.const 0) (i32.const 0) (i32.const 0x10000))

;; Out-of-bounds fills trap before executing any writes.
(assert_trap (invoke "a.fill" (i32.const 0xff00) (i32.const 1) (i32.const 0x101))
    "out of bounds memory access")
(assert_return (invoke "a.load8_u" (i32.const 0xff00)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 0xffff)) (i32.const 0))

;; Succeed when writing 0 bytes at the end of the region.
(invoke "a.fill" (i32.const 0x10000) (i32.const 0) (i32.const 0))

;; Out-of-bounds zero-byte fills trap.
(assert_trap (invoke "a.fill" (i32.const 0x10001) (i32.const 0) (i32.const 0))
    "out of bounds memory access")

;; Basic fill test.
(invoke "b.fill" (i32.const 1) (i32.const 0xff) (i32.const 3))
(assert_return (invoke "b.load8_u" (i32.const 0)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 1)) (i32.const 0xff))
(assert_return (invoke "b.load8_u" (i32.const 2)) (i32.const 0xff))
(assert_return (invoke "b.load8_u" (i32.const 3)) (i32.const 0xff))
(assert_return (invoke "b.load8_u" (i32.const 4)) (i32.const 0))

;; Fill value is stored as a byte.
(invoke "b.fill" (i32.const 0) (i32.const 0xbbaa) (i32.const 2))
(assert_return (invoke "b.load8_u" (i32.const 0)) (i32.const 0xaa))
(assert_return (invoke "b.load8_u" (i32.const 1)) (i32.const 0xaa))

;; Fill all of memory
(invoke "b.fill" (i32.const 0) (i32.const 0) (i32.const 0x10000))

;; Out-of-bounds fills trap before executing any writes.
(assert_trap (invoke "b.fill" (i32.const 0xff00) (i32.const 1) (i32.const 0x101))
    "out of bounds memory access")
(assert_return (invoke "b.load8_u" (i32.const 0xff00)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 0xffff)) (i32.const 0))

;; Succeed when writing 0 bytes at the end of the region.
(invoke "b.fill" (i32.const 0x10000) (i32.const 0) (i32.const 0))

;; Out-of-bounds zero-byte fills trap.
(assert_trap (invoke "b.fill" (i32.const 0x10001) (i32.const 0) (i32.const 0))
    "out of bounds memory access")


;; memory.copy
(module
  (memory $a (data "\aa\bb\cc\dd"))
  (memory $b (data "\11\22\33\44"))

  (func (export "aa.copy") (param i32 i32 i32)
    (memory.copy $a $a
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "ba.copy") (param i32 i32 i32)
    (memory.copy $b $a
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "bb.copy") (param i32 i32 i32)
    (memory.copy $b $b
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "a.load8_u") (param i32) (result i32)
    (i32.load8_u $a (local.get 0)))

  (func (export "b.load8_u") (param i32) (result i32)
    (i32.load8_u $b (local.get 0)))
)

;; Non-overlapping intra-memory copy.
(invoke "aa.copy" (i32.const 10) (i32.const 0) (i32.const 4))

(assert_return (invoke "a.load8_u" (i32.const 9)) (i32.const 0))
(assert_return (invoke "a.load8_u" (i32.const 10)) (i32.const 0xaa))
(assert_return (invoke "a.load8_u" (i32.const 11)) (i32.const 0xbb))
(assert_return (invoke "a.load8_u" (i32.const 12)) (i32.const 0xcc))
(assert_return (invoke "a.load8_u" (i32.const 13)) (i32.const 0xdd))
(assert_return (invoke "a.load8_u" (i32.const 14)) (i32.const 0))

;; Non-overlapping intra-memory copy.
(invoke "bb.copy" (i32.const 10) (i32.const 0) (i32.const 4))

(assert_return (invoke "b.load8_u" (i32.const 9)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 10)) (i32.const 0x11))
(assert_return (invoke "b.load8_u" (i32.const 11)) (i32.const 0x22))
(assert_return (invoke "b.load8_u" (i32.const 12)) (i32.const 0x33))
(assert_return (invoke "b.load8_u" (i32.const 13)) (i32.const 0x44))
(assert_return (invoke "b.load8_u" (i32.const 14)) (i32.const 0))

;; Copy between memories
(invoke "ba.copy" (i32.const 20) (i32.const 0) (i32.const 4))

(assert_return (invoke "b.load8_u" (i32.const 19)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 20)) (i32.const 0xaa))
(assert_return (invoke "b.load8_u" (i32.const 21)) (i32.const 0xbb))
(assert_return (invoke "b.load8_u" (i32.const 22)) (i32.const 0xcc))
(assert_return (invoke "b.load8_u" (i32.const 23)) (i32.const 0xdd))
(assert_return (invoke "b.load8_u" (i32.const 24)) (i32.const 0))

;; memory.init
(module
  (memory $a 1)
  (memory $b 1)
  (data $s "\aa\bb\cc\dd")

  (func (export "a.init") (param i32 i32 i32)
    (memory.init $a $s
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "b.init") (param i32 i32 i32)
    (memory.init $b $s
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "a.load8_u") (param i32) (result i32)
    (i32.load8_u $a (local.get 0)))

  (func (export "b.load8_u") (param i32) (result i32)
    (i32.load8_u $b (local.get 0)))
)

(invoke "a.init" (i32.const 0) (i32.const 1) (i32.const 2))
(assert_return (invoke "a.load8_u" (i32.const 0)) (i32.const 0xbb))
(assert_return (invoke "a.load8_u" (i32.const 1)) (i32.const 0xcc))
(assert_return (invoke "a.load8_u" (i32.const 2)) (i32.const 0))

(invoke "b.init" (i32.const 2) (i32.const 0) (i32.const 4))
(assert_return (invoke "b.load8_u" (i32.const 0)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 1)) (i32.const 0))
(assert_return (invoke "b.load8_u" (i32.const 2)) (i32.const 0xaa))
(assert_return (invoke "b.load8_u" (i32.const 3)) (i32.const 0xbb))
(assert_return (invoke "b.load8_u" (i32.const 4)) (i32.const 0xcc))
(assert_return (invoke "b.load8_u" (i32.const 5)) (i32.const 0xdd))
(assert_return (invoke "b.load8_u" (i32.const 6)) (i32.const 0))

;; Init ending at memory limit and segment limit is ok.
(invoke "a.init" (i32.const 0xfffc) (i32.const 0) (i32.const 4))
(invoke "b.init" (i32.const 0xfffc) (i32.const 0) (i32.const 4))
