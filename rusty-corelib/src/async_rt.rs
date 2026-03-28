use alloc::boxed::Box;
use alloc::vec::Vec;
use core::future::Future;
use core::hint::spin_loop;
use core::pin::Pin;
use core::task::{Context, Poll, RawWaker, RawWakerVTable, Waker};

/// Re-export of `core::future::poll_fn` for writing custom turn-polled futures.
///
/// This is useful when you want to build a one-off async adapter around
/// synchronous ecalls without defining a dedicated `Future` type.
pub use core::future::poll_fn;

use crate::{Direction, Result, turn};

type BoxedTask = Pin<Box<dyn Future<Output = Result<()>> + 'static>>;

/// Opaque handle returned by [`Scheduler::spawn`].
///
/// The handle can be used with [`Scheduler::contains`] to check whether a task
/// is still alive in the scheduler.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TaskHandle {
	id: u32,
}

/// Outcome of one [`Scheduler::tick`] call.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TaskResult {
	/// The game turn did not advance, so no task was polled.
	NoTurnAdvance,
	/// At least one task remains pending after this tick.
	PendingTasks(usize),
	/// All tasks have completed successfully.
	CompletedAll,
}

/// Future that resolves when `turn() >= target_turn`.
///
/// This is the primitive building block for turn-based waiting in Core RTT.
pub struct WaitUntil {
	target_turn: i32,
}

impl Future for WaitUntil {
	type Output = ();

	fn poll(self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<Self::Output> {
		if turn() >= self.target_turn {
			Poll::Ready(())
		} else {
			Poll::Pending
		}
	}
}

/// Wait until the runtime reaches `target_turn`.
///
/// If the current turn is already greater than or equal to `target_turn`, this
/// future resolves immediately.
pub fn wait_until(target_turn: i32) -> WaitUntil {
	WaitUntil { target_turn }
}

/// Wait for `turns` turns from the current turn.
///
/// `turns = 0` resolves immediately.
/// Negative values are not rejected but usually resolve immediately because the
/// computed target is in the past.
pub fn wait_for(turns: i32) -> WaitUntil {
	let current_turn = turn();
	WaitUntil {
		target_turn: current_turn + turns,
	}
}

/// Wait until the next turn.
///
/// Equivalent to `wait_for(1)`.
pub fn wait_next_turn() -> WaitUntil {
	wait_for(1)
}

/// Future that asynchronously receives one message into the provided buffer.
///
/// Poll behavior:
/// - `Ok(0)` from runtime means no message yet and maps to `Poll::Pending`.
/// - `Ok(len > 0)` maps to `Poll::Ready(Ok(len))`.
/// - `Err(code)` maps to `Poll::Ready(Err(code))`.
pub struct RecvMsgFuture<'a> {
	out: &'a mut [u8],
}

impl Future for RecvMsgFuture<'_> {
	type Output = Result<usize>;

	fn poll(
		mut self: Pin<&mut Self>,
		_cx: &mut Context<'_>,
	) -> Poll<Self::Output> {
		match crate::recv_msg(self.out) {
			Ok(0) => Poll::Pending,
			Ok(len) => Poll::Ready(Ok(len)),
			Err(err) => Poll::Ready(Err(err)),
		}
	}
}

/// Asynchronously receive one message.
///
/// This is the async counterpart of [`crate::recv_msg`]. The returned future
/// borrows `out` until completion.
pub fn recv_msg(out: &mut [u8]) -> RecvMsgFuture<'_> {
	RecvMsgFuture { out }
}

/// Asynchronously move one tile and then wait for the next turn.
///
/// This helper composes a successful [`crate::move_unit`] call with
/// [`wait_next_turn`], so callers can naturally chain movement in async loops.
pub async fn move_unit(direction: Direction) -> Result<()> {
	crate::move_unit(direction)?;
	wait_next_turn().await;
	Ok(())
}

struct TaskSlot {
	handle: TaskHandle,
	task: BoxedTask,
}

/// Turn-driven cooperative task scheduler.
///
/// Design notes:
/// - Tasks are polled at most once per turn.
/// - Polling order is a simple round-robin over internal storage.
/// - Task output type is fixed to [`Result<()>`] for direct ecall-style error
///   propagation.
pub struct Scheduler {
	tasks: Vec<TaskSlot>,
	next_id: u32,
	last_turn: i32,
}

impl Default for Scheduler {
	fn default() -> Self {
		Self::new()
	}
}

impl Scheduler {
	/// Create an empty scheduler.
	pub fn new() -> Self {
		Self {
			tasks: Vec::new(),
			next_id: 1,
			last_turn: turn().wrapping_sub(1),
		}
	}

	/// Create an empty scheduler with preallocated capacity for tasks.
	pub fn with_capacity(capacity: usize) -> Self {
		Self {
			tasks: Vec::with_capacity(capacity),
			next_id: 1,
			last_turn: turn().wrapping_sub(1),
		}
	}

	/// Return the number of active tasks.
	pub fn len(&self) -> usize {
		self.tasks.len()
	}

	/// Return `true` when there are no active tasks.
	pub fn is_empty(&self) -> bool {
		self.tasks.is_empty()
	}

	/// Spawn a new task and return its handle.
	///
	/// The task must be `'static` because it is heap-allocated and owned by the
	/// scheduler until completion.
	pub fn spawn<F>(&mut self, future: F) -> TaskHandle
	where
		F: Future<Output = Result<()>> + 'static,
	{
		let handle = TaskHandle { id: self.next_id };
		self.next_id = self.next_id.wrapping_add(1);
		self.tasks.push(TaskSlot {
			handle,
			task: Box::pin(future),
		});
		handle
	}

	/// Return whether a task identified by `handle` is still present.
	pub fn contains(&self, handle: TaskHandle) -> bool {
		self.tasks.iter().any(|slot| slot.handle == handle)
	}

	/// Poll all tasks once if turn advanced.
	///
	/// Returns:
	/// - [`TaskResult::NoTurnAdvance`] if called multiple times within one turn.
	/// - [`TaskResult::PendingTasks`] with remaining task count.
	/// - [`TaskResult::CompletedAll`] when no tasks remain.
	///
	/// If any task resolves to `Err`, that task is removed and the error is
	/// returned immediately.
	pub fn tick(&mut self) -> Result<TaskResult> {
		let current_turn = turn();
		if current_turn == self.last_turn {
			return Ok(TaskResult::NoTurnAdvance);
		}
		self.last_turn = current_turn;

		let waker = noop_waker();
		let mut cx = Context::from_waker(&waker);
		let mut idx = 0;
		while idx < self.tasks.len() {
			let poll_result = self.tasks[idx].task.as_mut().poll(&mut cx);
			match poll_result {
				Poll::Ready(Ok(())) => {
					self.tasks.swap_remove(idx);
				}
				Poll::Ready(Err(err)) => {
					self.tasks.swap_remove(idx);
					return Err(err);
				}
				Poll::Pending => {
					idx += 1;
				}
			}
		}

		if self.tasks.is_empty() {
			Ok(TaskResult::CompletedAll)
		} else {
			Ok(TaskResult::PendingTasks(self.tasks.len()))
		}
	}

	/// Run scheduler until all tasks complete or one task returns an error.
	///
	/// This function spins between turns and repeatedly calls [`Self::tick`].
	pub fn run_until_complete(&mut self) -> Result<()> {
		while !self.tasks.is_empty() {
			match self.tick()? {
				TaskResult::NoTurnAdvance => {
					spin_loop();
				}
				TaskResult::PendingTasks(_) | TaskResult::CompletedAll => {}
			}
		}
		Ok(())
	}
}

/// Drive one future with a turn-based polling loop.
///
/// Unlike general-purpose runtimes, this helper polls at most once per turn,
/// matching Core RTT's turn progression model.
pub fn block_on<F>(future: F) -> F::Output
where
	F: Future,
{
	let mut future = core::pin::pin!(future);
	let waker = noop_waker();
	let mut cx = Context::from_waker(&waker);
	let mut last_turn = turn().wrapping_sub(1);

	loop {
		let current_turn = turn();
		if current_turn == last_turn {
			spin_loop();
			continue;
		}
		last_turn = current_turn;

		if let Poll::Ready(output) = future.as_mut().poll(&mut cx) {
			return output;
		}
	}
}

unsafe fn noop_clone(_data: *const ()) -> RawWaker {
	noop_raw_waker()
}

unsafe fn noop_wake(_data: *const ()) {}
unsafe fn noop_wake_by_ref(_data: *const ()) {}
unsafe fn noop_drop(_data: *const ()) {}

static NOOP_WAKER_VTABLE: RawWakerVTable =
	RawWakerVTable::new(noop_clone, noop_wake, noop_wake_by_ref, noop_drop);

fn noop_raw_waker() -> RawWaker {
	RawWaker::new(core::ptr::null(), &NOOP_WAKER_VTABLE)
}

fn noop_waker() -> Waker {
	// SAFETY: `NOOP_WAKER_VTABLE` implements a valid no-op waker contract.
	unsafe { Waker::from_raw(noop_raw_waker()) }
}
