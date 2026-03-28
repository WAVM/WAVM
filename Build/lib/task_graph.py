"""Task scheduling framework with parallel execution support."""

import os
import threading
import time
from concurrent.futures import ThreadPoolExecutor, Future, wait, FIRST_COMPLETED
from dataclasses import dataclass, field
from datetime import timedelta
from enum import Enum
from typing import Any, Optional

from .output import output


class TaskState(Enum):
    """Task execution state."""

    PENDING = "pending"  # Waiting for dependencies
    READY = "ready"  # Dependencies met, can be scheduled
    QUEUED = "queued"  # Submitted to executor, waiting for worker
    RUNNING = "running"  # Currently executing
    COMPLETED = "completed"  # Finished successfully
    FAILED = "failed"  # Finished with error
    SKIPPED = "skipped"  # Skipped due to dependency failure


@dataclass
class TaskResult:
    """Result of a task execution."""

    success: bool
    error_message: str = ""
    warning_message: str = ""
    # Named artifacts that dependent tasks can access
    artifacts: dict[str, Any] = field(default_factory=dict)


class Task:
    """A unit of work in the task graph."""

    def __init__(
        self,
        name: str,
        description: str,
        dependencies: Optional[list["Task"]] = None,
        priority: tuple[int, float] = (1, 0.0),  # (level, -duration); lower = higher priority
    ):
        self.name = name
        self.description = description
        self.dependencies = dependencies or []
        self.priority = priority
        self.state = TaskState.PENDING
        self.result: Optional[TaskResult] = None
        self.started_at: Optional[float] = None

    def run(self) -> TaskResult:
        # By default, do nothing and succeed.
        return TaskResult(True)


# =============================================================================
# Module-level helpers
# =============================================================================


def _format_duration(seconds: float) -> str:
    """Format a duration as a compact string."""
    return str(timedelta(seconds=round(seconds)))


def _update_states(tasks: list[Task]) -> None:
    """Update PENDING tasks to READY or SKIPPED based on dependencies.

    Loops until a fixed point so transitive skips propagate fully
    regardless of task ordering in the list.
    """
    changed = True
    while changed:
        changed = False
        for task in tasks:
            if task.state != TaskState.PENDING:
                continue

            deps_met = True
            deps_failed = False

            for dep in task.dependencies:
                if dep.state in (TaskState.FAILED, TaskState.SKIPPED):
                    deps_failed = True
                    break
                if dep.state != TaskState.COMPLETED:
                    deps_met = False

            if deps_failed:
                task.state = TaskState.SKIPPED
                changed = True
            elif deps_met:
                task.state = TaskState.READY
                changed = True


def _get_ready_tasks(tasks: list[Task]) -> list[Task]:
    """Get tasks that are ready to run, sorted by priority (lowest first)."""
    ready = [t for t in tasks if t.state == TaskState.READY]
    ready.sort(key=lambda t: t.priority)
    return ready


def _update_status(tasks: list[Task], start_time: float) -> None:
    """Update the status display with progress and running task info."""
    now = time.monotonic()
    elapsed = now - start_time
    total = len(tasks)
    completed = sum(
        1 for t in tasks if t.state in (TaskState.COMPLETED, TaskState.FAILED, TaskState.SKIPPED)
    )
    running = [t for t in tasks if t.state == TaskState.RUNNING]

    if completed == total and not running:
        return

    running_count = len(running)
    ready_count = sum(
        1 for t in tasks if t.state in (TaskState.QUEUED, TaskState.READY)
    )
    pending_count = sum(1 for t in tasks if t.state == TaskState.PENDING)

    counts = f"{completed}/{total}"
    count_parts = []
    if running_count:
        count_parts.append(f"{running_count} running")
    if ready_count:
        count_parts.append(f"{ready_count} ready")
    if pending_count:
        count_parts.append(f"{pending_count} pending")
    if count_parts:
        counts += f" ({', '.join(count_parts)})"

    lines = [f"[{_format_duration(elapsed)}] {counts}"]

    if running:
        # Longest running task
        longest = max(running, key=lambda t: now - (t.started_at or now))
        longest_dur = now - (longest.started_at or now)
        lines.append(f"  longest: {longest.description} ({_format_duration(longest_dur)})")

        # Most recently started task
        latest = max(running, key=lambda t: t.started_at or 0.0)
        if latest is not longest:
            lines.append(f"  latest:  {latest.description}")

    output.status(*lines)


# =============================================================================
# DAG-driven executor
# =============================================================================


def execute(demanded_tasks: list[Task], suppress_summary: bool = False) -> bool:
    """Execute tasks by walking the dependency DAG from demanded roots.

    Discovers all transitive dependencies, then runs tasks in parallel
    using a ThreadPoolExecutor. Tasks can dynamically add dependencies
    to other tasks during execution; the executor discovers newly added
    deps each iteration.

    Returns True if all tasks succeeded, False otherwise.
    If suppress_summary is True, suppresses the summary message on success.
    """
    start_time = time.monotonic()

    # Discovery state
    tasks: list[Task] = []
    task_names: set[str] = set()
    visited: set[int] = set()
    known_dep_counts: dict[int, int] = {}

    def discover(task: Task) -> None:
        if id(task) in visited:
            return
        visited.add(id(task))
        assert task.name not in task_names, (
            f"Duplicate task name: {task.name}"
        )
        task_names.add(task.name)
        tasks.append(task)
        known_dep_counts[id(task)] = len(task.dependencies)
        for dep in task.dependencies:
            discover(dep)

    for task in demanded_tasks:
        discover(task)

    lock = threading.Lock()
    max_workers = os.cpu_count() or 4

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures: dict[Future, Task] = {}

        while True:
            with lock:
                # Scan for dynamically added dependencies
                for task in list(tasks):
                    current_count = len(task.dependencies)
                    known_count = known_dep_counts[id(task)]
                    if current_count > known_count:
                        new_deps = task.dependencies[known_count:]
                        known_dep_counts[id(task)] = current_count
                        for dep in new_deps:
                            if dep.state == TaskState.COMPLETED:
                                raise RuntimeError(
                                    f"New dependency {dep.name} added to {task.name}"
                                    " but dependency already completed"
                                )
                            discover(dep)

                _update_states(tasks)
                ready = _get_ready_tasks(tasks)

                # Don't resubmit tasks already in flight
                in_flight = {id(futures[f]) for f in futures}
                ready = [t for t in ready if id(t) not in in_flight]

                def run_task(task: Task) -> TaskResult:
                    task.state = TaskState.RUNNING
                    task.started_at = time.monotonic()
                    output.verbose(f"Started: {task.description}")
                    return task.run()

                def submit_task(task: Task) -> None:
                    task.state = TaskState.QUEUED
                    future = executor.submit(run_task, task)
                    futures[future] = task

                # Submit highest priority tasks up to available slots
                available_slots = max_workers - len(futures)
                for task in ready[:available_slots]:
                    submit_task(task)

                _update_status(tasks, start_time)

            if not futures:
                # No tasks running; check if there are pending tasks
                pending = [t for t in tasks if t.state == TaskState.PENDING]
                if not pending:
                    break
                # Pending tasks but no futures means a deadlock
                break

            # Wait for at least one task to complete, with timeout for status updates
            done, _ = wait(futures.keys(), return_when=FIRST_COMPLETED, timeout=1.0)

            # If timeout expired with no completed tasks, just update status and continue
            if not done:
                with lock:
                    _update_status(tasks, start_time)
                continue

            with lock:
                for future in done:
                    task = futures.pop(future)

                    try:
                        result = future.result()
                        task.result = result

                        if result.success:
                            task.state = TaskState.COMPLETED
                            if result.warning_message:
                                output.print(f"{task.description}: {result.warning_message}")
                            output.verbose(f"Completed: {task.description}")
                        else:
                            task.state = TaskState.FAILED
                            output.error(f"{task.description}: {result.error_message}")
                    except Exception as e:
                        task.state = TaskState.FAILED
                        error_msg = f"{type(e).__name__}: {e}"
                        task.result = TaskResult(False, error_msg)
                        output.error(f"{task.description} raised {error_msg}")

    # Verify all tasks reached a terminal state
    terminal_states = {TaskState.COMPLETED, TaskState.FAILED, TaskState.SKIPPED}
    stuck_tasks = [t for t in tasks if t.state not in terminal_states]
    if stuck_tasks:
        output.clear_status()
        for task in stuck_tasks:
            dep_states = ", ".join(
                f"{d.name}={d.state.value}" for d in task.dependencies
            )
            output.error(
                f"Task stuck in {task.state.value}: {task.description}"
                + (f" (deps: {dep_states})" if dep_states else "")
            )
        assert False, (
            f"{len(stuck_tasks)} task(s) never reached a terminal state"
        )

    # Summary
    elapsed = time.monotonic() - start_time
    failed = [t for t in tasks if t.state == TaskState.FAILED]
    skipped = [t for t in tasks if t.state == TaskState.SKIPPED]
    completed = [t for t in tasks if t.state == TaskState.COMPLETED]
    elapsed_str = _format_duration(elapsed)

    assert len(failed) + len(skipped) + len(completed) == len(tasks)

    output.clear_status()

    warned = [
        t
        for t in tasks
        if t.state == TaskState.COMPLETED and t.result and t.result.warning_message
    ]

    if failed or not suppress_summary:
        if failed:
            parts = [f"Failed: {len(failed)} task(s)"]
            if completed:
                parts.append(f"{len(completed)} completed")
        else:
            parts = [f"Completed {len(completed)} task(s)"]
        if warned:
            parts.append(f"with {len(warned)} warning(s)")
        if skipped:
            parts.append(f"{len(skipped)} skipped")
        output.print(f"{', '.join(parts)} in {elapsed_str}\n")

    return len(failed) == 0
