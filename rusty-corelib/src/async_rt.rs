use alloc::boxed::Box;
use alloc::vec::Vec;
use core::future::Future;
use core::hint::spin_loop;
use core::pin::Pin;
use core::task::{Context, Poll, RawWaker, RawWakerVTable, Waker};

pub use core::future::poll_fn;

use crate::{Direction, Result, turn};

type BoxedTask = Pin<Box<dyn Future<Output = Result<()>> + 'static>>;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TaskHandle {
	id: u64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TaskResult {
	NoTurnAdvance,
	PendingTasks(usize),
	CompletedAll,
}

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

pub fn wait_until(target_turn: i32) -> WaitUntil {
	WaitUntil { target_turn }
}

pub fn wait_for(turns: i32) -> WaitUntil {
	let current_turn = turn();
	WaitUntil {
		target_turn: current_turn + turns,
	}
}

pub fn wait_next_turn() -> WaitUntil {
	wait_for(1)
}

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

pub fn recv_msg(out: &mut [u8]) -> RecvMsgFuture<'_> {
	RecvMsgFuture { out }
}

pub async fn move_unit(direction: Direction) -> Result<()> {
	crate::move_unit(direction)?;
	wait_next_turn().await;
	Ok(())
}

struct TaskSlot {
	handle: TaskHandle,
	task: BoxedTask,
}

pub struct Scheduler {
	tasks: Vec<TaskSlot>,
	next_id: u64,
	last_turn: i32,
}

impl Default for Scheduler {
	fn default() -> Self {
		Self::new()
	}
}

impl Scheduler {
	pub fn new() -> Self {
		Self {
			tasks: Vec::new(),
			next_id: 1,
			last_turn: turn().wrapping_sub(1),
		}
	}

	pub fn with_capacity(capacity: usize) -> Self {
		Self {
			tasks: Vec::with_capacity(capacity),
			next_id: 1,
			last_turn: turn().wrapping_sub(1),
		}
	}

	pub fn len(&self) -> usize {
		self.tasks.len()
	}

	pub fn is_empty(&self) -> bool {
		self.tasks.is_empty()
	}

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

	pub fn contains(&self, handle: TaskHandle) -> bool {
		self.tasks.iter().any(|slot| slot.handle == handle)
	}

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
