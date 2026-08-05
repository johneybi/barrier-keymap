# Input Leap macOS arm64 client handoff

Use this client with the Windows server from branch
`codex/input-leap-keymap`. This test does not use Karabiner or VirtualHID.

## Download and install

```sh
gh run download 31041910240 \
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

## Keymap checks

Confirm that macOS has at least two input sources and that Control+Space is
assigned to the previous input source. Then test in this order while the
pointer is on the Mac:

1. Tap Windows Right Alt once: the macOS input source changes.
2. Hold Right Alt with another key: it behaves as Right Command.
3. Press Control+C and Control+V: they behave as Command+C and Command+V.
4. Press Print Screen: macOS starts the Command+Shift+4 capture tool.

The Windows server log should show `key remap` DEBUG1 records for each matched
rule. The Mac client log should remain connected without repeated enter/leave
or keep-alive failures.
