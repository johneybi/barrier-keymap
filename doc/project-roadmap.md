# Project roadmap

This project extends Barrier in layers so input transport changes can be tested
independently from key mapping behavior.

## Stable core

The `stable/v2.4.0-server-remap` branch starts at the exact Barrier v2.4.0
release tag (`3e0d758b`). It keeps the release mouse, socket, platform event,
and client paths unchanged, and adds only server-side key remapping,
configuration, tests, documentation, and build portability includes. The first
end-to-end target is a Windows server with the official Barrier macOS client.

The older `stable/server-remap` branch is retained for investigation only. It
was based on a post-release upstream commit and includes Windows cursor-warp
experiments, so it is not the release baseline.

## Extended macOS keys

An optional macOS compatibility layer adds F17-F19 to Barrier's existing key
table. This is a small client change for users who need those keys; it does not
change mouse handling, networking, or the macOS event loop.

## Karabiner VirtualHID bridge

An optional output backend exposes remote Barrier keyboard input as a distinct
VirtualHID keyboard. Karabiner-Elements can then apply device-specific complex
modifications to remote input while local keyboard rules remain independent.

Native macOS key output remains the default. VirtualHID must be opt-in, use a
stable vendor/product identity, fall back safely when unavailable, and avoid
feeding Karabiner's own output back into the bridge.

## Experimental input work

Carbon wake scheduling, socket diagnostics, mouse compression, and cursor warp
experiments remain outside the stable core. They should be promoted only when a
reproducible test demonstrates an upstream problem and the change passes both
native and VirtualHID client test matrices.

## Release gates

1. Official server and official macOS client establish the baseline.
2. Stable server with remaps disabled must match baseline mouse and keyboard
   behavior.
3. Stable server with F16 remaps must pass tap, hold, chord, screen-switch,
   disconnect, and stuck-modifier tests.
4. Extended F19 and VirtualHID layers are tested separately after the stable
   core passes.
