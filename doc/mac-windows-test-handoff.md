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
