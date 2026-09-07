# Recovery of the installed 3.0.3-gureum-modes client

This branch preserves recovered historical source, not a new IME fix. It starts
at Input Leap commit `e36e16ff` and replays four recorded patches, in order.
The new restart, clipboard and automatic-termination fixes are deliberately
absent here so the installed baseline can be compared independently.

## Evidence and sequence

The original local task history contains the worktree creation, complete patch
payloads, build/package/install commands and subsequent tool results. Only the
source patches are preserved here (paths made repository-relative); raw session
history, machine configuration, logs and private network information are not.

Times below are UTC on 2026-08-30/31 (KST is UTC+9):

1. Aug 30 23:42:45: create a worktree from origin/codex/input-leap-keymap,
   corresponding to `e36e16ff`.
2. 23:43:21: `01.patch.txt` adds focused F19 delivery to the foreground PID.
3. 23:55:37: `02.patch.txt` removes that experiment, uses direct input-source
   selection with activation polling and a 75 ms notification settling interval.
4. 23:55:46: `03.patch.txt` removes the leftover header comment.
5. Aug 31 00:00:39: `04.patch.txt` prefers Gureum.system for Roman mode, with ABC
   fallback; Hangul selection prefers Gureum.han2 with Apple Korean fallback.
6. 00:00:46: rebuild input-leapc and unittests, run KeyRemapperTests.
7. 00:00:55: package with INPUTLEAP_KEYMAP_VERSION=3.0.3-gureum-modes.
8. 00:01:10: replace the prior experimental app; the result at 00:01:34 confirms
   the new version and a connected client.
9. 03:36:02: temporarily install the separate VHID experiment; at 03:36:54 the
   recorded command restores the previous gureum-modes bundle.

The four source patches apply cleanly to the base. Their net effect is limited
to OSXKeyState.cpp and OSXKeyState.h. The historical status output before the
direct-TIS/Gureum patches also lists only those source files as modified.

### Important bundle distinction

The last two builds only target input-leapc and unittests. The packaged GUI and
server were built earlier, while the focused-F19 experiment was present. Thus
the client baseline recovered here must not be described as proven source for
every executable in the installed bundle. The GUI is unaffected by these two
  platform source changes; the server's earlier build can contain focused F19.

## Installed executable fingerprints (2026-09-07, read-only)

SHA-256:

| Executable | SHA-256 |
| --- | --- |
| input-leapc | 5e69dcb9a5abfe41d86f33fb9cb65ee97e3d43529927671a56f1687aceb830ee |
| input-leap | ace728c339917181f6d5929a82c88cdf3afadfe2b6a7d765f6150f97b39071e3 |
| input-leaps | 25ea211eba4f5c5073c5b8463f3148c7b3426e57303e5d7058de5d9baaa9248e |

Installed client string/symbol inspection confirms the activation-confirmed F19
message and Gureum.system/han2 identifiers; postKeyToFrontmostApplication is
absent, consistent with patch 02 removing patch 01's experiment. This is
corroboration, not proof of whole-executable byte identity.

Recovered Git blob IDs:

- OSXKeyState.cpp: `49bab4afb03472e9737d345eecead81cdadaec54`
- OSXKeyState.h: `65de8c671564baffedcd58076b0110a103b3fe2c`

Historical comments claiming this mode switch finishes Safari composition are
preserved verbatim for recovery fidelity. That remains an unconfirmed hypothesis;
the user subsequently reported Safari failures. Do not interpret recovery as a
Safari fix.

## Recorded build recipe

The original successful configuration uses Release, arm64, macOS target 11.0,
the CommandLineTools macOS SDK, Homebrew Qt 5/OpenSSL 3, bundled gulrak filesystem,
GUI/tests enabled, audio daemon disabled and INPUTLEAP_VERSION_DESC=git.
The SDK path was added after the first link attempt failed.

Pinned dependencies:

- gtest: 15460959cbbfa20e66ef0b5ab497367e47fc0a04
- gulrak-filesystem: 614bbe87b80435d87ab8791564370e0c1d13627d
- miniaudio: 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d

Rebuilding with today's SDK/libraries and a different source path does not
automatically establish byte-for-byte reproducibility of the signed bundle.
Installation, signing identity and Accessibility authorization need separate
checks before deployment. No running application is changed by source recovery.

## Rebuild and code comparison results (2026-09-07)

- Client and unit-test binaries built successfully with two parallel jobs.
- All 18 KeyRemapperTests passed. This is not a Safari runtime test.
- First rebuild: installed and rebuilt client strings differed only in the
  absolute SecureSocket.cpp build path. The displaced string changed literal
  addresses in the executable code.
- Rebuild using `-ffile-prefix-map=<recovery-source-root>=<historical-source-root>`
  to reproduce that one historical path: **the entire __TEXT,__text and
  __TEXT,__cstring section dumps match exactly**, including virtual addresses.
  No differences remain in the output of `strings` for the two clients.
- The installed server still has the focused-F19 messages, confirming the mixed
  build-stage bundle distinction above.

SHA-256 of `otool -s <segment> <section>` output after removing its first two
heading lines (these are text-dump hashes, not raw-section or file hashes):

| Section | Installed and rebuilt (identical) |
| --- | --- |
| __TEXT,__text | c4d2b4dc144070c6c8717c3747a2c81064be5f83a9e19fd14bc4e35c72fd0bca |
| __TEXT,__cstring | 2307f2641b99ffbedcf3f1b582e8ed8f3b8ce586c0f06bc3d349dedecea44e2c |

Rebuilt client file SHA-256 after prefix mapping:
`c54bfbac1160e4b7504a4ae0137d8de9f31876883ee7e2eb748725a939b15806`.
It differs from the installed signed/packaged file; complete bundle identity,
all data sections and runtime equivalence have not been established. The code
and string match is strong evidence for recovery of the installed client source.

Build caveat: Homebrew OpenSSL 3.6.3 objects warn that they target macOS 26.0
while the historical recipe requests 11.0. This recovery build must not be
advertised as validated for macOS 11.0. Existing deprecated API warnings remain.

The baseline was rebuilt before committing recovery, retaining the original
`e36e16ff` version metadata. A future build after the recovery commit naturally
has different version metadata unless explicitly reproduced. Do not use a
historical version label for a deployment containing newer stability changes.
