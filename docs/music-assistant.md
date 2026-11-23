# Using it with Music Assistant

The firmware is a Sendspin source. Music Assistant (or another Sendspin server) connects to it, pairs with it and asks it to start and stop streaming.

## Pair the device

1. Power the board and open the device log.
2. Get the pairing token, either from the log:

   ```text
   [I][sendspin.hub]: Pairing Token: SP:0...
   ```

   or from the `Pairing Token` entity on the device page in Home Assistant.
3. In Music Assistant, go to **Settings → Players / Sources**, find **A1S Sendspin Source** and choose **Setup / Pair**.
4. When it asks for `pairing_token`, paste the `SP:0...` token and continue.

The log confirms a completed pairing:

```text
[sendspin.noise_handshake]: Noise handshake complete: server_id=... psk_category=2
[sendspin.connection]: Noise transport active
[sendspin.hub]: Connection trust level: USER (Paired)
```

The hub also logs `Pairing started with server`, `Pairing succeeded with server` and a warning with a reason code if pairing fails. See [Troubleshooting](troubleshooting.md#pairing).

## Stream

The server decides when audio flows. When it starts a stream the log shows the format and free memory, then a status line every five seconds:

```text
[sendspin.source]: Server started stream (codec: PCM, 24000 Hz, 2 ch, 16 bit); internal free: 141224 B, max block: 110592 B; starting microphone
[sendspin.source]: Streaming: 5s active | 469 KB sent | 0 drops | 0 re-anchors
[sendspin.source]: Server stopped stream; duration: 45s, sent: 720 KB, drops: 0; stopping microphone
```

With `line_sense` on, the microphone is “keeping active” instead of starting and stopping. The counters are explained in [Entities and logs](reference/entities.md#log-lines).

## Autostart and autostop

With `line_sense: true` the source tells the server when a signal is present, so Music Assistant can start playback by itself, for example when a stylus touches the record.

1. In Music Assistant, open **Settings → Providers → Sendspin Source**.
2. Find **A1S Sendspin Source** in the list of sources.
3. Under **Autostart Target Player**, choose the speaker or group to play to.

The firmware then does the following:

- When the peak level stays at or above `signal_threshold` for `debounce_duration` (default 200 ms), it reports the signal as present.
- When the level stays below the threshold for `silence_timeout` (default 20 s), it reports the signal as absent.
- After boot, it reports an initial absent state once it has seen 1.5 seconds of silence.

What Music Assistant does with those reports (starting and stopping playback on the target player) is Music Assistant's behaviour. Tune the threshold, debounce and timeout in [Configuration](configuration.md#source-options), and watch the `Audio Peak Level` entity to choose a threshold for your source.
