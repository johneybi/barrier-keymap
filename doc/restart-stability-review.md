# Source-only restart stability change

Base: `e36e16ff` (`origin/codex/input-leap-keymap`). This is the Input Leap
branch, not the Barrier-based `fix/input-semantics-and-stability` branch.
The installed `3.0.3-gureum-modes` experimental app is not reproduced by this
base: its local TIS/Gureum edits need recovery/reconciliation before deployment.
Do not replace that app with this branch merely to try these lifecycle fixes.

## Issue 10 findings and changes

The Input Leap base already guards an active QProcess and ignores exit signals
from superseded processes. The earlier issue's claim that this guard is absent
was based on the other code line and is not applicable to this base.

The remaining pending-restart defect is concrete: a one-shot restart scheduled
after exit could run after Stop and set the desired state back to Started.
An owned QTimer now cancels on Stop, explicit Start and destruction, and checks
the desired state when it fires. Repeated process exits back off to 30 seconds.

Stop detaches and disconnects the owned process before waiting for termination:
waitForFinished can emit finished synchronously, which previously cleared the
member pointer before stopDesktop used it again.

Client reconnects now own one timer, cancel it on success/stop, skip callbacks
while suspended, and use 1/2/4/8/16/30-second bounded backoff. This trades slower
recovery during long outages (up to 30 seconds) for bounded retry traffic.
The backoff is deterministic; jitter is not included in this change.

On macOS, updated GUI instances use a per-user QLockFile before autostart.
This does not coordinate with an old installed GUI that lacks the lock, nor
with independently launched CLI clients or VHID helpers. It is not a complete
system-wide client singleton solution. Runtime lock files are created only if
the new GUI is eventually run; none were created during this source-only work.

None of this proves the cause of the reported overheating. A reconnect log
count is not a process count, and two generations of PIDs do not establish
whether an app restart was caused by a reboot, crash, or user action.

## Validation status and follow-up

Source review and `git diff --check` only. No compilation, test execution,
installation, application launch or restart performed.

`ClientRestartTests.cpp` adds tests for duplicate retry requests, cancellation
on Stop/success, reset after successful connection, and the backoff ceiling.
These are discovered by the existing unit-test source glob after configuration.
They have not been run.

Before deployment, build and run these tests and exercise GUI regressions with
a dummy child executable (no network/input injection):

- Child exits; Stop before timeout; no new child appears.
- Child exits; manual Start before timeout; exactly one child remains.
- Stop an active child; no crash during synchronous finished delivery.
- Repeated immediate exits; retry interval reaches the ceiling.
- Two updated GUIs launch; only one reaches autostart.
- Normal Stop/Start and configuration reload continue to work.

## Issue 9 boundary

No keyboard injection, TIS switching, or Gureum behavior changes are included.
The Safari composition-session explanation remains a hypothesis. First recover
the source corresponding to the working installed baseline; then record F19
receipt, selected source and subsequent key delivery and compare native vs
remote input. A VHID comparison also requires helper authorization/lifetime
work. It must remain opt-in and must not be interpreted as a confirmed fix.
