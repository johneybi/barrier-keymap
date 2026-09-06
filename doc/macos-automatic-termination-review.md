# macOS automatic-termination restart loop (#10)

## Observed evidence

On 2026-09-07 at 04:44:36–04:45:00 KST, macOS unified logs recorded 17
different input-leapc PIDs each reporting AppKit memory-pressure automatic
termination and then completion of termination. Client startup/connection logs
show repeated starts roughly one second after exits. This identifies an
automatic-exit/relaunch mechanism for this episode, not just a TCP reconnect.

Clipboard payloads logged in this episode were only 68–590 bytes. The source
of system memory pressure is unknown; these observations do not establish an
Input Leap leak, a large-clipboard cause, or the cause of the earlier freeze.
The installed app's Info.plist does not declare NSSupportsAutomaticTermination;
that absence did not prevent the observed AppKit automatic-exit behavior.

## Targeted source change

`OSXDragSimulator.mm::runCocoaApp()` is shared by the macOS client and server.
A noncopyable scope guard now calls NSProcessInfo.disableAutomaticTermination
before NSApplication initialization and balances it with enableAutomaticTermination
when the Cocoa loop returns, before the autorelease pool drains.

The guard covers the service loop, including idle and reconnect periods: no
visible window does not mean an input-sharing service is unused. It does not
opt into automatic-termination support, change Info.plist, veto explicit quit,
prevent sleep, or protect against forced OS kills. Existing Stop/IPC/signal
handling remains unchanged. No App Nap/power assertion is introduced.

Apple documents that the opt-out counter is maintained even if support is
enabled later, and specifically describes maintaining background connections
as a reason to defer automatic termination:

- https://developer.apple.com/documentation/foundation/processinfo/disableautomatictermination(_:)?language=objc
- https://developer.apple.com/documentation/foundation/processinfo/automaticterminationsupportenabled?language=objc

## Validation and deployment boundary

No installed binary, configuration, process, or clipboard was changed. This
is on the Input Leap source branch, not the Barrier code line. The installed
experimental TIS/Gureum source still needs reconciliation before deployment.

`git diff --check` passes. Direct syntax checking is blocked by the missing
generated config.h. An isolated Objective-C++ syntax check of the actual .mm
source with only the config-dependent OSXDragSimulator.h import omitted passes
against the local macOS SDK; it reports the pre-existing deprecated window-mask
constant. This does not replace a configured build or runtime validation.

Before release, on a disposable test environment (not the user's active Mac):

1. Build the reconciled macOS client/server; verify the guard is compiled in.
2. Verify the automatic-termination opt-out count increases by one during the
   service loop and is balanced after an explicit Stop; repeat start/stop.
3. Exercise connection, idle, reconnect, explicit Stop/IPC shutdown and normal
   system shutdown. No input-service process should survive explicit Stop.
4. Under controlled memory pressure, compare baseline and patched behavior:
   no AppKit auto-quit/relaunch loop while the service is active. Do not promise
   survival of a genuine forced low-memory kill.
5. Check ordinary mouse, keyboard, clipboard and drag behavior for regressions.

This patch targets the observed auto-quit path. It does not fix memory pressure,
network loss or Safari IME behavior. Keep #10 open pending runtime verification.
