;; poor man's tee
;; Outputs to stderr and stdout whatever comes in stdin

(module
  (import $__fwrite "env" "_fwrite" (param i32 i32 i32 i32) (result i32))
  (import $__fread "env" "_fread" (param i32 i32 i32 i32) (result i32))
  (import $_get__stdin "env" "get__stdin" (param) (result i32))
  (import $_get__stdout "env" "get__stdout" (param) (result i32))
  (import $_get__stderr "env" "get__stderr" (param) (result i32))
  (export "main" $main)

  (func $main
    (local $stdin i32)
    (local $stdout i32)
    (local $stderr i32)
    (local $nmemb i32)
    (set_local $stdin (i32.load align=4 (call_import $_get__stdin)))
    (set_local $stdout (i32.load align=4 (call_import $_get__stdout)))
    (set_local $stderr (i32.load align=4 (call_import $_get__stderr)))

    (loop $done $loop
      (set_local $nmemb (call_import $__fread (i32.const 0) (i32.const 1) (i32.const 32) (get_local $stdin)))
      (br_if $done (i32.eq (i32.const 0) (get_local $nmemb)))
      (call_import $__fwrite (i32.const 0) (i32.const 1) (get_local $nmemb) (get_local $stdout))
      (call_import $__fwrite (i32.const 0) (i32.const 1) (get_local $nmemb) (get_local $stderr))
      (br $loop)
    )
  )
)
