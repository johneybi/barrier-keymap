# Windows to macOS live-test baseline

This document is the single runtime contract for the current Windows-server,
macOS-client test pair. Do not change one item in isolation.

## Fixed values

- Windows server address: `192.168.0.10:24800`
- Transport: TLS disabled on both sides
- macOS client name: `ESKui-MacBookPro`
- macOS input-source shortcut: F19 (macOS virtual key code 80)
- Expected Right Alt tap protocol key: Barrier F19 (`0xEFD0`)
- macOS key output: ordinary IOHID output; do not enable the VirtualHID path
- Client count: exactly one connection using the `ESKui-MacBookPro` name

## Required Windows server configuration

The remap block belongs in the exact configuration file loaded by the running
Windows server. `mac` in the generic example configuration is only an example;
it does not match this client.

```text
section: remaps
  ESKui-MacBookPro:
    right_alt.alone = F19
    right_alt.hold = right_super
    hangul.alone = F19
    hangul.hold = right_super
end
```

Windows Korean layouts can report the physical Right Alt key as `Hangul`, so
both source rules are required. After changing this file, reload or restart
the Windows server before testing.

## Acceptance check

With the Mac screen active, tap Right Alt once. The Mac client log must contain
one F19 down/up pair:

```text
recv key down id=0x0000efd0
recv key up id=0x0000efd0
```

Any other result has a specific owner:

- No F19 pair: Windows server runtime/configuration problem.
- F18 (`0xEFCF`): Windows configuration uses the wrong target key.
- F19 pair but no input-source switch: macOS shortcut or macOS key injection
  problem.

Do not use a real Return event (`0xEF0D`) as evidence for Right Alt. It is the
normal Enter key used when submitting a test message.
