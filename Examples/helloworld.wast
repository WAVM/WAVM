;; WebAssembly WASM AST Hello World! program

(module
  (import "wasi_unstable" "fd_write" (func $wasi_fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_unstable" "proc_exit" (func $wasi_proc_exit (param i32)))

  (memory (export "memory") 1)
  
  ;; The iovec that is passed to fd_write.
  (data (i32.const 8)
    "\10\00\00\00" ;; buf = 16
    "\0d\00\00\00" ;; buf_len = 13
  )

  ;; The string that the iovec points to.
  (data (i32.const 16) "Hello World!\n")

  (func $main (export "_start")
    ;; Print "Hello World!\n" to stdout.
    (call $wasi_fd_write
      (i32.const 1)   ;; fd 1 (stdout)
      (i32.const 8)   ;; (iovec*)8
      (i32.const 1)   ;; 1 iovec
      (i32.const 12)) ;; write the number of written bytes back to iovec.buf_len

    ;; Pass the result of wasi_fd_write to wasi_proc_exit
    (call $wasi_proc_exit)
  )
)
