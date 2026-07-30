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

## 2026-07-30 v141 server Mac result

The Mac connected to the requested v141 server with the official Barrier 2.4.0
client. Mouse movement was usable but mild stutter remained. At the first
entry, the client received three immediate `leave -> enter` pairs before the
screen remained active.

The Right Alt failure is now captured precisely. At `17:14:42`, the Mac client
received:

```text
KeyID: 0xEF0D
button: 0x001C
macOS virtual key: 0x24
```

`0xEF0D` is Barrier `Return`, and macOS virtual key `0x24` is Return. The
expected Barrier F16 KeyID is `0xEFCD`. The macOS client therefore performed
exactly the event sent by the Windows server; this is not an F16 mapping error
on the Mac.

Capture the corresponding Windows `DEBUG1` lines:

```text
onKeyDown screen=... key=... id=... button=...
key remap pending tap ...
key remap tap ... ->F16 ...
onKeyUp screen=... key=... id=... button=...
```

Do not add a `Return -> F16` rule, because that would break the real Enter key.
The raw Windows KeyID and remapper output must be identified first.

## 2026-07-30 Windows official-server A/B result

The Windows side repeated the no-remap test with the official Barrier 2.4.0
Windows release:

```text
installer SHA256:
  7E66B7B4D13312E607EDD06F8EA38F3C9B09B3E8AEA2B55250C00B25F9892885
barriers.exe SHA256:
  C8A7C7D5CD839023FD482D069F940B2FEF6350C88E3C3DC356F294EAD328ED58
```

The official Windows server discarded 26 bogus delta motions during the test.
The identified custom v2.4.0 remap build discarded 9,660 with remaps disabled.
The official run also confirms the Mac report: the first interval had repeated
edge crossings, while the later interval had only one immediate re-entry and
then remained usable on the Mac.

This comparison moves the primary fault away from the macOS client and the
remap rules. The current suspect is the Windows custom build output or its
toolchain. The official v2.4.0 Windows release was built by the release-era
Visual Studio 2017 pipeline; the current package used Visual Studio 2022.

The earlier instruction to proceed immediately to the `DEBUG1` Right Alt test
is now on hold. Keep the official macOS 2.4.0 client unchanged as the known-good
comparison endpoint, and do not repeat the same coordinated connection test
until Windows publishes a new server binary identity and requests a reconnect.
This does not pause the project or prohibit independent Mac-side analysis; it
only avoids changing the validated client baseline while the failing Windows
build is replaced.

The Windows side will:

1. Rebuild the v2.4.0 remap source with the release-era v141/Visual Studio 2017
   toolchain as closely as possible.
2. Test that binary with no remaps before packaging.
3. Proceed to the `Right Alt`/`Hangul` to F16 `DEBUG1` test only if mouse
   behavior matches the official server.
4. Stop this fork approach if the toolchain-parity build still fails the
   no-remap mouse baseline.

External Windows key injection and Karabiner post-processing are not fallback
paths; they were already tested and found unusable with Barrier's input path.

## Required Karabiner input path

Stable mouse transport and server-side remapping are not the complete product
goal. Remote keyboard events must ultimately enter macOS through a virtual HID
keyboard path that Karabiner-Elements can recognize and process as an input
device. The existing Quartz/CGEvent injection path bypasses Karabiner and does
not satisfy this requirement.

This is required for the existing Mac-side application conditions, including
Figma, Photoshop, Illustrator, Finder, browser, and input-source rules.
Reimplementing all of those application-aware rules on the Windows server is
not an acceptable substitute because the server does not own the Mac frontmost
application context.

The intended division is:

1. Windows server remapping normalizes Windows-only source keys that Barrier
   cannot otherwise preserve, including Korean `Right Alt`/`Hangul`.
2. The Mac client emits the resulting key through a virtual HID input device.
3. Karabiner applies the existing Mac-wide and application-specific rules.

The temporary official-client baseline hold above only isolates the Windows
mouse regression. It does not remove or defer this requirement from the
definition of a usable release. After the Windows no-remap baseline passes,
the virtual HID-to-Karabiner path must be restored and validated separately
without mixing cursor or networking changes into that test.
