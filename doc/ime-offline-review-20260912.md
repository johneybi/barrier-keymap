# IME offline review and source-only changes — 2026-09-12

## Operating constraints

The user is working with the built-in keyboard and an attached mouse. All live
IME tests are deferred until explicitly requested. This follow-up uses source
inspection and inert automated tests only: no app launches, foreground changes,
CGEvent delivery, event taps, keyboard/clipboard observation, installed-app
replacement or preference changes. Builds use one job and request nice level
15. A sandboxed invocation could not change nice priority; its short inert tests
still ran. Subsequent compilation uses the approved low-priority invocation.

## Confirmed source defect: macOS modifier-side tracking

`OSXKeyState::postHIDVirtualKey` previously stored one boolean for both physical
sides of each modifier. Pressing both sides and releasing either cleared the
entire flag, including flags on the next character. An unmatched opposite-side
release also cleared a held modifier. These are deterministic state bugs, not
evidence that this particular sequence caused the user's Safari failure.

Two new tests failed against the original state logic, with OS delivery replaced
by an inert sink. A set of pressed modifier virtual keys replaces the shared
booleans. Repeats are idempotent; releases remove only their own side. The same
tests pass after the fix for Shift, Control, Option and Command, in either release
order. Caps Lock retains the original down/up interpretation; no new Caps Lock
or local/remote modifier arbitration policy is introduced.

`postKeyboardEvent` is the narrow overridable OS boundary. Tests exercise real
modifier bookkeeping and receive computed flags without creating or posting a
CGEvent. No hardware is polled. Production still uses the same CGEvent creation,
flags and HID posting sequence.

## Real core pipeline tests, simulated platform boundary

`KeyRemapPipelineTests` joins the production KeyRemapper, KeyState and KeyMap.
The layout is a small fixed fixture; all platform output is recorded in memory.
It does not test Windows hooks, protocol serialization, macOS F19 source selection,
TIS, real keyboard layouts, WebKit or IMK.

Covered sequences:

- 32 Right Alt and 32 Windows Hangul tap/first-letter cycles: one F19 pair,
  one ordinary key pair, no lingering key state or group changes in this map.
- Timer-expiry hold and another-key-interrupted hold: a Super modifier, no toggle.
- Shift held around a tap: modifier restored and eventually released.
- Disconnect cleanup: held synthetic modifier released before a subsequent tap.
- First character absent from the current group: group 1 override, key down,
  group 0 restoration, key up. This proves the conditional group-mutation path
  exists, not that the user's current layouts trigger it.

Do not remove group overrides globally based on that last test. They are part
of character mapping across layouts; a replacement policy needs layout-specific
coverage and eventual IME integration tests.

## Diagnostic correction without changing IME switching policy

`setGroup` now checks `TISSetInputMethodKeyboardLayoutOverride`'s OSStatus and
reports failure instead of logging every attempt as success. Group diagnostics
distinguish override from restoration. The API error path has been compiled but
not exercised against the operating system during this work.

The F19 message explicitly states that the foreground IME state is unverified.
Comments no longer claim that selecting Gureum.system cleanly finalizes Safari
composition. Source preference, activation polling and the historical 75 ms
settling interval are intentionally unchanged. No new toggle mechanism or TIS
fallback is being deployed.

## Gureum candidate scope

The pinned upstream 1.13.2 method extraction now has 28 assertions, including real
nested calls within the inert harness. The original fails 12; the candidate
passes 28. The fixture commit is inserted once in both nested-call cases.

Other assignments to `inputting` were reviewed: `input(event:client:)` and
`candidateSelected` have paired assignments without the same explicit early
return. They still deserve reentrancy tests in the real Gureum project. The
patch is deliberately limited to the demonstrated method, not a claimed
complete audit/fix of the input method. Input Leap cannot safely reset a foreign
input controller's composition state by changing its own local state.

## Outstanding before release

Fresh single-job builds of `unittests` and `input-leapc` succeeded. The targeted
Input Leap suite passed **34/34** tests; Gureum extraction passed **28/28** for
the candidate (upstream failed the expected 12 assertions), and the offline
web contract passed **14** event cases. Existing deprecated IOKit calls and
duplicate-library linker warnings remain; no full-suite pass is claimed.

`xcodebuild -version` cannot run with the current active developer directory
(`/Library/Developer/CommandLineTools`). The actual Xcode-project Gureum build
is not verified. No Xcode install or developer-directory switch was attempted.

- Real Gureum build/integration tests (not just extracted methods and mocks).
- Configured layout/group behavior and IME mode callbacks in a controlled test
  session, including mode preference mismatch (`qwerty` vs `system`).
- Safari composition/first-character validation, only when the user requests it.
- Packaging, signing identity and installation validation for any release.

The installed connection-stability build is untouched. Source changes here are
not a claim that Korean/English switching has been fixed in the running app.
