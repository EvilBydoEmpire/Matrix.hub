# Audio companion architecture

## Current vertical slice

The Mac performs all audio capture and FFT work. The focused native path sends
four useful controls—kick, snare, hi-hat, and Flow energy—while retaining the
ten-byte version-2 record for wire compatibility. The ESP32 does no audio
sampling or frequency transform.

```text
macOS source → Core Audio tap ─┬→ immediate 1,024/2,048 analysis → UDP → ESP32
                              └→ stereo delay → selected physical audio output
```

The companion lives at `tools/audio-companion/index.html` and is served from
localhost. Wi-Fi transport works through a persistent WebSocket on port 81.
Web Serial remains available as an automatic fallback and requires a secure
browser context; desktop Chrome and Edge treat localhost as trustworthy.

The browser page can request microphone input or a screen/system-audio capture.
Whether a browser offers a system-audio track for a selected Mac source varies.
A native companion also lives at `tools/matrix-audio-mac`. Version 0.8 uses a
private Core Audio process tap for either a chosen running application or the
whole system excluding the companion. It relays the captured stereo stream to
a selected physical output through a 0–1,000 ms delay line while feeding the
optimized radix-2 1,024-point and 2,048-point FFT paths immediately. Core
Audio's `mutedWhenTapped` mode removes the source's direct hardware path only
while the companion actively reads it. The ad-hoc-signed app bundle can be
built and used privately without the App Store.

The native app also has a predecoded MP3 reference source. It plays the
original file locally while feeding decoded mono 48 kHz PCM to the same
detector ahead of the audio playhead. Timestamped analysis frames let kick,
snare, hi-hat, and Flow energy use independent -1,000…+1,000 ms offsets. Negative
values emit a signal before its audible source event; positive values emit it
late. This removes causality as a variable when evaluating visual response and
can compensate network, firmware, panel-render, and perceptual delay without
delaying the song itself. These offsets apply only to predecoded file playback.
The live router cannot predict samples before they arrive, but delaying audible
playback makes an already-detected event early relative to the listener.

## Transport protocols

Protocol version 2 is a fixed 18-byte record: ASCII `AUD`, version 2, ten
unsigned signal bytes, and a little-endian 32-bit sequence number. The browser
uses it over its WebSocket on port 81; the native app sends the identical
datagram over UDP port 82. If browser Wi-Fi disconnects while USB is connected,
it immediately falls back to USB and keeps reconnecting in the background.

USB sends the existing newline-delimited representation:

```text
AUD2,sequence,sub,bass,lowMid,mid,presence,air,kick,snare,hat,rms
```

- `sequence` is an unsigned rolling packet number.
- All ten signals are integers from 0 through 255.
- The firmware rejects malformed or out-of-range records.
- A stream is considered active for one second after the latest valid packet.

The selectable browser update rates are 15, 30, and 60 packets per second, with
60 the default. The native app advances every 256 samples at 48 kHz, or 187.5
updates per second. The binary stream is only about 3.4 KB/s. Raw audio is never
transmitted or stored on the matrix. Firmware still accepts version-1 binary
and `AUD,sequence,low,mid,high` records and maps them into the expanded state.

## Analysis latency

The native analyzer uses a dedicated 1,024-sample raised-Hann kick window at
48 kHz (21.3 ms), while snare, hi-hat, Flow energy, and the spectrum retain a
trailing 2,048-sample Hann window (42.7 ms). Both advance on a 256-sample hop
(5.3 ms), giving the kick path 75% overlap. SuperFlux compares each spectrum bin against the maximum of that bin
and its two neighbours in the previous frame, suppressing pitch glides while
retaining new attacks. A deterministic synthetic test detects a kick 2.67 ms
after onset under ideal sample delivery.

Kick, snare, and hi-hat each have an adaptive mean-plus-deviation threshold,
editable frequency range, refractory time, and a fixed low/mid/high spectral
classifier. A learned estimate of a bare kick's mid-frequency leakage helps
preserve simultaneous kick+snare hits. Accepted hits retain relative velocity
and start configurable one-shot Attack–Hold–Decay–Sustain–Release envelopes.
The Impact control emphasizes the first few milliseconds before the envelope
falls to its velocity-scaled sustain plateau.

Flow energy is the useful continuous signal from the earlier Unity visualizer:
eight logarithmic spectral regions self-normalize, combine, and pass through a
fast-attack/controlled-decay buffer. It replaces the old collection of broad,
noisy energy bands and drives sustained motion well. The native UI therefore
shows only four signals. The legacy six packet slots are transmitted as zero so
the existing firmware receiver and saved route layout remain compatible.

The FFT window is only part of end-to-end latency. Packet scheduling at 60 Hz,
browser/media capture buffering, and the next 60 FPS render deadline also
contribute. The fast native kick path removes most of the former FFT group
delay. The remaining practical estimate is roughly 15–35 ms: Core Audio
delivery, up to one 5.3-ms analyzer hop, low-single-digit-ms local UDP, and
0–16.7 ms until the next animation frame. Exact end-to-end latency still
requires an audio/photodiode or high-speed-camera loopback measurement.

## Wi-Fi behavior

Eighteen-byte datagrams at 60 updates per second are negligible for the ESP32
radio. USB remains useful for deterministic diagnostics. Native UDP removes
WebSocket framing, heartbeat and browser scheduling overhead; packet loss is
harmless because every packet supersedes the previous level state.
In practice capture buffering, the FFT window, and the next 16.7 ms animation
frame remain larger latency contributors. Both transports feed exactly the same
firmware audio state and are reported by `/diagnostics`.

## Saved pattern mappings

Each pattern patch currently retains ten mapping slots for persistence
compatibility, while the dashboard exposes the four active musical signals:

- target: Off, master time scale, brightness, every animated renderer parameter,
  or a shared spatial style control;
- amount: signed modulation gain from -200% through +200%;
- per-band attack and decay are controlled in the companion.

The mapping stage should operate on the same temporary parameter copies already
used by the LFO, then clamps to each parameter's valid range. Saved base values
remain unchanged. Master time scale affects animation evolution, not electrical
panel refresh. Positive amounts move a parameter toward its maximum; negative
amounts move it toward its minimum. A silent or disconnected input has no effect.

## Native UDP companion

A browser cannot open a raw UDP socket, which is why the web companion retains
WebSocket transport. The native app is implemented and builds to
`tools/matrix-audio-mac/dist/Matrix Audio Companion.app`. Enter the matrix's
`.local` hostname or IP, select whole-system or single-application capture, the
playback output, and an audible delay, then click **Start delayed live audio**.
Grant macOS System Audio Recording permission. Its Monitor view displays the unprocessed
2,048-point FFT as a real-time logarithmic 20 Hz–20 kHz dBFS spectrum. Detector
and envelope settings persist locally in macOS user defaults. Bonjour hostname
resolution is used instead of broadcast discovery.

Native version 0.3 fixes window construction and explicitly reopens/activates
its main window. Version 0.5 adds the 512-point fast kick path plus graphical,
slider-and-number AHDSR editors with a short adjustable Impact peak. Its
connection line polls `/diagnostics` and reports controller
reachability, confirmed UDP reception, HTTP round-trip time, packets per second,
18-byte payload throughput, and sender/receiver sequence lag.

Version 0.6 bundles `05 - Shed - Dreamscapes.mp3` and adds MP3 selection,
play/pause/restart, and seeking. Version 0.7 replaces the single Visual lead
with independent saved offsets for kick, snare, and
hi-hat, plus a matching Flow offset. A timestamped look-ahead buffer composes
each outgoing UDP frame from the requested positions on those four timelines.
Version 0.7.1 expands the offset range to ±1,000 ms for unmistakable testing,
deduplicates timestamp-resampled hit indicators, uses a 1,024-point raised-Hann
kick path, adds a 750 ms cold-start learning period, and changes the default
kick minimum interval from 120 to 250 ms. The Dreamscapes regression count fell
from 1,296 to 790 accepted kick events without changing the 2.67 ms isolated
synthetic-onset result.
Version 0.8 replaces ScreenCaptureKit live monitoring with an in-series Core
Audio router. The default 120 ms delay is intended to cover detector, UDP, ESP32
and next-frame latency without making normal listening feel heavily detached.
Values around 300–400 ms are useful for experimenting with longer true
look-ahead, but become obvious during live control or DJ monitoring. Longer BPM
history can be analyzed independently and does not require increasing audible
delay.
The reference file decodes to about 61.6 MB of mono 48 kHz PCM. System capture
and file playback are mutually exclusive but share detector settings, UDP
protocol, dashboard routes, and live visualizations.
