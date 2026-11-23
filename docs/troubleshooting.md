# Troubleshooting

## Wrong codec on the board

If the I²C scan reports `0x1A` rather than `0x10`, the board has an AC101 codec and this configuration won't work. See [Hardware](hardware.md#check-the-codec).

## No audio, or noisy, inverted or swapped channels

The firmware writes ES8388 registers at boot to fix the I²S framing and gain. If you removed or changed the `on_boot` lambda, put it back. See [ES8388 registers](reference/registers.md).

If the audio is too quiet or clips, the ADC gain is 0 dB on purpose (`0x09` set to `0x00`). The previous +12 dB setting clipped hot line-level sources at 0 dBFS.

## The input is the wrong one

The `ADC Input` select is set to `LINE2` (the 3.5 mm jack) on every boot. Changing it in Home Assistant lasts until the next boot. `LINE1` is the onboard microphones.

## Drops or stutter

The log line `Streaming: ... | N drops` counts audio blocks that the source role refused. If the number grows:

- If you switched to `codec: opus`, go back to `pcm`. The example notes that Opus encoding took 35 to 58 ms per 40 ms chunk, slower than real time.
- Check Wi-Fi signal and use 2.4 GHz. The example already disables Wi-Fi power saving.
- Keep `capture_buffer` at the default of the example, `2000ms`, and `chunk_duration` at `40ms`.

A rising `re-anchors` count means samples were lost on the capture side.

## Pairing

- The `Pairing Token` entity only publishes when the token changes, so it can appear to do nothing after the first value. The token is also in the log at boot.
- A warning `Pairing failed with server: ... (reason: N)` means the server aborted pairing. Pair again with the token.
- The connection trust level `NONE (Unpaired/Sentinel)` means the device isn't paired with the server.

## The build fails or Noise errors appear

If you see `error 0x4503: 17667`, you have turned on ESPHome API or OTA encryption. Remove the `encryption:` key. See [Encryption](encryption.md#dont-enable-esphome-api-encryption).

After updating the component, run **Clean Build Files** (or `esphome clean`) before building.

## Autostart never fires

- Check `line_sense: true` is set.
- Watch `Audio Peak Level` while the source plays. If it stays under the threshold (default -42 dBFS), lower `signal_threshold`.
- Check that an **Autostart Target Player** is set in Music Assistant. See [Using it with Music Assistant](music-assistant.md#autostart-and-autostop).
