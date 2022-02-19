(module
  (memory 2)
  (func (export "i8x16.load_interleaved_2")
    (param $numIterations i32) (result v128 v128)
    (local $i i32)
    (local $acc0 v128)
    (local $acc1 v128)
    loop $loop
      (i32.and (i32.const 0xffff) (i32.mul (local.get $i) (i32.const 32)))
      v8x16.load_interleaved_2
      local.get $acc1 i8x16.add local.set $acc1
      local.get $acc0 i8x16.add local.set $acc0
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.ne (local.get $i) (local.get $numIterations)))
    end
    local.get $acc0
    local.get $acc1
  )
  (func (export "i8x16.load_interleaved_3")
    (param $numIterations i32) (result v128 v128 v128)
    (local $i i32)
    (local $acc0 v128)
    (local $acc1 v128)
    (local $acc2 v128)
    loop $loop
      (i32.and (i32.const 0xffff) (i32.mul (local.get $i) (i32.const 48)))
      v8x16.load_interleaved_3
      local.get $acc2 i8x16.add local.set $acc2
      local.get $acc1 i8x16.add local.set $acc1
      local.get $acc0 i8x16.add local.set $acc0
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.ne (local.get $i) (local.get $numIterations)))
    end
    local.get $acc0
    local.get $acc1
    local.get $acc2
  )
  (func (export "i8x16.load_interleaved_4")
    (param $numIterations i32) (result v128 v128 v128 v128)
    (local $i i32)
    (local $acc0 v128)
    (local $acc1 v128)
    (local $acc2 v128)
    (local $acc3 v128)
    loop $loop
      (i32.and (i32.const 0xffff) (i32.mul (local.get $i) (i32.const 64)))
      v8x16.load_interleaved_4
      local.get $acc3 i8x16.add local.set $acc3
      local.get $acc2 i8x16.add local.set $acc2
      local.get $acc1 i8x16.add local.set $acc1
      local.get $acc0 i8x16.add local.set $acc0
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.ne (local.get $i) (local.get $numIterations)))
    end
    local.get $acc0
    local.get $acc1
    local.get $acc2
    local.get $acc3
  )

  (func (export "v32x4.load_interleaved_3")
    (param $numIterations i32) (result v128 v128 v128)
    (local $i i32)
    (local $acc0 v128)
    (local $acc1 v128)
    (local $acc2 v128)
    loop $loop
      (i32.and (i32.const 0xffff) (i32.mul (local.get $i) (i32.const 48)))
      v32x4.load_interleaved_3
      local.get $acc2 f32x4.add local.set $acc2
      local.get $acc1 f32x4.add local.set $acc1
      local.get $acc0 f32x4.add local.set $acc0
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.ne (local.get $i) (local.get $numIterations)))
    end
    local.get $acc0
    local.get $acc1
    local.get $acc2
  )

  (func (export "emulated i8x16.load_interleaved_3")
    (param $numIterations i32) (result v128 v128 v128)
    (local $i i32)
    (local $acc0 v128)
    (local $acc1 v128)
    (local $acc2 v128)
    (local $address i32)
    (local $mem0 v128)
    (local $mem1 v128)
    (local $mem2 v128)
    loop $loop
      (local.set $address (i32.and (i32.const 0xffff) (i32.mul (local.get $i) (i32.const 48))))
      (local.set $mem0 (v128.load offset=0  (local.get $address)))
      (local.set $mem1 (v128.load offset=16 (local.get $address)))
      (local.set $mem2 (v128.load offset=32 (local.get $address)))
      (local.set $acc0 (i8x16.add (local.get $acc0)
        (i8x16.shuffle
          0 1 2 3 4 5 6 7 8 9 10 17 20 23 26 29
          (i8x16.shuffle 0 3 6 9 12 15 18 21 24 27 30 0 0 0 0 0 (local.get $mem0) (local.get $mem1))
          (local.get $mem2))))
      (local.set $acc1 (i8x16.add (local.get $acc1)
        (i8x16.shuffle
          0 1 2 3 4 5 6 7 8 9 10 18 21 24 27 30
          (i8x16.shuffle 1 4 7 10 13 16 19 22 25 28 31 0 0 0 0 0 (local.get $mem0) (local.get $mem1))
          (local.get $mem2))))
      (local.set $acc2 (i8x16.add (local.get $acc2)
        (i8x16.shuffle
          0 1 2 3 4 5 6 7 8 9 16 19 22 25 28 31
          (i8x16.shuffle 2 5 8 11 14 17 20 23 26 29 0 0 0 0 0 0 (local.get $mem0) (local.get $mem1))
          (local.get $mem2))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.ne (local.get $i) (local.get $numIterations)))
    end
    local.get $acc0
    local.get $acc1
    local.get $acc2
  )
)

(benchmark "i8x16.load_interleaved_2(60000)" (invoke "i8x16.load_interleaved_2" (i32.const 60000)))
(benchmark "i8x16.load_interleaved_2(120000)" (invoke "i8x16.load_interleaved_2" (i32.const 120000)))
(benchmark "i8x16.load_interleaved_2(240000)"  (invoke "i8x16.load_interleaved_2" (i32.const 240000)))

(benchmark "i8x16.load_interleaved_3(40000)" (invoke "i8x16.load_interleaved_3" (i32.const 40000)))
(benchmark "i8x16.load_interleaved_3(80000)" (invoke "i8x16.load_interleaved_3" (i32.const 80000)))
(benchmark "i8x16.load_interleaved_3(160000)"  (invoke "i8x16.load_interleaved_3" (i32.const 160000)))

(benchmark "i8x16.load_interleaved_4(30000)" (invoke "i8x16.load_interleaved_4" (i32.const 30000)))
(benchmark "i8x16.load_interleaved_4(60000)" (invoke "i8x16.load_interleaved_4" (i32.const 60000)))
(benchmark "i8x16.load_interleaved_4(120000)"  (invoke "i8x16.load_interleaved_4" (i32.const 120000)))

(benchmark "v32x4.load_interleaved_3(40000)" (invoke "v32x4.load_interleaved_3" (i32.const 40000)))
(benchmark "v32x4.load_interleaved_3(80000)" (invoke "v32x4.load_interleaved_3" (i32.const 80000)))
(benchmark "v32x4.load_interleaved_3(160000)"  (invoke "v32x4.load_interleaved_3" (i32.const 160000)))

(benchmark "emulated i8x16.load_interleaved_3(10000)" (invoke "emulated i8x16.load_interleaved_3" (i32.const 10000)))
(benchmark "emulated i8x16.load_interleaved_3(20000)" (invoke "emulated i8x16.load_interleaved_3" (i32.const 20000)))
(benchmark "emulated i8x16.load_interleaved_3(40000)" (invoke "emulated i8x16.load_interleaved_3" (i32.const 40000)))
