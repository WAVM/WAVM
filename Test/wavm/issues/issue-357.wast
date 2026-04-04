;; #357: Unexpected execution (illegal opcode 0xfd3d70)

(assert_malformed
  (module binary
    "\00\61\73\6d\01\00\00\00\01\08\02\60\00\00\60\00\01\7b\03"
    "\02\01\01\07\14\02\06\5f\73\74\61\72\74\00\00\07\74\6f\5f"
    "\74\65\73\74\00\00\0a\33\01\31\04\01\7f\01\7d\01\7e\01\7c"
    "\fd\0c\a4\9d\62\4c\7a\de\68\ca\d6\dc\67\9d\23\f2\d3\5e\fd"
    "\0c\93\d6\65\00\35\72\da\5a\e2\91\7b\fd\c6\98\9f\30\fd\f0"
    "\7a\0b")
  "unknown opcode")
