# How it works

## Roles

- The hub (`SendspinHub`) starts the Sendspin client and server, keeps the identity in NVS, reports trust and pairing events, and asks Wi-Fi for high performance while streaming.
- The source (`SendspinSource`) reads the microphone, timestamps the audio and hands it to the source role. It only runs the microphone while the server has a stream open, unless `line_sense` is on.
- `__init__.py` defines the YAML schema, pulls in `sendspin/sendspin-cpp`, and sets the build options.

## Half-duplex capture

ESPHome’s `i2s_audio` doesn't support full duplex. The configuration therefore captures only, and no speaker or media player should share the bus. Any output on the same bus would hold the I²S lock the microphone needs.

## Capture and timestamps

The microphone callback runs in the microphone task. It does only the work that must be real-time: updating the peak level and writing the block to the source role, with atomic counters for statistics. Logging and state changes happen in the main loop.

Capture times come from a sample clock: one `esp_timer_get_time()` anchor plus the number of frames captured since. That keeps scheduling jitter off the wire, which listeners would otherwise hear as speed changes. Residual drift is corrected by a slow slew limited to 100 ppm, and only when the filtered error exceeds 500 µs. The timeline re-anchors only after arrivals have been off by 30 ms or more for 200 ms of audio, which means samples were really lost. The `re-anchors` counter in the log counts those.

## Line sensing

With `line_sense` on, the microphone keeps running. The callback keeps the highest absolute sample value, and the main loop reads and resets it every 100 ms. The result feeds the `Audio Peak Level` entity, and the debounce and timeout logic reports present or absent to the server and updates `Audio Signal`.

## Memory

The capture buffer and the task stacks marked `task_stack_in_psram` live in PSRAM, which is why `psram` is required. The `micro-opus` pseudostack (60,000 bytes) and encoder state prefer internal RAM, falling back to PSRAM, because encoding from PSRAM is about 2× slower and can't keep up in real time. It's the one deliberate exception to keeping large buffers in PSRAM (`AGENTS.md`, rule 5), set by the `CONFIG_OPUS_*_PREFER_INTERNAL` options in `__init__.py`. The source logs the free internal heap when a stream starts.
