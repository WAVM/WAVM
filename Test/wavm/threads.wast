(module

	(import "threadTest" "createThread" (func $threadTest.createThread  (param funcref i32) (result i64)))
	(import "threadTest" "exitThread" (func $threadTest.exitThread (param i64)))
	(import "threadTest" "detachThread" (func $threadTest.detachThread (param i64)))
	(import "threadTest" "joinThread" (func $threadTest.joinThread (param i64) (result i64)))

	(memory 1 1 shared)

	(elem declare $createThreadEntry $createThreadEntry2)

	(data "x")

	(global $atomicAccumulatorAddress i32 (i32.const 0))

	(func $initAccumulator
		(i64.atomic.store (global.get $atomicAccumulatorAddress) (i64.const 0))
		)

	(func $atomicAccumulate (param $addend i64)
		(drop (i64.atomic.rmw.add (global.get $atomicAccumulatorAddress) (local.get $addend)))
		)

	(func $getAccumulator (result i64) (i64.atomic.load (global.get $atomicAccumulatorAddress)))

	(func $createThreadEntry (param $argument i32) (result i64)
		(call $atomicAccumulate (i64.extend_i32_u (local.get $argument)))
		i64.const 100
		)

	(func (export "createThreadWithReturn") (result i64)
		(call $initAccumulator)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry) (i32.const 11)))
		(return (call $getAccumulator))
		)
		
	(func $createThreadEntry2 (param $argument i32) (result i64)
		(call $atomicAccumulate (i64.extend_i32_u (local.get $argument)))
		(call $threadTest.exitThread (i64.const 200))
		unreachable
		)

	(func (export "createThreadWithExit") (result i64)
		(call $initAccumulator)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry2) (i32.const 11)))
		(return (call $getAccumulator))
		)

	(func (export "memory.atomic.notify") (param $numWaiters i32) (param $address i32) (result i32)
		(memory.atomic.notify (local.get $numWaiters) (local.get $address))
		)

	(func (export "memory.atomic.wait32") (param $address i32) (param $expectedValue i32) (param $timeout i64) (result i32)
		(memory.atomic.wait32 (local.get $address) (local.get $expectedValue) (local.get $timeout))
		)

	(func (export "memory.atomic.wait64") (param $address i32) (param $expectedValue i64) (param $timeout i64) (result i32)
		(memory.atomic.wait64 (local.get $address) (local.get $expectedValue) (local.get $timeout))
		)
)

(assert_return (invoke "createThreadWithReturn") (i64.const 11))
(assert_return (invoke "createThreadWithExit") (i64.const 11))

;; Cross-thread wait/notify tests

(module
	(import "threadTest" "createThread" (func $threadTest.createThread (param funcref i32) (result i64)))
	(import "threadTest" "joinThread" (func $threadTest.joinThread (param i64) (result i64)))

	(memory 1 1 shared)

	(elem declare $wait32ThreadEntry $wait64ThreadEntry)

	;; Memory layout:
	;; addr 0: value being waited on (i32 for wait32, i64 for wait64)
	;; addr 16: ready flag (i32, set to 1 by thread before calling wait)

	;; Thread entry: sets ready flag, then waits on addr 0 expecting i32 value 0
	(func $wait32ThreadEntry (param $argument i32) (result i64)
		;; Set the ready flag to signal we're about to wait
		(i32.atomic.store (i32.const 16) (i32.const 1))
		;; Wait on address 0, expecting value 0, with infinite timeout
		(i64.extend_i32_u
			(memory.atomic.wait32 (i32.const 0) (i32.const 0) (i64.const -1)))
	)

	;; Thread entry: sets ready flag, then waits on addr 0 expecting i64 value 0
	(func $wait64ThreadEntry (param $argument i32) (result i64)
		;; Set the ready flag to signal we're about to wait
		(i32.atomic.store (i32.const 16) (i32.const 1))
		;; Wait on address 0, expecting value 0, with infinite timeout
		(i64.extend_i32_u
			(memory.atomic.wait64 (i32.const 0) (i64.const 0) (i64.const -1)))
	)

	(func (export "testWait32Notify") (result i64)
		(local $thread i64)
		;; Initialize: clear wait address and ready flag
		(i32.atomic.store (i32.const 0) (i32.const 0))
		(i32.atomic.store (i32.const 16) (i32.const 0))
		;; Create thread that will wait on addr 0
		(local.set $thread
			(call $threadTest.createThread (ref.func $wait32ThreadEntry) (i32.const 0)))
		;; Spin until the ready flag is set
		(block $break
			(loop $spin
				(br_if $break (i32.atomic.load (i32.const 16)))
				(br $spin)
			)
		)
		;; Spin-notify until a waiter is woken
		(block $done
			(loop $notify_loop
				(br_if $done
					(i32.gt_u
						(memory.atomic.notify (i32.const 0) (i32.const 1))
						(i32.const 0)))
				(br $notify_loop)
			)
		)
		;; Join thread and return its result (should be 0 = "woken")
		(call $threadTest.joinThread (local.get $thread))
	)

	(func (export "testWait64Notify") (result i64)
		(local $thread i64)
		;; Initialize: clear wait address and ready flag
		(i64.atomic.store (i32.const 0) (i64.const 0))
		(i32.atomic.store (i32.const 16) (i32.const 0))
		;; Create thread that will wait on addr 0
		(local.set $thread
			(call $threadTest.createThread (ref.func $wait64ThreadEntry) (i32.const 0)))
		;; Spin until the ready flag is set
		(block $break
			(loop $spin
				(br_if $break (i32.atomic.load (i32.const 16)))
				(br $spin)
			)
		)
		;; Spin-notify until a waiter is woken
		(block $done
			(loop $notify_loop
				(br_if $done
					(i32.gt_u
						(memory.atomic.notify (i32.const 0) (i32.const 1))
						(i32.const 0)))
				(br $notify_loop)
			)
		)
		;; Join thread and return its result (should be 0 = "woken")
		(call $threadTest.joinThread (local.get $thread))
	)
)

;; wait32 + notify: thread should return 0 (woken)
(assert_return (invoke "testWait32Notify") (i64.const 0))

;; wait64 + notify: thread should return 0 (woken)
(assert_return (invoke "testWait64Notify") (i64.const 0))
