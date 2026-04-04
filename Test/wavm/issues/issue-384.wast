;; #384: A stack buffer overflow in serializeSection function

(assert_malformed
  (module binary
    "\00\61\73\6d\01\00\00\00\01\04\01\60\00\00\03\00\61\73\6d"
    "\01\00\00\00\01\04\01\60\00\00\03\00\00\00\0a\04\01\02\00"
    "\76\02\01\00\0a\04\01\02\00\76")
  "expected data but found end of stream")
