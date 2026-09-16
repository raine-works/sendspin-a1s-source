# ESP32-A1S Encrypted Sendspin Audio Source

[![ESPHome Version](https://img.shields.io/badge/ESPHome-2026.8%2B-blue.svg)](https://esphome.io/)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Security](https://img.shields.io/badge/Security-Noise_KKpsk2_Encrypted-green.svg)](https://noiseprotocol.org/)
[![Server](https://img.shields.io/badge/Compatible-Music_Assistant-orange.svg)](https://music-assistant.io/)
[![License](https://img.shields.io/badge/License-GPLv3%20%2F%20MIT-lightgrey.svg)](LICENSE)

High-fidelity ESPHome firmware that transforms the **Ai-Thinker ESP32-A1S Audio Kit** (Everest ES8388 variant) into a dedicated, encrypted **Sendspin Audio Source**. 

Stream pristine line-in audio from turntables, CD players, TV optical/aux outputs, or analog preamps directly into [Music Assistant](https://music-assistant.io/) and synchronized multi-room Sendspin speaker groups with microsecond timing accuracy.

---

## 🌟 Overview

This repository provides complete, production-ready ESPHome firmware for the Ai-Thinker ESP32-A1S Audio Kit, implementing the official `source@v1` audio capture specification. It features end-to-end `Noise_KKpsk2` transport encryption, persistent cryptographic identity in NVS, low-latency DMA streaming, and hardware-tuned ES8388 codec register configurations for pristine, uncompressed analog audio streaming.


---

## ✨ Features

- **🔐 End-to-End Transport Encryption:** Native `Noise_KKpsk2` handshake with Curve25519 key exchange, ChaCha20-Poly1305 authenticated ciphers, and SHA-256 hash states for all WebSocket control messages and audio chunks.
- **💎 Hi-Fi Line Capture (No Voice Compression):** Custom `on_boot` register overrides configure the ES8388 ADC for transparent line-level audio:
  - Philips I²S framing (`0x0C`) to eliminate Left-Justified bit-shift distortion and channel inversion.
  - Automatic Level Control (ALC) disabled (`0x12` $\rightarrow$ `0x22`) to prevent volume pumping.
  - Voice noise gate disabled (`0x16` $\rightarrow$ `0x00`) to preserve delicate musical decays and quiet passages.
- **🚀 Ultra-Low Latency DMA Engine:** Continuous 48 kHz / 16-bit stereo PCM streaming (192 KB/s) utilizing ESP32 I²S DMA buffers and an external PSRAM lock-free SPSC ring buffer.
- **⏱️ Adaptive Microsecond Slew Correction:** Software sample clock predictor continuously tracks ADC crystal drift against `esp_timer_get_time()`, filtering interrupt jitter and maintaining drift-free synchronization.
- **💾 Durable Cryptographic Identity:** Persistent Curve25519 identity keypair and server trust records stored in ESP32 Non-Volatile Storage (NVS). Your `client_id` remains stable across reboots.
- **🔑 Seamless Out-of-Band Pairing:** Automatically logs and surfaces the 107-character `SP:0...` pairing token in boot logs and exposes it as a Home Assistant sensor entity for effortless setup.
- **🎛️ Dual Input Multiplexing:** Easily toggle between the 3.5 mm Aux Line-In jack (`LINE2`, default) and the onboard stereo electret microphones (`LINE1`) via Home Assistant or ESPHome dashboard.

---

## 📐 Architecture Overview

```mermaid
flowchart LR
    Audio["Line-In (3.5mm Aux)"] --> Codec["ES8388 Audio Codec"]
    Codec --> ESP["ESP32-A1S (ESPHome)"]
    ESP -- "Sendspin over Wi-Fi" --> MA["Music Assistant"]
    MA --> Speakers["Multi-Room Speakers"]
```

The ESP32-A1S captures analog line-in audio through the ES8388 codec, packages it into a synchronized Sendspin audio stream, and transmits it over Wi-Fi to Music Assistant for playback across your speaker groups.

---

## 🛠️ Hardware Requirements & Pinout

This configuration is tailored for the **ESP32-A1S Audio Kit v2.2** equipped with the Everest Semi **ES8388** audio codec.

| Signal | ESP32 GPIO | ES8388 Pin / Function | Notes |
| :--- | :--- | :--- | :--- |
| **I²C SDA** | `GPIO33` | SDA | Codec communication at address `0x10` |
| **I²C SCL** | `GPIO32` | SCL | Clock rate: 50 kHz |
| **I²S MCLK** | `GPIO0` | MCLK | Master clock (256 $\times f_s$) |
| **I²S BCLK** | `GPIO27` | BCLK / SCLK | Bit clock |
| **I²S LRCK** | `GPIO25` | LRCK / DSRCLK | Frame sync / Word select |
| **I²S DIN** | `GPIO35` | ASDOUT (ADC Data Out) | Input-only GPIO (ideal for ADC data) |

> [!IMPORTANT]
> **Codec Variant Verification:**
> Verify that your board mounts the **ES8388** codec. On initial boot, the ESPHome I²C scan must report:
> ```text
> [C][i2c:119]: Found device at address 0x10
> ```
> *(Boards with address `0x1A` mount the AC101 codec, which is incompatible with this driver).*

---

## 🚀 Getting Started

### 1. Configure Wi-Fi Secrets
Ensure your ESPHome `secrets.yaml` contains your Wi-Fi credentials:
```yaml
wifi_ssid: "YourNetworkSSID"
wifi_password: "YourNetworkPassword"
```

### 2. Add Configuration to ESPHome
Copy [`a1s-sendspin-source.yaml`](file:///Users/rainepetersen/Projects/raineworks/sendspin-a1s-source/a1s-sendspin-source.yaml) into your ESPHome directory.

The configuration references this repository as an external component:
```yaml
external_components:
  - source: github://raine-works/sendspin-a1s-source@master
    components: [sendspin]
    refresh: always
```

### 3. Compile & Flash

#### Option A: Via Home Assistant ESPHome Dashboard
1. Open the **ESPHome Dashboard** in Home Assistant.
2. If updating from a previous build, click the **three dots (⋮)** on the **A1S Sendspin Source** card and select **Clean Build Files**.
3. Click **Install** → **Wirelessly** (or connect via USB for first flash).

#### Option B: Via ESPHome Command Line
```bash
esphome clean a1s-sendspin-source.yaml
esphome run a1s-sendspin-source.yaml
```

---

## 📱 Pairing with Music Assistant

1. **Power On:** Power the ESP32-A1S board and open the device log.
2. **Retrieve Pairing Token:** 
   * On boot, the log will output your device's unique token:
     ```text
     [I][sendspin.hub]: Pairing Token: SP:0...
     ```
   * Alternatively, view the **`Pairing Token`** entity on the device page in Home Assistant.
3. **Authorize in Music Assistant:**
   * Open **Music Assistant** → **Settings** → **Players / Sources**.
   * Locate **`A1S Sendspin Source`** and click **Setup / Pair**.
   * When prompted for `pairing_token`, paste the `SP:0...` token and click **Next**.
4. **Verified Connection:** The ESPHome log will confirm pairing:
   ```text
   [sendspin.noise_handshake]: Noise handshake complete: server_id=... psk_category=2
   [sendspin.connection]: Noise transport active
   [sendspin.hub]: Connection trust level: USER (Paired)
   ```

---

## ⚙️ Configuration Options

Fine-tune streaming characteristics in [`a1s-sendspin-source.yaml`](file:///Users/rainepetersen/Projects/raineworks/sendspin-a1s-source/a1s-sendspin-source.yaml):

```yaml
sendspin:
  id: sendspin_hub
  task_stack_in_psram: true   # Moves HTTP/WebSocket task stack into 8 MB PSRAM
  source:
    task_stack_in_psram: true # Moves audio capture task stack into PSRAM
    microphone:
      microphone: a1s_adc
      channels: [0, 1]        # 0: Left, 1: Right
    
    # Optional parameters (defaults shown):
    # codec: pcm              # 'pcm' (uncompressed, lossless) or 'opus' (compressed)
    # chunk_duration: 20ms    # 5ms - 60ms (20ms is optimal for network overhead)
    # capture_buffer: 150ms   # PSRAM ring buffer depth (protects against Wi-Fi jitter)
    # opus_bitrate: 128000    # Active only when codec is set to 'opus'
    # opus_complexity: 2      # 0 - 10 (2 is optimized for ESP32 CPU budget)
```

---

## 🔍 Technical Details & Quirks

### ES8388 Codec Framing
ESPHome's upstream `es8388` driver initializes the ADC in Left-Justified mode (`0x0D`). However, ESPHome's `i2s_audio` component reads Philips I²S standard (1-bit clock delay). This mismatch results in inverted polarity, lost sign bits, and swapped stereo channels. This firmware forcibly overrides register `0x0C` (`ADCCONTROL4`) to `0x0C` on boot, establishing bit-perfect Philips I²S alignment.

### Half-Duplex Operation
ESPHome's `i2s_audio` bus does not support simultaneous full-duplex operation. To guarantee zero audio dropouts, this firmware configures the ESP32-A1S strictly as an audio **source**. Output speakers on the same bus are disabled to eliminate bus contention.

---

## 📄 License & Credits

- Core firmware and component modifications by [Raine Petersen](https://github.com/raine-works).
- Underlying component architecture derived from [ESPHome](https://github.com/esphome/esphome) (licensed under MIT and GPLv3).
- C++ cryptographic and protocol layer powered by [`raine-works/sendspin-cpp`](https://github.com/raine-works/sendspin-cpp) (Apache 2.0).
- Compatible with the open [Sendspin Specification](https://sendspin.io) and [Music Assistant](https://music-assistant.io).
