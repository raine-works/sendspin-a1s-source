# Hardware

## What you need

- An Ai-Thinker ESP32-A1S Audio Kit with the Everest Semi ES8388 codec. The configuration is written for the v2.2 board.
- A line-level source on the 3.5 mm line-in jack (a turntable preamp, CD player, TV audio output and so on).
- A 2.4 GHz Wi-Fi network. The configuration needs `psram`, which the board's ESP32 provides.
- A Music Assistant server (or another Sendspin server) on the same network.

The ESP32 is configured as an `esp-wrover-kit` with ESP-IDF, a 240 MHz CPU, 80 MHz PSRAM and a minimum chip revision of 3.1.

## Check the codec

Boards that mount an AC101 codec instead of the ES8388 are not supported. The I²C scan in the boot log tells them apart:

```text
[C][i2c:119]: Found device at address 0x10
```

`0x10` is the ES8388. `0x1A` is the AC101, which this configuration doesn't support.

## Pins

| Signal | ESP32 pin | Notes |
| :--- | :--- | :--- |
| I²C SDA | `GPIO33` | Codec control |
| I²C SCL | `GPIO32` | Codec control |
| I²S MCLK | `GPIO0` | Master clock |
| I²S BCLK | `GPIO27` | Bit clock |
| I²S LRCLK | `GPIO25` | Word select |
| I²S DIN | `GPIO35` | ADC data from the codec |

The I²C bus is scanned at boot (`scan: true`). The configuration's header says this pinout is the one shared by the A1S configurations linked from the Home Assistant community thread on the A1S media player.

## Inputs

The `ADC Input` select on the device chooses the codec input. The configuration sets it to `LINE2`, the 3.5 mm line-in jack, on every boot. `LINE1` is the onboard microphones. See [Entities and logs](reference/entities.md).

## Source only

This firmware only captures audio. ESPHome's `i2s_audio` has no full-duplex mode, and a speaker on the same I²S bus would hold the lock the microphone needs. Don't add a speaker or media player to the configuration. See [How it works](how-it-works.md).
