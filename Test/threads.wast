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
