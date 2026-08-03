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
