# Input Leap macOS arm64 client handoff

Use this client with the Windows server from branch
`codex/input-leap-keymap`. This test does not use Karabiner or VirtualHID.

## Download and install

```sh
gh run download 31042349918 \
  -R johneybi/barrier-keymap \
  -n input-leap-keymap-macos-arm64-client

tar -xzf input-leap-keymap-macos-arm64.tar.gz
mkdir -p "$HOME/Applications/InputLeapKeymap"
cp input-leapc "$HOME/Applications/InputLeapKeymap/input-leapc"
chmod 755 "$HOME/Applications/InputLeapKeymap/input-leapc"
xattr -dr com.apple.quarantine "$HOME/Applications/InputLeapKeymap"
codesign --verify --verbose=2 "$HOME/Applications/InputLeapKeymap/input-leapc"
```

Keep this path stable so macOS Accessibility permission remains associated
with the same executable.

## Permissions

In System Settings, open Privacy & Security > Accessibility and enable:

```text
~/Applications/InputLeapKeymap/input-leapc
```

Do not run the client as root. Karabiner and VirtualHID are not part of this
test path.

## Start the client

```sh
mkdir -p "$HOME/Library/Logs/InputLeapKeymap"

"$HOME/Applications/InputLeapKeymap/input-leapc" \
  --no-daemon \
  --disable-crypto \
  --debug DEBUG1 \
  --name ESKui-MacBookPro \
  --log "$HOME/Library/Logs/InputLeapKeymap/client.log" \
  192.168.0.10:24800
```

The Windows server configuration contains only the client name
`ESKui-MacBookPro`, without a `.local` suffix.

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

The old custom Barrier client ignored `NextGroup`. That result is not evidence
against the Input Leap client path and should not be worked around on the
Windows server.

## Input-source test

Confirm that macOS has at least two enabled input sources. With the pointer on
the Mac, tap Windows Right Alt and inspect the Mac DEBUG1 log for receipt of
key ID `0xEE08`.

If the Input Leap client receives `0xEE08` but the input source does not change,
fix the macOS implementation rather than replacing the command with F-keys,
Karabiner, or VirtualHID. The relevant path is:

```text
KeyState::fakeKeyDown(kKeyNextGroup)
  -> KeyMap::mapKey()
  -> Keystroke::kGroup
  -> OSXKeyState::fakeKey()
  -> OSXKeyState::setGroup()
```

`OSXKeyState::setGroup()` currently uses
`TISSetInputMethodKeyboardLayoutOverride()`. Verify whether the enabled Korean
and Latin input sources require selecting the actual source with
`TISSelectInputSource()` on the target macOS version. Add logs for the current
source ID, candidate source IDs, selected index, and API return status.

Do not reintroduce Karabiner, the privileged VirtualHID helper, or F16/F18/F19
translation. Commit and push the Mac-side diagnosis and fix to
`codex/input-leap-keymap`.

## Remaining keymap checks

After native input-source switching works, test in this order while the pointer
is on the Mac:

1. Tap Windows Right Alt: the macOS input source changes exactly once.
2. Hold Right Alt with another key: the configured hold modifier is emitted.
3. Press Control+C and Control+V: they behave as Command+C and Command+V.
4. Press Print Screen: macOS starts the Command+Shift+4 capture tool.

The Mac client log should remain connected without repeated enter/leave or
keep-alive failures.
