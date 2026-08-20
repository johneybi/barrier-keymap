# macOS Karabiner Virtual HID client

The macOS client can optionally publish remote keyboard events through
Karabiner's keyboard-only Virtual HID device. Mouse events remain on the
regular Input Leap path.

## Build

Set `INPUTLEAP_KARABINER_VHID_ROOT` to a checkout of
`Karabiner-DriverKit-VirtualHIDDevice` and enable the feature:

```sh
cmake -S . -B build-macos-vhid \
  -DINPUTLEAP_BUILD_GUI=ON \
  -DINPUTLEAP_USE_KARABINER_VHID=ON \
  -DINPUTLEAP_KARABINER_VHID_ROOT=/path/to/Karabiner-DriverKit-VirtualHIDDevice
cmake --build build-macos-vhid
```

The Karabiner driver must be installed. The VHID GUI package includes a
macOS launcher that starts only the small Virtual HID helper with administrator
privileges. The main Input Leap client remains in the logged-in user's session
so macOS pasteboard sharing continues to work. Regular builds are unchanged.

The launcher uses a user-owned `0600` Unix socket for the two-byte keyboard
event messages between the client and the privileged helper. It also keeps a
per-user lock so two clients with the same screen name are not started by
accident.

To package the GUI with the privileged launcher, set
`INPUTLEAP_USE_KARABINER_VHID=1` when invoking
`dist/macos/package_keymap_gui.sh`.
