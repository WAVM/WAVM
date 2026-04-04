;; #354: Unexpected execution (type mismatch not rejected)

;; Original test case: local.tee $0 (i32) followed by f32.floor in unreachable code.
(assert_invalid
  (module binary
    "\00\61\73\6d\01\00\00\00\01\08\02\60\00\00\60\00\01\7b\03"
    "\02\01\01\07\14\02\06\5f\73\74\61\72\74\00\00\07\74\6f\5f"
    "\74\65\73\74\00\00\0a\36\01\34\04\02\7f\01\7d\01\7e\01\7c"
    "\fd\0c\36\0f\4c\a7\74\11\65\a7\e2\01\d3\66\0d\f1\0b\fd\0c"
    "\00\98\00\00\22\00\8e\00\89\00\00\91\00\00\be\00\00\00\00"
    "\00\fd\95\01\0b")
  "type mismatch")

;; Minimal reproduction: local.tee pushes i32, f32.neg expects f32.
(assert_invalid
  (module
    (func (local i32)
      unreachable
      local.tee 0  ;; pops unknown, must push i32 (not none)
      f32.neg      ;; expects f32, should find i32
      drop))
  "type mismatch")

;; Verify local.tee in unreachable code pushes the correct type for i64.
(assert_invalid
  (module
    (func (local i64)
      unreachable
      local.tee 0
      i32.eqz      ;; expects i32, should find i64
      drop))
  "type mismatch")

;; Verify the fix doesn't break valid uses of local.tee in unreachable code.
(module
  (func (result i32) (local i32)
    unreachable
    local.tee 0
    i32.add))
