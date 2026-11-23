# Encryption

## Transport encryption

Sendspin traffic between the board and the server, both control messages and audio, is encrypted with the `Noise_KKpsk2` protocol. The component builds the encrypted sendspin-cpp library from `raine-works/sendspin-cpp`, pinned to a specific commit in `components/sendspin/__init__.py`.

The repository's README describes the primitives as Curve25519 key exchange, ChaCha20-Poly1305 and SHA-256.

## Identity and trust

The device keeps its identity and the trust records for servers in the ESP32's non-volatile storage (NVS, namespace `sendspin`). The hub reads, writes and erases those records through the library. Because they are stored, the device's client ID and its pairing survive reboots.

The device shows its pairing token (`SP:0...`) in the log at boot, in the hub’s configuration dump, and in the `Pairing Token` entity. You give it to the server once. See [Using it with Music Assistant](music-assistant.md#pair-the-device).

The hub logs the connection's trust level as either `USER (Paired)` or `NONE (Unpaired/Sentinel)`.

## Don't enable ESPHome API encryption

Leave `api:` without an `encryption:` key, and don't turn on OTA encryption. According to the README, ESPHome's native API encryption builds Noise with only the `Noise_NNpsk0` protocol, which breaks Sendspin's `Noise_KKpsk2` handshake with `error 0x4503: 17667`. Sendspin encrypts its own traffic, so nothing is left unprotected on the Sendspin connection.

This does mean the ESPHome native API connection itself is not encrypted with this configuration.
