;; WebAssembly WASM AST Hello World! program

(module
  (memory 1
   (segment 8 "Hello World!\n")
  )
  (import $__fwrite "env" "_fwrite" (param i32 i32 i32 i32) (result i32))
  (import $_get__stdout "env" "get__stdout" (param) (result i32))
  (export "main" $main)

  (func $main (result i32)
    (local $stdout i32)
    (set_local $stdout (call_import $_get__stdout))

    (return (call_import $__fwrite
       (i32.const 8)         ;; void *ptr    => Address of our string
       (i32.const 1)         ;; size_t size  => Data size
       (i32.const 13)        ;; size_t nmemb => Length of our string
       (get_local $stdout))  ;; stream
    )
  )
)
