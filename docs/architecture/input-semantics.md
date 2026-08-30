# Input semantics overview

KeyStitch extends Barrier at the point where server-side keyboard events are
selected for a target screen. A remap can therefore be scoped to the target
operating system before the event is delivered to the remote client.

```text
physical/local input
        ↓
Barrier server event handling
        ↓
KeyStitch remap rules
        ↓
target-screen event stream
        ↓
Barrier client / platform keyboard injection
```

This boundary is important because a remote event is not always equivalent to
a local hardware event once it reaches the receiving operating system. Server
side translation keeps the platform-specific rule close to the target-screen
decision and avoids requiring a separate remapping utility on every client.

The implementation currently covers direct remaps, tap/hold rules, modifier
chords, left/right modifier names, and platform-specific handling documented in
the [key remap guide](../../doc/key-remaps.md).
