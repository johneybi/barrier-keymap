# Audio relay design

> The current ScreenCaptureKit sender is a system-audio mirroring milestone.
> The product goal of making the Windows computer selectable as a normal macOS
> audio output is specified separately in
> [macos-virtual-audio-output-handoff.md](macos-virtual-audio-output-handoff.md).

The audio path is intentionally separate from Input Leap's keyboard and mouse
connection. The input protocol stays on TCP port `24800`; the audio path will
use a separate media port, currently `24801`.

## First implementation boundary

The `src/lib/audio` library currently contains the platform-independent audio
format and relay configuration contract. AOO owns the media packet format,
Opus encoding, packet loss handling, and jitter buffer; the product must not
invent a second audio packet protocol around it.

This boundary lets the platform code evolve independently:

```text
macOS ScreenCaptureKit -> PCM source -> Opus/AOO
                                              |
                                              v
Windows WASAPI <- playback/jitter buffer <- Opus/AOO
```

## Planned open-source components

- AOO (`https://git.iem.at/cm/aoo`), BSD-licensed, for low-latency peer-to-peer
  audio transport and jitter handling.
- Opus (`https://opus-codec.org/`), BSD-licensed, for the encoded payload.
- miniaudio (`https://github.com/mackron/miniaudio`), MIT-0/public-domain
  option for Windows WASAPI device enumeration and playback boilerplate.
- macOS ScreenCaptureKit remains a native Apple API because it is the system
  API that can capture application or system audio. Screen-recording permission
  is required.

SonoBus and Snapcast are useful reference applications, but their GPLv3
licenses make direct code integration into this GPLv2 project unsuitable
without a separate legal review. They can still be used as independent
experiments.

## Security and reliability requirements

The audio channel must not share the input TCP stream. It needs its own
authentication/encryption, bounded jitter buffering, sequence-gap handling,
device selection, and a clean stop path. Audio is an optional feature: if it
fails, keyboard, mouse, and clipboard sharing must remain connected.

The first executable milestone is a Windows playback-only receiver using AOO.
It is built as `input-leap-audiod` and is intentionally separate from the
keyboard/mouse process. The macOS source is built as `input-leap-audios` and
now has a CoreAudio device path as its product path. ScreenCaptureKit remains
an explicit fallback for diagnostics and requires macOS 13 or newer plus Screen
Recording permission. The AOO invitation is accepted automatically for this
milestone; authenticated pairing is still a follow-up requirement.

## Build the receiver milestone

Initialize the external sources and enable the optional target:

```text
git submodule update --init --recursive
cmake -S . -B build -DINPUTLEAP_BUILD_GUI=OFF -DINPUTLEAP_BUILD_AUDIO_DAEMON=ON
cmake --build build --target input-leap-audiod
```

When the audio target is enabled, CMake verifies and applies the pinned AOO
IPv4 transport patch before adding the dependency. Re-running CMake is safe;
an already-applied patch is detected and left unchanged.

Run it on the Windows speaker machine with the Mac source endpoint:

```text
input-leap-audiod.exe --mode receive --source 192.168.0.40 --source-port 24801
```

This milestone opens the default Windows playback device. Build the macOS
sender with:

```text
cmake -S . -B build -DINPUTLEAP_BUILD_GUI=OFF -DINPUTLEAP_BUILD_AUDIO_DAEMON=ON
cmake --build build --target input-leap-audios
```

List the macOS devices first and identify the input side of the virtual device
by its stable UID:

```text
input-leap-audios --list-audio-devices
```

Run `input-leap-audios` on the Mac source machine with that UID. The Windows
receiver should use the Mac's address as `--source`, with the same UDP port and
source ID:

```text
input-leap-audios --capture-mode device \
  --audio-device-uid <virtual-device-uid> \
  --media-port 24801 --source-id 1
input-leap-audiod.exe --mode receive --source 192.168.0.40 \
  --source-port 24801 --media-port 24801 --source-id 1
```

Phase 1 expects a stereo, 48 kHz device. On a development Mac, install the
test device with `brew install --cask blackhole-2ch`, authenticate the package
installer, and reboot if macOS requests it. Select BlackHole 2ch as the macOS
Sound output before starting the sender. The sender does not install or
rebrand that third-party driver. Use `--capture-mode screen` only
to exercise the older ScreenCaptureKit fallback, which mirrors audio instead
of becoming a selectable macOS output.

The receiver sends the AOO invitation; the Mac sender accepts it and begins
forwarding captured PCM frames. This first connection is not authenticated,
so it should only be tested on the trusted local network.

Both commands remain running without an attached standard-input stream and
stop on `SIGINT`/`SIGTERM` (`Ctrl+C` in an attached terminal), which allows a
GUI or startup task to supervise them safely.
