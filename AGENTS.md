# Instructions for AI Coding Agents (`AGENTS.md`)

This file contains critical guidelines, architecture details, and operating rules for AI coding assistants working in the `sendspin-a1s-source` repository.

---

## ⚠️ Core Operating Rules

1. **NO COMMITS OR PUSHES WITHOUT EXPLICIT APPROVAL**
   - **Never** execute `git commit`, `git push`, `git rebase`, `git reset`, or switch branches without direct, unambiguous instruction from the user.
   - Always present code changes, test results, and `git diff` output to the user for review first.

2. **HARDWARE INTEGRITY & REGISTER PRESERVATION**
   - **Do not modify ES8388 codec register overrides** in `a1s-sendspin-source.yaml` (`esphome.on_boot`) unless specifically diagnosing codec register issues.
   - Register `0x0C = 0x0C` fixes ESPHome's 1-bit I²S offset / Left-Justified mismatch (prevents inverted sign bits and channel swap).
   - Register `0x09 = 0x44` sets +12 dB ADC PGA gain for clean line-in capture.
   - Register `0x12 = 0x22` and `0x16 = 0x00` disable voice ALC volume pumping and noise gating.

3. **STRICT HALF-DUPLEX INVARIANT (SOURCE ONLY)**
   - The Ai-Thinker ESP32-A1S Audio Kit shares its I²S bus pins (`GPIO0`, `GPIO27`, `GPIO25`). ESPHome's `i2s_audio` component **does not support full duplex**.
   - **Never configure speaker/DAC media player entities on this bus.** Any concurrent output will hold the I²S DMA lock, starve the microphone input, and cause immediate buffer overruns. This firmware is strictly an audio capture source.

4. **REAL-TIME THREAD SAFETY & TASK BOUNDARIES**
   - The microphone data callback (`add_data_callback`) runs inside the **real-time microphone FreeRTOS task**.
   - **Never** perform blocking I/O, heavy memory allocations, or unfiltered logging inside `add_data_callback`. Only push audio frames to `source_role_->write_audio(...)` and update lock-free/atomic counters (`std::atomic`).
   - Periodic tasks, state changes, and logging belong in `loop()` or event callbacks (`on_streaming_started`, `on_streaming_stopped`) executing on ESPHome's main loop thread.

5. **MEMORY DISCIPLINE (PSRAM PREFERRED)**
   - ESP32 internal SRAM is severely constrained (~100–150 KB free heap).
   - All sizable buffers (capture ring buffer, chunk staging buffers, Opus encoder state/arena, task stacks) **must** reside in external PSRAM:
     - `task_stack_in_psram: true` on both hub and source.
     - `MemoryLocation::PREFER_EXTERNAL` in role configs.
   - Never allocate large buffers in internal SRAM.

6. **LOG HYGIENE & ANTI-SPAM**
   - Do not emit high-frequency or unthrottled logs during active audio capture.
   - Streaming telemetry must be throttled (default is every 5 seconds).
   - State sensors (such as `Pairing Token`) must deduplicate their outputs (returning `{}` when unchanged) to avoid flooding the log with recurring poll updates.

---

## 📐 Architecture & Key Components

```
sendspin-a1s-source/
├── a1s-sendspin-source.yaml        # Main ESPHome configuration for ESP32-A1S
├── components/
│   └── sendspin/                   # ESPHome external component
│       ├── __init__.py             # Component schema, codegen, and ESP-IDF sdkconfig
│       ├── sendspin_hub.h/.cpp     # Hub component: Noise encryption, NVS keys, WebSocket
│       └── sendspin_source.h/.cpp  # Audio source role: I2S capture, slew correction, telemetry
├── README.md                       # User-facing documentation & wiring guide
└── AGENTS.md                       # Rules and guidelines for AI agents
```

### Component Roles & Responsibilities

1. **`SendspinHub` (`sendspin_hub.h`, `sendspin_hub.cpp`)**:
   - Manages client lifecycle and connects to `sendspin-cpp`.
   - Handles `Noise_KKpsk2` key generation, NVS persistence, and out-of-band pairing token retrieval (`get_pairing_token()`).
   - Surfaces server trust states (`NONE`, `DETECTED`, `PIN`, `USER`).

2. **`SendspinSource` (`sendspin_source.h`, `sendspin_source.cpp`)**:
   - Implements `sendspin::SourceRoleListener`.
   - Gated by server commands: the microphone is only started when the server sends `start` and stopped when the server sends `stop`.
   - Tracks real-time slew correction between ADC hardware arrival times and `esp_timer_get_time()`.
   - Tracks atomic byte counters and logs 5-second streaming telemetry (`Streaming: X s active | Y KB sent | Z drops`).

3. **`__init__.py`**:
   - Registers ESP-IDF dependencies (`sendspin/sendspin-cpp`, `esphome/micro-opus`).
   - Configures critical sdkconfig settings:
     - `CONFIG_SENDSPIN_ENABLE_SOURCE = True`
     - `CONFIG_HTTPD_QUEUE_WORK_BLOCKING = True`
     - `CONFIG_LWIP_UDP_RECVMBOX_SIZE = 32`
     - `CONFIG_OPUS_NONTHREADSAFE_PSEUDOSTACK = True`
     - `CONFIG_OPUS_PSEUDOSTACK_PREFER_PSRAM = True`
     - `CONFIG_OPUS_STATE_PREFER_PSRAM = True`

---

## 🛠️ Testing & Local Iteration

When modifying files in `components/sendspin/`:
- `a1s-sendspin-source.yaml` references the component. If iterating locally without pushing to GitHub first, update `external_components` to use the local path:
  ```yaml
  external_components:
    - source:
        type: local
        path: components
      components: [sendspin]
  ```
- Before publishing or committing changes, ensure that `README.md` stays synchronized with any configuration or code changes.
