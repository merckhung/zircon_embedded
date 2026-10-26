#ifndef ZIRCON_EMBEDDED_SRC_SHIM_TTY_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_TTY_H_

// Terminal layer: line discipline shared by the UART console and the
// /dev/ptmx <-> /dev/pts/N pseudo terminals, controlling-terminal / session
// bookkeeping and tty-generated signals.

#include "src/shim/shim_common.h"

// The UART-backed system console tty (created on first use).
std::shared_ptr<PtyPair> GetConsoleTty();

// Line-discipline input: bytes "typed" into a tty (PTY master writes, UART
// receive, TIOCSTI). Performs ICRNL/INLCR/IGNCR/ISTRIP, ISIG (VINTR/VQUIT/
// VSUSP -> signals to the foreground process group), canonical editing
// (VERASE/VKILL/VWERASE/VEOF/VEOL/VEOL2/VLNEXT) and echo. Caller must NOT hold
// tty->mtx. Echo output for the console is written to the UART.
void TtyInput(const std::shared_ptr<PtyPair>& tty, const uint8_t* buf,
              size_t len);

// Output processing (OPOST/ONLCR/OCRNL/ONOCR/ONLRET) of bytes written to the
// slave side; appends to `out`. Caller holds tty->mtx.
void TtyProcessOutputLocked(const LinuxTermios& t, const uint8_t* buf,
                            size_t len, std::deque<uint8_t>* out);

// Slave-side read honouring ICANON line boundaries / VEOF and VMIN/VTIME.
// Returns bytes read, 0 on EOF, or a negative errno.
ssize_t TtySlaveRead(LinuxProcess* proc, const std::shared_ptr<PtyPair>& tty,
                     void* buf, size_t count, bool nonblock);
// Slave-side write (output processing, VSTOP flow control, hangup -> EIO).
ssize_t TtySlaveWrite(LinuxProcess* proc, const std::shared_ptr<PtyPair>& tty,
                      const void* buf, size_t count, bool nonblock);
// Master-side read (TIOCPKT framing, EIO once all slaves closed).
ssize_t TtyMasterRead(LinuxProcess* proc, const std::shared_ptr<PtyPair>& tty,
                      void* buf, size_t count, bool nonblock);
uint32_t TtyPollEvents(const std::shared_ptr<PtyPair>& tty, bool master);
uint64_t TtyEdgeCounter(const std::shared_ptr<PtyPair>& tty, bool master);
int TtyBytesAvailable(const std::shared_ptr<PtyPair>& tty, bool master);

// PTY allocation / lookup / teardown.
std::shared_ptr<PtyPair> PtyAllocate();
std::shared_ptr<PtyPair> PtyLookup(int index);
// Called when the last master description is destroyed: hang up the slave
// side, remove /dev/pts/N and free the index.
void PtyMasterClosed(const std::shared_ptr<PtyPair>& pty);
void PtySlaveClosed(const std::shared_ptr<PtyPair>& pty);

// Controlling terminal of `proc`'s session (nullptr if none).
std::shared_ptr<PtyPair> TtyGetCtty(LinuxProcess* proc);
// Called when a tty is opened without O_NOCTTY: a session leader without a
// controlling terminal acquires it (Linux tty_open rules). `can_be_ctty`
// is false for PTY masters and /dev/console.
void TtyMaybeAcquireCtty(LinuxProcess* proc,
                         const std::shared_ptr<PtyPair>& tty);
// For /proc/<pid>/stat: device number of the session's controlling tty and
// its foreground process group. Returns false if the session has none.
bool TtySessionInfo(int sid, uint64_t* rdev, int* fg_pgrp);
uint64_t TtyRdev(const std::shared_ptr<PtyPair>& tty, bool master);

// Terminal ioctls on a tty description (console, PTY master or slave).
// Returns kENOTTY for requests that are not terminal ioctls.
int64_t TtyIoctl(LinuxProcess* proc, FileDescription* desc,
                 const std::shared_ptr<PtyPair>& tty, bool master,
                 uint32_t req, uintptr_t arg);

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_TTY_H_
