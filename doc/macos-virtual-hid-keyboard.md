# macOS VirtualHIDKeyboard investigation

This note tracks the plan for changing the macOS Barrier client keyboard output
from direct HID event posting to a virtual keyboard device that Karabiner can
observe and modify.

## Current Barrier key injection path

Remote key packets received by the macOS client are injected through this path:

```text
ServerProxy receives key packet
-> Client
-> Screen::keyDown / keyRepeat / keyUp
-> PlatformScreen::fakeKeyDown / fakeKeyRepeat / fakeKeyUp
-> KeyState::fakeKeyDown / fakeKeyRepeat / fakeKeyUp
-> KeyState::fakeKeys
-> OSXKeyState::fakeKey
-> OSXKeyState::postHIDVirtualKey
-> IOHIDPostEvent
```

The final injection point is:

```text
src/lib/platform/OSXKeyState.cpp
  OSXKeyState::fakeKey(const Keystroke&)
  OSXKeyState::postHIDVirtualKey(UInt8 virtualKeyCode, bool postDown)
```

`OSXKeyState::fakeKey` converts Barrier's local `KeyButton` into a macOS
virtual key code using `mapKeyButtonToVirtualKey`. It then calls
`postHIDVirtualKey`.

`postHIDVirtualKey` currently sends:

- modifier changes via `IOHIDPostEvent(..., NX_FLAGSCHANGED, ...)`
- non-modifier key down/up via `IOHIDPostEvent(..., NX_KEYDOWN/NX_KEYUP, ...)`

This is lower level than `CGEventPost`, but it is still not a physical keyboard
device. Karabiner is designed around grabbing actual HID devices, modifying
their input, and emitting the result through its own virtual keyboard. Direct
Barrier `IOHIDPostEvent` output is therefore a poor target for Karabiner device
rules.

## Why VirtualHIDKeyboard is the right boundary

The desired architecture is:

```text
Windows Barrier server
-> Barrier protocol key packet
-> macOS Barrier client
-> Barrier input virtual keyboard
-> Karabiner grabs/modifies that virtual keyboard
-> Karabiner output virtual keyboard
-> macOS apps
```

For a first proof, the smallest replacement boundary is
`OSXKeyState::postHIDVirtualKey`, not the generic `KeyState` mapping layer.
Keeping the existing `KeyMap` path preserves Barrier's current protocol
handling, modifier state tracking, layout groups, repeat handling, and
media-key fallback.

There is one caveat: the current macOS key map collapses right-handed modifier
keys onto left-handed macOS virtual key codes:

```text
kKeyShift_R   -> kVK_Shift
kKeyControl_R -> kVK_Control
kKeyAlt_R     -> kVK_Option
kKeySuper_R   -> kVK_Command
```

If Karabiner rules need to distinguish left and right modifiers from Barrier,
the production VirtualHID adapter must not rely only on the final macOS virtual
key code. It needs either:

- a custom macOS key map that keeps distinct local buttons for right modifiers,
- access to the original `KeyID` before `KeyState::fakeKeys` converts it into
  `Keystroke`, or
- client data in `KeyMap::Keystroke` that carries the original side-specific
  identity into `OSXKeyState::fakeKey`.

For a first proof, plain alphanumeric keys and generic modifiers can be tested
at `postHIDVirtualKey`. For production-grade Karabiner integration, preserving
left/right modifier identity is a separate requirement.

`KeyMap::KeyItem::m_client` already survives into
`KeyMap::Keystroke::m_data.m_button.m_client`, so the least invasive production
path is to store macOS/USB output metadata there when building the macOS key
map, then consume it from `OSXKeyState::fakeKey`. That keeps the existing
`KeyState::fakeKeyDown`, `fakeKeyRepeat`, and `fakeKeyUp` bookkeeping intact
while avoiding the right-modifier collapse that happens after
`mapKeyButtonToVirtualKey`.

## VirtualHID integration options

### Option A: Use Karabiner-DriverKit-VirtualHIDDevice client library

Karabiner-DriverKit-VirtualHIDDevice provides a virtual keyboard and mouse via
DriverKit. Its client is a header-only C++ library that sends commands to
`Karabiner-VirtualHIDDevice-Daemon` over a root-only socket.

Implications:

- Karabiner's virtual HID driver package must be installed and activated.
- `Karabiner-VirtualHIDDevice-Daemon` must be running.
- Barrier must run with root privileges, or a privileged helper must bridge
  Barrier's user session to the daemon.
- This avoids building and signing a new DriverKit extension for Barrier.
- The client API exposes `virtual_hid_keyboard_parameters`, including
  `vendor_id`, `product_id`, and `country_code`.
- The DriverKit keyboard device description applies those values as
  `kIOHIDVendorIDKey`, `kIOHIDProductIDKey`, and `kIOHIDCountryCodeKey`.

This is the lowest-risk implementation path. It supports a custom
vendor/product identity without Barrier shipping its own DriverKit extension.

### Option B: Build a Barrier-owned virtual HID device

Barrier could create its own DriverKit virtual keyboard with a custom
vendor/product ID.

Implications:

- Requires Apple DriverKit and virtual HID entitlements.
- Requires system extension packaging, signing, user approval, and notarization.
- Much higher maintenance and distribution burden.
- Gives full control over device identity.

This is only worth considering if Karabiner's existing virtual HID daemon cannot
expose a separate Barrier input keyboard identity.

## Device identity strategy

The Barrier input keyboard should have a stable identity that users can target
in Karabiner:

```text
vendor_id: 0x1209 or another explicit project/vendor value
product_id: 0x4b42 or another Barrier-specific keyboard product ID
product: "Barrier Input Virtual Keyboard"
manufacturer: "Barrier Keymap"
is_keyboard: true
```

Karabiner device conditions can match by `vendor_id`, `product_id`, and
`is_keyboard`. Users should verify the actual values in Karabiner-EventViewer's
Devices tab after the virtual device appears.

Example condition shape:

```json
{
  "type": "device_if",
  "identifiers": [
    {
      "vendor_id": 4617,
      "product_id": 19266,
      "is_keyboard": true
    }
  ]
}
```

The numeric values above are the current probe identity. They are provisional,
but they must stay different from Karabiner's own output virtual keyboard.

## Loop prevention

The Barrier input virtual keyboard and Karabiner output virtual keyboard must be
distinct devices.

Karabiner rules should:

- enable modifications for the Barrier input virtual keyboard
- avoid matching Karabiner's own output virtual keyboard
- use `device_if` for Barrier input rules
- use `device_unless` or device-level settings to exclude Karabiner output

If Barrier uses Karabiner's same virtual keyboard as output, a loop is likely:

```text
Barrier sends virtual key
-> Karabiner modifies it
-> Karabiner emits virtual key
-> Karabiner sees its own output again
```

So the critical implementation requirement is a separate input device identity,
not merely "send through Karabiner's existing output keyboard."

## Implementation sketch

1. Add a macOS-only keyboard output abstraction:

```text
src/lib/platform/OSXKeyboardOutput.h
src/lib/platform/OSXKeyboardOutput.cpp
src/lib/platform/OSXIOHIDKeyboardOutput.*
src/lib/platform/OSXVirtualHIDKeyboardOutput.*
```

2. Keep `OSXKeyState::fakeKey` as the mapping boundary, but replace the direct
call to `postHIDVirtualKey` with an output object. For the early proof this can
accept macOS virtual key codes:

```text
OSXKeyState::fakeKey
-> mapKeyButtonToVirtualKey
-> keyboardOutput->postVirtualKey(virtualKey, keyDown)
```

For the production path, prefer a richer call that also passes the Barrier
client data carried by the keystroke:

```text
OSXKeyState::fakeKey
-> button, keyDown, repeat, clientData
-> keyboardOutput->postKey(button, keyDown, repeat, clientData)
```

3. Preserve the existing `IOHIDPostEvent` output as the default/fallback.

4. Add a config or command-line switch for the virtual HID output while it is
experimental:

```text
--mac-key-output iohid
--mac-key-output karabiner-virtual-hid
```

5. Implement the VirtualHID output as a separate macOS-only target that can be
disabled at CMake time if Karabiner headers are not available.

6. Add logging that prints:

```text
virtual HID daemon connection status
virtual keyboard vendor/product ID
whether output is IOHID or VirtualHID
```

7. The VirtualHID output is report-state based. Its adapter must keep:

```text
pressed modifier usages
pressed non-modifier usages
last posted report
daemon connection/ready state
```

Each key down/up updates the pressed sets and posts one complete
`keyboard_input` report. On disconnect, screen reset, or `fakeAllKeysUp`, it
must post an empty report to avoid stuck keys.

## Confirmed Karabiner VirtualHID facts

The upstream `Karabiner-DriverKit-VirtualHIDDevice` client example uses:

```cpp
pqrs::dispatcher::extra::initialize_shared_dispatcher();

auto client =
    std::make_unique<
        pqrs::karabiner::driverkit::virtual_hid_device_service::client>();

client->connected.connect([&client] {
  pqrs::karabiner::driverkit::virtual_hid_device_service::
      virtual_hid_keyboard_parameters parameters;

  parameters.set_vendor_id(...);
  parameters.set_product_id(...);
  parameters.set_country_code(pqrs::hid::country_code::us);

  client->async_virtual_hid_keyboard_initialize(parameters);
});

client->virtual_hid_keyboard_ready.connect([&client](auto&& ready) {
  if (ready) {
    pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::
        keyboard_input report;

    report.modifiers.insert(...);
    report.keys.insert(type_safe::get(
        pqrs::hid::usage::keyboard_or_keypad::keyboard_e));

    client->async_post_report(report);
  }
});

client->async_start();
```

The DriverKit keyboard implementation uses the supplied IDs when constructing
the HID device:

```text
kIOHIDVendorIDKey  <- provider->getKeyboardVendorId()
kIOHIDProductIDKey <- provider->getKeyboardProductId()
kIOHIDCountryCodeKey <- provider->getKeyboardCountryCode()
```

The installed machine already has the required Karabiner components:

```text
org.pqrs.Karabiner-DriverKit-VirtualHIDDevice.dext
Karabiner-VirtualHIDDevice-Daemon
Karabiner-Elements
Karabiner-EventViewer
```

The example client can be compiled directly with the local command-line tools:

```sh
clang++ -std=c++23 \
  -I/tmp/Karabiner-DriverKit-VirtualHIDDevice/vendor/vendor/include \
  -I/tmp/Karabiner-DriverKit-VirtualHIDDevice/include \
  /tmp/Karabiner-DriverKit-VirtualHIDDevice/examples/virtual-hid-device-service-client/src/main.cpp \
  -o /tmp/virtual-hid-device-service-client
```

This repository also contains a safer probe that does not post key events by
default. It only initializes a keyboard with Barrier-specific IDs and waits:

```sh
tools/macos-virtual-hid/build_probe.sh
sudo /tmp/barrier_virtual_hid_keyboard_probe
```

To post a single test key after the virtual keyboard is ready:

```sh
sudo /tmp/barrier_virtual_hid_keyboard_probe --send-test-key
```

For a fuller local verification flow, run:

```sh
tools/macos-virtual-hid/verify_probe_device.sh
```

The script builds the probe, ensures the Karabiner profile has the Barrier input
keyboard `ignore: false` device entry, starts the probe with sudo, then checks
`karabiner_cli --list-connected-devices` for the Barrier input keyboard.

It writes a machine-readable result to:

```text
/tmp/barrier-vhid-verification-status.json
```

To leave the probe running after a successful check, use:

```sh
tools/macos-virtual-hid/verify_probe_device.sh --keep-running
```

This is useful when verifying Karabiner-EventViewer Devices manually or when
testing whether a `device_if` rule applies to the Barrier input keyboard.

The probe currently uses this provisional identity:

```text
vendor_id: 0x1209 (4617)
product_id: 0x4b42 (19266)
```

The client connects to root-only local datagram sockets under:

```text
/Library/Application Support/org.pqrs/tmp/rootonly/vhidd_server
/Library/Application Support/org.pqrs/tmp/rootonly/vhidd_response
/Library/Application Support/org.pqrs/tmp/rootonly/vhidd_client
```

So a normal user process cannot assume it can directly access the daemon. The
practical choices are:

- run the Barrier client as root for early experiments,
- add a small privileged helper that owns the VirtualHID connection, or
- integrate through a service installed with proper privileges.

This was confirmed locally: running the compiled example as the normal user
failed before connection with:

```text
filesystem_error: posix_stat Permission denied
["/Library/Application Support/org.pqrs/tmp/rootonly/vhidd_server"]
```

The local probe now performs this preflight and prints:

```text
cannot access Karabiner VirtualHID root-only socket: ...
run this probe with sudo.
```

Keyboard output is report-state based, not individual event based. A
`keyboard_input` report contains:

```text
report_id = 1
modifiers = left/right modifier bitset
keys = up to 32 USB HID keyboard usages
```

Thus the Barrier adapter needs to track the currently pressed USB HID usages and
send a full report after each key down/up. It cannot simply forward each
`postHIDVirtualKey(virtualKey, down)` call without maintaining state.

Karabiner's profile device entries use `ignore: false` for devices whose events
are modified. A Barrier virtual keyboard entry should look like:

```json
{
  "identifiers": {
    "is_keyboard": true,
    "is_pointing_device": false,
    "vendor_id": 4617,
    "product_id": 19266
  },
  "ignore": false,
  "simple_modifications": []
}
```

The helper script can add or update that entry for the selected Karabiner
profile:

```sh
tools/macos-virtual-hid/enable_karabiner_modify_events.py
```

It creates a timestamped backup by default, sets `ignore: false` for the Barrier
input virtual keyboard, and leaves Karabiner's own output virtual keyboard
unchanged. It also prints ready-to-copy `device_if` and `device_unless`
conditions for complex modification rules.

On the current machine, Karabiner's own output virtual keyboard is visible via:

```sh
"/Library/Application Support/org.pqrs/Karabiner-Elements/bin/karabiner_cli" \
  --list-connected-devices
```

Relevant current output:

```json
{
  "device_identifiers": {
    "is_keyboard": true,
    "is_virtual_device": true,
    "product_id": 591,
    "vendor_id": 1452
  },
  "manufacturer": "pqrs.org",
  "product": "Karabiner DriverKit VirtualHIDKeyboard 1.8.0",
  "serial_number": "pqrs.org:Karabiner-DriverKit-VirtualHIDKeyboard"
}
```

Therefore the Barrier input virtual keyboard must use a different identifier,
such as the probe's provisional `4617:19266`, and Karabiner rules must target
only the Barrier input device.

## Open questions

- What privilege model is acceptable for the Barrier client: run as root,
  privileged helper, or user process plus installed daemon?
- Which key-code representation should be passed to the virtual keyboard:
  macOS virtual key codes, USB HID usage IDs, or Karabiner `pqrs::hid` usage
  values?
- How should Barrier preserve left/right modifier identity on macOS before the
  current key map collapses those modifiers?
- How should media keys be routed, since current Barrier handles them through a
  separate `fakeMediaKey` path?

## Immediate verification checklist

1. Run `tools/macos-virtual-hid/verify_probe_device.sh` from a terminal.
2. Confirm a non-Karabiner input virtual keyboard appears in
   Karabiner-EventViewer Devices.
3. Confirm Karabiner can apply a `device_if` rule to that device.
4. Confirm Karabiner output keyboard remains a separate device.
5. Only after this proof should Barrier code be changed to depend on
   VirtualHIDKeyboard output.
