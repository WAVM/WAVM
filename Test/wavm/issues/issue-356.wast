;; #356: Unexpected execution due to illegal opcode (0xf9)

(assert_malformed
  (module binary
    "\00\61\73\6d\01\00\00\00\01\08\02\60\00\00\60\00\01\7b\03"
    "\02\01\01\07\14\02\06\5f\73\74\61\72\74\00\00\07\74\6f\5f"
    "\74\65\73\74\00\00\0a\21\01\1f\04\01\7f\01\7d\01\7e\01\7c"
    "\fd\0c\5e\5a\2a\f6\a9\f3\10\0d\41\53\d2\28\7b\ac\70\26\f9"
    "\fd\69\0b")
  "unknown opcode")
