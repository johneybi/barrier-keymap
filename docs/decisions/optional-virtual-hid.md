# Decision: keep the macOS virtual HID path optional

## Context

macOS input injection and authorization differ from the other supported
platforms. A virtual HID helper can improve compatibility for some keyboard
paths, but it adds permissions, lifecycle, and packaging complexity.

## Decision

Treat the virtual HID keyboard path as an optional platform capability rather
than making it a prerequisite for every macOS client.

## Consequences

- The ordinary client path remains usable without the helper.
- The helper needs separate authorization, lifecycle, and packaging checks.
- Changes to this path should be verified against the investigation notes in
  [the macOS virtual HID document](../../doc/macos-virtual-hid-keyboard.md).
