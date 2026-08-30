# KeyStitch

```text
⌘  ——  KeyStitch  ——  Ctrl
```

**Seamless input across machines.**

KeyStitch is a Barrier-based cross-platform input project that keeps keyboard
behavior consistent across Windows, macOS, and Linux. It preserves Barrier's
familiar mouse-edge switching while translating OS-specific keyboard meaning
before remote input reaches the target machine.

## Why KeyStitch?

Barrier makes the pointer move naturally between computers, but keyboard
behavior does not always travel with it. `Ctrl`, `Command`, `Alt`, `Option`,
`Hangul`, input-source switching, and left/right modifier keys can mean
different things on different operating systems.

KeyStitch addresses that seam in the input path:

```text
Keyboard / mouse
       ↓
Barrier server
       ↓
KeyStitch input semantics
       ↓
Barrier protocol
       ↓
Windows / macOS / Linux
```

## What it adds

- Per-screen key remapping
- Tap/hold behavior
- Modifier-chord translation
- Left/right modifier handling
- Korean Windows `Right Alt` / `Hangul` handling
- macOS extended-key support
- Cross-platform build and release packaging

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
- [Windows/macOS live baseline](doc/live-windows-mac-baseline.md)
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
