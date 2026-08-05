# Server-side key remaps

This branch can remap keyboard input on the Input Leap server before the event
is relayed to the active client screen. Rules are configured independently for
each target screen.

Remaps are configured in a `section: remaps` block. Put this section after
`section: screens`, because remap targets are validated against known screen
names.

```text
section: remaps
  mac:
    right_alt.alone = F19
    right_alt.hold = right_super
    hangul.alone = F19
    hangul.hold = right_super
    control+c = command+c
    control+v = command+v
    print_screen = command+shift+4

  windows:
    left_super = left_control
    right_super.alone = hangul
end
```

## Supported rules

### Key remap

```text
right_alt = right_super
left_super = left_control
```

A key remap changes the key ID and translates the corresponding modifier mask
while the key is held. Left and right modifier key IDs are supported for direct
key remaps. Remaps are applied once and are not chained into another rule.

### Tap-hold remap

```text
right_alt.alone = F16
right_alt.hold = right_super
```

or:

```text
right_super.alone = hangul
```

When the source key is pressed, Input Leap holds the event pending. If the key
is released without another key, Input Leap sends the `.alone` key as a tap. If
any other key arrives first, Input Leap sends the `.hold` key down and lets it act
as a modifier. If no other key arrives within 200 ms, Barrier treats the key as
a hold and sends the `.hold` key down so long modifier presses do not wait until
the next key.

If `.hold` is omitted, the source key is used as the hold key. A `.hold` rule
requires a matching `.alone` rule.

The macOS example sends a plain `F19` when Right Alt is tapped, while using
Right Super when Right Alt is held with another key.

On a Windows server using the Korean keyboard layout, Windows reports the
physical Right Alt key as `Hangul` instead of `Alt_R`. Configure both source
rules when the same Input Leap configuration may be used across keyboard layouts:

```text
right_alt.alone = F19
right_alt.hold = right_super
hangul.alone = F19
hangul.hold = right_super
```

The Windows example maps a right Super tap to `Hangul`, while keeping right
Super usable as a normal modifier when it is combined with another key.

### Modifier chord remap

```text
control+c = command+c
print_screen = command+shift+4
```

A chord rule matches an exact generic modifier mask plus a non-modifier key.
The source may also be an unmodified key when the target is a chord, as in the
Print Screen example. `control+c` does not match `control+shift+c` unless that
chord is configured separately.

For the first implementation, chord remaps emit the target key as a tap on the
source key down event, then suppress the source repeat/up events. Source
modifiers that are not part of the target mask are temporarily released around
the tap and restored immediately after it. This is intended for hotkey-style
rules such as input-source toggles.

## Key names

The parser accepts Input Leap key names such as `F19`, `Space`, `Hangul`,
`Control_L`, and `Super_R`. It also accepts these aliases:

- `left_shift`, `right_shift`
- `left_control`, `right_control`, `left_ctrl`, `right_ctrl`
- `left_alt`, `right_alt`
- `left_meta`, `right_meta`
- `left_super`, `right_super`
- `left_command`, `right_command`, `left_cmd`, `right_cmd`
- `hangul`, `print_screen`, `space`
- lowercase function keys such as `f19`

Chord modifiers use generic names: `shift`, `control`/`ctrl`, `alt`/`option`,
`meta`, `super`/`command`/`cmd`/`win`, and `altgr`.

## Current limitations

- Chord rules currently use generic modifier masks, so left/right distinction is
  available for direct key and tap-hold rules, not for `control+space` style
  chord masks.
- Chord remaps are hotkey taps, not holdable replacement keys.
- The tap-hold timeout is currently fixed at 200 ms.

## Verification

Useful local checks:

```sh
cmake -S . -B /tmp/input-leap-keymap-build \
  -DINPUTLEAP_BUILD_GUI=OFF \
  -DINPUTLEAP_BUILD_TESTS=OFF

cmake --build /tmp/input-leap-keymap-build --target input-leaps

cmake -S . -B /tmp/input-leap-keymap-tests \
  -DINPUTLEAP_BUILD_GUI=OFF \
  -DINPUTLEAP_BUILD_TESTS=ON

cmake --build /tmp/input-leap-keymap-tests --target unittests
/tmp/input-leap-keymap-tests/bin/unittests --gtest_filter=KeyRemapperTests.*
```

On macOS, a sandboxed terminal may fail after configuration parsing because
Input Leap needs Accessibility, pasteboard, and WindowServer access. The important
parse check is the `configuration read successfully` log line.
