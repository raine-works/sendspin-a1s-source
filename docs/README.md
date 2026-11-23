# ESP32-A1S Sendspin source documentation

ESPHome firmware that turns an Ai-Thinker ESP32-A1S Audio Kit (ES8388 variant) into an encrypted Sendspin audio source, so line-in audio such as a turntable can play through Music Assistant and Sendspin speakers.

| Guide | What it covers |
| :--- | :--- |
| [Hardware](hardware.md) | The board, the codec check and the pins the configuration uses |
| [Installation](installation.md) | Adding the configuration to ESPHome, compiling and flashing |
| [Configuration](configuration.md) | Every `sendspin:` option with its default, and the rest of the example configuration |
| [Using it with Music Assistant](music-assistant.md) | Pairing, streaming and turntable autostart |
| [Encryption](encryption.md) | The Noise transport, the stored identity and why ESPHome API encryption must stay off |
| [How it works](how-it-works.md) | Half-duplex capture, timestamping, line sensing and memory use |
| [Troubleshooting](troubleshooting.md) | Wrong codec, no audio, drops, pairing and build problems |
| [Development](development.md) | Iterating on the component, the repository rules and releases |

| Reference | What it covers |
| :--- | :--- |
| [Entities and logs](reference/entities.md) | The Home Assistant entities and the log lines the firmware writes |
| [ES8388 registers](reference/registers.md) | The codec registers written at boot and why |
