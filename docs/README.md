# KeyStitch documentation

This directory explains the project as a product, an input pipeline, and a
maintained open-source codebase.

## User guides

- [Key remaps](../doc/key-remaps.md)
- [Release checklist](../doc/release-checklist.md)

## Architecture

- [Input semantics overview](architecture/input-semantics.md)

## Decisions

- [Why remapping is server-side](decisions/server-side-remapping.md)
- [Windows Korean Right Alt and Hangul](decisions/windows-korean-right-alt.md)
- [Why virtual HID is optional](decisions/optional-virtual-hid.md)

## Testing and operations

- [Testing strategy](testing/strategy.md)
- [Windows/macOS test handoff](../doc/mac-windows-test-handoff.md)
- [macOS input investigations](../doc/macos-client-input-stutter.md)
- [macOS virtual HID notes](../doc/macos-virtual-hid-keyboard.md)

The original `doc/` paths remain stable because they are referenced by build,
release, and upstream-facing material.
