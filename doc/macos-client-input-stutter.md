# macOS client input stutter investigation

## Reproduction

- Windows runs the Barrier server at `192.168.0.10:24800`.
- macOS connects as `ESKui-MacBookPro` with crypto disabled.
- An earlier test with the original Barrier client was smooth under an earlier
  Windows server run.
- This project's macOS client connects, but mouse movement arrives in large,
  visibly delayed jumps. Keyboard input can also repeat excessively.

On 2026-07-28, the client diagnostic log repeatedly reported 400 to 860
absolute mouse messages per second. During the same samples every message was
marked as compressed and the pointer updated at a much lower rate. One screen
entry also produced seven `enter` and seven `leave` messages in one second.

## Initial client-side hypothesis

`ServerProxy::handleData()` drains all currently available protocol messages
before calling `flushCompressedMouse()`. When the server sends mouse messages
continuously, the stream may remain ready for a long time. The compressed
position then waits for the read loop to finish, causing event-loop starvation
and large pointer jumps.

The diagnostic `forwarded` counter also did not include updates sent by
`flushCompressedMouse()`, so earlier logs understated the actual output rate.

## Client-side mitigation tested

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

The client latency cap prevents starvation inside a long read callback, but
repeated screen switching is a separate server/layout issue and should be
fixed at its source.

## Validation status

The patched macOS client and all unit tests build successfully. All 143 unit
tests pass.

On a later live run, the latency cap was active but only two to four mouse
positions were forwarded per second while 300 to 600 messages were processed.
This means the messages were already arriving in short, widely spaced batches;
the eight millisecond cap cannot smooth data that has not reached the client.

An unmodified Barrier v2.4.0 macOS client was then built for arm64 and connected
to the same currently running Windows server with the same screen name and
crypto setting. It showed the same severe stutter. This A/B result rules out
the key remapper, macOS VirtualHID work, protocol diagnostics, and the client
latency patch as the primary cause of the current symptom.

## Current leading cause: verbose server logging

The committed Windows test logs show the server running at `DEBUG1`. At an
unlinked screen edge, every movement can synchronously write both:

```text
try to leave "<screen>" on <direction>
no neighbor <direction>
```

Measured in the existing logs:

- up to 648 log lines in one second;
- 34,157 `try to leave` or `no neighbor` lines;
- 39,773 total lines and about 2.37 MB across the three test logs.

This is consistent with the Mac receiving hundreds of mouse messages in only
two to four batches per second. It also explains why the original client
stutters against the current server even though it was smooth in the earlier
test.

## Next Windows test

Run the Windows server at `INFO` or `NOTE`, with `DEBUG1`/`DEBUG2` disabled.
Avoid a verbose file logger for this test. The once-per-second `switch health`
line remains available at `INFO`.

Then connect either Mac client and compare:

- pointer smoothness during ten seconds of continuous movement;
- `switch health` counts on Windows;
- `protocol health` counts on the patched Mac client;
- repeated `enter`/`leave` pairs at the configured edge.

Commit `fa942057` insets secondary-screen entry coordinates and may fix the
edge bounce. It does not address delayed motion while the pointer is already
inside the Mac screen, so test the logging level independently.

If the `INFO` run is smooth, keep production defaults at `INFO` or `NOTE` and
rate-limit or raise the high-frequency edge messages above `DEBUG1`. If it
still stutters, capture TCP packet timestamps on both hosts before changing
more input code.
