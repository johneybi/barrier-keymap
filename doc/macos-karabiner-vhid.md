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

The Karabiner driver must be installed and the client must run with
administrator privileges. The VHID GUI package includes a macOS launcher that
requests authorization when the client starts; regular builds are unchanged.

To package the GUI with the privileged launcher, set
`INPUTLEAP_USE_KARABINER_VHID=1` when invoking
`dist/macos/package_keymap_gui.sh`.
