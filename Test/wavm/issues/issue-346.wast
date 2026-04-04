;; #346: Unexpected execution (over-alignment not rejected)

;; Original regression test from issue report. The alignment byte is 0x40 (log2=64),
;; which is rejected during deserialization (alignment >= 64 is malformed).
(assert_malformed
  (module binary
    "\00\61\73\6d\01\00\00\00\01\10\03\60\04\7f\7f\7f\7f\01\7f"
    "\60\01\7f\00\60\00\00\03\02\01\02\05\03\01\00\01\06\5d\0c"
    "\7f\00\41\9d\04\0b\7f\01\41\bf\01\0b\7d\00\43\00\40\07\44"
    "\0b\7d\01\43\00\00\40\43\0b\7e\00\42\36\0b\7e\01\42\13\0b"
    "\7c\00\44\00\00\00\00\00\00\4b\40\0b\7c\01\44\00\00\00\00"
    "\00\00\33\40\0b\7f\01\41\00\0b\7d\01\43\00\00\00\00\0b\7e"
    "\01\42\00\0b\7c\01\44\00\00\00\00\00\00\00\00\0b\07\14\02"
    "\06\5f\73\74\61\72\74\00\00\07\74\6f\5f\74\65\73\74\00\00"
    "\0a\49\01\47\04\01\7f\01\7d\01\7e\8a\01\7c\41\c0\91\91\01"
    "\21\00\43\d9\c1\74\3f\0f\21\01\42\a5\8c\90\90\80\80\91\7b"
    "\21\02\44\00\0a\55\8c\b0\f0\c5\c6\21\03\41\01\41\00\36\40"
    "\8b\20\00\24\08\20\01\24\09\20\02\24\0a\20\13\24\0b\0b\0b"
    "\32\03\00\41\08\0b\08\10\00\00\00\0d\00\00\00\00\41\10\0b"
    "\0d\48\65\6c\6c\6f\20\57\6f\72\6c\64\21\0a\00\41\20\0b\0d"
    "\48\65\6c\6c\6f\20\57\6f\72\6c\64\21\0a")
  "alignment")

;; Minimal over-alignment: i32.store with alignment log2 = 4 (16 bytes),
(assert_invalid
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"                 ;; type section: (func)
    "\03\02\01\00"                       ;; function section: func 0 is type 0
    "\05\03\01\00\01"                    ;; memory section: (memory 1)
    "\0a\0b\01"                          ;; code section: 1 function
    "\09\00"                             ;; body: 9 bytes, 0 locals
    "\41\00"                             ;; i32.const 0
    "\41\00"                             ;; i32.const 0
    "\36\04\00"                          ;; i32.store align=2^4 offset=0
    "\0b"                                ;; end
  )
  "alignment")

;; Maximum well-formed over-alignment: i32.store with alignment log2 = 63 (2^63 bytes),
(assert_invalid
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"                 ;; type section: (func)
    "\03\02\01\00"                       ;; function section: func 0 is type 0
    "\05\03\01\00\01"                    ;; memory section: (memory 1)
    "\0a\0b\01"                          ;; code section: 1 function
    "\09\00"                             ;; body: 9 bytes, 0 locals
    "\41\00"                             ;; i32.const 0
    "\41\00"                             ;; i32.const 0
    "\36\3f\00"                          ;; i32.store align=2^63 offset=0
    "\0b"                                ;; end
  )
  "alignment")

;; Malformed over-alignment that avoids the 0x40 memory index flag: i32.store with alignment log2 = 128 (2^128 bytes),
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"                 ;; type section: (func)
    "\03\02\01\00"                       ;; function section: func 0 is type 0
    "\05\03\01\00\01"                    ;; memory section: (memory 1)
    "\0a\0b\01"                          ;; code section: 1 function
    "\09\00"                             ;; body: 9 bytes, 0 locals
    "\41\00"                             ;; i32.const 0
    "\41\00"                             ;; i32.const 0
    "\36\80\01\00"                       ;; i32.store align=2^128 offset=0
    "\0b"                                ;; end
  )
  "alignment")
