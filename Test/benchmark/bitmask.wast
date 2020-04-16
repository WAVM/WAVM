(module
  (memory 1)
  (func (export "i8x16.bitmask")
    (param $numIterations i32)
    (result i32)
    (local $i i32)
    (local $result i32)
    loop $loop
      (local.set $result (i32.xor
        (local.get $result)
        (i8x16.bitmask (v128.load (i32.const 0)))
      ))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.lt_u (local.get $i) (local.get $numIterations)))
    end
    (local.get $result)
  )
  
  (func (export "i16x8.bitmask")
    (param $numIterations i32)
    (result i32)
    (local $i i32)
    (local $result i32)
    loop $loop
      (local.set $result (i32.xor
        (local.get $result)
        (i16x8.bitmask (v128.load (i32.const 0)))
      ))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.lt_u (local.get $i) (local.get $numIterations)))
    end
    (local.get $result)
  )
  
  (func (export "i32x4.bitmask")
    (param $numIterations i32)
    (result i32)
    (local $i i32)
    (local $result i32)
    loop $loop
      (local.set $result (i32.xor
        (local.get $result)
        (i32x4.bitmask (v128.load (i32.const 0)))
      ))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.lt_u (local.get $i) (local.get $numIterations)))
    end
    (local.get $result)
  )

  (func (export "emulated i8x16.bitmask")
    (param $numIterations i32)
    (result i32)
    (local $i i32)
    (local $x v128)
    (local $m8 v128)
    (local $m4 v128)
    (local $m2 v128)
    (local $m1 v128)
    (local $result i32)
    loop $loop
      (local.set $x (v128.load (i32.const 0)))
      (local.set $m8 (v128.and
        (i8x16.lt_s (local.get $x) (v128.const i64x2 0 0))
        (i64x2.splat (i64.const 0x8040201008040201))
      ))
      (local.set $m4 (v128.or (local.get $m8) (i64x2.shr_u (local.get $m8) (i32.const 32))))
      (local.set $m2 (v128.or (local.get $m4) (i64x2.shr_u (local.get $m4) (i32.const 16))))
      (local.set $m1 (v128.or (local.get $m2) (i64x2.shr_u (local.get $m2) (i32.const 8 ))))
      (local.set $result (i32.xor
        (local.get $result)
        (i32.or
          (i32.and (i32x4.extract_lane 0 (local.get $m1)) (i32.const 0xff))
          (i32.shl (i32.and (i32x4.extract_lane 2 (local.get $m1)) (i32.const 0xff)) (i32.const 8))
        )
      ))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $loop (i32.lt_u (local.get $i) (local.get $numIterations)))
    end
    (local.get $result)
  )
)

(benchmark "i8x16.bitmask" (invoke "i8x16.bitmask" (i32.const 1000000)))
(benchmark "emulated i8x16.bitmask" (invoke "emulated i8x16.bitmask" (i32.const 1000000)))

(benchmark "i16x8.bitmask" (invoke "i16x8.bitmask" (i32.const 1000000)))
(benchmark "i32x4.bitmask" (invoke "i32x4.bitmask" (i32.const 1000000)))
