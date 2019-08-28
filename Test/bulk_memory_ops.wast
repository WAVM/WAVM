;; passive data segments and data segment encoding

(module (data "test"))

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1
	"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
	"\00\01"                             ;;   (memory 1)
	"\0b\07\01"                          ;; data section: 5 bytes, 1 entry
	"\00"                                ;;   [0] active data segment, memory 0
	"\41\00\0b"                          ;;     base offset (i32.const 0)
	"\01\00"                             ;;     1 byte of data
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1
	"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
	"\00\01"                             ;;   (memory 1)
	"\0b\04\01"                          ;; data section: 4 bytes, 1 entry
	"\01\01\00"                          ;;   [0] passive data segment: 1 byte of data
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1
	"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
	"\00\01"                             ;;   (memory 1)
	"\0b\08\01"                          ;; data section: 8 bytes, 1 entry
	"\02\00"                             ;;   [0] active data segment, memory 0
	"\41\00\0b"                          ;;     base offset (i32.const 0)
	"\01\00"                             ;;     1 byte of data
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1
	"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
	"\00\01"                             ;;   (memory 1)
	"\0b\0a\02"                          ;; data section: 9 bytes, 2 entries
	"\00"                                ;;   [0] active data segment, memory 0
	"\41\00\0b"                          ;;     base offset (i32.const 0)
	"\01\00"                             ;;     1 byte of data
	"\01\01\00"                          ;;   [1] passive data segment: 1 byte of data
)

(assert_malformed
	(module binary
		"\00asm" "\01\00\00\00"              ;; WebAssembly version 1
		"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
		"\00\01"                             ;;   (memory 1)
		"\0b\07\01"                          ;; data section: 7 bytes, 1 entries
		"\03"                                ;;   [0] <invalid data segment flags>
		"\41\00\0b"                          ;;     base offset (i32.const 0)
		"\01\00"                             ;;     1 byte of data
	)
	"invalid data segment flags"
)

;; memory.init/data.drop

(assert_invalid
	(module
		(memory $m 1)
		(data "test")
		(func (memory.init (i32.const 0) (i32.const 0) (i32.const 0)))
	)
	"invalid data segment index"
)
(assert_invalid
	(module
		(memory $m 1)
		(data "test")
		(func (memory.init 1 (i32.const 0) (i32.const 0) (i32.const 0)))
	)
	"invalid data segment index"
)
(assert_invalid
	(module
		(memory $m 1)
		(data "test")
		(func (memory.init 0 1 (i32.const 0) (i32.const 0) (i32.const 0)))
	)
	"invalid memory index"
)
(assert_invalid
	(module
		(memory $m 1)
		(data "test")
		(func (data.drop 1))
	)
	"invalid data segment index"
)

(module
	(memory $m 1 1)
	(data "a")
	(func (memory.init 0 (i32.const 0) (i32.const 0) (i32.const 0)))
)

(module
	(memory $m 1 1)
	(data "a")
	(func (memory.init 0 0 (i32.const 0) (i32.const 0) (i32.const 0)))
)

(module
	(memory $m 1 1)
	(data "a")
	(func (memory.init 0 $m (i32.const 0) (i32.const 0) (i32.const 0)))
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
	"\00\01"                             ;;   (memory 1)

	"\0c\01\01"                          ;; DataCount section: 1 byte, 1 segment

	"\0a\11\01"                          ;; Code section
	"\0f\00"                             ;; function 0: 15 bytes, 0 local sets
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\fc\08\00\00"                       ;; memory.init 0 0
	"\fc\09\00"                          ;; data.drop 0
	"\0b"                                ;; end

	"\0b\04\01"                          ;; data section: 4 bytes, 1 entry
	"\01\01\00"                          ;;   [0] 1 byte passive data segment
)

(assert_invalid
	(module binary
		"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

		"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
		"\60\00\00"                          ;;   Function type () -> ()

		"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
		"\00"                                ;;   Function 0: type 0

		"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
		"\00\01"                             ;;   (memory 1)

		"\0c\01\01"                          ;; DataCount section: 1 byte, 1 segment

		"\0a\11\01"                          ;; Code section
		"\0f\00"                             ;; function 0: 15 bytes, 0 local sets
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\fc\08\01\00"                       ;; memory.init 1 0
		"\fc\09\01"                          ;; data.drop 1
		"\0b"                                ;; end
		
		"\0b\03\01"                          ;; data section: 3 bytes, 1 entry
		"\01\00"                             ;;   [0] 1 byte passive data segment
	)
	"invalid data segment index"
)

(assert_invalid
	(module binary
		"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

		"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
		"\60\00\00"                          ;;   Function type () -> ()

		"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
		"\00"                                ;;   Function 0: type 0

		"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
		"\00\01"                             ;;   (memory 1)

		"\0c\01\01"                          ;; DataCount section: 1 byte, 1 segment

		"\0a\11\01"                          ;; Code section
		"\0f\00"                             ;; function 0: 15 bytes, 0 local sets
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\fc\08\00\01"                       ;; memory.init 0 1
		"\fc\09\00"                          ;; data.drop 0
		"\0b"                                ;; end

		"\0b\04\01"                          ;; data section: 5 bytes, 1 entry
		"\01"                                ;; passive data segment
		"\01\00"                             ;;   1 byte of data
	)
	"invalid memory index"
)

;; Test that it's valid to reference an active data segment with memory.init
(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\05\03\01"                          ;; memory section: 3 bytes, 1 entry
	"\00\01"                             ;;   (memory 1)

	"\0c\01\01"                          ;; DataCount section: 1 byte, 1 segment

	"\0a\11\01"                          ;; Code section
	"\0f\00"                             ;; function 0: 15 bytes, 0 local sets
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\fc\08\00\00"                       ;; memory.init 0 0
	"\fc\09\00"                          ;; data.drop 0
	"\0b"                                ;; end
		
	"\0b\07\01"                          ;; data section: 5 bytes, 1 entry
	"\00\41\00\0b"                       ;; active data segment, base offset (i32.const 0)
	"\01\00"                             ;;   1 byte of data
)

(module
	(memory $m 1 1)

	(data "\01\02\03\04")
	(data "\05\06\07\08\09\0a\0b\0c\0d\0e\0f\10\11\12\13\14")

	(func (export "memory.init 0")
		(param $destAddress i32)
		(param $sourceOffset i32)
		(param $numBytes i32)
		(memory.init 0 $m (local.get $destAddress) (local.get $sourceOffset) (local.get $numBytes))
	)
	
	(func (export "memory.init 1")
		(param $destAddress i32)
		(param $sourceOffset i32)
		(param $numBytes i32)
		(memory.init 1 $m (local.get $destAddress) (local.get $sourceOffset) (local.get $numBytes))
	)
	
	(func (export "data.drop 0") (data.drop 0))
	(func (export "data.drop 1") (data.drop 1))

	(func (export "i32.load8_u") (param $address i32) (result i32)
		(i32.load8_u (local.get $address))
	)
)

(assert_return (invoke "memory.init 0" (i32.const 0) (i32.const 0) (i32.const 4)))
(assert_return (invoke "i32.load8_u" (i32.const 0)) (i32.const 0x1))
(assert_return (invoke "i32.load8_u" (i32.const 1)) (i32.const 0x2))
(assert_return (invoke "i32.load8_u" (i32.const 2)) (i32.const 0x3))
(assert_return (invoke "i32.load8_u" (i32.const 3)) (i32.const 0x4))
(assert_return (invoke "i32.load8_u" (i32.const 4)) (i32.const 0))

(assert_return (invoke "memory.init 1" (i32.const 0) (i32.const 0) (i32.const 16)))
(assert_return (invoke "i32.load8_u" (i32.const 0 )) (i32.const 0x05))
(assert_return (invoke "i32.load8_u" (i32.const 1 )) (i32.const 0x06))
(assert_return (invoke "i32.load8_u" (i32.const 2 )) (i32.const 0x07))
(assert_return (invoke "i32.load8_u" (i32.const 3 )) (i32.const 0x08))
(assert_return (invoke "i32.load8_u" (i32.const 4 )) (i32.const 0x09))
(assert_return (invoke "i32.load8_u" (i32.const 5 )) (i32.const 0x0a))
(assert_return (invoke "i32.load8_u" (i32.const 6 )) (i32.const 0x0b))
(assert_return (invoke "i32.load8_u" (i32.const 7 )) (i32.const 0x0c))
(assert_return (invoke "i32.load8_u" (i32.const 8 )) (i32.const 0x0d))
(assert_return (invoke "i32.load8_u" (i32.const 9 )) (i32.const 0x0e))
(assert_return (invoke "i32.load8_u" (i32.const 10)) (i32.const 0x0f))
(assert_return (invoke "i32.load8_u" (i32.const 11)) (i32.const 0x10))
(assert_return (invoke "i32.load8_u" (i32.const 12)) (i32.const 0x11))
(assert_return (invoke "i32.load8_u" (i32.const 13)) (i32.const 0x12))
(assert_return (invoke "i32.load8_u" (i32.const 14)) (i32.const 0x13))
(assert_return (invoke "i32.load8_u" (i32.const 15)) (i32.const 0x14))

(assert_return (invoke "memory.init 0" (i32.const 1) (i32.const 0) (i32.const 2)))
(assert_return (invoke "i32.load8_u" (i32.const 0)) (i32.const 0x5))
(assert_return (invoke "i32.load8_u" (i32.const 1)) (i32.const 0x1))
(assert_return (invoke "i32.load8_u" (i32.const 2)) (i32.const 0x2))
(assert_return (invoke "i32.load8_u" (i32.const 3)) (i32.const 0x8))

(assert_return (invoke "memory.init 0" (i32.const 10) (i32.const 1) (i32.const 3)))
(assert_return (invoke "i32.load8_u" (i32.const 9)) (i32.const 0xe))
(assert_return (invoke "i32.load8_u" (i32.const 10)) (i32.const 0x2))
(assert_return (invoke "i32.load8_u" (i32.const 11)) (i32.const 0x3))
(assert_return (invoke "i32.load8_u" (i32.const 12)) (i32.const 0x4))
(assert_return (invoke "i32.load8_u" (i32.const 13)) (i32.const 0x12))

(assert_return (invoke "memory.init 0" (i32.const 65532) (i32.const 0) (i32.const 4)))
(assert_trap (invoke "memory.init 0" (i32.const 65532) (i32.const 4) (i32.const 1)) "out of bounds data segment access")
(assert_trap (invoke "memory.init 0" (i32.const 65533) (i32.const 0) (i32.const 4)) "out of bounds memory access")
(assert_return (invoke "i32.load8_u" (i32.const 65533)) (i32.const 1))
(assert_return (invoke "i32.load8_u" (i32.const 65534)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 65535)) (i32.const 3))
(assert_return (invoke "memory.init 1" (i32.const 65520) (i32.const 0) (i32.const 16)))
(assert_trap (invoke "memory.init 1" (i32.const 65541) (i32.const 0) (i32.const 16)) "out of bounds memory access")
(assert_trap (invoke "memory.init 1" (i32.const 100) (i32.const 9) (i32.const 8)) "out of bounds data segment access")
(assert_return (invoke "i32.load8_u" (i32.const 100)) (i32.const 0x0e))
(assert_return (invoke "i32.load8_u" (i32.const 101)) (i32.const 0x0f))
(assert_return (invoke "i32.load8_u" (i32.const 102)) (i32.const 0x10))
(assert_return (invoke "i32.load8_u" (i32.const 103)) (i32.const 0x11))
(assert_return (invoke "i32.load8_u" (i32.const 104)) (i32.const 0x12))
(assert_return (invoke "i32.load8_u" (i32.const 105)) (i32.const 0x13))
(assert_return (invoke "i32.load8_u" (i32.const 106)) (i32.const 0x14))
(assert_return (invoke "i32.load8_u" (i32.const 107)) (i32.const 0))

(assert_trap (invoke "memory.init 0" (i32.const 0xffffffff) (i32.const 0) (i32.const 1)) "out of bounds memory access")
(assert_trap (invoke "memory.init 0" (i32.const 0) (i32.const 0xffffffff) (i32.const 1)) "out of bounds data segment access")

(assert_return (invoke "data.drop 0"))
(assert_trap (invoke "memory.init 0" (i32.const 0) (i32.const 0) (i32.const 4)) "invalid argument")
(assert_trap (invoke "data.drop 0") "invalid argument")

(module
	(memory $m 1 1)

	(data (i32.const 0) "\10\11\12\13\14\15\16\17")
	
	(func (export "memory.copy")
		(param $destAddress i32)
		(param $sourceAddress i32)
		(param $numBytes i32)
		(memory.copy (local.get $destAddress) (local.get $sourceAddress) (local.get $numBytes))
	)
	
	(func (export "i32.load8_u") (param $address i32) (result i32)
		(i32.load8_u (local.get $address))
	)
)

(assert_return (invoke "i32.load8_u" (i32.const 0)) (i32.const 0x10))
(assert_return (invoke "i32.load8_u" (i32.const 1)) (i32.const 0x11))
(assert_return (invoke "i32.load8_u" (i32.const 2)) (i32.const 0x12))
(assert_return (invoke "i32.load8_u" (i32.const 3)) (i32.const 0x13))
(assert_return (invoke "i32.load8_u" (i32.const 4)) (i32.const 0x14))
(assert_return (invoke "i32.load8_u" (i32.const 5)) (i32.const 0x15))
(assert_return (invoke "i32.load8_u" (i32.const 6)) (i32.const 0x16))
(assert_return (invoke "i32.load8_u" (i32.const 7)) (i32.const 0x17))

(assert_return (invoke "memory.copy" (i32.const 8) (i32.const 0) (i32.const 8)))
(assert_return (invoke "i32.load8_u" (i32.const 8 )) (i32.const 0x10))
(assert_return (invoke "i32.load8_u" (i32.const 9 )) (i32.const 0x11))
(assert_return (invoke "i32.load8_u" (i32.const 10)) (i32.const 0x12))
(assert_return (invoke "i32.load8_u" (i32.const 11)) (i32.const 0x13))
(assert_return (invoke "i32.load8_u" (i32.const 12)) (i32.const 0x14))
(assert_return (invoke "i32.load8_u" (i32.const 13)) (i32.const 0x15))
(assert_return (invoke "i32.load8_u" (i32.const 14)) (i32.const 0x16))
(assert_return (invoke "i32.load8_u" (i32.const 15)) (i32.const 0x17))
(assert_return (invoke "i32.load8_u" (i32.const 16)) (i32.const 0x00))

(assert_return (invoke "memory.copy" (i32.const 0) (i32.const 4) (i32.const 8)))
(assert_return (invoke "i32.load8_u" (i32.const 0)) (i32.const 0x14))
(assert_return (invoke "i32.load8_u" (i32.const 1)) (i32.const 0x15))
(assert_return (invoke "i32.load8_u" (i32.const 2)) (i32.const 0x16))
(assert_return (invoke "i32.load8_u" (i32.const 3)) (i32.const 0x17))
(assert_return (invoke "i32.load8_u" (i32.const 4)) (i32.const 0x10))
(assert_return (invoke "i32.load8_u" (i32.const 5)) (i32.const 0x11))
(assert_return (invoke "i32.load8_u" (i32.const 6)) (i32.const 0x12))
(assert_return (invoke "i32.load8_u" (i32.const 7)) (i32.const 0x13))
(assert_return (invoke "i32.load8_u" (i32.const 8)) (i32.const 0x10))

(assert_return (invoke "memory.copy" (i32.const 15) (i32.const 0) (i32.const 7)))
(assert_return (invoke "i32.load8_u" (i32.const 14)) (i32.const 0x16))
(assert_return (invoke "i32.load8_u" (i32.const 15)) (i32.const 0x14))
(assert_return (invoke "i32.load8_u" (i32.const 16)) (i32.const 0x15))
(assert_return (invoke "i32.load8_u" (i32.const 17)) (i32.const 0x16))
(assert_return (invoke "i32.load8_u" (i32.const 18)) (i32.const 0x17))
(assert_return (invoke "i32.load8_u" (i32.const 19)) (i32.const 0x10))
(assert_return (invoke "i32.load8_u" (i32.const 20)) (i32.const 0x11))
(assert_return (invoke "i32.load8_u" (i32.const 21)) (i32.const 0x12))
(assert_return (invoke "i32.load8_u" (i32.const 22)) (i32.const 0))

(assert_return (invoke "memory.copy" (i32.const 4) (i32.const 0) (i32.const 8)))
(assert_return (invoke "i32.load8_u" (i32.const 4)) (i32.const 0x14))
(assert_return (invoke "i32.load8_u" (i32.const 5)) (i32.const 0x15))
(assert_return (invoke "i32.load8_u" (i32.const 6)) (i32.const 0x16))
(assert_return (invoke "i32.load8_u" (i32.const 7)) (i32.const 0x17))
(assert_return (invoke "i32.load8_u" (i32.const 8)) (i32.const 0x10))
(assert_return (invoke "i32.load8_u" (i32.const 9)) (i32.const 0x11))
(assert_return (invoke "i32.load8_u" (i32.const 10)) (i32.const 0x12))
(assert_return (invoke "i32.load8_u" (i32.const 11)) (i32.const 0x13))
(assert_return (invoke "i32.load8_u" (i32.const 12)) (i32.const 0x14))

(assert_return (invoke "memory.copy" (i32.const 65535) (i32.const 0)   (i32.const 1)))
(assert_return (invoke "i32.load8_u" (i32.const 65535)) (i32.const 0x14))
(assert_return (invoke "memory.copy" (i32.const 65535) (i32.const 100)   (i32.const 1)))
(assert_return (invoke "i32.load8_u" (i32.const 65535)) (i32.const 0))
(assert_trap   (invoke "memory.copy" (i32.const 65535) (i32.const 0)     (i32.const 2)) "out of bounds memory access")
(assert_return (invoke "i32.load8_u" (i32.const 65535)) (i32.const 0))
(assert_trap   (invoke "memory.copy" (i32.const 65536) (i32.const 0)     (i32.const 1)) "out of bounds memory access")

(invoke "memory.copy" (i32.const 65535) (i32.const 0) (i32.const 1))
(assert_trap   (invoke "memory.copy" (i32.const 200)   (i32.const 65535) (i32.const 2)) "out of bounds memory access")
(assert_return (invoke "i32.load8_u" (i32.const 200))  (i32.const 0x14))
(assert_trap   (invoke "memory.copy" (i32.const 300)   (i32.const 65535) (i32.const 4)) "out of bounds memory access")
(assert_return (invoke "i32.load8_u" (i32.const 300))  (i32.const 0x14))
(assert_trap   (invoke "memory.copy" (i32.const 400)   (i32.const 65535) (i32.const 8)) "out of bounds memory access")
(assert_return (invoke "i32.load8_u" (i32.const 400))  (i32.const 0x14))
(assert_trap   (invoke "memory.copy" (i32.const 100)   (i32.const 65536) (i32.const 1)) "out of bounds memory access")

(assert_trap   (invoke "memory.copy" (i32.const 0xffffffff) (i32.const 0) (i32.const 1)) "out of bounds memory access")
(assert_trap   (invoke "memory.copy" (i32.const 0) (i32.const 0xffffffff) (i32.const 1)) "out of bounds memory access")

;; memory.fill

(module
	(memory $m 1 1)
	
	(func (export "memory.fill")
		(param $destAddress i32)
		(param $value i32)
		(param $numBytes i32)
		(memory.fill (local.get $destAddress) (local.get $value) (local.get $numBytes))
	)
	
	(func (export "i32.load8_u") (param $address i32) (result i32)
		(i32.load8_u (local.get $address))
	)
)

(assert_return (invoke "memory.fill" (i32.const 0) (i32.const 1) (i32.const 4)))
(assert_return (invoke "i32.load8_u" (i32.const 0)) (i32.const 1))
(assert_return (invoke "i32.load8_u" (i32.const 1)) (i32.const 1))
(assert_return (invoke "i32.load8_u" (i32.const 2)) (i32.const 1))
(assert_return (invoke "i32.load8_u" (i32.const 3)) (i32.const 1))
(assert_return (invoke "i32.load8_u" (i32.const 4)) (i32.const 0))

(assert_return (invoke "memory.fill" (i32.const 4) (i32.const 2) (i32.const 4)))
(assert_return (invoke "i32.load8_u" (i32.const 3)) (i32.const 1))
(assert_return (invoke "i32.load8_u" (i32.const 4)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 5)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 6)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 7)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 8)) (i32.const 0))

(assert_return (invoke "memory.fill" (i32.const 4) (i32.const 2) (i32.const 4)))
(assert_return (invoke "i32.load8_u" (i32.const 4)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 5)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 6)) (i32.const 2))
(assert_return (invoke "i32.load8_u" (i32.const 7)) (i32.const 2))

(assert_return (invoke "memory.fill" (i32.const 65535) (i32.const 0) (i32.const 1)))
(assert_trap   (invoke "memory.fill" (i32.const 65535) (i32.const 0) (i32.const 2)) "out of bounds memory access")
(assert_trap   (invoke "memory.fill" (i32.const 65536) (i32.const 0) (i32.const 1)) "out of bounds memory access")

(assert_trap   (invoke "memory.fill" (i32.const 0xffffffff) (i32.const 0) (i32.const 1)) "out of bounds memory access")

;; passive elem segments

(module (elem funcref (ref.func $f)) (func $f))
(module (elem funcref (ref.null)))
(assert_invalid
	(module (table $t 1 funcref) (elem (i32.const 0) (ref.func $f)) (func $f))
	"unexpected expression"
)
(assert_invalid
	(module (table $t 1 funcref) (elem (i32.const 0) funcref (unreachable)) (func $f))
	"expected 'ref.func' or 'ref.null'"
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\04\04\01"                          ;; table section: 4 bytes, 1 entry
	"\70\00\01"                          ;;   (table 1 funcref)
	
	"\09\07\01"                          ;; elem section: 7 bytes, 1 entry
	"\00"                                ;;   [0] active elem segment
	"\41\00\0b"                          ;;     base offset (i32.const 0)
	"\01"                                ;;     elem segment with 1 element
	"\00"                                ;;     [0] function 0
	
	"\0a\04\01"                          ;; Code section
	"\02\00"                             ;; function 0: 2 bytes, 0 local sets
	"\0b"                                ;; end
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\04\04\01"                          ;; table section: 4 bytes, 1 entry
	"\70\00\01"                          ;;   (table 1 funcref)
	
	"\09\07\01"                          ;; elem section: 7 bytes, 1 entry
	"\05\70"                             ;;   [0] passive elem funcref expression segment
	"\01"                                ;;     elem segment with 1 element
	"\d2\00\0b"                          ;;     [0] ref.func 0
	
	"\0a\04\01"                          ;; Code section
	"\02\00"                             ;; function 0: 2 bytes, 0 local sets
	"\0b"                                ;; end
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\04\04\01"                          ;; table section: 4 bytes, 1 entry
	"\70\00\01"                          ;;   (table 1 funcref)
	
	"\09\06\01"                          ;; elem section: 6 bytes, 1 entry
	"\05\70"                             ;;   [0] passive elem funcref expression segment
	"\01"                                ;;     elem segment with 1 element
	"\d0\0b"                             ;;     [0] ref.null
	
	"\0a\04\01"                          ;; Code section
	"\02\00"                             ;; function 0: 2 bytes, 0 local sets
	"\0b"                                ;; end
)

(assert_invalid
	(module binary
		"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

		"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
		"\60\00\00"                          ;;   Function type () -> ()

		"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
		"\00"                                ;;   Function 0: type 0

		"\04\04\01"                          ;; table section: 4 bytes, 1 entry
		"\70\00\01"                          ;;   (table 1 funcref)
	
		"\09\06\01"                          ;; elem section: 6 bytes, 1 entry
		"\05\70"                             ;;   [0] passive elem funcref expression segment
		"\01"                                ;;     elem segment with 1 element
		"\00\0b"                             ;;     [0] unreachable
	
		"\0a\04\01"                          ;; Code section
		"\02\00"                             ;; function 0: 2 bytes, 0 local sets
		"\0b"                                ;; end
	)
	"invalid elem opcode"
)

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\04\04\01"                          ;; table section: 4 bytes, 1 entry
	"\70\00\01"                          ;;   (table 1 funcref)
	
	"\09\09\01"                          ;; elem section: 8 bytes, 1 entry
	"\02\00"                             ;;   [0] active elem index segment, table 0
	"\41\00\0b"                          ;;     base offset (i32.const 0)
	"\00"                                ;;     func extern kind
	"\01"                                ;;     elem segment with 1 element
	"\00"                                ;;     [0] function 0
	
	"\0a\04\01"                          ;; Code section
	"\02\00"                             ;; function 0: 2 bytes, 0 local sets
	"\0b"                                ;; end
)

;; table.init and elem.drop

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\04\04\01"                          ;; table section: 4 bytes, 1 entry
	"\70\00\01"                          ;;   (table 1 funcref)
	
	"\09\07\01"                          ;; elem section: 7 bytes, 1 entry
	"\05\70"                             ;;   [0] passive elem funcref expression segment
	"\01"                                ;;     elem segment with 1 element
	"\d2\00\0b"                          ;;     [0] ref.func 0
	
	"\0a\11\01"                          ;; Code section
	"\0f\00"                             ;; function 0: 15 bytes, 0 local sets
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\fc\0c\00\00"                       ;; table.init 0 0
	"\fc\0d\00"                          ;; elem.drop 0
	"\0b"                                ;; end
)

;; Test that it's valid to reference an active elem segment with table.init and elem.drop
(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\04\04\01"                          ;; table section: 4 bytes, 1 entry
	"\70\00\01"                          ;;   (table 1 funcref)
	
	"\09\07\01"                          ;; elem section: 7 bytes, 1 entry
	"\00"                                ;;   [0] active elem index segment
	"\41\00\0b"                          ;;     base offset (i32.const 0)
	"\01"                                ;;     elem segment with 1 element
	"\00"                                ;;     [0] function 0
	
	"\0a\11\01"                          ;; Code section
	"\0f\00"                             ;; function 0: 15 bytes, 0 local sets
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\fc\0c\00\00"                       ;; table.init 0 0
	"\fc\0d\00"                          ;; elem.drop 0
	"\0b"                                ;; end
)

(module
	(table $t 8 8 funcref)
	
	(type $type_i32 (func (result i32)))
	(type $type_i64 (func (result i64)))

	(elem funcref (ref.func $0) (ref.func $1))
	(elem funcref (ref.func $2) (ref.func $3))

	(func $0 (type $type_i32) (result i32) i32.const 0)
	(func $1 (type $type_i32) (result i32) i32.const 1)
	(func $2 (type $type_i64) (result i64) i64.const 2)
	(func $3 (type $type_i64) (result i64) i64.const 3)

	(func (export "call_indirect i32") (param $index i32) (result i32)
		(call_indirect (type $type_i32) (local.get $index))
	)
	
	(func (export "call_indirect i64") (param $index i32) (result i64)
		(call_indirect (type $type_i64) (local.get $index))
	)

	(func (export "table.init 0")
		(param $destOffset i32)
		(param $sourceOffset i32)
		(param $numElements i32)
		(table.init 0 $t (local.get $destOffset) (local.get $sourceOffset) (local.get $numElements))
	)
	
	(func (export "table.init 1")
		(param $destOffset i32)
		(param $sourceOffset i32)
		(param $numElements i32)
		(table.init 1 $t (local.get $destOffset) (local.get $sourceOffset) (local.get $numElements))
	)
	
	(func (export "elem.drop 0") (elem.drop 0))
	(func (export "elem.drop 1") (elem.drop 1))
)

(assert_trap   (invoke "call_indirect i32" (i32.const 0)) "uninitialized element")

(assert_return (invoke "table.init 0" (i32.const 0) (i32.const 0) (i32.const 2)))
(assert_return (invoke "call_indirect i32" (i32.const 0)) (i32.const 0))
(assert_return (invoke "call_indirect i32" (i32.const 1)) (i32.const 1))
(assert_trap   (invoke "call_indirect i32" (i32.const 2)) "uninitialized element")

(assert_return (invoke "table.init 1" (i32.const 2) (i32.const 0) (i32.const 2)))
(assert_return (invoke "call_indirect i64" (i32.const 2)) (i64.const 2))
(assert_return (invoke "call_indirect i64" (i32.const 3)) (i64.const 3))
(assert_trap   (invoke "call_indirect i64" (i32.const 4)) "uninitialized element")

(assert_return (invoke "table.init 0" (i32.const 4) (i32.const 0) (i32.const 1)))
(assert_return (invoke "call_indirect i32" (i32.const 4)) (i32.const 0))
(assert_trap   (invoke "call_indirect i32" (i32.const 5)) "uninitialized element")

(assert_return (invoke "table.init 1" (i32.const 5) (i32.const 1) (i32.const 1)))
(assert_return (invoke "call_indirect i64" (i32.const 5)) (i64.const 3))
(assert_trap   (invoke "call_indirect i64" (i32.const 6)) "uninitialized element")

(assert_trap   (invoke "table.init 0" (i32.const 8) (i32.const 0) (i32.const 1)) "undefined element")
(assert_trap   (invoke "table.init 0" (i32.const 7) (i32.const 0) (i32.const 2)) "undefined element")
(assert_trap   (invoke "table.init 0" (i32.const 6) (i32.const 1) (i32.const 2)) "out of bounds elem segment access")
(assert_trap   (invoke "table.init 0" (i32.const 5) (i32.const 0) (i32.const 3)) "out of bounds elem segment access element")

(assert_trap   (invoke "table.init 0" (i32.const 0xffffffff) (i32.const 0) (i32.const 1)) "undefined element")
(assert_trap   (invoke "table.init 0" (i32.const 0) (i32.const 0xffffffff) (i32.const 1)) "out of bounds elem segment access")

(assert_return (invoke "elem.drop 0"))
(assert_trap   (invoke "table.init 0" (i32.const 0) (i32.const 0) (i32.const 2)) "invalid argument")
(assert_trap   (invoke "elem.drop 0") "invalid argument")
(assert_return (invoke "table.init 1" (i32.const 0) (i32.const 0) (i32.const 2)))
(assert_return (invoke "call_indirect i64" (i32.const 0)) (i64.const 2))
(assert_return (invoke "call_indirect i64" (i32.const 1)) (i64.const 3))

(assert_return (invoke "elem.drop 1"))
(assert_trap   (invoke "table.init 1" (i32.const 0) (i32.const 0) (i32.const 2)) "invalid argument")
(assert_trap   (invoke "elem.drop 1") "invalid argument")

;; table.init with (ref.null) elems

(module
	(table $t 3 3 funcref)
	
	(type $type_i32 (func (result i32)))
	(type $type_i64 (func (result i64)))

	(elem funcref (ref.func $0) (ref.null) (ref.func $1))

	(func $0 (type $type_i32) (result i32) i32.const 0)
	(func $1 (type $type_i32) (result i32) i32.const 1)

	(func (export "call_indirect") (param $index i32) (result i32)
		(call_indirect (type $type_i32) (local.get $index))
	)

	(func (export "table.init")
		(param $destOffset i32)
		(param $sourceOffset i32)
		(param $numElements i32)
		(table.init 0 $t (local.get $destOffset) (local.get $sourceOffset) (local.get $numElements))
	)
)

(assert_trap   (invoke "call_indirect" (i32.const 0)) "uninitialized element")

(assert_return (invoke "table.init" (i32.const 0) (i32.const 0) (i32.const 3)))
(assert_return (invoke "call_indirect" (i32.const 0)) (i32.const 0))
(assert_trap   (invoke "call_indirect" (i32.const 1)) "uninitialized element")
(assert_return (invoke "call_indirect" (i32.const 2)) (i32.const 1))

;; table.copy

(module binary
	"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

	"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
	"\60\00\00"                          ;;   Function type () -> ()

	"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
	"\00"                                ;;   Function 0: type 0

	"\04\04\01"                          ;; table section: 4 bytes, 1 entry
	"\70\00\01"                          ;;   (table 1 funcref)
	
	"\09\05\01"                          ;; elem section: 6 bytes, 1 entry
	"\01\00"                             ;;   [0] passive elem function index segment
	"\01"                                ;;     elem segment with 1 element
	"\00"                                ;;     [0] function 0
	
	"\0a\0e\01"                          ;; Code section
	"\0c\00"                             ;; function 0: 12 bytes, 0 local sets
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\41\00"                             ;; i32.const 0
	"\fc\0e\00\00"                       ;; table.copy 0 0
	"\0b"                                ;; end
)

(assert_invalid
	(module binary
		"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

		"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
		"\60\00\00"                          ;;   Function type () -> ()

		"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
		"\00"                                ;;   Function 0: type 0

		"\04\04\01"                          ;; table section: 4 bytes, 1 entry
		"\70\00\01"                          ;;   (table 1 funcref)
	
		"\0a\0f\01"                          ;; Code section
		"\0c\00"                             ;; function 0: 12 bytes, 0 local sets
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\fc\0e\01\00"                       ;; table.copy 1 0
		"\0b"                                ;; end
	)
	"invalid table index"
)

(assert_invalid
	(module binary
		"\00asm" "\01\00\00\00"              ;; WebAssembly version 1

		"\01\04\01"                          ;; Type section: 4 bytes, 1 entry
		"\60\00\00"                          ;;   Function type () -> ()

		"\03\02\01"                          ;; Function section: 2 bytes, 1 entry
		"\00"                                ;;   Function 0: type 0

		"\04\04\01"                          ;; table section: 4 bytes, 1 entry
		"\70\00\01"                          ;;   (table 1 funcref)
	
		"\0a\0f\01"                          ;; Code section
		"\0c\00"                             ;; function 0: 12 bytes, 0 local sets
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\41\00"                             ;; i32.const 0
		"\fc\0e\00\01"                       ;; table.copy 0 1
		"\0b"                                ;; end
	)
	"invalid table index"
)

(module
	(table $t 8 8 funcref)
	
	(type $type_i32 (func (result i32)))
	(type $type_i64 (func (result i64)))

	(elem (table $t) (i32.const 0) $0 $1 $2 $3)

	(func $0 (type $type_i32) (result i32) i32.const 0)
	(func $1 (type $type_i32) (result i32) i32.const 1)
	(func $2 (type $type_i64) (result i64) i64.const 2)
	(func $3 (type $type_i64) (result i64) i64.const 3)

	(func (export "call_indirect i32") (param $index i32) (result i32)
		(call_indirect (type $type_i32) (local.get $index))
	)
	
	(func (export "call_indirect i64") (param $index i32) (result i64)
		(call_indirect (type $type_i64) (local.get $index))
	)

	(func (export "table.copy")
		(param $destOffset i32)
		(param $sourceOffset i32)
		(param $numElements i32)
		(table.copy (local.get $destOffset) (local.get $sourceOffset) (local.get $numElements))
	)
)

(assert_trap   (invoke "call_indirect i32" (i32.const 4)) "uninitialized element")
(assert_return (invoke "table.copy" (i32.const 4) (i32.const 0) (i32.const 4)))
(assert_return (invoke "call_indirect i32" (i32.const 4)) (i32.const 0))
(assert_return (invoke "call_indirect i32" (i32.const 4)) (i32.const 0))
(assert_return (invoke "call_indirect i32" (i32.const 5)) (i32.const 1))
(assert_return (invoke "call_indirect i64" (i32.const 6)) (i64.const 2))
(assert_return (invoke "call_indirect i64" (i32.const 7)) (i64.const 3))

(assert_return (invoke "table.copy" (i32.const 3) (i32.const 2) (i32.const 2)))
(assert_return (invoke "call_indirect i64" (i32.const 3)) (i64.const 2))
(assert_return (invoke "call_indirect i64" (i32.const 4)) (i64.const 3))

(assert_trap   (invoke "table.copy" (i32.const 8) (i32.const 0) (i32.const 1)) "undefined element")
(assert_trap   (invoke "table.copy" (i32.const 7) (i32.const 0) (i32.const 2)) "undefined element")
(assert_trap   (invoke "table.copy" (i32.const 0) (i32.const 8) (i32.const 1)) "undefined element")
(assert_trap   (invoke "table.copy" (i32.const 0) (i32.const 7) (i32.const 2)) "undefined element")

(assert_trap   (invoke "table.copy" (i32.const 0xffffffff) (i32.const 0) (i32.const 1)) "undefined element")
(assert_trap   (invoke "table.copy" (i32.const 0) (i32.const 0xffffffff) (i32.const 1)) "undefined element")
