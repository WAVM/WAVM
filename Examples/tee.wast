;; poor man's tee
;; Outputs to stderr and stdout whatever comes in stdin

(module

  (import "wasi_unstable" "fd_read" (func $wasi_fd_read (param i32 i32 i32 i32) (result i32)))
  (import "wasi_unstable" "fd_write" (func $wasi_fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_unstable" "proc_exit" (func $wasi_proc_exit (param i32)))

  (memory (export "memory") 1)

  (global $ioVecAddress i32 (i32.const 0))
  (global $ioBufferAddress i32 (i32.const 8))
  (global $ioBufferNumBytes i32 (i32.const 1024))
  
  (func $main (export "_start")
    (local $result i32)
    (loop $loop
      (block $done
        (i32.store offset=0 (global.get $ioVecAddress) (global.get $ioBufferAddress))
        (i32.store offset=4 (global.get $ioVecAddress) (global.get $ioBufferNumBytes))

        ;; Read up to $ioBufferNumBytes from stdin.
        (local.set $result (call $wasi_fd_read
          (i32.const 0)                                      ;; fd = stdin
          (global.get $ioVecAddress)                         ;; iovec = $ioVecAddress
          (i32.const 1)                                      ;; 1 iovec
          (i32.add (global.get $ioVecAddress) (i32.const 4)) ;; store the number of bytes read back to iovec.buf_len
        ))

        ;; Break out of the loop if fd_read returned an error, or 0 bytes were read.
        (br_if $done (i32.or
          (i32.ne (i32.const 0) (local.get $result))
          (i32.eq (i32.const 0) (i32.load offset=4 (global.get $ioVecAddress)))
        ))

        ;; Write the bytes read from stdin to stdout.
        (local.set $result (call $wasi_fd_write
          (i32.const 1)                                      ;; fd = stdout
          (global.get $ioVecAddress)                         ;; iovec = $ioVecAddress
          (i32.const 1)                                      ;; 1 iovec
          (i32.add (global.get $ioVecAddress) (i32.const 4)) ;; store the number of bytes written back to iovec.buf_len
        ))
        (br_if $done (i32.ne (i32.const 0) (local.get $result)))

        ;; Write the bytes read from stdin to stderr.
        (local.set $result (call $wasi_fd_write
          (i32.const 1)                                      ;; fd = stdout
          (global.get $ioVecAddress)                         ;; iovec = $ioVecAddress
          (i32.const 1)                                      ;; 1 iovec
          (i32.add (global.get $ioVecAddress) (i32.const 4)) ;; store the number of bytes written back to iovec.buf_len
        ))
        (br_if $done (i32.ne (i32.const 0) (local.get $result)))

        (br $loop)
      )
    )

    (call $wasi_proc_exit (local.get $result))
  )
)
