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

## Server logging finding

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

This can amplify stalls and makes `DEBUG1` unsuitable for performance tests.
The high-frequency edge messages have therefore been raised to `DEBUG2`.

However, a later Windows run used `DEBUG`, not `DEBUG1`, and did not emit these
per-motion edge lines. The Mac still showed severe stutter while TCP remained
established and the server logged no keep-alive death or disconnect. Verbose
logging is therefore an aggravating factor, but it does not explain the current
symptom by itself.

## Next Windows test

Build and install a Windows server containing the secondary-entry inset, then
run it at `INFO` or `NOTE`, with `DEBUG1`/`DEBUG2` disabled. Avoid a verbose
file logger for this test. The once-per-second `switch health` line remains
available at `INFO`.

Then connect either Mac client and compare:

- pointer smoothness during ten seconds of continuous movement;
- `switch health` counts on Windows;
- `protocol health` counts on the patched Mac client;
- repeated `enter`/`leave` pairs at the configured edge.

Commit `fa942057` insets secondary-screen entry coordinates and may fix the
edge bounce. It does not address delayed motion while the pointer is already
inside the Mac screen, so test the logging level independently.

If it still stutters, capture TCP packet timestamps on both hosts. Compare when
the Windows server queues and sends mouse protocol messages with when macOS
receives each batch. TCP already has `TCP_NODELAY` enabled in `TCPSocket`, so
the timestamps are needed before changing socket buffering or event scheduling.

## Windows packet capture result

A Windows `pktmon` capture filtered to `192.168.0.40:24800` recorded a
continuous movement test at the physical NIC:

- 11,054 Windows-to-Mac data packets over 63.1 seconds;
- 11,039 mouse-sized packets with a 12-byte TCP payload;
- 1.986 ms median transmit gap and 5.017 ms 90th-percentile gap;
- roughly 170 to 585 transmitted mouse packets per active second.

The Mac TCP stack acknowledged the movement stream continuously:

- 5,636 ACKs during the 30-second movement interval;
- 3.411 ms median ACK gap and 8.782 ms 90th-percentile gap;
- cumulative ACKs advanced mostly in 12, 24, or 36-byte increments.

Therefore the Windows socket multiplexer, NIC transmission, Wi-Fi path, and
Mac kernel TCP reception are not producing the observed two-to-four batches
per second. The delay occurs after bytes reach the Mac TCP stack and before
Barrier dispatches `ServerProxy::handleData()`.

## macOS event queue hypothesis

`TCPSocket::doRead()` posts `inputReady` through `EventQueue`, but macOS replaces
the normal condition-variable buffer with `OSXEventQueueBuffer`. That legacy
buffer posts Barrier's internal `Syne` events to the Carbon event queue at
`kEventPriorityStandard`. If the event is delayed, the socket thread continues
filling its input buffer without posting another readiness event because the
buffer is no longer empty. When Carbon finally dispatches the pending event,
`ServerProxy::handleData()` drains hundreds of accumulated mouse messages in
one batch.

As a focused test, internal Barrier events are now posted with
`kEventPriorityHigh`. This does not change packet parsing, mouse coalescing, or
VirtualHID behavior. A live Mac test should compare `protocol health` batch
frequency and pointer smoothness before considering a larger replacement of
the deprecated Carbon queue integration.

## High-priority event test result

The macOS client at commit `de1ecfab` was rebuilt and tested with
`kEventPriorityHigh`. The pointer remained severely stuttery.

For active one-second samples with at least 100 mouse messages:

- 26 samples covering 26.23 seconds;
- `mouse=11,168`;
- `forwarded=76`;
- `compressed=11,164`;
- 425.8 received mouse messages per second;
- 2.90 forwarded positions per second;
- 99.96% of received messages entered the compression path;
- per-sample `mouse` range of 175 to 808;
- per-sample `forwarded` range of 2 to 4.

Only 0.681% as many positions were forwarded as mouse messages received. The
eight millisecond compression cap was active, but each readiness dispatch
still delivered a short burst followed by a long wait. Raising the Carbon event
priority therefore did not restore timely socket-readiness dispatch.

The next macOS experiment should measure `OSXEventQueueBuffer::waitForEvent()`
and `TCPSocket::doRead()` timestamps directly. If they confirm that the Carbon
queue remains the delay boundary, replace the legacy Carbon wake-up with a
condition-variable, pipe, or another native run-loop wake-up that cannot merge
hundreds of socket messages behind one pending `Syne` event.

The next diagnostic build logs `socket read health` once per active second. It
reports `calls`, `bytes`, `maxCall`, `ready`, and `buffered` from
`TCPSocket::doRead()`. Hundreds of calls with only a few forwarded positions
would isolate the delay to event dispatch. Only two to four large read calls
would instead implicate the macOS socket multiplexer wake-up path.

## Confirmed macOS event queue fix

The socket diagnostic showed that the Mac read continuously while Barrier
dispatched only about two readiness events per second:

- 219.6 socket reads per second;
- 1.93 `inputReady` events per second;
- 3.6 KB average unread buffer;
- 462 mouse messages per second;
- 2.97 forwarded mouse positions per second.

The fix keeps macOS system events on the Carbon queue but stores Barrier user
event IDs in a separate thread-safe FIFO. Carbon `Syne` events remain as the
cross-thread wake-up mechanism. Because current macOS can leave those wake-ups
pending for hundreds of milliseconds, `waitForEvent()` now bounds each Carbon
wait to four milliseconds and checks the FIFO again.

A live test was smooth. Across nine active one-second samples:

- 832.2 mouse messages per second;
- 203.7 forwarded mouse positions per second;
- 260.7 socket reads per second;
- 187.2 `inputReady` events per second;
- 41 bytes average unread buffer.

Forwarding improved from about 3 to 204 positions per second while the unread
buffer fell from about 3.6 KB to 41 bytes. The test client used about 1.4% CPU
after movement stopped. This confirms that delayed Carbon wake-up dispatch,
not TCP delivery, packet parsing, VirtualHID, or key remapping, caused the
severe input stutter.
