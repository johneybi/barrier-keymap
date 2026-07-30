# Windows v2.4.0 remap handoff

## Branch

Use `stable/v2.4.0-server-remap`.

This branch starts at the exact Barrier v2.4.0 release tag (`3e0d758b`). Do not
use `stable/server-remap` for the baseline test.

## Scope

The branch adds:

- server-side key remapping
- `section: remaps` configuration parsing
- simple, tap-hold, and modifier chord rules
- F19 and Korean Windows Right Alt aliases
- unit tests and build portability includes

It does not change:

- `MSWindowsScreen`
- any macOS platform input or output code
- client networking or event scheduling
- mouse compression or cursor entry coordinates
- VirtualHID support
- the Barrier v2.4.0 Windows DPI manifest behavior

## Windows steps

```powershell
git fetch origin
git switch stable/v2.4.0-server-remap
git pull --ff-only
git submodule update --init --recursive
```

Build the server with the same toolchain and packaging path used for the
working Barrier v2.4.0 comparison whenever possible.

## Test order

1. Run the official Barrier v2.4.0 server and official macOS client with the
   current screen configuration. Confirm mouse movement and screen switching.
2. Replace only the Windows server binary with this branch's build.
3. Start with no `section: remaps`. Mouse and ordinary keyboard behavior must
   match the official server.
4. Add the remap section and test tap, hold, chord, screen switching,
   disconnect, and stuck modifiers.
5. Do not add cursor-warp or macOS client patches during this comparison.

The first failure determines the scope: a failure before remaps are enabled is
a build or baseline mismatch; a failure only after remaps are enabled belongs
to the server remapper path.

## 2026-07-30 Mac baseline attempt

This attempt is not a valid result for the new branch because the running
Windows server binary was not identified before testing.

The Mac used the official Barrier 2.4.0 DMG client:

```text
SHA256 53369a4579223e0f8742b897d96b6a9a6c3abc9f6ef9c4fec2779b0ef7bd5715
client ESKui-MacBookPro
server 192.168.0.10:24800
crypto disabled
```

The client connected and received one screen entry at `0,0`. Mouse button and
keyboard events arrived, but there was no usable pointer movement. The client
was stopped and the macOS cursor was restored.

Before another test, verify the exact Windows process:

```powershell
git branch --show-current
git rev-parse HEAD
Get-CimInstance Win32_Process -Filter "Name='barriers.exe'" |
    Select-Object ProcessId, ExecutablePath, CommandLine
Get-FileHash <the-running-barriers.exe> -Algorithm SHA256
```

Expected branch: `stable/v2.4.0-server-remap`.

The branch must contain remap source commit `69ee31ba`. Documentation commits
may move HEAD forward, so verify ancestry with:

```powershell
git merge-base --is-ancestor 69ee31ba HEAD
if ($LASTEXITCODE -ne 0) { throw "Wrong remap source revision" }
```

Stop every older `barriers.exe`, start the binary built from that HEAD, and
record its executable path, SHA256, and server log before asking the Mac to
reconnect.

## 2026-07-30 official-client remap test

With the identified v2.4.0-based server build, the official macOS client had
smooth mouse movement in the first run. A later run had mild stutter and one
immediate `enter -> leave -> enter` sequence at the same entry coordinate.

The user also reported that tapping Windows Right Alt looked like a Return
keypress. However, the macOS client recorded no key-down or key-up event during
that test window. This means the key was not mis-mapped by the official macOS
client in that run; it was consumed or handled before reaching the client.

For the next test:

1. Keep the pointer active on `ESKui-MacBookPro`.
2. Enable `DEBUG1` on the Windows server.
3. Tap the physical Windows Right Alt key exactly once.
4. Capture the server lines containing `key remap`, the source KeyID, target
   screen, and generated key.
5. Confirm that the active configuration contains both Korean-layout sources:

```text
ESKui-MacBookPro:
  right_alt.alone = F16
  right_alt.hold = right_super
  hangul.alone = F16
  hangul.hold = right_super
```

The official Barrier 2.4.0 macOS client maps function keys only through F16.
F17-F19 must not be used for this baseline.
