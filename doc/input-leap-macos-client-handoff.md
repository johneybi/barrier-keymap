# Input Leap macOS arm64 client handoff

Use this client with the Windows server from branch
`codex/input-leap-keymap`. The native mappings currently under test do not
require Karabiner or VirtualHID.

## Download and install

```sh
run_id="$(gh run list \
  -R johneybi/barrier-keymap \
  -w input-leap-keymap-ci.yml \
  -b codex/input-leap-keymap \
  -s success -L 1 \
  --json databaseId --jq '.[0].databaseId')"

gh run download "$run_id" \
  -R johneybi/barrier-keymap \
  -n input-leap-keymap-macos-arm64-app

tar -xzf input-leap-keymap-macos-arm64.tar.gz
cp -R "Input Leap Keymap.app" /Applications/
xattr -dr com.apple.quarantine "/Applications/Input Leap Keymap.app"
codesign --verify --deep --strict --verbose=2 \
  "/Applications/Input Leap Keymap.app"
codesign -d -r- "/Applications/Input Leap Keymap.app" 2>&1
```

Keep the application path and bundle identifier stable so macOS Accessibility
permission remains associated with the client.

Beta packages use a stable ad-hoc designated requirement. The final command
above must report:

```text
designated => identifier "com.johneybi.input-leap-keymap.client"
```

Packages made before this requirement was added were identified only by their
changing binary cdhash. After replacing one of those older packages, remove
the stale Accessibility entry, add `Input Leap Keymap` again, and grant it once.

## Permissions

In System Settings, open Privacy & Security > Accessibility and enable:

```text
Input Leap Keymap
```

Do not run the client as root. Karabiner and VirtualHID are not part of this
test path.

## Start the client

Open `Input Leap Keymap` from Applications. On the first launch, enable the
same application in System Settings > Privacy & Security > Accessibility,
then reopen it.

In the GUI:

1. Select `Client`.
2. Disable `Auto config` and enter `192.168.0.10` as the server IP.
3. Open Settings and set the screen name to `ESKui-MacBookPro`.
4. Disable SSL and enable `Show Tray Icon upon App Start`.
5. Press Start.

The Windows server configuration contains only the client name
`ESKui-MacBookPro`, without a `.local` suffix.

The GUI owns and monitors the bundled `input-leapc` process. Closing a Terminal
or Codex task no longer stops the client. Avoid starting the bundled command-line
client separately, because two clients with the same screen name cause duplicate
server connections and can make input appear intermittent.

## Current Windows state

The Windows server is already receiving the Korean-layout Right Alt key as
`Hangul` and remapping a tap to Input Leap's semantic input-source command:

```text
Windows Right Alt
  -> Hangul (KeyID 0xEF31, button 0x0138)
  -> NextGroup (KeyID 0xEE08)
  -> macOS Input Leap client
```

The Windows DEBUG1 log has confirmed both the match and transmission:

```text
key remap pending tap ... key=\uef31 alone=\uee08
key remap tap ... key=\uef31->\uee08
send key down ... id=60936, mask=0x0000
send key up ... id=60936, mask=0x0000
```

The Windows handoff is confirmed working: the Mac receives `0xEE08`, executes
the relative group keystroke, and selects the next enabled macOS input source.

## Input-source test

Confirm that macOS has at least two enabled input sources. With the pointer on
the Mac, tap Windows Right Alt. The implemented path is:

```text
KeyState::fakeKeyDown(kKeyNextGroup)
  -> KeyMap::mapKey()
  -> Keystroke::kGroup
  -> OSXKeyState::fakeKey()
  -> OSXKeyState::cycleInputSource()
  -> TISSelectInputSource()
```

Keyboard layout groups and selectable input sources are intentionally separate.
Regular key mapping uses only sources with Unicode keyboard-layout data, while
`NextGroup` cycles enabled, selectable sources such as ABC and 2-Set Korean.
This prevents a source switch around every ordinary key event.

## Confirmed macOS latency fix

Current macOS versions can leave Carbon `Syne` wake events pending for hundreds
of milliseconds. Before the fix, the client received 278 to 482 mouse messages
per second but forwarded only 2 to 4 positions. The macOS event buffer now
keeps Input Leap event IDs in a thread-safe FIFO and checks it at least every
4 ms. A live test forwarded 169 of 708 coalesced positions in one second and
the pointer was smooth.

Keep production tests at `INFO`. `DEBUG1` is useful for a single key diagnosis,
but should not be used for sustained usability testing.

## Remaining keymap checks

After native input-source switching works, test in this order while the pointer
is on the Mac:

1. Tap Windows Right Alt: the macOS input source changes exactly once.
2. Hold Right Alt with another key: the configured hold modifier is emitted.
3. Press Control+C and Control+V: they behave as Command+C and Command+V.
4. Press Print Screen: macOS starts the Command+Shift+4 capture tool.

The Mac client log should remain connected without repeated enter/leave or
keep-alive failures.

## Immediate screen-switch bounce guard

The macOS client log can show a rapid `enter`/`leave` pair even while the
connection and mouse event rate remain healthy. In this case the Windows server
has interpreted a stale opposite-direction delta immediately after crossing an
edge and switched straight back to the screen the pointer just left.

The server now ignores only a return to the immediately previous screen during
the first 250 ms after a switch. It does not delay entry, block another screen,
or prevent an intentional return after that interval. At `DEBUG1`, a suppressed
event is logged as:

```text
ignoring immediate switch back to "<screen-name>"
```

After rebuilding the Windows server, repeatedly enter the Mac at different
speeds and confirm that it stays there. Also confirm that moving deliberately
back to Windows after a short pause still works normally.
