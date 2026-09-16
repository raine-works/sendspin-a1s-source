# sendspin-a1s-source

ESPHome firmware that turns an ESP32-A1S Audio Kit (ES8388 variant) into an encrypted Sendspin **source**: it streams the codec's ADC (line-in jack by default) to a Sendspin server such as modern Music Assistant.

Built on top of `sendspin-cpp` with **Noise_KKpsk2 transport encryption** and **source@v1** support (`brandenc77/sendspin-cpp@fix/source-pairing`), resolving the Music Assistant *"connected without encryption (legacy mode)"* restriction and enabling full device pairing.

## What's here

- `a1s-sendspin-source.yaml` is the device config. It pulls the component below from this repo.
- `components/sendspin` is ESPHome's `sendspin` component updated for encrypted source streaming:
  - Uses `brandenc77/sendspin-cpp` (`fix/source-pairing`), combining the Noise protocol transport encryption (`Noise_KKpsk2` with Curve25519 / ChaCha20-Poly1305 via `noise-c`) and the `source@v1` audio capture role.
  - End-to-end transport encryption for all WebSocket frames: control messages, stream lifecycle (`client-stream/start`, `client-stream/end`), and timestamped binary PCM/Opus audio chunks.
  - Persistent device identity and pairing records stored in ESP32 NVS (Non-Volatile Storage), ensuring stable `client_id` across reboots.
  - Hub gains a `source:` block that feeds an ESPHome microphone into the Sendspin source role. Codec, chunking, and Opus options fall back to sendspin-cpp's defaults when omitted.

The pinout (I²C SDA 33 / SCL 32, I²S MCLK 0 / BCLK 27 / LRCK 25 / DIN 35) is the one shared by the A1S configs in the [Home Assistant community thread](https://community.home-assistant.io/t/esp32-a1s-audio-kit-media-player/522245).

## Using it

Copy `a1s-sendspin-source.yaml` into your ESPHome config directory, provide `wifi_ssid` / `wifi_password` secrets, and build with ESPHome 2026.8 or newer.

- **Clean builds:** The IDF component manager pins resolved commits in `dependencies.lock`. If updating the component or branch, clean your ESPHome build directory before compiling.
- **Server:** Fully compatible with modern Music Assistant (`aiosendspin`) with encrypted transport and pairing authorization.
- **Input:** The `ADC Input` select switches between `LINE2` (3.5 mm line-in, the boot default) and `LINE1` (onboard mics).
- **Codec variant:** The boot log's I²C scan should show `0x10`. `0x1A` is the AC101 variant, which this config does not support.

## Known quirks

- **ES8388 ADC setup:** ESPHome's `es8388` driver sets the ADC up for voice. `on_boot` switches it to line-level capture:
  - I²S framing: the driver writes left-justified (`0x0D`), but `i2s_audio` reads Philips I²S. Rewritten to `0x0C`.
  - ALC turned off.
  - Muting noise gate turned off.
- **Source only:** ESPHome's `i2s_audio` has no full-duplex mode. A speaker on the same bus would hold the lock the microphone needs.
- **Windows builds from a long path** fail while unpacking micro-opus (MAX_PATH). Set `ESPHOME_DATA_DIR` to a short directory.

## License

`components/sendspin` is derived from [ESPHome](https://github.com/esphome/esphome) and remains under the ESPHome License (MIT for the Python code, GPLv3 for the C++ code); see `LICENSE`.
