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

## Why a separate input VirtualHIDKeyboard is the right boundary

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

This path is useful as a reliable macOS HID output and as a transport prototype,
but it is not a Karabiner input path. Karabiner identifies every device whose
manufacturer is `pqrs.org` and whose product starts with
`Karabiner DriverKit VirtualHIDKeyboard` as its own virtual device. That test
does not use the configured vendor/product IDs. Karabiner observes such devices
for Caps Lock LED state, never seizes them, and does not place their ordinary
key events in the manipulation queue.

The current helper therefore proves that Barrier can deliver complete keyboard
reports through a privileged process, but its DriverKit backend must be replaced
before Karabiner rules can transform those reports.

### Option B: Build a Barrier-owned virtual HID device

Barrier could create its own DriverKit virtual keyboard with a custom
vendor/product ID.

Implications:

- Requires Apple DriverKit and virtual HID entitlements.
- Requires system extension packaging, signing, user approval, and notarization.
- Much higher maintenance and distribution burden.
- Gives full control over device identity.

The existing Karabiner virtual HID daemon cannot expose a separate input
keyboard identity: the driver fixes the manufacturer and product strings used
by Karabiner's own-device check. A Barrier-owned input device, or another
independent virtual-input implementation, is therefore required for the
Karabiner integration goal.

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

Karabiner prevents a loop from its own output keyboard by classifying it as a
virtual device and excluding its ordinary events from manipulation:

```text
Barrier sends report to Karabiner's output virtual keyboard
-> macOS applications receive it
-> Karabiner observes the device but does not manipulate the key
```

So the critical implementation requirement is a genuinely separate input
device implementation, not merely different vendor/product parameters on
Karabiner's existing output keyboard.

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

## Experimental Barrier integration

The repository now has an experimental macOS client integration behind a CMake
option:

```sh
cmake -S . -B /tmp/barrier-keymap-vhid-build \
  -DBARRIER_ENABLE_MAC_VIRTUAL_HID=ON \
  -DBARRIER_BUILD_GUI=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build /tmp/barrier-keymap-vhid-build --target barrierc
```

Build and start the privileged helper for the logged-in user:

```sh
tools/macos-virtual-hid/build_helper.sh
sudo /tmp/barrier_virtual_hid_helper --uid "$(id -u)"
```

The helper creates `/var/run/barrier-keymap-vhid-<uid>.sock`, assigns it to the
requested user with mode `0600`, and verifies the peer UID for every accepted
connection. It waits for Karabiner VirtualHID to become ready before exposing
the socket.

At runtime, the existing IOHID path remains the default. Start `barrierc` as
the normal logged-in user, not with `sudo`:

```sh
BARRIER_MAC_KEY_OUTPUT=virtual-hid \
  /tmp/barrier-keymap-vhid-build/bin/barrierc ...
```

The current implementation:

- sends complete keyboard reports to the privileged helper
- the helper initializes a Karabiner VirtualHID keyboard with
  `vendor_id=0x1209` and `product_id=0x4b42`
- keeps a pressed modifier set and pressed key set
- posts complete `keyboard_input` reports after each key transition
- falls back to the original `IOHIDPostEvent` path if VirtualHID is not ready
  or if a key is not yet mapped to a USB HID usage
- keeps the original IOHID behavior unless `BARRIER_MAC_KEY_OUTPUT=virtual-hid`
  is set

Current limitations:

- the helper is an experimental command-line process, not an installed
  launchd service yet
- right modifiers still collapse to left modifiers because the current macOS
  key map maps both sides to the same Carbon virtual key before output
- media keys still use the existing separate `fakeMediaKey` path
- this is intended for command-line `barrierc` testing before GUI packaging

### Root client test result

Running the complete Barrier client as root is not a viable privilege model.
On 2026-07-30, the v2.4.0 VirtualHID build reached:

```text
macOS key output: Karabiner VirtualHID keyboard
Barrier input VirtualHID keyboard ready=true
```

but the root process saw a different display topology than the logged-in user
and then terminated with:

```text
event queue is not ready within 5 sec
```

The production path must therefore keep `barrierc` in the logged-in user
session and move only the Karabiner VirtualHID service connection into a small
privileged helper. The helper must accept full keyboard reports from the local
Barrier client, validate the connecting user, post them to the root-only
Karabiner service, and send an empty report when either side disconnects.

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

The client uses the root-only Unix domain service socket returned by the
installed VirtualHID client headers. With the current headers, it is:

```text
/Library/Application Support/org.pqrs/tmp/rootonly/karabiner_virtual_hid_device_service.sock
```

So a normal user process cannot assume it can directly access the daemon. The
practical choices are:

- run the Barrier client as root for early experiments,
- add a small privileged helper that owns the VirtualHID connection, or
- integrate through a service installed with proper privileges.

Running the compiled client as a normal user fails the root-only socket
preflight. The integration uses the official
`constants::get_server_socket_file_path()` API instead of hardcoding a
version-specific path.

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

## 2026-07-31 coordinated connection result

Windows ran the v141 remap server with SSL disabled and the requested F16
rules. After the handshake keep-alive fix, one Mac connection reached:

```text
client "ESKui-MacBookPro" has connected
```

However, additional TCP connections using the same client name arrived before
the first connection closed. The server reported:

```text
a client with name "ESKui-MacBookPro" is already connected
```

and two Mac TCP sessions were simultaneously established. This prevents a
valid key test and triggers a Windows v141 disconnect-path crash.

Before reconnecting, stop every user-session `barrierc` process and any wrapper
that automatically starts another copy. Keep the privileged VirtualHID helper
running, then start exactly one `barrierc` process from commit `6867d3dc` or
later. Verify locally that only one client process exists before allowing it to
connect to `192.168.0.10:24800`.

### Mac cleanup result

The Mac found three stale `barrierc` processes: two user-session clients and
one earlier root daemon. All three were stopped while the privileged
VirtualHID helper remained running. Exactly one client was then started from
commit `607c5a86`.

The remaining client currently reaches `192.168.0.10:24800` as a single
`SYN_SENT` connection, but the TCP attempt times out after 15 seconds. The
Windows server should be restarted after its duplicate-client disconnect
crash; the Mac client will continue retrying automatically.

## 2026-07-31 single-client v141 retest

After the duplicate clients were removed, one Mac client connected to the
Windows v141 server and completed the Barrier 1.6 handshake. The connection
remained established, but mouse movement was again severely stuttered.

During the same run, the Mac received repeated screen transitions, including:

```text
[2026-07-31T00:52:28] DEBUG1: recv leave
[2026-07-31T00:52:28] INFO: leaving screen
[2026-07-31T00:52:31] DEBUG1: recv enter, 0,657 15 0000
[2026-07-31T00:52:33] DEBUG1: recv leave
```

These transitions remain relevant to the mouse-stutter investigation, but
they must not be attributed to the Right Alt test described below.

The privileged VirtualHID helper also accepted and then lost the Barrier Unix
socket connection during this run. That is a separate Mac keyboard-output
issue and cannot explain the repeated screen enter/leave events.

### Invalid Right Alt attribution

The user later confirmed that the keyboard used for the Right Alt tests was
accidentally connected directly to the Mac, not to the Windows Barrier server.
The Karabiner-EventViewer sequence:

```text
right_option down
right_option + return_or_enter down
return_or_enter up
right_option up
```

EventViewer attributed these post-modification events to
`Karabiner DriverKit VirtualHIDKeyboard 1.8.0`. The active Karabiner profile
contains a Korean-input rule described as `오른쪽 option 키를 사용하여 한글을
한자로 변환`, which maps `right_option` to `right_option + return_or_enter`.

This correctly explains the local newline and Hanja candidate popover, but it
provides no evidence about Windows Right Alt, Barrier remapping, F16 output, or
the VirtualHID bridge. Discard all remote-key conclusions from this capture.
Repeat the test only after verifying that the physical keyboard is attached to
Windows and that the Mac has no directly connected test keyboard.

### Valid remote Right Alt capture

The isolated test was repeated with the physical keyboard confirmed on the
Windows server. At `01:13:16`, the Mac client received:

```text
recv key down id=0x0000efcd, mask=0x0000, button=0x0138
recv key up id=0x0000efcd, mask=0x0000, button=0x0138
```

`0xEFCD` is Barrier F16. The macOS key map then produced virtual key `0x6a`,
also F16. This proves the Windows Right Alt/Hangul remap and Barrier protocol
delivery are working for this rule.

The client failed at the next boundary:

```text
VirtualHID helper connection failed; using IOHID
```

The privileged helper process was still present, but its Unix socket peer had
closed before the first keyboard report was sent. The client therefore fell
back to IOHID, which does not prove the required Karabiner input path. The next
Mac task is to restart the helper from the current build, capture helper-side
connection logs, and make the bridge recover from a closed or stale helper
connection before repeating the F16 test in Karabiner-EventViewer.

### VirtualHID connection recovery

The Mac client now keeps the opt-in VirtualHID output object alive when the
helper is temporarily unavailable. A failed report send records the exact
`errno`, reconnects once, and retries the complete keyboard-state report. If
that retry also fails, the affected key's down/up lifetime remains on IOHID so
one physical key cannot be split across two output devices. Reconnection is
attempted again after the fallback input state is fully released.

The live stale-socket reproduction reached:

```text
VirtualHID helper report send failed on attempt 1:
Broken pipe (errno=32, offset=0/76)
reconnected to privileged Karabiner VirtualHID helper
VirtualHID helper report recovered after reconnect
```

The helper now logs peer UID/GID, connection IDs, report counts, partial
reports, EOF, and protocol fields for invalid messages. It exposes its Unix
socket only while Karabiner reports the Barrier keyboard ready. If the
Karabiner service connection closes, the helper shuts down the active Barrier
socket, removes the listener, recreates its Karabiner client, and publishes a
new listener only after the virtual keyboard is ready again. This prevents a
live but ineffective helper from silently discarding reports after sleep,
console-user changes, or Karabiner service restarts.

`KeyboardReport` is fixed at 76 bytes for protocol version 1. A compile-time
size assertion requires a protocol-version change if its wire layout changes.
Unit tests cover valid initialization, version mismatch, and key-count bounds.

The first run with the current helper identified the original immediate
disconnect: on macOS, the accepted Unix socket retained `O_NONBLOCK` from the
listener. The helper processed the F16 press and release as two complete
reports, then treated the next `recv` result, `EAGAIN` (`errno=35`), as a
disconnect. Each accepted client socket is now explicitly changed back to
blocking mode before reading reports. The listener remains non-blocking so the
helper can continue monitoring shutdown and Karabiner reconnect state.

### Confirmed Karabiner output-device limitation

Karabiner-Elements v16.1.8 source at commit
`f936a32f838de6af29ebaa0a05420842d9d950e9` confirms the live `observed`
result:

- `src/share/iokit_utility.hpp` classifies the `pqrs.org` DriverKit keyboard as
  a Karabiner virtual device by manufacturer and product-name prefix.
- `device_grabber_details/entry.hpp` makes virtual devices observable but
  explicitly returns false from `needs_to_seize_device`.
- `device_grabber.hpp` handles only Caps Lock state from virtual-device input;
  ordinary input reaches the manipulation queue only in the non-virtual branch.

Changing the helper's vendor/product parameters cannot change this behavior.
The next macOS implementation must replace only the helper's report-output
backend with an independent virtual input keyboard. The Barrier protocol,
keyboard-state report, reconnect logic, peer checks, IOHID fallback, and
Windows function-key remap can all remain in place.

### IOHIDUserDevice feasibility result

`tools/macos-virtual-hid/barrier_iohid_user_device_probe.cpp` is a minimal
independent keyboard proof. It uses manufacturer `Barrier Keymap`, product
`Barrier Input Virtual Keyboard`, identity `0x1209:0x4b42`, and sends F19.

The local SDK and execution test confirm that `IOHIDUserDevice` is not an
unsigned local-development shortcut:

- the API requires `com.apple.developer.hid.virtual.device`;
- the unsigned probe builds but device creation returns null;
- adding that entitlement with an ad-hoc signature causes macOS to terminate
  the process because the entitlement is not provisioned;
- administrator privileges do not replace code-signing entitlement validation.

Build the probe with:

```sh
tools/macos-virtual-hid/build_iohid_user_device_probe.sh
```

`BARRIER_SIGN_IDENTITY` may be set only when the selected Apple signing identity
and provisioning profile are authorized for the virtual-device entitlement.

This leaves two honest product paths:

1. Ship a Barrier-owned DriverKit virtual input keyboard. This preserves the
   full Karabiner integration goal, but requires Apple entitlement approval,
   signing, system-extension installation, notarization, and release packaging.
2. Keep the current Karabiner DriverKit backend as optional direct macOS HID
   output and implement the required mappings in Barrier. This is easier to
   distribute, but those events do not pass through Karabiner manipulators.

The transport and recovery work completed so far applies to both paths.

## Decision: implement mappings in the macOS Barrier client

The user selected product path 2 for the fastest usable result. The macOS side
owns this implementation because it must build, run, and validate the
`OSXKeyState` injection path against the active macOS input sources.

Keep the verified Windows contract unchanged:

```text
Windows Right Alt / Hangul tap
-> server remapper F16
-> macOS Barrier client
```

Do not replace F16 with F18 or F19. The first macOS client mapping must consume
an F16 tap and emit a balanced `left_control + spacebar` press/release sequence
through the existing direct macOS HID output path. Suppress the original F16
from applications. The server already resolves a held Right Alt / Hangul key
to `right_super`, so the client must not turn that hold path into an input
source toggle.

Implementation requirements:

- perform the mapping before the final macOS HID report is emitted;
- keep key-down and key-up state balanced so Control cannot remain stuck;
- produce exactly one input-source toggle per F16 tap;
- retain the existing VirtualHID reconnect and IOHID fallback behavior;
- add a low-volume INFO diagnostic for the transformed gesture;
- add focused tests for tap, repeated taps, unrelated F16 state, and cleanup
  after output failure or disconnect.

Acceptance test:

1. Right Alt tap toggles Korean/English exactly once.
2. Right Alt hold does not toggle and retains the configured `right_super`
   behavior.
3. Control is not stuck after repeated toggles or reconnects.
4. Pointer movement and return to Windows remain smooth.

The Windows test on 2026-07-31 used server commit `02a4e3f8`, SSL disabled,
and INFO logging. The Mac client connected successfully and pointer movement
was smooth. `TCP_NODELAY` verified as enabled, with up to 823 actual socket
writes per second and only 12-89 bytes of queued output. DEBUG1 must remain
disabled for usability testing because its event-volume logging was the
remaining cause of severe pointer stutter.

## Mac response: align the live contract before adding a client transform

The Mac side had completed work that was not yet visible to the Windows side
when the preceding decision was written:

- macOS input-source switching is now configured to F19 in System Settings;
- a direct F19 test switches between Korean and ABC successfully;
- current `master` maps Barrier F17 through F19 to the corresponding macOS
  virtual key codes;
- the privileged VirtualHID helper was stopped and removed from the live path
  after it competed with Karabiner Core Service and caused local keyboard
  timeouts and resets;
- the smooth-pointer test used ordinary IOHID output, without
  `BARRIER_MAC_KEY_OUTPUT=virtual-hid`.

The full Mac client log also corrects a mistaken report in
`doc/mac-windows-test-handoff.md`: Windows Right Alt was repeatedly received
as F16 (`0xEFCD`, button `0x0138`) and mapped to macOS F16 (`0x6a`). The
observed Return event was the real Enter key used to submit a message, not
Right Alt.

Do not implement the proposed F16-to-Control+Space client state machine yet.
The next test should use the smallest existing path:

```text
Windows Right Alt / Hangul tap
-> server remapper F19
-> current master macOS client
-> direct macOS F19 input-source shortcut
```

This experiment requires changing only the Windows tap output from F16 to F19.
If it succeeds, no client-side Control+Space transform is needed for the basic
input-source toggle. It does not solve the broader goal of passing arbitrary
remote keys through Karabiner; that remains a separate product path requiring
an independent virtual input device or its entitlement-backed equivalent.

Karabiner-free operation is a useful secondary goal, not the sole product
goal. Keep the Barrier-native server remapper, while preserving a future
Karabiner-compatible path where technically possible.

## Confirmed live result: F19 input-source toggle

The coordinated Windows-to-Mac test on 2026-07-31 succeeded with:

```text
Windows Right Alt / Hangul tap
-> server remapper F19
-> macOS Barrier client direct IOHID output
-> macOS F19 input-source shortcut
-> Korean/English input source toggled exactly once
```

The Windows server used commit `02a4e3f8`, SSL disabled, INFO logging, and the
following destination remap:

```text
right_alt.alone = F19
right_alt.hold = right_super
hangul.alone = F19
hangul.hold = right_super
```

The pointer remained smooth and could return to Windows. This is now the
baseline contract for the basic Right Alt input-source feature. Do not revert
the tap output to F16, add the proposed F16-to-Control+Space client transform,
or enable the privileged VirtualHID helper for this path.
