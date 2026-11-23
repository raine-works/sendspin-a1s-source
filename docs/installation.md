# Installation

## Before you start

- ESPHome 2026.8.0 or newer (the configuration sets `min_version: 2026.8.0`).
- Wi-Fi credentials in your ESPHome `secrets.yaml`:

  ```yaml
  wifi_ssid: "YourNetworkSSID"
  wifi_password: "YourNetworkPassword"
  ```

- A board with the ES8388 codec. See [Hardware](hardware.md).

## Add the configuration

Copy `a1s-sendspin-source.yaml` from the repository into your ESPHome directory. It pulls the `sendspin` component from this repository:

```yaml
external_components:
  - source: github://raine-works/sendspin-a1s-source@v2.0.1
    components: [sendspin]
```

The component is pinned to a release tag, so every build uses the same code. To update, change the tag to a newer [release](https://github.com/raine-works/sendspin-a1s-source/releases) (see [Development](development.md#releases)).

The device is named `a1s-sendspin-source`, with the friendly name “A1S Sendspin Source”. Change `name` and `friendly_name` under `esphome:` if you want something else; Music Assistant shows the friendly name.

## Compile and flash

### From the ESPHome dashboard

1. If you're updating from an earlier build, open the card’s menu and choose **Clean Build Files**.
2. Choose **Install**, then **Wirelessly**, or connect over USB for the first flash.

### From the command line

```bash
esphome clean a1s-sendspin-source.yaml
esphome run a1s-sendspin-source.yaml
```

## After flashing

1. Open the device log and check the I²C scan reports `0x10`.
2. Find the pairing token in the log or in the `Pairing Token` entity, then pair the device in Music Assistant. See [Using it with Music Assistant](music-assistant.md).

## Updating

Change the tag in `external_components` to the release you want, then clean the build files so the new component and its ESP-IDF dependencies are rebuilt.
