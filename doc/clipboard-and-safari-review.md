# Clipboard pacing and remaining Safari investigation

Source-only follow-up to issues #9 and #10. The installed application was not
replaced, launched or restarted. See `restart-stability-review.md` for the
source/installed-version mismatch; this branch is not a deployment candidate.

## Clipboard change (#10)

The user reports that large clipboard contents still correlate with the mouse
bouncing back to Windows. This is an observation, not proof of causation.

Previously `StreamChunker::sendClipboard` copied the entire serialized payload
into 32 KiB events in one call, including a keepalive event per chunk. These
events could build a large backlog alongside input and connection processing.

Both connection proxies now own a `ClipboardSender`. It writes one protocol
chunk per one-shot 10 ms timer, retains START/DATA/END ordering, cancels work on
disconnect/destruction, and coalesces queued snapshots to the latest per
clipboard ID. A transfer that has emitted START finishes before another begins,
because the unchanged wire protocol has no cancellation message. Normal
connection heartbeat timers remain responsible for keepalives.

Receive buffers and expected sizes are now per connection instead of static
process-wide state. Concurrent transfers from different peers must not reset
one another's receive state, especially with longer paced transfers.

Tradeoffs and limits:

- Nominal payload ceiling is about 3.1 MiB/s; large clipboard delivery is slower.
- One active snapshot plus at most one pending snapshot per clipboard ID remains
  in memory. The caller still marshals/reads the complete clipboard synchronously.
- This does not bound the socket output buffer or provide transport backpressure.
  A slow network can still delay input behind already-written clipboard data.
- Old Windows binaries still use the original sender. Changing source or pulling
  commits on either machine does not update its installed program.
- Clipboard pacing is not a fix for network outages, unintended screen switching,
  process duplication, or the Safari input-session issue.

## Validation

`git diff --check` passed. `ClipboardSender.cpp` passed C++17 syntax-only checking
with the system clang (no executable produced). Syntax-only checks of the
integration files stopped at missing generated `config.h`; no complete build,
unit-test execution, installation or input injection was performed.

`ClipboardSenderTests.cpp` adds deterministic tests for one-chunk-per-tick work,
binary preservation, serialized transfers, latest-snapshot coalescing,
cancellation/destruction, empty data and invalid IDs. These tests are not yet run.

Before release, build and run both new test suites, test concurrent inbound
clipboard transfers from two peers, and test both transfer directions with
empty/small/1 MiB/10 MiB text and image payloads. Include repeated copies,
disconnect mid-transfer, slow-reader/network conditions and active mouse/key
input. Compare delays, output-buffer growth and disconnects with clipboard
sharing disabled. Use only disposable test clipboard contents.

## Safari remains unresolved (#9)

Latest report: Safari web fields intermittently stop accepting the intended
language despite a correct menu-bar input-source indicator. Typing Hangul in
the address bar and then returning to the web field often restores input.
Recovery by clicking alone is not reliable. Do not count menu-bar changes as
successful text input.

The suspected IMK/Gureum composition or focus-session mismatch is unconfirmed.
No new TIS, key-injection or focus-changing workaround is applied here: the
installed experimental source must first be reconciled, and previous changes
have already caused regressions in global switching.

Next controlled comparison, once runtime testing is authorized:

1. Local physical keyboard versus Windows-delivered keys in the same Safari field.
2. Hangul-to-Roman and Roman-to-Hangul, with/without an active composition.
3. Plain textarea, contenteditable and address bar; Gureum and Apple Korean.
4. Record F19 receipt, input-source transition, subsequent key-event counts and
   focus changes with timestamps. Do not log text or clipboard contents.
5. Distinguish recovery from focus change alone versus typing in the address bar.

Only after this should an opt-in alternative injection/session path be selected.
Do not use synthetic Return/Escape, clicks or automatic address-bar focus as a
general fix: those can submit forms, discard composition or alter user work.
