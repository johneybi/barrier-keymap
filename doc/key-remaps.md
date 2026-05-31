# Server-side key remaps

This fork can remap keyboard input on the Barrier server before the event is
relayed to the active client screen. This keeps Barrier's normal mouse edge
switching behavior, while avoiding per-client tools such as Karabiner-Elements
or AutoHotkey for the supported remap subset.

Remaps are configured in a `section: remaps` block. Put this section after
`section: screens`, because remap targets are validated against known screen
names.

```text
section: remaps
  mac:
    right_alt = right_super
    right_super.alone = F19
    right_super.hold = right_super
    control+space = F19

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
key remaps.

### Tap-hold remap

```text
right_super.alone = F19
right_super.hold = right_super
```

or:

```text
right_super.alone = hangul
```

When the source key is pressed, Barrier holds the event pending. If the key is
released without another key, Barrier sends the `.alone` key as a tap. If any
other key arrives first, Barrier sends the `.hold` key down and lets the key act
as a modifier.

If `.hold` is omitted, the source key is used as the hold key. A `.hold` rule
requires a matching `.alone` rule.

The Windows example maps a right Super tap to `Hangul`, while keeping right
Super usable as a normal modifier when it is combined with another key.

### Modifier chord remap

```text
control+space = F19
```

A chord rule matches an exact generic modifier mask plus a non-modifier key. For
example, `control+space` does not match `control+shift+space` unless that chord
is configured separately.

For the first implementation, chord remaps emit the target key as a tap on the
source key down event, then suppress the source repeat/up events. Source
modifiers that are not part of the target mask are temporarily released around
the tap and restored immediately after it. This is intended for hotkey-style
rules such as input-source toggles.

## Key names

The parser accepts Barrier key names such as `F19`, `Space`, `Hangul`,
`Control_L`, and `Super_R`. It also accepts these aliases:

- `left_shift`, `right_shift`
- `left_control`, `right_control`, `left_ctrl`, `right_ctrl`
- `left_alt`, `right_alt`
- `left_meta`, `right_meta`
- `left_super`, `right_super`
- `left_command`, `right_command`, `left_cmd`, `right_cmd`
- `hangul`, `space`
- lowercase function keys such as `f19`

Chord modifiers use generic names: `shift`, `control`/`ctrl`, `alt`/`option`,
`meta`, `super`/`command`/`cmd`/`win`, and `altgr`.

## Current limitations

- Karabiner JSON import is not implemented.
- Chord rules currently use generic modifier masks, so left/right distinction is
  available for direct key and tap-hold rules, not for `control+space` style
  chord masks.
- Chord remaps are hotkey taps, not holdable replacement keys.
- Tap-hold timeout is not implemented yet; pending taps flush when another key
  or repeat arrives, and are cleared on screen/client reset paths.

## Verification

Useful local checks:

```sh
cmake -S . -B /tmp/barrier-keymap-build \
  -DBARRIER_BUILD_GUI=OFF \
  -DBARRIER_BUILD_TESTS=OFF \
  -DBARRIER_BUILD_INSTALLER=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build /tmp/barrier-keymap-build --target barriers

cmake -S . -B /tmp/barrier-keymap-tests \
  -DBARRIER_BUILD_GUI=OFF \
  -DBARRIER_BUILD_TESTS=ON \
  -DBARRIER_BUILD_INSTALLER=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build /tmp/barrier-keymap-tests --target unittests
/tmp/barrier-keymap-tests/bin/unittests '--gtest_filter=KeyRemapperTests.*'
```

On macOS, a sandboxed terminal may fail after configuration parsing because
Barrier needs Accessibility, pasteboard, and WindowServer access. The important
parse check is the `configuration read successfully` log line.
