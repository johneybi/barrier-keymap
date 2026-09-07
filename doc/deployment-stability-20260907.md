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
