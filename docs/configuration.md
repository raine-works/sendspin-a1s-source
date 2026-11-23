# Configuration

The example `a1s-sendspin-source.yaml` is a complete configuration. This page lists what the `sendspin` component accepts and what the rest of the file does.

## The `sendspin` component

```yaml
sendspin:
  id: sendspin_hub
  task_stack_in_psram: true
  source:
    id: sendspin_source
    task_stack_in_psram: true
    microphone:
      microphone: a1s_adc
      channels: [0, 1]
    chunk_duration: 40ms
    capture_buffer: 2000ms
    codec: pcm
    line_sense: true
    signal_threshold: -42dB
    silence_timeout: 20s
    debounce_duration: 200ms
```

The component only builds on ESP32. The `network` component is required, and `mdns` is loaded for you.

### Hub options

| Option | Default | What it does |
| :--- | :--- | :--- |
| `id` | generated | The hub's ID. The example uses `sendspin_hub`. |
| `task_stack_in_psram` | not set | Puts the HTTP and WebSocket task stack in PSRAM. The example sets `true`. |
| `source` | none | The audio source role. Without it the source role is disabled in the build. |

### Source options

| Option | Default | What it does |
| :--- | :--- | :--- |
| `id` | generated | The source's ID. The example uses `sendspin_source`, which the template entities refer to. |
| `microphone` | required | A microphone source: `microphone:` (the ID of the `i2s_audio` microphone) and `channels:`. The microphone must be 1 or 2 channels at 16 to 32 bits per sample. The example uses `channels: [0, 1]` (left, right). |
| `codec` | not set here | `pcm` or `opus`. When omitted, the sendspin-cpp default applies. The example uses `pcm`. |
| `chunk_duration` | not set here | How much audio goes in each chunk. The example uses `40ms`. |
| `capture_buffer` | not set here | The capture ring buffer length, which absorbs Wi-Fi jitter. The example uses `2000ms`. |
| `opus_bitrate` | not set here | Opus only, in bits per second. The commented example is `128000`. |
| `opus_complexity` | not set here | Opus only, 0 to 255. The commented example is `0`. |
| `line_sense` | `false` | Turns on signal sensing. See [Line sensing](#line-sensing). |
| `signal_threshold` | `-42dB` | The level that counts as signal. See [Thresholds](#thresholds). |
| `silence_timeout` | `20s` | How long the level must stay below the threshold before the signal is reported absent. |
| `debounce_duration` | `200ms` | How long the level must stay at or above the threshold before the signal is reported present. |
| `task_stack_in_psram` | not set | Puts the audio capture task stack in PSRAM. The example sets `true`. |

“Not set here” means the component passes no value, so sendspin-cpp chooses. Durations are ESPHome time periods in milliseconds or longer units.

### Codecs

`pcm` sends uncompressed audio. At the example's 24 kHz, 16-bit stereo it is 768 kbps.

`opus` compresses the audio. The example leaves it off, with this comment: Opus encoding was taking 35 to 58 ms per 40 ms chunk on the hardware, which overflows the capture ring and drops about five chunks a second. If you switch to Opus, watch the `drops` counter in the log. See [Troubleshooting](troubleshooting.md#drops-or-stutter).

### Line sensing

With `line_sense: false` the microphone starts when the server starts a stream and stops when the server stops it.

With `line_sense: true` the microphone runs all the time. The firmware tracks the peak level, checks it every 100 ms and tells the server when a signal appears or disappears. Music Assistant can use that to start and stop playback. See [Using it with Music Assistant](music-assistant.md#autostart-and-autostop).

### Thresholds

`signal_threshold` accepts:

| Form | Example | Meaning |
| :--- | :--- | :--- |
| Decibels | `-42dB` or `-42dBFS` | Relative to full scale (32767) |
| Percentage | `1%` | Share of full scale |
| Amplitude | `300` | Raw sample amplitude |
| Negative number | `-42` | Decibels |

The value is converted to an amplitude and limited to 1 to 32767. The default `-42dB` is an amplitude of 260. A bare decimal at or below `1.0` is a fraction of full scale. Anything else is rejected with “Invalid signal threshold”.

## The rest of the example

| Block | What it does |
| :--- | :--- |
| `esphome` | Name, friendly name, `min_version: 2026.8.0`, and the boot step that selects `LINE2` and writes the codec registers. See [ES8388 registers](reference/registers.md). |
| `esp32` | `esp-wrover-kit`, ESP-IDF, minimum chip revision 3.1, 240 MHz and `-O2` compiler optimisation. |
| `psram` | `speed: 80MHz`. |
| `logger`, `api`, `ota`, `captive_portal` | Defaults. `api` has no encryption key; see [Encryption](encryption.md). |
| `wifi` | Credentials from secrets, `power_save_mode: none`, `fast_connect: true` and a fallback `ap`. |
| `external_components` | The component source. See [Installation](installation.md). |
| `i2c` | `GPIO33` and `GPIO32`, with a scan at boot. |
| `audio_dac` and `select` | The ES8388 driver and the `ADC Input` select. |
| `i2s_audio` | The I²S bus: LRCLK `GPIO25`, BCLK `GPIO27`, MCLK `GPIO0`. |
| `microphone` | The external ADC on `GPIO35`: not PDM, 24000 Hz, 16 bit, stereo. |
| `binary_sensor`, `sensor`, `text_sensor` | The `Audio Signal`, `Audio Peak Level` and `Pairing Token` entities. See [Entities and logs](reference/entities.md). |

## Build settings the component applies

When a `source` is configured, the component sets these ESP-IDF options for you. Don't repeat them in your YAML.

- `CONFIG_SENDSPIN_ENABLE_SOURCE` on; the player, controller, metadata, artwork, colour and visualizer roles are off.
- `CONFIG_HTTPD_QUEUE_WORK_BLOCKING` on, so the HTTP control queue blocks instead of dropping during Wi-Fi jitter.
- `CONFIG_LWIP_UDP_RECVMBOX_SIZE` of 32.
- Opus: non-thread-safe pseudostack of 60000 bytes, with the pseudostack and codec state preferring internal RAM, no `alloca`, Xtensa optimisations on.
- `CONFIG_COMPILER_OPTIMIZATION_PERF` on.

It also asks for high-performance networking, reserves sockets for the Sendspin WebSocket server and client, and enables Wi-Fi runtime power-save and roaming control.
