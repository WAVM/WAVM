(module

	(import "threadTest" "createThread" (func $threadTest.createThread  (param funcref i32) (result i64)))
	(import "threadTest" "forkThread" (func $threadTest.forkThread (result i64)))
	(import "threadTest" "exitThread" (func $threadTest.exitThread (param i64)))
	(import "threadTest" "detachThread" (func $threadTest.detachThread (param i64)))
	(import "threadTest" "joinThread" (func $threadTest.joinThread (param i64) (result i64)))

	(memory 1 1 shared)

	(data passive "x")

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

	(func $createThreadEntry3 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(local.set $forkedThread (call $threadTest.forkThread))
		(i64.ne (local.get $forkedThread) (i64.const 0))
		if
			(call $atomicAccumulate (i64.const 17))
			(drop (call $threadTest.joinThread (local.get $forkedThread)))
		else
			(call $atomicAccumulate (i64.extend_i32_u (local.get $argument)))
		end
		i64.const 300
		)

	(func (export "forkThreadWithReturn") (result i64)
		(local $i i32)
		loop $iterLoop
			(call $initAccumulator)
			(drop (call $threadTest.joinThread
				(call $threadTest.createThread (ref.func $createThreadEntry3) (i32.const 13))))
			(local.tee $i (i32.add (local.get $i) (i32.const 1)))
			i32.const 100
			i32.lt_u
			br_if $iterLoop
		end
		(return (call $getAccumulator))
		)

	(func $createThreadEntry4 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(local.set $forkedThread (call $threadTest.forkThread))
		(i64.ne (local.get $forkedThread) (i64.const 0))
		if
			(call $atomicAccumulate (i64.const 19))
			(drop (call $threadTest.joinThread (local.get $forkedThread)))
		else
			(call $atomicAccumulate (i64.extend_i32_u (local.get $argument)))
		end
		(call $threadTest.exitThread (i64.const 400))
		unreachable
		)

	(func (export "forkThreadWithExit") (result i64)
		(local $i i32)
		loop $iterLoop
			(call $initAccumulator)
			(drop (call $threadTest.joinThread
				(call $threadTest.createThread (ref.func $createThreadEntry4) (i32.const 23))))
			(local.tee $i (i32.add (local.get $i) (i32.const 1)))
			i32.const 100
			i32.lt_u
			br_if $iterLoop
		end
		(return (call $getAccumulator))
		)
		
	(func $createThreadEntry5 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(local.set $forkedThread (call $threadTest.forkThread))
		(i64.ne (local.get $forkedThread) (i64.const 0))
		if
			(call $atomicAccumulate
				(call $threadTest.joinThread (local.get $forkedThread)))
		else
			(call $atomicAccumulate (i64.const 29))
		end
		(call $threadTest.exitThread (i64.const 500))
		unreachable
		)

	(func (export "forkThreadWithJoin") (result i64)
		(call $initAccumulator)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry5) (i32.const 31)))
		(return (call $getAccumulator))
		)

	(func $createThreadEntry6 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(local.set $forkedThread (call $threadTest.forkThread))
		(i64.ne (local.get $forkedThread) (i64.const 0))
		if (result i64)
			(i64.mul
				(i64.const 3)
				(call $threadTest.joinThread (local.get $forkedThread)))
		else
			(local.set $forkedThread (call $threadTest.forkThread))
			(i64.ne (local.get $forkedThread) (i64.const 0))
			if (result i64)
				(i64.add
					(i64.const 17)
					(call $threadTest.joinThread (local.get $forkedThread)))
			else
				(i64.extend_i32_u (local.get $argument))
			end
		end
		)

	(func (export "forkForkedThreadWithReturn") (result i64)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry6) (i32.const 100)))
		)

	(func $createThreadEntry7 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(local.set $forkedThread (call $threadTest.forkThread))
		(i64.ne (local.get $forkedThread) (i64.const 0))
		if
			(i64.mul
				(i64.const 5)
				(call $threadTest.joinThread (local.get $forkedThread)))
			call $threadTest.exitThread
		else
			(local.set $forkedThread (call $threadTest.forkThread))
			(i64.ne (local.get $forkedThread) (i64.const 0))
			if
				(i64.add
					(i64.const 19)
					(call $threadTest.joinThread (local.get $forkedThread)))
				call $threadTest.exitThread
			else
				(i64.extend_i32_u (local.get $argument))
				call $threadTest.exitThread
			end
		end
		unreachable
		)
		
	(func (export "forkForkedThreadWithExit") (result i64)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry7) (i32.const 200)))
		)
		
	(func $createThreadEntry8 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(set_local $forkedThread (call $threadTest.forkThread))
		(i64.ne (get_local $forkedThread) (i64.const 0))
		if
			(i64.mul
				(i64.const 5)
				(call $threadTest.joinThread (get_local $forkedThread)))
			call $threadTest.exitThread
		else
			(set_local $forkedThread (call $threadTest.forkThread))
			(i64.ne (get_local $forkedThread) (i64.const 0))
			if
				(i64.add
					(i64.const 19)
					(call $threadTest.joinThread (get_local $forkedThread)))
				call $threadTest.exitThread
			else
				(memory.init 0 (i32.const 0xffffffff) (i32.const 0) (i32.const 1))
				unreachable
			end
		end
		unreachable
		)
		
	(func (export "forkForkedThreadWithTrap") (result i64)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry8) (i32.const 200)))
		)

	(func (export "atomic.notify") (param $numWaiters i32) (param $address i32) (result i32)
		(atomic.notify (local.get $numWaiters) (local.get $address))
		)

	(func (export "i32.atomic.wait") (param $address i32) (param $expectedValue i32) (param $timeout i64) (result i32)
		(i32.atomic.wait (local.get $address) (local.get $expectedValue) (local.get $timeout))
		)

	(func (export "i64.atomic.wait") (param $address i32) (param $expectedValue i64) (param $timeout i64) (result i32)
		(i64.atomic.wait (local.get $address) (local.get $expectedValue) (local.get $timeout))
		)
)

(assert_return (invoke "createThreadWithReturn") (i64.const 11))
(assert_return (invoke "createThreadWithExit") (i64.const 11))
(assert_return (invoke "forkThreadWithReturn") (i64.const 30))
(assert_return (invoke "forkThreadWithExit") (i64.const 42))
(assert_return (invoke "forkThreadWithJoin") (i64.const 529))
(assert_return (invoke "forkForkedThreadWithReturn") (i64.const 351))
(assert_return (invoke "forkForkedThreadWithExit") (i64.const 1095))

;; do this twice to make sure no locks were orphaned by the first trap
(assert_trap (invoke "forkForkedThreadWithTrap") "out of bounds memory access")
(assert_trap (invoke "forkForkedThreadWithTrap") "out of bounds memory access")