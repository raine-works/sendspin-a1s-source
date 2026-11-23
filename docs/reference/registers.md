# ES8388 registers

ESPHome's `es8388` driver sets the ADC up for voice capture. The `on_boot` step in the example (priority -100) selects `LINE2` and then writes four registers to put the codec back to plain line-level capture.

| Register | Value | Meaning |
| :--- | :--- | :--- |
| `0x0C` (ADCCONTROL4) | `0x0C` | 16-bit I²S framing. The driver writes `0x0D` (left-justified), but `i2s_audio` reads Philips I²S, one bit late: every sample loses its sign bit and left and right swap. |
| `0x09` (ADCCONTROL1) | `0x00` | 0 dB ADC input gain. +12 dB (`0x44`) clipped hot line-level sources at 0 dBFS. |
| `0x12` (ADCCONTROL10) | `0x22` | Automatic level control off. The driver's voice setting pumps the volume on music. |
| `0x16` (ADCCONTROL14) | `0x00` | Noise gate off. The driver's gate mutes the ADC below -40.5 dBFS. |

The registers are written through the `es8388_dac` ID, which the `audio_dac` entry defines. Keep that ID, and leave the values alone unless you're diagnosing the codec.
