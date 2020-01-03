;; Classic Unix echo program in WebAssembly WASM AST

(module
  (import "wasi_unstable" "args_get" (func $wasi_args_get (param i32 i32) (result i32)))
  (import "wasi_unstable" "args_sizes_get" (func $wasi_args_sizes_get (param i32 i32) (result i32)))
  (import "wasi_unstable" "fd_write" (func $wasi_fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_unstable" "proc_exit" (func $wasi_proc_exit (param i32)))

  (memory (export "memory") 1)
  
  (global $spaceAddress i32 (i32.const 0))
  (data (i32.const 0) " ")

  (global $newlineAddress i32 (i32.const 1))
  (data (i32.const 1) "\n")

  (global $argcAddress i32 (i32.const 4))
  (global $argBufferNumBytesAddress i32 (i32.const 8))

  (global $numBytesWrittenAddress i32 (i32.const 12))

  (global $dynamicTopAddress (mut i32) (i32.const 128))

  (func $sbrk (param $numBytes i32) (result i32)
    (local $resultAddress i32)
    (local.set $numBytes (i32.and (i32.add (local.get $numBytes) (i32.const 15)) (i32.const -16)))
    (local.set $resultAddress (global.get $dynamicTopAddress))
    (global.set $dynamicTopAddress (i32.add (local.get $resultAddress) (local.get $numBytes)))
    (local.get $resultAddress)
    )

  (func $strlen (param $s i32) (result i32)
    (local $head i32)
    (local.set $head (local.get $s))
    (loop $loop
      (block $done
        (br_if $done (i32.eq (i32.const 0) (i32.load8_u offset=0 (local.get $head))))
        (local.set $head (i32.add (local.get $head) (i32.const 1)))
        (br $loop)
      )
    )
    (return (i32.sub (local.get $head) (local.get $s)))
  )

  (func $main (export "_start")
    (local $result i32)
    (local $argc i32)
    (local $argBufferNumBytes i32)
    (local $argvAddress i32)
    (local $argBufferAddress i32)
    (local $iovAddress i32)
    (local $numIOVs i32)
    (local $argIndex i32)
    (local $iobufAddress i32)
    (local $argAddress i32)
    (local $argNumBytes i32)

    (block $exit
      ;; Query the number of arguments, and the total number of characters needed to store the arg strings.
      (local.set $result (call $wasi_args_sizes_get (global.get $argcAddress) (global.get $argBufferNumBytesAddress)))
      (br_if $exit (i32.ne (local.get $result) (i32.const 0)))
      (local.set $argc (i32.load (global.get $argcAddress)))
      (local.set $argBufferNumBytes (i32.load (global.get $argBufferNumBytesAddress)))

      ;; Allocate buffers for argv and the arg strings.
      (local.set $argvAddress (call $sbrk (i32.mul (local.get $argc) (i32.const 4))))
      (local.set $argBufferAddress (call $sbrk (local.get $argBufferNumBytes)))

      ;; Read the arguments into the buffers.
      (local.set $result (call $wasi_args_get (local.get $argvAddress) (local.get $argBufferAddress)))
      (br_if $exit (i32.ne (local.get $result) (i32.const 0)))

      ;; Allocate an iovec with 2*(argc-1) buffers.
      (local.set $numIOVs (i32.mul (i32.sub (local.get $argc) (i32.const 1)) (i32.const 2)))
      (local.set $iovAddress (call $sbrk (i32.mul (local.get $numIOVs) (i32.const 8))))
      (local.set $iobufAddress (local.get $iovAddress))

      (local.set $argIndex (i32.const 1))
      (block $done
        (loop $loop
          (br_if $done (i32.eq (local.get $argIndex) (local.get $argc)))

          ;; Read the argument string address from argv, and calculate its length.
          (local.set $argAddress (i32.load (i32.add (local.get $argvAddress) (i32.mul (local.get $argIndex) (i32.const 4)))))
          (local.set $argNumBytes (call $strlen (local.get $argAddress)))

          ;; Add two buffers to the iovec: one that points to the argument string, and one that
          ;; points to either space or newline (after the last argument).
          (i32.store offset=0 (local.get $iobufAddress) (local.get $argAddress))
          (i32.store offset=4 (local.get $iobufAddress) (local.get $argNumBytes))
          (i32.store offset=8 (local.get $iobufAddress) (select
            (global.get $newlineAddress)
            (global.get $spaceAddress)
            (i32.eq (local.get $argc) (i32.add (local.get $argIndex) (i32.const 1)))
          ))
          (i32.store offset=12 (local.get $iobufAddress) (i32.const 1))

          ;; Advance to the next argument.
          (local.set $argIndex (i32.add (local.get $argIndex) (i32.const 1)))
          (local.set $iobufAddress (i32.add (local.get $iobufAddress) (i32.const 16))) ;; 2 iobufs/arg
          (br $loop)
        )
      )

      ;; Pass the iovec containing all the arguments to fd_write.
      (local.set $result (call $wasi_fd_write
        (i32.const 1)
        (local.get $iovAddress)
        (local.get $numIOVs)
        (global.get $numBytesWrittenAddress)))
    )

    (call $wasi_proc_exit (local.get $result))
  )
)
