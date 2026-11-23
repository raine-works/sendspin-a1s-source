# Entities and logs

## Home Assistant entities

The example configuration defines these entities, plus the `ADC Input` select from the `es8388` platform.

| Entity | Type | What it shows |
| :--- | :--- | :--- |
| ADC Input | Select | The codec input: `LINE2` (3.5 mm jack, set at every boot) or `LINE1` (onboard microphones) |
| Audio Signal | Binary sensor (`sound`) | Whether the line-in signal is present, from `is_signal_present()` |
| Audio Peak Level | Sensor, dBFS, diagnostic | The peak level over the last 100 ms check, updated every second. A peak of 0 reads as -96.0 |
| Pairing Token | Text sensor, diagnostic | The `SP:0...` pairing token, checked every 10 seconds and published only when it changes |

`Audio Signal` and `Audio Peak Level` only change when `line_sense` is on.

## Log lines

| Tag | Line | Meaning |
| :--- | :--- | :--- |
| `sendspin.hub` | `Pairing Token: ...` | Written once at startup |
| `sendspin.hub` | `Connection trust level: ...` | `USER (Paired)` or `NONE (Unpaired/Sentinel)` |
| `sendspin.hub` | `Pairing started`, `Pairing succeeded`, `Pairing failed` | Pairing progress with the server's ID |
| `sendspin.hub` | `Pairing PIN: ...` | A PIN the server asked the device to display |
| `sendspin.source` | `Line sense enabled (...)` | Threshold as an amplitude, debounce and silence timeout, at startup |
| `sendspin.source` | `Audio signal detected` / `Audio silence for Ns` | The signal was reported present or absent |
| `sendspin.source` | `Initial line-in state: ABSENT` | The first report after boot |
| `sendspin.source` | `Server started stream (...)` | Codec, rate, channels, bit depth and free internal memory |
| `sendspin.source` | `Streaming: Ns active \| N KB sent \| N drops \| N re-anchors` | Written every five seconds while streaming |
| `sendspin.source` | `Server stopped stream; ...` | The final duration, data sent and drops |

The counters mean:

- **KB sent** is audio accepted by the source role.
- **drops** is blocks the source role refused.
- **re-anchors** is how many times the timeline restarted after sample loss.

The source and hub also print their settings in the device configuration dump at boot (`dump_config`).
