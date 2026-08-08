# macOS virtual audio output handoff

## Product goal

The Windows computer must appear to a Mac user as a normal audio output
choice. After installation, the macOS Sound output menu should contain a
clearly named virtual device such as `Input Leap Windows Audio`. Selecting that
device and playing YouTube or any other system audio must play the sound from
the Windows computer's current default playback device.

This is audio routing, not background system-audio mirroring. ScreenCaptureKit
does not expose an output device and therefore is not the primary solution for
this goal.

```text
macOS application
  -> selected macOS virtual output device
  -> CoreAudio PCM reader / input-leap-audios
  -> Opus / AOO over UDP 24801
  -> input-leap-audiod on Windows
  -> current Windows default playback device
```

## Ownership

Implement and validate the virtual device and CoreAudio capture path on macOS.
Keep the existing Windows receiver protocol unchanged unless a concrete
compatibility defect is found. Commit all work to
`codex/input-leap-keymap` so the Windows side can pull and run an integration
test from the same revision.

## MVP behavior

1. Install or configure one stereo macOS virtual audio device.
2. Show the device in System Settings > Sound > Output and the menu bar Sound
   picker.
3. When the user selects it, route all audio written to that device into
   `input-leap-audios`.
4. Send stereo audio to the existing AOO source on UDP port `24801`, source ID
   `1` by default.
5. Let the Windows receiver initiate the existing AOO invitation and play the
   stream through its default playback device.
6. Stop or recover cleanly when the virtual device changes sample format, the
   default output changes, the network disconnects, or the receiver restarts.
7. Do not require Screen Recording permission for the virtual-device path.

Selecting the virtual output means the Mac's physical speakers do not need to
play the same stream. Simultaneous Mac and Windows playback is a later feature
and may use a Multi-Output Device or explicit duplication after clock-drift
behavior has been validated.

## Implementation sequence

### Phase 1: validate the route quickly

Use an externally installed, open-source two-channel virtual CoreAudio device
such as BlackHole for the first end-to-end implementation. Do not copy or
rebrand third-party driver code until its license and redistribution terms
have been recorded.

For a development machine, install BlackHole 2ch interactively with:

```text
brew install --cask blackhole-2ch
```

The package installer requires administrator authentication and macOS may
require a reboot before the device appears. After reboot, select BlackHole 2ch
as the macOS Sound output, then run `--list-audio-devices` and use the UID of
the device entry that reports two input channels.

- Locate the device by stable CoreAudio device UID, never only by display name.
- Add `input-leap-audios --list-audio-devices` to print device name, UID,
  channel counts, nominal sample rate, and transport type.
- Add `--audio-device-uid UID` to select the readable side of the virtual
  device explicitly.
- Read PCM with a CoreAudio HAL/AudioUnit callback rather than ScreenCaptureKit.
- Keep the existing ScreenCaptureKit implementation available only as an
  explicit fallback mode, for example `--capture-mode screen`.
- Make the virtual-device path explicit, for example
  `--capture-mode device --audio-device-uid UID`.

The first implementation now provides the device-mode CLI and CoreAudio HAL
capture path. It accepts a stereo 48 kHz virtual device UID, captures through
an input-enabled HAL output unit, and hands PCM frames to the existing AOO
sender through a bounded preallocated queue. A device is not bundled or
installed yet; BlackHole remains the external test dependency for this phase.

The sender must accept the device's actual stream format and convert it to the
relay contract where necessary. The network format remains 48 kHz, stereo,
float PCM before Opus encoding. Conversion and interleaving must happen outside
the real-time callback wherever possible; the callback must not log, allocate,
block, or perform network I/O.

### Phase 2: make it distributable

After Phase 1 proves the complete path, decide whether to distribute the
third-party device with its required notices or implement a product-owned
AudioServerPlugIn/DriverKit device. Record the decision and license analysis in
the repository before adding driver source or binaries.

The distributable implementation must include:

- a stable device name, UID, bundle identifier, and version;
- signed installation and uninstallation paths;
- recovery after reboot, sleep/wake, and sender crashes;
- no silent replacement of the user's previous physical output device;
- restoration guidance if installation or startup fails;
- packaging notes for development signing and production notarization.

Do not block Phase 1 transport validation on production signing or a custom
driver.

## Compatibility contract

Do not change these defaults during this task:

| Setting | Value |
| --- | --- |
| Media transport | AOO over UDP/IPv4 |
| Media port | `24801` |
| AOO source ID | `1` |
| Codec | Opus |
| Network sample rate | `48000` Hz |
| Channels | stereo (`2`) |
| Receiver | `input-leap-audiod` on Windows |
| Invitation direction | Windows receiver invites Mac source |

The audio relay must remain independent of the keyboard/mouse TCP connection
on port `24800`. Audio failure must never disconnect or stall Input Leap input,
clipboard, or cursor handling.

## Required diagnostics

At startup, log the selected device name and UID, input channel count, device
sample rate, network format, local UDP endpoint, and AOO source ID. Log device
loss, format changes, invitation acceptance, invitation timeout, and stream
restart as state transitions rather than once per audio callback.

Add counters that can be printed periodically at debug level for captured
frames, encoded frames, dropped callback frames, conversion failures, and
active sinks. These diagnostics are necessary to distinguish device-capture
failure from network or Windows playback failure.

## Acceptance criteria

The Mac work is ready for Windows integration testing when all of the following
are true:

1. The virtual device is visible and selectable in the standard macOS Sound
   output picker.
2. With that device selected, playing a browser video produces non-zero PCM
   capture counters without Screen Recording permission.
3. The current Windows `input-leap-audiod` accepts the stream and plays it from
   the Windows default output device.
4. Changing back to MacBook Speakers stops new audio from entering the virtual
   device and restores normal local playback.
5. Restarting the Windows receiver reconnects without restarting the Mac or
   reselecting the output device.
6. Thirty minutes of playback has no sustained busy loop, unbounded memory
   growth, repeated device reopen loop, or keyboard/mouse degradation.
7. Measured end-to-end LAN latency and dropouts are reported in the handoff;
   target interactive latency is at most 150 ms for the MVP.
8. The macOS arm64 CI artifact contains the updated sender, and its exact commit
   SHA and launch command are included in the handoff.

## Out of scope for this iteration

- Per-application audio routing.
- Simultaneous playback to multiple Windows computers.
- Mac and Windows synchronized dual playback.
- A polished GUI or menu-bar device selector.
- Remote-internet use.
- Replacing the current trusted-LAN security limitation.

The next handoff must state which virtual device was used, its UID, the exact
sender command, whether PCM counters increased, whether the AOO invitation was
accepted, and the commit containing the implementation.
