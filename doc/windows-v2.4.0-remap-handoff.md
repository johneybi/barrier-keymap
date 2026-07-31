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

No Right Alt test occurred during this run. The event received at `17:14:42`
was an ordinary Return event:

```text
KeyID: 0xEF0D
button: 0x001C
macOS virtual key: 0x24
```

`0xEF0D` is Barrier `Return`, and macOS virtual key `0x24` is Return. An earlier
version of this document incorrectly attributed this event to Right Alt based
on conversational timing. That attribution and the conclusions derived from
it were invalid.

Right Alt behavior remains untested for this run. Before the next test, record
an explicit timestamp or marker, keep the pointer on the Mac, press Right Alt
exactly once, and correlate the corresponding Windows `DEBUG1` lines:

```text
onKeyDown screen=... key=... id=... button=...
key remap pending tap ...
key remap tap ... ->F16 ...
onKeyUp screen=... key=... id=... button=...
```

The raw Windows KeyID and remapper output must be identified before drawing a
conclusion about the Right Alt rule.

### Isolated Right Alt capture

The Right Alt test was repeated with an explicit `17:18:37` marker. The user
pressed and released Right Alt exactly once before typing the confirmation
message. At `17:18:44`, the official Mac client received:

```text
recv key down id=0x0000ef31, mask=0x0000, button=0x0138
key ef31 is not on keyboard
recv key up id=0x0000ef31, mask=0x0000, button=0x0138
```

`0xEF31` is Barrier `Hangul`. This event is distinct from the confirmation
message that started at `17:18:49`. The Windows server recognized the Korean
Right Alt/Hangul input but relayed it unchanged, so the official Mac client
discarded it before Karabiner could receive the intended F16 trigger.

Ensure the active Windows server configuration contains the following rules
under the exact canonical Mac screen name, then restart the server:

```text
section: remaps
  ESKui-MacBookPro:
    right_alt.alone = F16
    right_alt.hold = right_super
    hangul.alone = F16
    hangul.hold = right_super
end
```

On the next isolated tap, the Windows log must show the Hangul tap rule
producing F16, and the Mac log must receive `id=0xEFCD`. If the Mac still
receives `0xEF31`, verify that the running `barriers.exe` is the remap build and
that it loaded the edited configuration file rather than a GUI-generated or
temporary config without `section: remaps`.

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

## 2026-07-31 client burst-delivery measurement

The single Mac client at commit `1d652e67` measured the live v141 server with
INFO-level diagnostics and an 8 ms maximum mouse-compression delay. During
continuous movement, the client received between 188 and 806 absolute mouse
messages per second, but forwarded only 2 to 4 pointer updates per second.
Almost every received message was compressed.

Representative samples:

```text
mouse=334 forwarded=4 compressed=334
mouse=666 forwarded=3 compressed=666
mouse=806 forwarded=3 compressed=806
mouse=750 forwarded=3 compressed=750
```

No input batch exceeded 50 ms, so the Mac drained each available batch
quickly. The 8 ms client latency bound cannot help when hundreds of messages
arrive in only 2 to 4 large bursts per second and no newer data is available
between bursts. Keyboard input shares the same TCP stream and was also visibly
delayed.

This moves the active investigation to Windows output delivery rather than
Mac event processing. The Windows build should:

1. Verify `TCP_NODELAY` with `getsockopt` immediately after `setsockopt` on the
   accepted client socket and log both the value and any Winsock error.
2. Count socket writes, output flushes, messages, and bytes per second.
3. Compare packet timestamps for the same movement using the official server
   and the custom v141 server.
4. Run an INFO-level test to exclude thousands of discarded-motion log writes
   from the timing path.

## 2026-07-31 valid remote Right Alt result

The test was repeated after confirming that the physical keyboard was attached
to the Windows server and the pointer was on the Mac screen. At `01:13:16`, the
Mac client received exactly one F16 press and release:

```text
recv key down id=0x0000efcd, mask=0x0000, button=0x0138
recv key up id=0x0000efcd, mask=0x0000, button=0x0138
```

The Mac key map translated this to macOS virtual key `0x6a`, which is F16.
This closes the Windows-side Right Alt/Hangul remap question: the active server
configuration converted the physical Windows Right Alt input to F16 and
delivered it correctly over the Barrier protocol.

Do not change the working remap rule for the next test. The remaining keyboard
failure is on the Mac output side. Immediately before posting F16, the Mac
client's privileged VirtualHID socket had closed, so the client logged:

```text
VirtualHID helper connection failed; using IOHID
```

The Mac side will repair and retest the helper connection. Windows should keep
the known working remap configuration and concentrate only on preserving the
usable mouse-delivery baseline. A Windows DEBUG1 capture of the same tap is
useful corroboration, but it is no longer required to identify the failing
stage.

## 2026-08-01 extra mouse button observation

The user reported that the physical mouse's side Back button triggered an
unexpected action on the Mac target. Testing was stopped for the day, so the
exact action and live button trace have not yet been captured.

The existing path is independent of keyboard remapping:

1. The Windows server maps `XBUTTON1` and `XBUTTON2` to Barrier
   `kButtonExtra0` and `kButtonExtra1`.
2. The Mac client maps those IDs to Quartz button indexes 3 and 4.
3. Quartz posts them as generic `kCGEventOtherMouseDown` and
   `kCGEventOtherMouseUp` events.

This preserves numbered mouse buttons, but it does not explicitly encode the
semantic actions Browser Back and Browser Forward. Their behavior may
therefore depend on the target application or the local mouse driver. Do not
change the keyboard remapper or reconnect the client to investigate this.

For the next isolated test, enable `DEBUG1`, press side Back once and side
Forward once, and record the incoming Barrier button IDs and the Mac
`faking mouse button id` lines. Repeat once in a browser and once in Finder.
If the IDs are correct but the actions remain application-dependent, add an
explicit, target-screen mouse-button mapping instead of changing the global
Barrier button numbering. If the actions are merely reversed, fix the
`Extra0`/`Extra1` mapping with a focused regression test.
