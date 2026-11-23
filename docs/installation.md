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
  - source: github://raine-works/sendspin-a1s-source@master
    components: [sendspin]
    refresh: always
```

`@master` tracks the latest commit. To pin a build to a release, use a tag such as `@vX.Y.Z` instead (see [Development](development.md#releases)).

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

Because the example uses `refresh: always`, each compile fetches the component again. Clean the build files before updating so the new component and its ESP-IDF dependencies are rebuilt.
