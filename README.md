# KeyStitch

```text
⌘  ───────  KeyStitch  ───────  Ctrl
```

**Seamless input across machines.**

> A Barrier-based cross-platform input project that preserves natural keyboard
> behavior across Windows, macOS, and Linux.

[![CI](https://github.com/johneybi/keystitch/actions/workflows/keymap-ci.yml/badge.svg)](https://github.com/johneybi/keystitch/actions/workflows/keymap-ci.yml)
[![Releases](https://img.shields.io/github/v/release/johneybi/keystitch?include_prereleases&label=release)](https://github.com/johneybi/keystitch/releases)

Barrier lets one keyboard and mouse control multiple computers. KeyStitch keeps
the keyboard's meaning intact while the pointer moves between them.

## The problem

The pointer can cross an OS boundary naturally. Keyboard semantics often cannot:

- `Ctrl` and `Command` are not interchangeable
- `Alt` and `Option` have different meanings
- left and right modifiers can arrive as different events
- Korean Windows layouts may report Right Alt as `Hangul`
- input-source switching depends on the receiving platform

Tools such as Karabiner-Elements and AutoHotkey are useful on local machines,
but remote Barrier input may not reach them as ordinary local keyboard input.

## The insight

> Instead of fixing the key after it reaches the destination OS, translate its
> meaning before Barrier sends it.

## The solution

KeyStitch adds a target-screen-aware input semantics layer to Barrier. The
existing mouse-edge switching experience stays intact, while configured
keyboard rules are translated for the destination machine.

```text
Physical keyboard / mouse
          │
          ▼
    Barrier server
          │
          ▼
   ┌────────────────┐
   │   KeyStitch    │
   │  Remap         │
   │  Tap / Hold    │
   │  Modifiers     │
   │  Input source  │
   └────────────────┘
          │
    Barrier protocol
          │
     ┌────┴────┐
     ▼         ▼
  Windows    macOS
```

## What KeyStitch adds

- **Per-screen remapping** — rules can target a specific remote machine
- **Tap / hold modifiers** — one key can have separate tap and hold behavior
- **Cross-OS modifier translation** — map Windows and macOS conventions
- **Hotkey chord remapping** — translate combinations such as `Control+Space`
- **Korean Hangul key handling** — distinguish `Right Alt` and `Hangul`
- **Extended macOS function keys** — support platform-specific key paths
- **Keyboard + mouse sharing** — the proven Barrier workflow remains intact

The remapping layer only affects configured keyboard rules on configured
screens. Barrier's normal mouse and clipboard behavior remains available.

## Quick example

Remaps are configured in a `section: remaps` block and can be scoped per target
screen:

```text
section: remaps
  mac:
    right_alt.alone = F19
    right_alt.hold = right_super
    hangul.alone = F19
    hangul.hold = right_super
    control+space = F19

  windows:
    left_super = left_control
    right_super.alone = hangul
end
```

See [the key remap guide](doc/key-remaps.md) for the supported configuration
syntax, examples, limitations, and verification commands.

## Downloads

Download Linux and macOS builds from the
[releases page](https://github.com/johneybi/keystitch/releases). Releases
include `.sha256` files for verifying downloaded archives.

## Documentation

- [Key remaps](doc/key-remaps.md)
- [Windows/macOS test handoff](doc/mac-windows-test-handoff.md)
- [macOS virtual HID keyboard](doc/macos-virtual-hid-keyboard.md)
- [macOS client input stutter](doc/macos-client-input-stutter.md)
- [Release checklist](doc/release-checklist.md)

The documentation records both user-facing behavior and the investigation
behind platform-specific input decisions.

## Build and test

The project uses CMake. The repository contains platform build scripts and
GitHub Actions workflows for CI and release packaging. For the manual release
and verification flow, see the [release checklist](doc/release-checklist.md).

## Relationship to Barrier

KeyStitch is an unofficial modified build based on Barrier. It is not
affiliated with or endorsed by the upstream Barrier maintainers.

The original Barrier project and its package ecosystem are available at
<https://github.com/debauchee/barrier>.

KeyStitch aims to contribute useful fixes and documentation upstream where
appropriate, while providing a focused build for cross-platform keyboard
semantics.

## Support and contributions

For KeyStitch remapping, packaging, or release issues, open an issue in this
repository:
<https://github.com/johneybi/keystitch/issues>.

For bugs that also reproduce in upstream Barrier without a `section: remaps`
configuration, report them to the
[upstream Barrier project](https://github.com/debauchee/barrier).

Contributions are welcome, especially around remap rules, platform key-code
coverage, tests, and release packaging.

## License

KeyStitch follows the licensing terms of the Barrier source tree. See
[LICENSE](LICENSE) for details.
