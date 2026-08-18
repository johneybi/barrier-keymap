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

## 2026-08-11 duplicate client incident

The Windows server was reachable, but the Mac repeatedly opened four TCP
connections with the same client name, `ESKui-MacBookPro`. Each group completed
the 1.6 protocol hello and screen-info exchange, then collided with the other
instances. The server logged the following every six seconds:

```text
accepted client connection
created proxy for client "ESKui-MacBookPro" version 1.6
a client with name "ESKui-MacBookPro" is already connected
disconnecting client "ESKui-MacBookPro"
```

This is not discovery, firewall, screen-layout, or TCP reachability failure.
The Mac is running multiple supervised or manually launched client instances.

Before another Windows test, inspect the GUI, LaunchAgents, login items, and
manual `input-leapc` processes. Stop all Mac client instances and start exactly
one owner: either the GUI/supervisor or one CLI process, never both. Make sure a
supervisor does not start another client while one is already running.

The Mac handoff is complete only when:

1. exactly one `input-leapc` process is running;
2. Windows shows one established TCP session from `192.168.0.40` to port
   `24800`;
3. the server logs one successful `ESKui-MacBookPro` connection without
   `already connected` or six-second reconnect cycles; and
4. the pointer crosses the configured right edge and remains on the Mac.

On Windows, an old `barriers.exe` process was also found competing with the
new `input-leaps.exe` listener. It was stopped, and the active listener is now
the current Input Leap build. Do not use the old Barrier GUI to supervise a
second Windows server during this test.

## 2026-08-11 post-fix connection observation

After the Windows bitmap clipboard crash fix (`f895d433`), the Windows server
remained alive while one Mac client stayed connected from approximately
08:39:15 through 09:05:56 UTC. During that interval the server recorded normal
screen switches in both directions and clipboard updates. There was no
`string too long` fatal error and no listener loss. The Mac client then
disconnected; the Windows server continued listening on `24800`.

For the next failure report, inspect the macOS client log around the exact
disconnect time and report whether the client process exited, deliberately
restarted, or lost its keep-alive. This observation moves the remaining issue
from Windows bitmap conversion to macOS client lifetime or reconnect handling.

## 2026-08-11 Hangul remap diagnostic

Windows DEBUG1 traces prove that the Hangul key path is working through the
network boundary. At 16:33:22 through 16:33:24 UTC, each key tap produced:

```text
onKeyDown id=61233 ... button=0x0138
key remap pending tap ... key=\uef31 alone=\uee08 hold=Super_R
onKeyUp id=61233 ... button=0x0138
key remap tap ... key=\uef31->\uee08
send key down ... id=60936, mask=0x0000, button=0x0138
send key up ... id=60936, mask=0x0000, button=0x0138
```

The Windows configuration and the Input Leap protocol are therefore not the
reason the macOS input source did not visibly change. Investigate the macOS
post-receive path:

```text
kKeyNextGroup (0xEE08)
  -> KeyMap group keystroke
  -> OSXKeyState::cycleInputSource(+1)
  -> TISSelectInputSource(target)
```

Run the macOS client at `DEBUG1`, tap the Windows Hangul key once while the
pointer is on the Mac, and capture the line beginning:

```text
cycle macOS input source offset=+1 current=... target=... status=...
```

The current implementation filters source candidates by
`kTISPropertyInputSourceIsSelectCapable` but does not additionally require
`kTISPropertyInputSourceIsEnabled`. This contradicts the intended behavior of
cycling enabled sources only and can select a disabled or irrelevant source.
Confirm the captured current/target IDs and status first, then filter the list
by both select-capable and enabled before rebuilding the macOS client.

## 2026-08-15 Windows handoff: pending local installer approval

Windows commit `9189c389` fixes an error in the Keymap GUI config writer. Before
this fix, every saved remap opened a new screen block, for example:

```text
section: remaps
    ESKui-MacBookPro:
        right_alt.alone = \\uee08
    ESKui-MacBookPro:
        hangul.alone = \\uee08
end
```

The Input Leap config grammar requires one screen block containing all of that
screen's rules. The generated config now has exactly one block:

```text
section: remaps
    ESKui-MacBookPro:
        right_alt.alone = F19
        right_alt.hold = right_super
        hangul.alone = F19
        hangul.hold = right_super
        control+c = command+c
        control+v = command+v
        print_screen = command+shift+4
end
```

The Windows and macOS package CI run for this commit succeeded. The Windows
installer artifact is available locally as
`releases/InputLeapKeymap-9189c389/InputLeapKeymapSetup-3.0.3-0.exe`.

At handoff time, Windows had no `input-leap` or `input-leaps` process and no
TCP listener on port `24800`: the installer is waiting for local Windows
elevation approval. Do not diagnose a Mac connection timeout as a client
failure until the Windows installer is accepted and the GUI server is running.

After the Windows GUI is started, verify both sides in this order:

1. Windows: `input-leaps.exe` owns TCP `24800` and has one established session
   from the Mac.
2. macOS: exactly one Input Leap client named `ESKui-MacBookPro` is running and
   its log says it connected without a duplicate-name reconnect loop.
3. Move the pointer to the Mac and press each physical side button once.

The Windows branch already contains side-button forwarding fixes in `6c7b0388`
and `7e634066`. Run the Windows GUI at `DEBUG1` only for this short test. The
decisive Windows lines are:

```text
event: button press button=4
event: button press button=5
```

If those lines are absent, the mouse driver is exposing its side buttons as
browser keyboard keys rather than `XBUTTON1`/`XBUTTON2`; send the surrounding
Windows DEBUG1 key log to the Windows owner. If the lines are present, capture
the corresponding macOS client receive/fake-mouse-button log to distinguish
network forwarding from Quartz event injection.
