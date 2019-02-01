;; Classic Unix echo program in WebAssembly WASM AST

(module
  (data (i32.const 8) "\n\00")
  (data (i32.const 12) " \00")

  (import "env" "memory" (memory 1))
  (import "env" "_fwrite" (func $__fwrite (param i32 i32 i32 i32) (result i32)))
  (import "env" "_stdout" (global $stdoutPtr i32))
  (export "main" (func $main))

  (func $strlen (param $s i32) (result i32)
    (local $head i32)
    (local.set $head (local.get $s))
    (loop $loop
	  (block $done
        (br_if $done (i32.eq (i32.const 0) (i32.load8_u offset=0 (local.get $head))))
        (local.set $head (i32.add (local.get $head) (i32.const 1)))
        ;; Would it be worth unrolling offset 1,2,3?
        (br $loop)
      )
    )
    (return (i32.sub (local.get $head) (local.get $s)))
  )

  (func $fputs (param $s i32) (param $stream i32) (result i32)
    (local $len i32)
    (local.set $len (call $strlen (local.get $s)))
    (return (call $__fwrite
      (local.get $s)        ;; ptr
      (i32.const 1)         ;; size_t size  => Data size
      (local.get $len)      ;; size_t nmemb => Length of our string
      (local.get $stream))) ;; stream
  )

  (func $main (param $argc i32) (param $argv i32) (result i32)
    (local $stdout i32)
    (local $s i32)
    (local $space i32)
    (local.set $space (i32.const 0))
    (local.set $stdout (i32.load align=4 (global.get $stdoutPtr)))

    (loop $loop
      (block $done
        (local.set $argv (i32.add (local.get $argv) (i32.const 4)))
        (local.set $s (i32.load (local.get $argv)))
        (br_if $done (i32.eq (i32.const 0) (local.get $s)))

        (if (i32.eq (i32.const 1) (local.get $space))
          (drop (call $fputs (i32.const 12) (local.get $stdout))) ;; ' '
        )
        (local.set $space (i32.const 1))

        (drop (call $fputs (local.get $s) (local.get $stdout)))
        (br $loop)
      )
    )

    (drop (call $fputs (i32.const 8) (local.get $stdout))) ;; \n

    (return (i32.const 0))
  )
)
