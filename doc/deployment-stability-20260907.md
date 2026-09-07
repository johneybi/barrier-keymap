# Local macOS stability deployment, 2026-09-07

## Source composition

- The installed Gureum client baseline was recovered in `f2a93d73` and
  integrated here as `e357fe4d`. See `recovery/20260831/README.md` for the
  executable-code and string-section comparison against the old installation.
- OSXKeyState.cpp/h are identical to that recovered source. This update does
  not introduce a new Safari IME workaround.
- Included stability changes: cancellable GUI restarts, client retry timer
  deduplication/backoff, per-user GUI locking, paced clipboard transfer with
  per-connection receive state, and automatic-termination deferral.
- The recovered old bundle mixed build stages: its server had earlier focused
  F19 code. The new client and server are built from one consistent source.

The prior source-only review documents describe historical validation status.
This deployment record supersedes their missing-source warning for the client;
it does not supersede their remaining runtime limitations.

## Packaging safeguards

The package Info.plist records `KeyStitchSourceRevision` so an installed app can
be traced to its commit. Its distinct version label must not reuse the old
`3.0.3-gureum-modes` label. The designated signing requirement and bundle ID are
kept unchanged; Accessibility authorization still needs verification at launch.

This local Apple Silicon build targets macOS 26.0, matching the Homebrew OpenSSL
objects available on the build machine. `INPUTLEAP_MINIMUM_MACOS=26.0` also sets
the package minimum OS honestly. This is not a general macOS 11 release.

Before replacement: build GUI/client/server and unit tests, verify the package
signature and revision, preserve the old bundle and relevant preferences in a
dated local backup, then stop only the old Input Leap GUI/client. Do not alter
Windows, Gureum preferences or unrelated processes.

After replacement: verify running executable paths/version, one GUI and client,
successful connection and absence of immediate restart loops. Confirm real
typing and cross-screen movement with the user; do not inject text into their
active document. Low-memory survival, long-run network stability and the known
Safari input issue require separate observation.

## Completed local installation

Installed at 2026-09-07 10:06 KST:

- Version: `3.0.3-gureum-stability-cd1086ee`
- Embedded source: `cd1086eef1349bfdf0677e9d2b23c0045a6e9262`
- Architecture/minimum OS: arm64 / macOS 26.0 (host: 26.5).
- Package and installed bundle pass deep/strict codesign verification with the
  same designated identifier requirement as the previous installation.

Validation:

- GUI, client, server and unit-test targets built successfully.
- All 25 targeted retry/clipboard-sender/key-remap tests passed.
- Full unit suite: **155 passed, 2 failed out of 157**. Both failures,
  SecureUtilsTest.FormatSslFingerprintHexWithSeparators and
  SecureUtilsTest.CreateFingerprintRandomArt, reproduce with identical actual
  values on the recovered pre-change baseline. They are not suppressed or
  claimed fixed. No SSL implementation changes are part of this deployment.
- One GUI and one child client observed after launch. Connected to the existing
  server, received clipboard updates and forwarded mouse events.
- Unified log confirms `starting cocoa loop (automatic termination deferred)`.
  The same client remained alive for the initial two-minute check with no
  observed automatic-exit/relaunch loop. This was not a memory-pressure test.
- Keyboard text behavior (especially Safari), long-run stability and stress
  behavior still require user/runtime confirmation.

Installed executable SHA-256:

| Executable | SHA-256 |
| --- | --- |
| input-leapc | 162e72fc24a094f8387b6a84fdb0cd4abfce8408fd1ee73481ea117b8eceebaf |
| input-leap | 0efcbbb67787722ecaa5a1fa2e29ed14cb6da29b71e742c8e520dd2358aba579 |
| input-leaps | 8fab8c9caf3b4be746485ed2da0a5954de31560358a1e4fef67353fb82700a7f |

Rollback assets are in the user's Application Support/InputLeapKeymap/Backups/
20260907-cd1086ee folder: a verified copy of the old bundle, the original bundle
moved at swap, and the two Input Leap preferences files. Old client fingerprint
matches the recovery record. No old bundle was deleted. Preferences were backed
up but not deliberately changed; normal GUI persistence may update them.

The old GUI restarted its client after the normal Quit request. It was stopped
with a verified-PID SIGTERM before its remaining client was stopped; neither was
SIGKILLed. Only after both had exited was the new bundle moved into place.

Windows software and input-method preferences were not modified. Windows still
uses its existing sender, so the new pacing is not yet deployed in that direction.
The installed source revision stays cd1086ee; the later deployment-record commit
is documentation only and does not require another app replacement.
