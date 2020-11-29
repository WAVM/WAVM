(module
  (memory i32 1000)
  
  (func (export "i64 copy loop")
    (param $dest i32) (param $source i32) (param $numBytes i32)
    block $exitCopyLoop
      loop $copyLoop
        (br_if $exitCopyLoop (i32.eq (local.get $numBytes) (i32.const 0)))
        (i64.store
          (local.get $dest)
          (i64.load (local.get $source)))
        (local.set $numBytes (i32.sub (local.get $numBytes) (i32.const 8)))
        (local.set $dest (i32.add (local.get $dest) (i32.const 8)))
        (local.set $source (i32.add (local.get $source) (i32.const 8)))
        (br $copyLoop)
      end
    end
  )

  (func (export "i64 unrolled copy loop")
    (param $dest i32) (param $source i32) (param $numBytes i32)
    block $exitCopyLoop
      loop $copyLoop
        (br_if $exitCopyLoop (i32.eq (local.get $numBytes) (i32.const 0)))
        (i64.store
          offset=0
          (local.get $dest)
          (i64.load offset=0 (local.get $source)))
        (i64.store
          offset=8
          (local.get $dest)
          (i64.load offset=8 (local.get $source)))
        (i64.store
          offset=16
          (local.get $dest)
          (i64.load offset=24 (local.get $source)))
        (i64.store
          offset=24
          (local.get $dest)
          (i64.load offset=24 (local.get $source)))
        (local.set $numBytes (i32.sub (local.get $numBytes) (i32.const 32)))
        (local.set $dest (i32.add (local.get $dest) (i32.const 32)))
        (local.set $source (i32.add (local.get $source) (i32.const 32)))
        (br $copyLoop)
      end
    end
  )

  (func (export "memory.copy")
    (param $dest i32) (param $source i32) (param $numBytes i32)
    (memory.copy (local.get $dest) (local.get $source) (local.get $numBytes))
  )
)

(benchmark "32-bit i64 copy loop         " (invoke "i64 copy loop"          (i32.const 0) (i32.const 8) (i32.const 2097152)))
(benchmark "32-bit i64 unrolled copy loop" (invoke "i64 unrolled copy loop" (i32.const 0) (i32.const 8) (i32.const 2097152)))
(benchmark "32-bit memory.copy           " (invoke "memory.copy"            (i32.const 0) (i32.const 8) (i32.const 2097152)))

(module
  (memory i64 1000)

  (func (export "i64 copy loop")
    (param $dest i64) (param $source i64) (param $numBytes i64)
    block $exitCopyLoop
      loop $copyLoop
        (br_if $exitCopyLoop (i64.eq (local.get $numBytes) (i64.const 0)))
        (i64.store
          (local.get $dest)
          (i64.load (local.get $source)))
        (local.set $numBytes (i64.sub (local.get $numBytes) (i64.const 8)))
        (local.set $dest (i64.add (local.get $dest) (i64.const 8)))
        (local.set $source (i64.add (local.get $source) (i64.const 8)))
        (br $copyLoop)
      end
    end
  )

  (func (export "i64 unrolled copy loop")
    (param $dest i64) (param $source i64) (param $numBytes i64)
    block $exitCopyLoop
      loop $copyLoop
        (br_if $exitCopyLoop (i64.eq (local.get $numBytes) (i64.const 0)))
        (i64.store
          offset=0
          (local.get $dest)
          (i64.load offset=0 (local.get $source)))
        (i64.store
          offset=8
          (local.get $dest)
          (i64.load offset=8 (local.get $source)))
        (i64.store
          offset=16
          (local.get $dest)
          (i64.load offset=24 (local.get $source)))
        (i64.store
          offset=24
          (local.get $dest)
          (i64.load offset=24 (local.get $source)))
        (local.set $numBytes (i64.sub (local.get $numBytes) (i64.const 32)))
        (local.set $dest (i64.add (local.get $dest) (i64.const 32)))
        (local.set $source (i64.add (local.get $source) (i64.const 32)))
        (br $copyLoop)
      end
    end
  )

  (func (export "memory.copy")
    (param $dest i64) (param $source i64) (param $numBytes i64)
    (memory.copy (local.get $dest) (local.get $source) (local.get $numBytes))
  )
)

(benchmark "64-bit i64 copy loop         " (invoke "i64 copy loop"          (i64.const 0) (i64.const 8) (i64.const 2097152)))
(benchmark "64-bit i64 unrolled copy loop" (invoke "i64 unrolled copy loop" (i64.const 0) (i64.const 8) (i64.const 2097152)))
(benchmark "64-bit memory.copy           " (invoke "memory.copy"            (i64.const 0) (i64.const 8) (i64.const 2097152)))
