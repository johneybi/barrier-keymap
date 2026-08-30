# Decision: perform remapping on the server

## Context

Barrier already knows which remote screen is active when it processes input.
Client-side tools such as Karabiner-Elements or AutoHotkey may not recognize
remote input as ordinary local keyboard input, or may observe it too late to
preserve modifier behavior.

## Decision

Translate configured key events on the Barrier server before sending them to
the selected screen. Rules may be scoped per target screen.

## Consequences

- The remap configuration has one authoritative location.
- A Windows-to-macOS rule can use macOS semantics without changing the local
  keyboard setup on the client.
- The server must understand tap/hold timing and modifier state.
- Platform-specific behavior needs live verification on the affected systems.
