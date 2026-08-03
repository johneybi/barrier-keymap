# macOS GUI application handoff

## Objective

Turn the proven macOS command-line client into an installable Barrier GUI
application. The user must be able to launch it normally, keep it in the menu
bar, reconnect automatically, and enable or disable launch at login without
running Terminal commands.

Start from branch `stable/v2.4.0-virtual-hid` at or after commit `198e02b1`.
Pull the branch before changing code.

Application-specific remaps are explicitly deferred. Key mappings remain a
Windows server responsibility for this version.

## Proven live contract: do not regress

The stable client path is:

```text
Windows Right Alt / Hangul tap
-> Windows server remapper F19
-> macOS Barrier client ordinary IOHID output
-> macOS F19 input-source shortcut
-> Korean/English input source toggles once
```

Keep all of these conditions:

- client screen name: `ESKui-MacBookPro`;
- server: `192.168.0.10:24800`;
- SSL disabled for the current coordinated test;
- INFO logging for normal use;
- exactly one `barrierc` process;
- ordinary IOHID output;
- no `BARRIER_MAC_KEY_OUTPUT=virtual-hid` environment variable;
- no privileged VirtualHID helper in the live path;
- macOS input-source shortcut remains F19.

Do not revert F19 to F16, add an F16-to-Control+Space client transform, or
change the input injection path while doing GUI packaging.

## Existing reusable GUI support

The repository already contains a Qt 5 GUI in `src/gui` and a macOS bundle
template in `dist/macos/bundle`. Existing GUI behavior includes:

- client/server selection;
- saved server address and screen name;
- start and stop actions;
- process restart behavior;
- Zeroconf auto configuration;
- menu bar icon and menu;
- minimize-to-tray and auto-hide settings;
- `.app` and `.dmg` packaging scripts.

The cross-platform GUI currently compiles only on Linux CI. macOS CI configures
`BARRIER_BUILD_GUI=OFF`, so the macOS GUI and bundle are not yet verified.

## Required implementation

1. Enable the Qt GUI in a macOS CI or local Release build and fix only the
   macOS-specific compile/link/package failures that appear.
2. Ensure `Barrier.app` contains `barrier`, `barrierc`, and all required Qt and
   runtime libraries. The GUI must launch its bundled `barrierc`, not another
   installed Barrier copy.
3. Implement `Launch in background at login` for macOS. The current
   `MainWindow::launchAtLoginEnabled()` and `setLaunchAtLoginEnabled()` branches
   return false/do nothing outside Windows. Use an appropriate per-user macOS
   mechanism such as a LaunchAgent, with an absolute path to the installed app.
4. Make the login item start the GUI hidden, with its menu bar icon available,
   and start exactly one client process when auto-start is enabled.
5. Persist and restore client mode, screen name, server address, SSL state,
   auto-connect, auto-start, auto-hide, and minimize-to-tray settings.
6. Prevent duplicate GUI and `barrierc` instances. If an old command-line
   client is running, report it clearly instead of starting a competing client.
7. Preserve normal macOS Accessibility/Input Monitoring behavior. The app must
   provide a useful visible status when permissions are missing. Do not attempt
   to bypass macOS permission prompts.
8. Produce a local Release `Barrier.app` and `.dmg`. Ad-hoc signing is acceptable
   for the first coordinated test; document whether the user must use Open or
   remove quarantine. Do not claim notarization unless it was actually done.
9. Add a repeatable macOS GUI build workflow or script and upload the `.dmg` as
   a CI artifact when feasible.

## First-run defaults for the coordinated test

Configure or make it easy to enter:

```text
Mode: Client
Screen name: ESKui-MacBookPro
Server: 192.168.0.10
Port: 24800
SSL: Off
Log level: INFO
Auto connect: On
Minimize to menu bar: On
Launch at login: user-selectable
```

Do not use `ESKui-MacBookPro.local`. Windows server layout must contain only
`ESKui-MacBookPro` for this test.

## Acceptance test

Complete all of the following on the physical Mac:

1. Launch `Barrier.app` from Finder without Terminal.
2. Confirm one GUI process and one bundled `barrierc` process.
3. Confirm connection to `192.168.0.10:24800` and automatic reconnection after
   restarting the Windows server.
4. Confirm the app can hide while its menu bar icon remains usable.
5. Toggle auto-connect from the menu bar and verify the connection stops and
   resumes as represented by the UI.
6. Enable launch at login, log out/in or reboot, and confirm one hidden GUI and
   one client start automatically. Disable it and confirm it no longer starts.
7. Confirm smooth pointer movement from Windows to Mac and back.
8. Confirm Right Alt toggles Korean/English exactly once.
9. Confirm Ctrl+C, Ctrl+V, and Print Screen mappings still work.
10. Test Korean composition separately in TextEdit, Safari, and Chromium. Do
    not treat the known Chromium composition problem as an F19 toggle failure.

## Report back through Git

Commit code, workflow, and documentation changes to the same branch and push
them. Update this document or add a short test report containing:

- commit SHA;
- macOS and CPU architecture;
- Qt/CMake/Xcode versions;
- `.app` and `.dmg` paths and SHA-256 values;
- signing/notarization status;
- exact launch-at-login implementation;
- process counts;
- permission state;
- acceptance-test results;
- relevant INFO logs for failures only.

Do not commit local certificates, signing identities, provisioning data,
private logs, or user-specific LaunchAgent files.

## 2026-08-03 local beta build report

Implementation commit: `ba8de12d` (`Build installable macOS GUI client`)

Build environment:

- macOS 26.5 (build 25F71), Apple Silicon arm64;
- Apple clang 21.0.0;
- CMake 4.3.2;
- Qt 5.15.19 from Homebrew;
- Command Line Tools SDK, without the full Xcode application.

Implemented GUI behavior:

- the GUI, client, and server executables are bundled together, and the GUI
  resolves `barrierc` from its own `Contents/MacOS` directory;
- a per-user LaunchAgent at
  `~/Library/LaunchAgents/org.barrier-foss.barrier-keymap.plist` launches the
  installed app executable with `--background`;
- the login action also enables hidden startup, menu-bar operation, and
  automatic client start through the existing saved settings;
- a `QLockFile` prevents duplicate GUI instances;
- an existing independently launched `barrierc` is reported before the GUI
  starts another client;
- a stopped child process is deleted before automatic restart, preventing
  stale process objects from accumulating;
- missing Accessibility permission produces a visible instruction and the app
  exits until the user grants permission;
- macOS CI now builds the Qt GUI, runs GUI tests, creates a signed app bundle,
  and uploads the DMG artifact.

Local artifacts:

```text
/private/tmp/barrier-mac-gui-build/bundle/Barrier.app
/private/tmp/barrier-mac-gui-build/bundle/Barrier-2.4.0-release.dmg
```

SHA-256:

```text
GUI executable:
0898bf3a77415854b336a4669f0145e73f30059fe26c833001435afffbdcc72d

barrierc executable:
4665d89dc54f24fbf398dbbca895cd4b3f6473d3e65ad0b25cfa401c62db05c7

DMG:
0b7a7c29c229395a646caec47f9e67b3fbc433086126e9aae9a19d4fb6d1c722
```

Verification completed:

- 152 core unit tests passed;
- 59 GUI unit tests passed, including LaunchAgent write/read/remove coverage;
- `codesign --verify --deep --strict` passed;
- the bundle contains only `barrier`, `barrierc`, and `barriers` in
  `Contents/MacOS`;
- the main executables are arm64 and no longer reference Homebrew Qt paths;
- the bundle is ad-hoc signed and is not notarized.

Physical acceptance testing is still pending. Process counts, permission
state, connection and reconnection, pointer smoothness, F19 input-source
toggle, clipboard shortcuts, Print Screen, Korean composition across apps,
and extra mouse buttons must be checked after installing the app in
`/Applications`. Because this beta is not Developer ID signed or notarized,
the first launch may require Finder's Open command or removal of quarantine.

## 2026-08-03 first physical GUI connection failure

The first physical test of the macOS GUI beta did not produce a usable client
session. The Windows server remained healthy and listening on
`192.168.0.10:24800`, but its INFO log recorded this sequence:

```text
13:33:06-13:33:19 client "ESKui-MacBookPro" repeatedly connected and disconnected
13:33:20 client "ESKui-MacBookPro" connected
13:33:23/26 small TCP output writes succeeded
13:33:29 client "ESKui-MacBookPro" is dead
```

After that, Windows showed only a stale `FIN_WAIT_2` connection and the mouse
could not cross to the Mac. This is not a Windows screen-layout failure: there
was no live client to enter.

The Mac GUI initially inherited SSL enabled while the coordinated Windows
server uses SSL disabled. The user disabled SSL, after which the protocol
handshake reached `client has connected`, but the bundled client still exited
or stopped responding within seconds. Commit `51831f83` changes the first-run
GUI default to SSL disabled, but existing saved settings must still be changed
manually once.

Before changing protocol or input code, diagnose the macOS GUI process
lifecycle on the physical Mac:

1. Confirm there is exactly one GUI and one `barrierc`, and record both PIDs.
2. Confirm the GUI launches `/Applications/Barrier.app/Contents/MacOS/barrierc`
   rather than an older command-line build.
3. Capture the GUI Show Log output and the complete bundled `barrierc` INFO
   output from process start through disconnect.
4. Record the child exit code/signal and whether the GUI immediately restarts
   it.
5. Check macOS crash reports and Accessibility/Input Monitoring permission for
   the installed app and bundled client.
6. Disable launch at login temporarily and stop every old command-line client
   before one foreground GUI test.
7. Re-test with screen name `ESKui-MacBookPro`, server `192.168.0.10`, port
   `24800`, SSL off, and auto-config off so only the explicit server address is
   used.
8. Compare the exact GUI-generated `barrierc` command line with the last proven
   command-line invocation. First fix any argument, environment, working
   directory, or restart-policy difference; do not alter the proven F19/IOHID
   path to address this lifecycle failure.

Commit the diagnosis and fix to the same branch, including the minimal INFO
log lines and the physical re-test result.

## 2026-08-03 severe pointer stutter after GUI reconnect

A later GUI client session stayed connected long enough to enter the Mac, but
the remote pointer appeared to update at roughly one frame per second. The
Windows INFO log rules out server capture and TCP backpressure during the
affected interval:

```text
13:36:21 switch to ESKui-MacBookPro
13:36:22 634 writes, 8068 bytes, pending 4 bytes
13:36:23 469 writes, 5648 bytes, pending 12 bytes
13:36:24 587 writes, 7044 bytes, pending 4 bytes
13:36:25 412 writes, 4944 bytes, pending 4 bytes
13:36:26 313 writes, 3752 bytes, pending 4 bytes
13:36:27 434 writes, 5220 bytes, pending 4 bytes
13:36:28 627 writes, 7572 bytes, pending 4 bytes
13:36:29 874 writes, 10508 bytes, pending 4 bytes
13:36:30 switch back to DESKTOP-BJNR3KH
```

The Windows server used INFO logging and consumed only about 0.016 CPU seconds
over a five-second sample. It was sending hundreds of writes per second with
negligible pending data. Investigate the macOS receive/event-injection path,
not Windows event generation.

First, inspect the Mac GUI's saved log level. Old Barrier GUI settings may be
reused by the beta. Set the client to INFO, disable file/event-volume logging,
stop it completely, and start it once. DEBUG1/DEBUG2 are prohibited for the
usability test because per-event logging previously caused the same severe
stutter.

If INFO still stutters, perform an A/B test using the exact bundled
`/Applications/Barrier.app/Contents/MacOS/barrierc` binary and identical
arguments:

1. run it once as the GUI child and record its exact command, environment,
   working directory, log level, and process CPU;
2. quit the GUI completely and run that same bundled client directly;
3. compare pointer smoothness and Mac-side receive/injection timing;
4. identify whether the regression follows the binary or only the GUI parent;
5. do not enable DEBUG1 during the movement interval; use bounded counters or
   sampling instead.

Report the saved GUI log level and A/B result before changing IOHID, F19,
compression, keep-alive, or the Windows remapper.
