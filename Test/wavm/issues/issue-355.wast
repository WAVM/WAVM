;; #355: Unexpected execution due to illegal opcode (0xec)

(assert_malformed
  (module binary
    "\00\61\73\6d\01\00\00\00\01\08\02\60\00\00\60\00\01\7b\03"
    "\02\01\01\07\14\02\06\5f\73\74\61\72\74\00\00\07\74\6f\5f"
    "\74\65\73\74\00\00\0a\22\01\20\04\01\7f\01\7d\01\7e\01\7c"
    "\fd\0c\a5\ed\29\da\2c\65\7b\54\2d\25\26\93\c8\bb\03\e6\ec"
    "\fd\fa\01\0b")
  "unknown opcode")
