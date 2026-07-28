# Mac to Windows test handoff

## Cursor regression observed on Mac

Testing Windows server commit `9c7882b5` with Mac client
`ESKui-MacBookPro` reproduced a cursor-control failure:

- TCP connection and the Barrier handshake succeeded.
- The Mac received `enter`.
- Received positions became stuck near `x=2559`, the right edge of the
  2560-wide Mac display.
- No `leave` arrived, so control could not return to Windows.
- The Mac client was stopped and the local cursor was restored.

Commit `9ced9a63` (`Break Windows cursor warp feedback loop`) appears to
address this exact failure. Pull, build, and restart the Windows server at
that commit before asking the Mac side to reconnect.

Run the next test in this order:

1. Enter the Mac screen.
2. Move continuously for 20 to 30 seconds.
3. Exit through each configured edge and return to Windows.
4. Confirm the cursor does not stick at `x=2559` and `leave` is emitted.
5. Test key remaps only after mouse behavior passes.

## Right Alt input-source switching is separate

On the Korean Windows keyboard layout, physical Right Alt arrives as Barrier
`Hangul` (`0xEF31`). The current server remap emitted `F18` (`0xEFCF`).
The tested Mac uses F18, macOS virtual key code 79, for input-source switching.

The Mac client special-key table only included F1 through F16. A local,
not-yet-pushed Mac change adds F17 through F19. It builds successfully and all
146 unit tests pass, but live validation must wait until mouse control is
stable.

## Windows-side status after this handoff

The Windows side pulled this document at commit `00024b3c` and confirmed the
findings against the source and server logs.

- The first recentering fix, `9c7882b5`, was not sufficient. Deltas around
  `+1266` through `+1270` passed the old half-screen heuristic and were sent to
  the Mac, keeping the remote cursor at the right edge.
- Commit `9ced9a63` now checks for stale edge events before issuing another
  cursor warp. It also avoids feeding a discarded edge coordinate back into
  the next-delta origin.
- GitHub Release run `30386514354` built `9ced9a63` successfully for Windows,
  macOS, and Linux.
- The Windows installer from that run is installed. The server starts with a
  single `2560x1440` display and listens on port `24800`.
- The current Windows test configuration temporarily emits `F18` because the
  Mac handoff reported virtual key code 79 as its configured input-source
  shortcut. This is not a project-wide choice: the output must match the
  shortcut actually configured on the Mac under test.
- Live mouse validation of `9ced9a63` is still pending. Do not treat the cursor
  regression as fixed until a Mac can move for 20 to 30 seconds and return to
  Windows through the left edge.

## Coordination required

The F17 through F19 macOS key-table change described above is not present in
the shared branch as of `00024b3c`. This statement does not imply that another
push will happen automatically.

To avoid duplicate or conflicting work, the Mac side should either:

1. push the already-tested F17 through F19 change and report its commit hash,
   or
2. explicitly hand ownership of that change to the Windows side.

Until one of those actions happens, the shared source cannot reliably map a
Barrier F17 through F19 event to the matching macOS virtual key. Before the
next key test, the Mac side must report which function key is actually
configured for input-source switching; the Windows remap must then emit that
same key. A server log showing either F18 or F19 is necessary but not
sufficient evidence that input-source switching works.
