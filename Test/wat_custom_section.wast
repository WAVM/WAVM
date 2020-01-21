(module
  (custom_section "a" "")
)

(module
  (type (func))
  (custom_section "b" (after type) "")
)

(module
  (import "spectest" "print_i32" (func (param i32)))
  (custom_section "c" (after import) "")
)

(module
  (func)
  (custom_section "d" (after func) "")
)

(module
  (table 0 0 funcref)
  (custom_section "e" (after table) "")
)

(module
  (memory 1)
  (custom_section "f" (after memory) "")
)

(module
  (global i32 (i32.const 0))
  (custom_section "g" (after global) "")
)

(module
  (exception_type)
  (custom_section "h" (after exception_type) "")
)

(module
  (func)
  (export "func" (func 0))
  (custom_section "i" (after export) "")
)

(module
  (func)
  (start 0)
  (custom_section "j" (after start) "")
)

(module
  (elem)
  (custom_section "j" (after elem) "")
)

(module
  (data)
  (custom_section "k" (after data_count) "")
)

(module
  (func)
  (custom_section "l" (after code) "")
)

(module
  (data)
  (custom_section "m" (after data) "")
)

;; Custom sections after non-present virtual sections

(assert_malformed
  (module quote "(custom_section \"b\" (after type) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"c\" (after import) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"d\" (after func) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"e\" (after table) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"f\" (after memory) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"g\" (after global) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"h\" (after exception_type) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"i\" (after export) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"j\" (after start) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"j\" (after elem) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"k\" (after data_count) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"l\" (after code) \"\")")
  "custom section is after a virtual section that is not present"
)

(assert_malformed
  (module quote "(custom_section \"m\" (after data) \"\")")
  "custom section is after a virtual section that is not present"
)

;; Out-of-order custom sections

(assert_malformed
  (module quote
    "(custom_section \"a\" (after type) \"\")"
    "(custom_section \"b\" \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after import) \"\")"
    "(custom_section \"b\" (after type) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after func) \"\")"
    "(custom_section \"b\" (after import) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after table) \"\")"
    "(custom_section \"b\" (after func) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after memory) \"\")"
    "(custom_section \"b\" (after table) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after global) \"\")"
    "(custom_section \"b\" (after memory) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after exception_type) \"\")"
    "(custom_section \"b\" (after global) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after export) \"\")"
    "(custom_section \"b\" (after exception_type) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after start) \"\")"
    "(custom_section \"b\" (after export) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after elem) \"\")"
    "(custom_section \"b\" (after start) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after data_count) \"\")"
    "(custom_section \"b\" (after elem) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after code) \"\")"
    "(custom_section \"b\" (after data_count) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after data) \"\")"
    "(custom_section \"b\" (after code) \"\")"
  )
  "out-of-order custom section"
)

(assert_malformed
  (module quote
    "(custom_section \"a\" (after data) \"\")"
    "(custom_section \"b\" (after code) \"\")"
  )
  "out-of-order custom section"
)

;; non UTF-8 custom section name
(assert_malformed
  (module quote "(custom_section \"\\80\" \"\")")
  "invalid UTF-8 encoding"
)

;; non UTF-8 custom section contents
(module (custom_section "a" "\80"))
