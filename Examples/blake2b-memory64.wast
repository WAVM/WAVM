;; Blake2b hash function

(module
	(import "wasi_unstable" "fd_write" (func $wasi_fd_write (param i32 i32 i32 i32) (result i32)))
	(import "wasi_unstable" "proc_exit" (func $wasi_proc_exit (param i32)))
	(memory (export "memory") i64 4096 4096)

	(global $numIterations i32 (i32.const 10))

	;; IV array
	(global $ivAddress i64 (i64.const 0))	;; 64 byte IV array
	(data (i64.const 0)
		"\08\c9\bc\f3\67\e6\09\6a"
		"\3b\a7\ca\84\85\ae\67\bb"
		"\2b\f8\94\fe\72\f3\6e\3c"
		"\f1\36\1d\5f\3a\f5\4f\a5"
		"\d1\82\e6\ad\7f\52\0e\51"
		"\1f\6c\3e\2b\8c\68\05\9b"
		"\6b\bd\41\fb\ab\d9\83\1f"
		"\79\21\7e\13\19\cd\e0\5b"
	)

	(global $pAddress i64 (i64.const 64))	;; 64 byte Param struct
	(data (i64.const 64)
		"\40" ;; digest_length
		"\00" ;; key_length
		"\01" ;; fanout
		"\01" ;; depth
		"\00\00\00\00" ;; leaf_length
		"\00\00\00\00" ;; node_offset
		"\00\00\00\00" ;; xof_length
		"\00" ;; node_depth
		"\00" ;; inner_length
		"\00\00\00\00\00\00\00\00\00\00\00\00\00\00" ;; reserved
		"\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" ;; salt
		"\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00" ;; personal
		)

	;; lookup table for converting a nibble to a hexit
	(global $hexitTable i64 (i64.const 128))
	(data (i64.const 128) "0123456789abcdef")
	
	(global $dataAddressAddress i64 (i64.const 144)) ;; 8 bytes
	(global $dataNumBytes i64 (i64.const 134217728))
	
	(global $outputStringAddress i64 (i64.const 152)) ;; 129 bytes
	
	(global $outputIOVecAddress i64 (i64.const 284)) ;; 8 bytes
	(data (i64.const 284) "\98\00\00\00" ;; buf = 152
						  "\81\00\00\00" ;; buf_len = 129
	)

	(global $dynamicTopAddressAddress i64 (i64.const 292)) ;; 8 bytes
	(data (i64.const 292) "\00\08\00\00")                  ;; 2048

	(global $stateAddress i64 (i64.const 1024))	;; 128 bytes
	(global $stateNumBytes i64 (i64.const 128))

	(func $sbrk (param $numBytes i64) (result i64)
		(local $resultAddress i64)
		(local.set $resultAddress (i64.load (global.get $dynamicTopAddressAddress)))
		(i64.store
			(global.get $dynamicTopAddressAddress)
			(i64.add (local.get $resultAddress) (local.get $numBytes))
			)
		(local.get $resultAddress)
		)

	(func $compress
		(param $blockAddress i64)

		(local $row1l v128)
		(local $row1h v128)
		(local $row2l v128)
		(local $row2h v128)
		(local $row3l v128)
		(local $row3h v128)
		(local $row4l v128)
		(local $row4h v128)
		(local $b0 v128)
		(local $b1 v128)
		(local $t0 v128)
		(local $t1 v128)

		;; Initialize the state

		(local.set $row1l (v128.load offset=0 (global.get $stateAddress)))
		(local.set $row1h (v128.load offset=16 (global.get $stateAddress)))
		(local.set $row2l (v128.load offset=32 (global.get $stateAddress)))
		(local.set $row2h (v128.load offset=48 (global.get $stateAddress)))
		(local.set $row3l (v128.load offset=0 (global.get $ivAddress)))
		(local.set $row3h (v128.load offset=16 (global.get $ivAddress)))
		(local.set $row4l (v128.xor (v128.load offset=32 (global.get $ivAddress)) (v128.load offset=64 (global.get $stateAddress))))
		(local.set $row4h (v128.xor (v128.load offset=48 (global.get $ivAddress)) (v128.load offset=80 (global.get $stateAddress))))
		
		;; Round 0

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		;; Round 1

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=96 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 0 1 2 3 4 5 6 7 (v128.load offset=0 (local.get $blockAddress)) (v128.const i64x2 0 0)))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))
		
		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		;; Round 2

		(local.set $b0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))
		(local.set $b1 (v128.bitselect (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=96 (local.get $blockAddress)) (v128.const i64x2 0xffffffffffffffff 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v128.bitselect (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress)) (v128.const i64x2 0xffffffffffffffff 0)))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 3

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v128.bitselect (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		(local.set $b1 (v128.bitselect (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 4

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v128.bitselect (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		(local.set $b1 (v128.bitselect (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v128.bitselect (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		(local.set $b1 (v128.bitselect (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))
		(local.set $b1 (v128.bitselect (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=96 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
		
		;; Round 5

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v128.bitselect (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))
		(local.set $b1 (v128.bitselect (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 6

		(local.set $b0 (v128.bitselect (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=96 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 0 1 2 3 4 5 6 7 (v128.load offset=64 (local.get $blockAddress)) (v128.const i64x2 0 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v128.bitselect (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 7

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))
		(local.set $b1 (v128.bitselect (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 8

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v128.load offset=96 (local.get $blockAddress)))
		(local.set $b1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v128.bitselect (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))
		(local.set $b1 (v128.load offset=32 (local.get $blockAddress)))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 9

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))
		(local.set $b1 (v128.bitselect (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))  (v128.const i64x2 0xffffffffffffffff 0)))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=16 (local.get $blockAddress)) (v128.load offset=96 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=0 (local.get $blockAddress))))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 10

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=0 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=32 (local.get $blockAddress)) (v128.load offset=48 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=80 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))
		
		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Round 11

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=112 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=64 (local.get $blockAddress)) (v128.load offset=96 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=64 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=112 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))

		(local.set $b0 (v8x16.shuffle 8 9 10 11 12 13 14 15 0 1 2 3 4 5 6 7 (v128.load offset=0 (local.get $blockAddress)) (v128.const i64x2 0 0)))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=80 (local.get $blockAddress)) (v128.load offset=32 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2l) (v128.const i64x2 0 0)))
		(local.set $row2h (v8x16.shuffle 3 4 5 6 7 0 1 2 11 12 13 14 15 8 9 10 (local.get $row2h) (v128.const i64x2 0 0)))

		(local.set $b0 (v8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23 (v128.load offset=96 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))
		(local.set $b1 (v8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31 (v128.load offset=48 (local.get $blockAddress)) (v128.load offset=16 (local.get $blockAddress))))

		(local.set $row1l (i64x2.add (i64x2.add (local.get $row1l) (local.get $b0)) (local.get $row2l)))
		(local.set $row1h (i64x2.add (i64x2.add (local.get $row1h) (local.get $b1)) (local.get $row2h)))
		(local.set $row4l (v128.xor (local.get $row4l) (local.get $row1l)))
		(local.set $row4h (v128.xor (local.get $row4h) (local.get $row1h)))
		(local.set $row4l (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4l) (v128.const i64x2 0 0)))
		(local.set $row4h (v8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9 (local.get $row4h) (v128.const i64x2 0 0)))
		(local.set $row3l (i64x2.add (local.get $row3l) (local.get $row4l)))
		(local.set $row3h (i64x2.add (local.get $row3h) (local.get $row4h)))
		(local.set $row2l (v128.xor (local.get $row2l) (local.get $row3l)))
		(local.set $row2h (v128.xor (local.get $row2h) (local.get $row3h)))
		(local.set $row2l (v128.xor (i64x2.shr_u (local.get $row2l) (i32.const 63)) (i64x2.add (local.get $row2l) (local.get $row2l))))
		(local.set $row2h (v128.xor (i64x2.shr_u (local.get $row2h) (i32.const 63)) (i64x2.add (local.get $row2h) (local.get $row2h))))

		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2l) (local.get $row2h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row2h) (local.get $row2l)))
		(local.set $row2l (local.get $t0))
		(local.set $row2h (local.get $t1))
		(local.set $t0 (local.get $row3l))
		(local.set $row3l (local.get $row3h))
		(local.set $row3h (local.get $t0))
		(local.set $t0 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4l) (local.get $row4h)))
		(local.set $t1 (v8x16.shuffle 24 25 26 27 28 29 30 31 0 1 2 3 4 5 6 7 (local.get $row4h) (local.get $row4l)))
		(local.set $row4l (local.get $t1))
		(local.set $row4h (local.get $t0))
			
		;; Update the hash

		(local.set $row1l (v128.xor (local.get $row3l) (local.get $row1l)))
		(local.set $row1h (v128.xor (local.get $row3h) (local.get $row1h)))
		(v128.store offset=0 (global.get $stateAddress) (v128.xor (v128.load offset=0 (global.get $stateAddress)) (local.get $row1l)))
		(v128.store offset=16 (global.get $stateAddress) (v128.xor (v128.load offset=16 (global.get $stateAddress)) (local.get $row1h)))
		(local.set $row2l (v128.xor (local.get $row4l) (local.get $row2l)))
		(local.set $row2h (v128.xor (local.get $row4h) (local.get $row2h)))
		(v128.store offset=32 (global.get $stateAddress) (v128.xor (v128.load offset=32 (global.get $stateAddress)) (local.get $row2l)))
		(v128.store offset=48 (global.get $stateAddress) (v128.xor (v128.load offset=48 (global.get $stateAddress)) (local.get $row2h)))
	)

	(func $memsetAligned
		(param $address i64)
		(param $value i32)
		(param $numBytes i64)

		(local $value128 v128)
		(local $endAddress i64)
		(local $endAddress128 i64)

		(local.set $value128 (i8x16.splat (local.get $value)))
		(local.set $endAddress (i64.add (local.get $address) (local.get $numBytes)))
		(local.set $endAddress128 (i64.sub (local.get $endAddress) (i64.const 16)))

		block $loop128End
			loop $loop128
				(br_if $loop128End (i64.gt_u (local.get $address) (local.get $endAddress128)))
				(v128.store align=16 (local.get $address) (local.get $value128))
				(local.set $address (i64.add (local.get $address) (i64.const 16)))
				br $loop128
			end
		end

		block $loopEnd
			loop $loop
				(br_if $loopEnd (i64.ge_u (local.get $address) (local.get $endAddress)))
				(i32.store8 (local.get $address) (local.get $value))
				(local.set $address (i64.add (local.get $address) (i64.const 1)))
				br $loop
			end
		end
	)

	(func $blake2b
		(param $inputAddress i64)
		(param $numBytes i64)

		(local $stateAddress i64)
		(local $temp i64)
		
		;; zero the state
		(call $memsetAligned (global.get $stateAddress) (i32.const 0) (global.get $stateNumBytes))

		;; initialize the hash to the first 64 bytes of the param struct XORed with the contents of IV
		(v128.store offset=0 (global.get $stateAddress) (v128.xor
			(v128.load offset=0 (global.get $pAddress))
			(v128.load offset=0 (global.get $ivAddress))))
		(v128.store offset=16 (global.get $stateAddress) (v128.xor
			(v128.load offset=16 (global.get $pAddress))
			(v128.load offset=16 (global.get $ivAddress))))
		(v128.store offset=32 (global.get $stateAddress) (v128.xor
			(v128.load offset=32 (global.get $pAddress))
			(v128.load offset=32 (global.get $ivAddress))))
		(v128.store offset=48 (global.get $stateAddress) (v128.xor
			(v128.load offset=48 (global.get $pAddress))
			(v128.load offset=48 (global.get $ivAddress))))

		block $loopEnd
			loop $loop
				(br_if $loopEnd (i64.lt_u (local.get $numBytes) (i64.const 128)))

				(local.set $temp (i64.add (i64.load offset=64 (global.get $stateAddress)) (i64.const 128)))
				(i64.store offset=64 (global.get $stateAddress) (local.get $temp))
				(i64.store offset=72 (global.get $stateAddress) (i64.add
					(i64.load offset=72 (global.get $stateAddress))
					(i64.extend_i32_u (i64.lt_u (local.get $temp) (i64.const 128)))))

				(if (i64.eq (local.get $numBytes) (i64.const 128))
					(i64.store offset=80 (global.get $stateAddress) (i64.const 0xffffffffffffffff)))

				(call $compress (global.get $stateAddress) (local.get $inputAddress))

				(local.set $inputAddress (i64.add (local.get $inputAddress) (i64.const 128)))
				(local.set $numBytes (i64.sub (local.get $numBytes) (i64.const 128)))
				br $loop
			end
		end
	)

	(func $main (export "_start")
		(local $i i32)
		(local $byte i32)
		
		;; Initialize the test data.
		(i64.store (global.get $dataAddressAddress) (call $sbrk (global.get $dataNumBytes)))
		(local.set $i (i32.const 0))
		loop $initDataLoop
			(i32.store (i64.add (i64.load (global.get $dataAddressAddress)) (i64.extend_i32_u (local.get $i))) (local.get $i))
			(local.set $i (i32.add (local.get $i) (i32.const 4)))
			(br_if $initDataLoop (i64.lt_u (i64.extend_i32_u (local.get $i)) (global.get $dataNumBytes)))
		end
		
		;; Hash the test data enough times to dilute all the non-hash components of the timing.
		(local.set $i (i32.const 0))
		loop $iterLoop
			(call $blake2b (i64.load (global.get $dataAddressAddress)) (global.get $dataNumBytes))
			(local.set $i (i32.add (local.get $i) (i32.const 1)))
			(br_if $iterLoop (i32.lt_u (local.get $i) (global.get $numIterations)))
		end

		;; Create a hexadecimal string from the hash.
		(local.set $i (i32.const 0))
		loop $loop
			(local.set $byte (i32.load8_u (i64.add (global.get $stateAddress) (i64.extend_i32_u (local.get $i)))))
			(i32.store8 offset=0
				(i64.add (global.get $outputStringAddress) (i64.shl (i64.extend_i32_u (local.get $i)) (i64.const 1)))
				(i32.load8_u (i64.add (global.get $hexitTable) (i64.and (i64.extend_i32_u (local.get $byte)) (i64.const 0x0f)))))
			(i32.store8 offset=1
				(i64.add (global.get $outputStringAddress) (i64.shl (i64.extend_i32_u (local.get $i)) (i64.const 1)))
				(i32.load8_u (i64.add (global.get $hexitTable) (i64.shr_u (i64.extend_i32_u (local.get $byte)) (i64.const 4)))))
			(local.set $i (i32.add (local.get $i) (i32.const 1)))
			(br_if $loop (i32.lt_u (local.get $i) (i32.const 64)))
		end
		(i32.store8 offset=128 (global.get $outputStringAddress) (i32.const 10))

		;; Print the string to the output.
		(call $wasi_fd_write
			(i32.const 1)                                                           ;; fd 1
			(i32.wrap_i64 (global.get $outputIOVecAddress))                         ;; iovec address
			(i32.const 1)                                                           ;; 1 iovec
			(i32.wrap_i64 (i64.add (global.get $outputIOVecAddress) (i64.const 4))) ;; write the number of written bytes back to the iovec buf_len.
		)

		;; Pass the result of wasi_fd_write to wasi_proc_exit
		call $wasi_proc_exit
	)

	(func (export "establishStackSpace") (param i32 i32) (nop))
)
