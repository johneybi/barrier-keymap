# macOS client input stutter investigation

## Reproduction

- Windows runs the Barrier server at `192.168.0.10:24800`.
- macOS connects as `ESKui-MacBookPro` with crypto disabled.
- The original Barrier macOS client is smooth against the same server.
- This project's macOS client connects, but mouse movement arrives in large,
  visibly delayed jumps. Keyboard input can also repeat excessively.

On 2026-07-28, the client diagnostic log repeatedly reported 400 to 860
absolute mouse messages per second. During the same samples every message was
marked as compressed and the pointer updated at a much lower rate. One screen
entry also produced seven `enter` and seven `leave` messages in one second.

## Client-side cause

`ServerProxy::handleData()` drains all currently available protocol messages
before calling `flushCompressedMouse()`. When the server sends mouse messages
continuously, the stream may remain ready for a long time. The compressed
position then waits for the read loop to finish, causing event-loop starvation
and large pointer jumps.

The diagnostic `forwarded` counter also did not include updates sent by
`flushCompressedMouse()`, so earlier logs understated the actual output rate.

## Client-side mitigation

Keep Barrier's motion coalescing, but impose an 8 ms maximum delay. While a
large batch is being drained, the client now forwards the newest absolute or
relative position at least once per interval. The normal end-of-batch flush is
unchanged.

Expected healthy diagnostics under continuous movement:

- `mouse` can remain high.
- `compressed` can remain high because coalescing is intentional.
- `forwarded` should rise to roughly the display update rate instead of
  remaining near zero.
- movement should remain smooth while CPU and HID event volume stay bounded.

## Windows-side checks

The Windows investigation should focus on why the server can emit 400 to 860
mouse messages per second and why rapid edge entry produced repeated
`enter`/`leave` pairs.

Please capture the matching `switch health` lines and check:

- `primaryMove` and `switch` counts during one continuous movement;
- the last coordinates and deltas at each switch;
- whether the Mac screen edge is configured at the expected side;
- whether identical coordinates are sent repeatedly;
- whether polling or high-resolution mouse settings changed the send rate.

The client latency cap prevents starvation, but repeated screen switching is a
separate server/layout issue and should be fixed at its source.

## Validation status

The patched macOS client and all unit tests build successfully. All 143 unit
tests pass.

The first patched live run connected on 2026-07-28, but the socket closed after
seven seconds before mouse input was exercised. A second run opened the
connection but did not receive the Barrier handshake. Re-run the live test
after the Windows server is ready and compare `forwarded` with the earlier
near-zero samples.
