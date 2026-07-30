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
