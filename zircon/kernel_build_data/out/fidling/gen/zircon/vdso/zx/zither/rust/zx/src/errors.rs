// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT.
// Generated from FIDL library `zx` by zither, a Fuchsia platform tool.

/// Indicates an operation was successful.
pub const OK: i32 = 0;

/// The system encountered an otherwise unspecified error while performing the
/// operation.
pub const ERR_INTERNAL: i32 = -1;

/// The operation is not implemented, supported, or enabled.
pub const ERR_NOT_SUPPORTED: i32 = -2;

/// The system was not able to allocate some resource needed for the operation.
pub const ERR_NO_RESOURCES: i32 = -3;

/// The system was not able to allocate memory needed for the operation.
pub const ERR_NO_MEMORY: i32 = -4;

/// The system call was interrupted, but should be retried. This should not be
/// seen outside of the VDSO.
pub const ERR_INTERNAL_INTR_RETRY: i32 = -6;

/// An argument is invalid. For example, a null pointer when a null pointer is
/// not permitted.
pub const ERR_INVALID_ARGS: i32 = -10;

/// A specified handle value does not refer to a handle.
pub const ERR_BAD_HANDLE: i32 = -11;

/// The subject of the operation is the wrong type to perform the operation.
///
/// For example: Attempting a message_read on a thread handle.
pub const ERR_WRONG_TYPE: i32 = -12;

/// The specified syscall number is invalid.
pub const ERR_BAD_SYSCALL: i32 = -13;

/// An argument is outside the valid range for this operation.
pub const ERR_OUT_OF_RANGE: i32 = -14;

/// The caller-provided buffer is too small for this operation.
pub const ERR_BUFFER_TOO_SMALL: i32 = -15;

/// The operation failed because the current state of the object does not allow
/// it, or a precondition of the operation is not satisfied.
pub const ERR_BAD_STATE: i32 = -20;

/// The time limit for the operation elapsed before the operation completed.
pub const ERR_TIMED_OUT: i32 = -21;

/// The operation cannot be performed currently but potentially could succeed if
/// the caller waits for a prerequisite to be satisfied, like waiting for a
/// handle to be readable or writable.
///
/// Example: Attempting to read from a channel that has no messages waiting but
/// has an open remote will return `ZX_ERR_SHOULD_WAIT`. In contrast, attempting
/// to read from a channel that has no messages waiting and has a closed remote
/// end will return `ZX_ERR_PEER_CLOSED`.
pub const ERR_SHOULD_WAIT: i32 = -22;

/// The in-progress operation, for example, a wait, has been canceled.
pub const ERR_CANCELED: i32 = -23;

/// The operation failed because the remote end of the subject of the operation
/// was closed.
pub const ERR_PEER_CLOSED: i32 = -24;

/// The requested entity is not found.
pub const ERR_NOT_FOUND: i32 = -25;

/// An object with the specified identifier already exists.
///
/// Example: Attempting to create a file when a file already exists with that
/// name.
pub const ERR_ALREADY_EXISTS: i32 = -26;

/// The operation failed because the named entity is already owned or controlled
/// by another entity. The operation could succeed later if the current owner
/// releases the entity.
pub const ERR_ALREADY_BOUND: i32 = -27;

/// The subject of the operation is currently unable to perform the operation.
///
/// This is used when there's no direct way for the caller to observe when the
/// subject will be able to perform the operation and should thus retry.
pub const ERR_UNAVAILABLE: i32 = -28;

/// The caller did not have permission to perform the specified operation.
pub const ERR_ACCESS_DENIED: i32 = -30;

/// Otherwise-unspecified error occurred during I/O.
pub const ERR_IO: i32 = -40;

/// The entity the I/O operation is being performed on rejected the operation.
///
/// Example: an I2C device NAK'ing a transaction or a disk controller rejecting
/// an invalid command, or a stalled USB endpoint.
pub const ERR_IO_REFUSED: i32 = -41;

/// The data in the operation failed an integrity check and is possibly
/// corrupted.
///
/// Example: CRC or Parity error.
pub const ERR_IO_DATA_INTEGRITY: i32 = -42;

/// The data in the operation is currently unavailable and may be permanently
/// lost.
///
/// Example: A disk block is irrecoverably damaged.
pub const ERR_IO_DATA_LOSS: i32 = -43;

/// The device is no longer available (has been unplugged from the system,
/// powered down, or the driver has been unloaded).
pub const ERR_IO_NOT_PRESENT: i32 = -44;

/// More data was received from the device than expected.
///
/// Example: a USB "babble" error due to a device sending more data than the
/// host queued to receive.
pub const ERR_IO_OVERRUN: i32 = -45;

/// An operation did not complete within the required timeframe.
///
/// Example: A USB isochronous transfer that failed to complete due to an
/// overrun or underrun.
pub const ERR_IO_MISSED_DEADLINE: i32 = -46;

/// The data in the operation is invalid parameter or is out of range.
///
/// Example: A USB transfer that failed to complete with TRB Error
pub const ERR_IO_INVALID: i32 = -47;

/// Path name is too long.
pub const ERR_BAD_PATH: i32 = -50;

/// The object is not a directory or does not support directory operations.
///
/// Example: Attempted to open a file as a directory or attempted to do
/// directory operations on a file.
pub const ERR_NOT_DIR: i32 = -51;

/// Object is not a regular file.
pub const ERR_NOT_FILE: i32 = -52;

/// This operation would cause a file to exceed a filesystem-specific size
/// limit.
pub const ERR_FILE_BIG: i32 = -53;

/// The filesystem or device space is exhausted.
pub const ERR_NO_SPACE: i32 = -54;

/// The directory is not empty for an operation that requires it to be empty.
///
/// For example, non-recursively deleting a directory with files still in it.
pub const ERR_NOT_EMPTY: i32 = -55;

/// An indicate to not call again.
///
/// The flow control values `ZX_ERR_STOP`, `ZX_ERR_NEXT`, and `ZX_ERR_ASYNC` are
/// not errors and will never be returned by a system call or public API. They
/// allow callbacks to request their caller perform some other operation.
///
/// For example, a callback might be called on every event until it returns
/// something other than `ZX_OK`. This status allows differentiation between
/// "stop due to an error" and "stop because work is done."
pub const ERR_STOP: i32 = -60;

/// Advance to the next item.
///
/// The flow control values `ZX_ERR_STOP`, `ZX_ERR_NEXT`, and `ZX_ERR_ASYNC` are
/// not errors and will never be returned by a system call or public API. They
/// allow callbacks to request their caller perform some other operation.
///
/// For example, a callback could use this value to indicate it did not consume
/// an item passed to it, but by choice, not due to an error condition.
pub const ERR_NEXT: i32 = -61;

/// Ownership of the item has moved to an asynchronous worker.
///
/// The flow control values `ZX_ERR_STOP`, `ZX_ERR_NEXT`, and `ZX_ERR_ASYNC` are
/// not errors and will never be returned by a system call or public API. They
/// allow callbacks to request their caller perform some other operation.
///
/// Unlike `ZX_ERR_STOP`, which implies that iteration on an object
/// should stop, and `ZX_ERR_NEXT`, which implies that iteration
/// should continue to the next item, `ZX_ERR_ASYNC` implies
/// that an asynchronous worker is responsible for continuing iteration.
///
/// For example, a callback will be called on every event, but one event needs
/// to handle some work asynchronously before it can continue. `ZX_ERR_ASYNC`
/// implies the worker is responsible for resuming iteration once its work has
/// completed.
pub const ERR_ASYNC: i32 = -62;

/// The specified protocol is not supported.
pub const ERR_PROTOCOL_NOT_SUPPORTED: i32 = -70;

/// The host is unreachable.
pub const ERR_ADDRESS_UNREACHABLE: i32 = -71;

/// Address is being used by someone else.
pub const ERR_ADDRESS_IN_USE: i32 = -72;

/// The socket is not connected.
pub const ERR_NOT_CONNECTED: i32 = -73;

/// The remote peer rejected the connection.
pub const ERR_CONNECTION_REFUSED: i32 = -74;

/// The connection was reset.
pub const ERR_CONNECTION_RESET: i32 = -75;

/// The connection was aborted.
pub const ERR_CONNECTION_ABORTED: i32 = -76;

/// A task was killed during an operation. This is a private error that should
/// not be seen outside of the VDSO.
pub const ERR_INTERNAL_INTR_KILLED: i32 = -502;
