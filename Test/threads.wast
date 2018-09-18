(module

	(import "threadTest" "createThread" (func $threadTest.createThread  (param anyfunc i32) (result i64)))
	(import "threadTest" "forkThread" (func $threadTest.forkThread (result i64)))
	(import "threadTest" "exitThread" (func $threadTest.exitThread (param i64)))
	(import "threadTest" "detachThread" (func $threadTest.detachThread (param i64)))
	(import "threadTest" "joinThread" (func $threadTest.joinThread (param i64) (result i64)))

	(memory 1 1 shared)

	(global $atomicAccumulatorAddress i32 (i32.const 0))

	(func $initAccumulator
		(i64.atomic.store (get_global $atomicAccumulatorAddress) (i64.const 0))
		)

	(func $atomicAccumulate (param $addend i64)
		(drop (i64.atomic.rmw.add (get_global $atomicAccumulatorAddress) (get_local $addend)))
		)

	(func $getAccumulator (result i64) (i64.atomic.load (get_global $atomicAccumulatorAddress)))

	(func $createThreadEntry (param $argument i32) (result i64)
		(call $atomicAccumulate (i64.extend_u/i32 (get_local $argument)))
		i64.const 100
		)

	(func (export "createThreadWithReturn") (result i64)
		(call $initAccumulator)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry) (i32.const 11)))
		(return (call $getAccumulator))
		)
		
	(func $createThreadEntry2 (param $argument i32) (result i64)
		(call $atomicAccumulate (i64.extend_u/i32 (get_local $argument)))
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
		(set_local $forkedThread (call $threadTest.forkThread))
		(i64.ne (get_local $forkedThread) (i64.const 0))
		if
			(call $atomicAccumulate (i64.const 17))
			(drop (call $threadTest.joinThread (get_local $forkedThread)))
		else
			(call $atomicAccumulate (i64.extend_u/i32 (get_local $argument)))
		end
		i64.const 300
		)

	(func (export "forkThreadWithReturn") (result i64)
		(local $i i32)
		loop $iterLoop
			(call $initAccumulator)
			(drop (call $threadTest.joinThread
				(call $threadTest.createThread (ref.func $createThreadEntry3) (i32.const 13))))
			(tee_local $i (i32.add (get_local $i) (i32.const 1)))
			i32.const 1000
			i32.lt_u
			br_if $iterLoop
		end
		(return (call $getAccumulator))
		)

	(func $createThreadEntry4 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(set_local $forkedThread (call $threadTest.forkThread))
		(i64.ne (get_local $forkedThread) (i64.const 0))
		if
			(call $atomicAccumulate (i64.const 19))
			(drop (call $threadTest.joinThread (get_local $forkedThread)))
		else
			(call $atomicAccumulate (i64.extend_u/i32 (get_local $argument)))
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
			(tee_local $i (i32.add (get_local $i) (i32.const 1)))
			i32.const 1000
			i32.lt_u
			br_if $iterLoop
		end
		(return (call $getAccumulator))
		)
		
	(func $createThreadEntry5 (param $argument i32) (result i64)
		(local $forkedThread i64)
		(set_local $forkedThread (call $threadTest.forkThread))
		(i64.ne (get_local $forkedThread) (i64.const 0))
		if
			(call $atomicAccumulate
				(call $threadTest.joinThread (get_local $forkedThread)))
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
		(set_local $forkedThread (call $threadTest.forkThread))
		(i64.ne (get_local $forkedThread) (i64.const 0))
		if (result i64)
			(i64.mul
				(i64.const 3)
				(call $threadTest.joinThread (get_local $forkedThread)))
		else
			(set_local $forkedThread (call $threadTest.forkThread))
			(i64.ne (get_local $forkedThread) (i64.const 0))
			if (result i64)
				(i64.add
					(i64.const 17)
					(call $threadTest.joinThread (get_local $forkedThread)))
			else
				(i64.extend_u/i32 (get_local $argument))
			end
		end
		)

	(func (export "forkForkedThreadWithReturn") (result i64)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry6) (i32.const 100)))
		)

	(func $createThreadEntry7 (param $argument i32) (result i64)
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
				(i64.extend_u/i32 (get_local $argument))
				call $threadTest.exitThread
			end
		end
		unreachable
		)

	(func (export "forkForkedThreadWithExit") (result i64)
		(call $threadTest.joinThread
			(call $threadTest.createThread (ref.func $createThreadEntry7) (i32.const 200)))
		)

	(func (export "atomic.wake") (param $numWaiters i32) (param $address i32) (result i32)
		(atomic.wake (get_local $numWaiters) (get_local $address))
		)

	(func (export "i32.atomic.wait") (param $address i32) (param $expectedValue i32) (param $timeout f64) (result i32)
		(i32.atomic.wait (get_local $address) (get_local $expectedValue) (get_local $timeout))
		)

	(func (export "i64.atomic.wait") (param $address i32) (param $expectedValue i64) (param $timeout f64) (result i32)
		(i64.atomic.wait (get_local $address) (get_local $expectedValue) (get_local $timeout))
		)
)

(assert_return (invoke "createThreadWithReturn") (i64.const 11))
(assert_return (invoke "createThreadWithExit") (i64.const 11))
(assert_return (invoke "forkThreadWithReturn") (i64.const 30))
(assert_return (invoke "forkThreadWithExit") (i64.const 42))
(assert_return (invoke "forkThreadWithJoin") (i64.const 529))
(assert_return (invoke "forkForkedThreadWithReturn") (i64.const 351))
(assert_return (invoke "forkForkedThreadWithExit") (i64.const 1095))
